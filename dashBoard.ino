#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi Hotspot Credentials
const char* ssid = "vivo Y56 5G";
const char* pass = "73728234";

// Admin Credentials
const char* ADMIN_USER = "admin";
const char* ADMIN_PASS = "mine2026";

SoftwareSerial megaSerial(14, 13); // D5 = RX, D7 = TX
#define BUZZER_PIN 12             // D6

ESP8266WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);

int mq2Val = 0, mq4Val = 0, soilVal = 0;
float tempVal = 0.0;
int humVal = 0;
int flameVal = 0;

const int MQ4_LIMIT = 400;
const int MQ2_LIMIT = 250;

int systemSeverity = 0;
int healthScore = 100;
String gasDiagnosis = "ATMOSPHERE NOMINAL";
String hazardText = "SYSTEM SECURE";
String activeThreat = "NONE";
String adminBroadcastMsg = "Standard operations active.";

bool adminDrillActive = false;
unsigned long buzzerMuteUntil = 0;
unsigned long lastSerialTime = 0;
bool linkLost = false;
unsigned long lastWifiCheck = 0;
unsigned long lastToneToggle = 0;
bool toneToggleState = false;

// ============================================================================
// REDESIGNED TACTICAL CONTROL CENTER WEB INTERFACE (PROGMEM STRING SPLIT)
// ============================================================================

const char PAGE_PART1[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Deep-Mine Shaft Control Matrix</title>
  <style>
    :root {
      --bg-dark: #030712;
      --panel-bg: rgba(17, 24, 39, 0.75);
      --panel-border: rgba(255, 255, 255, 0.08);
      --cyan-accent: #06b6d4;
      --blue-glow: #3b82f6;
      --green-ok: #10b981;
      --amber-warn: #f59e0b;
      --red-alert: #ef4444;
      --text-main: #f8fafc;
      --text-muted: #64748b;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", sans-serif; }
    body { background: radial-gradient(circle at top, #0f172a 0%, #030712 100%); color: var(--text-main); min-height: 100vh; padding: 12px; }
    .container { max-width: 1300px; margin: 0 auto; display: flex; flex-direction: column; gap: 12px; }
    
    /* Top Bar Header */
    .top-bar { display: flex; justify-content: space-between; align-items: center; background: var(--panel-bg); backdrop-filter: blur(12px); border: 1px solid var(--panel-border); padding: 12px 18px; border-radius: 12px; box-shadow: 0 4px 20px rgba(0,0,0,0.5); }
    .brand { font-size: 1rem; font-weight: 900; color: var(--cyan-accent); letter-spacing: 1.5px; display: flex; align-items: center; gap: 10px; }
    .live-dot { width: 10px; height: 10px; border-radius: 50%; background: var(--green-ok); box-shadow: 0 0 12px var(--green-ok); animation: pulse-green 2s infinite; }
    @keyframes pulse-green { 0%, 100% { opacity: 1; transform: scale(1); } 50% { opacity: 0.4; transform: scale(0.85); } }
    .clock-display { font-size: 0.85rem; font-family: 'Courier New', Courier, monospace; color: #94a3b8; font-weight: 700; letter-spacing: 1px; }
    .nav-actions { display: flex; gap: 8px; }

    /* Tactical Alert Strips */
    .status-strip { padding: 14px; border-radius: 10px; text-align: center; font-weight: 900; font-size: 1.1rem; letter-spacing: 2px; text-transform: uppercase; transition: all 0.3s ease; }
    .sev-0 { background: rgba(16,185,129,0.12); border: 1px solid var(--green-ok); color: #34d399; box-shadow: 0 0 15px rgba(16,185,129,0.15); }
    .sev-1 { background: rgba(245,158,11,0.18); border: 1px solid var(--amber-warn); color: #fbbf24; box-shadow: 0 0 20px rgba(245,158,11,0.25); }
    .sev-2 { background: rgba(239,68,68,0.25); border: 2px solid var(--red-alert); color: #fff; animation: strobe-alert 0.5s infinite alternate; }
    @keyframes strobe-alert { 0% { background: rgba(239,68,68,0.2); box-shadow: 0 0 10px var(--red-alert); } 100% { background: rgba(239,68,68,0.6); box-shadow: 0 0 35px var(--red-alert); } }

    .broadcast-box { background: var(--panel-bg); border: 1px solid var(--panel-border); padding: 10px 16px; border-radius: 10px; display: flex; justify-content: space-between; align-items: center; font-size: 0.82rem; flex-wrap: wrap; gap: 8px; }
    .broadcast-tag { padding: 3px 8px; border-radius: 4px; background: rgba(6,182,212,0.15); color: var(--cyan-accent); font-size: 0.7rem; font-weight: 800; letter-spacing: 1px; margin-right: 6px; }

    /* Sensor Grid */
    .gauge-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 12px; }
    .card { background: var(--panel-bg); backdrop-filter: blur(10px); border: 1px solid var(--panel-border); border-radius: 14px; padding: 14px 10px; display: flex; flex-direction: column; align-items: center; text-align: center; cursor: pointer; transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1); position: relative; overflow: hidden; }
    .card::before { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 3px; background: transparent; transition: background 0.3s; }
    .card:hover { transform: translateY(-3px); border-color: var(--cyan-accent); box-shadow: 0 8px 25px rgba(6, 182, 212, 0.15); }
    .card.active-stream { border-color: var(--cyan-accent); background: rgba(6, 182, 212, 0.08); box-shadow: 0 0 20px rgba(6, 182, 212, 0.25); }
    .card.active-stream::before { background: var(--cyan-accent); }
    
    .card-title { font-size: 0.68rem; font-weight: 800; color: var(--text-muted); text-transform: uppercase; letter-spacing: 1.2px; margin-bottom: 8px; }
    .circle-wrap { position: relative; width: 110px; height: 110px; display: flex; align-items: center; justify-content: center; }
    .circle-wrap svg { width: 110px; height: 110px; transform: rotate(-90deg); }
    .circle-bg { fill: none; stroke: rgba(255,255,255,0.05); stroke-width: 9; }
    .circle-bar { fill: none; stroke-width: 9; stroke-linecap: round; transition: stroke-dashoffset 0.5s ease, stroke 0.3s ease; filter: drop-shadow(0 0 4px currentColor); }
    .dial-val { position: absolute; display: flex; flex-direction: column; align-items: center; }
    .dial-num { font-size: 1.25rem; font-weight: 900; font-family: 'Courier New', monospace; }
    .dial-unit { font-size: 0.58rem; color: var(--text-muted); font-weight: 800; text-transform: uppercase; letter-spacing: 1px; }
    .card-meta { font-size: 0.62rem; color: #475569; margin-top: 8px; font-weight: 700; letter-spacing: 0.5px; }

    /* Canvas Graph Card */
    .chart-card { background: var(--panel-bg); backdrop-filter: blur(10px); border: 1px solid var(--panel-border); border-radius: 14px; padding: 16px; box-shadow: 0 4px 20px rgba(0,0,0,0.4); }
    .chart-top { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; flex-wrap: wrap; gap: 6px; }
    canvas { width: 100%; height: 210px; background: #020617; border-radius: 10px; border: 1px solid rgba(255,255,255,0.05); display: block; }

    /* Bottom Status Bar */
    .bottom-bar { background: var(--panel-bg); border: 1px solid var(--panel-border); border-radius: 10px; padding: 10px 16px; display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 8px; }
    .btn { padding: 9px 16px; border-radius: 8px; font-weight: 800; font-size: 0.72rem; letter-spacing: 1px; text-transform: uppercase; cursor: pointer; border: none; color: #fff; transition: all 0.2s; box-shadow: 0 2px 10px rgba(0,0,0,0.3); }
    .btn:hover { transform: translateY(-1px); filter: brightness(1.15); }
    .btn-audio { background: linear-gradient(135deg, #0284c7, #0369a1); }
    .btn-admin { background: linear-gradient(135deg, #dc2626, #991b1b); }
    .btn-blackbox { background: linear-gradient(135deg, #4f46e5, #3730a3); }

    /* Modal Windows */
    .modal { display: none; position: fixed; inset: 0; background: rgba(2, 6, 23, 0.85); backdrop-filter: blur(8px); z-index: 99; align-items: center; justify-content: center; padding: 16px; }
    .modal-box { background: #0f172a; border: 1px solid rgba(255,255,255,0.12); border-radius: 16px; width: 100%; max-width: 420px; padding: 24px; box-shadow: 0 20px 50px rgba(0,0,0,0.8); }
    .modal-head { display: flex; justify-content: space-between; margin-bottom: 16px; font-weight: 900; color: var(--cyan-accent); font-size: 1rem; letter-spacing: 1px; }
    .form-group { display: flex; flex-direction: column; gap: 6px; margin-bottom: 14px; }
    .form-group label { font-size: 0.72rem; color: var(--text-muted); font-weight: 800; text-transform: uppercase; letter-spacing: 0.8px; }
    .form-group input { padding: 12px; background: #020617; border: 1px solid #334155; border-radius: 8px; color: #fff; font-size: 0.9rem; outline: none; }
    .form-group input:focus { border-color: var(--cyan-accent); box-shadow: 0 0 10px rgba(6,182,212,0.3); }
    .modal-actions { display: flex; gap: 10px; justify-content: flex-end; }
  </style>
</head>
<body>
  <div class="modal" id="loginModal" style="display: flex;">
    <div class="modal-box">
      <div class="modal-head"><span>STATION AUTHENTICATION</span></div>
      <div class="form-group">
        <label>Operator Username</label>
        <input type="text" id="gateUser" placeholder="admin">
      </div>
      <div class="form-group">
        <label>Access Key</label>
        <input type="password" id="gatePass" placeholder="••••••••">
      </div>
      <div class="modal-actions" style="margin-top: 16px;">
        <button class="btn" style="background: #334155;" onclick="proceedAsGuest()">Guest Mode</button>
        <button class="btn btn-audio" onclick="verifyGateLogin()">Authenticate</button>
      </div>
    </div>
  </div>

  <div class="container">
    <div class="top-bar">
      <div class="brand"><span class="live-dot"></span> MINE-LINK // TELEMETRY CORE</div>
      <div class="clock-display" id="liveClock">2026-00-00 00:00:00</div>
      <div class="nav-actions">
        <button class="btn btn-audio" id="audioToggleBtn" onclick="initBrowserAudio()">Enable Live Siren</button>
        <button class="btn btn-admin" id="adminControlBtn" style="display:none;" onclick="openAdminPanel()">Command Console</button>
      </div>
    </div>

    <div id="statusStrip" class="status-strip sev-0">SYSTEM SECURE</div>

    <div class="broadcast-box">
      <div><span class="broadcast-tag">DIAGNOSIS</span> <span id="diagTag" style="color:#38bdf8; font-weight:800;">ANALYZING...</span></div>
      <div><span class="broadcast-tag" style="background:rgba(250,204,21,0.15); color:#facc15;">ADVISORY</span> <span id="broadcastText" style="color:#facc15; font-weight:800;">Standard operations active.</span></div>
    </div>

    <div class="gauge-grid">
      <div class="card" id="card-health" onclick="selectStream('health', 'Life Safety Index', '#10b981', 0, 100, this)">
        <div class="card-title">Shaft Life Safety</div>
        <div class="circle-wrap"><svg><circle class="circle-bg" cx="55" cy="55" r="45"></circle><circle id="ring-health" class="circle-bar" cx="55" cy="55" r="45" stroke-dasharray="283" stroke-dashoffset="0" stroke="#10b981"></circle></svg><div class="dial-val"><span class="dial-num" id="num-health" style="color:#10b981;">100%</span><span class="dial-unit">HEALTH</span></div></div>
        <div class="card-meta">TAP TO WAVEFORM</div>
      </div>

      <div class="card active-stream" id="card-mq4" onclick="selectStream('mq4', 'Methane Concentration (MQ-4)', '#06b6d4', 0, 700, this)">
        <div class="card-title">MQ-4 Methane</div>
        <div class="circle-wrap"><svg><circle class="circle-bg" cx="55" cy="55" r="45"></circle><circle id="ring-mq4" class="circle-bar" cx="55" cy="55" r="45" stroke-dasharray="283" stroke-dashoffset="283" stroke="#06b6d4"></circle></svg><div class="dial-val"><span class="dial-num" id="num-mq4" style="color:#06b6d4;">--</span><span class="dial-unit">ADC RAW</span></div></div>
        <div class="card-meta">SAFE THRESHOLD: 400</div>
      </div>

      <div class="card" id="card-mq2" onclick="selectStream('mq2', 'Smoke / Toxic Gas (MQ-2)', '#f59e0b', 0, 600, this)">
        <div class="card-title">MQ-2 Smoke/LPG</div>
        <div class="circle-wrap"><svg><circle class="circle-bg" cx="55" cy="55" r="45"></circle><circle id="ring-mq2" class="circle-bar" cx="55" cy="55" r="45" stroke-dasharray="283" stroke-dashoffset="283" stroke="#f59e0b"></circle></svg><div class="dial-val"><span class="dial-num" id="num-mq2" style="color:#f59e0b;">--</span><span class="dial-unit">ADC RAW</span></div></div>
        <div class="card-meta">SAFE THRESHOLD: 250</div>
      </div>

      <div class="card" id="card-temp" onclick="selectStream('temp', 'Ambient Temperature (°C)', '#fb7185', 10, 60, this)">
        <div class="card-title">Temperature</div>
        <div class="circle-wrap"><svg><circle class="circle-bg" cx="55" cy="55" r="45"></circle><circle id="ring-temp" class="circle-bar" cx="55" cy="55" r="45" stroke-dasharray="283" stroke-dashoffset="283" stroke="#fb7185"></circle></svg><div class="dial-val"><span class="dial-num" id="num-temp" style="color:#fb7185;">--</span><span class="dial-unit">CELSIUS</span></div></div>
        <div class="card-meta">NOMINAL &lt;45°C</div>
      </div>

      <div class="card" id="card-hum" onclick="selectStream('hum', 'Relative Humidity (%)', '#818cf8', 20, 100, this)">
        <div class="card-title">Humidity</div>
        <div class="circle-wrap"><svg><circle class="circle-bg" cx="55" cy="55" r="45"></circle><circle id="ring-hum" class="circle-bar" cx="55" cy="55" r="45" stroke-dasharray="283" stroke-dashoffset="283" stroke="#818cf8"></circle></svg><div class="dial-val"><span class="dial-num" id="num-hum" style="color:#818cf8;">--</span><span class="dial-unit">PERCENT</span></div></div>
        <div class="card-meta">NOMINAL &lt;85%</div>
      </div>

      <div class="card" id="card-soil" onclick="selectStream('soil', 'Soil Strata Saturation', '#fbbf24', 0, 1024, this)">
        <div class="card-title">Soil Strata</div>
        <div class="circle-wrap"><svg><circle class="circle-bg" cx="55" cy="55" r="45"></circle><circle id="ring-soil" class="circle-bar" cx="55" cy="55" r="45" stroke-dasharray="283" stroke-dashoffset="283" stroke="#fbbf24"></circle></svg><div class="dial-val"><span class="dial-num" id="num-soil" style="color:#fbbf24; font-size:1.05rem;">--</span><span class="dial-unit" id="unit-soil">RAW</span></div></div>
        <div class="card-meta">FLOOD: &lt;500</div>
      </div>

      <div class="card" id="card-flame">
        <div class="card-title">Flame IR Optic</div>
        <div class="circle-wrap"><svg><circle class="circle-bg" cx="55" cy="55" r="45"></circle><circle id="ring-flame" class="circle-bar" cx="55" cy="55" r="45" stroke-dasharray="283" stroke-dashoffset="0" stroke="#34d399"></circle></svg><div class="dial-val"><span class="dial-num" id="num-flame" style="color:#34d399; font-size:1.05rem;">CLEAR</span><span class="dial-unit">OPTIC</span></div></div>
        <div class="card-meta">STATE: OPTICAL</div>
      </div>
    </div>

    <div class="chart-card">
      <div class="chart-top">
        <div style="font-size:0.85rem; font-weight:800; color:#cbd5e1; letter-spacing:1px;">Real-Time Telemetry Stream &mdash; <span id="chartLabel" style="color:var(--cyan-accent);">Methane (MQ-4)</span></div>
      </div>
      <canvas id="liveChart"></canvas>
    </div>

    <div class="bottom-bar">
      <div style="font-size:0.75rem; color:#64748b; font-weight:800; letter-spacing:1px;" id="userRoleBadge">ACCESS: GUEST MONITORING MODE</div>
      <button class="btn btn-blackbox" onclick="exportBlackboxCSV()">Export Blackbox (.CSV)</button>
    </div>
  </div>

  <div class="modal" id="adminModal">
    <div class="modal-box">
      <div class="modal-head"><span>TACTICAL COMMAND OVERRIDE</span><button class="btn" style="background:#475569; padding:4px 10px;" onclick="closeAdminPanel()">X</button></div>
      <div class="form-group"><label>Surface Broadcast Directive</label><input type="text" id="adminMsgInput" placeholder="Message for miner dashboards..."></div>
      <div style="display:grid; grid-template-columns:1fr 1fr; gap:8px; margin-top:12px;">
        <button class="btn btn-admin" onclick="submitAdminAction('drill')">Evacuation Drill</button>
        <button class="btn" style="background:#059669;" onclick="submitAdminAction('broadcast')">Post Message</button>
        <button class="btn" style="background:#475569;" onclick="submitAdminAction('mute')">Mute Siren (45s)</button>
        <button class="btn" style="background:#334155;" onclick="submitAdminAction('reset')">Reset Normal</button>
      </div>
    </div>
  </div>
)rawliteral";

const char PAGE_PART2[] PROGMEM = R"rawliteral(
  <script>
    const CIRC = 283.0;
    const historyData = [];
    const blackboxLogs = [];
    let activeParam = 'mq4', currentLabel = 'Methane Concentration (MQ-4)', currentColor = '#06b6d4', minScale = 0, maxScale = 700;
    let audioCtx = null, osc = null, gainNode = null, sirenInterval = null;
    let authUser = "", authPass = "";

    function updateClock() {
      const now = new Date();
      const yr = now.getFullYear();
      const mo = String(now.getMonth() + 1).padStart(2, '0');
      const da = String(now.getDate()).padStart(2, '0');
      const hr = String(now.getHours()).padStart(2, '0');
      const mi = String(now.getMinutes()).padStart(2, '0');
      const se = String(now.getSeconds()).padStart(2, '0');
      document.getElementById('liveClock').innerText = `${yr}-${mo}-${da} ${hr}:${mi}:${se}`;
    }
    setInterval(updateClock, 1000);
    updateClock();

    function selectStream(param, label, color, min, max, el) {
      activeParam = param;
      currentLabel = label;
      currentColor = color;
      minScale = min;
      maxScale = max;
      document.querySelectorAll('.card').forEach(c => c.classList.remove('active-stream'));
      if (el) el.classList.add('active-stream');
      document.getElementById('chartLabel').innerText = label;
      drawChart();
    }

    function proceedAsGuest() {
      document.getElementById('loginModal').style.display = 'none';
      document.getElementById('userRoleBadge').innerText = "ACCESS: GUEST MONITORING MODE";
    }

    async function verifyGateLogin() {
      const u = document.getElementById('gateUser').value;
      const p = document.getElementById('gatePass').value;
      const res = await fetch(`/adminAction?user=${encodeURIComponent(u)}&pass=${encodeURIComponent(p)}&action=verify`);
      if (res.status === 200) {
        authUser = u;
        authPass = p;
        document.getElementById('loginModal').style.display = 'none';
        document.getElementById('adminControlBtn').style.display = 'inline-block';
        document.getElementById('userRoleBadge').innerText = "ACCESS: COMMAND CHIEF (AUTHENTICATED)";
        document.getElementById('userRoleBadge').style.color = "#06b6d4";
      } else {
        alert("Authentication Denied: Invalid Credentials");
      }
    }

    function openAdminPanel() { document.getElementById('adminModal').style.display = 'flex'; }
    function closeAdminPanel() { document.getElementById('adminModal').style.display = 'none'; }

    async function submitAdminAction(act) {
      const m = document.getElementById('adminMsgInput').value;
      let url = `/adminAction?user=${encodeURIComponent(authUser)}&pass=${encodeURIComponent(authPass)}&action=${act}`;
      if (act === 'broadcast') url += `&msg=${encodeURIComponent(m)}`;
      const res = await fetch(url);
      if (res.status === 403) alert("Unauthorized");
      else { alert("Command Dispatched"); closeAdminPanel(); }
    }

    function initBrowserAudio() {
      if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        gainNode = audioCtx.createGain();
        gainNode.gain.setValueAtTime(0.2, audioCtx.currentTime);
        gainNode.connect(audioCtx.destination);
      }
      document.getElementById('audioToggleBtn').innerText = "Siren Ready";
      document.getElementById('audioToggleBtn').style.background = "#059669";
    }

    function playBrowserSiren() {
      if (!audioCtx || sirenInterval) return;
      osc = audioCtx.createOscillator();
      osc.type = "sawtooth";
      osc.frequency.setValueAtTime(550, audioCtx.currentTime);
      osc.connect(gainNode);
      osc.start();
      let toggle = false;
      sirenInterval = setInterval(() => {
        if (!osc) return;
        osc.frequency.setValueAtTime(toggle ? 950 : 550, audioCtx.currentTime);
        toggle = !toggle;
      }, 250);
    }

    function stopBrowserSiren() {
      if (sirenInterval) { clearInterval(sirenInterval); sirenInterval = null; }
      if (osc) { osc.stop(); osc.disconnect(); osc = null; }
    }

    const canvas = document.getElementById('liveChart');
    const ctx = canvas.getContext('2d');

    function drawChart() {
      canvas.width = canvas.parentElement.clientWidth - 32;
      canvas.height = 210;
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      
      // Grid lines
      ctx.strokeStyle = 'rgba(255,255,255,0.04)';
      ctx.lineWidth = 1;
      for (let y = 30; y < canvas.height; y += 35) {
        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(canvas.width, y); ctx.stroke();
      }
      
      if (historyData.length < 2) return;

      // Gradient Fill under path
      const grad = ctx.createLinearGradient(0, 0, 0, canvas.height);
      grad.addColorStop(0, currentColor + '44');
      grad.addColorStop(1, currentColor + '00');

      ctx.beginPath();
      for (let i = 0; i < historyData.length; i++) {
        const val = historyData[i][activeParam];
        const x = (canvas.width / 39) * i;
        const norm = (val - minScale) / (maxScale - minScale);
        const y = canvas.height - (norm * (canvas.height - 40) + 20);
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.lineTo((canvas.width / 39) * (historyData.length - 1), canvas.height);
      ctx.lineTo(0, canvas.height);
      ctx.closePath();
      ctx.fillStyle = grad;
      ctx.fill();

      // Stroke Line
      ctx.beginPath();
      ctx.strokeStyle = currentColor;
      ctx.lineWidth = 2.5;
      for (let i = 0; i < historyData.length; i++) {
        const val = historyData[i][activeParam];
        const x = (canvas.width / 39) * i;
        const norm = (val - minScale) / (maxScale - minScale);
        const y = canvas.height - (norm * (canvas.height - 40) + 20);
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.stroke();

      // Current Value Overlay
      const lastVal = historyData[historyData.length - 1][activeParam];
      ctx.fillStyle = currentColor;
      ctx.font = '900 12px sans-serif';
      ctx.fillText(currentLabel.toUpperCase() + ': ' + lastVal, 12, 22);
    }

    function setDial(ringId, numId, cardId, val, max, alertVal, normalCol, alertCol) {
      const ring = document.getElementById(ringId);
      const num = document.getElementById(numId);
      const card = document.getElementById(cardId);
      const clamped = Math.max(0, Math.min(val, max));
      ring.style.strokeDashoffset = CIRC - (clamped / max) * CIRC;
      if (val >= alertVal) {
        ring.style.stroke = alertCol; num.style.color = alertCol; card.style.borderColor = alertCol;
      } else {
        ring.style.stroke = normalCol; num.style.color = normalCol;
      }
    }

    function exportBlackboxCSV() {
      if (blackboxLogs.length === 0) { alert("Blackbox telemetry buffer initializing..."); return; }
      let csv = "Timestamp,MQ4,MQ2,Temp,Hum,Soil,Flame,Health,Diagnosis,Status\n";
      blackboxLogs.forEach(r => {
        csv += `"${r.time}",${r.mq4},${r.mq2},${r.temp},${r.hum},${r.soil},${r.flame},${r.health},"${r.diagnosis}","${r.status}"\n`;
      });
      const blob = new Blob([csv], { type: "text/csv" });
      const a = document.createElement("a");
      a.href = URL.createObjectURL(blob);
      a.download = "mine_telemetry_blackbox_" + Date.now() + ".csv";
      a.click();
    }

    async function poll() {
      try {
        const res = await fetch('/data');
        const d = await res.json();
        
        document.getElementById('num-health').innerText = d.health + "%";
        const hColor = d.health > 70 ? "#10b981" : (d.health > 40 ? "#f59e0b" : "#ef4444");
        document.getElementById('num-health').style.color = hColor;
        document.getElementById('ring-health').style.stroke = hColor;
        document.getElementById('ring-health').style.strokeDashoffset = CIRC - (d.health / 100) * CIRC;

        document.getElementById('num-mq4').innerText = d.mq4;
        setDial('ring-mq4', 'num-mq4', 'card-mq4', d.mq4, 700, d.mq4Limit, '#06b6d4', '#ef4444');

        document.getElementById('num-mq2').innerText = d.mq2;
        setDial('ring-mq2', 'num-mq2', 'card-mq2', d.mq2, 600, d.mq2Limit, '#06b6d4', '#f59e0b');

        document.getElementById('num-temp').innerText = (d.temp <= 0) ? "Wire" : d.temp + "°";
        setDial('ring-temp', 'num-temp', 'card-temp', d.temp, 60, 45, '#fb7185', '#ef4444');

        document.getElementById('num-hum').innerText = d.hum + "%";
        setDial('ring-hum', 'num-hum', 'card-hum', d.hum, 100, 85, '#818cf8', '#ef4444');

        document.getElementById('num-soil').innerText = (d.soil < 500) ? "MOIST" : "DRY";
        document.getElementById('unit-soil').innerText = "ADC: " + d.soil;
        setDial('ring-soil', 'num-soil', 'card-soil', 1024 - d.soil, 1024, 600, '#fbbf24', '#06b6d4');

        const fNum = document.getElementById('num-flame'), fRing = document.getElementById('ring-flame'), fCard = document.getElementById('card-flame');
        if (d.flame === 1) {
          fNum.innerText = "FIRE!"; fNum.style.color = "#ef4444"; fRing.style.stroke = "#ef4444"; fCard.style.borderColor = "#ef4444";
        } else {
          fNum.innerText = "CLEAR"; fNum.style.color = "#34d399"; fRing.style.stroke = "#34d399"; fCard.style.borderColor = "rgba(255,255,255,0.08)";
        }

        document.getElementById('diagTag').innerText = d.diagnosis;
        document.getElementById('broadcastText').innerText = d.broadcast;
        const banner = document.getElementById('statusStrip');
        banner.innerText = d.status;
        banner.className = 'status-strip sev-' + d.severity;

        if (d.severity === 2) playBrowserSiren(); else stopBrowserSiren();

        historyData.push(d);
        if (historyData.length > 40) historyData.shift();

        blackboxLogs.push({ time: new Date().toLocaleTimeString(), mq4: d.mq4, mq2: d.mq2, temp: d.temp, hum: d.hum, soil: d.soil, flame: d.flame, health: d.health, diagnosis: d.diagnosis, status: d.status });
        if (blackboxLogs.length > 200) blackboxLogs.shift();
        drawChart();
      } catch (e) {}
    }
    setInterval(poll, 1000);
    window.addEventListener('resize', drawChart);
  </script>
</body>
</html>
)rawliteral";

// ============================================================================
// FIRMWARE CORE LOGIC
// ============================================================================

void evaluateAtmosphere() {
  if (adminDrillActive) {
    systemSeverity = 2;
    activeThreat = "DRILL";
    gasDiagnosis = "ADMIN SURFACE DRILL ACTIVE";
    hazardText = "CRITICAL: EVACUATION DRILL";
    healthScore = 10;
    return;
  }

  int penalty = 0;
  if (mq4Val > 150) penalty += map(constrain(mq4Val, 150, 600), 150, 600, 0, 45);
  if (mq2Val > 100) penalty += map(constrain(mq2Val, 100, 400), 100, 400, 0, 35);
  if (tempVal > 35.0 && tempVal > 0) penalty += (tempVal - 35.0) * 3;
  if (flameVal == 1) penalty = 100;
  healthScore = constrain(100 - penalty, 0, 100);

  if (flameVal == 1) {
    systemSeverity = 2; activeThreat = "FLAME";
    gasDiagnosis = "OPEN FLAME / COMBUSTION"; hazardText = "CRITICAL: ACTIVE FLAME";
  } else if (mq4Val >= MQ4_LIMIT && mq2Val >= MQ2_LIMIT) {
    systemSeverity = 2; activeThreat = "MULTI_GAS";
    gasDiagnosis = "COAL COMBUSTION / OXIDATION"; hazardText = "CRITICAL: EXPLOSIVE MIX";
  } else if (mq4Val >= MQ4_LIMIT) {
    systemSeverity = 2; activeThreat = "METHANE";
    gasDiagnosis = "METHANE POCKET INFLUX"; hazardText = "CRITICAL: METHANE LEL";
  } else if (mq2Val >= MQ2_LIMIT) {
    systemSeverity = 2; activeThreat = "SMOKE";
    gasDiagnosis = "CABLE INSULATION / FIRE"; hazardText = "CRITICAL: TOXIC SMOKE";
  } else if (mq4Val > (MQ4_LIMIT * 0.75) || mq2Val > (MQ2_LIMIT * 0.75)) {
    systemSeverity = 1; activeThreat = "ELEVATED";
    gasDiagnosis = "PRE-HAZARD RISE"; hazardText = "WARNING: GAS ELEVATED";
  } else {
    systemSeverity = 0; activeThreat = "NONE";
    gasDiagnosis = "ATMOSPHERE NOMINAL"; hazardText = "SYSTEM SECURE";
  }
}

void parseTelemetry(String data) {
  int c1 = data.indexOf(',');
  int c2 = data.indexOf(',', c1 + 1);
  int c3 = data.indexOf(',', c2 + 1);
  int c4 = data.indexOf(',', c3 + 1);
  int c5 = data.indexOf(',', c4 + 1);

  if (c1 != -1 && c2 != -1 && c3 != -1 && c4 != -1 && c5 != -1) {
    mq2Val   = data.substring(0, c1).toInt();
    mq4Val   = data.substring(c1 + 1, c2).toInt();
    soilVal  = data.substring(c2 + 1, c3).toInt();
    tempVal  = data.substring(c3 + 1, c4).toFloat();
    humVal   = data.substring(c4 + 1, c5).toInt();
    flameVal = data.substring(c5 + 1).toInt();

    lastSerialTime = millis();
    linkLost = false;
    evaluateAtmosphere();
  }
}

void processAlarmAudio() {
  if (millis() < buzzerMuteUntil || systemSeverity == 0) {
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }
  unsigned long now = millis();
  if (activeThreat == "FLAME") {
    if (now - lastToneToggle > 75) {
      lastToneToggle = now;
      toneToggleState = !toneToggleState;
      if (toneToggleState) tone(BUZZER_PIN, 2800); else noTone(BUZZER_PIN);
    }
  } else if (activeThreat == "METHANE" || activeThreat == "MULTI_GAS" || activeThreat == "DRILL") {
    if (now - lastToneToggle > 200) {
      lastToneToggle = now;
      toneToggleState = !toneToggleState;
      tone(BUZZER_PIN, toneToggleState ? 2400 : 1750);
    }
  } else {
    if (now - lastToneToggle > 380) {
      lastToneToggle = now;
      toneToggleState = !toneToggleState;
      if (toneToggleState) tone(BUZZER_PIN, 1300); else noTone(BUZZER_PIN);
    }
  }
}

void handleDataJson() {
  String json = "{";
  json += "\"mq2\":" + String(mq2Val) + ",";
  json += "\"mq4\":" + String(mq4Val) + ",";
  json += "\"soil\":" + String(soilVal) + ",";
  json += "\"temp\":" + String(tempVal, 1) + ",";
  json += "\"hum\":" + String(humVal) + ",";
  json += "\"flame\":" + String(flameVal) + ",";
  json += "\"health\":" + String(healthScore) + ",";
  json += "\"severity\":" + String(systemSeverity) + ",";
  json += "\"diagnosis\":\"" + gasDiagnosis + "\",";
  json += "\"status\":\"" + hazardText + "\",";
  json += "\"broadcast\":\"" + adminBroadcastMsg + "\",";
  json += "\"drill\":" + String(adminDrillActive ? "true" : "false") + ",";
  json += "\"mq4Limit\":" + String(MQ4_LIMIT) + ",";
  json += "\"mq2Limit\":" + String(MQ2_LIMIT) + ",";
  json += "\"linkLost\":" + String(linkLost ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleAdminAction() {
  String user = server.arg("user");
  String pass = server.arg("pass");
  if (user != ADMIN_USER || pass != ADMIN_PASS) {
    server.send(403, "text/plain", "AUTH_DENIED");
    return;
  }
  String action = server.arg("action");
  if (action == "verify") {
    server.send(200, "text/plain", "VERIFIED");
    return;
  } else if (action == "drill") {
    adminDrillActive = !adminDrillActive;
    if (adminDrillActive) adminBroadcastMsg = "EVACUATION: PROCEED TO SHAFT 1";
    else adminBroadcastMsg = "Drill clear. Shaft normal.";
    evaluateAtmosphere();
  } else if (action == "mute") {
    buzzerMuteUntil = millis() + 45000;
  } else if (action == "reset") {
    adminDrillActive = false;
    buzzerMuteUntil = 0;
    adminBroadcastMsg = "Standard operations active.";
    evaluateAtmosphere();
  } else if (action == "broadcast") {
    if (server.hasArg("msg")) adminBroadcastMsg = server.arg("msg");
  }
  server.send(200, "text/plain", "SUCCESS");
}

void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent_P(PAGE_PART1);
  server.sendContent_P(PAGE_PART2);
  server.sendContent("");
}

void setup() {
  Serial.begin(115200);
  megaSerial.begin(9600);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(4, 5); // SDA = D2, SCL = D1
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("CONNECTING WIFI");

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
  }

  server.on("/", handleRoot);
  server.on("/data", handleDataJson);
  server.on("/adminAction", handleAdminAction);
  server.begin();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP Address:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(3000);
  lcd.clear();
  lastSerialTime = millis() + 10000;
}

void loop() {
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiCheck > 10000) {
    lastWifiCheck = millis();
    WiFi.reconnect();
  }

  if (megaSerial.available()) {
    String incoming = megaSerial.readStringUntil('\n');
    incoming.trim();
    if (incoming.length() > 0) {
      parseTelemetry(incoming);
    }
  }

  if (millis() - lastSerialTime > 5000) {
    linkLost = true;
    systemSeverity = 2;
    activeThreat = "COMM_FAIL";
    hazardText = "LINK LOST: MEGA OFFLINE";
    gasDiagnosis = "UART TIMEOUT";
    healthScore = 0;
  }

  processAlarmAudio();

  lcd.setCursor(0, 0);
  if (linkLost) {
    lcd.print("! LINK ERROR !  ");
  } else if (systemSeverity == 2) {
    lcd.print("! EVACUATE NOW !");
  } else if (systemSeverity == 1) {
    lcd.print("WARNING: GAS UP ");
  } else {
    char hBuf[17];
    snprintf(hBuf, sizeof(hBuf), "SAFE | HLTH:%-3d%%", healthScore);
    lcd.print(hBuf);
  }

  lcd.setCursor(0, 1);
  char buf[17];
  snprintf(buf, sizeof(buf), "M4:%-4d S2:%-4d", mq4Val, mq2Val);
  lcd.print(buf);

  delay(20);
}
