// Снимает кадры всех эффектов в одну PNG-картинку («контактный лист»).
// Нужен для проверки эффектов без браузера и без матрицы.
//
//   node tools/snapshot.mjs [sim.wasm] [out.png] [кадры через запятую] [номера эффектов]
//   пример: node tools/snapshot.mjs sim.wasm sheet.png 1,50,200,600 15
import fs from 'node:fs';
import zlib from 'node:zlib';

const wasmPath = process.argv[2] || 'sim.wasm';
const outPath = process.argv[3] || 'sheet.png';
const frameList = (process.argv[4] || '1,25,100,250,750').split(',').map(Number);
const only = process.argv[5] ? process.argv[5].split(',').map(n => Number(n) - 1) : null;
const FRAME_MS = 40;
const START_TIME = Math.floor(Date.UTC(2026, 9, 26, 21, 30, 0) / 1000); // 21:30, фиксировано

async function loadSim() {
  let mem;
  const text = p => { const b = new Uint8Array(mem.buffer); let e = p; while (b[e]) e++; return new TextDecoder().decode(b.subarray(p, e)); };
  const imports = {
    env: { sim_log: p => {} },
    wasi_snapshot_preview1: {
      fd_write: (fd, iov, n, out) => {
        const v = new DataView(mem.buffer); let t = 0;
        for (let i = 0; i < n; i++) t += v.getUint32(iov + i * 8 + 4, true);
        v.setUint32(out, t, true); return 0;
      },
      fd_close: () => 0, fd_seek: () => 0,
    },
  };
  const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), imports);
  mem = instance.exports.memory;
  instance.exports._initialize?.();
  return { ex: instance.exports, mem, text };
}

function png(w, h, rgb) {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) { raw[y * (w * 3 + 1)] = 0; rgb.copy(raw, y * (w * 3 + 1) + 1, y * w * 3, (y + 1) * w * 3); }
  const chunk = (type, data) => {
    const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type), data]);
    const crc = Buffer.alloc(4); crc.writeUInt32BE(zlib.crc32(td) >>> 0);
    return Buffer.concat([len, td, crc]);
  };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

const sim = await loadSim();
const { ex } = sim;
ex.sim_init(42, START_TIME);
const W = ex.sim_width(), H = ex.sim_height(), N = ex.sim_effect_count();
const ids = only || [...Array(N).keys()];

const CELL = 6, GAP = 4;                       // пиксель матрицы = 6x6, отступ между кадрами
const tileW = W * CELL, tileH = H * CELL;
const sheetW = frameList.length * (tileW + GAP) + GAP;
const sheetH = ids.length * (tileH + GAP) + GAP;
const sheet = Buffer.alloc(sheetW * sheetH * 3, 40);
const gamma = v => Math.round(255 * Math.pow(v / 255, 1 / 2.2)); // как светодиод выглядит на глаз

const names = [];
let totalMs = 0; // millis() как на часах: общий для всех эффектов, не сбрасывается
ids.forEach((id, row) => {
  ex.sim_select(id);
  names.push(`${String(row + 1).padStart(2)}. #${id + 1} ${sim.text(ex.sim_effect_name(id))}`);
  let f = 0;
  frameList.forEach((target, col) => {
    let ptr = 0;
    while (f < target) { f++; totalMs += FRAME_MS; ptr = ex.sim_frame(totalMs, START_TIME + Math.floor(totalMs / 1000)); }
    const px = new Uint8Array(sim.mem.buffer, ptr, W * H * 3);
    const ox = GAP + col * (tileW + GAP), oy = GAP + row * (tileH + GAP);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      const s = (y * W + x) * 3;
      for (let dy = 0; dy < CELL - 1; dy++) for (let dx = 0; dx < CELL - 1; dx++) {
        const d = ((oy + y * CELL + dy) * sheetW + ox + x * CELL + dx) * 3;
        sheet[d] = gamma(px[s]); sheet[d + 1] = gamma(px[s + 1]); sheet[d + 2] = gamma(px[s + 2]);
      }
    }
  });
});
fs.writeFileSync(outPath, png(sheetW, sheetH, sheet));
console.log(`кадры: ${frameList.join(', ')} (1 кадр = ${FRAME_MS} мс)`);
console.log(names.join('\n'));
