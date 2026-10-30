// Проверка «ничего не сломалось»: прогоняет каждый эффект N кадров
// с фиксированным seed и временем и сравнивает кадры с эталоном.
//
//   node tools/golden.mjs save    [sim.wasm] [golden.json]   — записать эталон
//   node tools/golden.mjs compare [sim.wasm] [golden.json]   — сравнить с эталоном
import fs from 'node:fs';
import crypto from 'node:crypto';

const mode = process.argv[2];
const wasmPath = process.argv[3] || 'sim.wasm';
const goldenPath = process.argv[4] || 'tools/golden.json';
const FRAMES = 1500;                 // 60 секунд каждого эффекта
const FRAME_MS = 40;
const SEED = 20261026;
const START_TIME = Math.floor(Date.UTC(2026, 9, 26, 21, 30, 0) / 1000);

const bytes = fs.readFileSync(wasmPath);
async function fresh() {
  let mem;
  const imports = {
    env: { sim_log: () => {} },
    wasi_snapshot_preview1: {
      fd_write: (fd, iov, n, out) => { const v = new DataView(mem.buffer); let t = 0; for (let i = 0; i < n; i++) t += v.getUint32(iov + i * 8 + 4, true); v.setUint32(out, t, true); return 0; },
      fd_close: () => 0, fd_seek: () => 0,
    },
  };
  const { instance } = await WebAssembly.instantiate(bytes, imports);
  mem = instance.exports.memory;
  instance.exports._initialize?.();
  const text = p => { const b = new Uint8Array(mem.buffer); let e = p; while (b[e]) e++; return new TextDecoder().decode(b.subarray(p, e)); };
  return { ex: instance.exports, mem, text };
}

const probe = await fresh();
const count = probe.ex.sim_effect_count();
const result = {};
for (let id = 0; id < count; id++) {
  const { ex, mem, text } = await fresh();       // каждый эффект — с чистого листа
  ex.sim_init(SEED, START_TIME);
  ex.sim_set_wind(8);
  ex.sim_select(id);
  const name = text(ex.sim_effect_name(id));
  const frames = [];
  for (let f = 1; f <= FRAMES; f++) {
    const ms = f * FRAME_MS;
    const ptr = ex.sim_frame(ms, START_TIME + Math.floor(ms / 1000));
    const px = new Uint8Array(mem.buffer, ptr, ex.sim_width() * ex.sim_height() * 3);
    frames.push(crypto.createHash('sha1').update(px).digest('hex').slice(0, 12));
  }
  result[id + 1] = { name, frames };
}

if (mode === 'save') {
  fs.writeFileSync(goldenPath, JSON.stringify(result));
  console.log(`Эталон записан: ${count} эффектов × ${FRAMES} кадров -> ${goldenPath}`);
} else if (mode === 'compare') {
  const golden = JSON.parse(fs.readFileSync(goldenPath, 'utf8'));
  let bad = 0;
  for (const [id, g] of Object.entries(golden)) {
    const r = result[id];
    if (!r) { console.log(`#${id} ${g.name}: ЭФФЕКТ ПРОПАЛ`); bad++; continue; }
    if (r.name !== g.name) console.log(`#${id}: название изменилось «${g.name}» -> «${r.name}»`);
    const diff = g.frames.findIndex((h, i) => h !== r.frames[i]);
    if (diff >= 0) { console.log(`#${id} ${g.name}: отличается начиная с кадра ${diff + 1}`); bad++; }
    else console.log(`#${id} ${g.name}: совпадает`);
  }
  if (count !== Object.keys(golden).length) console.log(`Количество эффектов: было ${Object.keys(golden).length}, стало ${count}`);
  console.log(bad ? `\nОТЛИЧИЙ: ${bad}` : '\nВсе эффекты совпадают с эталоном кадр в кадр.');
  process.exit(bad ? 1 : 0);
} else {
  console.log('Использование: node tools/golden.mjs save|compare [sim.wasm] [golden.json]');
}
