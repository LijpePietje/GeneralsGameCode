// ── Lighting panel ────────────────────────────────────────────────────────────
// Global light editing: time of day, per-light colors (ambient/diffuse) and
// sun direction. Mirrors WB's Global Light Options via /api/map/lighting.

let _ltData   = null;   // last lighting_get response
let _ltLight  = 0;      // 0=sun, 1=accent1, 2=accent2
let _ltSendTimer = null;

function _ltHexToRgb(hex) {
  const n = parseInt(hex.slice(1), 16);
  return { r: (n >> 16) & 255, g: (n >> 8) & 255, b: n & 255 };
}
function _ltRgbToHex(r, g, b) {
  const c = v => Math.max(0, Math.min(255, v | 0)).toString(16).padStart(2, '0');
  return `#${c(r)}${c(g)}${c(b)}`;
}
// Light direction vector -> compass azimuth (0-360) + elevation (0-90, 90 = straight down)
function _ltPosToAngles(pos) {
  const [x, y, z] = pos;
  let az = Math.atan2(y, x) * 180 / Math.PI;
  if (az < 0) az += 360;
  const el = Math.acos(Math.max(-1, Math.min(1, z))) * 180 / Math.PI - 90;
  return { az: Math.round(az), el: Math.round(Math.max(0, Math.min(90, el))) };
}

function _ltStatus(msg) {
  const el = document.getElementById('lt-status');
  el.textContent = msg;
  if (msg) setTimeout(() => { if (el.textContent === msg) el.textContent = ''; }, 3000);
}

function _ltTarget() { return document.getElementById('lt-target').value; }

// Fill the controls from _ltData for the selected light + target
function _ltPopulate() {
  if (!_ltData?.ok) return;
  const arr = (_ltTarget() === 'objects') ? _ltData.objects : _ltData.terrain;
  const l = arr?.[_ltLight];
  if (!l) return;

  document.getElementById('lt-diffuse').value = _ltRgbToHex(...l.diffuse);
  document.getElementById('lt-ambient').value = _ltRgbToHex(...l.ambient);

  const { az, el } = _ltPosToAngles(l.pos);
  document.getElementById('lt-azimuth').value = az;
  document.getElementById('lt-elevation').value = el;
  document.getElementById('lt-azimuth-val').textContent = `${az}°`;
  document.getElementById('lt-elevation-val').textContent = `${el}°`;

  // Ambient only contributes on the sun light (first global light)
  const isSun = _ltLight === 0;
  document.getElementById('lt-ambient').disabled = !isSun;
  document.getElementById('lt-ambient-hint').style.display = isSun ? 'none' : '';

  // Active time-of-day button
  document.querySelectorAll('.lt-tod-btn').forEach(b =>
    b.classList.toggle('active', parseInt(b.dataset.tod) === _ltData.timeOfDay));
}

async function refreshLightingPanel() {
  _ltData = await apiGet('/map/lighting');
  if (!_ltData?.ok) { _ltStatus('WB not connected'); return; }
  _ltPopulate();
}

// Push the full state of the selected light (debounced for slider drags)
function _ltSend() {
  clearTimeout(_ltSendTimer);
  _ltSendTimer = setTimeout(async () => {
    const dif = _ltHexToRgb(document.getElementById('lt-diffuse').value);
    const amb = _ltHexToRgb(document.getElementById('lt-ambient').value);
    const body = {
      target:    _ltTarget(),
      light:     _ltLight,
      difR: dif.r, difG: dif.g, difB: dif.b,
      azimuth:   parseInt(document.getElementById('lt-azimuth').value),
      elevation: parseInt(document.getElementById('lt-elevation').value),
    };
    if (_ltLight === 0) { body.ambR = amb.r; body.ambG = amb.g; body.ambB = amb.b; }
    const r = await api('/map/lighting', body);
    _ltStatus(r?.ok ? 'Applied' : (r?.error || 'Failed'));
    // Keep local cache in sync so light/target switches show fresh values
    _ltData = await apiGet('/map/lighting');
  }, 120);
}

function initLightingPanel() {
  document.querySelectorAll('.lt-tod-btn').forEach(btn => {
    btn.addEventListener('click', async () => {
      const r = await api('/map/lighting', { timeOfDay: parseInt(btn.dataset.tod) });
      _ltStatus(r?.ok ? 'Time of day switched' : (r?.error || 'Failed'));
      await refreshLightingPanel();
    });
  });

  document.querySelectorAll('.lt-light-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.lt-light-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      _ltLight = parseInt(btn.dataset.light);
      _ltPopulate();
    });
  });

  document.getElementById('lt-target').addEventListener('change', _ltPopulate);
  document.getElementById('lt-diffuse').addEventListener('input', _ltSend);
  document.getElementById('lt-ambient').addEventListener('input', _ltSend);

  document.getElementById('lt-reset').addEventListener('click', async () => {
    const r = await api('/map/lighting/reset', {});
    _ltStatus(r?.ok ? 'Restored factory defaults' : (r?.error || 'Failed'));
    await refreshLightingPanel();
  });

  const az = document.getElementById('lt-azimuth');
  const el = document.getElementById('lt-elevation');
  az.addEventListener('input', () => {
    document.getElementById('lt-azimuth-val').textContent = `${az.value}°`;
    _ltSend();
  });
  el.addEventListener('input', () => {
    document.getElementById('lt-elevation-val').textContent = `${el.value}°`;
    _ltSend();
  });
}
