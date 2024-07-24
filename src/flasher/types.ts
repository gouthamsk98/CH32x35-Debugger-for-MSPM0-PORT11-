export type Response =
  | { type: "Ok"; data: Uint8Array }
  | { type: "Err"; code: number; data: Uint8Array };
