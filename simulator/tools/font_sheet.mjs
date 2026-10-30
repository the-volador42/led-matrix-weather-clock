// Рисует строку бегущим текстом и снимает кадры подряд — проверка шрифта.
//   node tools/font_sheet.mjs [sim.wasm] [out.png] "ТЕКСТ"
import fs from 'node:fs';
import zlib from 'node:zlib';

const [wasmPath = 'sim.wasm', outPath = 'font.png', text = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 .,:-!?+/%()\'"=° АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ абвг'] = process.argv.slice(2);
let mem;
const imports = { env: { sim_log: () => {} }, wasi_snapshot_preview1: { fd_write: () => 0, fd_close: () => 0, fd_seek: () => 0 } };
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), imports);
const ex = instance.exports; mem = ex.memory; ex._initialize?.();
ex.sim_init(1, 1700000000);
const W = ex.sim_width(), H = ex.sim_height();
const bytes = new TextEncoder().encode(text);
new Uint8Array(mem.buffer, ex.sim_text_buffer(), ex.sim_text_buffer_size()).fill(0).set(bytes);
ex.sim_apply_text();
let FX_TEXT = -1;
for (let i = 0; i < ex.sim_effect_count(); i++) {
  const p = ex.sim_effect_name(i); const b = new Uint8Array(mem.buffer); let e = p; while (b[e]) e++;
  if (new TextDecoder().decode(b.subarray(p, e)) === 'Бегущая строка') FX_TEXT = i;
}
ex.sim_select(FX_TEXT);
// Каждые 30 кадров (5 знаков по 6 точек) — новый снимок.
const chars = [...text].length, shots = Math.ceil((chars * 6 + W) / 30);
const CELL = 6, GAP = 4, tileW = W * CELL, tileH = H * CELL, perRow = 3;
const rows = Math.ceil(shots / perRow);
const sw = perRow * (tileW + GAP) + GAP, sh = rows * (tileH + GAP) + GAP;
const sheet = Buffer.alloc(sw * sh * 3, 40);
let f = 0;
for (let s = 0; s < shots; s++) {
  let ptr; for (let k = 0; k < 30; k++) { f++; ptr = ex.sim_frame(f * 40, 1700000000); }
  const px = new Uint8Array(mem.buffer, ptr, W * H * 3);
  const ox = GAP + (s % perRow) * (tileW + GAP), oy = GAP + Math.floor(s / perRow) * (tileH + GAP);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) for (let dy = 0; dy < CELL - 1; dy++) for (let dx = 0; dx < CELL - 1; dx++) {
    const d = ((oy + y * CELL + dy) * sw + ox + x * CELL + dx) * 3, q = (y * W + x) * 3;
    sheet[d] = px[q] ? 255 : 20; sheet[d + 1] = px[q + 1] ? 255 : 20; sheet[d + 2] = px[q + 2] ? 255 : 30;
  }
}
const raw = Buffer.alloc((sw * 3 + 1) * sh);
for (let y = 0; y < sh; y++) sheet.copy(raw, y * (sw * 3 + 1) + 1, y * sw * 3, (y + 1) * sw * 3);
const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(zlib.crc32(td) >>> 0); return Buffer.concat([l, td, c]); };
const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(sw, 0); ihdr.writeUInt32BE(sh, 4); ihdr[8] = 8; ihdr[9] = 2;
fs.writeFileSync(outPath, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
console.log('снимков:', shots);
