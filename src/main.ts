import "./style.css";

import { connect, erase, flash, readFile, reset } from "./connection.ts";

document.querySelector<HTMLDivElement>("#app")!.innerHTML = `
  <div>
    <h1>Port11 ch32x35_debugger</h1>
    <button id="connect" type="button">Connect</button
      <input type="file" id="myfile" name="myfile" accept=".hex">
      <input type="file" id="myfile" name="myfile"><br><br>
      <button id="erase" type="button">Erase</button>
      <button id="flash" type="button">Flash</button>
      <button id="reset" type="button">Reset</button>
      <textarea id="console" rows="15" cols="50" readonly></textarea>
  </div>
`;
connect(document.querySelector<HTMLButtonElement>("#connect")!);
erase(document.querySelector<HTMLButtonElement>("#erase")!);
flash(document.querySelector<HTMLButtonElement>("#flash")!);
readFile(document.querySelector<HTMLInputElement>("#myfile")!);
reset(document.querySelector<HTMLButtonElement>("#reset")!);
