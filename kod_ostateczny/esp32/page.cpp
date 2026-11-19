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
      .lcd{display:grid;gap:2px;background:#041217;border:2px solid #0d2c36;border-radius:6px;padding:8px;margin:6px 0 8px;box-shadow:inset 0 0 18px rgba(0,229,255,.08)}
      .lcd-row{display:grid;gap:2px}
      .lcd-row span{background:rgba(0,229,255,.06);font-family:var(--mono);font-size:13px;text-align:center;line-height:20px;color:var(--cyan);
        text-shadow:0 0 6px rgba(0,229,255,.8);border-radius:1px;min-width:0}
      .lcd-in{display:grid;grid-template-columns:1fr auto;gap:6px;align-items:center}
      .lcd-in+.lcd-in{margin-top:6px}
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
      #mode-map{grid-template-columns:minmax(0,1fr) 250px;grid-template-rows:minmax(0,1fr)}
      #mode-map .card-h{color:var(--lime)}
      .map-wrap{background:var(--bg)}
      #map{cursor:grab;touch-action:none}
      #map.drag{cursor:grabbing}
      .map-legend{background:rgba(6,7,11,.82);border-radius:5px;padding:5px 8px}
      .map-legend span::before{border-radius:1px;box-shadow:none;outline:1px solid var(--dim)}
      .map-legend span.wall::before{width:12px;height:0;background:none;outline:0;border-top:3px solid var(--c);vertical-align:3px}
      .map-legend span.wall.cur::before{border-top-style:dashed}
      .map-view{display:grid;grid-template-columns:auto auto 1fr 1fr;gap:6px;flex-shrink:0}
      .map-view .btn,.map-btns .btn{--c:var(--lime)}
      #map-follow.on{color:var(--lime);border-color:var(--lime)}
      #map-run{--c:var(--lime)}
      #map-run.on{--c:var(--red)}
      .map-btns{display:grid;grid-template-columns:1fr 1fr;gap:6px}
      .map-l{display:grid;grid-template-columns:minmax(0,1fr) 66px;gap:8px;align-items:center}
      .map-l input[type=range]{--c:var(--lime)}
      .map-l input[type=number]{width:100%;padding:4px 6px}
      .map-l input[type=number]:focus{border-color:var(--lime);box-shadow:0 0 0 2px rgba(180,255,31,.15)}
      .map-msg{font-size:10px;line-height:1.4;color:var(--muted)}
      .map-msg.err{color:var(--red)}

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
        #mode-radar,#mode-map{grid-template-columns:minmax(0,1fr);grid-template-rows:420px auto}
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
        #mode-radar,#mode-map{grid-template-rows:auto auto}
        .radar-main .radar-wrap{flex:none;height:auto;aspect-ratio:1.4}
        .map-main .radar-wrap{aspect-ratio:1}
        .map-view{grid-template-columns:1fr 1fr}
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
            <div class="radar-main map-main">
              <div class="card radar-wrap map-wrap" id="map-wrap">
                <canvas id="map"></canvas>
                <div class="radar-legend map-legend">
                  <span style="--c:#fff">wolna</span>
                  <span style="--c:#808080">nieznana</span>
                  <span style="--c:#000">zajęta</span>
                  <span style="--c:var(--magenta)">pojazd</span>
                  <span class="wall" style="--c:var(--cyan)">ściana</span>
                  <span class="wall cur" style="--c:var(--amber)">bieżąca ściana</span>
                </div>
              </div>
              <div class="map-view">
                <button class="btn sm" id="map-zoom-out" title="Oddal" aria-label="Oddal">−</button>
                <button class="btn sm" id="map-zoom-in" title="Przybliż" aria-label="Przybliż">+</button>
                <button class="btn sm" id="map-fit">Cała mapa</button>
                <button class="btn sm" id="map-follow">Śledź pojazd</button>
              </div>
            </div>
            <div class="radar-side">
              <div class="card">
                <div class="card-h">Mapowanie</div>
                <button class="btn solid" id="map-run">▶ Start mapowania</button>
                <div class="map-btns">
                  <button class="btn sm" id="map-clear">Wyczyść</button>
                  <button class="btn sm" id="map-save">Zapisz PNG</button>
                </div>
                <div class="map-msg" id="map-status" role="status">Wykrywanie ściany: gotowe do startu.</div>
              </div>
              <div class="card">
                <div class="card-h">Prędkości</div>
                <div class="ctl">
                  <div class="ctl-top"><span>Jazda</span><b>PWM</b></div>
                  <div class="map-l">
                    <input type="range" data-speed="drive" step="1" aria-label="Prędkość jazdy">
                    <input type="number" data-speed="drive" step="1" aria-label="Prędkość jazdy">
                  </div>
                </div>
                <div class="ctl">
                  <div class="ctl-top"><span>Obrót</span><b>PWM</b></div>
                  <div class="map-l">
                    <input type="range" data-speed="turn" step="1" aria-label="Prędkość obrotu">
                    <input type="number" data-speed="turn" step="1" aria-label="Prędkość obrotu">
                  </div>
                </div>
                <div class="ctl">
                  <div class="ctl-top"><span>Obrót dokładny</span><b>PWM</b></div>
                  <div class="map-l">
                    <input type="range" data-speed="turnSlow" step="1" aria-label="Prędkość obrotu dokładnego">
                    <input type="number" data-speed="turnSlow" step="1" aria-label="Prędkość obrotu dokładnego">
                  </div>
                </div>
                <button class="btn sm" id="map-speed-default" style="--c:var(--lime)">Wartości domyślne</button>
              </div>
              <div class="card">
                <div class="card-h">Parametry l</div>
                <div class="ctl">
                  <div class="ctl-top"><span>l<sub>occ</sub> · zajęta</span><b data-p="lOcc"></b></div>
                  <div class="map-l">
                    <input type="range" data-l="lOcc" min="0.05" max="3" step="0.05" aria-label="l occ">
                    <input type="number" data-l="lOcc" min="0.05" max="3" step="0.05" aria-label="l occ">
                  </div>
                </div>
                <div class="ctl">
                  <div class="ctl-top"><span>l<sub>free</sub> · wolna</span><b data-p="lFree"></b></div>
                  <div class="map-l">
                    <input type="range" data-l="lFree" min="-3" max="-0.05" step="0.05" aria-label="l free">
                    <input type="number" data-l="lFree" min="-3" max="-0.05" step="0.05" aria-label="l free">
                  </div>
                </div>
                <div class="ctl">
                  <div class="ctl-top"><span>l<sub>0</sub> · początkowa</span><b data-p="l0"></b></div>
                  <div class="map-l">
                    <input type="range" data-l="l0" min="-1" max="1" step="0.05" aria-label="l 0">
                    <input type="number" data-l="l0" min="-1" max="1" step="0.05" aria-label="l 0">
                  </div>
                </div>
                <div class="ctl">
                  <div class="ctl-top"><span>l<sub>min</sub> · dolna granica</span><b data-p="lMin"></b></div>
                  <div class="map-l">
                    <input type="range" data-l="lMin" min="-6.35" max="-0.05" step="0.05" aria-label="l min">
                    <input type="number" data-l="lMin" min="-6.35" max="-0.05" step="0.05" aria-label="l min">
                  </div>
                </div>
                <div class="ctl">
                  <div class="ctl-top"><span>l<sub>max</sub> · górna granica</span><b data-p="lMax"></b></div>
                  <div class="map-l">
                    <input type="range" data-l="lMax" min="0.05" max="6.35" step="0.05" aria-label="l max">
                    <input type="number" data-l="lMax" min="0.05" max="6.35" step="0.05" aria-label="l max">
                  </div>
                </div>
                <div class="map-msg" id="map-l-msg" role="status"></div>
                <button class="btn sm" id="map-l-default" style="--c:var(--lime)">Wartości domyślne</button>
              </div>
              <div class="card">
                <div class="card-h">Odczyt</div>
                <div>
                  <div class="kv"><span class="k">Siatka</span><span class="v" id="map-size"></span></div>
                  <div class="kv"><span class="k">Pamięć</span><span class="v" id="map-mem"></span></div>
                  <div class="kv"><span class="k">Komórka (i, j)</span><span class="v" id="map-cell">—</span></div>
                  <div class="kv"><span class="k">Punkt (x, y)</span><span class="v" id="map-xy">—</span></div>
                  <div class="kv"><span class="k">l · p</span><span class="v" id="map-lp">—</span></div>
                </div>
              </div>
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
            <div class="lcd"><div class="lcd-row" id="lcd-row-0"></div><div class="lcd-row" id="lcd-row-1"></div></div>
            <div class="lcd-in"><input type="text" id="lcd-text-0" placeholder="Pierwsza linia" autocomplete="off"><span class="cc" id="lcd-cc-0"></span></div>
            <div class="lcd-in"><input type="text" id="lcd-text-1" placeholder="Druga linia" autocomplete="off"><span class="cc" id="lcd-cc-1"></span></div>
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
      //  KONFIGURACJA
      const LCD_COLS = 16; // szerokość linii LCD
      const LOG_MAX = 60; // maks. liczba wpisów w logach debugowania
      const SPEED_MIN = 80;
      const SPEED_MAX = 255;
      const SERVO_MIN = 15;
      const SERVO_MAX = 165;
      const HOST = location.hostname || '192.168.4.1';
      const WS_URL = `ws://${HOST}:80/`;
      const CAM_URL = `http://${HOST}:81`;
      const WS_RETRY_MS = 1500; // odstęp między próbami ponownego połączenia
      const DATA_TIMEOUT_MS = 2000; // brak danych przez ten czas = brak odpowiedzi Arduino
      const WS_STALL_MS = 6000; // brak danych dłużej niż restart Arduino = zatrzymane połączenie, łączymy się od nowa
      const MAP_RESUME_DELAY_MS = 1000; // mapowanie przerwane utratą danych jest wznawiane, gdy dane płyną od tego czasu
      const MAP_RESUME_MAX_SHIFT_CM = 20; // większa zmiana pozy podczas przerwy blokuje wznowienie (np. utrata pozy
      const MAP_RESUME_MAX_TURN_DEG = 15; // po wyłączeniu zasilania Arduino albo przestawienie pojazdu)
      const MOTOR_REPEAT_MS = 500; // powtarzanie W:MOTOR podczas jazdy
      const SERVO_HOLD_MS = 400;
      const CALIB_GAP_MS = 1000;
      const CALIB_MAX_MS = 6000;
      const CAM_TIMEOUT_MS = 8000; // limit czasu żądań do kamery (budzenie trwa ok. 0,5 s)
      const PULSE_MS = 500; // czas animacji znacznika ramki
      const NO_ECHO = '-';
      const BEAM_DEG= 15;
      const MAP_SIZE_CM = 800; // bok kwadratowej mapy; punkt (0, 0) leży na jej środku
      const MAP_CELL_CM = 3.4; // bok komórki, zbliżony do rozdzielczości odległościowej sonaru
      const MAP_L_STEP = 0.05; // wartość l odpowiadająca jednostce zapisanej w bajcie komórki
      const MAP_L_RAW_MAX = 127; // największy moduł zapisanej wartości (l = +/-6,35)
      const MAP_L_DEFAULT = { lOcc: 0.85, lFree: -0.4, l0: 0, lMin: -2, lMax: 3.5 };
      const MAP_ZOOM_MIN = 0.2; // px/cm
      const MAP_ZOOM_MAX = 12;
      const MAP_ZOOM_STEP = 1.25;
      const MAP_PNG_SCALE = 4; // liczba pikseli PNG na bok komórki

      // symbole wyświetlane na stronie, zapisane kodami (takie same jak w HTML)
      const NO_DATA = '\u2014'; // pole bez wartości
      const DEG = '\u00b0';
      const PLUS_MINUS = '\u00b1';
      const DOT = '\u00b7';
      const TIMES = '\u00d7';
      const NBSP = '\u00a0';
      const ICON_START = '\u25b6';
      const ICON_STOP = '\u25a0';
      const ICON_POWER = '\u23fb';

      const $ = (s, r = document) => r.querySelector(s);
      const $$ = (s, r = document) => [...r.querySelectorAll(s)];
      const MONO = getComputedStyle(document.documentElement).getPropertyValue('--mono');
      const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
      const rad = d => d * Math.PI / 180;
      const p2 = n => String(n).padStart(2, '0');
      const hms = (d = new Date()) => d.toLocaleTimeString('pl-PL', { hour12: false });
      const hmsms = (d = new Date()) => hms(d) + '.' + String(d.getMilliseconds()).padStart(3, '0');
      const stamp = d => `${d.getFullYear()}${p2(d.getMonth() + 1)}${p2(d.getDate())}_${p2(d.getHours())}${p2(d.getMinutes())}${p2(d.getSeconds())}`;

      // WIDOK: STATUS POŁĄCZENIA
      const CONN = {
        offline: ['ROZŁĄCZONO', 'Brak połączenia z pojazdem'],
        connecting: ['ŁĄCZENIE...', 'Nawiązywanie połączenia'],
        online: ['POŁĄCZONO', 'Pojazd odpowiada'],
      };

      function setConnection(state, info = {}) {
        const [label, sub] = CONN[state];
        $('#conn').dataset.state = state;
        $('#conn-label').textContent = label;
        $('#conn-sub').textContent = info.sub || sub;
        for (const k of ['host', 'uart']) if (k in info) $(`[data-s=${k}]`).textContent = info[k];
      }

      // WIDOK: LOGI DEBUGOWANIA
      function pushLog(kind, text) {
        const ul = $('#log-' + kind);
        const li = document.createElement('li');
        const t = document.createElement('time');
        t.textContent = hmsms();
        const s = document.createElement('span');
        s.textContent = text;
        s.title = text;
        li.append(t, s);
        ul.append(li);
        while (ul.children.length > LOG_MAX) ul.firstChild.remove();
        ul.scrollTop = ul.scrollHeight;
      }

      // WIDOK: TELEMETRIA
      const T_ELS = $$('[data-t]');
      let lastPulse = -PULSE_MS;

      // animacja znacznika ramki; odczyt offsetWidth restartuje animację CSS
      function pulseFrame() {
        const now = performance.now();
        if (now - lastPulse < PULSE_MS) return;
        lastPulse = now;
        const p = $('#frame-pulse');
        p.classList.remove('hit');
        void p.offsetWidth;
        p.classList.add('hit');
      }

      function showTelemetry(d) {
        for (const el of T_ELS) {
          const k = el.dataset.t;
          if (d[k] == null) continue;
          const v = d[k];
          el.textContent = typeof v === 'number' ? v.toFixed(+(el.dataset.d || 0)) : String(v);
        }

        if (d.raw != null) $('#frame-raw').textContent = d.raw;
        pulseFrame();

        if (d.theta != null) $('#compass-needle').setAttribute('transform', `rotate(${-d.theta} 15 15)`);
        if (d.sonar != null) {
          $('#prox-bar').style.width = typeof d.sonar === 'number' ? clamp(100 - d.sonar / 2, 3, 100) + '%' : '0';
        }
      }

      const tickClock = () => $('#snap-time').textContent = hms();
      tickClock();
      setInterval(tickClock, 1000);

      // WIDOK: TRYBY (zakładki)
      function showMode(m) {
        $$('.tab').forEach(t => t.classList.toggle('active', t.dataset.mode === m));
        $$('.mode').forEach(s => s.classList.toggle('active', s.id === 'mode-' + m));
      }

      // WIDOK: SUWAKI
      for (const id of ['#s-left', '#s-right']) {
        const s = $(id);
        s.min = SPEED_MIN;
        s.max = SPEED_MAX;
      }
      $('#s-servo').min = SERVO_MIN;
      $('#s-servo').max = SERVO_MAX;

      function paintRange(el) {
        el.style.setProperty('--p', (el.value - el.min) / (el.max - el.min) * 100 + '%');
      }

      $$('input[type=range]').forEach(el => {
        paintRange(el);
        el.addEventListener('input', () => {
          paintRange(el);
          if (el.dataset.out) $('#' + el.dataset.out).textContent = el.value;
        });
      });
      $('#spd-range').textContent = `${SPEED_MIN} - ${SPEED_MAX}`;

      // WIDOK: KIERUNEK JAZDY
      const DIR_LABEL = { F: 'PRZÓD', B: 'TYŁ', L: 'LEWO', R: 'PRAWO', S: 'STOP' };

      function showDrive(dir) {
        $('#drive-dir').textContent = DIR_LABEL[dir];
        $('#drive-dir').classList.toggle('moving', dir !== 'S');
        $$('.pbtn').forEach(b => b.classList.toggle('pressed', b.dataset.dir === dir && dir !== 'S'));
        if (dir === 'S') {
          const s = $('.pbtn.stop');
          s.classList.add('pressed');
          setTimeout(() => s.classList.remove('pressed'), 150);
        }
      }

      function showDriveMode(m) {
        $$('#drive-mode button').forEach(b => b.classList.toggle('on', b.dataset.dm === m));
        $('#drive-hint').textContent = m === 'hold'
          ? `przytrzymaj kierunek ${DOT} W A S D / strzałki ${DOT} spacja = STOP`
          : `kliknij raz, aby jechać ${DOT} ponowne kliknięcie lub STOP zatrzymuje`;
      }

      // WIDOK: WSKAŹNIK SERWA (180 st. = lewo, 90 st. = przód, 0 st. = prawo)
      function gaugeXY(deg, len) {
        const v = rad(deg);
        return [100 + len * Math.cos(v), 100 - len * Math.sin(v)];
      }

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
          const t = $(id);
          t.textContent = deg + DEG;
          t.setAttribute('x', x.toFixed(2));
          t.setAttribute('y', (y + 17).toFixed(2));
        }
      })();

      function setServoUI(deg) {
        deg = clamp(Math.round(deg), SERVO_MIN, SERVO_MAX);
        const s = $('#s-servo');
        s.value = deg;
        paintRange(s);
        $('#servo-set').textContent = deg;
        const [nx, ny] = gaugeXY(deg, 72), n = $('#g-set');
        n.setAttribute('x2', nx);
        n.setAttribute('y2', ny);
        const [x, y] = gaugeXY(deg, 80), [x0, y0] = gaugeXY(SERVO_MIN, 80);
        $('#g-arc').setAttribute('d', deg === SERVO_MIN ? '' : `M${x0.toFixed(2)} ${y0.toFixed(2)} A80 80 0 0 0 ${x.toFixed(2)} ${y.toFixed(2)}`);
        return deg;
      }
      setServoUI(90);

      // WIDOK: RADAR - każdy pomiar rysowany jako łuk o szerokości stożka wiązki
      // (180 st. = lewo, 0 st. = prawo; skan w zakresie od-do, wykres w zakresie ruchu serwa)
      const RADAR_TICKS = [SERVO_MIN, 30, 60, 90, 120, 150, SERVO_MAX];
      const radar = {
        cv: $('#radar'), ctx: $('#radar').getContext('2d'),
        w: 0, h: 0, range: 200, angle: 90, dir: 1, from: SERVO_MIN, to: SERVO_MAX, pts: new Map(),

        resize() {
          const r = this.cv.getBoundingClientRect(), dpr = devicePixelRatio || 1;
          this.w = r.width;
          this.h = r.height;
          this.cv.width = r.width * dpr;
          this.cv.height = r.height * dpr;
          this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        },

        setAngle(a) {
          if (a !== this.angle) this.dir = a > this.angle ? 1 : -1;
          this.angle = a;
        },

        addPoint(a, d) {
          this.pts.set(Math.round(a), { d, t: performance.now() });
        },

        draw() {
          const { ctx: c, w, h } = this;
          if (!w || !h) return;
          c.clearRect(0, 0, w, h);
          const pad = 36, cx = w / 2, cy = h - 24, R = Math.max(20, Math.min(w / 2 - pad, h - pad - 24));
          const V = a => a; // kąt serwa na kąt na ekranie (0 st. po prawej)
          const P = (a, r) => [cx + r * Math.cos(rad(V(a))), cy - r * Math.sin(rad(V(a)))];
          // wycinek pierścienia między kątami serwa a1 < a2
          const sector = (a1, a2, r1, r2) => {
            const lo = rad(V(clamp(a1, SERVO_MIN, SERVO_MAX))), hi = rad(V(clamp(a2, SERVO_MIN, SERVO_MAX)));
            c.beginPath();
            c.arc(cx, cy, r2, -hi, -lo);
            c.arc(cx, cy, Math.max(0, r1), -lo, -hi, true);
            c.closePath();
          };

          // tło i siatka
          sector(SERVO_MIN, SERVO_MAX, 0, R);
          const g = c.createRadialGradient(cx, cy, 0, cx, cy, R);
          g.addColorStop(0, 'rgba(0,229,255,.10)');
          g.addColorStop(1, 'rgba(0,229,255,.015)');
          c.fillStyle = g;
          c.fill();
          c.lineWidth = 1;
          c.font = `10px ${MONO}`;
          for (let i = 1; i <= 4; i++) {
            const r = R * i / 4;
            c.strokeStyle = i === 4 ? 'rgba(0,229,255,.45)' : 'rgba(0,229,255,.16)';
            c.beginPath();
            c.arc(cx, cy, r, -rad(SERVO_MAX), -rad(SERVO_MIN));
            c.stroke();
            c.fillStyle = 'rgba(0,229,255,.55)';
            c.textAlign = 'left';
            c.fillText(Math.round(this.range * i / 4) + ' cm', cx + 4, cy - r + 12);
          }
          c.textAlign = 'center';
          for (const deg of RADAR_TICKS) {
            const [x, y] = P(deg, R), [lx, ly] = P(deg, R + 16);
            c.strokeStyle = 'rgba(0,229,255,.16)';
            c.beginPath();
            c.moveTo(cx, cy);
            c.lineTo(x, y);
            c.stroke();
            c.fillStyle = '#6f7b94';
            c.fillText(deg + DEG, lx, ly + 4);
          }

          // zakres skanu (od-do): przyciemnienie poza zakresem i linie graniczne
          c.fillStyle = 'rgba(6,7,11,.55)';
          if (this.from > SERVO_MIN) {
            sector(SERVO_MIN, this.from, 0, R);
            c.fill();
          }
          if (this.to < SERVO_MAX) {
            sector(this.to, SERVO_MAX, 0, R);
            c.fill();
          }
          c.save();
          c.strokeStyle = '#ff2e97';
          c.lineWidth = 1.5;
          c.setLineDash([5, 4]);
          c.shadowColor = '#ff2e97';
          c.shadowBlur = 6;
          for (const a of [this.from, this.to]) {
            const [lx, ly] = P(a, R);
            c.beginPath();
            c.moveTo(cx, cy);
            c.lineTo(lx, ly);
            c.stroke();
          }
          c.restore();

          // aktualny stożek wiązki (+/- BEAM_DEG / 2) i oś wiązki
          const half = BEAM_DEG / 2;
          sector(this.angle - half, this.angle + half, 0, R);
          const cg = c.createRadialGradient(cx, cy, 0, cx, cy, R);
          cg.addColorStop(0, 'rgba(0,229,255,.28)');
          cg.addColorStop(1, 'rgba(0,229,255,.04)');
          c.fillStyle = cg;
          c.fill();
          c.strokeStyle = 'rgba(0,229,255,.35)';
          c.setLineDash([3, 4]);
          c.stroke();
          c.setLineDash([]);
          const [bx, by] = P(this.angle, R);
          c.save();
          c.strokeStyle = '#00e5ff';
          c.lineWidth = 2;
          c.shadowColor = '#00e5ff';
          c.shadowBlur = 14;
          c.beginPath();
          c.moveTo(cx, cy);
          c.lineTo(bx, by);
          c.stroke();
          c.restore();

          // pomiary: łuk o szerokości wiązki i punkt środkowy (bez łączenia punktów liniami)
          const now = performance.now();
          let near = null;
          for (const [a, p] of this.pts) {
            if (p.d > this.range) continue;
            if (!near || p.d < near.d) near = { a, d: p.d };
            const r = R * p.d / this.range, [x, y] = P(a, r);
            const alpha = clamp(1 - (now - p.t) / 15000, .25, 1);
            const col = '180,255,31'; // pomiary: zielone
            c.strokeStyle = `rgba(${col},${.45 * alpha})`;
            c.lineWidth = 2;
            c.lineCap = 'round';
            c.beginPath();
            c.arc(cx, cy, r, -rad(V(clamp(a + half, SERVO_MIN, SERVO_MAX))), -rad(V(clamp(a - half, SERVO_MIN, SERVO_MAX))));
            c.stroke();
            c.fillStyle = `rgba(${col},${alpha})`;
            c.shadowColor = `rgb(${col})`;
            c.shadowBlur = 6 * alpha;
            c.beginPath();
            c.arc(x, y, 2.2, 0, Math.PI * 2);
            c.fill();
            c.shadowBlur = 0;
          }
          c.shadowBlur = 0;

          // pojazd
          c.fillStyle = '#ff2e97';
          c.shadowColor = '#ff2e97';
          c.shadowBlur = 12;
          c.beginPath();
          c.moveTo(cx, cy - 12);
          c.lineTo(cx + 8, cy + 4);
          c.lineTo(cx - 8, cy + 4);
          c.closePath();
          c.fill();
          c.shadowBlur = 0;

          $('#r-near').textContent = near ? near.d.toFixed(0) : NO_DATA;
          $('#r-near-a').textContent = near ? near.a : NO_DATA;
        }
      };
      $('#unc-legend').textContent = `łuk = szerokość wiązki ${PLUS_MINUS}${BEAM_DEG / 2}${DEG}`;
      new ResizeObserver(() => {
        if (mode === 'radar') radar.resize();
      }).observe($('#radar-wrap'));

      // pętla animacji radaru, uruchamiana w sekcji START
      function radarLoop() {
        if (mode === 'radar') radar.draw();
        requestAnimationFrame(radarLoop);
      }

      function showReading(angle, dist) {
        $('#r-cur').textContent = typeof dist === 'number' ? dist.toFixed(0) : NO_ECHO;
        $('#r-cur-a').textContent = Math.round(angle);
      }

      function showScan(on) {
        const rb = $('#radar-scan');
        rb.classList.toggle('on', on);
        rb.textContent = on ? `${ICON_STOP} Stop skanu` : `${ICON_START} Start skanu`;
      }

      // suwak z dwoma punktami: od-do (180 st. po lewej, 0 st. po prawej, jak na wizualizacji)
      const rFrom = $('#radar-from'), rTo = $('#radar-to');
      for (const r of [rFrom, rTo]) {
        r.min = SERVO_MIN;
        r.max = SERVO_MAX;
      }
      rFrom.value = SERVO_MIN;
      rTo.value = SERVO_MAX;

      function paintDual(e) {
        let f = +rFrom.value, t = +rTo.value;
        if (f > t) {
          if (e && e.target === rFrom) rFrom.value = f = t;
          else rTo.value = t = f;
        }
        radar.from = f;
        radar.to = t;
        $('#o-from').textContent = f;
        $('#o-to').textContent = t;
        const span = (SERVO_MAX - SERVO_MIN) / 100;
        const fill = $('#dual-fill');
        fill.style.left = (SERVO_MAX - t) / span + '%';
        fill.style.right = (f - SERVO_MIN) / span + '%';
        rFrom.style.zIndex = f > SERVO_MAX - 10 ? 3 : 1; // żeby nie zablokować uchwytów na skraju
      }
      rFrom.addEventListener('input', paintDual);
      rTo.addEventListener('input', paintDual);
      paintDual();

      // DANE: SIATKA ZAJĘTOŚCI
      // Komórka m(i, j) zajmuje jeden bajt ze znakiem: l(i, j) / MAP_L_STEP, gdzie l = ln(p / (1 - p)).
      // i rośnie wraz z x, j rośnie wraz z y; współrzędne świata w cm, jak w telemetrii.
      const mapSnap = l => +(Math.round(l / MAP_L_STEP) * MAP_L_STEP).toFixed(2);
      const mapProb = l => 1 - 1 / (1 + Math.exp(l));

      function mapRawParams(p) {
        const raw = {};
        for (const k in MAP_L_DEFAULT) raw[k] = Math.round(p[k] / MAP_L_STEP);
        return raw;
      }

      // zwraca opis błędu albo pusty tekst, jeśli parametry są poprawne
      function mapParamsError(p) {
        if (Object.keys(MAP_L_DEFAULT).some(k => !Number.isFinite(p[k]))) return 'Uzupełnij wszystkie parametry.';
        const r = mapRawParams(p);
        if (Object.values(r).some(v => Math.abs(v) > MAP_L_RAW_MAX)) return `Wartości l muszą mieścić się w ${PLUS_MINUS}${(MAP_L_RAW_MAX * MAP_L_STEP).toFixed(2)}.`;
        if (r.lOcc <= 0 || r.lFree >= 0) return 'Wymagane: l occ > 0 oraz l free < 0.';
        if (r.lFree >= r.l0 || r.l0 >= r.lOcc) return 'Wymagane: l free < l 0 < l occ.';
        if (r.lMin >= r.l0 || r.l0 >= r.lMax) return 'Wymagane: l min < l 0 < l max.';
        return '';
      }

      const occGrid = {
        cols: Math.ceil(MAP_SIZE_CM / MAP_CELL_CM),
        rows: Math.ceil(MAP_SIZE_CM / MAP_CELL_CM),
        cell: MAP_CELL_CM,
        originX: 0, originY: 0, // róg komórki (0, 0)
        cells: null, // Int8Array, wiersze od j = 0
        params: null, raw: null, // parametry l oraz te same wartości w jednostkach bajtu
        dirty: null, // prostokąt komórek do przerysowania
        updates: 0, // liczba zmian komórek od wyczyszczenia mapy

        init(params) {
          this.originX = -this.cols * this.cell / 2;
          this.originY = -this.rows * this.cell / 2;
          this.cells = new Int8Array(this.cols * this.rows);
          this.setRawParams(params);
          this.clear();
        },

        setRawParams(p) {
          this.raw = mapRawParams(p);
          this.params = {};
          for (const k in this.raw) this.params[k] = mapSnap(p[k]);
        },

        inBounds(i, j) { return i >= 0 && j >= 0 && i < this.cols && j < this.rows; },
        index(i, j) { return j * this.cols + i; },

        // przeliczenia świat <-> komórka; wynik colOf/rowOf może leżeć poza mapą (sprawdza inBounds)
        colOf(x) { return Math.floor((x - this.originX) / this.cell); },
        rowOf(y) { return Math.floor((y - this.originY) / this.cell); },
        cellX(i) { return this.originX + (i + .5) * this.cell; },
        cellY(j) { return this.originY + (j + .5) * this.cell; },

        // odczyt; komórka spoza mapy jest nieznana
        logOdds(i, j) {
          return this.inBounds(i, j) ? mapSnap(this.cells[this.index(i, j)] * MAP_L_STEP) : this.params.l0;
        },
        probability(i, j) { return mapProb(this.logOdds(i, j)); },

        // aktualizacja komórki: l = l + l_pom - l_0, ograniczone do [l_min, l_max]
        update(i, j, lPomRaw) {
          if (!this.inBounds(i, j)) return;
          const k = this.index(i, j), { l0, lMin, lMax } = this.raw;
          const v = clamp(this.cells[k] + lPomRaw - l0, lMin, lMax);
          if (v === this.cells[k]) return;
          this.cells[k] = v;
          this.updates++;
          this.markDirty(i, j, i, j);
        },
        markOccupied(i, j) { this.update(i, j, this.raw.lOcc); },
        markFree(i, j) { this.update(i, j, this.raw.lFree); },

        // Zmiana l_0 zachowuje zebrane pomiary (przesuwa wszystkie komórki), a zmiana granic
        // od razu obowiązuje też dla zapisanych komórek. l_occ i l_free działają na kolejne pomiary.
        setParams(p) {
          if (mapParamsError(p)) return false;
          const old = this.raw;
          this.setRawParams(p);
          const { l0, lMin, lMax } = this.raw, shift = l0 - old.l0;
          if (shift || lMin !== old.lMin || lMax !== old.lMax) {
            const c = this.cells;
            for (let k = 0; k < c.length; k++) c[k] = clamp(c[k] + shift, lMin, lMax);
            this.markDirty(0, 0, this.cols - 1, this.rows - 1);
          }
          return true;
        },

        clear() {
          this.cells.fill(this.raw.l0);
          this.updates = 0;
          this.markDirty(0, 0, this.cols - 1, this.rows - 1);
        },

        markDirty(i0, j0, i1, j1) {
          const d = this.dirty;
          this.dirty = d
            ? { i0: Math.min(d.i0, i0), j0: Math.min(d.j0, j0), i1: Math.max(d.i1, i1), j1: Math.max(d.j1, j1) }
            : { i0, j0, i1, j1 };
        },

        takeDirty() {
          const d = this.dirty;
          this.dirty = null;
          return d;
        },
      };
      occGrid.init(MAP_L_DEFAULT);

      // WIDOK: MAPA - odcień komórki wynika z p: 1 = czarna, 0,5 = szara, 0 = biała
      const MAP_GRAY = Uint8ClampedArray.from({ length: 256 }, (_, b) => 255 * (1 - mapProb((b - 128) * MAP_L_STEP)));

      const mapView = {
        cv: $('#map'), ctx: $('#map').getContext('2d'),
        off: document.createElement('canvas'), // obraz siatki: jeden piksel na komórkę
        img: new ImageData(occGrid.cols, occGrid.rows),
        w: 0, h: 0, zoom: 1, vx: 0, vy: 0, // zoom [px/cm], (vx, vy) = punkt na środku widoku
        fitted: false, follow: false, stale: true,
        pose: null, hover: null,
        marks: [], // wykryte granice ściany { x, y } [cm]
        walls: [], // dopasowane ściany na znanym odcinku { a, b, current } [cm]

        sx(x) { return this.w / 2 + (x - this.vx) * this.zoom; },
        sy(y) { return this.h / 2 - (y - this.vy) * this.zoom; },
        wx(px) { return this.vx + (px - this.w / 2) / this.zoom; },
        wy(py) { return this.vy - (py - this.h / 2) / this.zoom; },

        resize() {
          const r = this.cv.getBoundingClientRect(), dpr = devicePixelRatio || 1;
          this.w = r.width;
          this.h = r.height;
          this.cv.width = r.width * dpr;
          this.cv.height = r.height * dpr;
          this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
          this.stale = true;
          if (!this.fitted && this.w && this.h) this.fit();
        },

        fit() {
          this.fitted = true;
          this.setFollow(false);
          this.zoom = clamp(Math.min(this.w, this.h) / (occGrid.cols * occGrid.cell), MAP_ZOOM_MIN, MAP_ZOOM_MAX);
          this.moveTo(0, 0);
        },

        moveTo(x, y) {
          const half = occGrid.cols * occGrid.cell / 2;
          this.vx = clamp(x, -half, half);
          this.vy = clamp(y, -half, half);
          this.stale = true;
        },

        // przybliża tak, aby punkt pod (px, py) pozostał w miejscu
        zoomBy(f, px = this.w / 2, py = this.h / 2) {
          const x = this.wx(px), y = this.wy(py);
          this.zoom = clamp(this.zoom * f, MAP_ZOOM_MIN, MAP_ZOOM_MAX);
          if (this.follow) this.stale = true;
          else this.moveTo(x - (px - this.w / 2) / this.zoom, y + (py - this.h / 2) / this.zoom);
        },

        setFollow(on) {
          this.follow = on;
          $('#map-follow').classList.toggle('on', on);
          if (on && this.pose) this.moveTo(this.pose.x, this.pose.y);
        },

        setPose(p) {
          this.pose = p;
          this.stale = true;
          if (this.follow) this.moveTo(p.x, p.y);
        },

        setMarks(marks) {
          this.marks = marks;
          this.stale = true;
        },

        setWalls(walls) {
          this.walls = walls;
          this.stale = true;
        },

        // przepisuje zmienione komórki do obrazu siatki (wiersz j = 0 na dole)
        syncImage() {
          const d = occGrid.takeDirty();
          if (!d) return;
          const { cols, rows, cells } = occGrid, px = this.img.data;
          for (let j = d.j0; j <= d.j1; j++) {
            for (let i = d.i0; i <= d.i1; i++) {
              const g = MAP_GRAY[cells[occGrid.index(i, j)] + 128], o = ((rows - 1 - j) * cols + i) * 4;
              px[o] = px[o + 1] = px[o + 2] = g;
              px[o + 3] = 255;
            }
          }
          this.off.getContext('2d').putImageData(this.img, 0, 0, d.i0, rows - 1 - d.j1, d.i1 - d.i0 + 1, d.j1 - d.j0 + 1);
          this.stale = true;
          this.showHover();
        },

        // linie siatki co step, zaczepione w punkcie base, tylko w widocznej części mapy
        gridLines(baseX, baseY, step, style) {
          const { ctx: c, w, h } = this, { originX, originY, cols, rows, cell } = occGrid;
          const x0 = Math.max(originX, this.wx(0)), x1 = Math.min(originX + cols * cell, this.wx(w));
          const y0 = Math.max(originY, this.wy(h)), y1 = Math.min(originY + rows * cell, this.wy(0));
          c.strokeStyle = style;
          c.beginPath();
          for (let n = Math.ceil((x0 - baseX) / step); baseX + n * step <= x1; n++) {
            const s = Math.round(this.sx(baseX + n * step)) + .5;
            c.moveTo(s, this.sy(y0));
            c.lineTo(s, this.sy(y1));
          }
          for (let n = Math.ceil((y0 - baseY) / step); baseY + n * step <= y1; n++) {
            const s = Math.round(this.sy(baseY + n * step)) + .5;
            c.moveTo(this.sx(x0), s);
            c.lineTo(this.sx(x1), s);
          }
          c.stroke();
        },

        draw() {
          this.syncImage();
          const { ctx: c, w, h, zoom } = this;
          if (!this.stale || !w || !h) return;
          this.stale = false;
          const { originX, originY, cols, rows, cell } = occGrid;
          c.fillStyle = '#06070b';
          c.fillRect(0, 0, w, h);
          c.imageSmoothingEnabled = false;
          c.drawImage(this.off, this.sx(originX), this.sy(originY + rows * cell), cols * cell * zoom, rows * cell * zoom);

          // siatka: komórki (po przybliżeniu) i podziałka metrowa
          c.lineWidth = 1;
          if (cell * zoom >= 10) this.gridLines(originX, originY, cell, 'rgba(0,0,0,.14)');
          const step = zoom * 100 >= 40 ? 100 : 200;
          this.gridLines(0, 0, step, 'rgba(0,150,180,.42)');
          c.strokeStyle = '#00a9c7';
          c.lineWidth = 1.5;
          c.beginPath();
          c.moveTo(this.sx(-8 / zoom), this.sy(0));
          c.lineTo(this.sx(8 / zoom), this.sy(0));
          c.moveTo(this.sx(0), this.sy(-8 / zoom));
          c.lineTo(this.sx(0), this.sy(8 / zoom));
          c.stroke();
          c.strokeStyle = '#3b4458';
          c.strokeRect(this.sx(originX), this.sy(originY + rows * cell), cols * cell * zoom, rows * cell * zoom);

          // opisy podziałki przy krawędziach widoku
          c.font = `10px ${MONO}`;
          c.fillStyle = '#007f96';
          c.textAlign = 'center';
          for (let x = Math.ceil(Math.max(originX, this.wx(0)) / step) * step; x <= Math.min(-originX, this.wx(w)); x += step) {
            if (this.sx(x) > 40 && this.sx(x) < w - 20) c.fillText(x / 100 + ' m', this.sx(x), h - 6);
          }
          c.textAlign = 'left';
          for (let y = Math.ceil(Math.max(originY, this.wy(h)) / step) * step; y <= Math.min(-originY, this.wy(0)); y += step) {
            if (this.sy(y) < h - 18) c.fillText(y / 100 + ' m', 5, this.sy(y) - 3);
          }

          // dopasowane ściany: odcinek prostej na zakresie, w którym ściana jest znana
          c.lineCap = 'round';
          for (const wl of this.walls) {
            c.beginPath();
            c.moveTo(this.sx(wl.a.x), this.sy(wl.a.y));
            c.lineTo(this.sx(wl.b.x), this.sy(wl.b.y));
            c.setLineDash([]);
            c.strokeStyle = '#06070b';
            c.lineWidth = 5;
            c.stroke();
            c.setLineDash(wl.current ? [8, 5] : []);
            c.strokeStyle = wl.current ? '#ffb300' : '#00e5ff';
            c.lineWidth = 3;
            c.stroke();
          }
          c.setLineDash([]);
          c.lineCap = 'butt';

          // granice ściany
          c.fillStyle = '#b4ff1f';
          c.strokeStyle = '#06070b';
          c.lineWidth = 1.5;
          for (const m of this.marks) {
            c.beginPath();
            c.arc(this.sx(m.x), this.sy(m.y), 6, 0, Math.PI * 2);
            c.fill();
            c.stroke();
          }

          // pojazd: wierzchołek trójkąta wskazuje kierunek jazdy (theta = 0 wzdłuż osi x)
          if (this.pose) {
            c.save();
            c.translate(this.sx(this.pose.x), this.sy(this.pose.y));
            c.rotate(-rad(this.pose.theta));
            c.fillStyle = '#ff2e97';
            c.strokeStyle = '#06070b';
            c.lineWidth = 1.5;
            c.beginPath();
            c.moveTo(13, 0);
            c.lineTo(-7, 8);
            c.lineTo(-7, -8);
            c.closePath();
            c.fill();
            c.stroke();
            c.restore();
          }
        },

        setHover(px, py) {
          this.hover = px == null ? null : { x: this.wx(px), y: this.wy(py) };
          this.showHover();
        },

        showHover() {
          const p = this.hover, i = p ? occGrid.colOf(p.x) : -1, j = p ? occGrid.rowOf(p.y) : -1;
          const known = occGrid.inBounds(i, j);
          $('#map-cell').textContent = known ? `${i}, ${j}` : NO_DATA;
          $('#map-xy').textContent = known ? `${p.x.toFixed(0)}, ${p.y.toFixed(0)} cm` : NO_DATA;
          $('#map-lp').textContent = known ? `${occGrid.logOdds(i, j).toFixed(2)} ${DOT} ${occGrid.probability(i, j).toFixed(2)}` : NO_DATA;
        },
      };
      mapView.off.width = occGrid.cols;
      mapView.off.height = occGrid.rows;
      $('#map-size').textContent = `${occGrid.cols} ${TIMES} ${occGrid.rows} ${DOT} ${occGrid.cell} cm`;
      $('#map-mem').textContent = `${(occGrid.cells.length / 1024).toFixed(1)} kB`;
      new ResizeObserver(() => {
        if (mode === 'map') mapView.resize();
      }).observe($('#map-wrap'));

      // pętla animacji mapy, uruchamiana w sekcji START; rysuje tylko po zmianach
      function mapLoop() {
        if (mode === 'map') mapView.draw();
        requestAnimationFrame(mapLoop);
      }

      function showMapRun(on) {
        const b = $('#map-run');
        b.classList.toggle('on', on);
        b.textContent = on ? `${ICON_STOP} Stop mapowania` : `${ICON_START} Start mapowania`;
      }

      // pola parametrów l; skip = pole właśnie edytowane przez użytkownika
      function showMapParams(p, skip) {
        for (const k in MAP_L_DEFAULT) {
          const ok = Number.isFinite(p[k]);
          $(`[data-p=${k}]`).textContent = ok ? `p = ${mapProb(p[k]).toFixed(2)}` : NO_DATA;
          if (!ok) continue;
          for (const el of $$(`[data-l=${k}]`)) {
            if (el === skip) continue;
            el.value = el.type === 'number' ? p[k].toFixed(2) : p[k];
            if (el.type === 'range') paintRange(el);
          }
        }
        const err = mapParamsError(p), msg = $('#map-l-msg');
        msg.classList.toggle('err', !!err);
        msg.textContent = err ? err + ' Mapa używa ostatnich poprawnych wartości.' : 'l occ i l free działają na kolejne pomiary; l 0 i granice także na zapisane komórki.';
      }

      // WIDOK: LCD (dwie linie)
      const LCD_LINE_SEP = '|'; // separator linii w W:MSG
      const LCD_BAD_CHARS = /[^\x20-\x7b\x7d\x7e]/; // znaki spoza ASCII oraz separator
      const lcdLines = [0, 1].map(i => ({row: $(`#lcd-row-${i}`), input: $(`#lcd-text-${i}`), cc: $(`#lcd-cc-${i}`)}));
      $('#lcd-size').textContent = `2 × ${LCD_COLS} znaków`;
      lcdLines.forEach(({row, input}) => {
        row.style.gridTemplateColumns = `repeat(${LCD_COLS},1fr)`;
        for (let i = 0; i < LCD_COLS; i++) row.appendChild(document.createElement('span'));
        input.maxLength = LCD_COLS;
        input.addEventListener('input', renderLcd);
      });

      function renderLcd() {
        lcdLines.forEach(({row, input, cc}) => {
          const s = input.value.padEnd(LCD_COLS).slice(0, LCD_COLS);
          [...row.children].forEach((sp, i) => sp.textContent = s[i] === ' ' ? NBSP : s[i]);
          const bad = LCD_BAD_CHARS.test(input.value);
          cc.textContent = bad ? '!ZNAK' : `${input.value.length}/${LCD_COLS}`;
          cc.classList.toggle('warn', bad);
          cc.title = bad ? `Dozwolone są znaki ASCII bez ${LCD_LINE_SEP}` : '';
        });
      }
      renderLcd();

      // WIDOK: ODOMETRIA
      function showCalibration(on) {
        $('#odo-reset').disabled = $('#odo-calib').disabled = on;
        $('#odo-calib').textContent = on ? 'Kalibracja...' : 'Kalibracja';
      }

      // WIDOK: KAMERA
      function showCamera(on) {
        $('#cam').dataset.state = on ? 'on' : 'off';
        $('#cam-toggle').textContent = on ? `${ICON_POWER} Wyłącz` : `${ICON_POWER} Włącz`;
        const tag = $('#cam-tag');
        tag.textContent = on ? 'LIVE' : 'WYŁ.';
        tag.classList.toggle('hot', on);
        const img = $('#cam-stream');
        img.hidden = !on;
        if (on) img.src = `${CAM_URL}/stream?t=${Date.now()}`; // parametr wymusza nowe połączenie
        else img.removeAttribute('src');
      }

      function showSnapshot(url, date) {
        $('#snap-img').src = url;
        $('#shot').classList.add('has');
        const dl = $('#snap-dl');
        dl.href = url;
        dl.download = `zdjecie_${stamp(date)}.jpg`;
        dl.classList.remove('disabled');
      }

      // DANE: RAMKI TELEMETRII
      // ramka: {raw, encL, encR, speed, ax, ay, az, gx, gy, gz, temp, servo, sonar, sonarX, sonarY, sonarTheta, x, y, theta} (tylko niektóre są w ramce)
      const FRAMES = {
        ENC: [2, ([encL, encR]) => ({ encL, encR })],
        // mm = 0: brak echa; sonarX, sonarY, sonarTheta: poza z chwili pomiaru (osobno od bieżącej pozy x, y, theta)
        DIST: [5, ([servo, mm, x, y, theta]) => ({
          servo, sonar: mm ? mm / 10 : NO_ECHO,
          sonarX: x / 10, sonarY: y / 10, sonarTheta: theta / 100,
        })],
        MPU: [7, ([ax, ay, az, gx, gy, gz, temp]) => ({
          ax: ax / 1000, ay: ay / 1000, az: az / 1000,
          gx: gx / 100, gy: gy / 100, gz: gz / 100, temp: temp / 100,
        })],
        POS: [4, ([x, y, theta, v]) => ({ x: x / 10, y: y / 10, theta: theta / 100, speed: v / 10 })],
      };

      function parseLine(line) {
        const m = /^A:([A-Z]+):(-?\d+(?:,-?\d+)*)$/.exec(line);
        const def = m && FRAMES[m[1]];
        if (!def) return null;
        const vals = m[2].split(',').map(Number);
        return vals.length === def[0] ? {raw: line, ...def[1](vals)} : null;
      }

      function updateTelemetry(f) {
        const d = {...f};
        showTelemetry(d);

        if (d.servo != null) {
          if (!servoDragging && performance.now() >= servoHoldUntil) setServoUI(d.servo);
          radar.setAngle(d.servo);
          if (d.sonar != null) {
            if (typeof d.sonar === 'number') radar.addPoint(d.servo, d.sonar);
            else radar.pts.delete(Math.round(d.servo));
            showReading(d.servo, d.sonar);
          }
        }

        if (d.x != null) MapControl.pose({ x: d.x, y: d.y, theta: d.theta });
        if (d.sonarX != null) {
          MapControl.measurement({
            servo: d.servo, dist: typeof d.sonar === 'number' ? d.sonar : null,
            x: d.sonarX, y: d.sonarY, theta: d.sonarTheta,
          });
        }
      }

      // LOGIKA: TRANSPORT - WebSocket do ESP32-CAM
      const Transport = {
        ws: null, lastRx: 0,
        dataOk: null, // null = jeszcze nie wiadomo, true = dane docierają, false = dane ustały
        dataSince: 0, // od kiedy dane docierają bez przerwy
        calibrating: false, calibTimer: 0,
        queue: [], // komendy wstrzymane na czas kalibracji

        get open() {return !!this.ws && this.ws.readyState === WebSocket.OPEN;},

        connect() {
          setConnection('connecting', { host: HOST, uart: NO_DATA });
          const ws = this.ws = new WebSocket(WS_URL);
          ws.onopen = () => {
            this.lastRx = performance.now();
            setConnection('connecting', {sub: 'Oczekiwanie na dane z pojazdu'});
            this.send(scanOn ? radarCmd() : 'W:RADAROFF');
          };
          ws.onmessage = e => this.receive(String(e.data));
          ws.onclose = () => this.closed();
        },

        closed() {
          this.dataOk = null;
          this.finishCalibration();
          setConnection('offline', {uart: NO_DATA});
          held.length = 0;
          if (activeDir !== 'S') setDrive('S');
          MapControl.setRunning(false, 'connection');
          setTimeout(() => this.connect(), WS_RETRY_MS);
        },

        // przy zatrzymanym połączeniu TCP przeglądarka może bardzo długo nie zgłosić onclose,
        // dlatego stare gniazdo jest porzucane od razu
        reconnect() {
          const ws = this.ws;
          this.ws = null;
          ws.onclose = ws.onmessage = null;
          ws.close();
          this.closed();
        },

        // log = false dla komend powtarzanych cyklicznie, aby nie zalewały logu
        send(cmd, log = true) {
          if (this.open) {
            if (this.calibrating) this.queue.push(cmd);
            else this.ws.send(cmd);
          }
          if (log) Dashboard.logSent(this.open ? cmd : cmd + ' (brak połączenia)');
        },

        calibrate() {
          MapControl.setRunning(false, 'calibration');
          held.length = 0;
          drive('S'); // inaczej powtarzanie W:MOTOR wznowiłoby jazdę po kalibracji
          this.send('W:CALIBRATE');
          if (!this.open) return;
          this.calibrating = true;
          this.lastRx = performance.now();
          this.calibTimer = setTimeout(() => this.finishCalibration(), CALIB_MAX_MS);
          showCalibration(true);
          if (this.dataOk) setConnection('online', {sub: 'Kalibracja żyroskopu - nie ruszaj pojazdu'});
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
          this.ws.send('W:');
          cmds.forEach(c => this.ws.send(c));
        },

        receive(line) {
          const now = performance.now(), gap = now - this.lastRx;
          this.lastRx = now;
          if (this.calibrating && gap >= CALIB_GAP_MS) this.finishCalibration();
          if (gap >= DATA_TIMEOUT_MS / 2) this.dataSince = now; // po przerwie ciągłość liczy się od nowa
          if (this.dataOk !== true) {
            this.dataOk = true;
            this.dataSince = now;
            setConnection('online', {uart: 'OK'});
          }
          Dashboard.logReceived(line);
          const frame = parseLine(line);
          if (frame) updateTelemetry(frame);
        },

        // true, gdy dane z pojazdu nie przychodzą (przerwane połączenie albo restart Arduino)
        stale() {
          return !this.open || performance.now() - this.lastRx >= DATA_TIMEOUT_MS / 2;
        },

        // true, gdy dane przychodzą bez przerwy co najmniej od ms
        steadyFor(ms) {
          return !this.stale() && !this.calibrating && this.dataOk === true && performance.now() - this.dataSince >= ms;
        },

        checkData() {
          if (!this.open || this.calibrating) return;
          const silentMs = performance.now() - this.lastRx;
          if (silentMs >= WS_STALL_MS) return this.reconnect();
          if (this.dataOk === false || silentMs < DATA_TIMEOUT_MS) return;
          this.dataOk = false;
          setConnection('connecting', {sub: 'Brak danych z Arduino', uart: 'BRAK'});
        }
      };

      // LOGIKA: LOGI DEBUGOWANIA
      $$('[data-clear]').forEach(b => b.onclick = () => {
        $('#log-' + b.dataset.clear).replaceChildren();
      });

      // LOGIKA: TRYBY (zakładki)
      let mode = 'control';
      function setMode(m) {
        mode = m;
        showMode(m);
        if (m === 'radar') radar.resize();
        else if (scanOn) {
          setScan(false);
          Transport.send('W:RADAROFF');
        }
        if (m === 'map') mapView.resize();
        else MapControl.setRunning(false, 'tab');
      }
      $$('.tab').forEach(t => t.onclick = () => setMode(t.dataset.mode));

      // LOGIKA: PRĘDKOŚĆ KÓŁ
      for (const id of ['#s-left', '#s-right']) {
        $(id).addEventListener('change', () => {
          if (activeDir !== 'S') Transport.send(motorCmd(activeDir));
        });
      }

      const PRESETS = { min: SPEED_MIN, mid: Math.round((SPEED_MIN + SPEED_MAX) / 2), max: SPEED_MAX };
      $$('[data-preset]').forEach(b => b.onclick = () => {
        for (const id of ['#s-left', '#s-right']) {
          const s = $(id);
          s.value = PRESETS[b.dataset.preset];
          s.dispatchEvent(new Event('input'));
          s.dispatchEvent(new Event('change'));
        }
      });

      // LOGIKA: KIERUNKI JAZDY (tryb przytrzymaj / przełącz)
      const DIR_SIGN = { F: [1, 1], B: [-1, -1], L: [-1, 1], R: [1, -1], S: [0, 0] }; // znaki PWM kół: lewe, prawe
      let activeDir = 'S', driveMode = 'hold';

      function motorCmd(dir) {
        const [l, r] = DIR_SIGN[dir];
        return `W:MOTOR:${l * $('#s-left').value},${r * $('#s-right').value}`;
      }

      function setDrive(dir) {
        activeDir = dir;
        showDrive(dir);
      }

      function drive(dir, force = false) {
        if (dir === activeDir && !force) return;
        if (Transport.calibrating && dir !== 'S') return; // pojazd musi stać podczas kalibracji
        setDrive(dir);
        Transport.send(motorCmd(dir));
      }

      function setDriveMode(m) {
        driveMode = m;
        showDriveMode(m);
        held.length = 0;
        if (activeDir !== 'S') drive('S'); // zmiana trybu = bezpieczne zatrzymanie
      }
      $$('#drive-mode button').forEach(b => b.onclick = () => setDriveMode(b.dataset.dm));

      $$('.pbtn').forEach(b => {
        const dir = b.dataset.dir;
        b.addEventListener('pointerdown', e => {
          e.preventDefault();
          if (driveMode === 'toggle') {
            drive(dir !== 'S' && dir === activeDir ? 'S' : dir, dir === 'S');
            return;
          }
          b.setPointerCapture(e.pointerId);
          drive(dir, dir === 'S');
        });
        const release = () => {
          if (driveMode === 'hold' && dir !== 'S' && activeDir === dir) drive('S');
        };
        b.addEventListener('pointerup', release);
        b.addEventListener('pointercancel', release);
        b.addEventListener('contextmenu', e => e.preventDefault());
      });

      const KEYMAP = { KeyW: 'F', ArrowUp: 'F', KeyS: 'B', ArrowDown: 'B', KeyA: 'L', ArrowLeft: 'L', KeyD: 'R', ArrowRight: 'R' };
      const held = [];
      addEventListener('keydown', e => {
        if (e.target.closest('input,select,textarea')) return;
        if (['Digit1', 'Digit2', 'Digit3'].includes(e.code)) {
          setMode(['control', 'radar', 'map'][+e.code.slice(-1) - 1]);
          return;
        }
        if (mode !== 'control') return;
        if (e.code === 'Space') {
          e.preventDefault();
          held.length = 0;
          drive('S', true);
          return;
        }
        const d = KEYMAP[e.code];
        if (!d) return;
        e.preventDefault();
        if (e.repeat) return;
        if (driveMode === 'toggle') {
          drive(d === activeDir ? 'S' : d);
          return;
        }
        if (!held.includes(d)) held.push(d);
        drive(d);
      });
      addEventListener('keyup', e => {
        const d = KEYMAP[e.code];
        if (!d || driveMode !== 'hold') return;
        const i = held.indexOf(d);
        if (i >= 0) held.splice(i, 1);
        if (mode === 'control') drive(held.at(-1) || 'S');
      });
      addEventListener('blur', () => {
        if (driveMode === 'hold') {
          held.length = 0;
          drive('S');
        }
      });
      // przeglądarka spowalnia timery ukrytej karty, więc nie da się w niej utrzymać jazdy
      document.addEventListener('visibilitychange', () => {
        if (document.hidden) {
          held.length = 0;
          drive('S');
          MapControl.setRunning(false, 'hidden');
        }
      });
      setInterval(() => {
        if (activeDir !== 'S') Transport.send(motorCmd(activeDir), false);
      }, MOTOR_REPEAT_MS);

      // LOGIKA: SERWO
      let servoDragging = false, servoHoldUntil = 0;
      function sendServo(deg) {
        servoHoldUntil = performance.now() + SERVO_HOLD_MS;
        Transport.send('W:SERVO:' + deg);
      }
      $('#s-servo').addEventListener('pointerdown', () => servoDragging = true);
      addEventListener('pointerup', () => servoDragging = false);
      $('#s-servo').addEventListener('input', e => setServoUI(+e.target.value));
      $('#s-servo').addEventListener('change', e => {
        servoDragging = false;
        sendServo(e.target.value);
      });
      $$('[data-servo]').forEach(b => b.onclick = () => {
        const v = b.dataset.servo, cur = +$('#s-servo').value;
        sendServo(setServoUI(v === 'center' ? 90 : cur + +v));
      });

      // LOGIKA: SKAN RADARU
      let scanOn = false, scanInterval = 200;
      function setScan(on) {
        scanOn = on;
        showScan(on);
      }
      const radarCmd = () => `W:RADAR:${radar.from},${radar.to},${$('#radar-step').value},${scanInterval}`;
      const toggleScan = () => {
        setScan(!scanOn);
        Transport.send(scanOn ? radarCmd() : 'W:RADAROFF');
      };
      // Arduino przyjmuje ustawienia tylko razem z uruchomieniem radaru, więc zmiana w trakcie skanu go restartuje
      const sendScanConfig = () => {
        if (scanOn) Transport.send(radarCmd());
      };
      $('#radar-range').addEventListener('input', e => radar.range = +e.target.value);
      $('#radar-step').onchange = sendScanConfig;
      $('#radar-int').onchange = e => {
        scanInterval = +e.target.value;
        sendScanConfig();
      };
      $('#radar-clear').onclick = () => radar.pts.clear();
      rFrom.addEventListener('change', sendScanConfig);
      rTo.addEventListener('change', sendScanConfig);
      $('#radar-scan').onclick = toggleScan;

      // LOGIKA: LCD
      lcdLines.forEach(({input}) => input.addEventListener('keydown', e => {
        if (e.key === 'Enter') $('#lcd-send').click();
      }));
      // niedozwolone znaki nie są wysyłane - ostrzeżenie pokazuje renderLcd()
      $('#lcd-send').onclick = () => {
        const [first, second] = lcdLines.map(({input}) => input.value);
        if (LCD_BAD_CHARS.test(first + second)) return;
        Transport.send('W:MSG:' + first + (second ? LCD_LINE_SEP + second : ''));
      };
      $('#lcd-clear').onclick = () => {
        lcdLines.forEach(({input}) => input.value = '');
        renderLcd();
        Transport.send('W:MSG:');
      };

      // LOGIKA: ODOMETRIA
      $('#odo-reset').onclick = () => {
        Transport.send('W:RESETPOS');
        MapHandlers.onPoseReset();
      };
      $('#odo-calib').onclick = () => Transport.calibrate();

      // LOGIKA: KAMERA
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
        showCamera(on);
      }

      $('#cam-toggle').onclick = async () => {
        const btn = $('#cam-toggle'), on = !camOn;
        btn.disabled = true;
        try {
          if (!on) setCamera(false);
          await camFetch(on ? '/stream/start' : '/stream/stop');
          if (on) setCamera(true); // /stream działa dopiero po /stream/start
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

      // ostatnie zdjęcie - wyświetlane jest tylko jedno i tylko ono jest do pobrania
      let snapUrl = null;
      function setSnapshot(url, date = new Date()) {
        if (snapUrl && snapUrl.startsWith('blob:')) URL.revokeObjectURL(snapUrl);
        snapUrl = url;
        showSnapshot(url, date);
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

      // LOGIKA: WYKRYWANIE ŚCIANY
      // Pojazd dojeżdża do przeszkody, ustawia się prostopadle do ściany, a potem przesuwa się wzdłuż niej
      // w prawo. Ściany wyznacza na siatce zajętości (transformata Hougha i regresja), więc każdy skan
      // poprawia dopasowanie. Na końcu ściany wybiera z mapy najbardziej prawdopodobną kolejną ścianę
      // i tak obchodzi pomieszczenie, aż rozpozna pierwszą ścianę (pełny obieg).
      // Używa tylko W:MOTOR, W:SERVO, W:RADAR i W:RADAROFF; obroty i odległości kontroluje na podstawie A:POS.
      const WALL = {
        SIDE: -1, // kierunek obchodu: -1 = w prawo
        STOP_CM: 50, // docelowa odległość czujnika od ściany
        STEP_MIN_CM: 20, // krok, gdy dalszy ciąg ściany nie jest jeszcze widoczny na mapie
        STEP_MAX_CM: 60,
        STEP_MARGIN_CM: 15, // krok kończy się tyle przed końcem ściany widocznej na mapie
        BLOCK_CM: 20, // przeszkoda przerywająca jazdę
        SEARCH_MAX_CM: 400, // najdłuższy dojazd do przeszkody
        MAX_WALLS: 16, // limit bezpieczeństwa: liczba ścian
        MAX_PATH_CM: 4000, // limit bezpieczeństwa: przejechana droga
        CLOSE_MAX_TURN_DEG: 450, // po takiej sumie obrotów obieg kończy się także bez rozpoznania pierwszej ściany
        SEARCH_TURN_DEG: 120, // obrót przy szukaniu ściany poza zakresem skanu
        CORNER_PAST_CM: 30, // o ile minąć linię kończącej się ściany przy narożniku wypukłym
        BRAKE_CM: 2, // zapas na wybieg po wyłączeniu silników
        DRIVE_LAG_S: 0.2, // opóźnienie pozy i wybieg uwzględniane przy hamowaniu
        DIST_TOL_CM: 3, // dokładność ustawienia odległości od ściany
        DIST_TRIES: 4,
        FRONT_PINGS: 5, // liczba pomiarów przy sprawdzaniu odległości na postoju
        MAX_RANGE_CM: 200, // dalszych pomiarów nie używamy (rozdz. 4.2.3)
        SENSOR_OFFSET_CM: 15, // odległość a sonaru od osi pojazdu (równ. 3.4) - do zmierzenia
        // skan zawsze idzie od mniejszego kąta do większego, więc luz serwa daje stały błąd kierunku,
        // który trzeba tu skompensować (+ = w lewo); kalibracja: środek łuku płaskiej ściany na wprost = 90 st.
        SERVO_TRIM_DEG: 0,

        SCAN_STEP: 2, // krok skanu [st.]; jedno przejście
        TRACE_BACK_DEG: 30, // skan przy obchodzie: kierunek jazdy, ściana i tyle stopni w drugą stronę
        SCAN_INTERVAL_MS: 150,
        SWEEP_MS: 600, // dojazd serwa na początek skanu
        SERVO_MS: 350, // ustawienie serwa na wprost
        SETTLE_MS: 400, // uspokojenie pojazdu po zatrzymaniu
        FRONT_DEG: 4, // pomiary skanu w tym zakresie od osi dają odległość na wprost

        // ściany na mapie
        L_WALL: 1.5, // komórka pewnie zajęta (co najmniej dwa trafienia)
        INLIER_CELLS: 1.5, // komórki tak blisko prostej należą do ściany
        MIN_EVIDENCE: 10, // najmniejsza suma l komórek ściany
        GAP_CM: 10, // większa przerwa dzieli ścianę na odcinki
        WALL_MIN_CM: 15, // krótszy odcinek to zwykle tylko krawędź
        REFINE_MAX_DEG: 3, // największa poprawka kąta z regresji względem transformaty Hougha
        REFINE_BAND_CELLS: 1, // pas drugiego przejścia regresji
        CAND_LINES: 5, // najwięcej prostych wyznaczanych z jednego fragmentu mapy
        FIT_ALONG_CM: 80, // okno dopasowania bieżącej ściany (ogranicza wpływ dryfu odometrii)
        FIT_BAND_CM: 30, // komórki dalej od poprzedniego położenia ściany nie są brane pod uwagę
        FIT_ANG_TOL_DEG: 15, // największa zmiana kierunku ściany między dopasowaniami
        FIT_DIST_TOL_CM: 15, // największa zmiana odległości ściany między dopasowaniami
        AHEAD_CM: 150, // zasięg szukania ściany przed pojazdem
        COVER_CM: 5, // ściana kończy się, gdy pojazd minie jej koniec o więcej
        SAME_TOL_CM: 8, // ściana kończy się także, gdy odległość na wprost różni się od wynikającej z mapy o więcej
        FACE_RESCAN_DEG: 45, // ściana dalej od osi mogła nie zmieścić się w skanie: kolejny skan przodem do niej
        FACE_RESCANS: 2,
        ALIGN_TOL_DEG: 2, // mniejsza różnica kursu i kierunku ściany nie wymaga obrotu

        // kolejna ściana
        NEXT_RADIUS_CM: 150, // zasięg szukania wokół końca bieżącej ściany
        NEXT_DIST_CM: 60, // skala kary za odległość od końca bieżącej ściany
        NEXT_BEHIND_CM: 50, // ściany leżące w całości dalej za końcem bieżącej są pomijane
        NEXT_TRIES: 3, // liczba sprawdzanych kandydatów po jednym skanie
        EXCLUDE_CM: 7, // komórki tak blisko już obchodzonej ściany nie tworzą nowych
        CORNER_WINDOW_CM: 100, // zasięg szukania ściany w poprzek kierunku jazdy (narożnik wklęsły)
        CORNER_SEG_CM: 30, // tor może przecinać przedłużenie widocznego odcinka o tyle
        CLEAR_CM: 20, // pół szerokości toru sprawdzanego na mapie
        CLOSE_ANG_DEG: 5, // rozpoznanie pierwszej ściany: zgodność kierunku
        CLOSE_DIST_CM: 15, // i odległości od początku układu
        CLOSE_GAP_CM: 50, // zapas przy sprawdzaniu, czy koniec ostatniej ściany leży przed początkiem pierwszej

        TURN_SLOW_DEG: 25, // poniżej tego błędu obrót zwalnia
        TURN_TOL_DEG: 1,
        TURN_PULSE_DEG: 12, // poniżej tego błędu obrót odbywa się impulsami
        TURN_LAG_S: 0.2, // opóźnienie pozy i wybieg uwzględniane przy hamowaniu
        TURN_TRIES: 12,
        PULSE_MS_PER_DEG: 15, // początkowy czas impulsu na stopień (dalej dobierany automatycznie)
        PULSE_MIN_MS: 40,
        PULSE_MAX_MS: 400,
        PWM_BOOST: 20, // zwiększenie PWM, gdy pojazd nie rusza
        STALL_MS: 1200,
        STEER_GAIN: 3, // korekta PWM na stopień błędu kursu podczas jazdy
        STEER_MAX: 40,
        KEEPALIVE_MS: 250, // powtarzanie W:MOTOR (Arduino zatrzymuje silniki po 1 s bez komend)
        DATA_TIMEOUT_MS: 2000,
      };
      const WALL_SPEED_DEFAULT = { drive: 220, turn: 220, turnSlow: 200 }; // [PWM]
      const wallSpeeds = { ...WALL_SPEED_DEFAULT }; // ustawiane na stronie, działają od razu
      const WALL_ABORT = new Error('przerwano');
      const WALL_AXIS_CM = WALL.STOP_CM + WALL.SENSOR_OFFSET_CM; // docelowa odległość osi pojazdu od ściany
      const WALL_ANGLES = Array.from({ length: 180 }, (_, k) => k); // kierunki normalnej w transformacie Hougha
      const angDiff = (a, b) => ((a - b + 540) % 360 + 360) % 360 - 180; // a - b w zakresie [-180, 180)
      const lineDiff = (a, b) => { const d = angDiff(a, b); return d >= 90 ? d - 180 : d < -90 ? d + 180 : d; }; // różnica kierunków prostych
      const median = v => [...v].sort((a, b) => a - b)[v.length >> 1];
      const usableDist = d => d != null && d <= WALL.MAX_RANGE_CM;
      const sonarBearing = m => m.theta + m.servo - 90 + WALL.SERVO_TRIM_DEG; // kierunek wiązki na mapie

      // wektory {x, y} [cm]
      const vDir = deg => ({ x: Math.cos(rad(deg)), y: Math.sin(rad(deg)) });
      const vDot = (a, b) => a.x * b.x + a.y * b.y;
      const vAdd = (a, b, k = 1) => ({ x: a.x + k * b.x, y: a.y + k * b.y }); // a + k * b
      const vDist = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);
      function segDist(p, a, b) { // odległość punktu od odcinka
        const ab = vAdd(b, a, -1), l2 = vDot(ab, ab);
        const k = l2 ? clamp(vDot(vAdd(p, a, -1), ab) / l2, 0, 1) : 0;
        return vDist(p, vAdd(a, ab, k));
      }

      // Ściana to prosta { phi, rho }: punkty p spełniające p . n = rho, gdzie n = vDir(phi) wskazuje od pojazdu
      // do ściany (phi = kurs przodem do ściany). Odcinek ściany to dodatkowo { lo, hi }: zakres współrzędnej s
      // wzdłuż ściany, rosnącej w kierunku obchodu.
      const wallAlong = w => vDir(w.phi + WALL.SIDE * 90);
      const wallS = (w, p) => vDot(p, wallAlong(w));
      const wallPoint = (w, s) => vAdd(vAdd({ x: 0, y: 0 }, vDir(w.phi), w.rho), wallAlong(w), s); // punkt ściany o współrzędnej s
      const wallGap = (w, p) => w.rho - vDot(p, vDir(w.phi)); // odległość od ściany (dodatnia po stronie pojazdu)
      const wallEnds = w => [wallPoint(w, w.lo), wallPoint(w, w.hi)];
      // zwrot normalnej taki, aby punkt p (pojazd) leżał przed ścianą
      const wallOrient = (w, p) => wallGap(w, p) >= 0 ? w : { ...w, phi: w.phi + 180, rho: -w.rho };
      function wallCross(a, b) { // punkt przecięcia prostych albo null dla równoległych
        const n = vDir(a.phi), m = vDir(b.phi), det = n.x * m.y - n.y * m.x;
        if (Math.abs(det) < 1e-6) return null;
        return { x: (a.rho * m.y - b.rho * n.y) / det, y: (n.x * b.rho - m.x * a.rho) / det };
      }
      function wallSameLine(a, b) {
        if (Math.abs(angDiff(a.phi, b.phi)) > 90) b = { ...b, phi: b.phi + 180, rho: -b.rho };
        return Math.abs(angDiff(a.phi, b.phi)) <= WALL.CLOSE_ANG_DEG && Math.abs(a.rho - b.rho) <= WALL.CLOSE_DIST_CM;
      }
      const sensorPos = p => vAdd(p, vDir(p.theta), WALL.SENSOR_OFFSET_CM);

      // nanosi pomiar na siatkę (rozdz. 3.8): komórki stożka przed łukiem są wolne, a na łuku zajęte.
      // Brak echa albo echo dalej niż MAX_RANGE_CM oznacza wolny stożek, ale tylko tam, gdzie już widzieliśmy
      // wolną przestrzeń: komórki z przewagą „wolna”, w każdym kierunku do pierwszej komórki z przewagą „zajęta”
      // (najwyżej do MAX_RANGE_CM). Echa brakuje też od ściany skośnej do wiązki i przy zgubionym impulsie, więc
      // taki pomiar nie zmienia komórek nieznanych ani nie ściera ścian widocznych na mapie
      function mapAddMeasurement(m) {
        const echo = usableDist(m.dist), g = occGrid, half = g.cell / 2, beam = sonarBearing(m), s = sensorPos(m);
        const r = echo ? m.dist + half : WALL.MAX_RANGE_CM, cells = [];
        for (let j = g.rowOf(s.y - r); j <= g.rowOf(s.y + r); j++) {
          for (let i = g.colOf(s.x - r); i <= g.colOf(s.x + r); i++) {
            const dx = g.cellX(i) - s.x, dy = g.cellY(j) - s.y, rho = Math.hypot(dx, dy);
            const off = angDiff(Math.atan2(dy, dx) * 180 / Math.PI, beam);
            if (g.inBounds(i, j) && rho <= r && Math.abs(off) <= BEAM_DEG / 2) cells.push({ i, j, rho, off });
          }
        }
        if (echo) {
          for (const c of cells) {
            if (c.rho < m.dist - half) g.markFree(c.i, c.j);
            else g.markOccupied(c.i, c.j);
          }
          return;
        }
        // brak echa: najbliższa komórka z przewagą „zajęta” w każdym kierunku stożka (co 1 st.)
        const bin = c => Math.round(c.off + BEAM_DEG / 2), reach = new Array(BEAM_DEG + 1).fill(r);
        for (const c of cells) if (mapWeight(g.index(c.i, c.j)) > 0) reach[bin(c)] = Math.min(reach[bin(c)], c.rho);
        for (const c of cells) if (c.rho < reach[bin(c)] - g.cell && mapWeight(g.index(c.i, c.j)) < 0) g.markFree(c.i, c.j);
      }

      // nadwyżka l komórki k nad l_0: wartość dodatnia oznacza przewagę pomiarów „zajęta”
      const mapWeight = k => (occGrid.cells[k] - occGrid.raw.l0) * MAP_L_STEP;
      // próg komórki pewnie zajętej; niższy, gdy użytkownik ustawił małe l_max
      const mapWallL = () => Math.min(WALL.L_WALL, .8 * (occGrid.params.lMax - occGrid.params.l0));

      // komórki z przewagą „zajęta” w kole (c, r), spełniające keep(x, y); waga = nadwyżka l
      function mapPoints(c, r, keep) {
        const g = occGrid, pts = [];
        const i1 = Math.min(g.cols - 1, g.colOf(c.x + r)), j1 = Math.min(g.rows - 1, g.rowOf(c.y + r));
        for (let j = Math.max(0, g.rowOf(c.y - r)); j <= j1; j++) {
          for (let i = Math.max(0, g.colOf(c.x - r)); i <= i1; i++) {
            const w = mapWeight(g.index(i, j));
            if (w <= 0) continue;
            const x = g.cellX(i), y = g.cellY(j);
            if (Math.hypot(x - c.x, y - c.y) <= r && (!keep || keep(x, y))) pts.push({ x, y, w });
          }
        }
        return pts;
      }

      // czy tor a-b (pas o pół szerokości CLEAR_CM) omija komórki pewnie zajęte
      function mapPathClear(a, b) {
        const g = occGrid, r = WALL.CLEAR_CM, lim = mapWallL();
        const i1 = Math.min(g.cols - 1, g.colOf(Math.max(a.x, b.x) + r)), j1 = Math.min(g.rows - 1, g.rowOf(Math.max(a.y, b.y) + r));
        for (let j = Math.max(0, g.rowOf(Math.min(a.y, b.y) - r)); j <= j1; j++) {
          for (let i = Math.max(0, g.colOf(Math.min(a.x, b.x) - r)); i <= i1; i++) {
            if (mapWeight(g.index(i, j)) >= lim && segDist({ x: g.cellX(i), y: g.cellY(j) }, a, b) <= r) return false;
          }
        }
        return true;
      }

      // transformata Hougha z wagami: najsilniejsza prosta (kierunki normalnej z listy angles [st.]);
      // wynik prostej to suma wag w pasie trzech komórek
      function houghPeak(pts, c, angles) {
        const cell = occGrid.cell;
        let maxR = 0;
        for (const p of pts) maxR = Math.max(maxR, Math.hypot(p.x - c.x, p.y - c.y));
        const off = Math.ceil(maxR / cell) + 2, acc = new Float32Array(2 * off + 1);
        let best = null;
        for (const a of angles) {
          const n = vDir(a);
          acc.fill(0);
          for (const p of pts) acc[Math.round(((p.x - c.x) * n.x + (p.y - c.y) * n.y) / cell) + off] += p.w;
          for (let k = 1; k < acc.length - 1; k++) {
            const score = acc[k - 1] + acc[k] + acc[k + 1];
            if (!best || score > best.score) best = { phi: a, rho: (k - off) * cell + vDot(c, n), score };
          }
        }
        return best;
      }

      // dokładniejsza prosta: regresja ortogonalna (z wagami) komórek leżących blisko prostej z transformaty;
      // łuk z jednego skanu jest symetryczny względem prostopadłej do ściany, więc daje właściwy kierunek.
      // Drugie przejście w węższym pasie pomija komórki odstające (np. narożnik na skraju okna)
      function refineLine(pts, line, band) {
        const fit = (l, bd) => {
          const n = vDir(l.phi), inl = pts.filter(p => Math.abs(vDot(p, n) - l.rho) <= bd);
          let sw = 0, mx = 0, my = 0;
          for (const p of inl) {
            sw += p.w;
            mx += p.w * p.x;
            my += p.w * p.y;
          }
          if (!sw) return l;
          mx /= sw;
          my /= sw;
          let sxx = 0, syy = 0, sxy = 0;
          for (const p of inl) {
            const dx = p.x - mx, dy = p.y - my;
            sxx += p.w * dx * dx;
            syy += p.w * dy * dy;
            sxy += p.w * dx * dy;
          }
          let phi = Math.atan2(2 * sxy, sxx - syy) * 90 / Math.PI + 90; // normalna do kierunku największego rozrzutu
          if (Math.abs(lineDiff(phi, line.phi)) > WALL.REFINE_MAX_DEG) phi = line.phi; // za mało punktów na regresję
          if (Math.abs(angDiff(phi, line.phi)) > 90) phi += 180; // zachowuje zwrot z transformaty
          const m = vDir(phi);
          return { phi, rho: mx * m.x + my * m.y };
        };
        return fit(fit(line, band), WALL.REFINE_BAND_CELLS * occGrid.cell);
      }

      // odcinki prostej: ciągi komórek pewnie zajętych z przerwami najwyżej GAP_CM; ev = suma wag
      function lineSegments(line, pts) {
        const lim = mapWallL(), t = wallAlong(line);
        const ss = pts.filter(p => p.w >= lim).map(p => ({ s: vDot(p, t), w: p.w })).sort((a, b) => a.s - b.s);
        const segs = [];
        let cur = null;
        for (const q of ss) {
          if (cur && q.s - cur.hi <= WALL.GAP_CM) {
            cur.hi = q.s;
            cur.ev += q.w;
          } else segs.push(cur = { phi: line.phi, rho: line.rho, lo: q.s, hi: q.s, ev: q.w });
        }
        return segs;
      }

      // ściany w kole (c, r): kolejne najsilniejsze proste (komórki prostej są usuwane przed szukaniem
      // następnej), zwrócone przodem do pojazdu w pozie pose; wynik to odcinki co najmniej minLen długie
      function mapFindWalls(c, r, keep, angles, maxLines, pose, minLen) {
        const band = WALL.INLIER_CELLS * occGrid.cell, out = [];
        let pts = mapPoints(c, r, keep);
        for (let k = 0; k < maxLines && pts.length; k++) {
          const peak = houghPeak(pts, c, angles);
          if (!peak || peak.score < WALL.MIN_EVIDENCE) break;
          const line = wallOrient(refineLine(pts, peak, band), pose), n = vDir(line.phi);
          const near = p => Math.abs(vDot(p, n) - line.rho) <= band;
          const inl = pts.filter(near);
          if (!inl.length) break;
          pts = pts.filter(p => !near(p));
          for (const s of lineSegments(line, inl)) if (s.hi - s.lo >= minLen && s.ev >= WALL.MIN_EVIDENCE) out.push(s);
        }
        return out;
      }

      const showWallStatus = t => $('#map-status').textContent = t;

      const WallFinder = {
        active: false, runId: 0,
        pose: null,
        waiter: null, // jedyne oczekiwanie automatu: { kind, resolve, reject, timer }
        motors: [0, 0], keepalive: 0,
        pulseMsPerDeg: WALL.PULSE_MS_PER_DEG, pulseDrop: 0, // czas i obniżenie PWM impulsu, dobierane w trakcie pracy
        scanState: null,
        moving: false, nearCount: 0, lastFront: null,
        heading: 0, refDist: 0, // kierunek na ścianę i ostatnia zmierzona odległość od niej
        wallDist: 0, // docelowa odległość od bieżącej ściany
        wall: null, // bieżąca ściana (odcinek)
        walls: [], // obchodzone już ściany { phi, rho, a, b, src } - a, b: końce odcinka, src: ściana przed zapisaniem
        marks: [],
        trace: null, // postęp obchodu ścian { turned, walls, path, n }; zachowany do wznowienia

        // resume: wznowienie po przerwie w połączeniu - obchód jest kontynuowany, jeśli już się zaczął
        start(resume = false) {
          this.active = true;
          this.pose = null;
          if (!resume) {
            this.marks = [];
            this.trace = null;
            this.wall = null;
            this.walls = [];
          }
          mapView.setMarks([...this.marks]);
          this.showWalls();
          this.run(++this.runId, resume && this.trace !== null);
        },

        stop(reason) {
          if (!this.active) return;
          this.active = false;
          this.moving = false;
          this.scanState = null;
          this.motors = [0, 0];
          clearInterval(this.keepalive);
          this.keepalive = 0;
          if (Transport.open) {
            Transport.send('W:RADAROFF');
            Transport.send('W:SERVO:90');
          }
          if (reason !== 'finished') showWallStatus(`Zatrzymano (${reason}).`);
          const w = this.waiter;
          this.waiter = null;
          if (w) {
            clearTimeout(w.timer);
            w.reject(WALL_ABORT);
          }
        },

        async run(id, resume) {
          let reason = 'finished';
          try {
            drive('S', true);
            await this.nextPose();
            if (resume) {
              // ponowne ustawienie do obchodzonej ściany (heading wskazuje na nią także po przerwie)
              showWallStatus('Wznowienie: ustawianie do ściany...');
              const previous = this.heading;
              await this.turnTo(previous);
              const traced = this.wall;
              if (!await this.faceWall(traced)) throw new Error('nie odnaleziono ściany po wznowieniu');
              this.wall = { ...this.wall, lo: Math.min(traced.lo, this.wall.lo), hi: Math.max(traced.hi, this.wall.hi) };
              this.showWalls();
              await this.adjustDistance();
              this.trace.turned += angDiff(this.heading, previous);
            } else {
              showWallStatus('Dojazd do przeszkody...');
              const found = await this.driveForward(this.pose.theta, WALL.SEARCH_MAX_CM, WALL.STOP_CM);
              if (!found.blocked) throw new Error('nie znaleziono przeszkody');
              if (!await this.findWall()) throw new Error('nie wykryto ściany');
              this.refDist = await this.measureFront();
              this.wallDist = Math.min(WALL.STOP_CM, this.refDist);
            }
            showWallStatus(await this.traceWalls());
          } catch (e) {
            if (e === WALL_ABORT) return;
            if (Transport.stale()) {
              reason = 'connection'; // brak odpowiedzi wynika z przerwy w danych, a nie z błędu automatu
            } else {
              showWallStatus('Błąd: ' + e.message + '.');
              reason = 'error';
            }
          }
          if (id === this.runId && this.active) {
            this.stop('finished');
            MapControl.setRunning(false, reason);
          }
        },

        // obchodzone już ściany i bieżąca ściana na mapie
        showWalls() {
          const ws = this.walls.map(w => ({ a: w.a, b: w.b, current: false }));
          if (this.wall && this.walls.at(-1)?.src !== this.wall) {
            const [a, b] = wallEnds(this.wall);
            ws.push({ a, b, current: true });
          }
          mapView.setWalls(ws);
        },

        // ---- zdarzenia z telemetrii ----
        onPose(p) {
          if (!this.active) return;
          this.pose = p;
          if (this.waiter && this.waiter.kind === 'pose') this.finishWait(p);
        },

        onMeasurement(m) {
          if (!this.active) return;
          if (this.scanState) this.onScanPoint(m);
          else if (m.servo === 90) {
            if (this.moving) {
              mapAddMeasurement(m);
              this.lastFront = m.dist;
              this.nearCount = m.dist != null && m.dist < this.moving.stopCm ? this.nearCount + 1 : 0;
            }
            if (this.waiter && this.waiter.kind === 'ping') this.finishWait(m);
          }
        },

        // skan kończy się na krańcu zakresu albo przy zawróceniu serwa (gdy zgubiono ostatni punkt)
        onScanPoint(m) {
          const s = this.scanState, last = s.pts.at(-1);
          if (last && m.servo === last.servo) return;
          if (!last || m.servo > last.servo) {
            s.pts.push(m);
            mapAddMeasurement(m);
            if (m.servo < s.to) return;
          }
          if (this.waiter && this.waiter.kind === 'scan') this.finishWait(s.pts);
        },

        // ---- oczekiwanie ----
        wait(kind, ms, timeoutMsg) {
          return new Promise((resolve, reject) => {
            const w = this.waiter = { kind, resolve, reject };
            w.timer = setTimeout(() => {
              this.waiter = null;
              if (timeoutMsg) reject(new Error(timeoutMsg));
              else resolve();
            }, ms);
          });
        },
        finishWait(v) {
          const w = this.waiter;
          this.waiter = null;
          clearTimeout(w.timer);
          w.resolve(v);
        },
        sleep(ms) { return this.wait('sleep', ms); },
        nextPose() { return this.wait('pose', WALL.DATA_TIMEOUT_MS, 'brak danych o pozycji'); },
        nextPing() { return this.wait('ping', WALL.DATA_TIMEOUT_MS, 'brak pomiarów sonaru'); },

        // ---- silniki ----
        resetPulse() {
          this.pulseMsPerDeg = WALL.PULSE_MS_PER_DEG;
          this.pulseDrop = 0;
        },
        setMotors(l, r) {
          this.motors = [clamp(Math.round(l), -255, 255), clamp(Math.round(r), -255, 255)];
          this.sendMotors();
          clearInterval(this.keepalive);
          this.keepalive = l || r ? setInterval(() => this.sendMotors(), WALL.KEEPALIVE_MS) : 0;
        },
        sendMotors() { Transport.send(`W:MOTOR:${this.motors[0]},${this.motors[1]}`, false); },
        async stopMotors() {
          this.setMotors(0, 0);
          await this.sleep(WALL.SETTLE_MS);
        },

        // ---- ruch ----
        // jazda z utrzymaniem kursu (dir = -1: tyłem); kończy się po maxCm albo przy przeszkodzie bliżej niż stopCm
        async driveForward(heading, maxCm, stopCm, dir = 1) {
          Transport.send('W:SERVO:90');
          await this.sleep(WALL.SERVO_MS);
          const a = await this.nextPing(), b = await this.nextPing();
          if (a.dist != null && b.dist != null && a.dist < stopCm && b.dist < stopCm) {
            this.lastFront = b.dist;
            return { blocked: true, traveled: 0 };
          }

          const start = this.pose;
          let boost = 0, best = 0, bestAt = performance.now(), traveled = 0, blocked = false, prevAt = bestAt;
          this.nearCount = 0;
          this.moving = { stopCm };
          const pwm = () => dir * Math.min(255, wallSpeeds.drive + boost);
          this.setMotors(pwm(), pwm());
          while (true) {
            const p = await this.nextPose();
            const now = performance.now(), last = traveled;
            traveled = Math.hypot(p.x - start.x, p.y - start.y);
            const speed = Math.max(0, traveled - last) / Math.max(.02, (now - prevAt) / 1000);
            prevAt = now;
            if (this.nearCount >= 2) blocked = true;
            if (blocked || traveled + speed * WALL.DRIVE_LAG_S >= maxCm - WALL.BRAKE_CM) break;

            if (traveled >= best + 1) {
              best = traveled;
              bestAt = now;
            } else if (now - bestAt > WALL.STALL_MS) {
              if (Math.abs(pwm()) >= 255) throw new Error('pojazd nie rusza z miejsca');
              boost += WALL.PWM_BOOST;
              bestAt = now;
            }
            const steer = clamp(angDiff(heading, p.theta) * WALL.STEER_GAIN, -WALL.STEER_MAX, WALL.STEER_MAX);
            this.setMotors(pwm() - steer, pwm() + steer);
          }
          this.moving = false;
          await this.stopMotors();
          return { blocked, traveled };
        },

        // obrót w miejscu do kursu target (dodatni kąt = w lewo): najpierw ciągły, a blisko celu krótkimi
        // impulsami, bo poza przychodzi zbyt rzadko, aby trafić w cel podczas ruchu
        async turnTo(target) {
          let boost = 0;
          const power = base => Math.min(255, base + boost);
          const stalled = () => {
            if (power(wallSpeeds.turnSlow) >= 255) throw new Error('pojazd nie może się obrócić');
            boost += WALL.PWM_BOOST;
          };

          await this.nextPose();
          let err = angDiff(target, this.pose.theta);
          if (Math.abs(err) > WALL.TURN_PULSE_DEG) {
            const dir = Math.sign(err);
            let prev = this.pose.theta, prevAt = performance.now(), moved = 0, movedAt = prevAt;
            const spin = e => {
              const pwm = power(Math.abs(e) > WALL.TURN_SLOW_DEG ? wallSpeeds.turn : wallSpeeds.turnSlow);
              this.setMotors(-dir * pwm, dir * pwm);
            };
            spin(err);
            while (true) {
              const p = await this.nextPose(), now = performance.now();
              const step = angDiff(p.theta, prev), rate = Math.abs(step) / Math.max(.02, (now - prevAt) / 1000);
              prev = p.theta;
              prevAt = now;
              err = angDiff(target, p.theta);
              if (err * dir <= rate * WALL.TURN_LAG_S + WALL.TURN_PULSE_DEG / 2) break;
              moved += Math.abs(step);
              if (moved >= 1) {
                moved = 0;
                movedAt = now;
              } else if (now - movedAt > WALL.STALL_MS) {
                stalled();
                movedAt = now;
              }
              spin(err);
            }
            await this.stopMotors();
          }

          for (let n = 0; n < WALL.TURN_TRIES; n++) {
            await this.nextPose();
            err = angDiff(target, this.pose.theta);
            if (Math.abs(err) <= WALL.TURN_TOL_DEG) return;
            const dir = Math.sign(err), pwm = Math.max(SPEED_MIN, power(wallSpeeds.turnSlow) - this.pulseDrop);
            const ms = clamp(Math.abs(err) * this.pulseMsPerDeg, WALL.PULSE_MIN_MS, WALL.PULSE_MAX_MS);
            this.setMotors(-dir * pwm, dir * pwm);
            await this.sleep(ms);
            await this.stopMotors();
            await this.nextPose();
            const gain = (angDiff(this.pose.theta, target) + err) * dir; // obrót wykonany w stronę celu
            if (gain < .5) {
              if (this.pulseDrop > 0) this.pulseDrop = Math.max(0, this.pulseDrop - WALL.PWM_BOOST);
              else stalled();
            } else {
              this.pulseMsPerDeg = clamp(this.pulseMsPerDeg * clamp(Math.abs(err) / gain, .6, 1.4), 2, 60);
              // nawet najkrótszy impuls przeskakuje cel: trzeba go osłabić
              if (ms <= WALL.PULSE_MIN_MS && gain > Math.abs(err) + WALL.TURN_TOL_DEG) this.pulseDrop += WALL.PWM_BOOST;
            }
          }
          throw new Error('nie udało się ustawić kursu');
        },

        // przejazd przez kolejne punkty trasy; false, gdy przeszkoda spoza mapy przerwała jazdę
        async follow(route) {
          for (const q of route) {
            const p = this.pose, d = vDist(p, q);
            if (d <= WALL.DIST_TOL_CM) continue;
            const course = Math.atan2(q.y - p.y, q.x - p.x) * 180 / Math.PI;
            await this.turnTo(course);
            const r = await this.driveForward(course, d, WALL.BLOCK_CM);
            this.trace.path += r.traveled;
            if (r.blocked) return false;
          }
          return true;
        },

        // ---- skan ----
        // jedno przejście radaru od from do to (zawsze w tę stronę); pomiary trafiają na mapę
        async scan(from, to) {
          Transport.send('W:RADAROFF');
          Transport.send('W:SERVO:' + from);
          await this.sleep(WALL.SWEEP_MS);
          this.scanState = { to, pts: [] };
          Transport.send(`W:RADAR:${from},${to},${WALL.SCAN_STEP},${WALL.SCAN_INTERVAL_MS}`);
          const count = (to - from) / WALL.SCAN_STEP + 1;
          const pts = await this.wait('scan', count * (WALL.SCAN_INTERVAL_MS + 150) + 3000, 'skan nie został ukończony');
          this.scanState = null;
          Transport.send('W:RADAROFF');
          return pts;
        },

        // full: pełny zakres serwa, inaczej sektor przy obchodzie (kierunek jazdy i ściana);
        // zwraca odległość na wprost (mediana pomiarów blisko osi) albo null
        async scanMap(full) {
          const back = 90 + WALL.TRACE_BACK_DEG * (WALL.SIDE < 0 ? 1 : -1);
          const [from, to] = full ? [SERVO_MIN, SERVO_MAX] : WALL.SIDE < 0 ? [SERVO_MIN, back] : [back, SERVO_MAX];
          const pts = await this.scan(from, to);
          const ds = pts.filter(m => Math.abs(m.servo - 90) <= WALL.FRONT_DEG && usableDist(m.dist)).map(m => m.dist);
          return ds.length >= 2 ? median(ds) : null;
        },

        // ---- ściany na mapie ----
        // bieżąca ściana w oknie wokół rzutu pojazdu na nią, blisko przybliżenia prior; zwraca odcinek
        // najbliższy pojazdowi albo null
        fitWall(prior) {
          const pose = this.pose, sr = wallS(prior, pose), n = vDir(prior.phi);
          const keep = (x, y) => Math.abs(x * n.x + y * n.y - prior.rho) <= WALL.FIT_BAND_CM;
          const tol = WALL.FIT_ANG_TOL_DEG, angles = Array.from({ length: 2 * tol + 1 }, (_, k) => prior.phi - tol + k);
          const segs = mapFindWalls(wallPoint(prior, sr), WALL.FIT_ALONG_CM, keep, angles, 1, pose, 0);
          if (!segs.length || Math.abs(wallGap(segs[0], pose) - wallGap(prior, pose)) > WALL.FIT_DIST_TOL_CM) return null;
          const off = w => { const s = wallS(w, pose); return s < w.lo ? w.lo - s : s > w.hi ? s - w.hi : 0; };
          return segs.reduce((b, w) => off(w) < off(b) ? w : b);
        },

        // ściana przed pojazdem: najlepszy odcinek, przy czym liczą się suma wag i bliskość
        wallAhead() {
          const p = this.pose, h = vDir(p.theta), s = sensorPos(p);
          const keep = (x, y) => (x - p.x) * h.x + (y - p.y) * h.y > 0;
          let best = null;
          for (const w of mapFindWalls(s, WALL.AHEAD_CM, keep, WALL_ANGLES, WALL.CAND_LINES, p, WALL.WALL_MIN_CM)) {
            const score = w.ev * Math.exp(-segDist(s, ...wallEnds(w)) / WALL.NEXT_DIST_CM);
            if (!best || score > best.score) best = { w, score };
          }
          return best && best.w;
        },

        // filtr komórek: pomija obchodzone już ściany i extra (pierwsza ściana zostaje widoczna po obejściu
        // co najmniej trzech, aby dało się ją rozpoznać na końcu obiegu)
        unmasked(extra = []) {
          const ws = [...(this.walls.length >= 3 ? this.walls.slice(1) : this.walls), ...extra];
          return (x, y) => !ws.some(w => segDist({ x, y }, w.a, w.b) <= WALL.EXCLUDE_CM);
        },

        // ściana w poprzek kierunku jazdy przed pojazdem (narożnik wklęsły); { wall, u } - u: odległość
        // osi pojazdu od niej wzdłuż kierunku jazdy
        cornerAhead() {
          const w = this.wall, p = this.pose, t = wallAlong(w), [a, b] = wallEnds(w);
          const keep = this.unmasked([{ a, b }]);
          let best = null;
          for (const c of mapFindWalls(vAdd(p, t, WALL_AXIS_CM), WALL.CORNER_WINDOW_CM, keep, WALL_ANGLES, WALL.CAND_LINES, p, WALL.WALL_MIN_CM)) {
            const tn = vDot(t, vDir(c.phi));
            if (tn < Math.SQRT1_2) continue; // ściana musi stać w poprzek toru (do 45 st.)
            const u = wallGap(c, p) / tn, s = wallS(c, vAdd(p, t, u));
            if (u > WALL.STEP_MAX_CM + WALL_AXIS_CM || s < c.lo - WALL.CORNER_SEG_CM || s > c.hi + WALL.CORNER_SEG_CM) continue;
            if (!best || u < best.u) best = { wall: c, u };
          }
          return best;
        },

        // kandydaci na kolejną ścianę wokół końca E poprzedniej ściany prev, od najbardziej prawdopodobnego:
        // suma wag x kara za odległość od E x zgodność z kształtem pomieszczenia (narożnik, uskok)
        rankNext(prev, E) {
          const t = wallAlong(prev), sE = vDot(E, t);
          const weight = c => {
            const d = Math.abs(lineDiff(c.phi, prev.phi));
            return d >= 70 ? 1 : d <= 20 ? .7 : .3;
          };
          return mapFindWalls(E, WALL.NEXT_RADIUS_CM, this.unmasked(), WALL_ANGLES, WALL.CAND_LINES, this.pose, WALL.WALL_MIN_CM)
            .map(c => ({ c, ends: wallEnds(c) }))
            .filter(({ ends }) => Math.max(vDot(ends[0], t), vDot(ends[1], t)) >= sE - WALL.NEXT_BEHIND_CM)
            .map(({ c, ends }) => ({ c, score: c.ev * Math.exp(-segDist(E, ...ends) / WALL.NEXT_DIST_CM) * weight(c) }))
            .sort((a, b) => b.score - a.score)
            .map(x => x.c);
        },

        // kandydat c to pierwsza ściana obiegu: 'closed', gdy koniec E poprzedniej ściany leży przed jej obejściem
        // fragmentem (pojazd dochodzi do początku obiegu), 'skip' w innym miejscu (już obchodzona), inaczej null
        firstWall(c, E) {
          const f = this.walls[0];
          if (this.walls.length < 3 || !wallSameLine(c, f)) return null;
          return wallS(f, E) <= wallS(f, f.a) + WALL.CLOSE_GAP_CM ? 'closed' : 'skip';
        },

        // trasa do miejsca przed ścianą c (WALL_AXIS_CM od niej i od narożnika z poprzednią ścianą prev);
        // najpierw wprost, potem przez punkt pośredni; null, gdy na mapie każda trasa jest zajęta
        planRoute(c, prev, E) {
          const [A, B] = wallEnds(c), len = c.hi - c.lo;
          let X = Math.abs(Math.sin(rad(angDiff(c.phi, prev.phi)))) > .5 ? wallCross(c, prev) : null;
          if (X && segDist(X, A, B) > 2 * WALL.CORNER_SEG_CM) X = null;
          const corner = X !== null;
          if (!corner) X = vDist(A, E) <= vDist(B, E) ? A : B;
          // od narożnika (albo bliższego końca) w głąb ściany
          const mid = vAdd(A, vAdd(B, A, -1), .5), dl = vDist(mid, X);
          let dir = vAdd(mid, X, -1);
          if (dl < 1) {
            const t = vDir(c.phi + 90), k = vDot(t, vDir(prev.phi)) > 0 ? -1 : 1;
            dir = vAdd({ x: 0, y: 0 }, t, k);
          } else dir = vAdd({ x: 0, y: 0 }, dir, 1 / dl);
          const Q = vAdd(X, dir, corner ? WALL_AXIS_CM : Math.min(WALL_AXIS_CM, len / 2));
          const P = vAdd(Q, vDir(c.phi), -WALL_AXIS_CM);

          const p = this.pose, t = wallAlong(prev), k = vDot(vAdd(P, p, -1), t);
          for (const route of [[P], [vAdd(p, t, k), P], [vAdd(P, t, -k), P]]) {
            let a = p;
            if (route.every(q => { const ok = mapPathClear(a, q); a = q; return ok; })) return route;
          }
          return null;
        },

        // ---- etapy algorytmu ----
        // skan i ustawienie przodem do ściany (prior: przybliżenie, null: ściana przed pojazdem);
        // ustawia heading i bieżącą ścianę, zwraca ją albo null
        async faceWall(prior) {
          for (let n = 0; ; n++) {
            showWallStatus('Skan i szukanie ściany...');
            await this.scanMap(true);
            const w = prior ? this.fitWall(prior) : this.wallAhead();
            if (!w) return null;
            const off = Math.abs(angDiff(w.phi, this.pose.theta));
            await this.turnTo(w.phi);
            if (off > WALL.FACE_RESCAN_DEG && n < WALL.FACE_RESCANS) { // ściana mogła nie zmieścić się w skanie
              prior = w;
              continue;
            }
            this.heading = w.phi;
            this.wall = w;
            this.showWalls();
            return w;
          }
        },

        // ściana przed pojazdem albo - gdy jej nie ma - po obrocie w jedną lub drugą stronę
        async findWall() {
          const base = this.pose.theta;
          for (const turn of [0, WALL.SEARCH_TURN_DEG, -WALL.SEARCH_TURN_DEG]) {
            if (turn) await this.turnTo(base + turn);
            if (await this.faceWall(null)) return true;
          }
          return false;
        },

        // odległość na wprost z kilku pomiarów na postoju
        async measureFront() {
          Transport.send('W:SERVO:90');
          await this.sleep(WALL.SERVO_MS);
          const ds = [];
          for (let n = 0; n < 3 * WALL.FRONT_PINGS && ds.length < WALL.FRONT_PINGS; n++) { // zapas na brak echa
            const m = await this.nextPing();
            if (usableDist(m.dist)) ds.push(m.dist);
          }
          if (ds.length >= 3) return median(ds);
          // brak echa (np. ściana nie dokładnie prostopadła do wiązki): odległość wynikająca z mapy
          const d = this.wall && wallGap(this.wall, this.pose) - WALL.SENSOR_OFFSET_CM;
          if (!d || d <= 0 || d > WALL.MAX_RANGE_CM) throw new Error('brak pomiaru ściany na wprost');
          Dashboard.logSent(`Mapowanie: brak echa na wprost, odległość z mapy ${d.toFixed(0)} cm`);
          return d;
        },

        // podjazd lub odsunięcie, aż odległość od ściany będzie równa wallDist
        async adjustDistance() {
          showWallStatus('Ustawianie odległości od ściany...');
          for (let n = 0; ; n++) {
            this.refDist = await this.measureFront();
            const delta = this.refDist - this.wallDist;
            if (Math.abs(delta) <= WALL.DIST_TOL_CM || n >= WALL.DIST_TRIES) return;
            if (delta > 0) await this.driveForward(this.heading, delta, Math.min(WALL.BLOCK_CM, this.wallDist - WALL.DIST_TOL_CM));
            else await this.driveForward(this.heading, -delta, 0, -1);
            await this.turnTo(this.heading);
          }
        },

        // narożnik wypukły: odjazd w bok od krawędzi, minięcie jej i obrót przodem do bocznej ściany;
        // zwraca przejechaną drogę. Przeszkoda po drodze przerywa manewr
        async roundCorner() {
          const side = WALL.SIDE, course = this.heading + side * 90, ahead = this.heading;
          await this.turnTo(course);
          const away = await this.driveForward(course, WALL.STOP_CM, WALL.BLOCK_CM);
          if (away.blocked) return away.traveled;
          await this.turnTo(ahead);
          const past = this.refDist + WALL.SENSOR_OFFSET_CM + WALL.CORNER_PAST_CM;
          const pass = await this.driveForward(ahead, past, WALL.BLOCK_CM);
          if (!pass.blocked) await this.turnTo(ahead - side * 90);
          return away.traveled + pass.traveled;
        },

        // koniec bieżącej ściany: zapisuje ją i przechodzi do kolejnej (forced: już wybranej);
        // zwraca nową ścianę, 'closed' po rozpoznaniu pierwszej ściany albo null, gdy kolejnej nie ma.
        // Gdy mapa nie wskazuje osiągalnej ściany, kolejne próby zbierają więcej danych.
        async nextWall(forced) {
          const prev = this.wall, [S, E] = wallEnds(prev), base = this.heading;
          this.walls.push({ phi: prev.phi, rho: prev.rho, a: S, b: E, src: prev });
          this.marks.push(E);
          mapView.setMarks([...this.marks]);
          this.showWalls();
          const looks = [
            async () => { // skan po przekątnej: koniec ściany, kierunek jazdy i ewentualna ściana na wprost
              await this.turnTo(base + WALL.SIDE * 45);
              await this.scanMap(true);
            },
            async () => { // narożnik wypukły: kolejna ściana jest za krawędzią, poza zasięgiem sonaru
              if (vDist(this.pose, E) > WALL.NEXT_RADIUS_CM) return;
              await this.turnTo(base);
              this.trace.path += await this.roundCorner();
              await this.scanMap(true);
            },
            async () => {
              await this.turnTo(base + WALL.SEARCH_TURN_DEG);
              await this.scanMap(true);
            },
            async () => {
              await this.turnTo(base - WALL.SEARCH_TURN_DEG);
              await this.scanMap(true);
            },
          ];
          let cands = forced ? [forced] : [];
          for (let k = -1; k < looks.length; k++) {
            if (k >= 0) {
              showWallStatus(`Szukanie kolejnej ściany (${k + 1}/${looks.length})...`);
              await looks[k]();
              cands = this.rankNext(prev, E);
            }
            for (const c of cands.slice(0, WALL.NEXT_TRIES)) {
              const first = this.firstWall(c, E);
              if (first === 'closed') return 'closed';
              if (first) continue;
              const route = this.planRoute(c, prev, E);
              if (!route) continue;
              showWallStatus('Przejazd do kolejnej ściany...');
              if (!await this.follow(route)) break; // przeszkoda poza mapą: kolejny skan
              await this.turnTo(c.phi);
              const w = await this.faceWall(c);
              if (w) return w;
            }
          }
          return null;
        },

        // obchodzi kolejne ściany; zwraca opis zakończenia. Krok wzdłuż ściany sięga tak daleko, jak
        // ściana jest widoczna na mapie, i kończy się skanem, który poprawia jej dopasowanie
        async traceWalls() {
          // postęp jest w this.trace, aby obchód dało się kontynuować po wznowieniu
          if (!this.trace) this.trace = { turned: 0, walls: 1, path: 0, n: 0 };
          const t = this.trace;
          const summary = s => `Zakończono: ${s}; narożniki: ${this.marks.length}, droga: ${(t.path / 100).toFixed(1)} m.`;

          while (true) {
            if (t.walls > WALL.MAX_WALLS || t.path >= WALL.MAX_PATH_CM) return summary('osiągnięto limit obchodu');
            showWallStatus(`Ściana ${t.walls}, krok ${++t.n}...`);
            const sr = wallS(this.wall, this.pose);
            let step = clamp(this.wall.hi - sr - WALL.STEP_MARGIN_CM, WALL.STEP_MIN_CM, WALL.STEP_MAX_CM), next = null;
            const corner = this.cornerAhead();
            if (corner) {
              if (corner.u - WALL_AXIS_CM <= WALL.STEP_MIN_CM) next = corner.wall;
              else step = Math.min(step, corner.u - WALL_AXIS_CM);
            }

            if (!next) {
              const course = this.heading + WALL.SIDE * 90;
              await this.turnTo(course);
              const r = await this.driveForward(course, step, WALL.BLOCK_CM);
              t.path += r.traveled;
              if (!r.blocked) {
                await this.turnTo(this.heading);
                const front = await this.scanMap(false);
                // ściana trwa, gdy na mapie sięga pojazdu, a sonar na wprost widzi ją tam, gdzie wskazuje mapa
                // (echa od krawędzi potrafią przedłużyć ścianę na mapie poza jej koniec)
                const w = this.fitWall(this.wall), s = w && wallS(w, this.pose);
                if (w && s >= w.lo - WALL.COVER_CM && s <= w.hi + WALL.COVER_CM && front != null
                    && Math.abs(front - (wallGap(w, this.pose) - WALL.SENSOR_OFFSET_CM)) <= WALL.SAME_TOL_CM) {
                  this.wall = { ...w, lo: Math.min(this.wall.lo, w.lo), hi: Math.max(this.wall.hi, w.hi) };
                  this.showWalls();
                  const before = this.heading;
                  this.heading = w.phi;
                  if (Math.abs(angDiff(w.phi, before)) > WALL.ALIGN_TOL_DEG) await this.turnTo(w.phi);
                  t.turned += angDiff(w.phi, before);
                  this.refDist = front;
                  if (Math.abs(front - this.wallDist) > WALL.DIST_TOL_CM) await this.adjustDistance();
                  continue;
                }
              }
            }

            const previous = this.heading, res = await this.nextWall(next);
            if (res === 'closed') return summary('pełny obieg');
            if (!res) return summary('brak kolejnej ściany');
            this.wallDist = WALL.STOP_CM;
            await this.adjustDistance();
            t.turned += angDiff(this.heading, previous);
            t.walls++;
            if (Math.abs(t.turned) >= WALL.CLOSE_MAX_TURN_DEG) return summary('pełny obieg');
          }
        },
      };

      // MAPA: HANDLERY - łączą przyciski i telemetrię z wykrywaniem ściany
      // Dostępne elementy siatki:
      //   occGrid.colOf(x), rowOf(y)         - komórka zawierająca punkt [cm]
      //   occGrid.cellX(i), cellY(j)         - środek komórki [cm]
      //   occGrid.inBounds(i, j)             - czy komórka leży na mapie
      //   occGrid.markOccupied(i, j)         - dodaje l_occ do komórki (równ. 3.6, z ograniczeniem)
      //   occGrid.markFree(i, j)             - dodaje l_free do komórki (równ. 3.6, z ograniczeniem)
      //   occGrid.logOdds(i, j), probability(i, j) - odczyt komórki
      //   occGrid.params                     - aktualne { lOcc, lFree, l0, lMin, lMax }
      //   Transport.send('W:...')            - komenda do pojazdu
      // Zmienione komórki pojawiają się na mapie same, bez wywoływania rysowania.
      const MapHandlers = {
        // użytkownik uruchomił mapowanie albo jest ono wznawiane po przerwie w połączeniu (resume = true)
        onStart(resume) {
          WallFinder.start(resume);
        },

        // mapowanie zatrzymane, silniki dostały już komendę stop
        // reason: 'user', 'tab' (zmiana zakładki), 'connection', 'calibration', 'hidden' (ukryta karta),
        // 'poseReset', 'finished' (obie granice znalezione), 'error'
        onStop(reason) {
          WallFinder.stop(reason);
        },

        // pomiar sonaru podczas mapowania; poza (x, y, theta) pochodzi z chwili pomiaru
        // m = { servo [st., 90 = przód], dist [cm] albo null przy braku echa, x [cm], y [cm], theta [st.] }
        onMeasurement(m) {
          WallFinder.onMeasurement(m);
        },

        // bieżąca poza pojazdu podczas mapowania: p = { x [cm], y [cm], theta [st.] }
        onPose(p) {
          WallFinder.onPose(p);
        },

        // użytkownik wyczyścił mapę (occGrid jest już wypełniona wartością l_0)
        onClear() {
        },

        // użytkownik zmienił parametry l (occGrid używa już nowych wartości)
        onParamsChange(params) {
        },

        // użytkownik wyzerował pozycję: kolejne pomiary będą w nowym układzie współrzędnych
        onPoseReset() {
          MapControl.setRunning(false, 'poseReset');
        },
      };

      // LOGIKA: MAPA
      const MapControl = {
        running: false,
        resumePending: false, // mapowanie przerwane utratą danych - zostanie wznowione, gdy wrócą
        resumeFrom: null, stoppedAt: 0, // poza i czas w chwili przerwania
        lastPose: null, lastPoseAt: 0,

        setRunning(on, reason = 'user', resume = false) {
          if (!on && reason !== 'connection') this.resumePending = false; // każde inne zatrzymanie anuluje wznowienie
          if (on === this.running) return;
          this.running = on;
          showMapRun(on);
          if (on) {
            this.resumePending = false;
            MapHandlers.onStart(resume);
            return;
          }
          if (Transport.open) drive('S', true); // pojazd nie może jechać dalej bez nadzoru strony
          MapHandlers.onStop(reason);
          if (reason === 'connection') {
            this.resumePending = true;
            this.resumeFrom = this.lastPose;
            this.stoppedAt = performance.now();
            showWallStatus('Przerwa w połączeniu z pojazdem - mapowanie zostanie wznowione.');
          }
        },

        // wznawia mapowanie przerwane utratą danych, gdy dane znów płyną, a poza nie zmieniła się podczas przerwy
        tryResume() {
          // dane muszą wrócić po przerwie (dataSince po stoppedAt) i nieść już nową pozę
          if (!this.resumePending || !Transport.steadyFor(MAP_RESUME_DELAY_MS)) return;
          if (Transport.dataSince <= this.stoppedAt || this.lastPoseAt < Transport.dataSince) return;
          const a = this.resumeFrom, b = this.lastPose;
          if (!a || Math.hypot(b.x - a.x, b.y - a.y) > MAP_RESUME_MAX_SHIFT_CM
              || Math.abs(angDiff(b.theta, a.theta)) > MAP_RESUME_MAX_TURN_DEG) {
            this.resumePending = false;
            showWallStatus('Nie wznowiono mapowania: poza pojazdu zmieniła się podczas przerwy.');
            return;
          }
          Dashboard.logSent('Mapowanie: wznowienie po przerwie w połączeniu');
          this.setRunning(true, 'user', true);
        },

        pose(p) {
          this.lastPose = p;
          this.lastPoseAt = performance.now();
          mapView.setPose(p);
          if (this.running) MapHandlers.onPose(p);
        },

        measurement(m) {
          if (this.running) MapHandlers.onMeasurement(m);
        },
      };

      $('#map-run').onclick = () => {
        if (MapControl.running) MapControl.setRunning(false);
        else if (!Transport.open) Dashboard.logSent('Mapowanie: brak połączenia');
        else if (!Transport.calibrating) MapControl.setRunning(true);
      };
      $('#map-clear').onclick = () => {
        if (occGrid.updates && !confirm('Wyczyścić całą mapę?')) return;
        occGrid.clear();
        MapHandlers.onClear();
      };
      $('#map-save').onclick = () => {
        mapView.syncImage();
        const c = document.createElement('canvas'), ctx = c.getContext('2d');
        c.width = occGrid.cols * MAP_PNG_SCALE;
        c.height = occGrid.rows * MAP_PNG_SCALE;
        ctx.imageSmoothingEnabled = false;
        ctx.drawImage(mapView.off, 0, 0, c.width, c.height);
        c.toBlob(blob => {
          const a = document.createElement('a');
          a.href = URL.createObjectURL(blob);
          a.download = `mapa_${stamp(new Date())}.png`;
          a.click();
          setTimeout(() => URL.revokeObjectURL(a.href), 1000);
        });
      };

      // parametry l: suwak i pole liczbowe edytują wspólny szkic, a mapa przyjmuje go po zatwierdzeniu
      const mapDraft = { ...occGrid.params };

      function applyMapDraft() {
        showMapParams(mapDraft);
        if (occGrid.setParams(mapDraft)) MapHandlers.onParamsChange({ ...occGrid.params });
      }

      $$('[data-l]').forEach(el => {
        const k = el.dataset.l;
        el.addEventListener('input', () => {
          mapDraft[k] = el.value === '' ? NaN : el.type === 'range' ? mapSnap(+el.value) : +el.value;
          showMapParams(mapDraft, el);
        });
        el.addEventListener('change', () => {
          const v = el.value === '' ? occGrid.params[k] : +el.value;
          mapDraft[k] = mapSnap(clamp(v, +el.min, +el.max));
          applyMapDraft();
        });
      });
      $('#map-l-default').onclick = () => {
        Object.assign(mapDraft, MAP_L_DEFAULT);
        applyMapDraft();
      };
      showMapParams(mapDraft);

      // prędkości automatu; obrót dokładny nie może być szybszy od zwykłego
      function showWallSpeeds() {
        for (const el of $$('[data-speed]')) {
          el.value = wallSpeeds[el.dataset.speed];
          if (el.type === 'range') paintRange(el);
        }
      }

      function setWallSpeed(key, value) {
        const old = wallSpeeds.turnSlow;
        wallSpeeds[key] = value;
        if (key === 'turn') wallSpeeds.turnSlow = Math.min(wallSpeeds.turnSlow, value);
        if (key === 'turnSlow') wallSpeeds.turn = Math.max(wallSpeeds.turn, value);
        if (wallSpeeds.turnSlow !== old) WallFinder.resetPulse(); // inna reakcja na impuls
        showWallSpeeds();
      }

      $$('[data-speed]').forEach(el => {
        el.min = SPEED_MIN;
        el.max = SPEED_MAX;
        el.addEventListener('input', () => {
          for (const o of $$(`[data-speed=${el.dataset.speed}]`)) {
            if (o === el) continue;
            o.value = el.value;
            if (o.type === 'range') paintRange(o);
          }
        });
        el.addEventListener('change', () => {
          const v = el.value === '' ? wallSpeeds[el.dataset.speed] : Math.round(+el.value);
          setWallSpeed(el.dataset.speed, clamp(v, SPEED_MIN, SPEED_MAX));
        });
      });
      $('#map-speed-default').onclick = () => {
        Object.assign(wallSpeeds, WALL_SPEED_DEFAULT);
        WallFinder.resetPulse();
        showWallSpeeds();
      };
      showWallSpeeds();

      // widok mapy: przeciąganie, kółko myszy i przyciski
      const mapCv = mapView.cv;
      const mapPointer = e => {
        const r = mapCv.getBoundingClientRect();
        return [e.clientX - r.left, e.clientY - r.top];
      };
      let mapDrag = null;
      mapCv.addEventListener('pointerdown', e => {
        mapCv.setPointerCapture(e.pointerId);
        mapCv.classList.add('drag');
        mapDrag = mapPointer(e);
      });
      mapCv.addEventListener('pointermove', e => {
        const [px, py] = mapPointer(e);
        if (mapDrag) {
          mapView.setFollow(false);
          mapView.moveTo(mapView.vx - (px - mapDrag[0]) / mapView.zoom, mapView.vy + (py - mapDrag[1]) / mapView.zoom);
          mapDrag = [px, py];
        }
        mapView.setHover(px, py);
      });
      for (const type of ['pointerup', 'pointercancel']) {
        mapCv.addEventListener(type, () => {
          mapDrag = null;
          mapCv.classList.remove('drag');
        });
      }
      mapCv.addEventListener('pointerleave', () => mapView.setHover(null));
      mapCv.addEventListener('wheel', e => {
        e.preventDefault();
        mapView.zoomBy(e.deltaY < 0 ? MAP_ZOOM_STEP : 1 / MAP_ZOOM_STEP, ...mapPointer(e));
      }, { passive: false });
      $('#map-zoom-in').onclick = () => mapView.zoomBy(MAP_ZOOM_STEP);
      $('#map-zoom-out').onclick = () => mapView.zoomBy(1 / MAP_ZOOM_STEP);
      $('#map-fit').onclick = () => mapView.fit();
      $('#map-follow').onclick = () => mapView.setFollow(!mapView.follow);

      // PUBLICZNE API
      window.Dashboard = {
        updateTelemetry,
        setConnection,
        logSent: t => pushLog('sent', t),
        logReceived: t => pushLog('recv', t),
        setCamera,
        setSnapshot,
        setMode,
        radar,
        occGrid,
        mapView,
        mapHandlers: MapHandlers,
      };

      // START
      radarLoop();
      mapLoop();
      Transport.connect();
      setInterval(() => {
        Transport.checkData();
        MapControl.tryResume();
      }, 500);
    </script>
  </body>
</html>
)rawliteral";

esp_err_t pageHandler(httpd_req_t* req) {
  cameraSetCommonHeaders(req);
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, PAGE_HTML, sizeof(PAGE_HTML) - 1); // bez kończącego '\0'
}
