const ESP_HOST = "192.168.4.1";
const WS_URL = `ws://${ESP_HOST}:80/`;
const CAPTURE_URL = `http://${ESP_HOST}:81/capture`;
const SHOT_TIMEOUT_MS = 8000; // po tylu ms uznajemy zdjęcie za nieudane
const SLIDER_THROTTLE_MS = 50;
const MOTOR_REPEAT_MS = 200; // ponawianie polecenia jazdy (Arduino zatrzymuje silniki po 1s bez polecenia)

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

function onLine(line) {
  line = line.trim();
  if (!line) return;
  const t = parseData(line);
  if (t) {
    showData(t);
    updatePosition(t);
    lastFrameTime = Date.now();
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
  sendCmd(`C:S,${parseInt(servo.value, 10)}`);
}

servo.addEventListener("input", () => {
  $("servoVal").textContent = servo.value;
  sendServo(false);
});
servo.addEventListener("change", () => sendServo(true));

// sonar
$("btnTrig").onclick = () => sendCmd("C:TRIG");

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