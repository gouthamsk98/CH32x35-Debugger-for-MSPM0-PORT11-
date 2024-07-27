import { UsbTransport } from "./transport_handler";
import { Response } from "./types";
import { ErrCode } from "./types";
export class CH_loader extends UsbTransport {
  FLASH_ADDRESS = 0x00000000;
  BSL_ENABLED = false;
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
    super(device);
  }
  static checkSum(buf: Uint8Array, length: number): number {
    console.log("data is", buf, length);
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

    // SEND FRAME: [0xF9, 0xFF, L1, L2, L3, L4, cmd, function, parameter seq..., CKSM]
    txFrame[0] = 0xf9;
    txFrame[1] = 0xff;
    txFrame[2] = (frameLength >> 24) & 0xff;
    txFrame[3] = (frameLength >> 16) & 0xff;
    txFrame[4] = (frameLength >> 8) & 0xff;
    txFrame[5] = frameLength & 0xff;
    txFrame[6] = cmd;
    txFrame[7] = fun;
    txFrame.set(data, 8); // Insert 'data' starting at index 8
    txFrame[txFrame.length - 1] = CH_loader.checkSum(
      txFrame,
      txFrame.length - 1
    );

    return txFrame;
  }
  async enableBSL() {
    CH_loader.debugLog("Enabling Bootloader ...");
    const frame = this.frameToUSB(
      this.FOR_WRITE,
      this.START,
      new Uint8Array(),
      0
    );
    console.log("frame is 1", frame);
    await this.sendRaw(frame);
    while (true) {
      const res: Response = await this.recv();
      console.log("res 1", res);
      if (res.type == "Ok") {
        this.BSL_ENABLED = true;
        CH_loader.debugLog("BSL enabled");
        break;
      } else if (res.type == "Err" && res.code == ErrCode.operation_failed) {
        CH_loader.debugLog(res.code);
        CH_loader.debugLog("Reconnect the device");
      }
      CH_loader.debugLog("...");
      await new Promise((resolve) => setTimeout(resolve, 0));
    }
  }
  intelHexToUint8Array(hexString: string) {
    const lines = hexString.trim().split("\n");
    const data: Array<number> = [];
    lines.forEach((line) => {
      if (line.startsWith(":")) {
        const byteCount = parseInt(line.substr(1, 2), 16);
        const dataStartIndex = 9; // Data starts after 9 characters (: + 2-byte count + 4-byte address + 2-byte record type)
        const dataEndIndex = dataStartIndex + byteCount * 2;

        for (let i = dataStartIndex; i < dataEndIndex; i += 2) {
          data.push(parseInt(line.substr(i, 2), 16));
        }
      }
    });

    return new Uint8Array(data);
  }
  async eraseFlash() {
    if (!this.BSL_ENABLED) await this.enableBSL();
    const frame = this.frameToUSB(
      this.FOR_WRITE,
      this.ERASE,
      new Uint8Array(),
      0
    );
    this.sendRaw(frame);
    const res = await this.recv();
    if (res.type == "Ok") CH_loader.debugLog("Erase Completed");
  }
  async startApp() {
    if (this.BSL_ENABLED) CH_loader.debugLog("Disabling BSL mode ...");
    const frame = this.frameToUSB(
      this.FOR_WRITE,
      this.START_APP,
      new Uint8Array(),
      0
    );
    await this.sendRaw(frame);
    while (true) {
      await new Promise((resolve) => setTimeout(resolve, 0));
      const res: Response = await this.recv();
      if (res.type == "Ok") {
        this.BSL_ENABLED = false;
        CH_loader.debugLog("App Started");
        break;
      } else if (res.type == "Err" && res.code == ErrCode.operation_failed) {
        CH_loader.debugLog(res.code);
      }
    }
  }
  async flash(hex: string) {
    await this.enableBSL();
    CH_loader.debugLog("Flashing started ...");
    const hexData = this.intelHexToUint8Array(hex);
    const length = hexData.length;
    const tragetData = new Uint8Array([
      (this.FLASH_ADDRESS >> 24) & 0xff,
      (this.FLASH_ADDRESS >> 16) & 0xff,
      (this.FLASH_ADDRESS >> 8) & 0xff,
      this.FLASH_ADDRESS & 0xff,
      (length >> 24) & 0xff,
      (length >> 16) & 0xff,
      (length >> 8) & 0xff,
      length & 0xff,
    ]);
    const frame = this.frameToUSB(this.FOR_WRITE, this.WRITE, tragetData, 8);
    const writeData = await this.sendRaw(frame);
    console.log(
      "data transfer is",
      frame,
      writeData.bytesWritten,
      writeData.status
    );
    let transferred = writeData.bytesWritten;
    const txData = new Uint8Array(length);
    txData.set(hexData.subarray(0, length));
    const chunkSize = 64;
    let bytesToWrite = length;
    let offset = 0;
    while (bytesToWrite > 0) {
      const chunk = txData.subarray(offset, offset + chunkSize);
      const writeData = await this.sendRaw(chunk);
      console.log(
        "data transfer is",
        chunk,
        writeData.bytesWritten,
        writeData.status
      );
      transferred = writeData.bytesWritten;
      bytesToWrite -= transferred;
      offset += transferred;
      CH_loader.debugLog(`writen bytes ${offset} out of ${length} bytes`);
    }
    while (true) {
      await new Promise((resolve) => setTimeout(resolve, 0));
      const res: Response = await this.recv();
      console.log("res 1", res);
      if (res.type == "Ok") {
        CH_loader.debugLog("Flashed successfully");
        CH_loader.debugLog("Restart the device (reset).");
        break;
      } else if (res.type == "Err" && res.code == ErrCode.operation_failed) {
        CH_loader.debugLog(res.code);
        break;
      }
    }
  }
}
