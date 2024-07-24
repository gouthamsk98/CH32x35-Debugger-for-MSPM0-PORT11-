import { UsbTransport } from "./transport_handler";

export class CH_loader extends UsbTransport {
  static VENDOR_ID = 0x6249;
  static PRODUCT_ID = 0x7031;
  static CDC_DATA_INTERFACE = 0;
  //FUNCTIONS
  START = 0x01;
  ERASE = 0x02;
  PAGE_ERASE = 0x03;
  WRITE = 0x04;
  READ = 0x05;
  VERIFY = 0x06;
  START_APP = 0x07;
  EXIT = 0x08;
  static ACK = 0x09;
  DATA = 0x0a;

  FOR_WRITE = 0x11;
  FOR_READ = 0x22;

  static OK = 0x01;
  static FAIL = 0x00;

  WRITE_OK = 0x08;
  constructor(device: USBDevice) {
    super(device, CH_loader.CDC_DATA_INTERFACE);
  }
  static checkSum(buf: Uint8Array, length: number): number {
    let temp = 0;

    for (let i = 2; i < length - 1; i++) {
      temp += buf[i];
    }

    temp = temp & 0xff;
    temp = ~temp & 0xff;

    return temp;
  }
  frameToUSB(
    cmd: number,
    fun: number,
    data: Uint8Array,
    length: number
  ): Uint8Array {
    const size = length + 9; // total frame size
    const frameLength = length + 2; // size of data + cmd + function
    const txFrame = new Uint8Array(size);

    // "SEND FRAME : [0xF9 ,0xFF, L1,L2,L3,L4 ,cmd, function, parameter seq... , CKSM]"
    txFrame[0] = 0xf9;
    txFrame[1] = 0xff;
    txFrame[2] = (frameLength >> 24) & 0xff;
    txFrame[3] = (frameLength >> 16) & 0xff;
    txFrame[4] = (frameLength >> 8) & 0xff;
    txFrame[5] = frameLength & 0xff;
    txFrame[6] = cmd;
    txFrame[7] = fun;

    txFrame.set(data, 8);

    const checksum = CH_loader.checkSum(txFrame, txFrame.length);
    const txFrameWithChecksum = new Uint8Array(size + 1);
    txFrameWithChecksum.set(txFrame);
    txFrameWithChecksum[size] = checksum;

    return txFrameWithChecksum;
  }
  async eraseFlash() {
    const frame = this.frameToUSB(
      this.FOR_WRITE,
      this.START,
      new Uint8Array([]),
      0
    );
    console.log("frame is", frame);
    this.sendRaw(frame);
    const res = await this.recv();
    console.log(res);
  }
}
