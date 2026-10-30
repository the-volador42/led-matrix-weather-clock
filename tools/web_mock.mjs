// Имитация часов для проверки веб-интерфейса на компьютере.
//
// Страница берётся прямо из led_matrix_weather_clock/web_page.h, а кадры
// матрицы считает НАСТОЯЩИЙ код эффектов (simulator/sim.wasm) — фон
// страницы выглядит так же, как с живыми часами.
//
//   node tools/web_mock.mjs            — обычный режим, http://localhost:8080
//   node tools/web_mock.mjs --portal   — режим настройки Wi-Fi, http://localhost:8081
// Перед первым запуском собери симулятор: simulator/build.bat
import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const PORTAL = process.argv.includes('--portal');
const PORT = PORTAL ? 8081 : 8080;
const FRAME_MS = 40;

// ---------- настоящие эффекты ----------
let mem;
const imports = { env: { sim_log: () => {} }, wasi_snapshot_preview1: { fd_write: () => 0, fd_close: () => 0, fd_seek: () => 0 } };
const wasmPath = path.join(ROOT, 'simulator', 'sim.wasm');
if (!fs.existsSync(wasmPath)) { console.error('Нет simulator/sim.wasm — сначала запусти simulator/build.bat'); process.exit(1); }
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), imports);
const ex = instance.exports; mem = ex.memory; ex._initialize?.();
const cstr = p => { const b = new Uint8Array(mem.buffer); let e = p; while (b[e]) e++; return new TextDecoder().decode(b.subarray(p, e)); };
const START = Math.floor(Date.now() / 1000);
ex.sim_init((Math.random() * 1e9) >>> 0, START);
ex.sim_set_wind(9);
const EFFECTS = [...Array(ex.sim_effect_count()).keys()].map(i => cstr(ex.sim_effect_name(i)));
const fx = name => EFFECTS.indexOf(name);

const MODES = ['Погода', 'Демо', 'Часы', 'Огонь', 'Будильник', 'Бегущая строка',
  'Рассвет', 'Закат', 'Дождь', 'Снег', 'Ночь', 'Луна', 'Радуга', 'Выбранный эффект'];
// Какой эффект показывает каждый режим (как в modes.cpp).
const MODE_FX = { 0: fx('Снег лёгкий'), 1: fx('Смена дневных сцен'), 2: fx('Часы'), 3: fx('Огонь'), 4: fx('Рассвет'),
  5: fx('Бегущая строка'), 6: fx('Рассвет'), 7: fx('Закат'), 8: fx('Дождь слабый'), 9: fx('Снег лёгкий'),
  10: fx('Смена ночных сцен'), 11: fx('Звёздная ночь'), 12: fx('Радуга') };

const STATE = {
  fw: '3.1', power: true, brightness: 60, mode: 0, effect: 0, text: 'ПРИВЕТ', alarmDawn: 20, season: 0,
  alarm: { h: 7, m: 0, on: false, ringing: false },
  nightCfg: { on: true, start: 23 * 60, end: 7 * 60, bright: 5 },
  weather: { temp: -3, id: 601, effect: 'Снег лёгкий', ago: 240, error: '', city: 'Moscow,RU', tz: 3, hasKey: true },
  wifi: { connected: !PORTAL, ssid: PORTAL ? '' : 'My Home WiFi', ip: PORTAL ? '' : '192.168.1.57', rssi: -61, portal: PORTAL, ap: 'LED-Clock-1A2B' },
};

const seasonByMonth = m => (m === 12 || m <= 2) ? 3 : m <= 5 ? 0 : m <= 8 ? 1 : 2;
const seasonNow = () => STATE.season ? STATE.season - 1 : seasonByMonth(new Date().getMonth() + 1);

let shownFx = -1, ms = 0, frame = new Uint8Array(32 * 8 * 3);
function currentFx() { return STATE.mode === 13 ? STATE.effect : MODE_FX[STATE.mode]; }
setInterval(() => {
  const id = currentFx();
  if (id !== shownFx) { ex.sim_select(id); shownFx = id; }
  ex.sim_set_season(seasonNow());
  ms += FRAME_MS;
  frame = new Uint8Array(mem.buffer, ex.sim_frame(ms, START + Math.floor(ms / 1000)), 768).slice();
}, FRAME_MS);

function setText(t) {
  const bytes = new TextEncoder().encode(t);
  new Uint8Array(mem.buffer, ex.sim_text_buffer(), ex.sim_text_buffer_size()).fill(0).set(bytes.subarray(0, 500));
  ex.sim_apply_text();
}
setText(STATE.text);

// ---------- API ----------
const hhmm = s => { const [h, m] = s.split(':').map(Number); return h * 60 + m; };
function state() {
  const lt = new Date(), m = lt.getHours() * 60 + lt.getMinutes(), n = STATE.nightCfg;
  const inside = n.start < n.end ? (m >= n.start && m < n.end) : (m >= n.start || m < n.end);
  return { ...STATE, uptime: Math.floor(ms / 1000) + 3 * 3600, heap: 26100, seasonNow: seasonNow(),
    time: `${String(lt.getHours()).padStart(2, '0')}:${String(lt.getMinutes()).padStart(2, '0')}`, nightActive: n.on && inside };
}
function apply(p) {
  const g = k => p.get(k);
  if (p.has('power')) STATE.power = g('power') !== '0';
  if (p.has('brightness')) STATE.brightness = Math.max(1, Math.min(255, +g('brightness')));
  if (p.has('text')) { STATE.text = g('text'); setText(STATE.text); shownFx = -1; }
  if (p.has('mode')) { STATE.mode = +g('mode'); STATE.power = true; }
  if (p.has('effect')) { STATE.effect = +g('effect'); STATE.mode = 13; STATE.power = true; shownFx = -1; }
  if (p.has('alarmTime') && g('alarmTime')) { const t = hhmm(g('alarmTime')); STATE.alarm.h = Math.floor(t / 60); STATE.alarm.m = t % 60; }
  if (p.has('alarmOn')) STATE.alarm.on = g('alarmOn') !== '0';
  if (p.has('nightOn')) STATE.nightCfg.on = g('nightOn') !== '0';
  if (p.has('nightStart')) STATE.nightCfg.start = hhmm(g('nightStart'));
  if (p.has('nightEnd')) STATE.nightCfg.end = hhmm(g('nightEnd'));
  if (p.has('nightBright')) STATE.nightCfg.bright = +g('nightBright');
  if (p.has('season')) STATE.season = Math.max(0, Math.min(4, +g('season')));
  if (p.has('city') && g('city').trim()) STATE.weather.city = g('city').trim();
  if (p.has('tz')) STATE.weather.tz = +g('tz');
  if (p.has('owmKey')) STATE.weather.hasKey = true;
  if (p.has('weatherRefresh')) STATE.weather.ago = 0;
}
let scanCalls = 0;
const page = () => {
  const src = fs.readFileSync(path.join(ROOT, 'led_matrix_weather_clock', 'web_page.h'), 'utf8');
  const a = src.indexOf('R"rawliteral(') + 'R"rawliteral('.length;
  return src.slice(a, src.indexOf(')rawliteral"', a));
};

http.createServer((req, res) => {
  const url = new URL(req.url, 'http://x');
  const json = o => { res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' }); res.end(JSON.stringify(o)); };
  if (req.method === 'GET') {
    if (url.pathname === '/') { res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' }); return res.end(page()); }
    if (url.pathname === '/api/state') return json(state());
    if (url.pathname === '/api/meta') return json({ modes: MODES.map((name, id) => ({ id, name })).filter(m => m.id !== 4 && m.id !== 13), effects: EFFECTS });
    if (url.pathname === '/api/frame') {
      res.writeHead(200, { 'Content-Type': 'application/octet-stream', 'Cache-Control': 'no-store' });
      return res.end(STATE.power ? Buffer.from(frame) : Buffer.alloc(768));
    }
    if (url.pathname === '/api/scan') {
      if (++scanCalls % 3) return json({ scanning: true });
      return json({ scanning: false, networks: [
        { ssid: 'My Home WiFi', rssi: -58, secure: true }, { ssid: 'Neighbour_5G', rssi: -74, secure: true }, { ssid: 'Cafe free', rssi: -82, secure: false }] });
    }
  } else if (req.method === 'POST') {
    let body = '';
    req.on('data', c => body += c);
    req.on('end', () => {
      const p = new URLSearchParams(body);
      if (url.pathname === '/api/set') { apply(p); return json(state()); }
      if (url.pathname === '/api/wifi') { Object.assign(STATE.wifi, { ssid: p.get('ssid'), connected: true, ip: '192.168.1.57' }); return json({ ok: true }); }
      if (url.pathname === '/api/restart') return json({ ok: true });
      res.writeHead(404); res.end();
    });
    return;
  }
  res.writeHead(404); res.end('Not found');
}).listen(PORT, '127.0.0.1', () => console.log(`Имитация часов: http://localhost:${PORT}  (${PORTAL ? 'режим настройки Wi-Fi' : 'обычный режим'})`));
