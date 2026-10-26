// Glue between the serial protocol, the controls, and the canvas preview.
import { SerialSession } from "./serial.js";
import { WIDGETS, TEMPLATES, THEME_KEYS, DEFAULT_THEME, defaultSample } from "./models.js";
import { renderPreview } from "./preview.js";

const session = new SerialSession();
const state = {
  layout: null,
  theme: structuredClone(DEFAULT_THEME),
  pageIdx: 0,
  sample: defaultSample(),
};

const $ = (id) => document.getElementById(id);

function log(msg) {
  const el = $("log");
  el.textContent += msg + "\n";
  el.scrollTop = el.scrollHeight;
}

session.onLog((line) => log(`[dev] ${line}`));

function currentPage() {
  if (!state.layout) return { title: "ride", widgets: [1, 4, 5, 6, 7, 8], index: 0, total: 1 };
  const p = state.layout.pages[state.pageIdx];
  return { title: p.title, widgets: p.widgets, index: state.pageIdx, total: state.layout.pages.length };
}

function draw() {
  renderPreview($("preview"), currentPage(), state.theme, state.sample);
}

function buildThemeControls() {
  const wrap = $("themeControls");
  wrap.innerHTML = "";
  for (const [key, label] of THEME_KEYS) {
    const row = document.createElement("label");
    row.className = "row";
    row.innerHTML = `<span>${label}</span>`;
    const input = document.createElement("input");
    input.type = "color";
    input.value = state.theme[key];
    input.addEventListener("input", () => {
      state.theme[key] = input.value.toUpperCase();
      draw();
    });
    row.appendChild(input);
    wrap.appendChild(row);
  }
  const scaleRow = document.createElement("label");
  scaleRow.className = "row";
  scaleRow.innerHTML = `<span>Font scale</span>`;
  const scale = document.createElement("input");
  scale.type = "range";
  scale.min = "50";
  scale.max = "200";
  scale.value = String(state.theme.font_scale);
  scale.addEventListener("input", () => {
    state.theme.font_scale = Number(scale.value);
    $("scaleVal").textContent = `${scale.value}%`;
    draw();
  });
  scaleRow.appendChild(scale);
  const val = document.createElement("span");
  val.id = "scaleVal";
  val.textContent = `${state.theme.font_scale}%`;
  scaleRow.appendChild(val);
  wrap.appendChild(scaleRow);
}

function buildLayoutControls() {
  const pages = $("pages");
  pages.innerHTML = "";
  if (!state.layout) {
    pages.innerHTML = "<p>No layout loaded. Connect a device first.</p>";
    return;
  }
  state.layout.pages.forEach((p, i) => {
    const btn = document.createElement("button");
    btn.textContent = `${i + 1}. ${p.title}`;
    btn.className = i === state.pageIdx ? "active" : "";
    btn.addEventListener("click", () => {
      state.pageIdx = i;
      buildLayoutControls();
      buildPageEditor();
      draw();
    });
    pages.appendChild(btn);
  });
}

function buildPageEditor() {
  const ed = $("pageEditor");
  ed.innerHTML = "";
  const p = state.layout?.pages[state.pageIdx];
  if (!p) return;
  const titleLabel = document.createElement("label");
  titleLabel.className = "row";
  titleLabel.innerHTML = "<span>Title</span>";
  const title = document.createElement("input");
  title.value = p.title;
  title.maxLength = 15;
  title.addEventListener("input", () => {
    p.title = title.value;
    draw();
  });
  titleLabel.appendChild(title);
  ed.appendChild(titleLabel);

  const tplLabel = document.createElement("label");
  tplLabel.className = "row";
  tplLabel.innerHTML = "<span>Template</span>";
  const tpl = document.createElement("select");
  TEMPLATES.forEach((name, id) => {
    const opt = document.createElement("option");
    opt.value = String(id);
    opt.textContent = name;
    if (id === p.template) opt.selected = true;
    tpl.appendChild(opt);
  });
  tpl.addEventListener("change", () => {
    p.template = Number(tpl.value);
  });
  tplLabel.appendChild(tpl);
  ed.appendChild(tplLabel);

  p.widgets.forEach((wid, slot) => {
    const row = document.createElement("label");
    row.className = "row";
    row.innerHTML = `<span>Slot ${slot + 1}</span>`;
    const sel = document.createElement("select");
    WIDGETS.forEach((name, id) => {
      const opt = document.createElement("option");
      opt.value = String(id);
      opt.textContent = name;
      if (id === wid) opt.selected = true;
      sel.appendChild(opt);
    });
    sel.addEventListener("change", () => {
      p.widgets[slot] = Number(sel.value);
      draw();
    });
    row.appendChild(sel);
    ed.appendChild(row);
  });
}

function buildSampleControls() {
  const wrap = $("sampleControls");
  wrap.innerHTML = "";
  const defs = [
    ["speedKmh", "Speed", 0, 80, 1],
    ["distanceKm", "Distance", 0, 200, 0.5],
    ["cadence", "Cadence", -1, 130, 1],
    ["hr", "HR", -1, 200, 1],
    ["power", "Power", -1, 800, 5],
  ];
  for (const [key, label, min, max, step] of defs) {
    const row = document.createElement("label");
    row.className = "row";
    const val = document.createElement("span");
    const input = document.createElement("input");
    input.type = "range";
    input.min = String(min);
    input.max = String(max);
    input.step = String(step);
    input.value = String(state.sample[key]);
    const refresh = () => {
      state.sample[key] = Number(input.value);
      val.textContent = `${label}: ${input.value}`;
      draw();
    };
    input.addEventListener("input", refresh);
    row.innerHTML = `<span>${label}</span>`;
    row.appendChild(input);
    row.appendChild(val);
    wrap.appendChild(row);
    refresh();
  }
}

async function requireHello() {
  const hello = await session.command("HELLO");
  if (!hello.ok) throw new Error("handshake failed");
  $("devInfo").textContent =
    `FW ${hello.fw} · proto ${hello.proto} · layout schema ${hello.layout_schema} · theme schema ${hello.theme_schema}`;
  log(`connected: ${JSON.stringify(hello)}`);
}

async function loadFromDevice() {
  const layoutRes = await session.command("GET_LAYOUT");
  if (!layoutRes.ok) throw new Error("GET_LAYOUT failed");
  state.layout = layoutRes.layout;
  state.pageIdx = 0;
  const themeRes = await session.command("GET_THEME");
  if (!themeRes.ok) throw new Error("GET_THEME failed");
  state.theme = themeRes.theme;
  buildLayoutControls();
  buildPageEditor();
  buildThemeControls();
  draw();
  log(`loaded ${state.layout.pages.length} pages from device`);
}

$("connect").addEventListener("click", async () => {
  try {
    if (!session.supported) {
      alert("Web Serial is not supported. Use Chrome or Edge on desktop.");
      return;
    }
    await session.connect();
    $("connect").disabled = true;
    $("disconnect").disabled = false;
    await requireHello();
    await loadFromDevice();
  } catch (e) {
    log(`connect failed: ${e.message}`);
  }
});

$("disconnect").addEventListener("click", async () => {
  await session.disconnect();
  $("connect").disabled = false;
  $("disconnect").disabled = true;
  $("devInfo").textContent = "disconnected";
});

$("pushTheme").addEventListener("click", async () => {
  try {
    const res = await session.command("SET_THEME", JSON.stringify(state.theme));
    log(res.ok ? "theme applied on device" : `theme rejected: ${res.err}`);
  } catch (e) {
    log(`push theme failed: ${e.message}`);
  }
});

$("pushLayout").addEventListener("click", async () => {
  try {
    const payload = {
      page_count: state.layout.pages.length,
      schema: state.layout.schema,
      pages: state.layout.pages,
    };
    const res = await session.command("SET_LAYOUT", JSON.stringify(payload));
    log(res.ok ? "layout applied on device" : `layout rejected: ${res.err}`);
  } catch (e) {
    log(`push layout failed: ${e.message}`);
  }
});

$("resetAll").addEventListener("click", async () => {
  try {
    const res = await session.command("RESET_ALL");
    if (res.ok) await loadFromDevice();
  } catch (e) {
    log(`reset failed: ${e.message}`);
  }
});

$("exportTheme").addEventListener("click", () => {
  const blob = new Blob([JSON.stringify(state.theme, null, 2)], { type: "application/json" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "opencyclo-theme.json";
  a.click();
});

$("importTheme").addEventListener("change", async (e) => {
  const file = e.target.files[0];
  if (!file) return;
  state.theme = JSON.parse(await file.text());
  buildThemeControls();
  draw();
});

buildThemeControls();
buildSampleControls();
draw();
