// ── Panel config ──────────────────────────────────────────────────────────────
const PANEL_CONFIG = {
  mapsetup:  { icon: '🗺', title: 'Map Setup',                 panel: 'panel-mapsetup',  hasPanel: true },
  shapefill: { icon: '⬛', title: 'Terrain & Texture Painter', panel: 'panel-shapefill', hasPanel: true },
  terrain:   { icon: '⛰', title: 'Height Brush',              panel: 'panel-terrain',   hasPanel: true },
  texture:   { icon: '🎨', title: 'Texture Painter',           panel: 'panel-texture',   hasPanel: true  },
  objects:   { icon: '🏗', title: 'Place Objects',             panel: 'panel-objects',   hasPanel: true },
  nature:    { icon: '🌿', title: 'Plant Trees & Vegetation',  panel: 'panel-nature',    hasPanel: true },
  waypoints: { icon: '📍', title: 'Waypoints',                 panel: 'panel-waypoints', hasPanel: true },
  roads:     { icon: '🛣', title: 'Roads & Bridges',           panel: 'panel-roads',     hasPanel: true },
  setup:     { icon: '👥', title: 'Players & Teams',           panel: null,              hasPanel: false },
  sidelist:  { icon: '📜', title: 'Script Wizard',             panel: 'panel-sidelist',  hasPanel: true },
  lighting:  { icon: '💡', title: 'Lighting',                  panel: 'panel-lighting',  hasPanel: true },
  ai:        { icon: '🤖', title: 'AI Agent Control',          panel: 'panel-ai',        hasPanel: true },
};

let _currentTool = 'shapefill';

function switchPanel(toolName) {
  const cfg = PANEL_CONFIG[toolName] || PANEL_CONFIG['shapefill'];
  _currentTool = toolName;

  // Update header
  document.getElementById('panel-icon').textContent  = cfg.icon;
  document.getElementById('panel-title').textContent = cfg.title;

  // Show/hide panels
  for (const [, c] of Object.entries(PANEL_CONFIG)) {
    if (c.panel) document.getElementById(c.panel).style.display = 'none';
  }
  if (cfg.panel) document.getElementById(cfg.panel).style.display = '';

  // Side panel open/close
  document.getElementById('side-panel').classList.toggle('open', cfg.hasPanel);

  // Re-evaluate passthrough when tool changes (mouse may be stationary over viewport)
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);

  // Impassable overlay follows the texture panel's paint mode (auto-on/off)
  if (typeof _texSyncImpassableOverlay === 'function') _texSyncImpassableOverlay();

  // Object-properties polling runs on every tab EXCEPT shapefill — that tab has
  // its own selection UI (shape corners/coords). Panel shows whenever WB has a
  // selection (objects, waypoints, bridges), regardless of the active tool.
  if (toolName === 'shapefill') stopPropsPolling();
  else                          startPropsPolling();

  // Start panel-specific polling
  if (toolName === 'mapsetup') {
    stopSfPolling(); stopWpPolling(); stopSlPolling(); stopRdPolling();
    startMsPolling();
  } else if (toolName === 'shapefill') {
    stopMsPolling();
    startSfPolling();
    stopWpPolling();
    stopSlPolling();
    stopRdPolling();
  } else if (toolName === 'waypoints') {
    stopMsPolling();
    stopSfPolling();
    startWpPolling();
    stopSlPolling();
    stopRdPolling();
    refreshMapInfo();
  } else if (toolName === 'objects') {
    stopMsPolling();
    stopSfPolling();
    stopWpPolling();
    stopSlPolling();
    stopRdPolling();
    refreshMapInfo();
  } else if (toolName === 'setup') {
    stopMsPolling();
    stopSfPolling();
    stopWpPolling();
    startSlPolling();
    stopRdPolling();
  } else if (toolName === 'sidelist') {
    stopMsPolling();
    stopSfPolling();
    stopWpPolling();
    startSlPolling();
    stopRdPolling();
  } else if (toolName === 'roads') {
    stopMsPolling();
    stopSfPolling();
    stopWpPolling();
    stopSlPolling();
    rdInitPanel();
    startRdPolling();
  } else if (toolName === 'lighting') {
    stopMsPolling();
    stopSfPolling();
    stopWpPolling();
    stopSlPolling();
    stopRdPolling();
    refreshLightingPanel();
  } else if (toolName === 'ai') {
    stopMsPolling();
    stopSfPolling();
    stopWpPolling();
    stopSlPolling();
    stopRdPolling();
    initAiPanel();
  } else {
    stopMsPolling();
    stopSfPolling();
    stopWpPolling();
    stopSlPolling();
    stopRdPolling();
  }
}

// ── Toolbar button clicks ─────────────────────────────────────────────────────
document.querySelectorAll('.tool-btn').forEach(btn => {
  btn.addEventListener('click', async () => {
    const tool = btn.dataset.tool;
    // Setup opens full-screen modal; sidelist now uses side panel
    if (tool === 'setup') { openSetupModal(); return; }
    document.querySelectorAll('.tool-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    // Clicking the ROADS icon in the main nav always means "show me Roads" - reset
    // here, not in rdInitPanel, so the scripts-modal "jump to this specific bridge"
    // path (which sets _rdKind='bridge' then calls switchPanel('roads') directly,
    // bypassing this click handler) still works. Sub-tab clicks *within* the panel
    // still remember themselves via rdSetTab, this only resets the main-nav entry point.
    if (tool === 'roads') _rdKind = 'road';
    switchPanel(tool);

    if (tool === 'terrain') {
      const activeTerrainBtn = document.querySelector('.terrain-tool-btn.active');
      const tmode = activeTerrainBtn ? activeTerrainBtn.dataset.tmode : 'raise';
      await api('/terrain/mode', { mode: tmode });
      // Sync the remembered height into WB (its own default differs per tool)
      if (tmode !== 'smooth' && typeof _terHeightCfg !== 'undefined') {
        api('/terrain/set', { height: _terHeightCfg[_terHeightCat(tmode)].val });
      }
      document.getElementById('status-tool').textContent = `Terrain · ${tmode}`;
    } else if (tool === 'roads' || tool === 'ai') {
      // Roads: rdInitPanel activates WB's native road tool after loading types.
      // AI: pure info panel, there is no native WB tool to activate.
      document.getElementById('status-tool').textContent = `Tool: ${tool}`;
    } else {
      const result = await api('/tool', { tool });
      document.getElementById('status-tool').textContent =
        result.ok ? `Tool: ${tool}` : `Tool: ${tool}`;
    }

    if (tool === 'shapefill' && currentView === '2d') {
      const activeMode = document.querySelector('.mode-btn.active');
      if (activeMode) await sendShapeFillMode(activeMode.dataset.mode);
    }
  });
});

// ── ShapeFill mode helpers ────────────────────────────────────────────────────
const PANEL_MODE_TO_API = {
  Rect: 'rect', Circle: 'circle', Poly: 'polygon',
  Line: 'line', Select: 'select', Edit: 'edit', Fill: 'fill',
};

async function sendShapeFillMode(dataMode) {
  const apiMode = PANEL_MODE_TO_API[dataMode];
  if (apiMode) await api('/shapefill/mode', { mode: apiMode });
}

async function activateShapeFill() {
  await api('/tool', { tool: 'shapefill' });
  const activeMode = document.querySelector('.mode-btn.active');
  if (activeMode) await sendShapeFillMode(activeMode.dataset.mode);
}

// ── Mode buttons ──
document.querySelectorAll('.mode-btn').forEach(btn => {
  btn.addEventListener('click', async () => {
    document.querySelectorAll('.mode-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('status-tool').textContent =
      `ShapeFill · ${btn.dataset.mode} mode`;
    await sendShapeFillMode(btn.dataset.mode);
    await syncShapeFillState();
  });
});

// ── Sliders ──
function bindSlider(sliderId, valId, apiProp) {
  const slider = document.getElementById(sliderId);
  const val    = document.getElementById(valId);
  slider.addEventListener('input',  () => { val.textContent = slider.value; });
  slider.addEventListener('change', () => api('/shapefill/set', { [apiProp]: Number(slider.value) }));
}
bindSlider('sl-inner-height', 'val-inner-height', 'innerHeight');
bindSlider('sl-border-width', 'val-border-width', 'borderWidth');

// ── Ground level indicator ────────────────────────────────────────────────────
// Shows the map's base height and water level next to the Inner Height slider,
// so it's obvious whether the current value raises a hill, digs a pit, or goes
// under water (sea). On a map change the slider defaults to ground level.
let _sfGround    = null;
let _sfWater     = null;
let _sfGroundKey = null;  // filePath|WxH — detects map switches

async function refreshSfGround() {
  const [r, w, info] = await Promise.all([
    apiGet('/map/ground_height'),
    apiGet('/water/state'),
    apiGet('/map/info'),
  ]);
  if (w?.ok && typeof w.height === 'number') _sfWater = w.height;
  if (r?.ok && typeof r.ground === 'number') {
    _sfGround = r.ground;
    const hint = document.getElementById('sf-ground-hint');
    if (hint) hint.textContent = `Ground level: ${_sfGround} · Water: ${_sfWater ?? '—'} (click → set to ground)`;

    // New/other map since last visit? → default Inner Height to ground level
    const key = info?.ok ? `${info.filePath}|${info.width}x${info.height}` : null;
    if (key && key !== _sfGroundKey) {
      _sfGroundKey = key;
      const sl = document.getElementById('sl-inner-height');
      sl.value = _sfGround;  // clamps to slider range automatically
      document.getElementById('val-inner-height').textContent = sl.value;
      api('/shapefill/set', { innerHeight: Number(sl.value) });
    }
  }
  updateSfHeightDelta();
}

function updateSfHeightDelta() {
  const el = document.getElementById('sf-height-delta');
  if (!el) return;
  if (_sfGround == null) { el.textContent = ''; return; }
  const v = Number(document.getElementById('sl-inner-height').value);
  const d = v - _sfGround;
  if (_sfWater != null && v <= _sfWater) {
    el.textContent = `≈ ${v} is at/below water level (${_sfWater}) — this becomes sea`;
    el.style.color = '#3fd0d4';
  }
  else if (d > 0) { el.textContent = `▲ +${d} above ground — raises a hill`; el.style.color = '#e8a838'; }
  else if (d < 0) { el.textContent = `▼ ${d} below ground — digs a pit`;     el.style.color = '#5aaeff'; }
  else            { el.textContent = `= at ground level — flat`;             el.style.color = '#5cb85c'; }
}

document.getElementById('sl-inner-height').addEventListener('input', updateSfHeightDelta);
document.getElementById('sf-ground-hint').addEventListener('click', () => {
  if (_sfGround == null) return;
  const sl = document.getElementById('sl-inner-height');
  sl.value = _sfGround;
  document.getElementById('val-inner-height').textContent = sl.value;
  api('/shapefill/set', { innerHeight: Number(sl.value) });
  updateSfHeightDelta();
});

// ── Blend groups ─────────────────────────────────────────────────────────────
function wireBlendGroup(groupId, autoBlendProp, blendInwardProp) {
  const grp = document.getElementById(groupId);
  grp.querySelectorAll('.seg-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const val = btn.dataset.val;
      grp.querySelectorAll('.seg-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      const payload = {};
      payload[autoBlendProp]  = val !== 'none';
      payload[blendInwardProp] = val === 'in';
      api('/shapefill/set', payload);
    });
  });
}
wireBlendGroup('blend-border', 'autoBlend',      'blendInward');
wireBlendGroup('blend-fill',   'fillAutoBlend',  'fillBlendInward');
wireBlendGroup('blend-inner',  'innerAutoBlend', 'innerBlendInward');

function setBlendGroup(groupId, autoBlend, blendInward) {
  const val = !autoBlend ? 'none' : blendInward ? 'in' : 'out';
  const grp = document.getElementById(groupId);
  grp.querySelectorAll('.seg-btn').forEach(b => {
    b.classList.toggle('active', b.dataset.val === val);
  });
}

// ── Auto Save checkbox ──
document.getElementById('chk-auto-save').addEventListener('change', function() {
  api('/shapefill/set', { autoSave: this.checked });
});

// ── Extra Options (blends + session persistence) — collapsed by default ──────
function _sfSetExtraOpen(open) {
  document.getElementById('sf-extra-box').style.display = open ? '' : 'none';
  document.getElementById('sf-extra-toggle').textContent = open ? 'Extra Options ▴' : 'Extra Options ▾';
  localStorage.setItem('sf_extra_open', open ? '1' : '0');
}
document.getElementById('sf-extra-toggle').addEventListener('click', () => {
  _sfSetExtraOpen(document.getElementById('sf-extra-box').style.display === 'none');
});
_sfSetExtraOpen(localStorage.getItem('sf_extra_open') === '1');

// ── Texture picker ────────────────────────────────────────────────────────────
let _textures  = null;
let _texSlot   = null;
let _texCurIdx = { inner: -1, border: -1 };

async function loadTextures() {
  if (_textures) return _textures;
  const r = await apiGet('/texture/list');
  _textures = (r && r.ok && r.textures) ? r.textures : [];
  return _textures;
}

function openTexPicker(slot) {
  _texSlot = slot;
  const titles = { inner: 'Fill Texture', border: 'Border Texture',
                   'tex-fg': 'Foreground Texture', 'tex-bg': 'Background Texture' };
  document.getElementById('tex-picker-title').textContent = titles[slot] || 'Select Texture';
  document.getElementById('tex-picker-search').value = '';
  document.getElementById('tex-picker').classList.remove('hidden');
  renderTexGrid('');
  setTimeout(() => document.getElementById('tex-picker-search').focus(), 50);
}

function closeTexPicker() {
  document.getElementById('tex-picker').classList.add('hidden');
  _texSlot = null;
}

function renderTexGrid(query) {
  const grid   = document.getElementById('tex-picker-grid');
  const curIdx = _texCurIdx[_texSlot] ?? -1;
  const q       = query.toLowerCase().trim();
  const list    = (_textures || []).filter(t =>
    (!q || (t.uiName || t.name || '').toLowerCase().includes(q))
  );

  grid.innerHTML = '';

  if (!list.length) {
    const empty = document.createElement('div');
    empty.className = 'tex-picker-empty';
    empty.textContent = q ? `No results for "${query}"` : 'No textures found';
    grid.appendChild(empty);
    return;
  }

  for (const tex of list) {
    const el = document.createElement('div');
    el.className = 'tex-item' + (tex.index === curIdx ? ' selected' : '');

    if (tex.imgUrl) {
      const img = document.createElement('img');
      img.src     = tex.imgUrl;
      img.alt     = tex.uiName || tex.name;
      img.loading = 'lazy';
      el.appendChild(img);
    } else {
      const ph = document.createElement('div');
      ph.className = 'tex-item-no-img';
      ph.textContent = '🏔';
      el.appendChild(ph);
    }

    const lbl = document.createElement('span');
    lbl.textContent = tex.uiName || tex.name;
    el.appendChild(lbl);
    el.addEventListener('click', () => pickTex(tex));
    grid.appendChild(el);
  }

  const sel = grid.querySelector('.tex-item.selected');
  if (sel) sel.scrollIntoView({ block: 'center' });
}

async function pickTex(tex) {
  if (!_texSlot) return;
  if (_texSlot === 'tex-fg' || _texSlot === 'tex-bg') {
    const prop = _texSlot === 'tex-fg' ? 'fgClass' : 'bgClass';
    await api('/texture/set', { [prop]: tex.index });
    updateTexturePanelSwatch(_texSlot, tex.imgUrl, tex.uiName || tex.name);
    closeTexPicker();
    return;
  }
  const prop = _texSlot === 'inner' ? 'innerTexClass' : 'borderTexClass';
  await api('/shapefill/set', { [prop]: tex.index });
  _texCurIdx[_texSlot] = tex.index;
  updateSwatch(_texSlot, tex.imgUrl, tex.uiName || tex.name);
  closeTexPicker();
  await syncShapeFillState();
}

function updateTexturePanelSwatch(slot, imgUrl, label) {
  const swatchId = slot === 'tex-fg' ? 'tex-swatch-fg' : 'tex-swatch-bg';
  const labelId  = slot === 'tex-fg' ? 'tex-fg-name'   : 'tex-bg-name';
  const swatch   = document.getElementById(swatchId);
  if (swatch) {
    if (imgUrl) { swatch.style.backgroundImage = `url('${imgUrl}')`; swatch.style.backgroundColor = ''; }
    else        { swatch.style.backgroundImage = ''; swatch.style.backgroundColor = '#444'; }
  }
  const lbl = document.getElementById(labelId);
  if (lbl && label) lbl.textContent = label;
}

function updateSwatch(slot, imgUrl, label) {
  const swatchId = slot === 'inner' ? 'swatch-inner' : 'swatch-border';
  const labelId  = slot === 'inner' ? 'label-inner-tex' : 'label-border-tex';
  const swatch   = document.getElementById(swatchId);
  if (imgUrl) {
    swatch.style.backgroundImage = `url('${imgUrl}')`;
    swatch.style.backgroundColor = '';
  } else {
    swatch.style.backgroundImage = '';
    swatch.style.backgroundColor = '#444';
  }
  if (label) document.getElementById(labelId).textContent = label;
}

document.getElementById('btn-inner-tex').addEventListener('click', async () => {
  await loadTextures();
  openTexPicker('inner');
});
document.getElementById('btn-border-tex').addEventListener('click', async () => {
  await loadTextures();
  openTexPicker('border');
});

document.getElementById('tex-picker-search').addEventListener('input', function() {
  renderTexGrid(this.value);
});

document.getElementById('tex-picker-close').addEventListener('click', closeTexPicker);
document.addEventListener('keydown', e => {
  if (e.key === 'Escape' && _texSlot) closeTexPicker();
});
document.getElementById('tex-picker').addEventListener('click', e => {
  if (e.target.id === 'tex-picker') closeTexPicker();
});

// ── ShapeFill action buttons ──
async function sfAction(action) {
  if (action === 'apply') {
    const btn = document.getElementById('btn-apply');
    const orig = btn.textContent;
    btn.textContent = 'Applying…';
    btn.disabled = true;
    try {
      await api('/shapefill/action', { action });
      await syncShapeFillState();
    } finally {
      btn.textContent = orig;
      btn.disabled = false;
    }
    return;
  }
  await api('/shapefill/action', { action });
  await syncShapeFillState();
}
document.getElementById('btn-apply').addEventListener('click',       () => sfAction('apply'));
document.getElementById('btn-duplicate').addEventListener('click',   () => sfAction('duplicate'));
document.getElementById('btn-flip-h').addEventListener('click',      () => sfAction('flip_h'));
document.getElementById('btn-flip-v').addEventListener('click',      () => sfAction('flip_v'));
document.getElementById('btn-delete').addEventListener('click',      () => sfAction('delete'));
document.getElementById('btn-finish-poly').addEventListener('click', () => sfAction('finish_poly'));
document.getElementById('btn-finish-line').addEventListener('click', () => sfAction('finish_line'));
document.getElementById('btn-clear-lines').addEventListener('click', () => sfAction('clear_lines'));
document.getElementById('btn-rotate').addEventListener('click',      () => sfAction('rotate'));

// ── ShapeFill state sync ──────────────────────────────────────────────────────
const TYPE_ICON = { rect: '▪', circle: '●', polygon: '⬡', line: '╱' };

async function syncShapeFillState() {
  const s = await apiGet('/shapefill/state');
  if (!s || !s.ok) return;

  const slH  = document.getElementById('sl-inner-height');
  const slBW = document.getElementById('sl-border-width');
  if (slH  && document.activeElement !== slH)  { slH.value  = s.innerHeight; document.getElementById('val-inner-height').textContent = s.innerHeight; updateSfHeightDelta(); }
  if (slBW && document.activeElement !== slBW) { slBW.value = s.borderWidth; document.getElementById('val-border-width').textContent = s.borderWidth; }

  if (s.innerTexClass  != null) _texCurIdx.inner  = s.innerTexClass;
  if (s.borderTexClass != null) _texCurIdx.border = s.borderTexClass;
  if (_textures) {
    const iT = _textures.find(t => t.index === s.innerTexClass);
    const bT = _textures.find(t => t.index === s.borderTexClass);
    if (iT) updateSwatch('inner',  iT.imgUrl, iT.uiName || iT.name);
    if (bT) updateSwatch('border', bT.imgUrl, bT.uiName || bT.name);
  } else {
    document.getElementById('label-inner-tex').textContent  = s.innerTexName  || '(none)';
    document.getElementById('label-border-tex').textContent = s.borderTexName || '(same as inner)';
  }

  // Blend groups — shape blends hidden in Fill mode, fill blend only in Fill mode
  const isFillMode = s.mode === 'fill';
  document.getElementById('blend-shape-section').style.display = isFillMode ? 'none' : '';
  document.getElementById('blend-fill-section').style.display  = isFillMode ? '' : 'none';

  setBlendGroup('blend-border', s.autoBlend,      s.blendInward);
  setBlendGroup('blend-fill',   s.fillAutoBlend,  s.fillBlendInward);
  setBlendGroup('blend-inner',  s.innerAutoBlend, s.innerBlendInward);

  // Auto Save
  const chkAS = document.getElementById('chk-auto-save');
  if (chkAS && document.activeElement !== chkAS) chkAS.checked = !!s.autoSave;

  // Contextual draw buttons
  document.getElementById('ctx-poly-btns').style.display = s.isDrawingPoly ? '' : 'none';
  document.getElementById('ctx-line-btns').style.display = s.isDrawingLine ? '' : 'none';

  // Sync mode buttons to reported mode from WB
  if (s.mode) {
    const modeMap = { rect:'Rect', circle:'Circle', polygon:'Poly', line:'Line',
                      select:'Select', edit:'Edit', fill:'Fill' };
    const wbMode = modeMap[s.mode];
    if (wbMode) {
      document.querySelectorAll('.mode-btn').forEach(b =>
        b.classList.toggle('active', b.dataset.mode === wbMode));
    }
  }

  const list   = document.getElementById('shapes-list');
  const badge  = document.getElementById('shapes-count');
  const shapes = s.shapes || [];
  badge.textContent = shapes.length;
  // Global status bar (bottom of window) — was a hardcoded "Shapes: 3" placeholder that
  // never reflected the actual map, now mirrors this same live count.
  const statusShapes = document.getElementById('status-shapes');
  if (statusShapes) statusShapes.textContent = `Shapes: ${shapes.length}`;

  const selId  = s.selectedId;
  const listKey = shapes.map(sh => `${sh.id}:${sh.id === selId ? 1 : 0}`).join(',');
  if (list.dataset.key !== listKey) {
    list.dataset.key = listKey;
    list.innerHTML = '';
    for (const sh of shapes) {
      const el = document.createElement('div');
      el.className = 'shape-item' + (sh.id === selId ? ' active' : '');
      el.textContent = `${TYPE_ICON[sh.type] || '▪'} ${sh.name}`;
      el.addEventListener('click', async () => {
        await api('/shapefill/select', { id: sh.id });
        await api('/shapefill/mode', { mode: 'select' });
        document.querySelectorAll('.mode-btn').forEach(b => b.classList.toggle('active', b.dataset.mode === 'Select'));
        await syncShapeFillState();
      });
      list.appendChild(el);
    }
  }

  // ── Lines-lijst (parallel aan shapes): selecteer op id (kind:line) en verwijder met Delete ──
  const lines    = s.lines || [];
  const lSection = document.getElementById('lines-section');
  const lList    = document.getElementById('lines-list');
  const lBadge   = document.getElementById('lines-count');
  lSection.style.display = lines.length ? '' : 'none';
  lBadge.textContent = lines.length;
  const selLineId = s.selectedLineId;
  const lKey = lines.map(l => `${l.id}:${l.id === selLineId ? 1 : 0}`).join(',');
  if (lList.dataset.key !== lKey) {
    lList.dataset.key = lKey;
    lList.innerHTML = '';
    lines.forEach((l, i) => {
      const el = document.createElement('div');
      el.className = 'shape-item' + (l.id === selLineId ? ' active' : '');
      el.textContent = `╱ Line #${l.id} (${l.points} pts)`;
      el.addEventListener('click', async () => {
        await api('/shapefill/select', { id: l.id, kind: 'line' });
        await api('/shapefill/mode', { mode: 'select' });
        document.querySelectorAll('.mode-btn').forEach(b => b.classList.toggle('active', b.dataset.mode === 'Select'));
        await syncShapeFillState();
      });
      lList.appendChild(el);
    });
  }
}

let _sfPollTimer = null;
function startSfPolling() {
  stopSfPolling();
  syncShapeFillState();
  refreshSfGround(); // cached server-side per map — cheap on re-entry
  _sfPollTimer = setInterval(syncShapeFillState, 3000);
}
function stopSfPolling() {
  if (_sfPollTimer) { clearInterval(_sfPollTimer); _sfPollTimer = null; }
}

// ── Help button ──
document.getElementById('panel-help-btn').addEventListener('click', () => {
  if (_currentTool === 'shapefill') {
    const { shell } = require('electron');
    shell.openExternal('https://github.com/TheSuperHackers/GeneralsGameCode/blob/feature/shapefill-tool/worldbuilder-cpp/ShapeFillTool/USAGE.md');
  }
});

// ── View mode state ───────────────────────────────────────────────────────────
let currentView = '3d';

function setViewMode(mode) {
  currentView = mode;
  const is3D = mode === '3d';
  document.getElementById('view-2d').classList.toggle('active', !is3D);
  document.getElementById('view-3d').classList.toggle('active', is3D);
  document.getElementById('view-3d-notice').classList.toggle('visible', is3D);
  document.getElementById('panel-content').style.display = is3D ? 'none' : '';
}

document.getElementById('view-2d').addEventListener('click', async () => {
  setViewMode('2d');
  await api('/view/mode', { mode: '2d' });
  await activateShapeFill();
});
document.getElementById('view-3d').addEventListener('click', async () => {
  setViewMode('3d');
  await api('/view/mode', { mode: '3d' });
});

document.getElementById('btn-switch-2d').addEventListener('click', async () => {
  setViewMode('2d');
  await api('/view/mode', { mode: '2d' });
  await activateShapeFill();
});

// ── Title bar ──
document.getElementById('btn-undo').addEventListener('click', () => api('/map/undo', {}));
document.getElementById('btn-redo').addEventListener('click', () => api('/map/redo', {}));
document.getElementById('btn-save').addEventListener('click', async () => {
  const info = await api('/map/info');
  if (info?.filePath) {
    await api('/map/save', {});
    await refreshMapInfo();
  } else {
    openSaveAsModal();
  }
});
document.getElementById('btn-new').addEventListener('click', async () => {
  const r = await api('/map/new', {});
  if (r?.ok) autoAddSkirmishPlayers();
});
document.getElementById('btn-open').addEventListener('click', () => openLoadModal());
document.getElementById('btn-saveas').addEventListener('click', () => openSaveAsModal());

