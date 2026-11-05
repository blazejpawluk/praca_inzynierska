// Błażej Pawluk
// wyliczanie pozycji i kąta autka na podstawie danych z akcelerometru i żyroskopu

// założenia:
//    podczas kalibracji MPU autko stoi w pozycji (0, 0) i kącie 0
//    autko jeździ po płaskiej powierzchni - liczy się tylko ruch w płaszczyźnie XY i obrót wokół osi Z
// układ współrzędnych (ustalany w chwili kalibracji):
//    X - przód, Y - lewo, kąt - skręt w lewo (przeciwnie do ruchu wskazówek zegara)
// sposób liczenia:
//    kąt = całka z prędkości kątowej gz
//    przyspieszenie (ax, ay) obracane z układu autka do układu świata o aktualny kąt
//    prędkość = całka z przyspieszenia, pozycja = całka z prędkości (metoda trapezów)
//    przy zatrzymanych silnikach prędkość jest zerowana (ogranicza dryf podwójnego całkowania)

const G = 9.81; // m/s^2 na 1 g
const ACC_DEADZONE_G = 0.02; // przyspieszenia mniejsze od progu traktujemy jako szum
const GYRO_DEADZONE_DPS = 0.5; // prędkości kątowe mniejsze od progu traktujemy jako szum
const MAX_DT_S = 1.0; // dłuższa przerwa między ramkami (np. rozłączenie) - pomijamy krok całkowania

// stan
let posX = 0, posY = 0; // m
let velX = 0, velY = 0; // m/s (w układzie świata)
let heading = 0; // stopnie
let prev = null; // poprzednia próbka: { time, aX, aY, gz }

function deadzone(v, limit) {
  return Math.abs(v) < limit ? 0 : v;
}

function resetPosition() {
  posX = posY = 0;
  velX = velY = 0;
  heading = 0;
  prev = null;
  showPosition();
}

// t - ramka danych z parseData()
function updatePosition(t) {
  const time = performance.now() / 1000;
  const gz = deadzone(t.gz, GYRO_DEADZONE_DPS);
  const stopped = t.mL === 0 && t.mR === 0;

  // czas od poprzedniej ramki (0 - nie całkujemy)
  let dt = prev === null ? 0 : time - prev.time;
  if (dt <= 0 || dt > MAX_DT_S) dt = 0;

  // kąt
  if (dt > 0) heading += (prev.gz + gz) / 2 * dt;

  // przyspieszenie w układzie świata (m/s^2)
  const rad = heading * Math.PI / 180;
  const axb = deadzone(t.ax, ACC_DEADZONE_G) * G;
  const ayb = deadzone(t.ay, ACC_DEADZONE_G) * G;
  const aX = axb * Math.cos(rad) - ayb * Math.sin(rad);
  const aY = axb * Math.sin(rad) + ayb * Math.cos(rad);

  // prędkość i pozycja
  if (dt > 0) {
    const newVelX = stopped ? 0 : velX + (prev.aX + aX) / 2 * dt;
    const newVelY = stopped ? 0 : velY + (prev.aY + aY) / 2 * dt;
    posX += (velX + newVelX) / 2 * dt;
    posY += (velY + newVelY) / 2 * dt;
    velX = newVelX;
    velY = newVelY;
  }

  prev = { time, aX, aY, gz };
  showPosition();
}

function showPosition() {
  // kąt w zakresie (-180, 180]
  let h = heading % 360;
  if (h > 180) h -= 360;
  if (h <= -180) h += 360;

  document.getElementById("posX").textContent = (posX * 100).toFixed(1);
  document.getElementById("posY").textContent = (posY * 100).toFixed(1);
  document.getElementById("heading").textContent = h.toFixed(1);
  document.getElementById("speedNow").textContent = (Math.hypot(velX, velY) * 100).toFixed(1);
}

document.getElementById("btnResetPos").onclick = resetPosition;
