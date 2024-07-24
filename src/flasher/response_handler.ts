import { Response } from "./types";
import { CH_loader } from "./ch_loader";
export class ResponseHandler {
  static responseToString(response: Response): string {
    switch (response.type) {
      case "Ok":
        return `OK[${Buffer.from(response.data).toString("hex")}]`;
      case "Err":
        return `ERROR(${response.code.toString(16)})[${Buffer.from(
          response.data
        ).toString("hex")}]`;
    }
  }
  static isOk(response: Response): boolean {
    return response.type === "Ok";
  }
  static payload(response: Response): Uint8Array {
    return response.type === "Ok" ? response.data : response.data;
  }
  static fromRaw(raw: Uint8Array): Response {
    const len = new DataView(raw.buffer).getUint16(2, true);
    const remain = raw.subarray(4);
    const CKSM: number = CH_loader.checkSum(raw, raw.length);
    if (raw[0] !== 0xf9 || raw[1] !== 0xf5 || raw[raw.length - 1] !== CKSM) {
      return { type: "Err", code: raw[1], data: raw };
    }

    const cmd = raw[7];
    const data = raw.subarray(8, raw.length - 1);
    if (cmd == CH_loader.ACK && raw[0] == CH_loader.OK)
      return { type: "Ok", data: raw };
    else if (cmd == CH_loader.ACK && raw[0] == CH_loader.FAIL)
      return { type: "Err", code: raw[1], data: raw };
    return { type: "Err", code: raw[1], data: raw };
  }
}
