/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja strony interfejsu użytkownika (opis w page.h)
*/

#include <Arduino.h>
#include "camera.h"
#include "page.h"

// strona HTML w pamięci programu; na ESP32 pamięć flash jest mapowana w przestrzeni adresowej,
// więc tablicę można przekazać bezpośrednio do httpd_resp_send() bez kopiowania do RAM
static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Samojezdna platforma do skanowania pomieszczeń</title>
<!--
  Samojezdna platforma do skanowania pomieszczeń — dashboard
  Jeden plik, zero zależności zewnętrznych (działa offline, w sieci AP ESP32-CAM).

  Komunikacja z ESP32-CAM:
    - WebSocket ws://<host>:80/      → komendy W: do Arduino, dane A: z Arduino
    - HTTP http://<host>:81/         → /capture, /stream/start, /stream, /stream/stop

  Konwencja kąta serwa: 180° = lewa strona, 90° = przód, 0° = prawa strona.
-->
<style>
:root{
  --bg:#06070b;
  --panel:#0b0e15;
  --panel-2:#10141d;
  --line:#192030;
  --line-2:#242d43;
  --text:#e1e8f6;
  --muted:#6f7b94;
  --dim:#3b4458;
  --cyan:#00e5ff;
  --magenta:#ff2e97;
  --lime:#b4ff1f;
  --amber:#ffb300;
  --violet:#9d6bff;
  --red:#ff3d57;
  --mono:ui-monospace,"JetBrains Mono","Cascadia Code","SFMono-Regular",Consolas,"Liberation Mono",monospace;
  --sans:system-ui,-apple-system,"Segoe UI",Roboto,Ubuntu,sans-serif;
  --r:10px;
  --gap:10px;
}
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;background:var(--bg);color:var(--text);font-family:var(--sans);font-size:13px;overflow:hidden;-webkit-font-smoothing:antialiased}
button,input,select{font:inherit;color:inherit}
::selection{background:var(--magenta);color:#000}
::-webkit-scrollbar{width:6px;height:6px}
::-webkit-scrollbar-thumb{background:var(--line-2);border-radius:3px}
::-webkit-scrollbar-track{background:transparent}

/* ===== GŁÓWNY GRID ===== */
.app{
  height:100vh;display:grid;gap:var(--gap);padding:var(--gap);
  grid-template-columns:minmax(250px,19vw) minmax(0,1fr) minmax(290px,23vw);
  grid-template-rows:auto minmax(0,1fr) minmax(230px,33vh);
  grid-template-areas:
    "telem main   status"
    "telem main   debug"
    "telem cam    debug";
}
.panel{--accent:var(--cyan);background:var(--panel);border:1px solid var(--line);border-radius:var(--r);
  position:relative;display:flex;flex-direction:column;min-height:0;min-width:0;overflow:hidden}
.panel::before{content:"";position:absolute;inset:0 0 auto 0;height:2px;background:linear-gradient(90deg,var(--accent),transparent 75%);z-index:2}
.telem {grid-area:telem; --accent:var(--cyan)}
.main  {grid-area:main;  --accent:var(--magenta)}
.status{grid-area:status;--accent:var(--lime)}
.debug {grid-area:debug; --accent:var(--amber)}
.cam   {grid-area:cam;   --accent:var(--violet)}

.p-head{display:flex;align-items:center;gap:8px;padding:10px 12px 9px;border-bottom:1px solid var(--line);flex-shrink:0}
.p-title{font-size:11px;font-weight:800;letter-spacing:.16em;text-transform:uppercase;white-space:nowrap}
.p-title::before{content:"";display:inline-block;width:6px;height:6px;background:var(--accent);margin-right:8px;vertical-align:1px;box-shadow:0 0 10px var(--accent)}
.sp{flex:1}
.p-body{padding:12px;overflow:auto;min-height:0;flex:1}

.tag{font-family:var(--mono);font-size:10px;padding:3px 7px;border:1px solid var(--line-2);border-radius:4px;color:var(--muted);white-space:nowrap;letter-spacing:.04em}
.tag.hot{color:#000;background:var(--accent);border-color:var(--accent);font-weight:700}

/* ===== PRZYCISKI / FORMULARZE ===== */
.btn{--c:var(--cyan);display:inline-flex;align-items:center;justify-content:center;gap:6px;text-decoration:none;
  background:var(--panel-2);border:1px solid var(--line-2);border-radius:7px;padding:8px 12px;cursor:pointer;
  font-weight:700;font-size:11px;letter-spacing:.08em;text-transform:uppercase;transition:all .12s;white-space:nowrap;color:var(--text)}
.btn:hover:not(:disabled):not(.disabled){border-color:var(--c);color:var(--c);box-shadow:0 0 14px -4px var(--c)}
.btn:active:not(:disabled){transform:translateY(1px)}
.btn.solid{background:var(--c);border-color:var(--c);color:#000}
.btn.solid:hover:not(:disabled):not(.disabled){color:#000;box-shadow:0 0 20px -2px var(--c)}
.btn:disabled,.btn.disabled{opacity:.35;cursor:not-allowed;pointer-events:none}
.btn.sm{padding:5px 9px;font-size:10px}
.link{background:none;border:0;color:var(--dim);cursor:pointer;font-size:10px;text-transform:uppercase;letter-spacing:.1em}
.link:hover{color:var(--amber)}
.log-h.recv .link:hover{color:var(--cyan)}
select,input[type=text]{background:var(--bg);border:1px solid var(--line-2);border-radius:6px;padding:7px 9px;outline:none;min-width:0}
input[type=number]{background:var(--bg);border:1px solid var(--line-2);border-radius:6px;padding:6px 8px;outline:none;width:72px;font-family:var(--mono);text-align:right}
input[type=number]:focus{border-color:var(--cyan);box-shadow:0 0 0 2px rgba(0,229,255,.15)}
select:focus,input[type=text]:focus{border-color:var(--amber);box-shadow:0 0 0 2px rgba(255,179,0,.15)}
input[type=text]{font-family:var(--mono);width:100%}

input[type=range]{--c:var(--cyan);--p:50%;-webkit-appearance:none;appearance:none;width:100%;height:6px;border-radius:3px;outline:none;cursor:pointer;
  background:linear-gradient(90deg,var(--c) var(--p),var(--line-2) var(--p))}
input[type=range].rtl{direction:rtl;background:linear-gradient(270deg,var(--c) var(--p),var(--line-2) var(--p))}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:var(--bg);border:3px solid var(--c);box-shadow:0 0 12px var(--c)}
input[type=range]::-moz-range-thumb{width:12px;height:12px;border-radius:50%;background:var(--bg);border:3px solid var(--c);box-shadow:0 0 12px var(--c)}

.seg{display:flex;padding:3px;gap:3px;background:var(--bg);border:1px solid var(--line);border-radius:7px}
.seg button{--c:var(--magenta);flex:1;background:none;border:0;border-radius:5px;padding:5px 10px;cursor:pointer;color:var(--muted);
  font-size:10px;font-weight:800;letter-spacing:.1em;text-transform:uppercase;white-space:nowrap}
.seg button:hover{color:var(--c)}
.seg button.on{background:rgba(255,46,151,.14);color:var(--c);box-shadow:inset 0 0 0 1px rgba(255,46,151,.45)}

/* ===== TELEMETRIA ===== */
.frame{background:var(--panel-2);border:1px solid var(--line);border-radius:8px;padding:10px;margin-bottom:14px}
.frame-top{display:flex;align-items:center;gap:8px;margin-bottom:10px}
.frame-lbl{font-size:10px;letter-spacing:.16em;color:var(--muted);font-weight:700;white-space:nowrap}
.pulse{width:8px;height:8px;border-radius:50%;background:var(--dim);flex-shrink:0}
.pulse.hit{animation:hit .5s ease-out}
@keyframes hit{0%{background:var(--cyan);box-shadow:0 0 0 0 rgba(0,229,255,.8)}100%{background:var(--dim);box-shadow:0 0 0 8px rgba(0,229,255,0)}}
.frame-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px 10px}
.frame .k{font-size:10px;color:var(--muted);text-transform:uppercase;letter-spacing:.08em}
.frame-grid .v{font-family:var(--mono);font-variant-numeric:tabular-nums;font-size:12px;margin-top:2px}
.frame-grid .v.big{font-size:20px;font-weight:800;color:var(--cyan);text-shadow:0 0 12px rgba(0,229,255,.4)}
.raw{margin-top:4px;font-family:var(--mono);font-size:11.5px;color:var(--cyan);background:var(--bg);border:1px solid var(--line);border-left:2px solid var(--cyan);
  border-radius:5px;padding:7px 8px;white-space:pre-wrap;word-break:break-all;min-height:32px}

.grp{margin-bottom:14px}
.grp-h{display:flex;align-items:center;gap:6px;font-size:10px;letter-spacing:.16em;text-transform:uppercase;color:var(--cyan);font-weight:700;
  padding-bottom:6px;margin-bottom:4px;border-bottom:1px dashed var(--line-2)}
.grp-h small{color:var(--dim);letter-spacing:.06em;font-weight:600}
.kv{display:flex;justify-content:space-between;align-items:baseline;padding:4px 0;gap:8px}
.kv .k{color:var(--muted)}
.kv .v{font-family:var(--mono);font-variant-numeric:tabular-nums;text-align:right;white-space:nowrap}
.kv .v b{font-weight:700;font-size:13px}
.kv .v i{font-style:normal;color:var(--dim);font-size:10px;margin-left:4px}

/* ===== STATUS ===== */
.conn{--c:var(--red);display:flex;align-items:center;gap:12px;padding:10px 12px;border-radius:8px;background:var(--panel-2);border:1px solid var(--line)}
.conn[data-state=connecting]{--c:var(--amber)}
.conn[data-state=online]{--c:var(--lime)}
.conn-dot{position:relative;width:14px;height:14px;border-radius:50%;background:var(--c);box-shadow:0 0 16px var(--c);flex-shrink:0}
.conn-dot::after{content:"";position:absolute;inset:-5px;border-radius:50%;border:2px solid var(--c);opacity:0;animation:ring 1.6s ease-out infinite}
.conn[data-state=offline] .conn-dot::after{animation:none}
@keyframes ring{0%{transform:scale(.6);opacity:.9}100%{transform:scale(1.6);opacity:0}}
.conn-label{font-weight:900;letter-spacing:.14em;font-size:14px;color:var(--c)}
.conn-sub{font-size:11px;color:var(--muted);margin-top:2px}
.stat-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:8px}
.stat{background:var(--panel-2);border:1px solid var(--line);border-radius:6px;padding:6px 8px;display:flex;flex-direction:column;gap:2px;min-width:0}
.stat .k{font-size:9.5px;color:var(--muted);text-transform:uppercase;letter-spacing:.1em}
.stat .v{font-family:var(--mono);font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}

/* ===== DEBUG ===== */
.dbg{display:flex;flex-direction:column;gap:10px}
.log-box{display:flex;flex-direction:column;flex:1;min-height:90px}
.log-h{display:flex;align-items:center;gap:8px;font-size:10px;letter-spacing:.14em;text-transform:uppercase;font-weight:700;margin-bottom:5px}
.log-h .cnt{font-family:var(--mono);color:#000;background:var(--c);padding:0 6px;border-radius:3px;font-size:10px}
.log{--c:var(--amber);list-style:none;flex:1;min-height:0;overflow:auto;background:var(--bg);border:1px solid var(--line);border-left:2px solid var(--c);
  border-radius:6px;padding:4px 8px;font-family:var(--mono);font-size:11px}
.log:empty::before{content:attr(data-empty);color:var(--dim);display:block;padding:4px 0}
.log li{display:flex;gap:8px;padding:3px 0;border-bottom:1px solid rgba(255,255,255,.03);white-space:nowrap}
.log li time{color:var(--dim);flex-shrink:0}
.log li span{overflow:hidden;text-overflow:ellipsis}
.log li span::before{content:"› ";color:var(--c)}
.log li:last-child span{color:var(--c)}
#log-recv{--c:var(--cyan)}
#log-recv li span::before{content:"‹ "}
.log-h.sent{--c:var(--amber);color:var(--amber)} .log-h.recv{--c:var(--cyan);color:var(--cyan)}

.lcd-box{flex-shrink:0;background:var(--panel-2);border:1px solid var(--line);border-radius:8px;padding:10px}
.lcd{background:#041217;border:2px solid #0d2c36;border-radius:6px;padding:8px;margin:6px 0 8px;box-shadow:inset 0 0 18px rgba(0,229,255,.08)}
.lcd-row{display:grid;gap:2px}
.lcd-row span{background:rgba(0,229,255,.06);font-family:var(--mono);font-size:13px;text-align:center;line-height:20px;color:var(--cyan);
  text-shadow:0 0 6px rgba(0,229,255,.8);border-radius:1px;min-width:0}
.lcd-in{display:grid;grid-template-columns:1fr auto;gap:6px;align-items:center}
.lcd-in .cc{font-family:var(--mono);font-size:10px;color:var(--dim);min-width:38px;text-align:right}
.lcd-in .cc.warn{color:var(--red)}
.lcd-actions{display:flex;gap:6px;margin-top:8px}
.lcd-actions .btn{flex:1}

/* ===== MAIN: NAGŁÓWEK + TRYBY ===== */
.main-head{display:flex;align-items:center;gap:14px;padding:10px 12px;border-bottom:1px solid var(--line);flex-shrink:0}
.brand{flex:1 1 0;min-width:0;border-left:3px solid var(--magenta);padding-left:10px}
.brand-title{font-weight:900;font-size:clamp(12px,1.05vw,16px);letter-spacing:.02em;line-height:1.2}
.brand-author{font-size:10px;color:var(--muted);letter-spacing:.14em;text-transform:uppercase;margin-top:3px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.brand-author b{color:var(--magenta);font-weight:700}

.tabs{display:flex;gap:4px;padding:4px;background:var(--bg);border:1px solid var(--line);border-radius:9px;flex-shrink:0}
.tab{--tc:var(--magenta);background:none;border:0;padding:8px 18px;border-radius:6px;cursor:pointer;color:var(--muted);
  font-weight:800;font-size:11px;letter-spacing:.14em;text-transform:uppercase;transition:all .15s;display:flex;align-items:center;gap:8px}
.tab kbd{font-family:var(--mono);font-size:9px;opacity:.6;border:1px solid currentColor;border-radius:3px;padding:0 4px}
.tab:hover{color:var(--tc)}
.tab.active{background:var(--tc);color:#000;box-shadow:0 0 22px -4px var(--tc)}
.tab[data-mode=map]{--tc:var(--lime)}
.head-right{display:flex;gap:8px;align-items:center}

.modes{flex:1;min-height:0;display:flex}
.mode{display:none;flex:1;min-height:0;min-width:0;padding:12px;gap:12px}
.mode.active{display:grid}

.card{background:var(--panel-2);border:1px solid var(--line);border-radius:9px;padding:12px;display:flex;flex-direction:column;gap:12px;min-height:0;min-width:0;overflow:auto}
.card-h{font-size:10px;letter-spacing:.18em;text-transform:uppercase;font-weight:800;color:var(--ch,var(--magenta));display:flex;align-items:center;gap:8px}
.card-h::after{content:"";flex:1;height:1px;background:linear-gradient(90deg,var(--line-2),transparent)}
.mode .card-h{color:rgba(255,46,151,.85)}

/* --- tryb sterowania --- */
#mode-control{grid-template-columns:minmax(0,1fr) auto minmax(0,1fr);grid-template-rows:minmax(0,1fr) auto;
  grid-template-areas:"speed pad servo" "fb fb fb"}
.c-speed{grid-area:speed}.c-pad{grid-area:pad;align-items:center}.c-servo{grid-area:servo}

.sl-top{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:8px}
.sl-top span{color:var(--muted);font-size:11px;text-transform:uppercase;letter-spacing:.1em}
.sl-top b{font-family:var(--mono);font-size:18px;font-weight:800}
.sl-top b i{font-style:normal;font-size:10px;color:var(--dim);margin-left:4px;font-weight:600}
.vsl-wrap{flex:1;min-height:130px;display:grid;grid-template-columns:1fr 1fr;gap:12px;align-items:stretch}
.vsl{display:flex;flex-direction:column;align-items:center;gap:8px;min-height:0}
.vsl b{font-family:var(--mono);font-size:18px;font-weight:800;color:var(--text)}
.vsl span{font-size:10px;color:var(--muted);letter-spacing:.12em;text-transform:uppercase;font-weight:700}
input[type=range].vert{writing-mode:vertical-lr;direction:rtl;width:6px;height:auto;flex:1;min-height:90px;
  background:linear-gradient(0deg,var(--c) var(--p),var(--line-2) var(--p))}
.presets{display:flex;flex-direction:column;gap:6px;flex-shrink:0}
.presets .btn{--c:var(--magenta)}

.pad{--s:clamp(44px,6.6vh,74px);display:grid;grid-template-columns:repeat(3,var(--s));grid-template-rows:repeat(3,var(--s));gap:7px}
.pbtn{background:var(--panel);border:1px solid var(--line-2);border-radius:12px;font-size:calc(var(--s)*.34);cursor:pointer;color:var(--text);
  transition:background .08s,box-shadow .08s,border-color .08s;touch-action:none;user-select:none;-webkit-user-select:none}
.pbtn:hover{border-color:var(--magenta)}
.pbtn.pressed{background:var(--magenta);border-color:var(--magenta);color:#000;box-shadow:0 0 26px -2px var(--magenta)}
.pbtn.stop{border-color:rgba(255,61,87,.5);color:var(--red);font-size:calc(var(--s)*.2);font-weight:900;letter-spacing:.1em}
.pbtn.stop.pressed,.pbtn.stop:active{background:var(--red);color:#000;box-shadow:0 0 26px -2px var(--red)}
.drive-state{font-family:var(--mono);font-size:11px;color:var(--muted);text-align:center}
.drive-state b{display:block;font-size:16px;color:var(--text);letter-spacing:.12em;margin-top:2px}
.drive-state b.moving{color:var(--magenta);text-shadow:0 0 12px rgba(255,46,151,.45)}
.hint{font-size:10px;color:var(--dim);text-align:center;letter-spacing:.06em}
.c-pad .seg{align-self:stretch}

.gauge{width:100%;max-height:120px;display:block}
.servo-val{text-align:center;font-family:var(--mono);margin-top:-6px}
.servo-val b{font-size:24px;font-weight:900;color:var(--text)}
.servo-val small{display:block;font-size:10px;color:var(--muted)}
.servo-btns{display:grid;grid-template-columns:repeat(3,1fr);gap:6px}
.servo-btns .btn{--c:var(--magenta)}

.fb{grid-area:fb;display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:8px}
.metric{--c:var(--magenta);background:var(--panel-2);border:1px solid var(--line);border-left:2px solid var(--c);border-radius:7px;padding:9px 10px;min-width:0;position:relative;overflow:hidden}
.m-label{font-size:9.5px;letter-spacing:.14em;text-transform:uppercase;color:var(--muted);font-weight:700}
.m-val{font-family:var(--mono);font-variant-numeric:tabular-nums;font-size:clamp(15px,1.5vw,22px);font-weight:800;margin-top:3px;white-space:nowrap;color:var(--text)}
.metric .m-label{color:var(--c);opacity:.85}
.m-val i{font-style:normal;font-size:10px;color:var(--dim);margin-left:3px;font-weight:600}
.m-sub{font-family:var(--mono);font-size:10px;color:var(--muted);margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.prox{height:4px;background:var(--line-2);border-radius:2px;margin-top:6px;overflow:hidden}
.prox i{display:block;height:100%;width:0;background:var(--c);transition:width .2s,background .2s}
.compass{position:absolute;right:8px;top:8px;width:30px;height:30px}
.metric.has-compass .m-label,.metric.has-compass .m-val{padding-right:34px}

/* --- tryb radaru --- */
#mode-radar{grid-template-columns:minmax(0,1fr) 230px;grid-template-rows:minmax(0,1fr)}
.radar-wrap{position:relative;padding:0;overflow:hidden;background:radial-gradient(ellipse at 50% 100%,rgba(0,229,255,.06),transparent 70%),var(--panel-2)}
.radar-wrap canvas{position:absolute;inset:0;width:100%;height:100%}
.radar-legend{position:absolute;left:12px;top:10px;display:flex;flex-wrap:wrap;gap:6px 12px;font-size:10px;color:var(--muted);letter-spacing:.08em;text-transform:uppercase;pointer-events:none}
.radar-legend span::before{content:"";display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--c);margin-right:5px;box-shadow:0 0 6px var(--c)}
.radar-legend span.unc::before{width:14px;height:7px;border-radius:14px 14px 0 0;background:none;border:2px solid rgba(180,255,31,.8);border-bottom:0;box-shadow:none}
.radar-legend span.lim::before{width:12px;height:0;border-radius:0;background:none;border-top:2px dashed var(--magenta);box-shadow:none;vertical-align:3px}
.radar-side{display:flex;flex-direction:column;gap:12px;min-height:0;overflow:auto}
.radar-side .card{overflow:visible;flex-shrink:0}
.radar-side label{display:flex;justify-content:space-between;align-items:center;gap:8px;color:var(--muted);font-size:11px}
#radar-scan{--c:var(--magenta)}
.radar-main{display:flex;flex-direction:column;gap:8px;min-height:0;min-width:0}
.radar-main .radar-wrap{flex:1}
#radar-clear{--c:var(--magenta);align-self:stretch}
.ctl{display:flex;flex-direction:column;gap:8px}
.ctl-top{display:flex;justify-content:space-between;align-items:baseline;color:var(--muted);font-size:11px}
.ctl-top b{font-family:var(--mono);color:var(--text);font-size:13px}
.pair{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.pair label{flex-direction:column;align-items:stretch;gap:4px}
.pair select{width:100%}
.dual{position:relative;height:22px}
.dual-track{position:absolute;left:0;right:0;top:8px;height:6px;border-radius:3px;background:var(--line-2)}
.dual-track i{position:absolute;top:0;bottom:0;border-radius:3px;background:var(--magenta);box-shadow:0 0 8px rgba(255,46,151,.6)}
.dual input[type=range]{position:absolute;left:0;right:0;top:8px;margin:0;background:transparent;direction:rtl;pointer-events:none;--c:var(--magenta)}
.dual input[type=range]::-webkit-slider-thumb{pointer-events:auto}
.dual input[type=range]::-moz-range-thumb{pointer-events:auto}
.read .big-stat{color:var(--text);text-shadow:none}
.read.hl .big-stat{color:var(--magenta);text-shadow:0 0 14px rgba(255,46,151,.5)}
.read.hl{border-left:2px solid var(--magenta);padding-left:10px}
.read{border-left:2px solid var(--line-2);padding-left:10px}
#radar-scan.on{--c:var(--red)}
.big-stat{font-family:var(--mono);font-size:26px;font-weight:900;color:var(--magenta);text-shadow:0 0 14px rgba(255,46,151,.5)}
.big-stat i{font-style:normal;font-size:11px;color:var(--dim);margin-left:4px}

/* --- tryb mapowania --- */
#mode-map{grid-template-columns:1fr}
.map-empty{border:1px dashed var(--line-2);border-radius:9px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;text-align:center;
  background-image:linear-gradient(rgba(180,255,31,.05) 1px,transparent 1px),linear-gradient(90deg,rgba(180,255,31,.05) 1px,transparent 1px);background-size:28px 28px}
.map-empty .ico{font-size:42px;color:var(--lime);text-shadow:0 0 22px var(--lime);line-height:1}
.map-empty b{letter-spacing:.2em;font-size:13px}
.map-empty span{color:var(--muted);font-size:12px}

/* ===== KAMERA ===== */
.cam-body{flex:1;min-height:0;display:grid;grid-template-columns:minmax(0,1.6fr) minmax(180px,1fr);gap:10px;padding:10px}
.viewer{position:relative;background:#000;border:1px solid var(--line-2);border-radius:8px;overflow:hidden;min-height:0}
.viewer img{position:absolute;inset:0;width:100%;height:100%;object-fit:contain}
.viewer-off{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;
  font-family:var(--mono);font-size:11px;letter-spacing:.2em;color:var(--dim)}
.viewer-off .ico{font-size:30px;color:var(--line-2)}
.cam[data-state=on] .viewer-off{display:none}

.cam-side{display:flex;flex-direction:column;gap:8px;min-height:0;min-width:0}
.cam-btns{display:grid;grid-template-columns:1fr 1fr;gap:6px}
#cam-toggle{--c:var(--violet)}
.cam[data-state=on] #cam-toggle{--c:var(--red)}
#cam-snap{--c:var(--violet)}
.gal-h{display:flex;justify-content:space-between;font-size:10px;letter-spacing:.14em;text-transform:uppercase;color:var(--muted);font-weight:700;margin-top:2px}
.gal-h span:last-child{font-family:var(--mono);letter-spacing:.04em;text-transform:none}
.shot{position:relative;flex:1;min-height:0;background:var(--bg);border:1px solid var(--line);border-radius:6px;overflow:hidden}
.shot img{position:absolute;inset:0;width:100%;height:100%;object-fit:contain}
.shot:not(.has) img{display:none}
.shot-empty{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;color:var(--dim);font-size:11px}
.shot.has .shot-empty{display:none}
#snap-dl{--c:var(--violet)}

/* ===== RWD: wąskie / niskie ekrany ===== */
@media (max-height:760px){
  .fb .m-sub{display:none}
  .card{padding:10px;gap:9px}
}
@media (max-width:1150px){
  html,body{height:auto;overflow:visible}
  .app{height:auto;grid-template-columns:minmax(0,1fr);grid-template-rows:none;grid-template-areas:"status" "main" "cam" "telem" "debug"}
  .main{min-height:720px}.cam{height:340px}.debug{height:520px}
  #mode-control{grid-template-columns:minmax(0,1fr) minmax(0,1fr);grid-template-rows:none;grid-template-areas:"pad pad" "speed servo" "fb fb"}
  .vsl-wrap{min-height:190px}
  #mode-radar{grid-template-columns:minmax(0,1fr);grid-template-rows:420px auto}
  .fb{display:flex;flex-wrap:wrap;justify-content:center}
  .fb .metric{flex:0 0 calc((100% - 16px) / 3)}
  .main-head{flex-wrap:wrap}
}

/* ===== TELEFONY ===== */
@media (max-width:700px){
  :root{--gap:8px}
  body{font-size:14px}
  .app{padding:8px}

  /* nagłówek + wybór trybu */
  .main-head{flex-direction:column;align-items:stretch;gap:10px}
  .brand-title{font-size:15px}
  .tabs{width:100%}
  .tab{flex:1;justify-content:center;padding:10px 4px;font-size:10.5px;letter-spacing:.06em;min-width:0}
  .tab kbd{display:none}

  .main{min-height:0}
  .mode{padding:8px;gap:8px}
  .card{overflow:visible}

  /* sterowanie */
  #mode-control{grid-template-columns:minmax(0,1fr);grid-template-areas:"pad" "speed" "servo" "fb"}
  .pad{--s:64px}
  .vsl-wrap{min-height:190px}
  .presets{flex-direction:row}.presets .btn{flex:1;padding:10px 6px}
  .fb .metric{flex-basis:calc((100% - 8px) / 2)}
  .m-val{font-size:20px}
  .hint{display:none}

  /* radar */
  #mode-radar{grid-template-rows:auto auto}
  .radar-main .radar-wrap{flex:none;height:auto;aspect-ratio:1.4}
  .radar-legend{left:8px;top:6px;font-size:9px;gap:3px 8px;letter-spacing:.04em}
  #radar-clear{padding:11px}

  /* kamera — układ pionowy */
  .cam{height:auto}
  .cam-body{display:flex;flex-direction:column;gap:10px}
  .viewer{flex:none;aspect-ratio:4/3}
  .cam-side{gap:10px}
  .cam-btns .btn{padding:12px}
  .shot{flex:none;aspect-ratio:4/3}
  #snap-dl{padding:12px;font-size:11px}

  /* debugowanie */
  .debug{height:auto}
  .log-box{flex:none}
  .log{flex:none;height:170px}

  /* pola tekstowe ≥16px — iOS nie przybliża strony przy fokusie */
  input[type=text],select{font-size:16px}
}
</style>
</head>
<body>
<div class="app">

  <!-- ================= LEWO: TELEMETRIA ================= -->
  <aside class="panel telem">
    <div class="p-head">
      <span class="p-title">Telemetria</span>
    </div>
    <div class="p-body">
      <div class="frame">
        <div class="frame-top">
          <span class="pulse" id="frame-pulse"></span>
          <span class="frame-lbl">OSTATNIA RAMKA · TREŚĆ</span>
        </div>
        <div class="raw" id="frame-raw">brak danych</div>
      </div>

      <div class="grp">
        <div class="grp-h">Enkodery</div>
        <div class="kv"><span class="k">Impulsy L</span><span class="v"><b data-t="encL">—</b><i>imp</i></span></div>
        <div class="kv"><span class="k">Impulsy P</span><span class="v"><b data-t="encR">—</b><i>imp</i></span></div>
        <div class="kv"><span class="k">Prędkość</span><span class="v"><b data-t="speed" data-d="1">—</b><i>cm/s</i></span></div>
      </div>

      <div class="grp">
        <div class="grp-h">Odometria <small>· enkodery + żyroskop</small></div>
        <div class="kv"><span class="k">Pozycja X</span><span class="v"><b data-t="x" data-d="1">—</b><i>cm</i></span></div>
        <div class="kv"><span class="k">Pozycja Y</span><span class="v"><b data-t="y" data-d="1">—</b><i>cm</i></span></div>
        <div class="kv"><span class="k">Orientacja θ</span><span class="v"><b data-t="theta" data-d="1">—</b><i>°</i></span></div>
        <div class="lcd-actions">
          <button class="btn sm" id="odo-reset" title="Ustawia pozycję i orientację na (0, 0, 0)">Reset pozycji</button>
          <button class="btn sm" id="odo-calib" title="Kalibracja żyroskopu — pojazd musi stać nieruchomo">Kalibracja</button>
        </div>
      </div>

      <div class="grp">
        <div class="grp-h">Akcelerometr <small>· MPU6050</small></div>
        <div class="kv"><span class="k">X</span><span class="v"><b data-t="ax" data-d="2">—</b><i>g</i></span></div>
        <div class="kv"><span class="k">Y</span><span class="v"><b data-t="ay" data-d="2">—</b><i>g</i></span></div>
        <div class="kv"><span class="k">Z</span><span class="v"><b data-t="az" data-d="2">—</b><i>g</i></span></div>
      </div>

      <div class="grp">
        <div class="grp-h">Żyroskop <small>· MPU6050</small></div>
        <div class="kv"><span class="k">X</span><span class="v"><b data-t="gx" data-d="1">—</b><i>°/s</i></span></div>
        <div class="kv"><span class="k">Y</span><span class="v"><b data-t="gy" data-d="1">—</b><i>°/s</i></span></div>
        <div class="kv"><span class="k">Z</span><span class="v"><b data-t="gz" data-d="1">—</b><i>°/s</i></span></div>
      </div>

      <div class="grp">
        <div class="grp-h">Temperatura <small>· MPU6050</small></div>
        <div class="kv"><span class="k">Temperatura</span><span class="v"><b data-t="temp" data-d="1">—</b><i>°C</i></span></div>
      </div>

      <div class="grp">
        <div class="grp-h">Sonar <small>· HC-SR04 + serwo</small></div>
        <div class="kv"><span class="k">Kąt serwa</span><span class="v"><b data-t="servo">—</b><i>°</i></span></div>
        <div class="kv"><span class="k">Odległość od przeszkody</span><span class="v"><b data-t="sonar" data-d="1">—</b><i>cm</i></span></div>
      </div>
    </div>
  </aside>

  <!-- ================= ŚRODEK: STEROWANIE ================= -->
  <main class="panel main">
    <div class="main-head">
      <div class="brand">
        <div class="brand-title" title="Samojezdna platforma do skanowania pomieszczeń">Samojezdna platforma do skanowania pomieszczeń</div>
        <div class="brand-author">Autor: <b>Błażej Pawluk</b></div>
      </div>
      <nav class="tabs" role="tablist">
        <button class="tab active" data-mode="control" role="tab">Sterowanie <kbd>1</kbd></button>
        <button class="tab" data-mode="radar" role="tab">Radar <kbd>2</kbd></button>
        <button class="tab" data-mode="map" role="tab">Mapowanie <kbd>3</kbd></button>
      </nav>
    </div>

    <div class="modes">
      <!-- ---------- TRYB: STEROWANIE ---------- -->
      <section class="mode active" id="mode-control">
        <div class="card c-speed">
          <div class="card-h">Prędkość kół</div>
          <div class="vsl-wrap">
            <div class="vsl">
              <b><output id="o-left">180</output></b>
              <input type="range" class="vert" id="s-left" max="255" value="180" data-out="o-left" style="--c:var(--magenta)">
              <span>Lewe</span>
            </div>
            <div class="vsl">
              <b><output id="o-right">180</output></b>
              <input type="range" class="vert" id="s-right" max="255" value="180" data-out="o-right" style="--c:var(--magenta)">
              <span>Prawe</span>
            </div>
          </div>
          <div class="presets">
            <button class="btn sm" data-preset="min">Min</button>
            <button class="btn sm" data-preset="mid">Średnio</button>
            <button class="btn sm" data-preset="max">Max</button>
          </div>
          <div class="hint">PWM <span id="spd-range"></span></div>
        </div>

        <div class="card c-pad">
          <div class="card-h" style="align-self:stretch">Napęd</div>
          <div class="seg" id="drive-mode">
            <button data-dm="hold" class="on">Przytrzymaj</button>
            <button data-dm="toggle">Przełącz</button>
          </div>
          <div class="pad" id="pad">
            <span></span><button class="pbtn" data-dir="F">▲</button><span></span>
            <button class="pbtn" data-dir="L">◀</button>
            <button class="pbtn stop" data-dir="S">STOP</button>
            <button class="pbtn" data-dir="R">▶</button>
            <span></span><button class="pbtn" data-dir="B">▼</button><span></span>
          </div>
          <div class="drive-state">KIERUNEK<b id="drive-dir">STOP</b></div>
          <div class="hint" id="drive-hint">przytrzymaj kierunek · W A S D / strzałki · spacja = STOP</div>
        </div>

        <div class="card c-servo">
          <div class="card-h">Serwo · sonar</div>
          <svg class="gauge" viewBox="0 0 200 112" aria-hidden="true">
            <path id="g-track" fill="none" stroke="#242d43" stroke-width="10" stroke-linecap="round"/>
            <path id="g-arc" d="M180 100 A80 80 0 0 0 100 20" fill="none" stroke="#ff2e97" stroke-width="10" stroke-linecap="round" stroke-opacity=".75" style="filter:drop-shadow(0 0 3px rgba(255,46,151,.5))"/>
            <g id="g-ticks"></g>
            <line id="g-set" x1="100" y1="100" x2="100" y2="28" stroke="#e1e8f6" stroke-width="3" stroke-linecap="round"/>
            <circle cx="100" cy="100" r="6" fill="#06070b" stroke="#ff2e97" stroke-width="2"/>
            <text id="g-max" text-anchor="middle" fill="#6f7b94" font-size="9" font-family="monospace"></text>
            <text id="g-min" text-anchor="middle" fill="#6f7b94" font-size="9" font-family="monospace"></text>
          </svg>
          <div class="servo-val"><b><span id="servo-set">90</span>°</b><small>kąt serwa</small></div>
          <input type="range" class="rtl" id="s-servo" min="0" max="180" value="90" style="--c:var(--magenta)">
          <div class="servo-btns">
            <button class="btn sm" data-servo="+15">« 15°</button>
            <button class="btn sm" data-servo="center">⌖ 90°</button>
            <button class="btn sm" data-servo="-15">15° »</button>
          </div>
        </div>

        <div class="fb">
          <div style="--c:var(--cyan)" class="metric">
            <div class="m-label">Pozycja (x, y)</div>
            <div class="m-val"><span data-t="x" data-d="0">—</span><i>x</i> <span data-t="y" data-d="0">—</span><i>y</i></div>
            <div class="m-sub">cm · względem startu</div>
          </div>
          <div style="--c:var(--violet)" class="metric has-compass">
            <svg class="compass" viewBox="0 0 30 30" aria-hidden="true">
              <circle cx="15" cy="15" r="13" fill="none" stroke="#242d43" stroke-width="2"/>
              <g id="compass-needle"><path d="M15 4 L19 16 L15 14 L11 16 Z" fill="#9d6bff"/></g>
            </svg>
            <div class="m-label">Orientacja θ</div>
            <div class="m-val"><span data-t="theta" data-d="0">—</span><i>°</i></div>
            <div class="m-sub">kąt obrotu platformy</div>
          </div>
          <div style="--c:var(--amber)" class="metric" id="m-obst">
            <div class="m-label">Przeszkoda</div>
            <div class="m-val"><span data-t="sonar" data-d="0">—</span><i>cm</i></div>
            <div class="prox"><i id="prox-bar"></i></div>
          </div>
          <div style="--c:var(--lime)" class="metric">
            <div class="m-label">Prędkość</div>
            <div class="m-val"><span data-t="speed" data-d="1">—</span><i>cm/s</i></div>
            <div class="m-sub">z enkoderów</div>
          </div>
          <div style="--c:var(--magenta)" class="metric">
            <div class="m-label">Kąt serwa</div>
            <div class="m-val"><span data-t="servo">—</span><i>°</i></div>
            <div class="m-sub">kierunek sonaru</div>
          </div>
        </div>
      </section>

      <!-- ---------- TRYB: RADAR ---------- -->
      <section class="mode" id="mode-radar">
        <div class="radar-main">
          <div class="card radar-wrap" id="radar-wrap">
            <canvas id="radar"></canvas>
            <div class="radar-legend">
              <span style="--c:var(--lime)">pomiar</span>
              <span class="unc" id="unc-legend">łuk = szerokość wiązki</span>
              <span class="lim">zakres skanu</span>
            </div>
          </div>
          <button class="btn" id="radar-clear">Wyczyść</button>
        </div>
        <div class="radar-side">
          <div class="card">
            <div class="card-h">Skan</div>
            <button class="btn solid" id="radar-scan">▶ Start skanu</button>
            <div class="ctl">
              <div class="ctl-top"><span>Zasięg</span><b><output id="o-range">200</output> cm</b></div>
              <input type="range" id="radar-range" min="50" max="400" step="10" value="200" data-out="o-range" style="--c:var(--magenta)">
            </div>
            <div class="ctl">
              <div class="ctl-top"><span>Zakres (od–do)</span><b><span id="o-from">0</span>° – <span id="o-to">180</span>°</b></div>
              <div class="dual">
                <div class="dual-track"><i id="dual-fill"></i></div>
                <input type="range" id="radar-from" min="0" max="180" step="1" value="0" aria-label="Od">
                <input type="range" id="radar-to" min="0" max="180" step="1" value="180" aria-label="Do">
              </div>
            </div>
            <div class="pair">
              <label>Krok serwa
                <select id="radar-step"><option value="1">1°</option><option value="2">2°</option><option value="5" selected>5°</option><option value="10">10°</option></select>
              </label>
              <label>Interwał
                <select id="radar-int"><option value="60">60 ms</option><option value="100">100 ms</option><option value="200" selected>200 ms</option><option value="500">500 ms</option><option value="1000">1 s</option></select>
              </label>
            </div>
          </div>
          <div class="card">
            <div class="card-h">Odczyt</div>
            <div class="read hl">
              <div class="m-label">Najbliższy na wizualizacji</div>
              <div class="big-stat"><span id="r-near">—</span><i>cm</i></div>
              <div class="m-sub">przy kącie <span id="r-near-a">—</span>°</div>
            </div>
            <div class="read">
              <div class="m-label">Obecny pomiar</div>
              <div class="big-stat"><span id="r-cur">—</span><i>cm</i></div>
              <div class="m-sub">przy kącie <span id="r-cur-a">—</span>°</div>
            </div>
          </div>
        </div>
      </section>

      <!-- ---------- TRYB: MAPOWANIE ---------- -->
      <section class="mode" id="mode-map">
        <div class="map-empty">
          <div class="ico">⌗</div>
          <b>MAPOWANIE POMIESZCZENIA</b>
          <span>Moduł w przygotowaniu — tu pojawi się mapa 2D pomieszczenia.</span>
        </div>
      </section>
    </div>
  </main>

  <!-- ================= PRAWO GÓRA: STATUS ================= -->
  <section class="panel status">
    <div class="p-head"><span class="p-title">Połączenie</span></div>
    <div class="p-body">
      <div class="conn" id="conn" data-state="offline">
        <div class="conn-dot"></div>
        <div style="flex:1;min-width:0">
          <div class="conn-label" id="conn-label">ROZŁĄCZONO</div>
          <div class="conn-sub" id="conn-sub">Brak połączenia z pojazdem</div>
        </div>
      </div>
      <div class="stat-grid">
        <div class="stat"><span class="k">Host</span><span class="v" data-s="host">—</span></div>
        <div class="stat"><span class="k">Dane z Arduino</span><span class="v" data-s="uart">—</span></div>
      </div>
    </div>
  </section>

  <!-- ================= PRAWO DÓŁ: DEBUG ================= -->
  <section class="panel debug">
    <div class="p-head"><span class="p-title">Debugowanie</span></div>
    <div class="p-body dbg">
      <div class="log-box">
        <div class="log-h sent">Wysłane komendy<span class="sp"></span><button class="link" data-clear="sent">wyczyść</button></div>
        <ul class="log" id="log-sent" data-empty="— brak wysłanych komend —"></ul>
      </div>
      <div class="log-box">
        <div class="log-h recv">Odebrane komendy<span class="sp"></span><button class="link" data-clear="recv">wyczyść</button></div>
        <ul class="log" id="log-recv" data-empty="— brak odebranych komend —"></ul>
      </div>
      <div class="lcd-box">
        <div class="card-h" style="--ch:var(--amber)">Tekst na LCD <span id="lcd-size" style="color:var(--dim)"></span></div>
        <div class="lcd"><div class="lcd-row" id="lcd-row"></div></div>
        <div class="lcd-in"><input type="text" id="lcd-text" placeholder="Tekst do wyświetlenia" autocomplete="off"><span class="cc" id="lcd-cc"></span></div>
        <div class="lcd-actions">
          <button class="btn solid sm" id="lcd-send" style="--c:var(--amber)">Wyślij na LCD</button>
          <button class="btn sm" id="lcd-clear" style="--c:var(--amber)">Wyczyść LCD</button>
        </div>
      </div>
    </div>
  </section>

  <!-- ================= DÓŁ ŚRODEK: KAMERA ================= -->
  <section class="panel cam" id="cam" data-state="off">
    <div class="p-head">
      <span class="p-title">Kamera · ESP32-CAM</span><span class="sp"></span>
      <span class="tag" id="cam-tag">WYŁ.</span>
    </div>
    <div class="cam-body">
      <div class="viewer">
        <img id="cam-stream" alt="Podgląd z kamery" hidden>
        <div class="viewer-off"><div class="ico">◉</div>KAMERA WYŁĄCZONA</div>
      </div>
      <div class="cam-side">
        <div class="cam-btns">
          <button class="btn" id="cam-toggle">⏻ Włącz</button>
          <button class="btn" id="cam-snap">◎ Zdjęcie</button>
        </div>
        <div class="gal-h"><span>Ostatnie zdjęcie</span><span id="snap-time">—</span></div>
        <div class="shot" id="shot">
          <img id="snap-img" alt="Ostatnie zdjęcie">
          <div class="shot-empty">Brak zdjęcia</div>
        </div>
        <a class="btn sm disabled" id="snap-dl" download>⤓ Pobierz zdjęcie</a>
      </div>
    </div>
  </section>
</div>

<script>
'use strict';
/* =====================================================================
   KONFIGURACJA
   ===================================================================== */
const LCD_COLS    = 16;     // szerokość linii LCD
const LOG_MAX     = 60;     // maks. liczba wpisów w logach debugowania
const SPEED_MIN   = 80;     // minimalny PWM kół (poniżej silniki TT nie ruszają) — suwaki nie zejdą niżej
const SPEED_MAX   = 255;
const SERVO_MIN   = 15;     // ograniczenia mechaniczne serwa — Arduino nie sprawdza zakresu kąta
const SERVO_MAX   = 165;
const HOST        = location.hostname || '192.168.4.1';   // adres AP przy otwarciu strony z pliku
const WS_URL      = `ws://${HOST}:80/`;
const CAM_URL     = `http://${HOST}:81`;
const WS_RETRY_MS     = 1500;   // odstęp między próbami ponownego połączenia
const DATA_TIMEOUT_MS = 2000;   // brak danych przez ten czas = brak odpowiedzi Arduino
const MOTOR_REPEAT_MS = 500;    // powtarzanie W:MOTOR podczas jazdy (timeout na Arduino: 1 s)
const SERVO_HOLD_MS   = 400;    // po komendzie serwa ignorujemy starsze kąty z A:DIST
const CALIB_GAP_MS    = 1000;   // przerwa w danych = Arduino jest zajęte kalibracją (trwa min. 2,5 s)
const CALIB_MAX_MS    = 6000;   // kalibracja kończy się najpóźniej po ok. 4,6 s
const CAM_TIMEOUT_MS  = 8000;   // limit czasu żądań do kamery (budzenie trwa ok. 0,5 s)
const PULSE_MS        = 500;    // czas animacji znacznika ramki
const NO_ECHO         = '—';    // odległość, gdy sonar nie odebrał echa
// HC-SR04: efektywna szerokość wiązki — każdy pomiar rysowany jest jako łuk o tej szerokości
const BEAM_DEG    = 15;     // stożek ±7,5°

const $  = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];
const MONO = getComputedStyle(document.documentElement).getPropertyValue('--mono');
const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
const rad = d => d * Math.PI / 180;
const p2 = n => String(n).padStart(2, '0');
const hms = (d = new Date()) => d.toLocaleTimeString('pl-PL', { hour12: false });
const hmsms = (d = new Date()) => hms(d) + '.' + String(d.getMilliseconds()).padStart(3, '0');

/* =====================================================================
   TRANSPORT — WebSocket do ESP32-CAM
   ===================================================================== */
const Transport = {
  ws: null, lastRx: 0,
  dataOk: null,   // null = jeszcze nie wiadomo, true = dane docierają, false = dane ustały
  calibrating: false, calibTimer: 0,
  queue: [],      // komendy wstrzymane na czas kalibracji
  get open() { return !!this.ws && this.ws.readyState === WebSocket.OPEN; },
  connect() {
    setConnection('connecting', { host: HOST, uart: '—' });
    const ws = this.ws = new WebSocket(WS_URL);
    ws.onopen = () => {
      this.lastRx = performance.now();
      setConnection('connecting', { sub: 'Oczekiwanie na dane z pojazdu' });
      // radar na Arduino pozostaje włączony po rozłączeniu, więc ustawiamy stan zgodny ze stroną
      this.send(scanOn ? radarCmd() : 'W:RADAROFF');
    };
    ws.onmessage = e => this.receive(String(e.data));
    ws.onclose = () => {
      this.dataOk = null;
      this.finishCalibration();
      setConnection('offline', { uart: '—' });
      held.length = 0;
      if (activeDir !== 'S') showDrive('S');   // Arduino samo zatrzymuje silniki po rozłączeniu
      setTimeout(() => this.connect(), WS_RETRY_MS);
    };
  },
  // log = false dla komend powtarzanych cyklicznie, aby nie zalewały logu
  send(cmd, log = true) {
    if (this.open) {
      if (this.calibrating) this.queue.push(cmd);
      else this.ws.send(cmd);
    }
    if (log) Dashboard.logSent(this.open ? cmd : cmd + ' (brak połączenia)');
  },
  // Arduino nie potwierdza kalibracji: blokuje pętlę, a komendy odebrane w tym czasie kasuje,
  // więc są wstrzymywane do chwili, gdy po przerwie znów zaczną przychodzić dane
  calibrate() {
    held.length = 0;
    drive('S');   // inaczej powtarzanie W:MOTOR wznowiłoby jazdę po kalibracji
    this.send('W:CALIBRATE');
    if (!this.open) return;
    this.calibrating = true;
    this.lastRx = performance.now();
    this.calibTimer = setTimeout(() => this.finishCalibration(), CALIB_MAX_MS);
    showCalibration(true);
    if (this.dataOk) setConnection('online', { sub: 'Kalibracja żyroskopu — nie ruszaj pojazdu' });
  },
  finishCalibration() {
    if (!this.calibrating) return;
    this.calibrating = false;
    clearTimeout(this.calibTimer);
    showCalibration(false);
    const cmds = this.queue;
    this.queue = [];
    if (!this.open) return;
    if (this.dataOk) setConnection('online');
    this.ws.send('W:');   // Arduino po kalibracji pomija pierwszą linię; tę i tak by zignorowało
    cmds.forEach(c => this.ws.send(c));
  },
  receive(line) {
    const now = performance.now(), gap = now - this.lastRx;
    this.lastRx = now;
    if (this.calibrating && gap >= CALIB_GAP_MS) this.finishCalibration();
    if (this.dataOk !== true) {
      this.dataOk = true;
      setConnection('online', { uart: 'OK' });
    }
    Dashboard.logReceived(line);
    const frame = parseLine(line);
    if (frame) updateTelemetry(frame);
  },
  checkData() {
    if (!this.open || this.calibrating || this.dataOk === false || performance.now() - this.lastRx < DATA_TIMEOUT_MS) return;
    this.dataOk = false;
    setConnection('connecting', { sub: 'Brak danych z Arduino', uart: 'BRAK' });
  }
};

/* =====================================================================
   LOGI DEBUGOWANIA
   ===================================================================== */
function pushLog(kind, text) {
  const ul = $('#log-' + kind);
  const li = document.createElement('li');
  const t = document.createElement('time'); t.textContent = hmsms();
  const s = document.createElement('span'); s.textContent = text; s.title = text;
  li.append(t, s);
  ul.append(li);
  while (ul.children.length > LOG_MAX) ul.firstChild.remove();
  ul.scrollTop = ul.scrollHeight;   // automatyczne przewijanie na dół
}
$$('[data-clear]').forEach(b => b.onclick = () => {
  $('#log-' + b.dataset.clear).replaceChildren();
});

/* =====================================================================
   STATUS POŁĄCZENIA
   ===================================================================== */
const CONN = {
  offline:    ['ROZŁĄCZONO', 'Brak połączenia z pojazdem'],
  connecting: ['ŁĄCZENIE…',  'Nawiązywanie połączenia'],
  online:     ['POŁĄCZONO',  'Pojazd odpowiada'],
};
function setConnection(state, info = {}) {
  const [label, sub] = CONN[state];
  $('#conn').dataset.state = state;
  $('#conn-label').textContent = label;
  $('#conn-sub').textContent = info.sub || sub;
  for (const k of ['host', 'uart']) if (k in info) $(`[data-s=${k}]`).textContent = info[k];
}

/* =====================================================================
   TELEMETRIA
   ramka: { raw, encL, encR, speed, ax, ay, az, gx, gy, gz, temp,
            servo, sonar, x, y, theta } — każda wiadomość A: niesie tylko część pól
   ===================================================================== */
const T_ELS = $$('[data-t]');
const FRAMES = {
  ENC:  [2, ([encL, encR]) => ({ encL, encR })],
  DIST: [2, ([servo, mm]) => ({ servo, sonar: mm ? mm / 10 : NO_ECHO })],   // mm = 0: brak echa
  MPU:  [7, ([ax, ay, az, gx, gy, gz, temp]) => ({
    ax: ax / 1000, ay: ay / 1000, az: az / 1000,
    gx: gx / 100, gy: gy / 100, gz: gz / 100, temp: temp / 100,
  })],
  POS:  [4, ([x, y, theta, v]) => ({ x: x / 10, y: y / 10, theta: theta / 100, speed: v / 10 })],
};
// zamienia linię A:<typ>:<liczby> na ramkę telemetrii; zwraca null dla innych lub uszkodzonych linii
function parseLine(line) {
  const m = /^A:([A-Z]+):(-?\d+(?:,-?\d+)*)$/.exec(line);
  const def = m && FRAMES[m[1]];
  if (!def) return null;
  const vals = m[2].split(',').map(Number);
  return vals.length === def[0] ? { raw: line, ...def[1](vals) } : null;
}

let lastPulse = -PULSE_MS;
function updateTelemetry(f) {
  const d = { ...f };

  for (const el of T_ELS) {
    const k = el.dataset.t; if (d[k] == null) continue;
    const v = d[k];
    el.textContent = typeof v === 'number' ? v.toFixed(+(el.dataset.d || 0)) : String(v);
  }

  if (d.raw != null) $('#frame-raw').textContent = d.raw;
  const now = performance.now();
  if (now - lastPulse >= PULSE_MS) {   // wiadomości przychodzą częściej, niż trwa animacja
    lastPulse = now;
    const p = $('#frame-pulse'); p.classList.remove('hit'); void p.offsetWidth; p.classList.add('hit');
  }

  // θ rośnie przy obrocie w lewo, a dodatni kąt w SVG obraca w prawo
  if (d.theta != null) $('#compass-needle').setAttribute('transform', `rotate(${-d.theta} 15 15)`);
  const hasDist = typeof d.sonar === 'number';
  if (d.sonar != null) {
    $('#prox-bar').style.width = hasDist ? clamp(100 - d.sonar / 2, 3, 100) + '%' : '0';
  }
  if (d.servo != null) {
    if (!servoDragging && now >= servoHoldUntil) setServoUI(d.servo);   // jeden kąt serwa: zadany = rzeczywisty
    radar.setAngle(d.servo);
    if (d.sonar != null) {
      if (hasDist) radar.addPoint(d.servo, d.sonar);
      else radar.pts.delete(Math.round(d.servo));   // brak echa: stary pomiar jest nieaktualny
      $('#r-cur').textContent = hasDist ? d.sonar.toFixed(0) : NO_ECHO;
      $('#r-cur-a').textContent = Math.round(d.servo);
    }
  }
}

// zegar przy ostatnim zdjęciu — obecna godzina
const tickClock = () => $('#snap-time').textContent = hms();
tickClock(); setInterval(tickClock, 1000);

/* =====================================================================
   TRYBY (zakładki)
   ===================================================================== */
let mode = 'control';
function setMode(m) {
  mode = m;
  $$('.tab').forEach(t => t.classList.toggle('active', t.dataset.mode === m));
  $$('.mode').forEach(s => s.classList.toggle('active', s.id === 'mode-' + m));
  if (m === 'radar') radar.resize();
  else if (scanOn) { setScan(false); Transport.send('W:RADAROFF'); }   // wyjście z radaru zatrzymuje skan
}
$$('.tab').forEach(t => t.onclick = () => setMode(t.dataset.mode));

/* =====================================================================
   STEROWANIE: suwaki prędkości kół
   ===================================================================== */
for (const id of ['#s-left', '#s-right']) {
  const s = $(id); s.min = SPEED_MIN; s.max = SPEED_MAX;
  // prędkość jest częścią W:MOTOR, więc podczas postoju nie ma czego wysyłać
  s.addEventListener('change', () => { if (activeDir !== 'S') Transport.send(motorCmd(activeDir)); });
}
$('#s-servo').min = SERVO_MIN; $('#s-servo').max = SERVO_MAX;
function paintRange(el) { el.style.setProperty('--p', (el.value - el.min) / (el.max - el.min) * 100 + '%'); }
$$('input[type=range]').forEach(el => {
  paintRange(el);
  el.addEventListener('input', () => { paintRange(el); if (el.dataset.out) $('#' + el.dataset.out).textContent = el.value; });
});
const PRESETS = { min: SPEED_MIN, mid: Math.round((SPEED_MIN + SPEED_MAX) / 2), max: SPEED_MAX };
$('#spd-range').textContent = `${SPEED_MIN} – ${SPEED_MAX}`;
$$('[data-preset]').forEach(b => b.onclick = () => {
  for (const id of ['#s-left', '#s-right']) {
    const s = $(id); s.value = PRESETS[b.dataset.preset]; s.dispatchEvent(new Event('input')); s.dispatchEvent(new Event('change'));
  }
});

/* =====================================================================
   STEROWANIE: kierunki (tryb przytrzymaj / przełącz)
   ===================================================================== */
const DIR_LABEL = { F:'PRZÓD', B:'TYŁ', L:'LEWO', R:'PRAWO', S:'STOP' };
const DIR_SIGN  = { F:[1,1], B:[-1,-1], L:[-1,1], R:[1,-1], S:[0,0] };   // znaki PWM kół: lewe, prawe
let activeDir = 'S', driveMode = 'hold';
function motorCmd(dir) {
  const [l, r] = DIR_SIGN[dir];
  return `W:MOTOR:${l * $('#s-left').value},${r * $('#s-right').value}`;
}
function drive(dir, force = false) {
  if (dir === activeDir && !force) return;
  if (Transport.calibrating && dir !== 'S') return;   // pojazd musi stać podczas kalibracji
  showDrive(dir);
  Transport.send(motorCmd(dir));
}
function showDrive(dir) {
  activeDir = dir;
  $('#drive-dir').textContent = DIR_LABEL[dir];
  $('#drive-dir').classList.toggle('moving', dir !== 'S');
  $$('.pbtn').forEach(b => b.classList.toggle('pressed', b.dataset.dir === dir && dir !== 'S'));
  if (dir === 'S') { const s = $('.pbtn.stop'); s.classList.add('pressed'); setTimeout(() => s.classList.remove('pressed'), 150); }
}
function setDriveMode(m) {
  driveMode = m;
  $$('#drive-mode button').forEach(b => b.classList.toggle('on', b.dataset.dm === m));
  $('#drive-hint').textContent = m === 'hold'
    ? 'przytrzymaj kierunek · W A S D / strzałki · spacja = STOP'
    : 'kliknij raz, aby jechać · ponowne kliknięcie lub STOP zatrzymuje';
  held.length = 0;
  if (activeDir !== 'S') drive('S');   // zmiana trybu = bezpieczne zatrzymanie
}
$$('#drive-mode button').forEach(b => b.onclick = () => setDriveMode(b.dataset.dm));

$$('.pbtn').forEach(b => {
  const dir = b.dataset.dir;
  b.addEventListener('pointerdown', e => {
    e.preventDefault();
    if (driveMode === 'toggle') { drive(dir !== 'S' && dir === activeDir ? 'S' : dir, dir === 'S'); return; }
    b.setPointerCapture(e.pointerId); drive(dir, dir === 'S');
  });
  const release = () => { if (driveMode === 'hold' && dir !== 'S' && activeDir === dir) drive('S'); };
  b.addEventListener('pointerup', release);
  b.addEventListener('pointercancel', release);
  b.addEventListener('contextmenu', e => e.preventDefault());
});

const KEYMAP = { KeyW:'F', ArrowUp:'F', KeyS:'B', ArrowDown:'B', KeyA:'L', ArrowLeft:'L', KeyD:'R', ArrowRight:'R' };
const held = [];
addEventListener('keydown', e => {
  if (e.target.closest('input,select,textarea')) return;
  if (['Digit1','Digit2','Digit3'].includes(e.code)) { setMode(['control','radar','map'][+e.code.slice(-1) - 1]); return; }
  if (mode !== 'control') return;
  if (e.code === 'Space') { e.preventDefault(); held.length = 0; drive('S', true); return; }
  const d = KEYMAP[e.code]; if (!d) return;
  e.preventDefault(); if (e.repeat) return;
  if (driveMode === 'toggle') { drive(d === activeDir ? 'S' : d); return; }
  if (!held.includes(d)) held.push(d);
  drive(d);
});
addEventListener('keyup', e => {
  const d = KEYMAP[e.code]; if (!d || driveMode !== 'hold') return;
  const i = held.indexOf(d); if (i >= 0) held.splice(i, 1);
  if (mode === 'control') drive(held.at(-1) || 'S');
});
addEventListener('blur', () => { if (driveMode === 'hold') { held.length = 0; drive('S'); } });
// przeglądarka spowalnia timery ukrytej karty, więc nie da się w niej utrzymać jazdy
document.addEventListener('visibilitychange', () => { if (document.hidden) { held.length = 0; drive('S'); } });
setInterval(() => { if (activeDir !== 'S') Transport.send(motorCmd(activeDir), false); }, MOTOR_REPEAT_MS);

/* =====================================================================
   STEROWANIE: serwo  (180° = lewo, 90° = przód, 0° = prawo)
   ===================================================================== */
function gaugeXY(deg, len) { const v = rad(deg); return [100 + len * Math.cos(v), 100 - len * Math.sin(v)]; }
(function ticks() {
  let s = '';
  for (let d = SERVO_MIN; d <= SERVO_MAX; d += 15) {
    const [x1, y1] = gaugeXY(d, 66), [x2, y2] = gaugeXY(d, d % 45 ? 70 : 74);
    s += `<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${d % 45 ? '#3b4458' : '#6f7b94'}" stroke-width="1.5"/>`;
  }
  $('#g-ticks').innerHTML = s;
  const [x0, y0] = gaugeXY(SERVO_MAX, 80), [x1, y1] = gaugeXY(SERVO_MIN, 80);
  $('#g-track').setAttribute('d', `M${x0.toFixed(2)} ${y0.toFixed(2)} A80 80 0 0 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`);
  for (const [id, deg, x, y] of [['#g-max', SERVO_MAX, x0, y0], ['#g-min', SERVO_MIN, x1, y1]]) {
    const t = $(id); t.textContent = deg + '°'; t.setAttribute('x', x.toFixed(2)); t.setAttribute('y', (y + 17).toFixed(2));
  }
})();
function setServoUI(deg) {
  deg = clamp(Math.round(deg), SERVO_MIN, SERVO_MAX);
  const s = $('#s-servo'); s.value = deg; paintRange(s);
  $('#servo-set').textContent = deg;
  const [nx, ny] = gaugeXY(deg, 72), n = $('#g-set'); n.setAttribute('x2', nx); n.setAttribute('y2', ny);
  const [x, y] = gaugeXY(deg, 80), [x0, y0] = gaugeXY(SERVO_MIN, 80);
  $('#g-arc').setAttribute('d', deg === SERVO_MIN ? '' : `M${x0.toFixed(2)} ${y0.toFixed(2)} A80 80 0 0 0 ${x.toFixed(2)} ${y.toFixed(2)}`);
  return deg;
}
let servoDragging = false, servoHoldUntil = 0;
function sendServo(deg) {
  servoHoldUntil = performance.now() + SERVO_HOLD_MS;
  Transport.send('W:SERVO:' + deg);
}
$('#s-servo').addEventListener('pointerdown', () => servoDragging = true);
addEventListener('pointerup', () => servoDragging = false);
$('#s-servo').addEventListener('input', e => setServoUI(+e.target.value));
$('#s-servo').addEventListener('change', e => { servoDragging = false; sendServo(e.target.value); });
$$('[data-servo]').forEach(b => b.onclick = () => {
  const v = b.dataset.servo, cur = +$('#s-servo').value;
  sendServo(setServoUI(v === 'center' ? 90 : cur + +v));
});
let scanOn = false;
function setScan(on) {
  scanOn = on;
  const rb = $('#radar-scan'); rb.classList.toggle('on', on); rb.textContent = on ? '■ Stop skanu' : '▶ Start skanu';
}
const radarCmd = () => `W:RADAR:${radar.from},${radar.to},${$('#radar-step').value},${scanInterval}`;
const toggleScan = () => { setScan(!scanOn); Transport.send(scanOn ? radarCmd() : 'W:RADAROFF'); };
// Arduino przyjmuje ustawienia tylko razem z uruchomieniem radaru, więc zmiana w trakcie skanu go restartuje
const sendScanConfig = () => { if (scanOn) Transport.send(radarCmd()); };
setServoUI(90);

/* =====================================================================
   RADAR — każdy pomiar rysowany jako łuk o szerokości stożka wiązki
   (180° = lewo, 0° = prawo; skan w zakresie od–do, wykres w zakresie ruchu serwa)
   ===================================================================== */
const RADAR_TICKS = [SERVO_MIN, 30, 60, 90, 120, 150, SERVO_MAX];
const radar = {
  cv: $('#radar'), ctx: $('#radar').getContext('2d'),
  w: 0, h: 0, range: 200, angle: 90, dir: 1, from: SERVO_MIN, to: SERVO_MAX, pts: new Map(),
  resize() {
    const r = this.cv.getBoundingClientRect(), dpr = devicePixelRatio || 1;
    this.w = r.width; this.h = r.height;
    this.cv.width = r.width * dpr; this.cv.height = r.height * dpr;
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  },
  setAngle(a) { if (a !== this.angle) this.dir = a > this.angle ? 1 : -1; this.angle = a; },
  addPoint(a, d) { this.pts.set(Math.round(a), { d, t: performance.now() }); },
  draw() {
    const { ctx: c, w, h } = this; if (!w || !h) return;
    c.clearRect(0, 0, w, h);
    const pad = 36, cx = w / 2, cy = h - 24, R = Math.max(20, Math.min(w / 2 - pad, h - pad - 24));
    const V = a => a;                                             // kąt serwa → kąt na ekranie (0° po prawej)
    const P = (a, r) => [cx + r * Math.cos(rad(V(a))), cy - r * Math.sin(rad(V(a)))];
    const sector = (a1, a2, r1, r2) => {                          // wycinek pierścienia między kątami serwa a1<a2
      const lo = rad(V(clamp(a1, SERVO_MIN, SERVO_MAX))), hi = rad(V(clamp(a2, SERVO_MIN, SERVO_MAX)));
      c.beginPath(); c.arc(cx, cy, r2, -hi, -lo); c.arc(cx, cy, Math.max(0, r1), -lo, -hi, true); c.closePath();
    };

    // tło + siatka
    sector(SERVO_MIN, SERVO_MAX, 0, R);
    const g = c.createRadialGradient(cx, cy, 0, cx, cy, R);
    g.addColorStop(0, 'rgba(0,229,255,.10)'); g.addColorStop(1, 'rgba(0,229,255,.015)');
    c.fillStyle = g; c.fill();
    c.lineWidth = 1; c.font = `10px ${MONO}`;
    for (let i = 1; i <= 4; i++) {
      const r = R * i / 4;
      c.strokeStyle = i === 4 ? 'rgba(0,229,255,.45)' : 'rgba(0,229,255,.16)';
      c.beginPath(); c.arc(cx, cy, r, -rad(SERVO_MAX), -rad(SERVO_MIN)); c.stroke();
      c.fillStyle = 'rgba(0,229,255,.55)'; c.textAlign = 'left';
      c.fillText(Math.round(this.range * i / 4) + ' cm', cx + 4, cy - r + 12);
    }
    c.textAlign = 'center';
    for (const deg of RADAR_TICKS) {
      const [x, y] = P(deg, R), [lx, ly] = P(deg, R + 16);
      c.strokeStyle = 'rgba(0,229,255,.16)'; c.beginPath(); c.moveTo(cx, cy); c.lineTo(x, y); c.stroke();
      c.fillStyle = '#6f7b94'; c.fillText(deg + '°', lx, ly + 4);
    }

    // zakres skanu (od–do): przyciemnienie poza zakresem + linie graniczne
    c.fillStyle = 'rgba(6,7,11,.55)';
    if (this.from > SERVO_MIN) { sector(SERVO_MIN, this.from, 0, R); c.fill(); }
    if (this.to < SERVO_MAX)   { sector(this.to, SERVO_MAX, 0, R); c.fill(); }
    c.save(); c.strokeStyle = '#ff2e97'; c.lineWidth = 1.5; c.setLineDash([5, 4]); c.shadowColor = '#ff2e97'; c.shadowBlur = 6;
    for (const a of [this.from, this.to]) { const [lx, ly] = P(a, R); c.beginPath(); c.moveTo(cx, cy); c.lineTo(lx, ly); c.stroke(); }
    c.restore();

    // aktualny stożek wiązki (±BEAM_DEG/2) + oś wiązki
    const half = BEAM_DEG / 2;
    sector(this.angle - half, this.angle + half, 0, R);
    const cg = c.createRadialGradient(cx, cy, 0, cx, cy, R);
    cg.addColorStop(0, 'rgba(0,229,255,.28)'); cg.addColorStop(1, 'rgba(0,229,255,.04)');
    c.fillStyle = cg; c.fill();
    c.strokeStyle = 'rgba(0,229,255,.35)'; c.setLineDash([3, 4]); c.stroke(); c.setLineDash([]);
    const [bx, by] = P(this.angle, R);
    c.save(); c.strokeStyle = '#00e5ff'; c.lineWidth = 2; c.shadowColor = '#00e5ff'; c.shadowBlur = 14;
    c.beginPath(); c.moveTo(cx, cy); c.lineTo(bx, by); c.stroke(); c.restore();

    // pomiary: łuk o szerokości wiązki + punkt środkowy (bez łączenia punktów liniami)
    const now = performance.now();
    let near = null;
    for (const [a, p] of this.pts) {
      if (p.d > this.range) continue;
      if (!near || p.d < near.d) near = { a, d: p.d };
      const r = R * p.d / this.range, [x, y] = P(a, r);
      const alpha = clamp(1 - (now - p.t) / 15000, .25, 1);
      const col = '180,255,31';   // pomiary: zielone
      c.strokeStyle = `rgba(${col},${.45 * alpha})`; c.lineWidth = 2; c.lineCap = 'round';
      c.beginPath(); c.arc(cx, cy, r, -rad(V(clamp(a + half, SERVO_MIN, SERVO_MAX))), -rad(V(clamp(a - half, SERVO_MIN, SERVO_MAX)))); c.stroke();
      c.fillStyle = `rgba(${col},${alpha})`; c.shadowColor = `rgb(${col})`; c.shadowBlur = 6 * alpha;
      c.beginPath(); c.arc(x, y, 2.2, 0, Math.PI * 2); c.fill();
      c.shadowBlur = 0;
    }
    c.shadowBlur = 0;

    // pojazd
    c.fillStyle = '#ff2e97'; c.shadowColor = '#ff2e97'; c.shadowBlur = 12;
    c.beginPath(); c.moveTo(cx, cy - 12); c.lineTo(cx + 8, cy + 4); c.lineTo(cx - 8, cy + 4); c.closePath(); c.fill();
    c.shadowBlur = 0;

    $('#r-near').textContent = near ? near.d.toFixed(0) : '—';
    $('#r-near-a').textContent = near ? near.a : '—';
  }
};
$('#unc-legend').textContent = `łuk = szerokość wiązki ±${BEAM_DEG / 2}°`;
new ResizeObserver(() => { if (mode === 'radar') radar.resize(); }).observe($('#radar-wrap'));
(function loop() { if (mode === 'radar') radar.draw(); requestAnimationFrame(loop); })();
let scanInterval = 200;
$('#radar-range').addEventListener('input', e => radar.range = +e.target.value);
$('#radar-step').onchange  = sendScanConfig;
$('#radar-int').onchange   = e => { scanInterval = +e.target.value; sendScanConfig(); };
$('#radar-clear').onclick  = () => radar.pts.clear();
// suwak z dwoma punktami: od–do (180° po lewej, 0° po prawej — jak na wizualizacji)
const rFrom = $('#radar-from'), rTo = $('#radar-to');
for (const r of [rFrom, rTo]) { r.min = SERVO_MIN; r.max = SERVO_MAX; }
rFrom.value = SERVO_MIN; rTo.value = SERVO_MAX;
function paintDual(e) {
  let f = +rFrom.value, t = +rTo.value;
  if (f > t) { if (e && e.target === rFrom) rFrom.value = f = t; else rTo.value = t = f; }
  radar.from = f; radar.to = t;
  $('#o-from').textContent = f; $('#o-to').textContent = t;
  const span = (SERVO_MAX - SERVO_MIN) / 100;
  const fill = $('#dual-fill'); fill.style.left = (SERVO_MAX - t) / span + '%'; fill.style.right = (f - SERVO_MIN) / span + '%';
  rFrom.style.zIndex = f > SERVO_MAX - 10 ? 3 : 1;    // żeby nie zablokować uchwytów na skraju
}
rFrom.addEventListener('input', paintDual); rTo.addEventListener('input', paintDual);
rFrom.addEventListener('change', sendScanConfig); rTo.addEventListener('change', sendScanConfig);
paintDual();
$('#radar-scan').onclick   = toggleScan;

/* =====================================================================
   LCD (jedna linia)
   ===================================================================== */
const lcdRow = $('#lcd-row'), lcdIn = $('#lcd-text');
$('#lcd-size').textContent = `${LCD_COLS} znaków`;
lcdRow.style.gridTemplateColumns = `repeat(${LCD_COLS},1fr)`;
for (let i = 0; i < LCD_COLS; i++) lcdRow.appendChild(document.createElement('span'));
lcdIn.maxLength = LCD_COLS;
function renderLcd() {
  const s = lcdIn.value.padEnd(LCD_COLS).slice(0, LCD_COLS);
  [...lcdRow.children].forEach((sp, i) => sp.textContent = s[i] === ' ' ? ' ' : s[i]);
  const cc = $('#lcd-cc'), bad = /[^\x20-\x7e]/.test(lcdIn.value);
  cc.textContent = bad ? '!ASCII' : `${lcdIn.value.length}/${LCD_COLS}`;
  cc.classList.toggle('warn', bad);
  cc.title = bad ? 'LCD (HD44780) nie wyświetli polskich znaków' : '';
}
lcdIn.addEventListener('input', renderLcd);
lcdIn.addEventListener('keydown', e => { if (e.key === 'Enter') $('#lcd-send').click(); });
// ESP32 odrzuca komendy ze znakami spoza ASCII — ostrzeżenie pokazuje renderLcd()
$('#lcd-send').onclick = () => { if (!/[^\x20-\x7e]/.test(lcdIn.value)) Transport.send('W:MSG:' + lcdIn.value); };
$('#lcd-clear').onclick = () => { lcdIn.value = ''; renderLcd(); Transport.send('W:MSG:'); };
renderLcd();

/* =====================================================================
   ODOMETRIA
   ===================================================================== */
function showCalibration(on) {
  $('#odo-reset').disabled = $('#odo-calib').disabled = on;
  $('#odo-calib').textContent = on ? 'Kalibracja…' : 'Kalibracja';
}
$('#odo-reset').onclick = () => Transport.send('W:RESETPOS');
$('#odo-calib').onclick = () => Transport.calibrate();

/* =====================================================================
   KAMERA
   ===================================================================== */
let camOn = false;
function camFetch(path) {
  Dashboard.logSent('GET ' + path);
  return fetch(CAM_URL + path, { cache: 'no-store', signal: AbortSignal.timeout(CAM_TIMEOUT_MS) }).then(r => {
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return r;
  });
}
function setCamera(on) {
  camOn = on;
  $('#cam').dataset.state = on ? 'on' : 'off';
  $('#cam-toggle').textContent = on ? '⏻ Wyłącz' : '⏻ Włącz';
  const tag = $('#cam-tag'); tag.textContent = on ? 'LIVE' : 'WYŁ.'; tag.classList.toggle('hot', on);
  const img = $('#cam-stream'); img.hidden = !on;
  if (on) img.src = `${CAM_URL}/stream?t=${Date.now()}`;   // parametr wymusza nowe połączenie
  else img.removeAttribute('src');
}
$('#cam-toggle').onclick = async () => {
  const btn = $('#cam-toggle'), on = !camOn;
  btn.disabled = true;
  try {
    if (!on) setCamera(false);
    await camFetch(on ? '/stream/start' : '/stream/stop');
    if (on) setCamera(true);   // /stream działa dopiero po /stream/start
  } catch (err) {
    Dashboard.logReceived('Kamera: ' + err.message);
  }
  btn.disabled = false;
};
$('#cam-stream').onerror = () => {
  if (!camOn) return;
  setCamera(false);
  Dashboard.logReceived('Kamera: transmisja przerwana');
};

// Ostatnie zdjęcie — wyświetlane jest tylko jedno, i tylko ono jest do pobrania
let snapUrl = null;
function setSnapshot(url, date = new Date()) {
  if (snapUrl && snapUrl.startsWith('blob:')) URL.revokeObjectURL(snapUrl);
  snapUrl = url;
  const shot = $('#shot');
  $('#snap-img').src = url;
  shot.classList.add('has');
  const dl = $('#snap-dl');
  dl.href = url;
  dl.download = `zdjecie_${date.getFullYear()}${p2(date.getMonth() + 1)}${p2(date.getDate())}_${p2(date.getHours())}${p2(date.getMinutes())}${p2(date.getSeconds())}.jpg`;
  dl.classList.remove('disabled');
}
$('#cam-snap').onclick = async () => {
  const btn = $('#cam-snap');
  btn.disabled = true;
  try {
    const blob = await (await camFetch('/capture')).blob();
    setSnapshot(URL.createObjectURL(blob));
    Dashboard.logReceived(`/capture: ${Math.round(blob.size / 1024)} kB`);
  } catch (err) {
    Dashboard.logReceived('Kamera: ' + err.message);
  }
  btn.disabled = false;
};

/* =====================================================================
   PUBLICZNE API
   ===================================================================== */
window.Dashboard = {
  updateTelemetry,
  setConnection,
  logSent:     t => pushLog('sent', t),
  logReceived: t => pushLog('recv', t),
  setCamera,
  setSnapshot,
  setMode,
  radar,
};

/* =====================================================================
   START
   ===================================================================== */
Transport.connect();
setInterval(() => Transport.checkData(), 500);
</script>
</body>
</html>
)rawliteral";

esp_err_t pageHandler(httpd_req_t* req) {
  cameraSetCommonHeaders(req);
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, PAGE_HTML, sizeof(PAGE_HTML) - 1); // bez kończącego '\0'
}
