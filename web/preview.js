// 240x320 canvas preview. Draws the selected layout page with the active
// theme and sample telemetry: header, hero speed tile, then a 2-column tile
// grid for the remaining widgets.
import { modelSpeed, modelTile, TONE_TO_THEME } from "./models.js";

const W = 240;
const H = 320;

function toneColor(tile, theme) {
  return theme[TONE_TO_THEME[tile.tone] || "text"];
}

export function renderPreview(canvas, page, theme, sample) {
  canvas.width = W;
  canvas.height = H;
  const ctx = canvas.getContext("2d");
  const scale = theme.font_scale / 100;

  ctx.fillStyle = theme.bg;
  ctx.fillRect(0, 0, W, H);

  // Header.
  ctx.fillStyle = theme.panel;
  ctx.fillRect(0, 0, W, 30);
  ctx.fillStyle = theme.text;
  ctx.font = "bold 13px system-ui, sans-serif";
  ctx.textBaseline = "middle";
  ctx.fillText(page?.title || "ride", 8, 16);
  ctx.fillStyle = theme.label;
  ctx.font = "10px system-ui, sans-serif";
  ctx.textAlign = "right";
  ctx.fillText(`${(page?.index ?? 0) + 1}/${page?.total ?? 1}`, W - 8, 16);
  ctx.textAlign = "left";

  const widgets = page?.widgets ?? [1, 4, 5, 6, 7, 8];
  let y = 34;

  // Hero: speed widget if present, else the first tile enlarged.
  const speedIdx = widgets.indexOf(1);
  if (speedIdx >= 0) {
    const hero = modelSpeed(sample.speedKmh, sample.imperial, sample.speedSource);
    ctx.fillStyle = theme.bg;
    ctx.fillRect(4, y, W - 8, 92);
    ctx.fillStyle = theme.label;
    ctx.font = "11px system-ui, sans-serif";
    ctx.fillText(hero.source, 8, y + 12);
    ctx.fillStyle = theme.text;
    ctx.font = `bold ${Math.round(34 * scale)}px system-ui, sans-serif`;
    ctx.fillText(hero.value, 8, y + 52);
    ctx.fillStyle = theme.label;
    ctx.font = "11px system-ui, sans-serif";
    ctx.textAlign = "right";
    ctx.fillText(hero.unit, W - 8, y + 78);
    ctx.textAlign = "left";
    y += 96;
  }

  // Tile grid for everything except the hero speed.
  const rest = widgets.filter((w) => w !== 1);
  const cols = 2;
  const gap = 4;
  const tileW = (W - 8 - gap) / cols;
  const tileH = 56;
  rest.slice(0, 8).forEach((wid, i) => {
    const tile = modelTile(wid, sample);
    const tx = 4 + (i % cols) * (tileW + gap);
    const ty = y + Math.floor(i / cols) * (tileH + gap);
    if (ty + tileH > H) return;
    ctx.fillStyle = theme.bg;
    ctx.fillRect(tx, ty, tileW, tileH);
    ctx.fillStyle = theme.hairline;
    ctx.fillRect(tx, ty, tileW, 1);
    if (!tile) {
      ctx.fillStyle = theme.label;
      ctx.font = "10px system-ui, sans-serif";
      ctx.fillText(`[${wid}]`, tx + 4, ty + 20);
      return;
    }
    ctx.fillStyle = theme.label;
    ctx.font = "10px system-ui, sans-serif";
    ctx.fillText(tile.label, tx + 4, ty + 14);
    ctx.fillStyle = toneColor(tile, theme);
    ctx.font = `bold ${Math.round(15 * scale)}px system-ui, sans-serif`;
    ctx.fillText(tile.value, tx + 4, ty + 36);
  });
}
