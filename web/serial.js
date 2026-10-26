// USB serial session for the OpenCyclo WebSerial protocol.
// Device speaks 115200 8-N-1; commands are "OC+NAME\n" or "OC+NAME:<json>\n",
// replies are single "OC=<json>\n" lines. Log lines never start with "OC=".
export class SerialSession {
  constructor() {
    this.port = null;
    this.reader = null;
    this.readBuffer = "";
    this.pending = [];
    this.logListeners = [];
    this.readLoop = null;
  }

  get supported() {
    return "serial" in navigator;
  }

  onLog(fn) {
    this.logListeners.push(fn);
  }

  async connect(baudRate = 115200) {
    this.port = await navigator.serial.requestPort();
    await this.port.open({ baudRate });
    this.startReader();
  }

  async disconnect() {
    this.readLoop = null;
    try {
      await this.reader?.cancel();
    } catch { /* already closed */ }
    this.reader = null;
    try {
      await this.port?.close();
    } catch { /* already closed */ }
    this.port = null;
    this.failAll(new Error("disconnected"));
  }

  get connected() {
    return this.port !== null && this.port.readable !== null;
  }

  startReader() {
    const decoder = new TextDecoder();
    this.reader = this.port.readable.getReader();
    const pump = async () => {
      try {
        for (;;) {
          const { value, done } = await this.reader.read();
          if (done) break;
          if (value) this.onBytes(decoder.decode(value, { stream: true }));
          if (this.reader === null) break;
        }
      } catch {
        // Port closed or lost; pending commands fail, logs keep the error.
      }
      this.failAll(new Error("serial read ended"));
    };
    pump();
  }

  onBytes(text) {
    this.readBuffer += text;
    let idx;
    while ((idx = this.readBuffer.indexOf("\n")) >= 0) {
      const line = this.readBuffer.slice(0, idx).trim();
      this.readBuffer = this.readBuffer.slice(idx + 1);
      if (line.startsWith("OC=")) {
        const entry = this.pending.shift();
        if (entry) entry.resolve(line.slice(3));
      } else if (line.length > 0) {
        for (const fn of this.logListeners) fn(line);
      }
    }
  }

  failAll(err) {
    const waiting = this.pending;
    this.pending = [];
    for (const entry of waiting) entry.reject(err);
  }

  async command(name, payload = null, timeoutMs = 5000) {
    if (!this.connected) throw new Error("not connected");
    const line = payload === null ? `OC+${name}\n` : `OC+${name}:${payload}\n`;
    const writer = this.port.writable.getWriter();
    await writer.write(new TextEncoder().encode(line));
    writer.releaseLock();
    const reply = await new Promise((resolve, reject) => {
      const entry = {};
      const timer = setTimeout(() => {
        const i = this.pending.indexOf(entry);
        if (i >= 0) this.pending.splice(i, 1);
        reject(new Error(`timeout waiting for ${name}`));
      }, timeoutMs);
      entry.resolve = (text) => {
        clearTimeout(timer);
        resolve(text);
      };
      entry.reject = (err) => {
        clearTimeout(timer);
        reject(err);
      };
      this.pending.push(entry);
    });
    return JSON.parse(reply);
  }
}
