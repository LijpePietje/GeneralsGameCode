// ── Terrain panel ─────────────────────────────────────────────────────────────
function bindTerrainSlider(sliderId, valId, apiProp) {
  const slider = document.getElementById(sliderId);
  const val    = document.getElementById(valId);
  if (!slider) return;
  slider.addEventListener('input',  () => { val.textContent = slider.value; });
  slider.addEventListener('change', () => api('/terrain/set', { [apiProp]: Number(slider.value) }));
}

// Height value is remembered per category: relative step (raise/lower) and
// absolute target (flatten/stamp) are separate settings in WB too.
const _terHeightCfg = {
  step:   { min: 1, max: 30,  val: 5 },
  target: { min: 0, max: 255, val: 20 },
};
let _terMode = 'raise';

function _terHeightCat(mode) {
  return (mode === 'raise' || mode === 'lower') ? 'step' : 'target';
}

function _setBrushUIVisible(visible) {
  const ids = ['terrain-brush-divider-0', 'terrain-brush-section',
               'terrain-brush-divider-1', 'terrain-height-section',
               'terrain-smooth-section',  'terrain-brush-divider-2',
               'terrain-hint-section'];
  ids.forEach(id => {
    const el = document.getElementById(id);
    if (el) el.style.display = visible ? '' : 'none';
  });
  const moldDiv = document.getElementById('terrain-mold-divider');
  const moldSec = document.getElementById('terrain-mold-section');
  if (moldDiv) moldDiv.style.display = visible ? 'none' : '';
  if (moldSec) moldSec.style.display = visible ? 'none' : '';
}

function updateTerrainModeUI(mode) {
  _terMode = mode;

  if (mode === 'mesh-mold') {
    _setBrushUIVisible(false);
    // Set position from view center first so preview appears there, then activate
    (async () => {
      await moldUseViewCenter();
      api('/meshmold/activate', {});
    })();
    if (document.getElementById('mold-select').options.length === 0 ||
        document.getElementById('mold-select').options[0]?.text === '— loading —')
      moldLoadList();
    return;
  }

  _setBrushUIVisible(true);

  const heightSec  = document.getElementById('terrain-height-section');
  const smoothSec  = document.getElementById('terrain-smooth-section');
  const heightLbl  = document.getElementById('terrain-height-label');

  heightSec.style.display = (mode !== 'smooth') ? '' : 'none';
  smoothSec.style.display = (mode === 'smooth') ? '' : 'none';

  // Smooth (WB feather tool) ignores brush feather and shape — hide the dead controls
  const featherRow = document.getElementById('sl-terrain-feather')?.closest('.slider-row');
  const shapeGrp   = document.getElementById('terrain-brush-shape')?.parentElement;
  if (featherRow) featherRow.style.display = (mode === 'smooth') ? 'none' : '';
  if (shapeGrp)   shapeGrp.style.display   = (mode === 'smooth') ? 'none' : '';

  const isStep = (mode === 'raise' || mode === 'lower');
  heightLbl.textContent   = isStep ? 'HEIGHT STEP' : 'TARGET HEIGHT';

  // Swap slider range + value to the category's remembered setting
  const cfg = _terHeightCfg[_terHeightCat(mode)];
  const sl  = document.getElementById('sl-terrain-height');
  const num = document.getElementById('num-terrain-height');
  if (sl && num) {
    sl.min = cfg.min;  sl.max = cfg.max;  sl.value = cfg.val;
    num.min = cfg.min; num.max = cfg.max; num.value = cfg.val;
  }

  // Raise/Lower are RELATIVE (adds/digs per brush tick), Flatten/Stamp are
  // ABSOLUTE (paints terrain to exactly this height) — spell that out.
  const hint = document.getElementById('terrain-height-hint');
  if (hint) {
    if (mode === 'raise')      hint.textContent = 'Added per brush tick — keep low (2–10) for gradual hills; high values jump instantly';
    else if (mode === 'lower') hint.textContent = 'Dug out per brush tick — keep low (2–10) for gradual dips; high values jump instantly';
    else                       hint.textContent = 'Paints terrain to exactly this height — no visible change if it already is that height';
  }
}

// Clamp + sync slider/number field, remember per category, push to WB
function _terCommitHeight(raw) {
  const cfg = _terHeightCfg[_terHeightCat(_terMode)];
  let v = Math.round(Number(raw));
  if (isNaN(v)) v = cfg.min;
  v = Math.max(cfg.min, Math.min(cfg.max, v));
  cfg.val = v;
  document.getElementById('sl-terrain-height').value  = v;
  document.getElementById('num-terrain-height').value = v;
  api('/terrain/set', { height: v });
}

// Map size badge + New/Resize buttons moved to the Map Setup tab; this hook
// (still called after create/resize/startscreen) refreshes that panel instead.
async function refreshTerrainMapSize() {
  if (typeof _msFetchData === 'function') _msFetchData();
}

// ── New Map modal ─────────────────────────────────────────────────────────────
function updateNewmapPlayable() {
  const x = parseInt(document.getElementById('newmap-x').value) || 200;
  const y = parseInt(document.getElementById('newmap-y').value) || 200;
  const b = parseInt(document.getElementById('newmap-border').value) || 10;
  document.getElementById('newmap-playable').textContent = `${x - 2*b} × ${y - 2*b}`;
}

function openNewMapModal() {
  ipcRenderer.send('viewport-mouse', false);
  updateNewmapPlayable();
  document.getElementById('modal-newmap').classList.remove('hidden');
}
function closeNewMapModal() {
  document.getElementById('modal-newmap').classList.add('hidden');
}

async function doCreateMap() {
  const x = parseInt(document.getElementById('newmap-x').value);
  const y = parseInt(document.getElementById('newmap-y').value);
  const border = parseInt(document.getElementById('newmap-border').value);
  const height = parseInt(document.getElementById('newmap-height').value);
  const st = document.getElementById('newmap-status');
  st.textContent = 'Creating…';
  const r = await api('/map/new', { x, y, border, height });
  if (r && r.ok) {
    st.textContent = '';
    closeNewMapModal();
    setTimeout(refreshTerrainMapSize, 1200);
    autoAddSkirmishPlayers();
  } else {
    st.textContent = 'Failed — is WB running?';
  }
}

// ── Resize Map modal ──────────────────────────────────────────────────────────
let _resizeAnchor = 'mc';

function updateResizePlayable() {
  const x = parseInt(document.getElementById('resize-x').value) || 200;
  const y = parseInt(document.getElementById('resize-y').value) || 200;
  const b = parseInt(document.getElementById('resize-border').value) || 10;
  document.getElementById('resize-playable').textContent = `${x - 2*b} × ${y - 2*b}`;
}

function openResizeModal() {
  ipcRenderer.send('viewport-mouse', false);
  // Pre-fill with current map size if available
  apiGet('/map/info').then(info => {
    if (info && info.width) {
      const border = info.borderSize || 10;
      const playW = info.width  - 2 * border;
      const playH = info.height - 2 * border;
      document.getElementById('resize-x').value = playW;
      document.getElementById('resize-y').value = playH;
      document.getElementById('resize-border').value = border;
    }
    updateResizePlayable();
  }).catch(() => updateResizePlayable());
  document.getElementById('modal-resize').classList.remove('hidden');
}
function closeResizeModal() {
  document.getElementById('modal-resize').classList.add('hidden');
}

async function doResizeMap() {
  const x = parseInt(document.getElementById('resize-x').value);
  const y = parseInt(document.getElementById('resize-y').value);
  const border = parseInt(document.getElementById('resize-border').value);
  const height = parseInt(document.getElementById('resize-height').value);
  const st = document.getElementById('resize-status');
  st.textContent = 'Resizing…';
  const r = await api('/map/resize', { x, y, border, height, anchor: _resizeAnchor });
  if (r && r.ok) {
    st.textContent = '';
    closeResizeModal();
    setTimeout(refreshTerrainMapSize, 1200);
  } else {
    st.textContent = 'Failed — is WB running?';
  }
}

function initNewMapModal() {
  ['newmap-x','newmap-y','newmap-border'].forEach(id =>
    document.getElementById(id).addEventListener('input', updateNewmapPlayable));
  document.getElementById('btn-modal-newmap-close').addEventListener('click', closeNewMapModal);
  document.getElementById('btn-modal-newmap-cancel').addEventListener('click', closeNewMapModal);
  document.getElementById('btn-modal-newmap-create').addEventListener('click', doCreateMap);
  document.getElementById('modal-newmap').addEventListener('click', e => {
    if (e.target === e.currentTarget) closeNewMapModal();
  });
}

function initResizeModal() {
  ['resize-x','resize-y','resize-border'].forEach(id =>
    document.getElementById(id).addEventListener('input', updateResizePlayable));
  document.getElementById('btn-modal-resize-close').addEventListener('click', closeResizeModal);
  document.getElementById('btn-modal-resize-cancel').addEventListener('click', closeResizeModal);
  document.getElementById('btn-modal-resize-apply').addEventListener('click', doResizeMap);
  document.getElementById('modal-resize').addEventListener('click', e => {
    if (e.target === e.currentTarget) closeResizeModal();
  });
  // Anchor grid
  document.querySelectorAll('.anchor-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.anchor-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      _resizeAnchor = btn.dataset.anchor;
    });
  });
}

function initTerrainPanel() {
  // New Map / Resize modals live on the Map Setup tab, but their wiring stays here
  initNewMapModal();
  initResizeModal();

  document.querySelectorAll('.terrain-tool-btn').forEach(btn => {
    btn.addEventListener('click', async () => {
      document.querySelectorAll('.terrain-tool-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      const mode = btn.dataset.tmode;
      updateTerrainModeUI(mode);
      if (mode !== 'mesh-mold') {
        // Await mode switch first: /terrain/set dispatches on the server-side mode
        await api('/terrain/mode', { mode });
        if (mode !== 'smooth') api('/terrain/set', { height: _terHeightCfg[_terHeightCat(mode)].val });
      }
      document.getElementById('status-tool').textContent = `Terrain · ${mode}`;
    });
  });

  updateTerrainModeUI('raise'); // initial hint + range for the default active mode

  // Height: slider and number field are two-way bound, committed via _terCommitHeight
  const slH  = document.getElementById('sl-terrain-height');
  const numH = document.getElementById('num-terrain-height');
  slH.addEventListener('input',   () => { numH.value = slH.value; });
  slH.addEventListener('change',  () => _terCommitHeight(slH.value));
  numH.addEventListener('change', () => _terCommitHeight(numH.value));

  bindTerrainSlider('sl-terrain-size',    'val-terrain-size',    'brushSize');
  bindTerrainSlider('sl-terrain-feather', 'val-terrain-feather', 'feather');
  bindTerrainSlider('sl-terrain-rate',    'val-terrain-rate',    'rate');
  bindTerrainSlider('sl-terrain-radius',  'val-terrain-radius',  'radius');

  document.getElementById('terrain-brush-shape').querySelectorAll('.seg-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.getElementById('terrain-brush-shape').querySelectorAll('.seg-btn')
        .forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      api('/terrain/set', { brushShape: btn.dataset.val });
    });
  });

  initMoldPanel();
}

// ── Terrain Stamps (MeshMold) ─────────────────────────────────────────────────

let _moldState = { scale: 100, scaleX: 100, scaleY: 100, height: 0, angle: 0, mode: 'both' };

function _moldSlider(sliderId, valId, suffix, onCommit) {
  const sl  = document.getElementById(sliderId);
  const lbl = document.getElementById(valId);
  if (!sl) return;
  sl.addEventListener('input',  () => { lbl.textContent = sl.value + suffix; });
  sl.addEventListener('change', () => onCommit(Number(sl.value)));
}

async function moldLoadList() {
  const sel = document.getElementById('mold-select');
  const r = await apiGet('/meshmold/list');
  if (!r || !r.ok || !r.molds) { sel.innerHTML = '<option>— WB not ready —</option>'; return; }
  sel.innerHTML = r.molds.sort().map(m => `<option value="${m}">${m}</option>`).join('');
  // Push current selection to WB
  if (r.molds.length) moldSendModel(sel.value);
}

function moldSendModel(name) {
  if (name) api('/meshmold/set', { model: name });
}

async function moldApply() {
  const hint = document.getElementById('mold-hint');

  // A very large mold (big radius x high scale) over a big map can take WB a long
  // time to rasterize - found the hard way when an 86x-larger footprint turned a
  // ~1s apply into 20+ minutes with zero feedback in WB itself. Warn up front so
  // "it's still working" isn't mistaken for "it's stuck".
  const mapInfo = await apiGet('/map/info');
  const scaleFrac = Math.max(_moldState.scale, _moldState.scaleX, _moldState.scaleY) / 100;
  if (mapInfo && mapInfo.ok && scaleFrac > 3) {
    const proceed = confirm(
      `Scale is ${Math.round(scaleFrac * 100)}% - a mold this large relative to its base size ` +
      `can take WorldBuilder a long time to apply (minutes, not seconds) on a big map, with no ` +
      `progress shown in WB itself while it works.\n\nApply anyway?`
    );
    if (!proceed) { hint.textContent = 'Apply cancelled'; return; }
  }

  // No posX/posY here: the stamp lands where the preview is (WB's tool position).
  // Position is only pushed when the user edits Pos X/Y or clicks ⊕.
  await api('/meshmold/set', {
    model:     document.getElementById('mold-select').value,
    scale:     _moldState.scale / 100,
    scaleX:    _moldState.scaleX / 100,
    scaleY:    _moldState.scaleY / 100,
    height:    _moldState.height,
    angle:     _moldState.angle,
    raiseOnly: _moldState.mode === 'raise' ? 1 : 0,
    lowerOnly: _moldState.mode === 'lower' ? 1 : 0,
  });

  // Height at the stamp position before, so the post-apply message can show a real
  // before/after delta instead of just "ok" - the only way to catch a "technically
  // applied but the height barely changed" result without leaving this panel.
  const px = parseFloat(document.getElementById('mold-pos-x').value) || 0;
  const py = parseFloat(document.getElementById('mold-pos-y').value) || 0;
  const before = await apiGet(`/map/ground_height?wx=${px}&wy=${py}`);
  const beforeH = (before && before.ok) ? before.ground : null;

  // Elapsed-time readout so a slow apply reads as "still working" rather than
  // "the UI died" - updates every second until the action call resolves.
  const startTime = Date.now();
  hint.textContent = 'Applying…';
  const tickTimer = setInterval(() => {
    const secs = Math.round((Date.now() - startTime) / 1000);
    hint.textContent = secs < 5 ? 'Applying…' : `Applying… (${secs}s - large molds can take a while)`;
  }, 1000);

  const r = await api('/meshmold/action', {});
  clearInterval(tickTimer);

  if (r && r.ok) {
    const after = await apiGet(`/map/ground_height?wx=${px}&wy=${py}`);
    const afterH = (after && after.ok) ? after.ground : null;
    const deltaText = (beforeH != null && afterH != null)
      ? ` · height at stamp: ${beforeH} → ${afterH}`
      : '';
    hint.textContent = `✓ Mold applied where the preview was${deltaText} · Ctrl+Z to undo`;
  } else {
    hint.textContent = '✗ Failed — is WB open?';
  }
  setTimeout(() => { hint.textContent = 'Select a mold · set scale/angle · click Apply'; }, 8000);
}

async function moldUseViewCenter() {
  // World position at the center of the viewport (WB window overlays Electron 1:1)
  const r = await api('/view/screen_to_world', {
    sx: Math.round(window.innerWidth / 2),
    sy: Math.round(window.innerHeight / 2),
  });
  if (!r || !r.ok || r.wx == null) return;
  const px = Math.round(r.wx);
  const py = Math.round(r.wy);
  document.getElementById('mold-pos-x').value = px;
  document.getElementById('mold-pos-y').value = py;
  // Send to WB — this also triggers preview update via the POS_Y handler
  await api('/meshmold/set', { posX: px, posY: py });
  // First placement snaps mold height to terrain in WB; reflect that in the slider
  const st = await apiGet('/meshmold/state');
  if (st && st.ok && typeof st.height === 'number') {
    const h = Math.round(st.height / 100);
    _moldState.height = h;
    const sl = document.getElementById('sl-mold-height');
    sl.value = h;
    document.getElementById('val-mold-height').textContent = h;
  }
}

function initMoldPanel() {
  // Sliders — push to WB immediately so the live preview follows
  _moldSlider('sl-mold-scale',  'val-mold-scale',  '%', v => { _moldState.scale  = v; api('/meshmold/set', { scale:  v / 100 }); });
  _moldSlider('sl-mold-scalex', 'val-mold-scalex', '%', v => { _moldState.scaleX = v; api('/meshmold/set', { scaleX: v / 100 }); });
  _moldSlider('sl-mold-scaley', 'val-mold-scaley', '%', v => { _moldState.scaleY = v; api('/meshmold/set', { scaleY: v / 100 }); });
  _moldSlider('sl-mold-height', 'val-mold-height', '',  v => { _moldState.height = v; api('/meshmold/set', { height: v }); });
  _moldSlider('sl-mold-angle',  'val-mold-angle',  '°', v => { _moldState.angle  = v; api('/meshmold/set', { angle:  v }); });

  // Mold picker
  document.getElementById('mold-select').addEventListener('change', e => moldSendModel(e.target.value));

  // Mode buttons
  document.getElementById('mold-mode-group').querySelectorAll('.seg-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.getElementById('mold-mode-group').querySelectorAll('.seg-btn')
        .forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      _moldState.mode = btn.dataset.moldMode;
      api('/meshmold/set', {
        raiseOnly: _moldState.mode === 'raise' ? 1 : 0,
        lowerOnly: _moldState.mode === 'lower' ? 1 : 0,
      });
    });
  });

  // When user manually edits Pos X/Y, push to WB so preview updates
  ['mold-pos-x', 'mold-pos-y'].forEach(id => {
    document.getElementById(id).addEventListener('change', () => {
      api('/meshmold/set', {
        posX: parseFloat(document.getElementById('mold-pos-x').value) || 0,
        posY: parseFloat(document.getElementById('mold-pos-y').value) || 0,
      });
    });
  });

  document.getElementById('mold-apply-btn').addEventListener('click', moldApply);
  document.getElementById('mold-center-btn').addEventListener('click', moldUseViewCenter);
}

