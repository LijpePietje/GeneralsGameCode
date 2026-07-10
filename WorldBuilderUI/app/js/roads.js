// ── Roads & Bridges panel ─────────────────────────────────────────────────────
let _rdKind     = 'road'; // 'road' | 'bridge'
let _rdAllTypes = [];     // cached road types from INI
let _rdRoads    = [];     // cached road list [{x1,y1,x2,y2,type}]
let _rdBridges  = [];     // cached bridge list [{x1,y1,x2,y2,type,name}]
let _rdRoadsPrevCount   = 0;
let _rdBridgesPrevCount = 0;
const _rdAutoNamed = new Set(); // tracks "x1,y1" of bridges we auto-named this session
let _rdPollTimer = null;
let _lastRdPropsKey = null; // "kind:x1,y1" — skip full rebuild if same item
function startRdPolling() {
  stopRdPolling();
  rdRefreshList(_rdKind); // immediate first load
  _rdPollTimer = setInterval(() => rdRefreshList(_rdKind), 3000);
}
function stopRdPolling() {
  if (_rdPollTimer) { clearInterval(_rdPollTimer); _rdPollTimer = null; }
}

function rdSetTab(tab) {
  // tab is 'roads' or 'bridges' (from HTML onclick)
  const isRoad = (tab === 'roads');
  document.getElementById('rd-tab-roads').classList.toggle('active', isRoad);
  document.getElementById('rd-tab-bridges').classList.toggle('active', !isRoad);
  document.getElementById('rd-roads-tab').style.display   = isRoad  ? '' : 'none';
  document.getElementById('rd-bridges-tab').style.display = isRoad  ? 'none' : '';
  const newKind = isRoad ? 'road' : 'bridge';
  // Clear selection and props when switching between road/bridge tabs
  if (_rdKind !== newKind) {
    _rdSelected = { kind: null, x1: null, y1: null, data: null };
    _lastRdPropsKey = null;
    document.getElementById('obj-props-panel')?.classList.add('hidden');
  }
  _rdKind = newKind;
  rdApplyToWB(true);
  rdRefreshList(_rdKind);
}

// ── Type pickers (visual, like the object picker) ─────────────────────────────
// Road types from Roads.ini are split into three categories; bridges into
// sectional (drag between banks, Bridge entries) and landmark (object, 1 click).
const RD_RAILS_RE    = /^TrainTrack/i;
const RD_MARKINGS_RE = /^(Arrow|Word|Caution|CircleCross|Cracks$|Cross$|CrossWalk|DashedLine|DryCracks|Helipad|NoSymbol|Park|SingleWhiteLine|StreetHoleCover|ThickLine)/i;

// Landmark bridges are Object templates (verified in zh_object_names.txt),
// placed via the generic quick-place flow (click = place, drag = rotate).
const RD_LANDMARKS = [
  { key: 'lm_european',  template: 'EuropeanLandmarkBridge',  label: 'European Landmark' },
  { key: 'lm_tsingma',   template: 'TsingMaLandmarkBridge',   label: 'Tsing Ma Landmark' },
  { key: 'lm_monument',  template: 'MonumentTrainBridge',     label: 'Monument Train Bridge' },
  { key: 'lm_monumenth', template: 'MonumentTrainBridgeHigh', label: 'Monument Train Bridge (high)' },
  { key: 'lm_flood',     template: 'AsianFloodBridge',        label: 'Asian Flood Bridge' },
  { key: 'lm_generic',   template: 'GenericBridge',           label: 'Generic Bridge' },
];
// Bridge entries in Roads.ini that are NOT drag-bridges (landmark art / effects)
const RD_SECTIONAL_EXCLUDE = new Set([
  ...RD_LANDMARKS.map(d => d.template), 'SpecialEffectsTrainCrashObject',
]);

let _rdBridgeTypes = [];
let _rdPickerCat   = null;     // 'roads' | 'rails' | 'markings' | 'sectional' | 'landmark'
let _rdLastRoadCat = 'roads';  // Current-button on the roads tab reopens this category

const RD_PICKER_TITLES = {
  roads: 'Pick Road', rails: 'Pick Rail Track', markings: 'Pick Marking',
  sectional: 'Pick Sectional Bridge', landmark: 'Pick Landmark Bridge (object)',
};

function _rdPickerItems(cat) {
  if (cat === 'roads')     return _rdAllTypes.filter(t => !RD_RAILS_RE.test(t) && !RD_MARKINGS_RE.test(t)).map(t => ({ value: t, label: t }));
  if (cat === 'rails')     return _rdAllTypes.filter(t => RD_RAILS_RE.test(t)).map(t => ({ value: t, label: t }));
  if (cat === 'markings')  return _rdAllTypes.filter(t => RD_MARKINGS_RE.test(t)).map(t => ({ value: t, label: t }));
  if (cat === 'sectional') return _rdBridgeTypes.filter(t => !RD_SECTIONAL_EXCLUDE.has(t)).map(t => ({ value: t, label: t }));
  if (cat === 'landmark')  return RD_LANDMARKS.map(d => ({ value: d.key, label: d.label, template: d.template }));
  return [];
}

function rdOpenPicker(cat) {
  _rdPickerCat = cat;
  if (cat === 'roads' || cat === 'rails' || cat === 'markings') _rdLastRoadCat = cat;
  document.getElementById('rd-picker-title').textContent = RD_PICKER_TITLES[cat] || 'Pick';
  const search = document.getElementById('rd-picker-search');
  search.value = '';
  ipcRenderer.send('viewport-mouse', false); // full-screen modal — no passthrough
  document.getElementById('rd-picker').classList.remove('hidden');
  rdBuildPickerGrid('');
  setTimeout(() => search.focus(), 50);
}

function rdClosePicker() {
  document.getElementById('rd-picker').classList.add('hidden');
  _rdPickerCat = null;
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

function rdBuildPickerGrid(query) {
  const grid = document.getElementById('rd-picker-grid');
  const q = (query || '').toLowerCase().trim();
  const cat = _rdPickerCat;
  const current = cat === 'sectional' ? document.getElementById('rd-bridge-type').value
                : cat === 'landmark'  ? null
                : document.getElementById('rd-road-type').value;
  grid.innerHTML = '';
  const items = _rdPickerItems(cat).filter(it => !q || it.label.toLowerCase().includes(q) || it.value.toLowerCase().includes(q));
  if (!items.length) {
    grid.innerHTML = '<div style="padding:12px;font-size:11px;color:var(--text-hint)">No results</div>';
    return;
  }
  for (const it of items) {
    const el = document.createElement('div');
    el.className = 'rd-pick-item' + (it.value === current ? ' active' : '');
    el.title = it.template || it.value;
    if (cat === 'landmark') {
      el.appendChild(makeObjIcon(it.template, 'rd-pick-img')); // object icon (with fallback)
    } else {
      const img = document.createElement('img');
      img.className = 'rd-pick-img';
      img.src = `${API_BASE}/roadtex/${encodeURIComponent(it.value)}.png`;
      img.onerror = () => {
        const fb = document.createElement('div');
        fb.className = 'rd-pick-fallback';
        fb.textContent = cat === 'rails' ? '🛤' : cat === 'markings' ? '⛖' : '🛣';
        img.replaceWith(fb);
      };
      el.appendChild(img);
    }
    const lbl = document.createElement('span');
    lbl.className = 'rd-pick-label';
    lbl.textContent = it.label;
    el.appendChild(lbl);
    el.addEventListener('click', () => rdPick(it));
    grid.appendChild(el);
  }
}

function rdPick(it) {
  const cat = _rdPickerCat;
  rdClosePicker();
  if (cat === 'landmark') {
    // Object quick-place: click on map = place, drag = rotate, right-click = stop
    document.getElementById('rd-bridge-label').textContent = it.label;
    document.getElementById('rd-bridge-swatch').style.backgroundImage = '';
    msStartQuickPlace(it.value);
    return;
  }
  const isBridge = (cat === 'sectional');
  const inputId  = isBridge ? 'rd-bridge-type' : 'rd-road-type';
  document.getElementById(inputId).value = it.value;
  document.getElementById(isBridge ? 'rd-bridge-label' : 'rd-road-label').textContent = it.label;
  document.getElementById(isBridge ? 'rd-bridge-swatch' : 'rd-road-swatch').style.backgroundImage =
    `url(${API_BASE}/roadtex/${encodeURIComponent(it.value)}.png)`;
  if (typeof msCancelQuickPlace === 'function' && _msQpActive) msCancelQuickPlace();
  rdApplyToWB(true);
}

// Send current type+corner to WB's RoadOptions, then activate the native road tool via existing pipe
async function rdApplyToWB(activate) {
  const isBridge = (_rdKind === 'bridge');
  const name   = isBridge
    ? (document.getElementById('rd-bridge-type')?.value.trim() || '')
    : (document.getElementById('rd-road-type')?.value || '');
  const corner = document.getElementById('rd-corner')?.value || '0';
  const angled = corner === '1' ? 1 : 0;
  const tight  = corner === '2' ? 1 : 0;
  // Set road type + corner in WB's RoadOptions (no activate here)
  await api('/road/settool', { name, angled, tight, bridge: isBridge ? 1 : 0, activate: 0 });
  // Activate WB's native road tool via the existing 'tool' pipe command
  if (activate) {
    await api('/tool', { tool: 'road' });
  }
}

let _rdSelected = { kind: null, x1: null, y1: null, data: null };

function renderRdProps(kind, it) {
  const panel = document.getElementById('obj-props-panel');
  const body  = document.getElementById('obj-props-body');
  const title = document.getElementById('obj-props-title');
  if (!panel || !body) return;
  if (!it) { panel.classList.add('hidden'); _lastRdPropsKey = null; return; }

  const x1 = Math.round(it.x1 || 0), y1 = Math.round(it.y1 || 0);
  const x2 = Math.round(it.x2 || 0), y2 = Math.round(it.y2 || 0);
  const type = it.type || it.template || '?';
  const propsKey = `road:${x1},${y1}`;

  if (title) title.textContent = 'Road Properties';
  if (_lastRdPropsKey !== propsKey) {
    _lastRdPropsKey = propsKey;
    body.innerHTML = `
      <div class="op-section">
        <div class="op-section-label">GENERAL</div>
        <div class="op-field-row"><span class="op-label">Type</span><span class="op-val">${type}</span></div>
      </div>
      <div class="op-section">
        <div class="op-section-label">POSITION</div>
        <div class="op-field-row"><span class="op-label">Start</span><span class="op-val" id="rd-op-start">${x1},&nbsp;${y1}</span></div>
        <div class="op-field-row"><span class="op-label">End</span><span class="op-val" id="rd-op-end">${x2},&nbsp;${y2}</span></div>
      </div>`;
  } else {
    const startEl = document.getElementById('rd-op-start');
    const endEl   = document.getElementById('rd-op-end');
    if (startEl) startEl.textContent = `${x1}, ${y1}`;
    if (endEl)   endEl.textContent   = `${x2}, ${y2}`;
  }

  panel.classList.remove('hidden');
}


// Find nearest road/bridge endpoint to world coords wx, wy
function rdFindNearest(kind, wx, wy) {
  const list = kind === 'road' ? _rdRoads : _rdBridges;
  let best = null, bestDist = Infinity;
  for (const it of list) {
    for (const [ex, ey] of [[it.x1, it.y1], [it.x2, it.y2]]) {
      const d = Math.hypot(wx - ex, wy - ey);
      if (d < bestDist) { bestDist = d; best = it; }
    }
  }
  return bestDist < 600 ? best : null; // 600 world units (~60 tiles)
}

function rdBridgeAsProps(it) {
  return { ok: true, type: 'bridge',
    templateName: it.type || it.template || '',
    x1: it.x1, y1: it.y1, x2: it.x2 || it.x1, y2: it.y2 || it.y1,
    name: it.name || '' };
}

async function rdRefreshList(kind) {
  const endpoint = kind === 'road' ? '/api/map/roads' : '/api/map/bridges';
  const containerId = kind === 'road' ? 'rd-road-list' : 'rd-bridge-list';
  const el = document.getElementById(containerId);
  if (!el) return;
  try {
    const resp = await fetch(API_BASE + endpoint);
    const data = await resp.json();
    const items = data.roads || data.bridges || [];
    if (kind === 'road') _rdRoads = items;
    else _rdBridges = items;

    // Auto-assign Bridge IDs to unnamed bridges (once per coordinate per session)
    if (kind === 'bridge') {
      const perType = {};
      for (const it of items) {
        const t = it.type || 'Bridge';
        perType[t] = (perType[t] || 0) + 1;
        if (!it.name) {
          const key = `${Math.round(it.x1)},${Math.round(it.y1)}`;
          if (!_rdAutoNamed.has(key)) {
            it.name = `${t}_${String(perType[t]).padStart(2, '0')}`;
            _rdAutoNamed.add(key);
            api('/bridge/setname', { x1: Math.round(it.x1), y1: Math.round(it.y1), name: it.name });
          }
        }
      }
    }

    if (!items.length) { el.textContent = 'None placed.'; return; }
    el.innerHTML = items.map((it, i) => {
      const label = it.type || it.template || `#${i+1}`;
      const x1 = Math.round(it.x1 || 0), y1 = Math.round(it.y1 || 0);
      const x2 = Math.round(it.x2 || 0), y2 = Math.round(it.y2 || 0);
      const isSelected = _rdSelected.kind === kind && _rdSelected.x1 === x1 && _rdSelected.y1 === y1;
      const selFn = kind === 'road' ? `rdSelectRoad(${x1},${y1})` : `rdSelectBridge(${x1},${y1})`;
      const delFn = kind === 'road' ? `rdDeleteRoad(${x1},${y1})` : `rdDeleteBridge(${x1},${y1})`;
      const bridgeName = it.name || '';
      const isLandmark = !!it.landmark;
      const posLabel = isLandmark ? `(${x1},${y1})` : `(${x1},${y1})→(${x2},${y2})`;
      const landmarkTag = isLandmark ? `<span style="font-size:9px;opacity:0.6;margin-left:3px">landmark</span>` : '';
      const nameTag = (kind === 'bridge')
        ? `<span class="rd-bridge-name${bridgeName ? '' : ' rd-bridge-unnamed'}" onclick="event.stopPropagation();rdRenameBridge(${x1},${y1},'${bridgeName.replace(/'/g,"\'")}')">` +
          `${bridgeName || '+ name'}</span>`
        : '';
      return `<div class="rd-list-item${isSelected ? ' rd-selected' : ''}" data-x1="${x1}" data-y1="${y1}" onclick="${selFn}">` +
             `<span>${label} ${posLabel}</span>${landmarkTag}` +
             nameTag +
             `<button onclick="event.stopPropagation();${delFn}">Del</button></div>`;
    }).join('');
    // Sync data for currently selected item
    if (_rdSelected.kind === kind) {
      const found = items.find(it => Math.round(it.x1) === _rdSelected.x1 && Math.round(it.y1) === _rdSelected.y1);
      if (found) {
        _rdSelected.data = found;
        if (kind === 'bridge') renderObjProps(rdBridgeAsProps(found));
        else                   renderRdProps(kind, found);
      } else {
        // Item moved or deleted — clear stale props
        _rdSelected = { kind: null, x1: null, y1: null, data: null };
        _lastRdPropsKey = null;
        document.getElementById('obj-props-panel')?.classList.add('hidden');
      }
    }
    // Always auto-select newest when count grows — even if something is already selected
    const prevCount = kind === 'road' ? _rdRoadsPrevCount : _rdBridgesPrevCount;
    if (items.length > prevCount) {
      const newest = items[items.length - 1];
      const nx1 = Math.round(newest.x1 || 0), ny1 = Math.round(newest.y1 || 0);
      _rdSelected = { kind, x1: nx1, y1: ny1, data: newest };
      if (kind === 'bridge') renderObjProps(rdBridgeAsProps(newest));
      else                   renderRdProps(kind, newest);
      el.querySelectorAll('.rd-list-item').forEach((li, i) => li.classList.toggle('rd-selected', i === items.length - 1));
    }
    if (kind === 'road') _rdRoadsPrevCount = items.length;
    else _rdBridgesPrevCount = items.length;
  } catch (err) {
    el.textContent = 'Failed to load.';
  }
}

async function rdSelectRoad(x1, y1) {
  const data = _rdRoads.find(it => Math.round(it.x1) === x1 && Math.round(it.y1) === y1) || null;
  _rdSelected = { kind: 'road', x1, y1, data };
  if (data) renderRdProps('road', data);
  rdRefreshList('road');
  await api('/road/select', { x1, y1 });
}

async function rdSelectBridge(x1, y1) {
  const it = _rdBridges.find(b => Math.round(b.x1) === x1 && Math.round(b.y1) === y1) || null;
  _rdSelected = { kind: 'bridge', x1, y1, data: it };
  if (it) {
    // Immediate display from cache using the same renderObjProps path as propsPolling
    renderObjProps({ ok: true, type: 'bridge',
      templateName: it.type || it.template || '',
      x1: it.x1, y1: it.y1, x2: it.x2 || it.x1, y2: it.y2 || it.y1,
      name: it.name || '' });
  }
  await api('/road/select', { x1, y1, bridge: 1 });
  // propsPolling takes over from here (1.5s interval)
}

function rdUpdateStatus(kind, msg) {
  // Show a temporary message in the list area
  const id = kind === 'road' ? 'rd-road-list' : 'rd-bridge-list';
  const el = document.getElementById(id);
  if (!el) return;
  const old = el.innerHTML;
  el.textContent = msg;
  setTimeout(() => { el.innerHTML = old; }, 1200);
}

async function rdDeleteRoad(x1, y1) {
  const listEl = document.getElementById('rd-road-list');
  const item = listEl?.querySelector(`[data-x1="${x1}"][data-y1="${y1}"]`);
  if (item) item.remove();
  if (_rdSelected.kind === 'road' && _rdSelected.x1 === x1 && _rdSelected.y1 === y1) {
    _rdSelected = { kind: null, x1: null, y1: null, data: null };
    _lastRdPropsKey = null;
  }
  const r = await api('/road/delete', { x1, y1 });
  rdUpdateStatus('road', r?.ok ? 'Deleted.' : ('Error: ' + (r?.error || '?')));
  setTimeout(() => rdRefreshList('road'), 400);
}

async function rdDeleteBridge(x1, y1) {
  const listEl = document.getElementById('rd-bridge-list');
  const item = listEl?.querySelector(`[data-x1="${x1}"][data-y1="${y1}"]`);
  if (item) item.remove();
  if (_rdSelected.kind === 'bridge' && _rdSelected.x1 === x1 && _rdSelected.y1 === y1) {
    _rdSelected = { kind: null, x1: null, y1: null, data: null };
    _lastRdPropsKey = null;
    document.getElementById('obj-props-panel')?.classList.add('hidden');
  }
  const r = await api('/road/delete', { x1, y1, bridge: 1 }); // integer 1, not boolean true — JsonGetInt can't parse booleans
  rdUpdateStatus('bridge', r?.ok ? 'Deleted.' : ('Error: ' + (r?.error || '?')));
  setTimeout(() => rdRefreshList('bridge'), 400);
}

async function rdRenameBridge(x1, y1, currentName) {
  const name = prompt('Bridge script name (used in script conditions):', currentName);
  if (name === null) return; // cancelled
  await api('/bridge/setname', { x1, y1, name: name.trim() });
  rdRefreshList('bridge');
}

async function rdInitPanel() {
  // Load road + bridge type names from the game INI (parsed from BIG archives)
  if (!_rdAllTypes.length) {
    try {
      const resp = await fetch(API_BASE + '/api/ini/roads');
      const data = await resp.json();
      _rdAllTypes = data.names || data.roads || [];
    } catch {}
    if (!_rdAllTypes.length) _rdAllTypes = ['TwoLane'];
  }
  if (!_rdBridgeTypes.length) {
    try {
      const resp = await fetch(API_BASE + '/api/ini/bridges');
      const data = await resp.json();
      _rdBridgeTypes = data.names || [];
    } catch {}
  }

  // Default road swatch (TwoLane) — bridges start unpicked
  const swatch = document.getElementById('rd-road-swatch');
  if (swatch && !swatch.style.backgroundImage) {
    swatch.style.backgroundImage = `url(${API_BASE}/roadtex/TwoLane.png)`;
  }

  // Picker modal events (wire once)
  const search = document.getElementById('rd-picker-search');
  if (search && !search.dataset.wired) {
    search.dataset.wired = '1';
    search.addEventListener('input', () => rdBuildPickerGrid(search.value));
    document.getElementById('rd-picker').addEventListener('click', e => {
      if (e.target.id === 'rd-picker') rdClosePicker();
    });
    document.addEventListener('keydown', e => {
      if (e.key === 'Escape' && !document.getElementById('rd-picker').classList.contains('hidden')) rdClosePicker();
    });
  }

  // Remembers whichever tab the user (or an explicit jump, e.g. scripts-modal.js
  // sending you to a specific bridge) last left active - _rdKind defaults to
  // 'road' at module load, so a genuinely fresh session still opens on Roads.
  // rdSetTab compares against 'roads'/'bridges' (plural, matching its HTML onclick
  // callers) but _rdKind holds 'road'/'bridge' (singular) - passing it straight
  // through always failed the 'roads' check and fell through to Bridges, no
  // matter what _rdKind actually was. That was the real bug behind every
  // "Roads always opens on Bridges" report, not anything about remembering state.
  rdSetTab(_rdKind === 'road' ? 'roads' : 'bridges'); // shows current tab + activates WB road/bridge tool
}

