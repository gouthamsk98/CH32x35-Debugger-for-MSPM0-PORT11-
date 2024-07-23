export function connect(element: HTMLButtonElement) {
  element.addEventListener("click", () => {
    navigator.serial.requestPort().then((port) => {
      console.log(port.getInfo());
    });
  });
}
