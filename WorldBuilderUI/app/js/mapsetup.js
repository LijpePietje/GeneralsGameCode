// ── Map Setup panel ───────────────────────────────────────────────────────────

let _msPollTimer = null;
let _msMode      = localStorage.getItem('mapsetup_mode') || 'skirmish';
let _msPvp       = localStorage.getItem('mapsetup_pvp')  || '2v2';
let _msCoop      = localStorage.getItem('mapsetup_coop') || 'aod';
let _msWaypoints = [];
let _msMapInfo   = null;

const PVP_STARTS = { '1v1': 2, '2v2': 4, '3v3': 6, '4v4': 8, 'ffa': null };

function initMapSetupPanel() {
  document.querySelectorAll('[data-msmode]').forEach(btn => {
    btn.addEventListener('click', () => {
      _msMode = btn.dataset.msmode;
      localStorage.setItem('mapsetup_mode', _msMode);
      _msUpdateModeUI();
      _msRefreshStatus();
    });
  });

  document.querySelectorAll('[data-pvp]').forEach(btn => {
    btn.addEventListener('click', () => {
      _msPvp = btn.dataset.pvp;
      localStorage.setItem('mapsetup_pvp', _msPvp);
      document.querySelectorAll('[data-pvp]').forEach(b => b.classList.toggle('active', b.dataset.pvp === _msPvp));
      _msRefreshStatus();
    });
  });

  document.querySelectorAll('[data-coop]').forEach(btn => {
    btn.addEventListener('click', () => {
      _msCoop = btn.dataset.coop;
      localStorage.setItem('mapsetup_coop', _msCoop);
      document.querySelectorAll('[data-coop]').forEach(b => b.classList.toggle('active', b.dataset.coop === _msCoop));
    });
  });

  document.getElementById('ms-btn-new').addEventListener('click', openNewMapModal);
  document.getElementById('ms-btn-resize').addEventListener('click', openResizeModal);

  document.getElementById('ms-btn-place-starts').addEventListener('click', () => {
    _msToggleBox('ms-ps-box');
  });
  // ms-coop-* / ms-btn-losezone / ms-btn-waves / ms-mission-* live in the Co-op and
  // Mission sections, currently replaced with a "Coming soon" placeholder (their
  // markup is kept, commented, in index.html) - guard with ?. so a missing element
  // doesn't throw and break the rest of this init function.
  document.getElementById('ms-coop-btn-starts')?.addEventListener('click', () => {
    _msToggleBox('ms-coop-ps-box');
  });
  document.getElementById('ms-btn-income').addEventListener('click', () => {
    _msToggleBox('ms-inc-box');
  });
  document.getElementById('ms-coop-btn-income')?.addEventListener('click', () => {
    _msToggleBox('ms-coop-inc-box');
  });
  document.getElementById('ms-btn-extras').addEventListener('click', () => {
    _msToggleBox('ms-ext-box');
  });
  document.getElementById('ms-coop-btn-extras')?.addEventListener('click', () => {
    _msToggleBox('ms-coop-ext-box');
  });
  _msBuildOptGrids();
  document.getElementById('ms-btn-losezone')?.addEventListener('click', () => {
    _msSwitchTo('waypoints');
    setWpMode('area');
    const el = document.getElementById('wp-area-name');
    if (el && !el.value) el.value = 'LoseZone';
  });
  document.getElementById('ms-btn-waves')?.addEventListener('click', () => {
    _msSwitchTo('sidelist');
  });
  document.getElementById('ms-mission-starts')?.addEventListener('click', () => {
    _msSwitchTo('waypoints');
  });
  document.getElementById('ms-mission-scripts')?.addEventListener('click', () => {
    _msSwitchTo('sidelist');
  });

  _msUpdateModeUI();
  document.querySelectorAll('[data-pvp]').forEach(b => b.classList.toggle('active', b.dataset.pvp === _msPvp));
  document.querySelectorAll('[data-coop]').forEach(b => b.classList.toggle('active', b.dataset.coop === _msCoop));
}

// ── Map Setup dropdowns (accordion: opening one collapses the others) ─────────
const _MS_BOXES = ['ms-ps-box', 'ms-coop-ps-box', 'ms-inc-box', 'ms-coop-inc-box', 'ms-ext-box', 'ms-coop-ext-box'];

function _msToggleBox(id) {
  const box = document.getElementById(id);
  if (!box) return;
  const wasOpen = box.style.display !== 'none';
  for (const b of _MS_BOXES) {
    const el = document.getElementById(b);
    if (el) el.style.display = 'none';
  }
  // Collapsing (or switching) a box aborts whatever placement it hosted
  if (_wpPsActive)  cancelPlayerStartPlace();
  if (_msQpActive) msCancelQuickPlace();
  if (!wasOpen) {
    box.style.display = '';
    if (id.includes('-ps-')) syncWaypointsState(); // fetch waypoints so the grid shows current state
  }
}

// ── Quick placement: Income & Extras dropdowns ────────────────────────────────
// Template names verified against REAL_OBJECT_NAMES.txt
const MS_PLACE_GROUPS = {
  income: [
    { key: 'oil',       template: 'TechOilDerrick',  label: 'Oil Derrick' },
    { key: 'warehouse', template: 'SupplyWarehouse', label: 'Supply Warehouse' },
    { key: 'dock',      template: 'SupplyDock',      label: 'Supply Dock' },
    { key: 'pile',      template: 'SupplyPile',      label: 'Supply Pile' },
    { key: 'pilesmall', template: 'SupplyPileSmall', label: 'Supply Pile (small)' },
  ],
  extras: [
    { key: 'hospital',  template: 'TechHospital',         label: 'Hospital' },
    { key: 'reinforce', template: 'TechReinforcementPad', label: 'Reinforcement Pad' },
    { key: 'repairbay', template: 'TechRepairbay',        label: 'Repair Bay' },
    { key: 'refinery',  template: 'TechOilRefinery',      label: 'Oil Refinery' },
  ],
};
const _msPlaceByKey = {};
for (const defs of Object.values(MS_PLACE_GROUPS)) {
  for (const d of defs) _msPlaceByKey[d.key] = d;
}
// Landmark bridges from the Roads tab use the same quick-place flow
if (typeof RD_LANDMARKS !== 'undefined') {
  for (const d of RD_LANDMARKS) _msPlaceByKey[d.key] = d;
}

// 'rd-lm' = hint/cancel mount in the Roads tab (landmark bridge placement)
const _MS_QP_MOUNTS = ['ms-inc', 'ms-coop-inc', 'ms-ext', 'ms-coop-ext', 'rd-lm'];
let _msQpActive = null;  // key in _msPlaceByKey while placing, else null
let _msQpCount  = 0;     // objects placed in this placement session

// Fill every .ms-opt-grid with the option buttons of its data-group
function _msBuildOptGrids() {
  document.querySelectorAll('.ms-opt-grid').forEach(grid => {
    const defs = MS_PLACE_GROUPS[grid.dataset.group] || [];
    grid.innerHTML = defs.map(d =>
      `<button class="opt-btn" data-placekey="${d.key}" onclick="msStartQuickPlace('${d.key}')">${d.label}</button>`
    ).join('');
  });
}

function _msQpSetHint(text) {
  for (const m of _MS_QP_MOUNTS) {
    const hint = document.getElementById(`${m}-hint`);
    if (hint) { hint.textContent = text; hint.style.display = text ? '' : 'none'; }
  }
}

function _msQpRenderButtons() {
  document.querySelectorAll('.opt-btn[data-placekey]').forEach(btn =>
    btn.classList.toggle('primary', btn.dataset.placekey === _msQpActive));
  for (const m of _MS_QP_MOUNTS) {
    const cancel = document.getElementById(`${m}-cancel`);
    if (cancel) cancel.style.display = _msQpActive ? '' : 'none';
  }
}

function msStartQuickPlace(key) {
  _msQpActive = key;
  _msQpCount  = 0;
  _msQpSetHint(`↓ Click on map to place ${_msPlaceByKey[key].label} · right-click to stop ↓`);
  _msQpRenderButtons();
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

function msCancelQuickPlace() {
  _msQpActive = null;
  _msQpCount  = 0;
  _msQpSetHint('');
  _msQpRenderButtons();
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

// Stays active after each placement so multiple objects can be dropped in a row
async function msHandleQuickPlace(screenX, screenY) {
  const cfg = _msPlaceByKey[_msQpActive];
  if (!cfg) return;
  const wc = await screenToWorldAsync(screenX, screenY);
  if (!wc) return;
  const r = await api('/place/object', { template: cfg.template, wx: wc.wx, wy: wc.wy, angle: 0, team: '' });
  if (r?.ok) {
    _msQpCount++;
    _msQpSetHint(`${cfg.label} ×${_msQpCount} placed · click for more · right-click to stop`);
  } else {
    _msQpSetHint(`Failed: ${r?.error || 'unknown error'}`);
  }
}

function _msSwitchTo(tool) {
  document.querySelectorAll('.tool-btn').forEach(b => b.classList.toggle('active', b.dataset.tool === tool));
  switchPanel(tool);
  api('/tool', { tool });
}

function _msUpdateModeUI() {
  document.querySelectorAll('[data-msmode]').forEach(b => b.classList.toggle('active', b.dataset.msmode === _msMode));
  document.getElementById('ms-section-skirmish').style.display = _msMode === 'skirmish' ? '' : 'none';
  document.getElementById('ms-section-coop').style.display     = _msMode === 'coop'     ? '' : 'none';
  document.getElementById('ms-section-mission').style.display  = _msMode === 'mission'  ? '' : 'none';
}

async function _msFetchData() {
  const [infoR, wpR] = await Promise.all([
    apiGet('/map/info'),
    apiGet('/map/waypoints'),
  ]);
  if (infoR?.ok)                              _msMapInfo   = infoR;
  if (wpR?.ok && Array.isArray(wpR.waypoints)) _msWaypoints = wpR.waypoints;
  _msRefreshStatus();
}

function _msRefreshStatus() {
  // Map size
  if (_msMapInfo?.width) {
    const b  = _msMapInfo.borderSize || 0;
    const pw = _msMapInfo.width  - 2 * b;
    const ph = _msMapInfo.height - 2 * b;
    document.getElementById('ms-playable-size').textContent = `${pw} × ${ph}`;
    document.getElementById('ms-border-info').textContent   = `border ${b}`;
  } else {
    document.getElementById('ms-playable-size').textContent = '— × —';
    document.getElementById('ms-border-info').textContent   = 'no map open';
  }

  // Telt zoals het spel zelf (WaypointMap::update() in MapUtil.cpp): sequentieel vanaf
  // Player_1_Start, stopt bij de EERSTE ontbrekende. Een gat in de reeks (bv. Player_2_Start
  // mist) telt dus als 1, niet als "3 van de 4 aanwezig" — anders toont de UI een hoger
  // aantal dan het spel straks daadwerkelijk als speelbare slots herkent.
  const names = new Set(_msWaypoints.map(w => w.name));
  let placedCount = 0;
  for (let i = 1; i <= 8; i++) {
    if (!names.has(`Player_${i}_Start`)) break;
    placedCount++;
  }
  // Een Player_N_Start voorbij het gat (bv. Player_4_Start terwijl Player_2_Start mist)
  // is er wel, maar telt niet mee voor het spel — waarschuw expliciet, anders lijkt de
  // map "compleet" terwijl het spel maar een deel van de slots ziet.
  const hasGap = Array.from({length: 8}, (_, i) => i + 1).some(
    i => i > placedCount && names.has(`Player_${i}_Start`));
  const gapNote = hasGap ? ' — gap in sequence, rest ignored by game!' : '';

  if (_msMode === 'skirmish') {
    const expected = PVP_STARTS[_msPvp];
    const ok       = expected ? placedCount >= expected : placedCount >= 2;
    const label    = (expected
      ? `${placedCount} / ${expected} player starts placed`
      : `${placedCount} player starts placed`) + gapNote;
    _msSetStatus('ms-starts-icon', 'ms-starts-label', ok && !hasGap, label);
  }

  if (_msMode === 'coop') {
    const startOk    = placedCount >= 1;
    const startLabel = `${placedCount} human player start${placedCount !== 1 ? 's' : ''}` + gapNote;
    _msSetStatus('ms-coop-starts-icon', 'ms-coop-starts-label', startOk && !hasGap, startLabel);

    const hasLose  = _msWaypoints.some(w => /lose/i.test(w.name) && w.type === 'polygon');
    _msSetStatus('ms-losezone-icon', 'ms-losezone-label', hasLose,
      hasLose ? 'Lose zone present' : 'No lose zone');
  }
}

function _msSetStatus(iconId, labelId, ok, label) {
  const icon = document.getElementById(iconId);
  const lbl  = document.getElementById(labelId);
  if (icon) { icon.textContent = ok ? '✓' : '⚠'; icon.style.color = ok ? '#5cb85c' : '#e8a838'; }
  if (lbl)  lbl.textContent = label;
}

function startMsPolling() {
  stopMsPolling();
  _msFetchData();
  _msPollTimer = setInterval(_msFetchData, 5000);
}
function stopMsPolling() {
  if (_msPollTimer) { clearInterval(_msPollTimer); _msPollTimer = null; }
}
