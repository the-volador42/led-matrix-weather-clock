// Сцены во все четыре сезона на одной картинке: строки — эффекты, столбцы —
// весна, лето, осень, зима.
//   node tools/seasons_sheet.mjs [sim.wasm] [out.png] [номера эффектов] [кадр]
import fs from 'node:fs';
import zlib from 'node:zlib';

const [wasmPath = 'sim.wasm', outPath = 'seasons.png', idsArg = '18,19,7,15,17,21,9,1,22,25', frameArg = '150'] = process.argv.slice(2);
const ids = idsArg.split(',').map(n => Number(n) - 1);
const FRAME = Number(frameArg);
const bytes = fs.readFileSync(wasmPath);

async function fresh() {
  let mem;
  const imports = { env: { sim_log: () => {} }, wasi_snapshot_preview1: { fd_write: () => 0, fd_close: () => 0, fd_seek: () => 0 } };
  const { instance } = await WebAssembly.instantiate(bytes, imports);
  mem = instance.exports.memory; instance.exports._initialize?.();
  return { ex: instance.exports, mem };
}

const W = 32, H = 8, CELL = 6, GAP = 4, tileW = W * CELL, tileH = H * CELL;
const sw = 4 * (tileW + GAP) + GAP, sh = ids.length * (tileH + GAP) + GAP;
const sheet = Buffer.alloc(sw * sh * 3, 40);
const gamma = v => Math.round(255 * Math.pow(v / 255, 1 / 2.2));
const START = Math.floor(Date.UTC(2026, 9, 26, 22, 0, 0) / 1000);

for (let row = 0; row < ids.length; row++) {
  for (let season = 0; season < 4; season++) {
    const { ex, mem } = await fresh();
    ex.sim_init(5, START);
    ex.sim_set_wind(8);
    ex.sim_set_season(season);
    ex.sim_select(ids[row]);
    let ptr;
    for (let f = 1; f <= FRAME; f++) ptr = ex.sim_frame(f * 40, START + Math.floor(f * 40 / 1000));
    const px = new Uint8Array(mem.buffer, ptr, W * H * 3);
    const ox = GAP + season * (tileW + GAP), oy = GAP + row * (tileH + GAP);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) for (let dy = 0; dy < CELL - 1; dy++) for (let dx = 0; dx < CELL - 1; dx++) {
      const d = ((oy + y * CELL + dy) * sw + ox + x * CELL + dx) * 3, q = (y * W + x) * 3;
      sheet[d] = gamma(px[q]); sheet[d + 1] = gamma(px[q + 1]); sheet[d + 2] = gamma(px[q + 2]);
    }
  }
}
const raw = Buffer.alloc((sw * 3 + 1) * sh);
for (let y = 0; y < sh; y++) sheet.copy(raw, y * (sw * 3 + 1) + 1, y * sw * 3, (y + 1) * sw * 3);
const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(zlib.crc32(td) >>> 0); return Buffer.concat([l, td, c]); };
const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(sw, 0); ihdr.writeUInt32BE(sh, 4); ihdr[8] = 8; ihdr[9] = 2;
fs.writeFileSync(outPath, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
console.log('готово:', outPath);
