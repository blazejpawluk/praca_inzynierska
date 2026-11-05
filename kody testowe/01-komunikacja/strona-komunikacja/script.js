const ESP_HOST = "192.168.4.1";
const WS_URL = `ws://${ESP_HOST}:80/`;
const CAPTURE_URL = `http://${ESP_HOST}:81/capture`;
const SHOT_TIMEOUT_MS = 8000; // po tylu ms uznajemy zdjęcie za nieudane
const SLIDER_THROTTLE_MS = 50;
const MOTOR_REPEAT_MS = 200;

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
  if (p.length !== 9 || p.some(Number.isNaN)) return null;
  const [enL, enR, sonar, ax, ay, az, gx, gy, gz] = p;
  return { enL, enR, sonar, ax, ay, az, gx, gy, gz };
}

function showData(t) {
  $("enL").textContent = t.enL;
  $("enR").textContent = t.enR;
  $("sonar").textContent = t.sonar;
  for (const k of ["ax", "ay", "az", "gx", "gy", "gz"]) {
    $(k).textContent = t[k].toFixed(2);
  }
}

function onLine(line) {
  line = line.trim();
  if (!line) return;
  const t = parseData(line);
  if (t) {
    showData(t);
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

// silniki
const speedL = $("speedL");
const speedR = $("speedR");
const baseSpeed = $("baseSpeed");
let lastMotorSend = 0;

function motorCmd() {
  return `C:M,${parseInt(speedL.value, 10)},${parseInt(speedR.value, 10)}`;
}

function sendMotors(force) {
  const now = Date.now();
  if (!force && now - lastMotorSend < SLIDER_THROTTLE_MS) return;
  lastMotorSend = now;
  sendCmd(motorCmd());
}

function updateMotorLabels() {
  $("speedLVal").textContent = speedL.value;
  $("speedRVal").textContent = speedR.value;
}

function setMotors(l, r) {
  speedL.value = l;
  speedR.value = r;
  updateMotorLabels();
  sendMotors(true);
}

for (const s of [speedL, speedR]) {
  s.addEventListener("input", () => { updateMotorLabels(); sendMotors(false); });
  s.addEventListener("change", () => sendMotors(true));
}
baseSpeed.addEventListener("input", () => {
  $("baseSpeedVal").textContent = baseSpeed.value;
});

const base = () => parseInt(baseSpeed.value, 10);
$("btnLeftFwd").onclick = () => setMotors(base(), 0);
$("btnRightFwd").onclick = () => setMotors(0, base());
$("btnBothFwd").onclick = () => setMotors(base(), base());
$("btnBothBack").onclick = () => setMotors(-base(), -base());
$("btnStop").onclick = () => setMotors(0, 0);

// wysyłamy komendy jeśli jedzie, żeby Arduino mogło wykryć brak połączenia i się zatrzymać
setInterval(() => {
  if (parseInt(speedL.value, 10) !== 0 || parseInt(speedR.value, 10) !== 0) {
    sendMotors(true);
  }
}, MOTOR_REPEAT_MS);

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