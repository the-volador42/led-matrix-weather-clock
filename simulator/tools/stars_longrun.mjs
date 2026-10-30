// Долгий прогон «Звёздной ночи»: сколько звёзд видно на небе со временем.
//   node tools/stars_longrun.mjs [sim.wasm] [часов] [шаг отчёта, мин]
import fs from 'node:fs';

const wasmPath = process.argv[2] || 'sim.wasm';
const hours = Number(process.argv[3] || 2);
const STEP_MIN = Number(process.argv[4] || 15);   // как часто печатать, минут
const FRAME_MS = 40;
let mem;
const imports = {
  env: { sim_log: () => {} },
  wasi_snapshot_preview1: {
    fd_write: (fd, iov, n, out) => { const v = new DataView(mem.buffer); let t = 0; for (let i = 0; i < n; i++) t += v.getUint32(iov + i * 8 + 4, true); v.setUint32(out, t, true); return 0; },
    fd_close: () => 0, fd_seek: () => 0,
  },
};
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), imports);
const ex = instance.exports; mem = ex.memory; ex._initialize?.();

const START = Math.floor(Date.UTC(2026, 9, 26, 23, 0, 0) / 1000);
ex.sim_init(7, START);
ex.sim_select(21);                         // «Звёздная ночь»
const W = ex.sim_width(), H = ex.sim_height();

// Звезда = голубовато-белый пиксель в верхних 5 строках (небо тёмно-синее, луна жёлтая).
function countStars(px) {
  let n = 0;
  for (let y = 0; y < 5; y++) for (let x = 0; x < W; x++) {
    const s = (y * W + x) * 3, r = px[s], g = px[s + 1], b = px[s + 2];
    if (b > 120 && g > 90 && r < g && r > 40) n++;
  }
  return n;
}

const total = hours * 3600 * 1000 / FRAME_MS;
const report = [];
for (let f = 1; f <= total; f++) {
  const ms = f * FRAME_MS;
  const ptr = ex.sim_frame(ms, START + Math.floor(ms / 1000));
  if (f % (STEP_MIN * 60 * 1000 / FRAME_MS) === 0) {
    report.push(`${(ms / 60000).toFixed(0).padStart(4)} мин: звёзд на небе ${countStars(new Uint8Array(mem.buffer, ptr, W * H * 3))}`);
  }
}
console.log(report.join('\n'));
