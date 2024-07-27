import { CH_loader } from "./flasher/ch_loader";
let connection = false;
let loader: CH_loader;
let fileContent: string;
const filters = [{ vendorId: 0x6249, productId: 0x7031 }];
export function connect(element: HTMLButtonElement) {
  element.addEventListener("click", () => {
    if (connection && loader) {
      loader.disconnect();
      CH_loader.debugLog("Device disconnected");
      element.innerHTML = `Connect`;
      connection = false;
      return;
    }
    navigator.usb
      .requestDevice({ filters: filters })
      .then(async (device) => {
        loader = new CH_loader(device);
        await CH_loader.openNth(0);
        CH_loader.debugLog("Device connected");
        loader.enableBSL();
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
export function flash(element: HTMLButtonElement) {
  element.addEventListener("click", async () => {
    if (!connection) {
      CH_loader.debugLog("Please Connect First");
      return;
    }
    if (!fileContent) {
      CH_loader.debugLog("Please upload a .Hex file first");
      return;
    }
    element.innerHTML = `Flashing...`;
    await loader.flash(fileContent);
    element.innerHTML = `Flash`;
  });
}
function readFileAsText(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result as string);
    reader.onerror = reject;
    reader.readAsText(file);
  });
}
export function readFile(element: HTMLInputElement) {
  element.addEventListener("change", async (event) => {
    const file = (event.target as HTMLInputElement).files![0];
    if (file && file.name.endsWith(".hex")) {
      fileContent = await readFileAsText(file);
      console.log(fileContent);
    } else {
      alert("Please upload a valid .hex file");
    }
  });
}
export function reset(element: HTMLButtonElement) {
  element.addEventListener("click", async () => {
    if (!connection) {
      CH_loader.debugLog("Please Connect First");
      return;
    }

    element.innerHTML = `Resting...`;
    await loader.startApp();
    element.innerHTML = `Reset`;
  });
}
