import "./style.css";

import { connect } from "./connection.ts";

document.querySelector<HTMLDivElement>("#app")!.innerHTML = `
  <div>
    <h1>Port11 ch32x35_debugger</h1>
    <button id="connect" type="button">Connect</button>
      <button id="getInfo" type="button">Get Info</button>
      <label for="myfile">Upload a Hex:</label>
      <input type="file" id="myfile" name="myfile"><br><br>
      <button id="erase" type="button">Erase</button>
      <button id="flash" type="button">Flash</button>
      <textarea id="console" rows="15" cols="50" readonly></textarea>
  </div>
`;
connect(document.querySelector<HTMLButtonElement>("#connect")!);
