const elements = {
  speedGauge: document.getElementById("speedo-gauge"),
  speedValue: document.getElementById("vit"),
  voltage: document.getElementById("volt"),
  current: document.getElementById("amp"),
  power: document.getElementById("pwr"),
  satellites: document.getElementById("sat"),
  degrees: document.getElementById("deg"),
  needle: document.getElementById("needle"),
  recording: document.getElementById("rec"),
};

function rawValue(value, fallback) {
  return value === undefined || value === null ? fallback : String(value);
}

function setValueWithUnit(element, value, unit, fallback) {
  element.innerHTML = `${rawValue(value, fallback)}<span class="unit">${unit}</span>`;
}

function updateDashboard(data) {
  const speed = Number(data.vitesse) || 0;
  const cappedSpeed = Math.min(Math.max(speed, 0), 60);
  const angle = -45 + (cappedSpeed / 60) * 180;

  elements.speedGauge.style.transform = `rotate(${angle}deg)`;
  elements.speedValue.textContent = rawValue(data.vitesse, "0");
  setValueWithUnit(elements.voltage, data.tension, "V", "0.00");
  setValueWithUnit(elements.current, data.courant, "A", "0.0");
  setValueWithUnit(elements.power, data.puissance, "W", "0");
  elements.satellites.textContent = rawValue(data.sat, "0");
  elements.degrees.textContent = `${rawValue(data.cap, "0")}\u00B0`;
  elements.needle.style.transform = `rotate(${Number(data.cap) || 0}deg)`;
  elements.recording.style.display = data.mode === 1 ? "inline" : "none";
}

async function refreshData() {
  try {
    const response = await fetch("/data", { cache: "no-store" });
    if (!response.ok) return;

    const data = await response.json();
    updateDashboard(data);
  } catch (error) {
    // Le HUD reste affiche avec la derniere trame valide si le Wi-Fi saute.
  }
}

refreshData();
setInterval(refreshData, 200);
