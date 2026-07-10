const { ipcRenderer } = require('electron');

// ── Window controls ──
document.getElementById('btn-close').onclick = () => ipcRenderer.send('win-close');
document.getElementById('btn-min').onclick   = () => ipcRenderer.send('win-minimize');
document.getElementById('btn-max').onclick   = () => ipcRenderer.send('win-maximize');

// ── WorldBuilder status ──
const overlay    = document.getElementById('wb-overlay');
const overlayTxt = document.getElementById('wb-overlay-text');
const btnLaunch  = document.getElementById('btn-launch-wb');
const mapCanvas  = document.getElementById('map-canvas');

overlayTxt.textContent = 'WorldBuilder starten...';

ipcRenderer.on('wb-status', (_, status) => {
  if (status === 'loading') {
    overlay.classList.remove('hidden');
    overlayTxt.textContent = 'WorldBuilder starten...';
  } else if (status === 'ready') {
    overlay.classList.add('hidden');
    document.body.classList.add('wb-embedded');
    document.getElementById('status-tool').textContent = 'Gereed';
    setViewMode('3d');
    async function switchTo2D(attempts) {
      const result = await api('/view/mode', { mode: '2d' });
      if (result && result.ok) {
        setViewMode('2d');
        await activateShapeFill();
        startSfPolling();
        loadTextures();
        refreshMapInfo();
      } else if (attempts > 0) {
        setTimeout(() => switchTo2D(attempts - 1), 800);
      }
    }
    setTimeout(() => switchTo2D(5), 800);
  } else if (status === 'error') {
    overlay.classList.add('hidden');
    overlayTxt.textContent = 'Kon WorldBuilder niet starten';
  } else if (status === 'closed') {
    stopSfPolling();
    document.body.classList.remove('wb-embedded');
    overlay.classList.remove('hidden');
    overlayTxt.textContent = 'WorldBuilder gesloten';
    setViewMode('3d');
    drawMap();
  }
});

btnLaunch.addEventListener('click', () => {
  ipcRenderer.send('launch-worldbuilder');
});

// ── Map canvas ──
const canvas = document.getElementById('map-canvas');
const ctx    = canvas.getContext('2d');

function resize() {
  canvas.width  = window.innerWidth;
  canvas.height = window.innerHeight;
  drawMap();
}
window.addEventListener('resize', resize);

function drawMap() {
  const W = canvas.width, H = canvas.height;
  const grad = ctx.createLinearGradient(0, 0, W, H);
  grad.addColorStop(0,   '#1e1608');
  grad.addColorStop(0.4, '#2a1e0a');
  grad.addColorStop(1,   '#18120a');
  ctx.fillStyle = grad;
  ctx.fillRect(0, 0, W, H);

  ctx.strokeStyle = 'rgba(255,255,255,0.04)';
  ctx.lineWidth = 1;
  const grid = 64;
  for (let x = 0; x < W; x += grid) { ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,H); ctx.stroke(); }
  for (let y = 0; y < H; y += grid) { ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(W,y); ctx.stroke(); }

  const patches = [
    { x: 0.12, y: 0.22, rx: 0.18, ry: 0.14, col: '#3d2c0e', a: 0.7 },
    { x: 0.55, y: 0.48, rx: 0.14, ry: 0.10, col: '#3d2c0e', a: 0.55 },
    { x: 0.75, y: 0.18, rx: 0.12, ry: 0.09, col: '#2e2208', a: 0.5 },
    { x: 0.35, y: 0.72, rx: 0.16, ry: 0.10, col: '#2e2208', a: 0.45 },
    { x: 0.88, y: 0.60, rx: 0.10, ry: 0.08, col: '#3d2c0e', a: 0.4 },
  ];
  for (const p of patches) {
    const gp = ctx.createRadialGradient(p.x*W, p.y*H, 0, p.x*W, p.y*H, p.rx*W);
    gp.addColorStop(0, p.col + 'cc');
    gp.addColorStop(1, p.col + '00');
    ctx.globalAlpha = p.a;
    ctx.fillStyle = gp;
    ctx.beginPath();
    ctx.ellipse(p.x*W, p.y*H, p.rx*W, p.ry*H, 0, 0, Math.PI*2);
    ctx.fill();
  }
  ctx.globalAlpha = 1;

  ctx.strokeStyle = 'rgba(120, 90, 20, 0.35)';
  ctx.lineWidth = 6;
  ctx.beginPath();
  ctx.moveTo(0, H * 0.52);
  ctx.bezierCurveTo(W*0.3, H*0.48, W*0.7, H*0.56, W, H*0.52);
  ctx.stroke();

  const sx = W * 0.28, sy = H * 0.26, sw = W * 0.26, sh = H * 0.28;
  ctx.fillStyle = 'rgba(74, 158, 255, 0.07)';
  ctx.fillRect(sx, sy, sw, sh);
  ctx.fillStyle = 'rgba(74, 158, 255, 0.04)';
  ctx.fillRect(sx+20, sy+20, sw-40, sh-40);
  ctx.strokeStyle = 'rgba(74, 158, 255, 0.3)';
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 4]);
  ctx.strokeRect(sx+20, sy+20, sw-40, sh-40);
  ctx.setLineDash([]);
  ctx.strokeStyle = '#4a9eff';
  ctx.lineWidth = 2;
  ctx.strokeRect(sx, sy, sw, sh);
  for (const [hx, hy] of [[sx,sy],[sx+sw,sy],[sx,sy+sh],[sx+sw,sy+sh]]) {
    ctx.fillStyle = '#ffffff';
    ctx.fillRect(hx-5, hy-5, 10, 10);
    ctx.strokeStyle = '#4a9eff';
    ctx.lineWidth = 2;
    ctx.strokeRect(hx-5, hy-5, 10, 10);
  }
  ctx.globalAlpha = 1;

  ctx.fillStyle = 'rgba(255,255,255,0.18)';
  ctx.font = '11px Segoe UI, sans-serif';
  ctx.fillText('X: 284  Y: 190', 12, H - 32);
}

// ── REST API helpers ──
const API_BASE = 'http://127.0.0.1:8099';
const API = API_BASE + '/api';
async function api(endpoint, body) {
  try {
    const res = await fetch(`${API}${endpoint}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    return await res.json();
  } catch (e) {
    console.warn('API call failed:', endpoint, e.message);
    return { ok: false };
  }
}
async function apiGet(endpoint) {
  try {
    const res = await fetch(`${API}${endpoint}`, { method: 'GET' });
    return await res.json();
  } catch (e) {
    console.warn('API GET failed:', endpoint, e.message);
    return { ok: false };
  }
}

