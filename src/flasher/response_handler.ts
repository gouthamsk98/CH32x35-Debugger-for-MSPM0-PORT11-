import { Response } from "./types";
import { CH_loader } from "./ch_loader";
import { ErrCode } from "./types";
export class ResponseHandler {
  // static responseToString(response: Response): string {
  //   switch (response.type) {
  //     case "Ok":
  //       return `OK[${Buffer.from(response.data).toString("hex")}]`;
  //     case "Err":
  //       return `ERROR(${response.code.toString(16)})[${Buffer.from(
  //         response.data
  //       ).toString("hex")}]`;
  //   }
  // }
  static isOk(response: Response): boolean {
    return response.type === "Ok";
  }
  static payload(response: Response): Uint8Array {
    return response.type === "Ok" ? response.data : new Uint8Array();
  }
  static async fromRaw(raw: Uint8Array): Promise<Response> {
    const CKSM: number = CH_loader.checkSum(raw, raw.length);
    if (raw[0] !== 0xf9 || raw[1] !== 0xf5 || raw[raw.length - 1] !== CKSM) {
      return { type: "Err", code: ErrCode.invaid_frame, data: raw };
    }
    const cmd = raw[7];
    if (cmd == CH_loader.ACK && raw[8] == CH_loader.OK)
      return { type: "Ok", data: raw };
    else if (cmd == CH_loader.ACK && raw[8] == CH_loader.FAIL)
      return { type: "Err", code: ErrCode.operation_failed, data: raw };
    return { type: "Err", code: ErrCode.unknown_err, data: raw };
  }
}
