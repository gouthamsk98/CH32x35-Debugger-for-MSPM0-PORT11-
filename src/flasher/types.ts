export type Response =
  | { type: "Ok"; data: Uint8Array }
  | { type: "Err"; code: ErrCode; data?: Uint8Array };
export enum ErrCode {
  "invaid_frame" = "data recived from usb frame is invaid",
  "operation_failed" = "the current opertion failed",
  "read_timeout" = "reder has timeout",
  "unknown_err" = "unkown error",
}
