// ── Wizard logic ──────────────────────────────────────────────────────────────

// Populate wizard player dropdown from loaded sidelistData
function renderSlWizardPlayerList() { /* player derived per-wizard from team owner */ }

// Helper: resolve owner player for a team name
function wizTeamOwner(teamName) {
  const t = (_sidelistData?.teams || []).find(t => t.name === teamName);
  return t?.owner || (_sidelistData?.players?.[0]?.name) || '';
}

// Helper: build a player <select> and insert it into the form
function wizPlayerRow(id) {
  const players = _sidelistData?.players || [];
  if (!players.length) return `<div class="hint-text" style="color:#e87050">No players — add one in Setup first.</div>`;
  const opts = players.map(p => `<option value="${p.name}">${p.name}</option>`).join('');
  return wizRow('Player', `<select class="wb-form-input" id="${id}" style="flex:1">${opts}</select>`);
}

// Helper: create a labelled form row
function wizRow(label, inputHtml) {
  return `<div class="form-row" style="margin-bottom:4px">
    <span class="form-label" style="min-width:70px">${label}</span>
    ${inputHtml}
  </div>`;
}

// Boss-wizard: open de unit-picker en zet de keuze in het display-element (zoals de team builder).
async function wizBossPickUnit() {
  const disp = document.getElementById('wiz-boss-unit');
  if (!disp) return;
  const palette = await loadObjectPalette();
  openObjPicker(palette, { unitOnly: true, callback: tmpl => {
    disp.dataset.tpl = tmpl;
    disp.textContent = tmpl;
  }});
}

// Helper: dropdown from string array
function wizSelect(id, items) {
  const opts = items.map(i => `<option value="${i}">${i}</option>`).join('');
  return `<select class="wb-form-input" id="${id}" style="flex:1">${opts}</select>`;
}

// Helper: trigger-area-veld met datalist (suggesties van bestaande areas + zelf typen mag).
// Bestaande trigger-areas moeten bestaan voor TEAM_ENTERED_AREA e.d., maar je wilt ook een
// naam kunnen typen die je later aanmaakt — vandaar datalist i.p.v. harde dropdown.
function wizAreaInput(id, defVal) {
  const v = defVal || '';
  return `<input class="wb-form-input" id="${id}" value="${v}" placeholder="${v || 'TriggerArea'}" list="${id}-list" style="flex:1"><datalist id="${id}-list"></datalist>`;
}
async function wizLoadAreas(id) {
  const dl = document.getElementById(`${id}-list`);
  if (!dl) return;
  const r = await apiGet('/map/triggers');
  const areas = (r?.triggers || []).filter(t => t.name && t.name !== 'Default Water');
  dl.innerHTML = areas.map(t => `<option value="${t.name}">`).join('');
}

// Returns a datalist input pre-filled with existing script names (non-subroutine).
// Uses _sidelistData directly — safe to call inside buildFields (data is loaded by then).
function wizScriptInput(id, placeholder) {
  const names = [];
  for (const player of (_sidelistData?.players || [])) {
    for (const group of (player.scriptGroups || [])) {
      for (const script of (group.scripts || [])) {
        if (!script.subroutine) names.push(script.name);
      }
    }
  }
  names.sort();
  const listId = id + '-list';
  return `<input class="wb-form-input" id="${id}" placeholder="${placeholder || 'ScriptName'}" list="${listId}" style="flex:1"><datalist id="${listId}">${names.map(n => `<option value="${n}">`).join('')}</datalist>`;
}

// ── Wizard navigate-to-panel-and-return (waypoint / path / area fields) ────────
let _wizPendingReturn = null;  // { fieldId, ptype, existingItems }

async function _wizGoToMapFor(fieldId, ptype) {
  let existingItems;
  if (ptype === 'WAYPOINT') {
    existingItems = new Set((_wizWpData || []).map(w => w.name));
  } else if (ptype === 'WAYPOINT_PATH') {
    const _areaNames = new Set(_wizAreaData.map(a => a.name));
    const s = new Set();
    (_wizWpData || []).forEach(w => (w.paths || []).forEach(p => { if (p && !_areaNames.has(p)) s.add(p); }));
    existingItems = s;
  } else if (ptype === 'TRIGGER_AREA') {
    const r = await apiGet('/map/triggers');
    existingItems = new Set((r?.triggers || []).map(t => t.name));
  }
  _wizPendingReturn = { fieldId, ptype, existingItems };

  if (ptype === 'TRIGGER_AREA') {
    switchPanel('waypoints'); setWpMode('area'); api('/tool', { tool: 'waypoints' });
  } else if (ptype === 'WAYPOINT_PATH') {
    switchPanel('waypoints'); setWpMode('path'); api('/tool', { tool: 'waypoints' });
  } else {
    switchPanel('waypoints'); setWpMode('point'); api('/tool', { tool: 'waypoints' });
  }
  _wizShowReturnBanner();
}

function _wizShowReturnBanner() {
  document.getElementById('wiz-nav-return-banner')?.remove();
  const bar = document.createElement('div');
  bar.id = 'wiz-nav-return-banner';
  bar.style.cssText = 'display:flex;align-items:center;gap:10px;background:#1a3a5c;border:1px solid #2563eb;border-radius:10px;padding:6px 14px;font-size:12px;color:#90c8ff;margin-left:8px';
  bar.innerHTML = `<span>Add your item on the map, then click:</span>
    <button id="wiz-nav-return-btn" class="action-btn primary" style="padding:4px 14px;flex:unset">&#8617; Back to Wizard</button>`;
  document.getElementById('toolbar').appendChild(bar);
  document.getElementById('wiz-nav-return-btn').addEventListener('click', async () => {
    bar.remove();
    const saved = _wizPendingReturn;
    _wizPendingReturn = null;
    const [r1, r2] = await Promise.all([apiGet('/map/waypoints'), apiGet('/map/triggers')]);
    const newWps = r1?.waypoints || [];
    _wizWpData = newWps;
    let newVal = null;
    if (saved.ptype === 'WAYPOINT') {
      newVal = newWps.find(w => w.name && !saved.existingItems.has(w.name))?.name;
    } else if (saved.ptype === 'WAYPOINT_PATH') {
      const added = [];
      newWps.forEach(w => (w.paths || []).forEach(p => { if (p && !saved.existingItems.has(p)) added.push(p); }));
      newVal = added[0];
    } else if (saved.ptype === 'TRIGGER_AREA') {
      newVal = (r2?.triggers || []).find(t => t.name && !saved.existingItems.has(t.name))?.name;
    }
    switchPanel('sidelist');
    if (newVal) {
      setTimeout(() => {
        const el = document.getElementById(saved.fieldId);
        if (!el) return;
        if (el.tagName === 'SELECT') {
          if (![...el.options].some(o => o.value === newVal))
            el.insertAdjacentHTML('beforeend', `<option value="${newVal}">${newVal}</option>`);
          el.value = newVal;
        } else {
          el.value = newVal;
        }
      }, 80);
    }
  });
}

// ── Generic inline waypoint-placement row ────────────────────────────────────
// Renders: labelled row with select + + button + collapsible placement UI.
// All sub-element IDs are derived from selectId.
function wizWpInlineRow(selectId, label, options, autoPrefix) {
  const wrapId = selectId + '-wrap';
  const uiId   = selectId + '-place-ui';
  const emptyOpt = '<option value="">-- none --</option>';
  const opts   = emptyOpt + (options || []).map(n => `<option value="${n}">${n}</option>`).join('');
  return wizRow(label,
    `<div id="${wrapId}" style="display:flex;gap:4px">
      <select class="wb-form-input" id="${selectId}" style="flex:1">${opts}</select>
      <button class="action-btn" onclick="wizGoPlaceWpInline('${selectId}','${autoPrefix||''}')" title="Place new waypoint on map" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button>
    </div>
    <div id="${uiId}" style="display:none;background:rgba(74,144,226,0.08);border:1px solid rgba(74,144,226,0.25);border-radius:6px;padding:8px;margin-bottom:6px;margin-top:4px">
      <div style="font-size:11px;color:var(--text-dim);margin-bottom:6px">New waypoint</div>
      <div class="form-row" style="margin-bottom:8px">
        <span class="form-label" style="min-width:50px;font-size:11px">Name</span>
        <input class="wb-form-input" id="${uiId}-name" placeholder="${autoPrefix||'Waypoint'}_1" style="flex:1">
      </div>
      <div id="${uiId}-state" style="font-size:11px;color:#4a90e2;text-align:center;padding:4px 0">&#8595; Click on the map to place &#8595;</div>
      <div id="${uiId}-fb" style="font-size:10px;margin-top:4px"></div>
      <button class="action-btn" onclick="wizCancelWpInline('${selectId}')" style="width:100%;margin-top:6px;padding:4px;font-size:11px;flex:none">Cancel</button>
    </div>`
  );
}

function wizGoPlaceWpInline(selectId, autoPrefix) {
  _wizWpLastName = null;
  _wizWpLastCoords = null;
  _wizWpInlineTarget = selectId;

  const uiId   = selectId + '-place-ui';
  const nameEl = document.getElementById(uiId + '-name');
  if (nameEl && !nameEl.value) {
    const sel    = document.getElementById(selectId);
    const count  = sel ? [...sel.options].filter(o => o.value).length : 0;
    const prefix = autoPrefix || nameEl.placeholder.replace(/_\d+$/, '') || 'Waypoint';
    nameEl.value = `${prefix}_${count + 1}`;
  }

  const wrapEl = document.getElementById(selectId + '-wrap');
  if (wrapEl) wrapEl.style.display = 'none';
  const uiEl = document.getElementById(uiId);
  if (uiEl) uiEl.style.display = '';

  const stateEl = document.getElementById(uiId + '-state');
  if (stateEl) { stateEl.textContent = '↓ Click on the map to place ↓'; stateEl.style.color = '#4a90e2'; }
  const fbEl = document.getElementById(uiId + '-fb');
  if (fbEl) fbEl.textContent = '';

  _wizWpQuickPlace = true;
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

function wizCancelWpInline(selectId) {
  _wizWpQuickPlace = false;
  _wizWpInlineTarget = null;
  const uiEl = document.getElementById(selectId + '-place-ui');
  if (uiEl) uiEl.style.display = 'none';
  const wrapEl = document.getElementById(selectId + '-wrap');
  if (wrapEl) wrapEl.style.display = 'flex';
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

// ── Generic inline area (polygon) drawing row ────────────────────────────────
function wizAreaInlineRow(selectId, label, defaultName) {
  const wrapId = selectId + '-wrap';
  const drawId = selectId + '-draw-ui';
  const ph     = defaultName || 'TriggerArea';
  return wizRow(label,
    `<div id="${wrapId}" style="display:flex;gap:4px">
      <select class="wb-form-input" id="${selectId}" style="flex:1"><option value="">— pick area —</option></select>
      <button class="action-btn" onclick="wizGoDrawAreaInline('${selectId}','${ph}')" title="Draw new trigger area on map" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button>
    </div>
    <div id="${drawId}" style="display:none;background:rgba(74,144,226,0.08);border:1px solid rgba(74,144,226,0.25);border-radius:6px;padding:8px;margin-bottom:6px;margin-top:4px">
      <div style="font-size:11px;color:var(--text-dim);margin-bottom:6px">New trigger area</div>
      <div class="form-row" style="margin-bottom:6px">
        <span class="form-label" style="min-width:50px;font-size:11px">Name</span>
        <input class="wb-form-input" id="${drawId}-name" placeholder="${ph}" style="flex:1">
      </div>
      <div id="${drawId}-status" style="font-size:11px;color:#4a90e2;text-align:center;padding:4px 0">&#8595; Click on map to place vertices &middot; right-click to close &#8595;</div>
      <div id="${drawId}-fb" style="font-size:10px;color:#e05c5c;margin-top:4px"></div>
      <button class="action-btn" onclick="wizCancelDrawAreaInline('${selectId}')" style="width:100%;margin-top:6px;padding:4px;font-size:11px;flex:none">Cancel</button>
    </div>`
  );
}

let _wizAreaDrawActive   = false;
let _wizAreaInlineTarget = null;
let _wizAreaNodes        = [];

async function _wizLoadAreaSelect(selectId) {
  const sel = document.getElementById(selectId);
  if (!sel) return;
  const r = await apiGet('/map/triggers');
  const areas = (r?.triggers || []).filter(t => t.name && t.name !== 'Default Water');
  _wizAreaData = areas;
  const prev = sel.value;
  sel.innerHTML = '<option value="">— pick area —</option>' +
    areas.map(t => `<option value="${t.name}">${t.name}</option>`).join('');
  if (prev) sel.value = prev;
}

function wizGoDrawAreaInline(selectId, defaultName) {
  if (_wizAreaDrawActive) wizCancelDrawAreaInline(_wizAreaInlineTarget);
  _wizAreaDrawActive   = true;
  _wizAreaNodes        = [];
  _wizAreaInlineTarget = selectId;

  const drawId = selectId + '-draw-ui';
  const nameEl = document.getElementById(drawId + '-name');
  if (nameEl && !nameEl.value) {
    const sel   = document.getElementById(selectId);
    const count = sel ? [...sel.options].filter(o => o.value).length : 0;
    const ph    = nameEl.placeholder || defaultName || 'TriggerArea';
    nameEl.value = `${ph}${count > 0 ? '_' + (count + 1) : ''}`;
  }

  const wrapEl = document.getElementById(selectId + '-wrap');
  if (wrapEl) wrapEl.style.display = 'none';
  const drawEl = document.getElementById(drawId);
  if (drawEl) drawEl.style.display = '';

  const statusEl = document.getElementById(drawId + '-status');
  if (statusEl) { statusEl.textContent = '↓ Click on map to place vertices · right-click to close ↓'; statusEl.style.color = '#4a90e2'; }
  const fbEl = document.getElementById(drawId + '-fb');
  if (fbEl) fbEl.textContent = '';
  const cancelBtn = drawEl?.querySelector('.action-btn');
  if (cancelBtn) cancelBtn.textContent = 'Cancel';

  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

function wizCancelDrawAreaInline(selectId) {
  _wizAreaDrawActive   = false;
  _wizAreaNodes        = [];
  _wizAreaInlineTarget = null;
  if (!selectId) return;
  const drawEl = document.getElementById(selectId + '-draw-ui');
  if (drawEl) drawEl.style.display = 'none';
  const wrapEl = document.getElementById(selectId + '-wrap');
  if (wrapEl) wrapEl.style.display = 'flex';
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

async function handleWizAreaClick(screenX, screenY) {
  if (!_wizAreaDrawActive || !_wizAreaInlineTarget) return;
  const drawId   = _wizAreaInlineTarget + '-draw-ui';
  const statusEl = document.getElementById(drawId + '-status');

  const wc = await screenToWorldAsync(screenX, screenY);
  if (!wc) return;

  _wizAreaNodes.push({ wx: wc.wx, wy: wc.wy });
  if (statusEl) statusEl.textContent = `${_wizAreaNodes.length} vertex(es) placed · right-click to close`;
}

async function _finishWizAreaDraw() {
  if (!_wizAreaDrawActive || !_wizAreaInlineTarget) return;
  const selectId = _wizAreaInlineTarget;
  const drawId   = selectId + '-draw-ui';
  const nameEl   = document.getElementById(drawId + '-name');
  const statusEl = document.getElementById(drawId + '-status');
  const fbEl     = document.getElementById(drawId + '-fb');

  _wizAreaDrawActive   = false;
  _wizAreaInlineTarget = null;
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);

  const n     = _wizAreaNodes.length;
  const name  = nameEl?.value.trim() || 'TriggerArea';
  const nodes = [..._wizAreaNodes];
  _wizAreaNodes = [];

  if (n < 3) {
    if (statusEl) { statusEl.textContent = 'Need at least 3 vertices — cancelled'; statusEl.style.color = '#e05c5c'; }
    setTimeout(() => wizCancelDrawAreaInline(selectId), 1200);
    return;
  }

  const r = await api('/trigger', { name, points: nodes.map(v => ({ x: v.wx, y: v.wy })) });
  if (!r?.ok) {
    if (fbEl) fbEl.textContent = r?.error || 'Failed to create area';
    setTimeout(() => wizCancelDrawAreaInline(selectId), 1500);
    return;
  }

  // Refresh all area selects in the current wizard
  const r2    = await apiGet('/map/triggers');
  const areas = (r2?.triggers || []).filter(t => t.name && t.name !== 'Default Water');
  const areaOpts = '<option value="">— pick area —</option>' +
    areas.map(t => `<option value="${t.name}">${t.name}</option>`).join('');
  ['wiz-lose-area','wiz-sp-area','wiz-boss-lose','wiz-area-name'].forEach(sid => {
    const s = document.getElementById(sid);
    if (s) { const prev = s.value; s.innerHTML = areaOpts; s.value = prev || (s.id === selectId ? name : prev); }
  });
  const ownSel = document.getElementById(selectId);
  if (ownSel) ownSel.value = name;

  if (statusEl) { statusEl.textContent = `✓ "${name}" created (${n} vertices)`; statusEl.style.color = '#5cb85c'; }
  const cancelBtn = document.getElementById(drawId)?.querySelector('.action-btn');
  if (cancelBtn) cancelBtn.textContent = 'Done';
  setTimeout(() => wizCancelDrawAreaInline(selectId), 1500);
}

// ── Generic inline path-drawing row ──────────────────────────────────────────
function wizPathInlineRow(selectId, label, pathOptions) {
  const wrapId = selectId + '-wrap';
  const drawId = selectId + '-draw-ui';
  const emptyOpt = '<option value="">— pick path —</option>';
  const opts = emptyOpt + (pathOptions || []).map(p => `<option value="${p}">${p}</option>`).join('');
  return wizRow(label,
    `<div id="${wrapId}" style="display:flex;gap:4px">
      <select class="wb-form-input" id="${selectId}" style="flex:1">${opts}</select>
      <button class="action-btn" onclick="wizGoDrawPathInline('${selectId}')" title="Draw new path on map" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button>
    </div>
    <div id="${drawId}" style="display:none;background:rgba(74,144,226,0.08);border:1px solid rgba(74,144,226,0.25);border-radius:6px;padding:8px;margin-bottom:6px;margin-top:4px">
      <div style="font-size:11px;color:var(--text-dim);margin-bottom:6px">New path</div>
      <div class="form-row" style="margin-bottom:6px">
        <span class="form-label" style="min-width:50px;font-size:11px">Name</span>
        <input class="wb-form-input" id="${drawId}-name" placeholder="Path_1" style="flex:1">
      </div>
      <div id="${drawId}-status" style="font-size:11px;color:#4a90e2;text-align:center;padding:4px 0">&#8595; Click on map to place nodes &middot; right-click to finish &#8595;</div>
      <div id="${drawId}-fb" style="font-size:10px;color:#e05c5c;margin-top:4px"></div>
      <button class="action-btn" onclick="wizCancelDrawPathInline('${selectId}')" style="width:100%;margin-top:6px;padding:4px;font-size:11px;flex:none">Cancel</button>
    </div>`
  );
}

function wizGoDrawPathInline(selectId) {
  if (_wizPathDrawActive) {
    if (_wizSpPathDraw) cancelWizSpawnPathDraw();
    else if (_wizPathInlineTarget) wizCancelDrawPathInline(_wizPathInlineTarget);
    else cancelWizPathDraw();
  }
  _wizPathDrawActive   = true;
  _wizPathNodes        = [];
  _wizPathDrawRow      = null;
  _wizSpPathDraw       = false;
  _wizPathInlineTarget = selectId;

  const existingPaths = new Set();
  _wizWpData.forEach(w => (w.paths || []).forEach(p => existingPaths.add(p)));
  let n = 1;
  while (existingPaths.has(`Path_${n}`)) n++;
  _wizPathCurName = `Path_${n}`;

  const drawId = selectId + '-draw-ui';
  const wrapEl = document.getElementById(selectId + '-wrap');
  if (wrapEl) wrapEl.style.display = 'none';
  const drawEl = document.getElementById(drawId);
  if (drawEl) drawEl.style.display = '';

  const nameEl = document.getElementById(drawId + '-name');
  if (nameEl) nameEl.value = _wizPathCurName;
  const statusEl = document.getElementById(drawId + '-status');
  if (statusEl) { statusEl.textContent = '↓ Click on map to place nodes · right-click to finish ↓'; statusEl.style.color = '#4a90e2'; }
  const fbEl = document.getElementById(drawId + '-fb');
  if (fbEl) fbEl.textContent = '';
  const cancelBtn = drawEl?.querySelector('.action-btn');
  if (cancelBtn) cancelBtn.textContent = 'Cancel';

  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

function wizCancelDrawPathInline(selectId) {
  _wizPathDrawActive   = false;
  _wizPathNodes        = [];
  _wizPathInlineTarget = null;
  const drawEl = document.getElementById(selectId + '-draw-ui');
  if (drawEl) drawEl.style.display = 'none';
  const wrapEl = document.getElementById(selectId + '-wrap');
  if (wrapEl) wrapEl.style.display = 'flex';
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

// System teams generated by WB for each player — not user-created, filter from wizard
const _SYSTEM_TEAM_RE = /^(team$|teamPlyr|teamSkirmish)/;

// Helper: team dropdown showing only user-created teams + "+" button
function wizTeamSelect(id) {
  const allTeams = _sidelistData?.teams || [];
  const teams = allTeams.filter(t => !_SYSTEM_TEAM_RE.test(t.name));
  const plusBtn = `<button class="action-btn" style="padding:2px 8px;font-size:14px;line-height:1;flex:none" onclick="openNewTeamFromWizard()" title="New team">+</button>`;
  if (!teams.length) {
    return `<div style="display:flex;align-items:center;gap:4px">
      <span class="hint-text" style="color:#e87050;flex:1">No teams yet.</span>
      ${plusBtn}
    </div>`;
  }
  const opts = teams.map(t => {
    const unitSummary = (t.units||[]).map(u => `${u.count}x ${u.template}`).join(', ');
    const label = unitSummary ? `${t.name}  (${unitSummary})` : t.name;
    return `<option value="${t.name}">${label}</option>`;
  }).join('');
  return `<div style="display:flex;gap:4px;align-items:center">
    <select class="wb-form-input" id="${id}" style="flex:1" onchange="wizSpawnTeamChange()">${opts}</select>
    ${plusBtn}
  </div>`;
}

// ── Wizard inline waypoint placement ─────────────────────────────────────────
let _wizWpQuickPlace   = false;
let _wizWpLastCoords   = null;  // {wx, wy} of last placed quick waypoint
let _wizWpLastName     = null;
let _wizWpData         = [];    // cached full waypoint list for path-first-wp lookup
let _wizAreaData       = [];    // cached trigger areas (PolygonTrigger) — used to filter path dropdowns
let _wizWpInlineTarget = null;  // selectId currently in inline-place mode

// ── Wave Sequence wizard helpers ──────────────────────────────────────────────
let _wizTemplate         = null;  // active wizard key ('spawn', 'waves', etc.)
let _wizPathDrawActive   = false;
let _wizPathNodes        = [];   // [{name, wx, wy}] nodes placed so far
let _wizPathDrawRow      = null; // .wiz-ws-wave element that owns the active draw
let _wizPathCurName      = '';   // path label being drawn
let _wizSpPathDraw       = false; // true = drawing for Spawn Units wizard (not Wave Sequence)
let _wizPathInlineTarget = null;  // selectId currently in inline-draw mode
let _waveSeqState        = null; // persisted wave sequence config for re-edit

// ── Wizard instance list ──────────────────────────────────────────────────────
let _wizInstances      = [];   // [{ id, key, state, scriptRefs }]
let _editingInstanceId = null; // id of instance being re-edited
let _instanceCounter   = 0;

const _WIZ_INST_COLORS = {
  waves:   { bg: 'rgba(42,96,64,0.35)',  border: '#2a6040', text: '#7de0a8' },
  boss:    { bg: 'rgba(96,48,48,0.35)',  border: '#603030', text: '#e0a87d' },
  spawn:   { bg: 'rgba(26,48,85,0.35)',  border: '#1a3055', text: '#4a90e2' },
  win:     { bg: 'rgba(24,52,24,0.35)',  border: '#1e401e', text: '#6abf6a' },
  lose:    { bg: 'rgba(80,28,28,0.35)',  border: '#501c1c', text: '#e06a6a' },
  money:   { bg: 'rgba(52,44,12,0.35)',  border: '#3a3010', text: '#d4bf60' },
  message: { bg: 'rgba(18,40,60,0.35)',  border: '#182840', text: '#60b0e0' },
  camera:  { bg: 'rgba(38,24,60,0.35)', border: '#26183c', text: '#a07ae0' },
  timer:   { bg: 'rgba(48,30,60,0.35)', border: '#301e3c', text: '#c08ae0' },
};

function _wizMakeSummary(key, state) {
  if (!state) return '';
  switch (key) {
    case 'waves':   return `${state.waves?.length || 0} wave(s)  ·  ${state.group || ''}`;
    case 'spawn':   return `${state['wiz-sp-team']||'?'}  →  ${(state['wiz-sp-goal']||'?').replace(/_/g,' ')}`;
    case 'boss':    return `${state['wiz-boss-name']||'Boss'}: ${state['wiz-boss-unit']||'?'}`;
    case 'win':     return (state['wiz-win-cond'] === 'player')
      ? `Player wiped: ${state['wiz-win-side']||'?'}`
      : `Team destroyed: ${state['wiz-win-team']||'?'}`;
    case 'lose':    return (state['wiz-lose-cond'] === 'wiped')
      ? `Player wiped: ${state['wiz-lose-side']||'?'}`
      : `${state['wiz-lose-enemy']||'SkirmishGLA'} in ${state['wiz-lose-area']||'?'}`;
    case 'money':   return `$${state['wiz-money-amount']||0}  →  ${state['wiz-money-player']||'?'}`;
    case 'message': return state['wiz-msg-key']||'?';
    case 'camera':  return `Path: ${state['wiz-cam-path']||'?'}`;
    case 'timer':   return `${state['wiz-t-name']||'?'}  (${state['wiz-t-min']||0} min)`;
    default: return '';
  }
}

function _wizSnapState() {
  const state = {};
  const fields = document.getElementById('sl-wiz-fields');
  if (!fields) return state;
  fields.querySelectorAll('[id]').forEach(el => {
    if (el.tagName === 'INPUT' || el.tagName === 'SELECT') state[el.id] = el.value;
    else if (el.tagName === 'SPAN' && 'tpl' in el.dataset)  state[el.id] = el.dataset.tpl || '';
  });
  return state;
}

function _wizRestoreState(key, state) {
  if (!state) return;
  for (const [id, value] of Object.entries(state)) {
    const el = document.getElementById(id);
    if (!el) continue;
    if (el.tagName === 'SPAN' && 'tpl' in el.dataset) {
      el.dataset.tpl  = value;
      el.textContent  = value || '— pick unit —';
      if (value) el.style.color = '';
    } else if (el.tagName === 'INPUT' || el.tagName === 'SELECT') {
      el.value = value;
    }
  }
  // Trigger visibility-logic for conditional rows
  if (key === 'spawn') { wizSpawnGoalChange(); wizSpawnWhenChange(); wizSpawnTeamChange(); }
  if (key === 'lose')    wizLoseCondChange();
  if (key === 'win')     wizWinCondChange();
  if (key === 'message') wizMsgWhenChange();
}

function _wizGetTiming(key, state) {
  switch (key) {
    case 'spawn': {
      const when = state['wiz-sp-when'] || 'start';
      const secs = parseFloat(state['wiz-sp-secs']) || 0;
      if (when === 'after')     return { label: `+${secs}s`,  sortKey: secs };
      if (when === 'repeat')    return { label: `↻ ${secs}s`, sortKey: secs };
      if (when === 'destroyed') return { label: `☠ ${state['wiz-sp-dest-team']||'?'}`, sortKey: Infinity };
      return { label: '▶ start', sortKey: 0 };
    }
    case 'boss': {
      const trig = state['wiz-boss-trigger'] || 'team_dead';
      if (trig === 'start') return { label: '▶ start', sortKey: 0 };
      return { label: `☠ ${state['wiz-boss-trigteam']||'?'}`, sortKey: Infinity };
    }
    case 'win': {
      const cond = state['wiz-win-cond'] || 'team';
      const who  = cond === 'player' ? state['wiz-win-side'] : state['wiz-win-team'];
      return { label: `☠ ${who||'?'}`, sortKey: Infinity };
    }
    case 'lose': {
      const cond = state['wiz-lose-cond'] || 'area';
      if (cond === 'wiped') return { label: `☠ ${state['wiz-lose-side']||'?'}`, sortKey: Infinity };
      return { label: `⚠ ${state['wiz-lose-area']||'?'}`, sortKey: Infinity };
    }
    case 'message': {
      const delay = parseFloat(state['wiz-msg-delay']) || 0;
      if (state['wiz-msg-when'] === 'delay') return { label: `+${delay}s`, sortKey: delay };
      return { label: '▶ start', sortKey: 0 };
    }
    case 'timer': {
      const secs = (parseFloat(state['wiz-t-min']) || 0) * 60;
      return { label: `+${state['wiz-t-min']||0}min`, sortKey: secs };
    }
    case 'waves':
    case 'camera':
    case 'money':
    default:
      return { label: '▶ start', sortKey: 0 };
  }
}

function renderWizInstances() {
  const container = document.getElementById('sl-wiz-instances');
  if (!container) return;
  if (!_wizInstances.length) { container.innerHTML = ''; return; }

  const sorted = [..._wizInstances].sort((a, b) => {
    const ta = _wizGetTiming(a.key, a.state || {}).sortKey;
    const tb = _wizGetTiming(b.key, b.state || {}).sortKey;
    return ta - tb;
  });

  const rows = sorted.map(inst => {
    const c       = _WIZ_INST_COLORS[inst.key] || _WIZ_INST_COLORS.spawn;
    const title   = WIZ_TEMPLATES[inst.key]?.title || inst.key;
    const summary = _wizMakeSummary(inst.key, inst.state);
    const timing  = _wizGetTiming(inst.key, inst.state || {});
    const active  = _editingInstanceId === inst.id;
    return `<div style="background:${c.bg};border:1px solid ${active ? c.text : c.border};border-radius:6px;padding:6px 8px;margin-bottom:4px">
      <div style="display:flex;align-items:center;gap:4px">
        <span style="font-size:11px;color:${c.text};font-weight:600;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">${title}</span>
        <span style="font-size:10px;color:var(--text-dim);flex:none;white-space:nowrap">${timing.label}</span>
        <button class="action-btn" onclick="editWizInstance('${inst.id}')" style="padding:1px 8px;font-size:11px;flex:none">${active ? 'Editing…' : 'Edit'}</button>
        <button class="action-btn" onclick="deleteWizInstance('${inst.id}')" style="padding:1px 6px;font-size:11px;flex:none;color:#e05c5c" title="Delete wizard + scripts">✕</button>
      </div>
      ${summary ? `<div style="font-size:10px;color:var(--text-dim);margin-top:3px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap" title="${summary}">${summary}</div>` : ''}
    </div>`;
  }).join('');

  container.innerHTML = `<div class="section-label" style="margin-top:2px;margin-bottom:4px">MY WIZARDS</div>${rows}`;
}

async function editWizInstance(id) {
  const inst = _wizInstances.find(i => i.id === id);
  if (!inst) return;
  _editingInstanceId = id;

  // Waves wizard reads _waveSeqState inside buildFields — set it before opening
  if (inst.key === 'waves' && inst.state) _waveSeqState = JSON.parse(JSON.stringify(inst.state));

  openWizForm(inst.key);

  if (inst.key !== 'waves') {
    // Restore static fields immediately (teams, goal, stance, when, etc.)
    _wizRestoreState(inst.key, inst.state);
    // Restore dynamic selects (waypoints/paths) after their async apiGet has resolved
    setTimeout(() => _wizRestoreState(inst.key, inst.state), 500);
  }

  renderWizInstances();  // show "Editing…" label
}

async function deleteWizInstance(id) {
  const inst = _wizInstances.find(i => i.id === id);
  if (!inst) return;
  for (const ref of (inst.scriptRefs || [])) {
    await api('/sidelist/script/delete', { player: ref.player, group: ref.group, name: ref.name });
  }
  _wizInstances = _wizInstances.filter(i => i.id !== id);
  if (_editingInstanceId === id) {
    _editingInstanceId = null;
    document.getElementById('sl-wiz-form').style.display = 'none';
  }
  renderWizInstances();
  fetchSideList();
}

function wizGoDrawPath(plusBtn) {
  const row = plusBtn.closest('.wiz-ws-wave');
  if (!row) return;
  if (_wizPathDrawActive) {
    if (_wizSpPathDraw) cancelWizSpawnPathDraw();
    else cancelWizPathDraw();
  }

  _wizPathDrawActive = true;
  _wizPathNodes      = [];
  _wizPathDrawRow    = row;

  // Auto-name from existing path count
  const existingPaths = new Set();
  _wizWpData.forEach(w => (w.paths || []).forEach(p => existingPaths.add(p)));
  let n = 1;
  while (existingPaths.has(`Path_${n}`)) n++;
  _wizPathCurName = `Path_${n}`;

  row.querySelector('.wiz-ws-path-wrap').style.display = 'none';
  const ui = row.querySelector('.wiz-ws-path-draw-ui');
  ui.style.display = '';
  ui.querySelector('.wiz-ws-path-name').value = _wizPathCurName;
  ui.querySelector('.wiz-ws-path-status').textContent = '↓ Click on map to place nodes · right-click to finish ↓';
  ui.querySelector('.wiz-ws-path-status').style.color = '#4a90e2';
  ui.querySelector('.wiz-ws-path-fb').textContent = '';
  ui.querySelectorAll('.action-btn.primary').forEach(b => b.remove());
  const cancelBtn = ui.querySelector('.wiz-ws-path-cancel');
  if (cancelBtn) cancelBtn.textContent = 'Cancel';

  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

async function handleWizPathClick(screenX, screenY) {
  if (!_wizPathDrawActive) return;
  if (!_wizSpPathDraw && !_wizPathDrawRow && !_wizPathInlineTarget) return;

  let nameEl, statusEl, fbEl;
  if (_wizPathInlineTarget) {
    const drawId = _wizPathInlineTarget + '-draw-ui';
    nameEl   = document.getElementById(drawId + '-name');
    statusEl = document.getElementById(drawId + '-status');
    fbEl     = document.getElementById(drawId + '-fb');
  } else if (_wizSpPathDraw) {
    const ui = document.getElementById('wiz-sp-path-draw-ui');
    nameEl   = ui?.querySelector('.wiz-sp-path-name');
    statusEl = ui?.querySelector('.wiz-sp-path-status');
    fbEl     = ui?.querySelector('.wiz-sp-path-fb');
  } else {
    const ui = _wizPathDrawRow.querySelector('.wiz-ws-path-draw-ui');
    nameEl   = ui?.querySelector('.wiz-ws-path-name');
    statusEl = ui?.querySelector('.wiz-ws-path-status');
    fbEl     = ui?.querySelector('.wiz-ws-path-fb');
  }

  const pathName = nameEl?.value.trim() || _wizPathCurName;
  _wizPathCurName = pathName;

  const wc = await screenToWorldAsync(screenX, screenY);
  if (!wc) return;

  const idx      = _wizPathNodes.length;
  const nodeName = `${pathName}_${idx}`;
  const r = await api('/place/waypoint', { name: nodeName, wx: wc.wx, wy: wc.wy, pathLabel: pathName });
  if (!r?.ok) {
    if (fbEl) fbEl.textContent = r?.error || 'Failed to place node';
    return;
  }
  if (idx > 0) await api('/link/waypoints', { name1: _wizPathNodes[idx - 1].name, name2: nodeName });
  _wizPathNodes.push({ name: nodeName, wx: wc.wx, wy: wc.wy });
  syncWaypointsState();

  if (statusEl) statusEl.textContent = `${_wizPathNodes.length} node(s) placed · right-click to finish`;
}

async function _finishWizPathDraw() {
  if (!_wizPathDrawActive) return;
  _wizPathDrawActive = false;
  const wasSpawnMode = _wizSpPathDraw;
  const wasInlineTgt = _wizPathInlineTarget;
  _wizSpPathDraw       = false;
  _wizPathInlineTarget = null;
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);

  const row      = _wizPathDrawRow;
  const n        = _wizPathNodes.length;
  const pathName = _wizPathCurName;
  _wizPathNodes  = [];
  _wizPathDrawRow = null;

  if (wasInlineTgt) {
    const drawId   = wasInlineTgt + '-draw-ui';
    const statusEl = document.getElementById(drawId + '-status');
    if (n < 2) {
      if (statusEl) { statusEl.textContent = 'Need at least 2 nodes — cancelled'; statusEl.style.color = '#e05c5c'; }
      setTimeout(() => wizCancelDrawPathInline(wasInlineTgt), 1200);
      return;
    }
    const r2 = await apiGet('/map/waypoints');
    if (r2?.waypoints) {
      _wizWpData = r2.waypoints;
      const sel = document.getElementById(wasInlineTgt);
      if (sel) {
        sel.innerHTML = _wizWsPathOpts();
        sel.value = pathName;
      }
      document.querySelectorAll('#wiz-ws-wave-list .wiz-ws-path').forEach(s => {
        const prev = s.value; s.innerHTML = _wizWsPathOpts(); if (prev) s.value = prev;
      });
    }
    if (statusEl) { statusEl.textContent = `✓ ${n} nodes placed`; statusEl.style.color = '#5cb85c'; }
    const cancelBtn = document.getElementById(drawId)?.querySelector('.action-btn');
    if (cancelBtn) cancelBtn.textContent = 'Done';
    return;
  }

  if (wasSpawnMode) {
    const ui = document.getElementById('wiz-sp-path-draw-ui');
    const statusEl = ui?.querySelector('.wiz-sp-path-status');
    if (n < 2) {
      if (statusEl) { statusEl.textContent = 'Need at least 2 nodes — cancelled'; statusEl.style.color = '#e05c5c'; }
      setTimeout(() => {
        if (ui) ui.style.display = 'none';
        const pr = document.getElementById('wiz-sp-path-row');
        if (pr) pr.style.display = '';
      }, 1200);
      return;
    }
    const r2 = await apiGet('/map/waypoints');
    if (r2?.waypoints) {
      _wizWpData = r2.waypoints;
      wizRefreshWpDropdowns('wiz-sp-path', r2.waypoints);
    }
    const pathSel = document.getElementById('wiz-sp-path');
    if (pathSel) pathSel.value = pathName;
    if (statusEl) { statusEl.textContent = `✓ ${n} nodes placed`; statusEl.style.color = '#5cb85c'; }
    const cancelBtn = ui?.querySelector('.wiz-sp-path-cancel');
    if (cancelBtn) cancelBtn.textContent = 'Done ✓';
    return;
  }

  if (!row) return;
  const ui = row.querySelector('.wiz-ws-path-draw-ui');

  if (n < 2) {
    const statusEl = ui?.querySelector('.wiz-ws-path-status');
    if (statusEl) { statusEl.textContent = 'Need at least 2 nodes — cancelled'; statusEl.style.color = '#e05c5c'; }
    setTimeout(() => _cancelWizPathUi(row, ui), 1200);
    return;
  }

  // Refresh path dropdowns everywhere
  const r2 = await apiGet('/map/waypoints');
  if (r2?.waypoints) {
    _wizWpData = r2.waypoints;
    document.querySelectorAll('#wiz-ws-wave-list .wiz-ws-path').forEach(sel => {
      const prev = sel.value;
      sel.innerHTML = _wizWsPathOpts();
      if (prev) sel.value = prev;
    });
  }
  // Auto-select new path in this row's dropdown (for after Done)
  row.querySelector('.wiz-ws-path').innerHTML = _wizWsPathOpts();
  row.querySelector('.wiz-ws-path').value = pathName;

  const statusEl = ui?.querySelector('.wiz-ws-path-status');
  if (statusEl) { statusEl.textContent = `✓ ${n} nodes placed`; statusEl.style.color = '#5cb85c'; }
  const cancelBtn = ui?.querySelector('.wiz-ws-path-cancel');
  if (cancelBtn) cancelBtn.textContent = 'Done';
}

function _cancelWizPathUi(row, ui) {
  if (!row) return;
  if (ui) ui.style.display = 'none';
  const wrap = row.querySelector('.wiz-ws-path-wrap');
  if (wrap) wrap.style.display = '';
}

function cancelWizPathDraw(btn) {
  _wizPathDrawActive = false;
  _wizPathNodes = [];
  if (_wizPathInlineTarget) {
    const tgt = _wizPathInlineTarget;
    _wizPathInlineTarget = null;
    wizCancelDrawPathInline(tgt);
    return;
  }
  if (_wizSpPathDraw) {
    _wizSpPathDraw = false;
    const ui = document.getElementById('wiz-sp-path-draw-ui');
    if (ui) ui.style.display = 'none';
    const row2 = document.getElementById('wiz-sp-path-row');
    if (row2) row2.style.display = '';
  }
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
  const row = btn ? btn.closest('.wiz-ws-wave') : _wizPathDrawRow;
  _wizPathDrawRow = null;
  if (row) _cancelWizPathUi(row, row.querySelector('.wiz-ws-path-draw-ui'));
}

function cancelWizSpawnPathDraw() {
  _wizPathDrawActive = false;
  _wizSpPathDraw = false;
  _wizPathNodes = [];
  _wizPathDrawRow = null;
  const ui = document.getElementById('wiz-sp-path-draw-ui');
  if (ui) ui.style.display = 'none';
  const pr = document.getElementById('wiz-sp-path-row');
  if (pr) pr.style.display = '';
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

function wizGoDrawSpawnPath() {
  if (_wizPathDrawActive) {
    if (_wizSpPathDraw) cancelWizSpawnPathDraw();
    else cancelWizPathDraw();
  }
  _wizPathDrawActive = true;
  _wizSpPathDraw = true;
  _wizPathNodes = [];
  _wizPathDrawRow = null;

  const existingPaths = new Set();
  _wizWpData.forEach(w => (w.paths || []).forEach(p => existingPaths.add(p)));
  let n = 1;
  while (existingPaths.has(`Path_${n}`)) n++;
  _wizPathCurName = `Path_${n}`;

  document.getElementById('wiz-sp-path-row').style.display = 'none';
  const ui = document.getElementById('wiz-sp-path-draw-ui');
  if (ui) {
    ui.style.display = '';
    ui.querySelector('.wiz-sp-path-name').value = _wizPathCurName;
    ui.querySelector('.wiz-sp-path-status').textContent = '↓ Click on map to place nodes · right-click to finish ↓';
    ui.querySelector('.wiz-sp-path-status').style.color = '#4a90e2';
    ui.querySelector('.wiz-sp-path-fb').textContent = '';
    const cancelBtn = ui.querySelector('.wiz-sp-path-cancel');
    if (cancelBtn) cancelBtn.textContent = 'Cancel';
  }
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

function _wizWsTeamOpts() {
  const teams = (_sidelistData?.teams || []).filter(t => !_SYSTEM_TEAM_RE.test(t.name));
  return '<option value="">— pick team —</option>' + teams.map(t => `<option value="${t.name}">${t.name}</option>`).join('');
}
function _wizWsPathOpts() {
  const areaNames = new Set(_wizAreaData.map(a => a.name));
  const pathSet = new Set();
  _wizWpData.forEach(w => (w.paths || []).forEach(p => { if (p && !areaNames.has(p)) pathSet.add(p); }));
  return '<option value="">— pick path —</option>' + [...pathSet].map(p => `<option value="${p}">${p}</option>`).join('');
}
function _wizWsWaveHtml(idx, teamOpts, pathOpts) {
  const afterLabel = idx === 0 ? 'game start' : 'prev wave spawns';
  const defCap = `Wave ${idx + 1} incoming!`;
  return `<div class="wiz-ws-wave" data-collapsed="false" style="background:rgba(255,255,255,0.03);border:1px solid #1a3055;border-radius:6px;margin-bottom:6px">
    <div class="wiz-ws-header" onclick="wizWsToggleWave(this)" style="display:flex;align-items:center;gap:6px;padding:7px 8px;cursor:pointer;user-select:none">
      <span class="wiz-ws-label" style="font-size:11px;color:#4a90e2;font-weight:600;letter-spacing:.05em;flex:none">WAVE ${idx + 1}</span>
      <span class="wiz-ws-summary" style="font-size:11px;color:#8ab4cc;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap"></span>
      <button class="action-btn" onclick="event.stopPropagation();wizWsRemoveWave(this)" style="padding:1px 7px;font-size:13px;line-height:1;flex:none">×</button>
    </div>
    <div class="wiz-ws-body" style="padding:0 8px 8px">
      ${wizRow('Team', `<select class="wb-form-input wiz-ws-team" style="flex:1">${teamOpts}</select><button class="action-btn" onclick="openNewTeamFromWizard()" title="New team" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button>`)}
      <div class="wiz-ws-path-wrap">
        ${wizRow('Path', `<select class="wb-form-input wiz-ws-path" style="flex:1">${pathOpts}</select><button class="action-btn" onclick="wizGoDrawPath(this)" title="Draw new path on map" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button>`)}
      </div>
      <div class="wiz-ws-path-draw-ui" style="display:none;background:rgba(74,144,226,0.08);border:1px solid rgba(74,144,226,0.25);border-radius:6px;padding:8px;margin-bottom:6px">
        <div style="font-size:11px;color:var(--text-dim);margin-bottom:6px">New path</div>
        <div class="form-row" style="margin-bottom:6px">
          <span class="form-label" style="min-width:50px;font-size:11px">Name</span>
          <input class="wb-form-input wiz-ws-path-name" placeholder="Path_1" style="flex:1">
        </div>
        <div class="wiz-ws-path-status" style="font-size:11px;color:#4a90e2;text-align:center;padding:4px 0">&#8595; Click on map to place nodes &middot; right-click to finish &#8595;</div>
        <div class="wiz-ws-path-fb" style="font-size:10px;color:#e05c5c;margin-top:4px"></div>
        <button class="action-btn wiz-ws-path-cancel" onclick="cancelWizPathDraw(this)" style="width:100%;margin-top:6px;padding:4px;font-size:11px;flex:none">Cancel</button>
      </div>
      <div class="wiz-ws-delay-row">
        ${wizRow('Delay', `<input class="wb-form-input wb-num wiz-ws-delay" type="number" value="${idx === 0 ? 60 : 0}" min="0" step="5" style="width:72px;min-width:72px;flex:none"> <span class="wiz-ws-delay-hint" style="font-size:11px;color:var(--text-dim);margin-left:4px">${idx === 0 ? 'sec after game start' : 'sec after prev destroyed (0 = immediate)'}</span>`)}
      </div>
      ${wizRow('Movement', `<select class="wb-form-input wiz-ws-movement" style="flex:1">
          <option value="follow">Follow path (free attack)</option>
          <option value="exact">Follow path (exact / AOD)</option>
        </select>`)}
      ${wizRow('Stance', `<select class="wb-form-input wiz-ws-stance" style="flex:1">
          <option value="2">Aggressive — attack on sight</option>
          <option value="1">Alert</option>
          <option value="0">Normal</option>
          <option value="-1">Passive</option>
        </select>`)}
      ${wizRow('Caption', `<input class="wb-form-input wiz-ws-caption" placeholder="${defCap}" style="flex:1">`)}
      <button class="action-btn" onclick="wizWsSaveWave(this)" style="width:100%;margin-top:4px;padding:4px 8px;font-size:11px;flex:none">Save &#8963;</button>
    </div>
  </div>`;
}
// Boss-golf-kaart binnen de sequencer — hergebruikt de pad-teken-UI van de gewone kaart.
function _wizWsBossHtml(teamOpts, pathOpts) {
  return `<div class="wiz-ws-wave wiz-ws-boss" data-collapsed="false" style="background:rgba(96,48,48,0.12);border:1px solid #603030;border-radius:6px;margin-bottom:6px">
    <div class="wiz-ws-header" onclick="wizWsToggleWave(this)" style="display:flex;align-items:center;gap:6px;padding:7px 8px;cursor:pointer;user-select:none">
      <span class="wiz-ws-label" style="font-size:11px;color:#e0a87d;font-weight:600;letter-spacing:.05em;flex:none">BOSS</span>
      <span class="wiz-ws-summary" style="font-size:11px;color:#cc9a8a;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap"></span>
      <button class="action-btn" onclick="event.stopPropagation();wizWsRemoveWave(this)" style="padding:1px 7px;font-size:13px;line-height:1;flex:none">×</button>
    </div>
    <div class="wiz-ws-body" style="padding:0 8px 8px">
      ${wizRow('Unit', `<span class="wb-form-input wiz-ws-boss-unit" data-tpl="GLATankMarauder" style="flex:1;cursor:pointer;overflow:hidden;text-overflow:ellipsis;white-space:nowrap" onclick="wizWsPickBossUnit(this)">GLATankMarauder</span><button class="action-btn" onclick="wizWsPickBossUnit(this)" title="Pick unit" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button>`)}
      ${wizRow('Count', `<input class="wb-form-input wb-num wiz-ws-boss-count" type="number" value="1" min="1" max="6" style="width:72px;min-width:72px;flex:none">`)}
      ${wizRow('Veterancy', `<select class="wb-form-input wiz-ws-boss-vet" style="flex:1"><option value="3">Heroic</option><option value="2">Elite</option><option value="1">Veteran</option><option value="0">Regular</option></select>`)}
      ${wizRow('Max HP', `<input class="wb-form-input wb-num wiz-ws-boss-maxhp" type="number" value="8000" min="0" step="500" style="width:90px;min-width:90px;flex:none">`)}
      ${wizRow('Stance', `<select class="wb-form-input wiz-ws-stance" style="flex:1"><option value="2">Aggressive — attack on sight</option><option value="1">Alert</option><option value="0">Normal</option><option value="-1">Passive</option></select>`)}
      <div class="wiz-ws-path-wrap">
        ${wizRow('Path', `<select class="wb-form-input wiz-ws-path" style="flex:1">${pathOpts}</select><button class="action-btn" onclick="wizGoDrawPath(this)" title="Draw new path on map" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button>`)}
      </div>
      <div class="wiz-ws-path-draw-ui" style="display:none;background:rgba(74,144,226,0.08);border:1px solid rgba(74,144,226,0.25);border-radius:6px;padding:8px;margin-bottom:6px">
        <div style="font-size:11px;color:var(--text-dim);margin-bottom:6px">New path</div>
        <div class="form-row" style="margin-bottom:6px">
          <span class="form-label" style="min-width:50px;font-size:11px">Name</span>
          <input class="wb-form-input wiz-ws-path-name" placeholder="Path_1" style="flex:1">
        </div>
        <div class="wiz-ws-path-status" style="font-size:11px;color:#4a90e2;text-align:center;padding:4px 0">&#8595; Click on map to place nodes &middot; right-click to finish &#8595;</div>
        <div class="wiz-ws-path-fb" style="font-size:10px;color:#e05c5c;margin-top:4px"></div>
        <button class="action-btn wiz-ws-path-cancel" onclick="cancelWizPathDraw(this)" style="width:100%;margin-top:6px;padding:4px;font-size:11px;flex:none">Cancel</button>
      </div>
      ${wizRow('Delay', `<input class="wb-form-input wb-num wiz-ws-delay" type="number" value="0" min="0" step="5" style="width:72px;min-width:72px;flex:none"> <span class="wiz-ws-delay-hint" style="font-size:11px;color:var(--text-dim);margin-left:4px">sec from game start</span>`)}
      ${wizRow('Caption', `<input class="wb-form-input wiz-ws-caption" placeholder="BOSS WAVE!" style="flex:1">`)}
      <button class="action-btn" onclick="wizWsSaveWave(this)" style="width:100%;margin-top:4px;padding:4px 8px;font-size:11px;flex:none">Save &#8963;</button>
    </div>
  </div>`;
}
function wizAddBossWave() {
  const list = document.getElementById('wiz-ws-wave-list');
  if (!list) return;
  list.insertAdjacentHTML('beforeend', _wizWsBossHtml(_wizWsTeamOpts(), _wizWsPathOpts()));
  _wizWsRenumber();
}
async function wizWsPickBossUnit(el) {
  const disp = el.closest('.wiz-ws-wave')?.querySelector('.wiz-ws-boss-unit');
  if (!disp) return;
  const palette = await loadObjectPalette();
  openObjPicker(palette, { unitOnly: true, callback: tmpl => { disp.dataset.tpl = tmpl; disp.textContent = tmpl; }});
}
function _wizWsRenumber() {
  const pacing = document.getElementById('wiz-ws-pacing')?.value || 'timed';
  let n = 0, bossIdx = 0;
  document.querySelectorAll('#wiz-ws-wave-list .wiz-ws-wave').forEach((row) => {
    if (row.classList.contains('wiz-ws-boss')) {
      const hint = row.querySelector('.wiz-ws-delay-hint');
      if (hint) {
        if (bossIdx === 0) hint.textContent = pacing === 'timed' ? 'sec from game start' : 'sec after last wave cleared';
        else               hint.textContent = 'sec after prev boss cleared';
      }
      bossIdx++;
      return;
    }
    n++;
    const lbl = row.querySelector('.wiz-ws-label');
    if (lbl) lbl.textContent = `WAVE ${n}`;
    const hint = row.querySelector('.wiz-ws-delay-hint');
    if (hint) hint.textContent = n === 1
      ? 'sec after game start'
      : (pacing === 'timed' ? 'sec after prev spawns' : 'sec after prev destroyed (0 = immediate)');
  });
}
function wizAddWave() {
  const list = document.getElementById('wiz-ws-wave-list');
  if (!list) return;
  const idx = list.querySelectorAll('.wiz-ws-wave').length;
  list.insertAdjacentHTML('beforeend', _wizWsWaveHtml(idx, _wizWsTeamOpts(), _wizWsPathOpts()));
}
function wizWsRemoveWave(btn) {
  btn.closest('.wiz-ws-wave')?.remove();
  _wizWsRenumber();
}
function wizWsCollapseWave(row, collapse = true) {
  row.dataset.collapsed = collapse ? 'true' : 'false';
  const body = row.querySelector('.wiz-ws-body');
  if (body) body.style.display = collapse ? 'none' : '';
  _wizWsUpdateSummary(row);
}
function wizWsToggleWave(headerEl) {
  const row = headerEl.closest('.wiz-ws-wave');
  if (!row) return;
  wizWsCollapseWave(row, row.dataset.collapsed !== 'true');
}
function wizWsSaveWave(btn) {
  wizWsCollapseWave(btn.closest('.wiz-ws-wave'), true);
}
function _wizWsUpdateSummary(row) {
  const summary = row.querySelector('.wiz-ws-summary');
  if (!summary) return;
  if (row.dataset.collapsed === 'true') {
    const team = row.querySelector('.wiz-ws-team')?.value || '—';
    const path = row.querySelector('.wiz-ws-path')?.value || '—';
    summary.textContent = `${team}  →  ${path}`;
  } else {
    summary.textContent = '';
  }
}

function wizRefreshWpDropdowns(selectName, wps) {
  wps = wps || [];
  _wizWpData = wps;
  const emptyOpt = '<option value="">-- none --</option>';

  // Waypoint dropdown
  const wpOpts = emptyOpt + wps.map(w => `<option value="${w.name}">${w.name}</option>`).join('');
  const spSel = document.getElementById('wiz-sp-waypoint');
  if (spSel) { spSel.innerHTML = wpOpts; if (selectName) spSel.value = selectName; }
  const gposSel = document.getElementById('wiz-sp-gpos');
  if (gposSel) { const prev = gposSel.value; gposSel.innerHTML = wpOpts; if (prev) gposSel.value = prev; }

  // Path dropdown — collect unique path labels from waypoints
  const pathSet = new Set();
  wps.forEach(w => (w.paths || []).forEach(p => pathSet.add(p)));
  const pathOpts = emptyOpt + [...pathSet].map(p => `<option value="${p}">${p}</option>`).join('');
  const pathSel = document.getElementById('wiz-sp-path');
  if (pathSel) { const prev = pathSel.value; pathSel.innerHTML = pathOpts; if (prev) pathSel.value = prev; }
}

function wizGoPlaceWaypoint() {
  _wizWpLastName = null;
  _wizWpLastCoords = null;

  // Reset place UI (remove leftover rename buttons from previous use)
  const placeUi = document.getElementById('wiz-sp-place-ui');
  if (placeUi) {
    placeUi.querySelectorAll('.action-btn.primary').forEach(b => b.remove());
    const cancelBtn = placeUi.querySelector('button.action-btn');
    if (cancelBtn) cancelBtn.textContent = 'Cancel';
  }

  // Auto-number: count current options minus the "--none--" entry
  const existing = document.querySelectorAll('#wiz-sp-waypoint option').length - 1;
  const autoName = `SpawnPoint_${Math.max(1, existing + 1)}`;
  const nameEl = document.getElementById('wiz-wp-qname');
  if (nameEl) nameEl.value = autoName;

  document.getElementById('wiz-sp-waypt-wrap').style.display = 'none';
  document.getElementById('wiz-sp-place-ui').style.display = '';
  const state = document.getElementById('wiz-wp-qstate');
  if (state) { state.textContent = '↓ Click on the map to place ↓'; state.style.color = '#4a90e2'; }
  document.getElementById('wiz-wp-qfb').textContent = '';

  _wizWpQuickPlace = true;
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

function cancelWizWpPlace() {
  _wizWpQuickPlace = false;
  document.getElementById('wiz-sp-place-ui').style.display = 'none';
  document.getElementById('wiz-sp-waypt-wrap').style.display = '';
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

async function applyWizWpRename() {
  const newName = document.getElementById('wiz-wp-qname')?.value.trim();
  if (!newName || !_wizWpLastCoords) { cancelWizWpPlace(); return; }
  if (newName === _wizWpLastName) { cancelWizWpPlace(); return; }

  // delete old, re-place with new name at same coords
  await api('/delete/waypoint', { name: _wizWpLastName });
  const r = await api('/place/waypoint', { name: newName, wx: _wizWpLastCoords.wx, wy: _wizWpLastCoords.wy });
  if (!r?.ok) {
    showFeedback('wiz-wp-qfb', r?.error || 'Rename failed', false);
    return;
  }
  _wizWpLastName = newName;
  syncWaypointsState();
  const r2 = await apiGet('/map/waypoints');
  wizRefreshWpDropdowns(newName, r2?.waypoints);
  cancelWizWpPlace();
}

async function handleWizQuickWaypointPlace(screenX, screenY) {
  const inlineTarget = _wizWpInlineTarget;
  _wizWpInlineTarget = null;
  _wizWpQuickPlace   = false;
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);

  const uiId = inlineTarget ? inlineTarget + '-place-ui' : 'wiz-sp-place-ui';
  const name = (inlineTarget
    ? document.getElementById(uiId + '-name')
    : document.getElementById('wiz-wp-qname'))?.value.trim() || 'SpawnPoint_1';

  const wc = await screenToWorldAsync(screenX, screenY);
  if (!wc) {
    _wizWpQuickPlace = true;
    if (inlineTarget) _wizWpInlineTarget = inlineTarget;
    if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
    return;
  }

  _wizWpLastName   = name;
  _wizWpLastCoords = wc;

  const r = await api('/place/waypoint', { name, wx: wc.wx, wy: wc.wy });
  if (!r?.ok) {
    if (inlineTarget) {
      const fbEl = document.getElementById(uiId + '-fb');
      if (fbEl) fbEl.textContent = r?.error || 'Failed';
      _wizWpQuickPlace = true;
      _wizWpInlineTarget = inlineTarget;
    } else {
      showFeedback('wiz-wp-qfb', r?.error || 'Failed', false);
      _wizWpQuickPlace = true;
    }
    if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
    return;
  }

  syncWaypointsState();
  const r2 = await apiGet('/map/waypoints');
  if (r2?.waypoints) _wizWpData = r2.waypoints;

  if (inlineTarget) {
    const sel = document.getElementById(inlineTarget);
    if (sel && r2?.waypoints) {
      sel.innerHTML = '<option value="">-- none --</option>' +
        r2.waypoints.map(w => `<option value="${w.name}">${w.name}</option>`).join('');
      sel.value = name;
    }
    wizRefreshWpDropdowns(null, r2?.waypoints);

    const stateEl = document.getElementById(uiId + '-state');
    if (stateEl) { stateEl.textContent = '✓ Placed'; stateEl.style.color = '#5cb85c'; }
    const cancelBtn = document.getElementById(uiId)?.querySelector('.action-btn');
    if (cancelBtn) cancelBtn.textContent = 'Done ✓';
    setTimeout(() => wizCancelWpInline(inlineTarget), 1200);
  } else {
    wizRefreshWpDropdowns(name, r2?.waypoints);
    const state = document.getElementById('wiz-wp-qstate');
    if (state) { state.textContent = '✓ Placed — edit name if needed'; state.style.color = '#5cb85c'; }
    showFeedback('wiz-wp-qfb', '', true);

    const placeUi = document.getElementById('wiz-sp-place-ui');
    if (placeUi) {
      const cancelBtn = placeUi.querySelector('button');
      if (cancelBtn) cancelBtn.textContent = 'Done';
      const renameBtn = document.createElement('button');
      renameBtn.className = 'action-btn primary';
      renameBtn.style.cssText = 'width:100%;margin-bottom:4px;padding:4px;font-size:11px;flex:none';
      renameBtn.textContent = 'Apply name';
      renameBtn.onclick = applyWizWpRename;
      cancelBtn.before(renameBtn);
    }
  }
}

// ── Inline team creation modal ────────────────────────────────────────────────

function openNewTeamFromWizard() {
  const backdrop = document.getElementById('wiz-team-backdrop');
  backdrop.style.display = 'flex';
  ipcRenderer.send('viewport-mouse', false);

  // Populate owner dropdown, default to SkirmishGLA
  const ownerSel = document.getElementById('wt-owner');
  const players  = _sidelistData?.players || [];
  ownerSel.innerHTML = players.map(p => `<option value="${p.name}">${p.name}</option>`).join('');
  const gla = players.find(p => p.name === 'SkirmishGLA');
  if (gla) ownerSel.value = 'SkirmishGLA';

  // Reset form
  document.getElementById('wt-name').value = '';
  document.getElementById('wt-units').innerHTML = '';
  document.getElementById('wt-feedback').textContent = '';
  addWizTeamUnitRow();  // start with one empty unit row
}

function closeWizTeamModal() {
  document.getElementById('wiz-team-backdrop').style.display = 'none';
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

function addWizTeamUnitRow(template = '', count = 1) {
  const container = document.getElementById('wt-units');
  const row = document.createElement('div');
  row.className = 'form-row';
  row.style.cssText = 'margin-bottom:4px;gap:4px';

  const disp = document.createElement('span');
  disp.className = 'wb-form-input';
  disp.dataset.tpl = template || '';
  disp.textContent = template || '— pick unit —';
  disp.title = 'Click to pick a unit';
  disp.style.cssText = `flex:3;min-width:0;cursor:pointer;overflow:hidden;text-overflow:ellipsis;white-space:nowrap${template ? '' : ';color:var(--text-dim)'}`;
  disp.addEventListener('click', async () => {
    const palette = await loadObjectPalette();
    openObjPicker(palette, { unitOnly: true, callback: tmpl => {
      disp.dataset.tpl = tmpl;
      disp.textContent = tmpl;
      disp.style.color = '';
    }});
  });

  const countIn = document.createElement('input');
  countIn.type = 'number';
  countIn.className = 'wb-form-input wb-num';
  countIn.value = count;
  countIn.min = 1; countIn.max = 20;
  countIn.style.cssText = 'flex:none;width:48px';
  const delBtn = document.createElement('button');
  delBtn.className = 'action-btn';
  delBtn.style.cssText = 'padding:2px 6px;min-width:28px;flex:none';
  delBtn.textContent = '✕';
  delBtn.addEventListener('click', () => row.remove());
  row.append(disp, countIn, delBtn);
  container.appendChild(row);
}

async function createWizTeam() {
  const name  = document.getElementById('wt-name').value.trim();
  const owner = document.getElementById('wt-owner').value;
  if (!name) { showFeedback('wt-feedback', 'Name is required', false); return; }

  const body = { name, owner, home: '', isSingleton: true, priority: 1000 };
  let unitIdx = 0;
  document.getElementById('wt-units').querySelectorAll('.form-row').forEach(row => {
    const tpl = (row.querySelector('span[data-tpl]')?.dataset.tpl || '').trim();
    const cnt = parseInt(row.querySelector('input[type="number"]')?.value) || 1;
    if (tpl) {
      body[`unit${unitIdx}_template`] = tpl;
      body[`unit${unitIdx}_count`]    = cnt;
      unitIdx++;
    }
  });

  if (!unitIdx) { showFeedback('wt-feedback', 'Add at least one unit type', false); return; }

  const r = await api('/sidelist/team', body);
  if (!r?.ok) { showFeedback('wt-feedback', r?.error || 'Failed', false); return; }

  closeWizTeamModal();
  await fetchSideList();

  if (_wizTemplate === 'waves') {
    // Refresh team dropdowns in-place (don't rebuild the form — that would wipe current wave config)
    document.querySelectorAll('#wiz-ws-wave-list .wiz-ws-team').forEach(sel => {
      const prev = sel.value;
      sel.innerHTML = _wizWsTeamOpts();
      sel.value = prev || name;
    });
  } else {
    // Refresh the specific team select(s) for the active wizard in-place — no full re-render
    const allTeamOpts = (_sidelistData?.teams || []).filter(t => !_SYSTEM_TEAM_RE.test(t.name))
      .map(t => `<option value="${t.name}">${t.name}</option>`).join('');
    const selIds = {
      spawn: ['wiz-sp-team', 'wiz-sp-dest-team'],
      boss:  ['wiz-boss-trigteam'],
      win:   ['wiz-win-team'],
      area:  ['wiz-area-team'],
    }[_wizTemplate] || ['wiz-sp-team'];
    selIds.forEach(id => {
      const sel = document.getElementById(id);
      if (!sel) return;
      const prev = sel.value;
      sel.innerHTML = allTeamOpts;
      sel.value = prev || name;
    });
    if (_wizTemplate === 'spawn') wizSpawnTeamChange();
  }
}

// Called when team dropdown changes in spawn wizard — update "owner" info line
function wizSpawnTeamChange() {
  const team = document.getElementById('wiz-sp-team')?.value || '';
  const owner = wizTeamOwner(team);
  const info = document.getElementById('wiz-sp-owner-info');
  if (info) info.textContent = owner ? `Scripts owned by: ${owner}` : '';
}

// Called when "When" dropdown changes in spawn wizard — show/hide sub-fields
function wizSpawnWhenChange() {
  const when = document.getElementById('wiz-sp-when')?.value;
  document.getElementById('wiz-sp-min-row').style.display   = (when === 'after' || when === 'repeat') ? '' : 'none';
  document.getElementById('wiz-sp-dest-row').style.display  = (when === 'destroyed') ? '' : 'none';
}

const _GOAL_NEEDS_PATH = new Set(['follow_exact', 'follow_path', 'flee']);
function wizSpawnGoalChange() {
  const goal = document.getElementById('wiz-sp-goal')?.value;
  document.getElementById('wiz-sp-path-row').style.display   = _GOAL_NEEDS_PATH.has(goal) ? '' : 'none';
  document.getElementById('wiz-sp-area-row').style.display   = (goal === 'guard_area')    ? '' : 'none';
  document.getElementById('wiz-sp-gpos-row').style.display   = (goal === 'guard_pos')     ? '' : 'none';
  // Path goals auto-derive spawn point from path start — hide manual waypoint picker
  const wayptWrap = document.getElementById('wiz-sp-waypt-wrap');
  if (wayptWrap) wayptWrap.style.display = _GOAL_NEEDS_PATH.has(goal) ? 'none' : '';
}

// Map of template → { title, buildFields(container), buildScript() }
const WIZ_TEMPLATES = {

  spawn: {
    title: 'Spawn Units',
    buildFields(f) {
      const teamSel   = wizTeamSelect('wiz-sp-team');
      const destTeams = (_sidelistData?.teams || []).filter(t => !_SYSTEM_TEAM_RE.test(t.name)).map(t => t.name);
      const firstOwner = wizTeamOwner((_sidelistData?.teams||[]).find(t => !_SYSTEM_TEAM_RE.test(t.name))?.name || '');

      f.innerHTML = ''
        + wizRow('Team', teamSel)
        + `<div id="wiz-sp-owner-info" class="hint-text" style="margin-bottom:6px">${firstOwner ? `Scripts owned by: ${firstOwner}` : ''}</div>`
        + `<div class="panel-divider"></div>`

        // ── GOAL ──────────────────────────────────────────────────────────
        + wizRow('Goal', `<select class="wb-form-input" id="wiz-sp-goal" style="flex:1" onchange="wizSpawnGoalChange()">
            <option value="">— choose a goal —</option>
            <option value="follow_exact">Follow path (exact / AOD)</option>
            <option value="follow_path">Follow path (free attack)</option>
            <option value="hunt">Hunt enemies</option>
            <option value="guard_area">Guard area</option>
            <option value="guard_pos">Guard position</option>
            <option value="flee">Flee along path</option>
            <option value="none">Nothing (hold position)</option>
          </select>`)
        + `<div id="wiz-sp-path-row" style="display:none">` + wizRow('Path', `<select class="wb-form-input" id="wiz-sp-path" style="flex:1"><option value="">Loading...</option></select><button class="action-btn" onclick="wizGoDrawSpawnPath()" title="Draw new path on map" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button>`) + `</div>`
        + `<div id="wiz-sp-path-draw-ui" style="display:none;background:rgba(74,144,226,0.08);border:1px solid rgba(74,144,226,0.25);border-radius:6px;padding:8px;margin-bottom:6px">`
        + `  <div style="display:flex;gap:6px;align-items:center;margin-bottom:6px"><span class="form-label" style="flex:none">Name</span><input class="wb-form-input wiz-sp-path-name" style="flex:1" value="Path_1"></div>`
        + `  <div class="wiz-sp-path-status" style="font-size:11px;color:#4a90e2;text-align:center;margin-bottom:4px"></div>`
        + `  <div class="wiz-sp-path-fb" style="font-size:11px;color:#e05c5c;text-align:center;margin-bottom:4px"></div>`
        + `  <button class="action-btn wiz-sp-path-cancel" onclick="cancelWizSpawnPathDraw()" style="width:100%">Cancel</button>`
        + `</div>`
        + `<div id="wiz-sp-area-row" style="display:none">` + wizAreaInlineRow('wiz-sp-area', 'Area', 'GuardArea') + `</div>`
        + `<div id="wiz-sp-gpos-row" style="display:none">` + wizWpInlineRow('wiz-sp-gpos', 'Position', [], 'GuardPos') + `</div>`

        // ── SPAWN AT (waypoint) ───────────────────────────────────────────
        + `<div id="wiz-sp-waypt-wrap">`
          + wizRow('Spawn at', `<select class="wb-form-input" id="wiz-sp-waypoint" style="flex:1"><option value="">Loading...</option></select><button class="action-btn" onclick="wizGoPlaceWaypoint()" title="Place a new waypoint on the map" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button>`)
        + `</div>`
        + `<div id="wiz-sp-place-ui" style="display:none;background:rgba(74,144,226,0.08);border:1px solid rgba(74,144,226,0.25);border-radius:6px;padding:8px;margin-bottom:6px">
            <div style="font-size:11px;color:var(--text-dim);margin-bottom:6px">New waypoint</div>
            <div class="form-row" style="margin-bottom:8px">
              <span class="form-label" style="min-width:50px;font-size:11px">Name</span>
              <input class="wb-form-input" id="wiz-wp-qname" placeholder="SpawnPoint_1" style="flex:1">
            </div>
            <div id="wiz-wp-qstate" style="font-size:11px;color:#4a90e2;text-align:center;padding:4px 0">&#8595; Click on the map to place &#8595;</div>
            <div id="wiz-wp-qfb" style="font-size:10px;margin-top:4px"></div>
            <button class="action-btn" onclick="cancelWizWpPlace()" style="width:100%;margin-top:6px;padding:4px;font-size:11px;flex:none">Cancel</button>
          </div>`

        // ── STANCE ───────────────────────────────────────────────────────
        + wizRow('Stance', `<select class="wb-form-input" id="wiz-sp-stance" style="flex:1">
            <option value="0">Normal (default)</option>
            <option value="2">Aggressive</option>
            <option value="1">Alert</option>
            <option value="-1">Passive</option>
            <option value="-2">Sleep</option>
          </select>`)

        + `<div class="panel-divider"></div>`

        // ── WHEN ─────────────────────────────────────────────────────────
        + wizRow('When', `<select class="wb-form-input" id="wiz-sp-when" style="flex:1" onchange="wizSpawnWhenChange()">
            <option value="start">At scenario start</option>
            <option value="after">After N minutes</option>
            <option value="repeat">Every N minutes</option>
            <option value="destroyed">When a team is destroyed</option>
          </select>`)
        + `<div id="wiz-sp-min-row" style="display:none">`
          + wizRow('Minutes', `<input class="wb-form-input wb-num" id="wiz-sp-min" type="number" value="10" min="0.5" step="0.5" style="flex:1">`)
        + `</div>`
        + `<div id="wiz-sp-dest-row" style="display:none">`
          + wizRow('Team', `<div style="display:flex;gap:4px"><select class="wb-form-input" id="wiz-sp-dest-team" style="flex:1">${destTeams.map(t => `<option value="${t}">${t}</option>`).join('')}</select><button class="action-btn" onclick="openNewTeamFromWizard()" title="New team" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button></div>`)
        + `</div>`

        + `<div class="panel-divider"></div>`

        // ── SCRIPT GROUP NAME ─────────────────────────────────────────────
        + wizRow('Group', `<input class="wb-form-input" id="wiz-sp-group" placeholder="[Quick] Spawn" style="flex:1">`);

      // Fetch waypoints and populate dropdowns
      apiGet('/map/waypoints').then(r => {
        wizRefreshWpDropdowns(null, r?.waypoints);
        _wizLoadAreaSelect('wiz-sp-area');
      });
    },
    buildScript() {
      const team    = document.getElementById('wiz-sp-team')?.value || '';
      const goal    = document.getElementById('wiz-sp-goal')?.value || '';
      const stance  = parseInt(document.getElementById('wiz-sp-stance')?.value) || 0;
      const wp      = (document.getElementById('wiz-sp-waypoint')?.value || '').trim();
      const path    = document.getElementById('wiz-sp-path')?.value || '';
      const area    = document.getElementById('wiz-sp-area')?.value || '';
      const gpos    = document.getElementById('wiz-sp-gpos')?.value || '';
      const when    = document.getElementById('wiz-sp-when')?.value || 'start';
      const mins    = parseFloat(document.getElementById('wiz-sp-min')?.value) || 10;
      const secs    = mins * 60;
      const destTm  = document.getElementById('wiz-sp-dest-team')?.value || '';
      const groupIn = document.getElementById('wiz-sp-group')?.value.trim();
      const player  = wizTeamOwner(team);
      const group   = groupIn || '[Quick] Spawn';
      const timerNm = `spawn_${team}_timer`;
      if (!team || !goal) return [];

      // For path goals, derive spawn point from first waypoint belonging to the chosen path
      let spawnWp = wp;
      if (_GOAL_NEEDS_PATH.has(goal) && path) {
        const firstOnPath = _wizWpData.find(w => (w.paths || []).includes(path));
        if (firstOnPath) spawnWp = firstOnPath.name;
      }

      // Mét spawn-waypoint: CREATE_REINFORCEMENT_TEAM (34) — spawnt direct, werkt voor een
      // dummy-vijand zonder productie (AOD). Zonder waypoint: BUILD_TEAM (69) — AI bouwt 'm zelf.
      const spawnActions = spawnWp
        ? [{type:34, params:[{pt:'TEAM',v:team},{pt:'WAYPOINT',v:spawnWp}]}]
        : [{type:69, params:[{pt:'TEAM',v:team}]}];

      // STANCE → always set attitude after spawn
      spawnActions.push({type:46, params:[{pt:'TEAM',v:team},{pt:'AI_MOOD',v:String(stance)}]});

      // GOAL → movement/behavior action
      if (goal === 'follow_exact') {
        spawnActions.push({type:281, params:[{pt:'TEAM',v:team},{pt:'WAYPOINT_PATH',v:path},{pt:'BOOLEAN',v:'0'}]});
      } else if (goal === 'follow_path') {
        spawnActions.push({type:36,  params:[{pt:'TEAM',v:team},{pt:'WAYPOINT_PATH',v:path},{pt:'BOOLEAN',v:'0'}]});
      } else if (goal === 'hunt') {
        spawnActions.push({type:60,  params:[{pt:'TEAM',v:team}]});
      } else if (goal === 'guard_area') {
        spawnActions.push({type:204, params:[{pt:'TEAM',v:team},{pt:'TRIGGER_AREA',v:area}]});
      } else if (goal === 'guard_pos') {
        spawnActions.push({type:202, params:[{pt:'TEAM',v:team},{pt:'WAYPOINT',v:gpos}]});
      } else if (goal === 'flee') {
        spawnActions.push({type:113, params:[{pt:'TEAM',v:team},{pt:'WAYPOINT_PATH',v:path}]});
      }
      // goal === 'none' → no movement action, only stance

      if (when === 'start') {
        return [buildFlatScript(player, group, `${team}_spawn`,
          [{type:3,params:[]}],
          spawnActions, true)];
      }
      if (when === 'after') {
        const s1 = buildFlatScript(player, group, `${team}_start_timer`,
          [{type:3,params:[]}],
          [{type:20,params:[{pt:'TIMER_NAME',v:timerNm},{pt:'REAL',v:String(secs)}]}], true);
        const s2 = buildFlatScript(player, group, `${team}_spawn`,
          [{type:4,params:[{pt:'TIMER_NAME',v:timerNm}]}],
          spawnActions, true);
        return [s1, s2];
      }
      if (when === 'repeat') {
        const s1 = buildFlatScript(player, group, `${team}_start_timer`,
          [{type:3,params:[]}],
          [{type:20,params:[{pt:'TIMER_NAME',v:timerNm},{pt:'REAL',v:String(secs)}]}], true);
        const s2 = buildFlatScript(player, group, `${team}_spawn_wave`,
          [{type:4,params:[{pt:'TIMER_NAME',v:timerNm}]}],
          [...spawnActions, {type:20,params:[{pt:'TIMER_NAME',v:timerNm},{pt:'REAL',v:String(secs)}]}], false);
        return [s1, s2];
      }
      if (when === 'destroyed') {
        return [buildFlatScript(player, group, `${team}_spawn_on_destroy`,
          [{type:8,params:[{pt:'TEAM',v:destTm}]}],
          spawnActions, false)];
      }
      return [];
    },
  },

  waves: {
    title: 'Wave Sequence',
    buildFields(f) {
      f.innerHTML = ''
        + wizRow('Pacing', `<select class="wb-form-input" id="wiz-ws-pacing" onchange="_wizWsRenumber()" style="flex:1">
            <option value="timed">Timed — waves spawn on a fixed schedule</option>
            <option value="gated">Clear to advance — next wave spawns after prev is destroyed</option>
          </select>`)
        + wizRow('Group', `<input class="wb-form-input" id="wiz-ws-group" placeholder="[AOD] Waves" style="flex:1">`)
        + `<div class="panel-divider"></div>`
        + `<div id="wiz-ws-wave-list"></div>`
        + `<button class="action-btn" onclick="wizAddWave()" style="width:100%;margin-bottom:4px">+ Add Wave</button>`
        + `<button class="action-btn" onclick="wizAddBossWave()" style="width:100%;margin-bottom:6px;border-color:#603030;color:#e0a87d">+ Add Boss Wave</button>`;

      // Load waypoints + trigger areas in parallel (areas needed to filter path dropdowns)
      Promise.all([apiGet('/map/waypoints'), apiGet('/map/triggers')]).then(([r, rt]) => {
        if (r?.waypoints) _wizWpData = r.waypoints;
        _wizAreaData = (rt?.triggers || []).filter(t => t.name && t.name !== 'Default Water');
        if (_waveSeqState) {
          if (_waveSeqState.pacing) document.getElementById('wiz-ws-pacing').value = _waveSeqState.pacing;
          document.getElementById('wiz-ws-group').value    = _waveSeqState.group;
          _waveSeqState.waves.forEach((w, i) => {
            wizAddWave();
            const row = document.querySelectorAll('#wiz-ws-wave-list .wiz-ws-wave')[i];
            if (!row) return;
            row.querySelector('.wiz-ws-team').value  = w.team;
            const pathSel = row.querySelector('.wiz-ws-path');
            pathSel.innerHTML = _wizWsPathOpts();
            pathSel.value = w.path;
            row.querySelector('.wiz-ws-delay').value   = w.delay;
            row.querySelector('.wiz-ws-caption').value = w.caption || '';
            const ms = row.querySelector('.wiz-ws-movement'); if (ms && w.movement) ms.value = w.movement;
            const ss = row.querySelector('.wiz-ws-stance');   if (ss && w.stance)   ss.value = w.stance;
            if (w.collapsed) wizWsCollapseWave(row, true);
          });
        } else {
          wizAddWave();
        }
      });
    },
    async beforeCreate() {
      // Boss-units zijn geplaatste OBJECTEN — bij her-toepassen eerst opruimen (op naam),
      // anders krijg je dubbele units. Daarna opnieuw plaatsen + name/veterancy/maxHP zetten.
      const bossEls   = [...document.querySelectorAll('#wiz-ws-wave-list .wiz-ws-boss')];
      const firstTeam = document.querySelector('#wiz-ws-wave-list .wiz-ws-wave:not(.wiz-ws-boss) .wiz-ws-team')?.value;
      const player    = (firstTeam ? wizTeamOwner(firstTeam) : 'SkirmishGLA') || 'SkirmishGLA';
      const team      = 'team' + player;
      // sweep: verwijder mogelijke oude boss-objecten (deterministische namen)
      for (let j = 1; j <= 8; j++) for (let k = 1; k <= 6; k++) {
        const sel = await api('/object/select', { name: `boss_${j}_${k}` });
        if (sel?.ok) await api('/object/delete_selected', {});
      }
      // plaats nieuwe boss-units bij de eerste waypoint van hun pad
      for (let j = 0; j < bossEls.length; j++) {
        const el  = bossEls[j];
        const tpl = el.querySelector('.wiz-ws-boss-unit')?.dataset.tpl || 'GLATankMarauder';
        const cnt = parseInt(el.querySelector('.wiz-ws-boss-count')?.value) || 1;
        const vet = parseInt(el.querySelector('.wiz-ws-boss-vet')?.value) || 3;
        const mhp = parseInt(el.querySelector('.wiz-ws-boss-maxhp')?.value) || 0;
        const path = el.querySelector('.wiz-ws-path')?.value || '';
        const wp  = (_wizWpData || []).find(w => (w.paths || []).includes(path)) || (_wizWpData || [])[0];
        if (!wp) continue;
        for (let k = 0; k < cnt; k++) {
          await api('/place/object', { template: tpl, wx: wp.wx + k * 130, wy: wp.wy, angle: 180, team });
          await api('/object/set_prop', { key: 'name', value: `boss_${j + 1}_${k + 1}` });
          await api('/object/set_prop', { key: 'veterancy', value: vet });
          if (mhp) await api('/object/set_prop', { key: 'maxHP', value: mhp });
        }
      }
      // Bestaande script-cleanup: verwijder scripts van weggehaalde (team-)waves.
      if (_waveSeqState?.scriptNames?.length) {
        const teamCount = document.querySelectorAll('#wiz-ws-wave-list .wiz-ws-wave:not(.wiz-ws-boss)').length;
        const keep = new Set(['wave_init', 'boss_init', ...Array.from({length: teamCount}, (_, i) => `wave_${i + 1}`)]);
        for (const sname of _waveSeqState.scriptNames) {
          if (!keep.has(sname)) {
            await api('/sidelist/script/delete', { player: _waveSeqState.player, group: _waveSeqState.group, name: sname });
          }
        }
      }
    },
    afterCreate(scripts) {
      const waves = [...document.querySelectorAll('#wiz-ws-wave-list .wiz-ws-wave')].map((el, i) => ({
        team:      el.querySelector('.wiz-ws-team')?.value    || '',
        path:      el.querySelector('.wiz-ws-path')?.value    || '',
        delay:     parseFloat(el.querySelector('.wiz-ws-delay')?.value) || 120,
        caption:   el.querySelector('.wiz-ws-caption')?.value.trim() || `Wave ${i + 1} incoming!`,
        collapsed: el.dataset.collapsed === 'true',
        movement:  el.querySelector('.wiz-ws-movement')?.value || 'follow',
        stance:    el.querySelector('.wiz-ws-stance')?.value   || '2',
      }));
      _waveSeqState = {
        pacing:      document.getElementById('wiz-ws-pacing')?.value    || 'timed',
        group:       document.getElementById('wiz-ws-group')?.value.trim() || '[AOD] Waves',
        player:      scripts[0]?.player || '',
        waves,
        scriptNames: scripts.map(s => s.name),
      };
    },
    buildScript() {
      const pacing   = document.getElementById('wiz-ws-pacing')?.value || 'timed';
      const groupIn  = document.getElementById('wiz-ws-group')?.value.trim();
      const group    = groupIn || '[AOD] Waves';

      const waveEls = [...document.querySelectorAll('#wiz-ws-wave-list .wiz-ws-wave:not(.wiz-ws-boss)')];
      if (!waveEls.length) return [];

      const waves = waveEls.map((el, i) => ({
        team:     el.querySelector('.wiz-ws-team')?.value    || '',
        path:     el.querySelector('.wiz-ws-path')?.value    || '',
        delay:    parseFloat(el.querySelector('.wiz-ws-delay')?.value) || 0,
        caption:  el.querySelector('.wiz-ws-caption')?.value.trim() || `Wave ${i + 1} incoming!`,
        movement: el.querySelector('.wiz-ws-movement')?.value || 'follow',
        stance:   parseInt(el.querySelector('.wiz-ws-stance')?.value) || 2,
      }));

      const bossEls = [...document.querySelectorAll('#wiz-ws-wave-list .wiz-ws-boss')];
      if (!waves.some(w => w.team) && !bossEls.length) return [];
      const firstTeam = waves.find(w => w.team)?.team;
      const player = (firstTeam ? wizTeamOwner(firstTeam) : 'SkirmishGLA') || 'SkirmishGLA';

      const scripts = [];

      // Init: game start → set wave 1 timer; in timed mode also set boss absolute timer directly.
      const delay1 = waves[0]?.delay || 60;
      const initActions = [{type: 20, params: [{pt: 'TIMER_NAME', v: 'wave_1_timer'}, {pt: 'REAL', v: String(delay1)}]}];
      if (pacing === 'timed' && bossEls.length) {
        const boss1Delay = parseFloat(bossEls[0]?.querySelector('.wiz-ws-delay')?.value) || 0;
        initActions.push({type: 20, params: [{pt: 'TIMER_NAME', v: 'boss_1_timer'}, {pt: 'REAL', v: String(boss1Delay)}]});
      }
      scripts.push(buildFlatScript(player, group, `wave_init`, [{type: 3, params: []}], initActions, true));

      waves.forEach((w, i) => {
        if (!w.team) return;
        const spawnWp = _wizWpData.find(wp => (wp.paths || []).includes(w.path))?.name;

        const spawnActions = [];
        // CREATE_REINFORCEMENT_TEAM (34) spawnt het team direct bij de waypoint — werkt voor een
        // AOD-dummy-vijand zonder productie. BUILD_TEAM (69) vereist gebouwen → spawnt niets in AOD.
        if (spawnWp) spawnActions.push({type: 34, params: [{pt: 'TEAM', v: w.team}, {pt: 'WAYPOINT', v: spawnWp}]});
        spawnActions.push({type: 46, params: [{pt: 'TEAM', v: w.team}, {pt: 'AI_MOOD', v: String(w.stance)}]});
        if (w.path) {
          const followType = w.movement === 'exact' ? 281 : 36;
          spawnActions.push({type: followType, params: [{pt: 'TEAM', v: w.team}, {pt: 'WAYPOINT_PATH', v: w.path}, {pt: 'BOOLEAN', v: '0'}]});
        }
        // SHOW_MILITARY_CAPTION (139), duur in MILLISECONDEN. Was 134 (CAMERA_MOTION_BLUR_JUMP)
        // met 10ms → onzichtbaar.
        spawnActions.push({type: 139, params: [{pt: 'TEXT_STRING', v: w.caption}, {pt: 'INT', v: '6000'}]});

        if (pacing === 'timed') {
          // TIMED (relentless): elke golf op zijn eigen timer; deze golf zet meteen de timer
          // voor de volgende. Boss timer wordt apart gezet in wave_init — los van de golf-keten.
          const nextW = waves[i + 1];
          if (nextW && nextW.team) {
            const nextDelay = nextW.delay || 22;  // fallback-interval als geen delay opgegeven
            spawnActions.push({type: 20, params: [{pt: 'TIMER_NAME', v: `wave_${i + 2}_timer`}, {pt: 'REAL', v: String(nextDelay)}]});
          }
          scripts.push(buildFlatScript(player, group, `wave_${i + 1}`,
            [{type: 4, params: [{pt: 'TIMER_NAME', v: `wave_${i + 1}_timer`}]}],
            spawnActions, true));
        } else if (i === 0) {
          // GATED golf 1: TIMER_EXPIRED(wave_1_timer) → spawn
          scripts.push(buildFlatScript(player, group, `wave_1`,
            [{type: 4, params: [{pt: 'TIMER_NAME', v: 'wave_1_timer'}]}],
            spawnActions, true));
        } else {
          // GATED golf 2+: pas na vorige team vernietigd (+ optionele delay)
          const prevTeam   = waves[i - 1].team;
          const delayTimer = `wave_${i + 1}_delay`;
          if (w.delay > 0) {
            scripts.push(buildFlatScript(player, group, `wave_${i + 1}_trigger`,
              [{type: 8, params: [{pt: 'TEAM', v: prevTeam}]}],
              [{type: 20, params: [{pt: 'TIMER_NAME', v: delayTimer}, {pt: 'REAL', v: String(w.delay)}]}],
              true));
            scripts.push(buildFlatScript(player, group, `wave_${i + 1}`,
              [{type: 4, params: [{pt: 'TIMER_NAME', v: delayTimer}]}],
              spawnActions, true));
          } else {
            scripts.push(buildFlatScript(player, group, `wave_${i + 1}`,
              [{type: 8, params: [{pt: 'TEAM', v: prevTeam}]}],
              spawnActions, true));
          }
        }
      });

      // ── Boss-golven (NAMED, vooraf-geplaatst) — komen NA de team-golven in de cascade ──
      if (bossEls.length) {
        const teamCards = waves.filter(w => w.team);
        const lastTeam  = teamCards.length ? teamCards[teamCards.length - 1].team : null;
        // bevries alle boss-units bij game start (beforeCreate heeft ze al geplaatst)
        const allNames = [];
        bossEls.forEach((el, j) => {
          const cnt = parseInt(el.querySelector('.wiz-ws-boss-count')?.value) || 1;
          for (let k = 0; k < cnt; k++) allNames.push(`boss_${j + 1}_${k + 1}`);
        });
        scripts.push(buildFlatScript(player, group, 'boss_init',
          [{type: 3, params: []}],
          allNames.map(nm => ({type: 225, params: [{pt: 'UNIT', v: nm}, {pt: 'BOOLEAN', v: '1'}]})), true));

        // eerste boss triggert op:
        //   timed → TIMER_EXPIRED(boss_1_timer), absoluut gezet door wave_init (sec from game start)
        //   gated → TEAM_DESTROYED(lastTeam) + optionele delay, of CONDITION_TRUE als geen teams
        let prevDone = pacing === 'timed'
          ? [{type: 4, params: [{pt: 'TIMER_NAME', v: 'boss_1_timer'}]}]
          : lastTeam ? [{type: 8, params: [{pt: 'TEAM', v: lastTeam}]}] : [{type: 3, params: []}];
        bossEls.forEach((el, j) => {
          const cnt        = parseInt(el.querySelector('.wiz-ws-boss-count')?.value) || 1;
          const path       = el.querySelector('.wiz-ws-path')?.value || '';
          const delay      = parseFloat(el.querySelector('.wiz-ws-delay')?.value) || 0;
          const cap        = el.querySelector('.wiz-ws-caption')?.value.trim() || 'BOSS WAVE!';
          const bossStance = parseInt(el.querySelector('.wiz-ws-stance')?.value) || 2;
          const names = Array.from({length: cnt}, (_, k) => `boss_${j + 1}_${k + 1}`);
          const acts  = [{type: 139, params: [{pt: 'TEXT_STRING', v: cap}, {pt: 'INT', v: '6000'}]}];
          names.forEach(nm => {
            acts.push({type: 85,  params: [{pt: 'UNIT', v: nm}, {pt: 'SIDE', v: player}]});
            acts.push({type: 225, params: [{pt: 'UNIT', v: nm}, {pt: 'BOOLEAN', v: '0'}]});
            acts.push({type: 45,  params: [{pt: 'UNIT', v: nm}, {pt: 'AI_MOOD', v: String(bossStance)}]});
            if (path) acts.push({type: 282, params: [{pt: 'UNIT', v: nm}, {pt: 'WAYPOINT_PATH', v: path}]});
          });
          // Timed: boss 1 triggert op absolute timer (delay = sec from game start), geen extra tussentimer.
          // Gated: delay > 0 zet een tussentimer na het TEAM_DESTROYED event.
          // Boss 2+: altijd op unit-destroyed chain, delay mag nog steeds een tussentimer zijn.
          const useDirect = pacing === 'timed' && j === 0;
          if (!useDirect && delay > 0) {
            const dt = `boss_${j + 1}_delay`;
            scripts.push(buildFlatScript(player, group, `boss_${j + 1}_trigger`,
              prevDone, [{type: 20, params: [{pt: 'TIMER_NAME', v: dt}, {pt: 'REAL', v: String(delay)}]}], true));
            scripts.push(buildFlatScript(player, group, `boss_${j + 1}`,
              [{type: 4, params: [{pt: 'TIMER_NAME', v: dt}]}], acts, true));
          } else {
            scripts.push(buildFlatScript(player, group, `boss_${j + 1}`, prevDone, acts, true));
          }
          // volgende golf triggert op ALLE boss-units van deze golf vernietigd
          prevDone = names.map(nm => ({type: 15, params: [{pt: 'UNIT', v: nm}]}));
        });
      }

      return scripts;
    },
  },

  // Boss-wizard — vooraf-geplaatste, opgevoerde NAMED boss-units (pattern B uit echte maps).
  // beforeCreate plaatst de units + zet name/veterancy/maxHP; buildScript maakt hold/activatie/lose.
  boss: {
    title: 'Boss Unit (AOD)',
    buildFields(f) {
      const wpNames = (_wizWpData || []).map(w => w.name);
      const paths = new Set(); (_wizWpData || []).forEach(w => (w.paths || []).forEach(p => paths.add(p)));
      const teams = (_sidelistData?.teams || []).map(t => t.name);
      f.innerHTML = ''
        + wizPlayerRow('wiz-boss-player')
        + wizRow('Boss name', `<input class="wb-form-input" id="wiz-boss-name" value="Boss" style="flex:1">`)
        + wizRow('Unit type', `<span class="wb-form-input" id="wiz-boss-unit" data-tpl="GLATankMarauder" style="flex:1;cursor:pointer;overflow:hidden;text-overflow:ellipsis;white-space:nowrap" onclick="wizBossPickUnit()">GLATankMarauder</span><button class="action-btn" onclick="wizBossPickUnit()" title="Pick unit" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button>`)
        + wizRow('Count', `<input class="wb-form-input wb-num" id="wiz-boss-count" type="number" value="1" min="1" max="6" style="flex:1">`)
        + wizRow('Veterancy', wizSelect('wiz-boss-vet', ['3 - Heroic', '2 - Elite', '1 - Veteran', '0 - Regular']))
        + wizRow('Max HP', `<input class="wb-form-input wb-num" id="wiz-boss-maxhp" type="number" value="8000" min="0" step="500" style="flex:1">`)
        + wizWpInlineRow('wiz-boss-wp', 'Spawn at', wpNames, 'BossSpawn')
        + wizPathInlineRow('wiz-boss-path', 'Follow path', [...paths])
        + wizRow('Appears', `<select class="wb-form-input" id="wiz-boss-trigger" style="flex:1">
            <option value="team_dead">When a team is destroyed</option>
            <option value="start">At game start</option></select>`)
        + wizRow('After team', `<div style="display:flex;gap:4px"><select class="wb-form-input" id="wiz-boss-trigteam" style="flex:1">${teams.map(t => `<option value="${t}">${t}</option>`).join('')}</select><button class="action-btn" onclick="openNewTeamFromWizard()" title="New team" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button></div>`)
        + wizRow('Caption', `<input class="wb-form-input" id="wiz-boss-caption" value="BOSS WAVE!" style="flex:1">`)
        + wizAreaInlineRow('wiz-boss-lose', 'Lose area', 'LoseZone')
        + `<div class="hint-text" style="margin-top:4px">Pre-places tough named units off the spawn waypoint, frozen, then releases them down the path when triggered.</div>`;
      _wizLoadAreaSelect('wiz-boss-lose');
      apiGet('/map/waypoints').then(r => {
        if (!r?.waypoints) return;
        _wizWpData = r.waypoints;
        const wpSel = document.getElementById('wiz-boss-wp');
        if (wpSel) {
          wpSel.innerHTML = '<option value="">-- none --</option>' +
            r.waypoints.map(w => `<option value="${w.name}">${w.name}</option>`).join('');
        }
        const pathSet = new Set();
        r.waypoints.forEach(w => (w.paths || []).forEach(p => pathSet.add(p)));
        const pathSel = document.getElementById('wiz-boss-path');
        if (pathSel) {
          pathSel.innerHTML = '<option value="">— pick path —</option>' +
            [...pathSet].map(p => `<option value="${p}">${p}</option>`).join('');
        }
      });
    },
    _names() {
      const base = (document.getElementById('wiz-boss-name')?.value || 'Boss').trim();
      const n = parseInt(document.getElementById('wiz-boss-count')?.value) || 1;
      return n > 1 ? Array.from({length: n}, (_, i) => `${base}_${i + 1}`) : [base];
    },
    async beforeCreate() {
      const g = id => document.getElementById(id);
      const tpl   = (g('wiz-boss-unit')?.dataset.tpl || 'GLATankMarauder').trim();
      const owner = g('wiz-boss-player')?.value || '';
      const wpNm  = g('wiz-boss-wp')?.value || '';
      const vet   = parseInt((g('wiz-boss-vet')?.value || '3')[0]) || 3;
      const mhp   = parseInt(g('wiz-boss-maxhp')?.value) || 0;
      const wp    = (_wizWpData || []).find(w => w.name === wpNm);
      if (!wp || !owner) return;
      const team  = 'team' + owner;
      const names = WIZ_TEMPLATES.boss._names();
      for (let i = 0; i < names.length; i++) {
        await api('/place/object', { template: tpl, wx: wp.wx + i * 130, wy: wp.wy, angle: 180, team });
        await api('/object/set_prop', { key: 'name', value: names[i] });
        await api('/object/set_prop', { key: 'veterancy', value: vet });
        if (mhp) await api('/object/set_prop', { key: 'maxHP', value: mhp });
      }
    },
    buildScript() {
      const g = id => document.getElementById(id);
      const player  = g('wiz-boss-player')?.value || '';
      const path    = g('wiz-boss-path')?.value || '';
      const caption = (g('wiz-boss-caption')?.value || 'BOSS WAVE!').trim();
      const trigger = g('wiz-boss-trigger')?.value || 'team_dead';
      const trigTm  = g('wiz-boss-trigteam')?.value || '';
      const loseA   = (g('wiz-boss-lose')?.value || '').trim();
      const base    = (g('wiz-boss-name')?.value || 'Boss').trim();
      const names   = WIZ_TEMPLATES.boss._names();
      if (!player) return [];
      const GRP = '[AOD] Boss';
      const scripts = [];
      // bevries tot de trigger (alleen zinvol als de boss later verschijnt)
      if (trigger !== 'start') {
        scripts.push(buildFlatScript(player, GRP, `${base}_hold`,
          [{type: 3, params: []}],
          names.map(nm => ({type: 225, params: [{pt: 'UNIT', v: nm}, {pt: 'BOOLEAN', v: '1'}]})), true));
      }
      // activatie: caption + per boss vrijlaten + aggressive + pad volgen
      const cond = trigger === 'team_dead' && trigTm
        ? [{type: 8, params: [{pt: 'TEAM', v: trigTm}]}]
        : [{type: 3, params: []}];
      const acts = [{type: 139, params: [{pt: 'TEXT_STRING', v: caption}, {pt: 'INT', v: '6000'}]}];
      names.forEach(nm => {
        acts.push({type: 85,  params: [{pt: 'UNIT', v: nm}, {pt: 'SIDE', v: player}]});       // draag over → vijandig
        acts.push({type: 225, params: [{pt: 'UNIT', v: nm}, {pt: 'BOOLEAN', v: '0'}]});
        acts.push({type: 45,  params: [{pt: 'UNIT', v: nm}, {pt: 'AI_MOOD', v: '2'}]});
        if (path) acts.push({type: 282, params: [{pt: 'UNIT', v: nm}, {pt: 'WAYPOINT_PATH', v: path}]});
      });
      scripts.push(buildFlatScript(player, GRP, `${base}_activate`, cond, acts, true));
      // lose: boss in de lose-area → defeat
      if (loseA) names.forEach(nm => {
        scripts.push(buildFlatScript(player, '[AOD] End', `lose_${nm}`,
          [{type: 37, params: [{pt: 'UNIT', v: nm}, {pt: 'TRIGGER_AREA', v: loseA}]}],
          [{type: 4, params: []}], false));
      });
      return scripts;
    },
  },

  win: {
    title: 'Victory condition',
    buildFields(f) {
      const teams   = (_sidelistData?.teams   || []).filter(t => !_SYSTEM_TEAM_RE.test(t.name)).map(t => t.name);
      const players = (_sidelistData?.players || []).map(p => p.name);
      f.innerHTML = ''
        + wizRow('Condition', `<select class="wb-form-input" id="wiz-win-cond" onchange="wizWinCondChange()">`
            + `<option value="team">Team is destroyed</option>`
            + `<option value="player">Player is wiped out</option>`
            + `</select>`)
        + `<div id="wiz-win-team-row">${wizRow('Team', `<div style="display:flex;gap:4px"><select class="wb-form-input" id="wiz-win-team" style="flex:1">${teams.map(t => `<option value="${t}">${t}</option>`).join('')}</select><button class="action-btn" onclick="openNewTeamFromWizard()" title="New team" style="padding:2px 8px;font-size:14px;line-height:1;flex:none">+</button></div>`)}</div>`
        + `<div id="wiz-win-player-row" style="display:none">${wizRow('Player', `<select class="wb-form-input" id="wiz-win-side">${players.map(p => `<option value="${p}">${p}</option>`).join('')}</select>`)}</div>`
        + `<div class="hint-text" id="wiz-win-hint" style="margin-top:4px">Triggers VICTORY when the selected team is destroyed</div>`;
    },
    buildScript() {
      const cond   = document.getElementById('wiz-win-cond')?.value || 'team';
      const owner  = (_sidelistData?.players || []).find(p => p.name === 'SkirmishGLA')?.name
                   || _sidelistData?.players?.[0]?.name || 'SkirmishGLA';
      if (cond === 'team') {
        const team = document.getElementById('wiz-win-team')?.value || '';
        return [buildFlatScript(owner, '[Quick] Victory', `Victory_on_${team}_destroyed`,
          [{type:8, params:[{pt:'TEAM',v:team}]}],
          [{type:3, params:[]}],
          true)];
      } else {
        const side = document.getElementById('wiz-win-side')?.value || '';
        return [buildFlatScript(owner, '[Quick] Victory', `Victory_on_${side}_wiped`,
          [{type:5, params:[{pt:'SIDE',v:side}]}],
          [{type:3, params:[]}],
          true)];
      }
    },
  },

  lose: {
    title: 'Defeat condition',
    buildFields(f) {
      const players   = (_sidelistData?.players || []).map(p => p.name);
      const enemyOpts = players.map(p => `<option value="${p}"${p === 'SkirmishGLA' ? ' selected' : ''}>${p}</option>`).join('');
      const allyOpts  = players.map(p => `<option value="${p}"${p !== 'SkirmishGLA' ? '' : ''}>${p}</option>`).join('');
      f.innerHTML = ''
        + wizRow('Condition', `<select class="wb-form-input" id="wiz-lose-cond" onchange="wizLoseCondChange()">`
            + `<option value="area">Enemy enters area</option>`
            + `<option value="wiped">Player wiped out</option>`
            + `</select>`)
        + `<div id="wiz-lose-area-row">`
            + wizRow('Enemy', `<select class="wb-form-input" id="wiz-lose-enemy">${enemyOpts}</select>`)
            + wizAreaInlineRow('wiz-lose-area', 'Defeat area', 'LoseZone')
        + `</div>`
        + `<div id="wiz-lose-wiped-row" style="display:none">`
            + wizRow('Player', `<select class="wb-form-input" id="wiz-lose-side">${allyOpts}</select>`)
        + `</div>`
        + `<div class="hint-text" id="wiz-lose-hint" style="margin-top:4px">Triggers DEFEAT when the enemy player has units inside the area</div>`;
      _wizLoadAreaSelect('wiz-lose-area');
    },
    buildScript() {
      const cond  = document.getElementById('wiz-lose-cond')?.value || 'area';
      const owner = (_sidelistData?.players || []).find(p => p.name === 'SkirmishGLA')?.name
                  || _sidelistData?.players?.[0]?.name || 'SkirmishGLA';
      if (cond === 'area') {
        const enemy = document.getElementById('wiz-lose-enemy')?.value || 'SkirmishGLA';
        const area  = document.getElementById('wiz-lose-area')?.value || 'LoseZone';
        return [buildFlatScript(owner, '[Quick] Defeat', `Defeat_${enemy}_in_${area}`,
          [{type:96, params:[{pt:'SIDE',v:enemy},{pt:'TRIGGER_AREA',v:area}]}],
          [{type:4,  params:[]}],
          false)];
      } else {
        const side = document.getElementById('wiz-lose-side')?.value || '';
        return [buildFlatScript(owner, '[Quick] Defeat', `Defeat_on_${side}_wiped`,
          [{type:5, params:[{pt:'SIDE',v:side}]}],
          [{type:4, params:[]}],
          true)];
      }
    },
  },

  money: {
    title: 'Give Money',
    buildFields(f) {
      f.innerHTML = ''
        + wizPlayerRow('wiz-money-player')
        + wizRow('Amount ($)', `<input class="wb-form-input wb-num" id="wiz-money-amount" type="number" value="2000" min="0" step="500" style="flex:1">`)
        + `<div class="hint-text" style="margin-top:4px">Gives the player money at scenario start (one-shot)</div>`;
    },
    buildScript() {
      const player = document.getElementById('wiz-money-player')?.value || '';
      const amount = parseInt(document.getElementById('wiz-money-amount')?.value) || 2000;
      return [buildFlatScript(player, '[Quick] Effects', 'GiveMoney',
        [{type:3, params:[]}],
        [{type:155, params:[{pt:'SIDE',v:player},{pt:'INT',v:String(amount)}]}],
        true)];
    },
  },

  message: {
    title: 'Show Message',
    buildFields(f) {
      f.innerHTML = ''
        + wizRow('Message key', `<input class="wb-form-input" id="wiz-msg-key" placeholder="GUI:Objective1" style="flex:1">`)
        + wizRow('Duration (s)', `<input class="wb-form-input wb-num" id="wiz-msg-dur" type="number" value="10" min="1" step="1" style="flex:1">`)
        + wizRow('When', `<select class="wb-form-input" id="wiz-msg-when" onchange="wizMsgWhenChange()">
            <option value="start">Game start</option>
            <option value="delay">After delay</option>
          </select>`)
        + `<div id="wiz-msg-delay-row" style="display:none">` + wizRow('Delay (s)', `<input class="wb-form-input wb-num" id="wiz-msg-delay" type="number" value="20" min="1" step="1" style="flex:1">`) + `</div>`
        + `<div class="hint-text" style="margin-top:4px">Key must exist in the map's string table (e.g. GUI:Objective1).</div>`;
    },
    buildScript() {
      const owner = (_sidelistData?.players || []).find(p => p.name === 'SkirmishGLA')?.name
                  || _sidelistData?.players?.[0]?.name || 'SkirmishGLA';
      const key   = document.getElementById('wiz-msg-key')?.value || 'GUI:Objective1';
      const secs  = parseFloat(document.getElementById('wiz-msg-dur')?.value) || 10;
      const ms    = Math.round(secs * 1000);
      const when  = document.getElementById('wiz-msg-when')?.value || 'start';
      const safeName = key.replace(/[^a-zA-Z0-9]/g,'_');
      if (when === 'delay') {
        const delay     = parseFloat(document.getElementById('wiz-msg-delay')?.value) || 20;
        const timerName = `msg_timer_${safeName}`;
        const s1 = buildFlatScript(owner, '[Quick] Effects', `ShowMessage_${safeName}_timer`,
          [{type:3,  params:[]}],
          [{type:20, params:[{pt:'COUNTER',v:timerName},{pt:'REAL',v:String(delay)}]}],
          true);
        const s2 = buildFlatScript(owner, '[Quick] Effects', `ShowMessage_${safeName}`,
          [{type:4,   params:[{pt:'COUNTER',v:timerName}]}],
          [{type:139, params:[{pt:'TEXT_STRING',v:key},{pt:'INT',v:String(ms)}]}],
          true);
        return [s1, s2];
      }
      return [buildFlatScript(owner, '[Quick] Effects', `ShowMessage_${safeName}`,
        [{type:3,   params:[]}],
        [{type:139, params:[{pt:'TEXT_STRING',v:key},{pt:'INT',v:String(ms)}]}],
        true)];
    },
  },

  camera: {
    title: 'Cinematic',
    buildFields(f) {
      f.innerHTML = ''
        + wizPlayerRow('wiz-cam-player')
        + wizWpInlineRow('wiz-cam-path', 'Path start waypoint', [], 'CameraPath')
        + wizRow('Duration (s)', `<input class="wb-form-input wb-num" id="wiz-cam-dur" type="number" value="5" step="0.5" style="flex:1">`)
        + wizRow('Message key', `<input class="wb-form-input" id="wiz-cam-msg" placeholder="GUI:CinematicText1 (optional)" style="flex:1">`)
        + `<div class="hint-text" style="margin-top:4px">Moves camera along waypoint path at scenario start</div>`;
      apiGet('/map/waypoints').then(r => {
        if (r?.waypoints) {
          _wizWpData = r.waypoints;
          const sel = document.getElementById('wiz-cam-path');
          if (sel) {
            sel.innerHTML = '<option value="">-- none --</option>' +
              r.waypoints.map(w => `<option value="${w.name}">${w.name}</option>`).join('');
          }
        }
      });
    },
    buildScript() {
      const player = document.getElementById('wiz-cam-player')?.value || '';
      const path   = document.getElementById('wiz-cam-path')?.value || 'CameraPath_1';
      const dur    = parseFloat(document.getElementById('wiz-cam-dur')?.value) || 5;
      const msg    = (document.getElementById('wiz-cam-msg')?.value || '').trim();
      const group  = `[Quick] Cinematic`;
      const acts   = [{type:17, params:[{pt:'WAYPOINT',v:path},{pt:'REAL',v:String(dur)},{pt:'REAL',v:'0.5'},{pt:'REAL',v:'0.5'},{pt:'REAL',v:'0.5'}]}];
      if (msg) acts.push({type:139, params:[{pt:'TEXT_STRING',v:msg},{pt:'INT',v:String(Math.round(dur*1000))}]});
      return [buildFlatScript(player, group, `Camera_${path}`,
        [{type:3,params:[]}], acts, true)];
    },
  },

  timer: {
    title: 'Timer',
    buildFields(f) {
      f.innerHTML = ''
        + wizPlayerRow('wiz-t-player')
        + wizRow('Timer name', `<input class="wb-form-input" id="wiz-t-name" value="my_timer" style="flex:1">`)
        + wizRow('Delay (min)', `<input class="wb-form-input wb-num" id="wiz-t-min" type="number" value="5" min="0.5" step="0.5" style="flex:1">`)
        + wizRow('Repeat', `<select class="wb-form-input" id="wiz-t-repeat" style="flex:1">
            <option value="0">No (one-shot)</option>
            <option value="1">Yes (loop)</option>
          </select>`)
        + wizRow('Then enable script', wizScriptInput('wiz-t-script', 'ScriptName (optional)'))
        + `<div class="hint-text" style="margin-top:4px">Starts timer on map load, enables script when it fires</div>`;
    },
    buildScript() {
      const player = document.getElementById('wiz-t-player')?.value || '';
      const name   = document.getElementById('wiz-t-name')?.value || 'my_timer';
      const mins   = parseFloat(document.getElementById('wiz-t-min')?.value) || 5;
      const secs   = mins * 60;
      const repeat = document.getElementById('wiz-t-repeat')?.value === '1';
      const target = document.getElementById('wiz-t-script')?.value || '';
      const group  = `[Quick] Timer ${name}`;
      const fireActs = target ? [{type:8,params:[{pt:'SCRIPT',v:target}]}] : [{type:5,params:[]}];
      if (repeat) fireActs.push({type:20,params:[{pt:'TIMER_NAME',v:name},{pt:'REAL',v:String(secs)}]});
      const s1 = buildFlatScript(player, group, `${name}_start`,
        [{type:3,params:[]}],
        [{type:20,params:[{pt:'TIMER_NAME',v:name},{pt:'REAL',v:String(secs)}]}], true);
      const s2 = buildFlatScript(player, group, `${name}_fire`,
        [{type:4,params:[{pt:'TIMER_NAME',v:name}]}],
        fireActs, !repeat);
      return [s1, s2];
    },
  },
};

// Build the flat-key body object for sidelist_script_set
function buildFlatScript(player, group, name, conditions, actionsTrue, oneShot) {
  const body = { player, group, name, active: true, easy: true, normal: true, hard: true, oneShot, subroutine: false };
  body.condCount = conditions.length;
  conditions.forEach((c, ci) => {
    body[`cond${ci}_type`]   = c.type;
    body[`cond${ci}_pCount`] = c.params.length;
    c.params.forEach((p, pi) => {
      body[`cond${ci}_p${pi}_pt`] = p.pt;
      body[`cond${ci}_p${pi}_v`]  = p.v;
    });
  });
  body.actCount = actionsTrue.length;
  actionsTrue.forEach((a, ai) => {
    body[`act${ai}_type`]   = a.type;
    body[`act${ai}_pCount`] = a.params.length;
    a.params.forEach((p, pi) => {
      body[`act${ai}_p${pi}_pt`] = p.pt;
      body[`act${ai}_p${pi}_v`]  = p.v;
    });
  });
  body.actFalseCount = 0;
  return body;
}

function wizMsgWhenChange() {
  const isDelay = document.getElementById('wiz-msg-when')?.value === 'delay';
  const row = document.getElementById('wiz-msg-delay-row');
  if (row) row.style.display = isDelay ? '' : 'none';
}

function wizLoseCondChange() {
  const isArea    = (document.getElementById('wiz-lose-cond')?.value || 'area') === 'area';
  const areaRow   = document.getElementById('wiz-lose-area-row');
  const wipedRow  = document.getElementById('wiz-lose-wiped-row');
  const hint      = document.getElementById('wiz-lose-hint');
  if (areaRow)  areaRow.style.display  = isArea ? '' : 'none';
  if (wipedRow) wipedRow.style.display = isArea ? 'none' : '';
  if (hint) hint.textContent = isArea
    ? 'Triggers DEFEAT when the enemy player has units inside the area'
    : 'Triggers DEFEAT when all units and buildings of the player are gone';
}

function wizWinCondChange() {
  const isTeam = (document.getElementById('wiz-win-cond')?.value || 'team') === 'team';
  const teamRow   = document.getElementById('wiz-win-team-row');
  const playerRow = document.getElementById('wiz-win-player-row');
  const hint      = document.getElementById('wiz-win-hint');
  if (teamRow)   teamRow.style.display   = isTeam ? '' : 'none';
  if (playerRow) playerRow.style.display = isTeam ? 'none' : '';
  if (hint) hint.textContent = isTeam
    ? 'Triggers VICTORY when the selected team is destroyed'
    : 'Triggers VICTORY when all units and buildings of the player are gone';
}


function openWizForm(templateKey) {
  _wizTemplate = templateKey;
  const tpl = WIZ_TEMPLATES[templateKey];
  if (!tpl) return;
  document.getElementById('sl-wiz-title').textContent = tpl.title;
  document.getElementById('sl-wiz-form').style.display = '';
  tpl.buildFields(document.getElementById('sl-wiz-fields'));
  showFeedback('sl-wiz-feedback', '', true);
}

async function applyWizard(templateKey) {
  const tpl = WIZ_TEMPLATES[templateKey];
  if (!tpl) return;

  // Delete old scripts when re-editing (waves handles its own cleanup in beforeCreate)
  if (_editingInstanceId && templateKey !== 'waves') {
    const existing = _wizInstances.find(i => i.id === _editingInstanceId);
    for (const ref of (existing?.scriptRefs || [])) {
      await api('/sidelist/script/delete', { player: ref.player, group: ref.group, name: ref.name });
    }
  }

  if (tpl.beforeCreate) await tpl.beforeCreate();

  const scripts = tpl.buildScript();
  if (!scripts.length) { showFeedback('sl-wiz-feedback', 'Select a team and goal first', false); return; }
  if (scripts.some(s => !s.player)) { showFeedback('sl-wiz-feedback', 'Select a player first', false); return; }

  let allOk = true;
  for (const body of scripts) {
    const r = await api('/sidelist/script', body);
    if (!r?.ok) { allOk = false; showFeedback('sl-wiz-feedback', r?.error || 'Failed', false); break; }
  }
  if (allOk) {
    if (tpl.afterCreate) tpl.afterCreate(scripts);

    // Snapshot form state (waves wizard: use _waveSeqState which afterCreate just updated)
    const state = templateKey === 'waves'
      ? JSON.parse(JSON.stringify(_waveSeqState || {}))
      : _wizSnapState();
    const scriptRefs = scripts.map(s => ({ player: s.player, group: s.group, name: s.name }));

    if (_editingInstanceId) {
      const idx = _wizInstances.findIndex(i => i.id === _editingInstanceId);
      if (idx !== -1) { _wizInstances[idx].state = state; _wizInstances[idx].scriptRefs = scriptRefs; }
      _editingInstanceId = null;
    } else {
      _wizInstances.push({ id: `inst_${++_instanceCounter}`, key: templateKey, state, scriptRefs });
    }

    renderWizInstances();
    showFeedback('sl-wiz-feedback', `Created ${scripts.length} script(s)`, true);
    fetchSideList();
    setTimeout(() => { document.getElementById('sl-wiz-form').style.display = 'none'; }, 1200);
  }
}

