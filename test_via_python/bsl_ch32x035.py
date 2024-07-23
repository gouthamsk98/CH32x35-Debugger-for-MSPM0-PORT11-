from subprocess import Popen, PIPE

import time
import sys
import glob
import argparse
import os
import binascii

import usb.core
import usb.util
import signal

from tqdm import tqdm
# version
__version__ = "2.1"
# Verbose level
QUIET = 5

CDC_DATA_INTERFACE = 0  
VENDOR_ID = 0x6249 
PRODUCT_ID = 0x7031 
# port        = None

MAX_BUFF_LEN = 64
SETUP 		 = False
BSL          = False
CRC32_POLY = 0xEDB88320

#FUNCTIONS
START      = 0x01
ERASE      = 0x02
PAGE_ERASE = 0x03
WRITE      = 0x04
READ       = 0x05
VERIFY     = 0x06
START_APP  = 0x07
EXIT       = 0x08
ACK        = 0x09               
DATA       = 0x0A        

FOR_WRITE  = 0x11
FOR_READ   = 0x22

OK = 0x01
FAIL = 0x00

WRITE_OK = 0x08
 
try:
    import usb.core
    import usb.util
except ImportError:
    print('{} requires the Python usb library'.format(sys.argv[0]))
    print('Please install it with:')
    print('')
    print('   sudo pip3 install pyusb')
    sys.exit(1)


"function for printing messages based on the verbosity level "
def mdebug(level, message, attr='\n'):
    if QUIET >= level:
        print(message, end=attr, file=sys.stderr)

"""
Custom exception class for command-related errors.

This exception is raised to indicate errors that occur during the execution
of commands in the bootloader utility. It inherits from the base Exception class.

Usage:
When a command encounters an error condition that cannot be handled locally,
it raises an instance of this exception class with an optional error message.

Example:
>>> raise CmdException("Error: Unable to perform command X.")
"""

class CmdException(Exception):
    pass

"""
calculates and returns the checksum 

parameters : 
    transmit frame 
    length of tx_frame
"""
def checkSum(buf, length):
    temp = 0
    for i in range(2 , length - 1):
        temp += buf[i]
       
    temp = temp & 0xFF
    temp = ~temp & 0xFF
    
    return temp 

"this function constructs and return the transmit frame"
def frame_to_serial(cmd , fun , data , length):

    size = length + 9                #total frame size
    frameLength = length + 2         #size of data + cmd + function
    tx_frame = [0]*size              

    "SEND FRAME : [0xF9 ,0xFF, L1,L2,L3,L4 ,cmd, function, parameter seq... , CKSM]"
    tx_frame[0] = 0xF9
    tx_frame[1] = 0xFF
    tx_frame[2] = (frameLength >> 24) & 0xFF
    tx_frame[3] = (frameLength >> 16) & 0xFF
    tx_frame[4] = (frameLength >> 8)  & 0xFF
    tx_frame[5] = (frameLength) & 0xFF 
    tx_frame[6] = cmd
    tx_frame[7] = fun
    tx_frame[8:] = data
    tx_frame.append(checkSum(tx_frame , len(tx_frame)))

    return tx_frame

def data_from_serial(frame):

    CKSM = checkSum(frame , len(frame))
    if frame[0] != 0xF9 or frame[1] != 0xF5 or frame[-1] != CKSM:
        mdebug(5, "Invalid frame ")
        return None

    return frame[7] ,frame[8 : -1]    

class FirmwareFile(object):
    HEX_FILE_EXTENSIONS = ('hex', 'ihx', 'ihex')

    def __init__(self, path):
        """
        Read a firmware file and store its data ready for device programming.

        This class will try to guess the file type if python-magic is available.

        If python-magic indicates a plain text file, and if IntelHex is
        available, then the file will be treated as one of Intel HEX format.

        In all other cases, the file will be treated as a raw binary file.

        In both cases, the file's contents are stored in bytes for subsequent
        usage to program a device or to perform a crc check.

        Parameters:
            path -- A str with the path to the firmware file.

        Attributes:
            bytes: A bytearray with firmware contents ready to send to the
            device
        """
        self._crc32 = None

        try:
            from magic import from_file
            file_type = from_file(path, mime=True)

            if file_type == 'text/plain':
                mdebug(10, "Firmware file: Intel Hex")
                self.__read_hex(path)
            elif file_type == 'application/octet-stream':
                mdebug(10, "Firmware file: Raw Binary")
                self.__read_bin(path)
            else:
                error_str = "Could not determine firmware type. Magic " \
                            "indicates '%s'" % (file_type)
                raise CmdException(error_str)
        except ImportError:
            if os.path.splitext(path)[1][1:] in self.HEX_FILE_EXTENSIONS:
                mdebug(10, "Your firmware looks like an Intel Hex file")
                self.__read_hex(path)
            else:
                mdebug(10, "Cannot auto-detect firmware filetype: Assuming .bin")
                self.__read_bin(path)

            mdebug(10, "For more solid firmware type auto-detection, install "
                       "python-magic.")
            mdebug(10, "Please see the readme for more details.")

    def __read_hex(self, path):
        try:
            from intelhex import IntelHex
            self.bytes = bytearray(IntelHex(path).tobinarray())
        except ImportError:
            error_str = "Firmware is Intel Hex, but the IntelHex library " \
                        "could not be imported.\n" \
                        "Install IntelHex in site-packages or program " \
                        "your device with a raw binary (.bin) file.\n" \
                        "Please see the readme for more details."
            raise CmdException(error_str)

    def __read_bin(self, path):
        with open(path, 'rb') as f:
            self.bytes = bytearray(f.read())


    def crc32(self, data):
        """
        Return the crc32 checksum of the firmware image

        Return:
            The firmware's CRC32, ready for comparison with the CRC
            returned by the ROM bootloader's COMMAND_CRC32
        """
        self._crc32 = 0xFFFFFFFF

        for byte in data:
            self._crc32  = self._crc32 ^ byte

            for _ in range(8):
                mask = -(self._crc32 & 1)
                self._crc32= (self._crc32 >> 1) ^ (CRC32_POLY & mask)
        return self._crc32 & 0xFFFFFFFF


def version():
    # Get the version using "git describe".
    try:
        p = Popen(['git', 'describe', '--tags', '--match', '[0-9]*'],
                  stdout=PIPE, stderr=PIPE)
        p.stderr.close()
        line = p.stdout.readlines()[0]
        return line.decode('utf-8').strip()
    except:
        # We're not in a git repo, or git failed, use fixed version string.
        return __version__

def query_yes_no(question, default="yes"):
    valid = {"yes": True,
             "y": True,
             "ye": True,
             "no": False,
             "n": False}
    if default == None:
        prompt = " [y/n] "
    elif default == "yes":
        prompt = " [Y/n] "
    elif default == "no":
        prompt = " [y/N] "
    else:
        raise ValueError("invalid default answer: '%s'" % default)

    while True:
        sys.stdout.write(question + prompt)
        choice = input().lower()
        if default != None and choice == '':
            return valid[default]
        elif choice in valid:
            return valid[choice]
        else:
            sys.stdout.write("Please respond with 'yes' or 'no' "
                             "(or 'y' or 'n').\n")    

class commandInterface(object):
    def __init__(self):
        # Some defaults. The child can override.
        self.flash_start_addr = 0x00000000
        self.page_size = 2048

    def erase(self):

        """
        initiates erase operation by sending command frame to serial port

        SEND FRAME: [0xf9, 0xff, L1,L2,L3,L4 , cmd , function(erase) ,CKSM]

        then waits for RESPONSE FRAME from serial. Upon receiving the ACK as OK , 
        it confirms the erase operation is done. and exit from while loop
        
        if the ACK data is FAIL, it raises a cmdException 

        if the response frame is not received , it prints a debug message 
        """
        transferred = device.write(0x01, frame_to_serial(FOR_WRITE , ERASE , [] , 0), timeout = 2000)
        mdebug(10 , "performing mass erase")
        while True:
            frame = read_ser(MAX_BUFF_LEN)
            if frame:
                fun , data = data_from_serial(frame)
                if fun == ACK and data[0] == OK:
                    mdebug(10, "Erase done")
                    break
                elif fun == ACK and data[0] == FAIL:
                    raise CmdException("Erase Failed")
            else:
                mdebug(10, "erase: no data from serial")

    def erase_page(self , startaddr , endaddr):

        """
        initiates a range erase operation by sending a command frame containing the 
        start and end addresses of the range to be erased.

        SEND FRAME: [0xf9, 0xff, L1,L2,L3,L4 , cmd , function(PAGE_ERASE), SA1,SA2,SA3,SA4,EA1,EA2,EA3,EA4 ,CKSM]

        then waits for RESPONSE FRAME from serial. Upon receiving the ACK as OK , 
        it confirms the partial erase operation is done. and exit from while loop
        
        if the ACK data is FAIL, it raises a cmdException 

        if the response frame is not received , it prints a debug message 
        """
        transferred = device.write(0x01, frame_to_serial(FOR_WRITE , PAGE_ERASE , [(startaddr >> 24 ) & 0xFF, (startaddr >> 16) & 0xFF , 
        (startaddr >> 8) & 0xFF, (startaddr) & 0xFF , (endaddr >>24) & 0xFF , (endaddr >>16) & 0xFF, 
        (endaddr >> 8) & 0xFF , (endaddr) & 0xFF] , 8))
        
        mdebug(10 , "performing range erase")
        while True:
            frame = read_ser(MAX_BUFF_LEN)
            if frame:
                fun , data = data_from_serial(frame)
                if fun == ACK and data[0] == OK:
                    mdebug(10,"partial erase done")
                    break
                elif fun == ACK and data[0] == FAIL:
                    raise CmdException("page erase failed")
            else:
                mdebug(10, "page erase: no data from serial")

    def writeMemory(self, addr):

        """
        this method send firmware data

        1. first send a frame containing the targets memory address and length of the firmware data

        SEND FRAME: [0xf9, 0xff, L1,L2,L3,L4, cmd, fun(WRITE) , A1,A2,A3,A4,L1,L2,L3,L4, CKSM]

        2. next sends the firmware bytes to serial

        after sends the firmware bytes , it waits for responseframe ,

          if ACK data is OK , print debug message and it proceeds to start the application .break the while loop
          if fails , raises a cmd exception

        if response frame is not received , print debug message
        """
        length = len(firmware.bytes)
        mdebug(10,f"length of data to write: {length}")
        transferred = device.write(0x01, frame_to_serial(FOR_WRITE, WRITE, [(addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, (addr) & 0xFF , 
          (length >> 24) & 0xFF, (length >> 16) & 0xFF, (length >> 8) & 0xFF, (length) & 0xFF ] , 8))
        mdebug(10, "write start")
        tx_data = [0]*length
        tx_data[:length] = firmware.bytes
        chunk_size = 64
        bytes_to_write = length
        offset = 0
        pbar = tqdm(desc = "writing" , total = bytes_to_write)
        while bytes_to_write > 0:
            chunk = tx_data[offset:offset + chunk_size]
            transferred = device.write(0x01, chunk, timeout=1000)
            bytes_to_write -= transferred
            offset += transferred
            pbar.update(transferred)
        pbar.close()            
        mdebug(10, "All data transferred")
        while True:
            frame =  read_ser(MAX_BUFF_LEN)
            if frame:
                fun , data = data_from_serial(frame)
                if fun == ACK and data[0] == OK:
                    mdebug(10, "write done ")
                    self.startApp()
                    break
                elif fun == ACK and data[0] == FAIL:
                    raise CmdException("write failed")
            else:
                mdebug(10 , "write : no data from serial")
    
    def startApp(self):

        """
        this method for start the appplication 

        SEND FRAME:[0xf9, 0xff, L1,L2,L3,L4, cmd, function(START_APP) , CKSM]

        SAME as above methods
        """
        transferred = device.write(0x01, frame_to_serial(FOR_WRITE , START_APP , [] , 0))
        while True:
            frame =  read_ser(MAX_BUFF_LEN)
            if frame:
                fun , data = data_from_serial(frame)
                if fun == ACK and data[0] == OK:
                    mdebug(10 , "app started")
                    mdebug(5, "Done")
                    mdebug(5, "Reconnect PORT11 to start Application")
                    break
                elif fun == ACK and data[0] == FAIL:
                    raise CmdException("app not started")
            else:
                mdebug(10 ,"app : no data from serial")

    def verify(self ,addr , length):

        """
        This method verifies the written firmware data by comparing its CRC32 
        checksum with that of the data stored in the target device's memory.
        
        transmit frame containing the CRC32 checksum, memory address, and length of the data to be verified.

        SEND FRAME: SEND FRAME: [0xf9, 0xff, L1,L2,L3,L4, cmd, fun(VERIFY) , C1,C2,C3,C4,A1,A2,A3,A4,L1,L2,L3,L4, CKSM]

        SAME as above methods
        """
        mdebug(10, "Verifying by comparing CRC32 calculations.")
        crc = firmware.crc32(firmware.bytes)
        transferred = device.write(0x01, frame_to_serial(FOR_WRITE , VERIFY ,[(crc >> 24) & 0xFF ,(crc >> 16) & 0xFF , (crc >> 8) & 0xFF, (crc) & 0xFF,
            (addr >> 24) & 0xFF , (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, (addr) & 0xFF , (length >> 24) & 0xFF , 
            (length >> 16) & 0xFF, (length >> 8) & 0xFF, (length) & 0xFF] , 12))
                       
        while True:
            frame = read_ser(MAX_BUFF_LEN)
            if frame:
                fun , data = data_from_serial(frame)
                if fun == ACK and data[0] == OK:
                    mdebug(10 , "verification done")
                    break
                elif fun == ACK  and data[0] == FAIL:
                    raise CmdException("verification failed")
            else:
                mdebug(10 , "verify : no data from serial")

    def readMemory(self , addr ,length):

        """
        This method reads data from the target device's memory starting from the specified address (addr) 
        and of the specified length (length).

        It constructs a transmit frame containing the memory address and length of the data to be read.
        
        SEND FRAME: [0xf9, 0xff, L1,L2,L3,L4, cmd, fun(READ) , A1,A2,A3,A4,L1,L2,L3,L4, CKSM]

        The read data is written to a file specified in the command-line arguments (args.file).

        It continues reading until the specified length of data is received.

        If acknowledgment indicating failure is received during the read operation, 
        it raises a CmdException.
        """

        mdebug(10 ,"reading")
        transferred = device.write(0x01, frame_to_serial(FOR_READ, READ, [(addr >> 24) & 0xFF , (addr >> 16) & 0xFF , (addr >> 8) & 0xFF, 
            (addr) & 0xFF  , (length >> 24) & 0xFF , (length >> 16) & 0xFF , (length >> 8) & 0xFF, (length) & 0xFF] , 8))
           
        data_len = length
        f = open(args.file , 'wb' , buffering = length)
        
        while True:
            frame = read_ser(MAX_BUFF_LEN)
            if frame:
                fun , data = data_from_serial(frame)
                if fun == DATA:
                    f.write(data)
                    data_len -= len(data)  
                    if data_len < 1:
                        mdebug(10 , "read completed")
                        break 
                elif fun == ACK and data[0] == FAIL:
                    raise CmdException(" read failed") 
            else:
                mdebug(10 , " read: no data from serial")

                           
def read_ser(num_bytes=1):
    #function for reading and return the data rfrom serial
    try:
        data = device.read(0x82, MAX_BUFF_LEN, timeout=30000)
        mdebug(10,f"Received data {len(data)} bytes")
        return data
    except Exception as e:
        mdebug(5,f"Error: {e}")



def cli_setup():
    """
    This function sets up an argument parser to handle command-line arguments

    parses the command line arguments and return the parsed result
    """
    parser = argparse.ArgumentParser()

    parser.add_argument('-q', action='store_true', help='Quiet')
    parser.add_argument('-V', action='store_true', help='Verbose')
    parser.add_argument('-f', '--force', action='store_true', help='Force operation(s) without asking any questions')
    parser.add_argument('-e', '--erase', action='store_true', help='Mass erase')
    parser.add_argument('-E', '--erase-page', help='Receives an address(a) range , default is address(a) eg: -E a,0x00000000,0x00001000 ')
    parser.add_argument('-w', '--write', action='store_true', help='Write')
    parser.add_argument('-W', '--erase-write', action='store_true', help='Write after erasing section to write to (avoids a mass erase). Rounds up section to erase if not page aligned.')
    parser.add_argument('-v', '--verify', action='store_true', help='Verify (CRC32 check) Length or size of the data to be verified is greater than 1kB .1kB <= size <= 64 KB.')
    parser.add_argument('-r', '--read', action='store_true', help='Read')
    parser.add_argument('-l', '--len', type=int, default=0x80000, help='Length of read')
    parser.add_argument('-a', '--address', type=int, help='Target address')
    parser.add_argument('-i', '--ieee-address', help='Set the secondary 64 bit IEEE address')
    parser.add_argument('--version', action='version', version='%(prog)s ' + version())
    parser.add_argument('file')

    return parser.parse_args()

def signal_handler(sig, frame):
  usb.util.release_interface(device, CDC_DATA_INTERFACE)
  usb.util.dispose_resources(device)
  print("Ctrl+C pressed. Resources released.")
  sys.exit(0)


if __name__ == "__main__" :

    global args
    args = cli_setup()

    # Set QUIET mode based on command-line arguments
    if args.V:
        QUIET = 10    #verbose
    elif args.q:
        QUIET = 0     #quiet
    
    prev = time.time()

    while not SETUP:
        try:
            signal.signal(signal.SIGINT, signal_handler)

            device = usb.core.find(idVendor = VENDOR_ID, idProduct = PRODUCT_ID)
            if device is None:
                raise Exception("device not found")

            if device.is_kernel_driver_active(CDC_DATA_INTERFACE) is True:
                device.detach_kernal_driver(CDC_DATA_INTERFACE)

            device.set_configuration()
            usb.util.claim_interface(device, CDC_DATA_INTERFACE)

            SETUP = True
            transferred = device.write(0x01, frame_to_serial(FOR_WRITE , START , [] , 0))
            mdebug(5, "Connecting to device")
            while not BSL:
                mdebug(10, "not BSL")
                frame = read_ser(MAX_BUFF_LEN)
                if frame:
                    fun, data = data_from_serial(frame)
                    if fun == ACK and data[0] == OK:
                        BSL = True
                        mdebug(5, "Bootloader active")
                        break
                    elif fun == ACK and data[0] == FAIL:                        
                        mdebug(10 , " not in BSL mode")
                        # Exit if not in bootloader mode 
                        """send EXIT command for restart the esp
                        SEND FRAME: SEND FRAME: [0xf9, 0xff, L1,L2,L3,L4 , cmd , function(EXIT) ,CKSM]
                        """
                        sys.exit(0)
                    else:
                        mdebug(10, "else cdtn")
                        time.sleep(0.1)
        except Exception as e:
            mdebug(5, f"Error: {e} , RECONNECT PORT11 and TRY")
            sys.exit(0)

    if BSL:    #if bootloader is active  
        try:
            # Check if conflicting operations are specified and prompt for confirmation
            if (args.write and args.read) or (args.erase_write and args.read):
                if not (args.force or
                        query_yes_no("You are reading and writing to the same "
                                     "file. This will overwrite your input file. "
                                     "Do you want to continue?", "no")):
                    raise Exception('Aborted by user.')

            if ((args.erase and args.read) or (args.erase_page and args.read)
                and not (args.write or args.erase_write)):
                if not (args.force or
                        query_yes_no("You are about to erase your target before "
                                     "reading. Do you want to continue?", "no")):
                    raise Exception('Aborted by user.')

            if (args.read and not (args.write  or args.erase_write)
                and args.verify):
                raise Exception('Verify after read not implemented.')

            if args.len < 0:
                raise Exception('Length must be positive but %d was provided'
                                % (args.len,))
 
            cmd =  commandInterface()         # Create an instance of commandInterface class

            if args.write or args.erase_write or args.verify:
                # Read data from the specified file for write, erase-write, or verify operations
                mdebug(10, "Reading data from %s" % args.file)
                firmware = FirmwareFile(args.file) 

            mdebug(10, "connecting to target ")

            if not args.address:
                args.address = cmd.flash_start_addr        # Use default flash start address if not specified
           
            if args.erase:
                #perform mass erase operation 
                cmd.erase()

            if args.erase_page:
                # Perform page erase operation for the specified address range
                values = args.erase_page.split(',')
                if len(values) == 3 and values[0] in ('a'):
                    range_type, start_addr, end_addr = values
                    if range_type == 'a':
                        cmd.erase_page(eval(start_addr) , eval(end_addr))
                    else:
                        mdebug(10, " Invalid range type.please specify 'a' for address range ")
                else:
                    mdebug(10 , "Invalid format for erase page argument. Please provide in the format '-E a,0x00000000,0x00001000'.")

            if args.write:
                # Write data to memory
                cmd.writeMemory(args.address)

            if args.erase_write:
                cmd.erase()     #erase before write
                cmd.writeMemory(args.address)   #write

            if args.verify:
                #perform verify operation      
                cmd.verify(args.address , args.len)
                
            if args.read:
                #read data from memory
                cmd.readMemory(args.address , args.len)
            
            #send exit frame after all the operation is success
            # device.write(frame_to_serial(FOR_WRITE , EXIT , [] , 0))

        except Exception as e:
            mdebug(5, f"Error: {e}")
            sys.exit(0)
        finally:
            usb.util.release_interface(device, CDC_DATA_INTERFACE)
            usb.util.dispose_resources(device)
            mdebug(10,"Resources released")
            







