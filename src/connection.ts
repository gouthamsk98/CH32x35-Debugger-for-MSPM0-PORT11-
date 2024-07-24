import { CH_loader } from "./flasher/ch_loader";
let connection = false;
let loader: CH_loader;
const filters = [{ vendorId: 0x6249, productId: 0x7031 }];
export function connect(element: HTMLButtonElement) {
  element.addEventListener("click", () => {
    navigator.usb
      .requestDevice({ filters: filters })
      .then(async (device) => {
        loader = new CH_loader(device);
        CH_loader.openNth(0);
        CH_loader.debugLog("Connected");
        connection = !connection;
        element.innerHTML = `Disconnect`;
      })
      .catch((error) => {
        console.error(error);
        element.innerHTML = `Connect`;
      });
  });
}
export function erase(element: HTMLButtonElement) {
  element.addEventListener("click", async () => {
    if (!connection) {
      CH_loader.debugLog("Please Connect First");
      return;
    }
    element.innerHTML = `Erasing...`;
    await loader.eraseFlash();
    element.innerHTML = `Erase`;
  });
}
