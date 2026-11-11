const ESP_HOST = "192.168.4.1";
const WS_URL = `ws://${ESP_HOST}:80/`;
const CAPTURE_URL = `http://${ESP_HOST}:81/capture`;
const SHOT_TIMEOUT_MS = 8000; // po tylu ms uznajemy zdjęcie za nieudane
const SLIDER_THROTTLE_MS = 50;
const MOTOR_REPEAT_MS = 200; // ponawianie polecenia jazdy (Arduino zatrzymuje silniki po 1s bez polecenia)
const RADAR_FADE_MS = 10000; // po tylu ms punkt radaru jest najbardziej wygaszony
const RADAR_REDRAW_MS = 100;
const RADAR_BEAM_DEG = 30; // szerokość wiązki HC-SR04 - przeszkoda może leżeć w dowolnym miejscu łuku

const $ = (id) => document.getElementById(id);

let ws = null;
let lastFrameTime = null;

function log(text) {
  const el = $("log");
  const lines = el.textContent.split("\n").slice(0, 19);
  el.textContent = `${new Date().toLocaleTimeString()} ${text}\n` + lines.join("\n");
}

// WebSocket
function setStatus(text) {
  $("connection-status").textContent = text;
}

function connect() {
  ws = new WebSocket(WS_URL);
  ws.onopen = () => { setStatus("połączono"); log("WS: połączono"); };
  ws.onclose = () => { setStatus("rozłączono, ponawiam..."); setTimeout(connect, 1000); };
  ws.onerror = () => { ws.close(); };
  ws.onmessage = (ev) => onLine(String(ev.data));
}

// wysyłanie komend do autka
function sendCmd(cmd) {
  if (!ws || ws.readyState !== WebSocket.OPEN) return false;
  ws.send(cmd);
  $("lastCmd").textContent = cmd;
  return true;
}

// odbiór danych z autka
function parseData(line) {
  if (!line.startsWith("D:")) return null;
  const p = line.slice(2).split(",").map(Number);
  if (p.length !== 11 || p.some(Number.isNaN)) return null;
  const [enL, enR, sonar, ax, ay, az, gx, gy, gz, mL, mR] = p;
  return { enL, enR, sonar, ax, ay, az, gx, gy, gz, mL, mR };
}

function showData(t) {
  $("enL").textContent = t.enL;
  $("enR").textContent = t.enR;
  $("sonar").textContent = t.sonar === 0 ? "brak echa / brak pomiaru" : t.sonar.toFixed(1) + " cm";
  for (const k of ["ax", "ay", "az", "gx", "gy", "gz"]) {
    $(k).textContent = t[k].toFixed(2);
  }
  $("mL").textContent = t.mL;
  $("mR").textContent = t.mR;
}

function parsePosition(line) {
  if (!line.startsWith("P:")) return null;
  const p = line.slice(2).split(",").map(Number);
  if (p.length !== 4|| p.some(Number.isNaN)) return null;
  const [x, y, heading, vel] = p;
  return { x, y, heading, vel };
}

function showPosition(p) {
  $("posX").textContent = p.x.toFixed(1);
  $("posY").textContent = p.y.toFixed(1);
  $("heading").textContent = p.heading.toFixed(1);
  $("vel").textContent = p.vel.toFixed(1);
}

function parseRadar(line) {
  if (!line.startsWith("R:")) return null;
  const p = line.slice(2).split(",").map(Number);
  if (p.length !== 5 || p.some(Number.isNaN)) return null;
  const [angle, dist, x, y, heading] = p;
  return { angle, dist, x, y, heading };
}

function onLine(line) {
  line = line.trim();
  if (!line) return;
  const t = parseData(line);
  const p = parsePosition(line);
  const r = parseRadar(line);
  if (t) {
    showData(t);
    lastFrameTime = Date.now();
  } else if (p) {
    showPosition(p);
  } else if (r) {
    showRadar(r);
  } else {
    log("RX (nierozpoznane): " + line);
  }
}

setInterval(() => {
  $("last-received-data").textContent = lastFrameTime === null
    ? "brak"
    : ((Date.now() - lastFrameTime) / 1000).toFixed(2) + " s temu";
}, 10);

// serwo
const servo = $("servo");
let lastServoSend = 0;

function sendServo(force) {
  const now = Date.now();
  if (!force && now - lastServoSend < SLIDER_THROTTLE_MS) return;
  lastServoSend = now;
  setRadarRunning(false); // ręczne ustawienie serwa wyłącza radar na Arduino
  sendCmd(`C:S,${parseInt(servo.value, 10)}`);
}

servo.addEventListener("input", () => {
  $("servoVal").textContent = servo.value;
  sendServo(false);
});
servo.addEventListener("change", () => sendServo(true));

// sonar
$("btnTrig").onclick = () => {
  setRadarRunning(false); // ręczny pomiar wyłącza radar na Arduino
  sendCmd("C:TRIG");
};

// pozycja
$("btnResetPos").onclick = () => sendCmd("C:RESETPOS");

// silniki
const speed = $("speed");
let driveDir = null; // aktualny kierunek jazdy (null - stoi)
let driveTimer = null;

// mnożniki prędkości [lewe, prawe] dla kierunków (skręt w miejscu)
const DIRS = {
  forward: [1, 1],
  back: [-1, -1],
  left: [-1, 1],
  right: [1, -1],
};

function sendDrive() {
  const v = parseInt(speed.value, 10);
  const [l, r] = DIRS[driveDir];
  sendCmd(`C:M,${l * v},${r * v}`);
}

function startDrive(dir) {
  if (driveDir === dir) return;
  driveDir = dir;
  sendDrive();
  clearInterval(driveTimer);
  driveTimer = setInterval(sendDrive, MOTOR_REPEAT_MS);
}

function stopDrive() {
  clearInterval(driveTimer);
  driveTimer = null;
  driveDir = null;
  sendCmd("C:STOP");
}

speed.addEventListener("input", () => {
  $("speedVal").textContent = speed.value;
});

for (const btn of document.querySelectorAll(".drive")) {
  btn.addEventListener("pointerdown", (ev) => {
    btn.setPointerCapture(ev.pointerId);
    startDrive(btn.dataset.dir);
  });
  btn.addEventListener("pointerup", stopDrive);
  btn.addEventListener("pointercancel", stopDrive);
}
$("btnStop").onclick = stopDrive;

const KEYS = {
  w: "forward", arrowup: "forward",
  s: "back", arrowdown: "back",
  a: "left", arrowleft: "left",
  d: "right", arrowright: "right",
};

document.addEventListener("keydown", (ev) => {
  if (ev.target.tagName === "INPUT") return;
  const key = ev.key.toLowerCase();
  if (key === " ") { ev.preventDefault(); stopDrive(); return; }
  const dir = KEYS[key];
  if (!dir) return;
  ev.preventDefault();
  startDrive(dir);
});

document.addEventListener("keyup", (ev) => {
  if (KEYS[ev.key.toLowerCase()] === driveDir) stopDrive();
});

// zatrzymanie po utracie fokusu (np. przełączenie karty z wciśniętym klawiszem)
window.addEventListener("blur", () => { if (driveDir) stopDrive(); });

// radar
const canvas = $("radar");
const ctx = canvas.getContext("2d");
const radarPoints = new Map(); // kąt -> ostatni pomiar { dist, x, y, heading, time }
let radarSweep = null; // kąt ostatniego pomiaru

function setRadarRunning(on, desc) {
  $("radarStatus").textContent = on ? "włączony " + desc : "wyłączony";
}

function showRadar(r) {
  $("radarAngle").textContent = r.angle;
  $("radarDist").textContent = r.dist === 0 ? "brak echa / brak pomiaru" : r.dist.toFixed(1) + " cm";
  $("radarPosX").textContent = r.x.toFixed(1);
  $("radarPosY").textContent = r.y.toFixed(1);
  $("radarHeading").textContent = r.heading.toFixed(1);
  radarPoints.set(r.angle, { ...r, time: Date.now() });
  radarSweep = r.angle;
}

function drawRadar() {
  const w = canvas.width, h = canvas.height;
  const cx = w / 2, cy = h - 20;
  const R = Math.min(cx - 30, cy - 25); // promień wykresu w pikselach
  const range = Math.max(parseFloat($("radarRange").value) || 200, 1); // zasięg wykresu w cm
  const toXY = (angle, r) => {
    const a = angle * Math.PI / 180;
    return [cx + r * Math.cos(a), cy - r * Math.sin(a)];
  };
  const line = (x1, y1, x2, y2) => {
    ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke();
  };

  ctx.fillStyle = "#001a00";
  ctx.fillRect(0, 0, w, h);

  // siatka: odległości
  ctx.strokeStyle = "#0a5a0a";
  ctx.fillStyle = "#3c9c3c";
  ctx.lineWidth = 1;
  ctx.font = "12px monospace";
  ctx.textAlign = "center";
  for (let i = 1; i <= 4; i++) {
    const r = R * i / 4;
    ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, 2 * Math.PI); ctx.stroke();
    ctx.fillText((range * i / 4).toFixed(0), cx + r, cy + 14);
  }
  ctx.fillText("cm", cx, cy + 14);

  // siatka: kąty
  for (let a = 0; a <= 180; a += 30) {
    const [x, y] = toXY(a, R);
    line(cx, cy, x, y);
    const [lx, ly] = toXY(a, R + 15);
    ctx.fillText(a + "°", lx, ly + 4);
  }

  // pomiary (starsze bledną)
  const now = Date.now();
  const half = RADAR_BEAM_DEG / 2 * Math.PI / 180;
  for (const [angle, p] of radarPoints) {
    if (p.dist === 0) continue;
    const alpha = Math.max(0.2, 1 - (now - p.time) / RADAR_FADE_MS);
    const out = p.dist > range; // poza zasięgiem wykresu - rysowany na krawędzi
    const r = out ? R : R * p.dist / range;
    const a = angle * Math.PI / 180;
    const [x, y] = toXY(angle, r);
    ctx.strokeStyle = `rgba(0, 255, 0, ${alpha * 0.3})`;
    ctx.lineWidth = 2;
    line(cx, cy, x, y);
    // łuk szerokości wiązki (oś Y canvasa skierowana w dół - kąty ze znakiem minus)
    ctx.strokeStyle = out ? `rgba(255, 200, 0, ${alpha * 0.25})` : `rgba(80, 255, 80, ${alpha * 0.25})`;
    ctx.beginPath(); ctx.arc(cx, cy, r, -a - half, -a + half); ctx.stroke();
    ctx.fillStyle = out ? `rgba(255, 200, 0, ${alpha})` : `rgba(80, 255, 80, ${alpha})`;
    ctx.beginPath(); ctx.arc(x, y, 3, 0, 2 * Math.PI); ctx.fill();
  }

  // wiązka ostatniego pomiaru
  if (radarSweep !== null) {
    const a = radarSweep * Math.PI / 180;
    ctx.fillStyle = "rgba(127, 255, 127, 0.15)";
    ctx.beginPath(); ctx.moveTo(cx, cy); ctx.arc(cx, cy, R, -a - half, -a + half); ctx.closePath(); ctx.fill();
    const [x, y] = toXY(radarSweep, R);
    ctx.strokeStyle = "#7fff7f";
    ctx.lineWidth = 2;
    line(cx, cy, x, y);
  }

  // robot
  ctx.fillStyle = "#7fff7f";
  ctx.fillRect(cx - 6, cy - 4, 12, 8);
}

$("btnRadarStart").onclick = () => {
  const ids = ["radarFrom", "radarTo", "radarStep", "radarInterval"];
  const [from, to, step, interval] = ids.map((id) => parseInt($(id).value, 10));
  if ([from, to, step, interval].some(Number.isNaN)) {
    log("Radar: uzupełnij wszystkie parametry");
    return;
  }
  if (sendCmd(`C:RADAR,${from},${to},${step},${interval}`)) {
    setRadarRunning(true, `(${from}-${to}°, krok ${step}°, co ${interval} ms)`);
  }
};
$("btnRadarStop").onclick = () => {
  sendCmd("C:RADAROFF");
  setRadarRunning(false);
};
$("btnRadarClear").onclick = () => {
  radarPoints.clear();
  radarSweep = null;
};

setInterval(drawRadar, RADAR_REDRAW_MS);

// zdjęcie
let shotBusy = false; // true, dopóki trwa pobieranie zdjęcia (nie wysyłamy kilku naraz)
let lastBlobUrl = null;

function stamp() {
  const d = new Date();
  const p = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}_` +
         `${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}`;
}

async function takePhoto() {
  if (shotBusy) return;
  shotBusy = true;
  $("btnShot").disabled = true;
  $("shotInfo").textContent = "robię zdjęcie...";

  const ctrl = new AbortController();
  const timeout = setTimeout(() => ctrl.abort(), SHOT_TIMEOUT_MS);
  const t0 = performance.now();

  try {
    const resp = await fetch(`${CAPTURE_URL}?t=${Date.now()}`, { signal: ctrl.signal, cache: "no-store" });
    if (!resp.ok) throw new Error("HTTP " + resp.status);
    const blob = await resp.blob();

    const url = URL.createObjectURL(blob);
    $("cam").src = url;
    if (lastBlobUrl) URL.revokeObjectURL(lastBlobUrl);
    lastBlobUrl = url;

    $("shotInfo").textContent =
      `zdjęcie z ${new Date().toLocaleTimeString()} - ${(blob.size / 1024).toFixed(1)} kB, ` +
      `${Math.round(performance.now() - t0)} ms`;
  } catch (e) {
    const msg = e.name === "AbortError" ? "przekroczono czas oczekiwania" : e.message;
    $("shotInfo").textContent = "błąd zdjęcia: " + msg;
    log("Zdjęcie: " + msg);
  } finally {
    clearTimeout(timeout);
    shotBusy = false;
    $("btnShot").disabled = false;
  }
}
$("btnShot").onclick = takePhoto;

// rozpoczęcie działania
connect();