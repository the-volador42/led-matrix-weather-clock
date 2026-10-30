// =====================================================================
//  СТРАНИЦА ВЕБ-ИНТЕРФЕЙСА (хранится во флеш-памяти, не занимает ОЗУ)
//  Фон — живое размытое изображение с матрицы (/api/frame),
//  поверх — карточки «из стекла».
//  Проверить без часов: node tools/web_mock.mjs и открыть http://localhost:8080
// =====================================================================
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b0d14">
<title>LED-часы</title>
<style>
:root{
 --text:#f4f6fb;--muted:rgba(235,240,255,.66);--faint:rgba(235,240,255,.42);
 --glass:linear-gradient(140deg,rgba(255,255,255,.16),rgba(255,255,255,.05) 55%,rgba(255,255,255,.08));
 --edge:rgba(255,255,255,.22);--edge2:rgba(255,255,255,.08);
 --field:rgba(255,255,255,.08);--field-edge:rgba(255,255,255,.16);
 --accent1:#6ee7ff;--accent2:#a78bfa;--accent:linear-gradient(135deg,var(--accent1),var(--accent2));
 --ink:#081018;--ok:#5ef0a0;--warn:#ffc46b;--bad:#ff8a8a;--r:22px;
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html{background:#0b0d14}
html,body{margin:0;min-height:100%;color:var(--text)}   /* у body фона нет: иначе он закроет живой фон */
body{font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;padding:max(14px,env(safe-area-inset-top)) 16px 48px;overflow-x:hidden}
/* ---------- живой фон ---------- */
#bg{position:fixed;inset:-12%;width:124%;height:124%;z-index:-3;filter:blur(42px) saturate(1.55) brightness(.9);image-rendering:auto;transform:translateZ(0)}
.shade{position:fixed;inset:0;z-index:-2;pointer-events:none;
 background:radial-gradient(120% 70% at 50% -10%,rgba(255,255,255,.10),transparent 60%),linear-gradient(180deg,rgba(6,8,16,.25) 0%,rgba(6,8,16,.55) 45%,rgba(6,8,16,.80) 100%)}
.grain{position:fixed;inset:0;z-index:-1;pointer-events:none;opacity:.05;background-image:radial-gradient(rgba(255,255,255,.9) .5px,transparent .6px);background-size:3px 3px}
.wrap{max-width:760px;margin:0 auto}
/* ---------- стекло ---------- */
.glass{position:relative;background:var(--glass);border:1px solid var(--edge2);border-top-color:var(--edge);border-radius:var(--r);
 -webkit-backdrop-filter:blur(22px) saturate(1.4);backdrop-filter:blur(22px) saturate(1.4);
 box-shadow:0 10px 40px rgba(0,0,0,.35),inset 0 1px 0 rgba(255,255,255,.12)}
.card{padding:16px;margin-bottom:14px}
.card h2{font-size:12px;font-weight:650;text-transform:uppercase;letter-spacing:.09em;color:var(--muted);margin:0 0 12px;display:flex;align-items:center;gap:8px}
.card h2::before{content:"";width:6px;height:6px;border-radius:50%;background:var(--accent);box-shadow:0 0 10px var(--accent1)}
/* ---------- шапка ---------- */
.hero{padding:18px 18px 16px;margin-bottom:14px}
.top{display:flex;justify-content:space-between;align-items:flex-end;gap:12px}
.time{font-size:54px;line-height:1;font-weight:250;letter-spacing:-.02em;font-variant-numeric:tabular-nums}
.date{color:var(--muted);margin-top:6px;font-size:14px}
.temp{text-align:right}.temp b{font-size:34px;font-weight:300;display:block;line-height:1}.temp span{color:var(--muted);font-size:13px}
.preview{margin:16px 0 12px;border-radius:14px;background:rgba(0,0,0,.55);padding:8px;box-shadow:inset 0 0 0 1px rgba(255,255,255,.06),inset 0 6px 20px rgba(0,0,0,.5)}
#pv{display:block;width:100%;height:auto}
.now{display:flex;justify-content:space-between;align-items:center;gap:10px}
.now b{font-weight:600}.now small{color:var(--muted);display:block;font-size:12px}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:12px}
.chip{font-size:12px;color:var(--muted);background:rgba(255,255,255,.07);border:1px solid rgba(255,255,255,.10);border-radius:99px;padding:4px 10px}
.chip.ok{color:var(--ok)}.chip.warn{color:var(--warn)}
.banner{padding:14px 16px;margin-bottom:14px;border-color:rgba(110,231,255,.35)}
/* ---------- элементы ---------- */
.row{display:flex;align-items:center;gap:10px;margin:10px 0}.grow{flex:1;min-width:0}
.hint{font-size:13px;color:var(--muted);margin:8px 0 0}.err{color:var(--bad)}
button,input,select{font:inherit;color:var(--text);background:var(--field);border:1px solid var(--field-edge);border-radius:14px;padding:10px 14px;min-height:44px;transition:background .15s,border-color .15s,transform .1s}
button{cursor:pointer}button:hover{background:rgba(255,255,255,.13)}button:active{transform:scale(.98)}
button:focus-visible,input:focus-visible,select:focus-visible{outline:2px solid var(--accent1);outline-offset:2px}
input::placeholder{color:var(--faint)}select option{background:#1a1d27;color:var(--text)}
input[type=time]{color-scheme:dark}
.primary{background:var(--accent);color:var(--ink);border:0;font-weight:650;box-shadow:0 6px 20px rgba(110,231,255,.25)}
.primary:hover{background:var(--accent);filter:brightness(1.07)}
input[type=text],input[type=password]{width:100%}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:30px;background:none;border:0;padding:0;min-height:30px}
input[type=range]::-webkit-slider-runnable-track{height:8px;border-radius:99px;background:linear-gradient(90deg,rgba(255,255,255,.25),rgba(255,255,255,.75) var(--p,50%),rgba(255,255,255,.12) var(--p,50%))}
input[type=range]::-moz-range-track{height:8px;border-radius:99px;background:rgba(255,255,255,.14)}
input[type=range]::-moz-range-progress{height:8px;border-radius:99px;background:rgba(255,255,255,.7)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:24px;height:24px;margin-top:-8px;border-radius:50%;background:#fff;box-shadow:0 2px 10px rgba(0,0,0,.4)}
input[type=range]::-moz-range-thumb{width:24px;height:24px;border:0;border-radius:50%;background:#fff;box-shadow:0 2px 10px rgba(0,0,0,.4)}
.pills{display:flex;flex-wrap:wrap;gap:8px}
.pills button{border-radius:99px;padding:8px 14px;min-height:38px;font-size:14px}
.tiles{display:grid;grid-template-columns:repeat(auto-fill,minmax(148px,1fr));gap:8px}
.tiles button{text-align:left;font-size:14px;padding:12px;border-radius:16px;min-height:52px}
.pills button[aria-pressed=true],.tiles button[aria-pressed=true],.seg button[aria-pressed=true]{background:var(--accent);color:var(--ink);border-color:transparent;font-weight:650;box-shadow:0 6px 18px rgba(110,231,255,.25)}
.seg{display:grid;grid-template-columns:repeat(5,1fr);gap:4px;padding:4px;border-radius:16px;background:rgba(0,0,0,.25);border:1px solid rgba(255,255,255,.08)}
.seg button{border:0;background:none;border-radius:12px;padding:8px 4px;min-height:40px;font-size:13px}
.sw{width:54px;min-width:54px;height:32px;min-height:32px;border-radius:99px;padding:0;position:relative;background:rgba(255,255,255,.18);border:0}
.sw::after{content:"";position:absolute;top:3px;left:3px;width:26px;height:26px;border-radius:50%;background:#fff;box-shadow:0 2px 8px rgba(0,0,0,.35);transition:left .18s}
.sw[aria-pressed=true]{background:linear-gradient(135deg,#34d399,#6ee7ff)}.sw[aria-pressed=true]::after{left:25px}
.val{color:var(--muted);font-variant-numeric:tabular-nums;font-size:13px}
.nets button{display:flex;justify-content:space-between;width:100%;margin:6px 0;text-align:left}
details summary{cursor:pointer;color:var(--muted);padding:4px 0}a{color:var(--accent1)}
.two{display:grid;grid-template-columns:1fr 1fr;gap:14px}@media(max-width:620px){.two{grid-template-columns:1fr}}
.two .card{margin-bottom:0}.two{margin-bottom:14px}
#toast{position:fixed;left:50%;bottom:max(18px,env(safe-area-inset-bottom));transform:translate(-50%,120px);padding:10px 18px;border-radius:99px;transition:transform .3s;z-index:5;max-width:90vw;font-size:14px}
#toast.show{transform:translate(-50%,0)}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style></head><body>
<canvas id="bg" width="32" height="8" aria-hidden="true"></canvas><div class="shade"></div><div class="grain"></div>
<div class="wrap">

<section class="glass hero">
 <div class="top"><div><div class="time" id="clock">--:--</div><div class="date" id="date"></div></div>
  <div class="temp"><b id="temp">—</b><span id="wname"></span></div></div>
 <div class="preview"><canvas id="pv" width="640" height="160" aria-label="Что сейчас на матрице"></canvas></div>
 <div class="now"><div><b id="nowName">—</b><small id="nowSub"></small></div><button class="sw" id="power" aria-label="Экран вкл/выкл"></button></div>
 <div class="row" style="margin-bottom:0"><span class="val">☀︎</span><input type="range" id="bright" min="1" max="255" class="grow" aria-label="Яркость"><span class="val" id="brightVal"></span></div>
 <div class="chips" id="chips"></div>
</section>

<div class="glass banner" id="portal" hidden>Часы в режиме настройки. Выбери домашнюю сеть Wi-Fi ниже и нажми «Подключить» — после подключения часы покажут свой новый адрес бегущей строкой.</div>

<section class="glass card"><h2>Режим</h2><div class="pills" id="modes"></div></section>

<section class="glass card"><h2>Время года</h2>
 <div class="seg" id="season">
  <button data-s="0">Авто</button><button data-s="1">Весна</button><button data-s="2">Лето</button><button data-s="3">Осень</button><button data-s="4">Зима</button>
 </div>
 <p class="hint" id="seasonHint"></p>
</section>

<section class="glass card"><h2>Эффекты</h2><div class="tiles" id="effects"></div></section>

<section class="glass card"><h2>Бегущая строка</h2>
 <div class="row"><input type="text" id="text" maxlength="90" class="grow" placeholder="Что написать?"><button class="primary" id="textGo">Показать</button></div>
 <p class="hint">Русские и латинские буквы, цифры и знаки . , : - ! ? + / % ( ) ' " = °</p>
</section>

<div class="two">
<section class="glass card"><h2>Будильник</h2>
 <div class="row"><input type="time" id="alarmTime" class="grow"><button class="sw" id="alarmOn" aria-label="Будильник"></button></div>
 <p class="hint" id="alarmHint"></p>
</section>
<section class="glass card"><h2>Ночной режим</h2>
 <div class="row"><span class="grow">Приглушать ночью</span><button class="sw" id="nightOn" aria-label="Ночной режим"></button></div>
 <div class="row"><input type="time" id="nightStart" class="grow" aria-label="Начало"><span class="val">—</span><input type="time" id="nightEnd" class="grow" aria-label="Конец"></div>
 <div class="row"><input type="range" id="nightBright" min="1" max="60" class="grow" aria-label="Яркость ночью"><span class="val" id="nightBrightVal"></span></div>
</section>
</div>

<section class="glass card"><h2>Погода</h2>
 <p id="weatherLine" style="margin:0 0 6px"></p>
 <div class="row"><input type="text" id="city" placeholder="Город, например Moscow,RU" class="grow"></div>
 <div class="row"><span class="grow">Часовой пояс</span><select id="tz"></select></div>
 <div class="row"><input type="password" id="owmKey" class="grow" autocomplete="off"></div>
 <div class="row"><button class="primary" id="weatherSave">Сохранить</button><button id="weatherNow">Обновить сейчас</button></div>
 <p class="hint">Ключ бесплатно выдаёт openweathermap.org после регистрации.</p>
</section>

<section class="glass card" id="wifiCard"><h2>Wi-Fi</h2>
 <p id="wifiLine" style="margin:0 0 8px"></p>
 <button id="scan">Найти сети</button>
 <div class="nets" id="nets"></div>
 <div id="joinBox" hidden>
  <div class="row"><input type="text" id="ssid" placeholder="Имя сети" class="grow"></div>
  <div class="row"><input type="password" id="pass" placeholder="Пароль" class="grow"></div>
  <button class="primary" id="join">Подключить</button>
 </div>
 <p class="hint">Часы всегда доступны по адресу <a href="http://ledclock.local">ledclock.local</a>.</p>
</section>

<section class="glass card"><h2>Система</h2>
 <p id="sysLine" class="hint" style="margin:0 0 8px"></p>
 <details><summary>Обновление прошивки</summary>
  <p class="hint">Из Arduino IDE: Инструменты → Порт → «ledclock at …» → Загрузить.<br>
  Или файлом: Скетч → Экспорт бинарного файла, затем <a href="/update">загрузить .bin</a> (логин admin, пароль — как для прошивки по Wi-Fi).</p>
  <div class="row"><input type="password" id="otaPass" placeholder="Новый пароль для прошивки" class="grow"><button id="otaSave">Сохранить</button></div>
 </details>
 <div class="row"><button id="restart">Перезагрузить часы</button></div>
</section>
</div>
<div class="glass" id="toast" role="status"></div>

<script>
const $=id=>document.getElementById(id);
let S=null,meta=null,movedWifi=false,gen=0;   // gen: опрос не затирает более свежий ответ
const SEASONS=['весна','лето','осень','зима'];
const pad=n=>String(n).padStart(2,'0'),hm=m=>pad(Math.floor(m/60))+':'+pad(m%60);
const busy=el=>document.activeElement===el;
function setVal(el,v){if(!busy(el))el.value=v;fillRange(el)}
function fillRange(el){if(el.type==='range')el.style.setProperty('--p',((el.value-el.min)/(el.max-el.min)*100)+'%')}
function sw(el,on){el.setAttribute('aria-pressed',on?'true':'false')}
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
async function api(path,params){const o=params?{method:'POST',body:new URLSearchParams(params)}:{};const r=await fetch(path,o);if(!r.ok)throw new Error(r.status);return r.json()}
async function set(p){gen++;try{S=await api('/api/set',p);render()}catch(e){toast('Не удалось: '+e.message)}}
let tt;function toast(t){const el=$('toast');el.textContent=t;el.classList.add('show');clearTimeout(tt);tt=setTimeout(()=>el.classList.remove('show'),3200)}
let tmr={};function later(k,fn,ms){clearTimeout(tmr[k]);tmr[k]=setTimeout(fn,ms||200)}

/* ---------- живой кадр с матрицы ---------- */
const N=768,FR=330;let fA=new Uint8Array(N),fB=new Uint8Array(N),fT=0,fCur=new Uint8Array(N);
const G=new Uint8Array(256).map((_,v)=>Math.round(255*Math.pow(v/255,1/2.2)));   // как светодиод видит глаз
function mix(){const t=Math.min(1,(performance.now()-fT)/FR);for(let i=0;i<N;i++)fCur[i]=fA[i]+(fB[i]-fA[i])*t;return fCur}
async function pull(){
 if(!document.hidden){try{const r=await fetch('/api/frame',{cache:'no-store'});const b=new Uint8Array(await r.arrayBuffer());if(b.length===N){fA=mix().slice();fB=b;fT=performance.now()}}catch(e){}}
 setTimeout(pull,document.hidden?1500:FR)}
const bg=$('bg'),bx=bg.getContext('2d'),img=bx.createImageData(32,8);
const pv=$('pv'),px=pv.getContext('2d');
function sizePv(){const d=devicePixelRatio||1,w=pv.clientWidth;pv.width=Math.round(w*d);pv.height=Math.round(w/4*d)}
addEventListener('resize',sizePv);
let lastDraw=0;
function draw(now){
 requestAnimationFrame(draw);
 if(now-lastDraw<50)return;   // 20 кадров в секунду: сильное размытие на весь экран не грузит телефон
 lastDraw=now;
 const f=mix();
 for(let i=0,j=0;i<N;i+=3,j+=4){img.data[j]=G[f[i]|0];img.data[j+1]=G[f[i+1]|0];img.data[j+2]=G[f[i+2]|0];img.data[j+3]=255}
 bx.putImageData(img,0,0);
 const c=pv.width/32,r=c*.36;px.clearRect(0,0,pv.width,pv.height);
 for(let y=0;y<8;y++)for(let x=0;x<32;x++){const i=(y*32+x)*3,R=G[f[i]|0],Gg=G[f[i+1]|0],B=G[f[i+2]|0];
  px.fillStyle=(R+Gg+B>12)?`rgb(${R},${Gg},${B})`:'rgba(255,255,255,.05)';
  px.beginPath();px.arc((x+.5)*c,(y+.5)*c,r,0,6.2832);px.fill()}
}

/* ---------- состояние ---------- */
function buildMeta(){
 const m=$('modes');m.innerHTML='';
 meta.modes.forEach(x=>{const b=document.createElement('button');b.textContent=x.name;b.dataset.id=x.id;b.onclick=()=>set({mode:x.id});m.appendChild(b)});
 const e=$('effects');e.innerHTML='';
 meta.effects.forEach((n,i)=>{const b=document.createElement('button');b.textContent=n;b.dataset.id=i;b.onclick=()=>set({effect:i});e.appendChild(b)});
 const tz=$('tz');for(let h=-12;h<=14;h++){const o=document.createElement('option');o.value=h;o.textContent='UTC'+(h>=0?'+':'')+h+(h==3?' (Москва)':'');tz.appendChild(o)}
}
function render(){
 if(!S)return;const w=S.wifi,W=S.weather;
 $('clock').textContent=S.time||'--:--';
 const d=new Date();$('date').textContent=d.toLocaleDateString('ru-RU',{weekday:'long',day:'numeric',month:'long'});
 $('temp').textContent=W.temp===null?'—':(W.temp>0?'+':'')+W.temp+'°';$('wname').textContent=W.temp===null?'':W.effect;
 const modeName=S.mode===13?meta&&meta.effects[S.effect]:(meta&&(meta.modes.find(m=>m.id===S.mode)||{}).name);
 $('nowName').textContent=S.power?(modeName||'—'):'Экран выключен';
 $('nowSub').textContent=S.alarm.ringing?'идёт рассвет будильника':S.nightActive?'ночь: яркость ограничена до '+S.nightCfg.bright:'сейчас '+SEASONS[S.seasonNow];
 sw($('power'),S.power);setVal($('bright'),S.brightness);$('brightVal').textContent=S.brightness;
 const c=[];
 c.push(w.connected?`<span class="chip ok">Wi-Fi · ${esc(w.ssid)}</span>`:'<span class="chip warn">Wi-Fi не подключён</span>');
 if(w.ip)c.push(`<span class="chip">${w.ip}</span>`);
 if(S.alarm.on)c.push(`<span class="chip">⏰ ${pad(S.alarm.h)}:${pad(S.alarm.m)}</span>`);
 if(S.nightActive)c.push('<span class="chip">ночь</span>');
 $('chips').innerHTML=c.join('');
 $('portal').hidden=!w.portal;
 if(w.portal&&!movedWifi){movedWifi=true;$('portal').after($('wifiCard'))}
 document.querySelectorAll('#modes button').forEach(b=>b.setAttribute('aria-pressed',+b.dataset.id===S.mode?'true':'false'));
 document.querySelectorAll('#effects button').forEach(b=>b.setAttribute('aria-pressed',S.mode===13&&+b.dataset.id===S.effect?'true':'false'));
 document.querySelectorAll('#season button').forEach(b=>b.setAttribute('aria-pressed',+b.dataset.s===S.season?'true':'false'));
 $('seasonHint').textContent=S.season===0?'Сезон по дате: сейчас '+SEASONS[S.seasonNow]+'. Меняет траву и кроны деревьев.':'Сезон выбран вручную. Меняет траву и кроны деревьев.';
 setVal($('text'),S.text);
 setVal($('alarmTime'),pad(S.alarm.h)+':'+pad(S.alarm.m));sw($('alarmOn'),S.alarm.on);
 $('alarmHint').textContent=S.alarm.ringing?'Идёт рассвет. Касание кнопки на часах выключит его.':S.alarm.on?`Рассвет начнётся за ${S.alarmDawn} мин, каждый день.`:'Будильник выключен.';
 const n=S.nightCfg;sw($('nightOn'),n.on);setVal($('nightStart'),hm(n.start));setVal($('nightEnd'),hm(n.end));setVal($('nightBright'),n.bright);$('nightBrightVal').textContent=n.bright;
 $('weatherLine').innerHTML=W.error?`<span class="err">${esc(W.error)}</span>`:W.ago===null?'Погода ещё не получена.':`${W.temp>0?'+':''}${W.temp}°, эффект «${esc(W.effect)}», обновлено ${W.ago<60?'только что':Math.round(W.ago/60)+' мин назад'}.`;
 setVal($('city'),W.city);setVal($('tz'),W.tz);$('owmKey').placeholder=W.hasKey?'Ключ задан — вставь новый, чтобы заменить':'Ключ OpenWeatherMap';
 $('wifiLine').textContent=w.connected?`Подключено к «${w.ssid}», адрес ${w.ip}, сигнал ${w.rssi} дБм`:w.portal?`Часы раздают сеть «${w.ap}». Выбери домашнюю сеть:`:'Подключаюсь…';
 const up=S.uptime,dd=Math.floor(up/86400),hh=Math.floor(up%86400/3600),mi=Math.floor(up%3600/60);
 $('sysLine').textContent=`Версия ${S.fw} · работают ${dd?dd+' д ':''}${hh} ч ${mi} мин · свободно памяти ${Math.round(S.heap/1024)} КБ`;
}

/* ---------- действия ---------- */
$('power').onclick=()=>set({power:S.power?0:1});
$('bright').oninput=e=>{fillRange(e.target);$('brightVal').textContent=e.target.value;later('b',()=>set({brightness:e.target.value}),150)};
document.querySelectorAll('#season button').forEach(b=>b.onclick=()=>set({season:b.dataset.s}));
$('textGo').onclick=()=>set({text:$('text').value,mode:5});
$('text').onkeydown=e=>{if(e.key==='Enter')$('textGo').click()};
$('alarmTime').onchange=e=>set({alarmTime:e.target.value});
$('alarmOn').onclick=()=>set({alarmOn:S.alarm.on?0:1,alarmTime:$('alarmTime').value});
$('nightOn').onclick=()=>set({nightOn:S.nightCfg.on?0:1});
$('nightStart').onchange=e=>set({nightStart:e.target.value});
$('nightEnd').onchange=e=>set({nightEnd:e.target.value});
$('nightBright').oninput=e=>{fillRange(e.target);$('nightBrightVal').textContent=e.target.value;later('nb',()=>set({nightBright:e.target.value}),200)};
$('weatherSave').onclick=()=>{const p={city:$('city').value,tz:$('tz').value};if($('owmKey').value)p.owmKey=$('owmKey').value;$('owmKey').value='';set(p).then(()=>toast('Сохранено'))};
$('weatherNow').onclick=()=>set({weatherRefresh:1}).then(()=>toast('Запросил погоду…'));
$('otaSave').onclick=()=>{if(!$('otaPass').value)return;set({otaPass:$('otaPass').value}).then(()=>{$('otaPass').value='';toast('Пароль сохранён, заработает после перезагрузки')})};
$('restart').onclick=async()=>{if(!confirm('Перезагрузить часы?'))return;await api('/api/restart',{}).catch(()=>{});toast('Перезагружаюсь… обнови страницу через 10 секунд')};
$('scan').onclick=async()=>{
 $('scan').disabled=true;$('scan').textContent='Ищу сети…';
 for(let i=0;i<15;i++){const r=await api('/api/scan').catch(()=>null);
  if(r&&!r.scanning){const box=$('nets');box.innerHTML='';
   r.networks.forEach(n=>{const b=document.createElement('button');b.innerHTML=`<span>${esc(n.ssid)}</span><span class="val">${n.secure?'🔒 ':''}${n.rssi} дБм</span>`;b.onclick=()=>{$('joinBox').hidden=false;$('ssid').value=n.ssid;$('pass').focus()};box.appendChild(b)});
   if(!r.networks.length)box.innerHTML='<p class="hint">Сетей не найдено.</p>';break}
  await new Promise(r=>setTimeout(r,1000))}
 $('joinBox').hidden=false;$('scan').disabled=false;$('scan').textContent='Найти сети ещё раз'};
$('join').onclick=async()=>{const ssid=$('ssid').value.trim();if(!ssid)return;
 await api('/api/wifi',{ssid,pass:$('pass').value}).catch(()=>{});
 toast('Подключаюсь к «'+ssid+'». Новый адрес часы покажут бегущей строкой.')};

async function poll(){if(!document.hidden){const g=gen;try{const s=await api('/api/state');if(g===gen){S=s;render()}}catch(e){}}setTimeout(poll,3000)}
(async()=>{sizePv();requestAnimationFrame(draw);pull();
 try{meta=await api('/api/meta');buildMeta()}catch(e){toast('Нет связи с часами')}poll()})();
</script></body></html>)rawliteral";
