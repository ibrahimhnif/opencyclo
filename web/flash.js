// Firmware flashing via esptool-js (global ESPLoader/Transport from bundle.js).
// Flash parameters mirror platformio.ini: qio mode, 80MHz, 16MB flash.
const $ = (id) => document.getElementById(id);
const logEl = $("log");

function log(msg) {
  logEl.textContent += msg + "\n";
  logEl.scrollTop = logEl.scrollHeight;
}

let port = null;
let loader = null;

const terminal = {
  clean() {},
  writeLine(data) { log(String(data)); },
  write(data) { logEl.textContent += String(data); },
};

function readFile(input) {
  const file = input.files[0];
  if (!file) return Promise.resolve(null);
  return file.arrayBuffer().then((buf) => new Uint8Array(buf));
}

$("connect").addEventListener("click", async () => {
  try {
    if (typeof ESPLoader === "undefined" || typeof Transport === "undefined") {
      log("esptool-js failed to load from CDN. Check network and reload.");
      return;
    }
    port = await navigator.serial.requestPort();
    const transport = new Transport(port, true);
    loader = new ESPLoader({ transport, baudrate: 115200, terminal });
    const chip = await loader.main();
    $("chip").textContent = `connected: ${chip}`;
    $("flash").disabled = false;
    $("erase").disabled = false;
    log(`connected: ${chip}`);
  } catch (e) {
    log(`connect failed: ${e.message}`);
  }
});

$("flash").addEventListener("click", async () => {
  try {
    const parts = [
      { file: await readFile($("bootloader")), address: 0x0, name: "bootloader" },
      { file: await readFile($("partitions")), address: 0x8000, name: "partitions" },
      { file: await readFile($("firmware")), address: 0x10000, name: "firmware" },
    ].filter((p) => p.file !== null);
    if (parts.length === 0) {
      log("pick at least one .bin file first");
      return;
    }
    $("flash").disabled = true;
    await loader.writeFlash({
      fileArray: parts.map((p) => ({ data: p.file, address: p.address })),
      flashMode: "qio",
      flashFreq: "80m",
      flashSize: "16MB",
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        const pct = Math.round((written / total) * 100);
        $("progress").value = pct;
        log(`${parts[fileIndex].name}: ${pct}%`);
      },
    });
    log("flash complete, resetting device");
    await loader.after("hard_reset");
  } catch (e) {
    log(`flash failed: ${e.message}`);
  } finally {
    $("flash").disabled = false;
  }
});

$("erase").addEventListener("click", async () => {
  try {
    $("erase").disabled = true;
    await loader.eraseFlash();
    log("erase complete");
  } catch (e) {
    log(`erase failed: ${e.message}`);
  } finally {
    $("erase").disabled = false;
  }
});
