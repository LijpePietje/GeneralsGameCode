// ── Objects panel ─────────────────────────────────────────────────────────────
let _objectPalette    = null;
let _selectedTemplate = null;
const ICON_BASE = 'http://127.0.0.1:8099/icons/';
const ICON_PREFIXES = ['AirF_','Lazr_','SupW_','Infa_','Nuke_','Tank_','Demo_','Slth_','Chem_','Boss_','CINE_','Amb_','GC_'];

function getFriendlyName(template) {
  let s = template;
  // Strip general prefix
  for (const p of ICON_PREFIXES) {
    if (s.startsWith(p)) { s = s.slice(p.length); break; }
  }
  // Strip faction
  for (const f of ['America','China','GLA','Tech','Civilian','Generic']) {
    if (s.startsWith(f) && s.length > f.length) { s = s.slice(f.length); break; }
  }
  // Strip leading unit-category word (Infantry, Vehicle, Jet, Tank)
  for (const c of ['Infantry','Vehicle','Jet','Tank']) {
    if (s.startsWith(c) && s.length > c.length) { s = s.slice(c.length); break; }
  }
  if (!s) return template;
  // Split CamelCase into words
  return s.replace(/([A-Z][a-z])/g, ' $1').replace(/([a-z])([A-Z])/g, '$1 $2').trim() || template;
}

function makeObjIcon(tmpl, cls) {
  const img = document.createElement('img');
  img.className = cls;
  img.alt = '';
  img.src = ICON_BASE + tmpl + '.png';
  img.onerror = () => {
    // Try parent faction version: strip general prefix (e.g. AirF_AmericaAirfield → AmericaAirfield)
    for (const p of ICON_PREFIXES) {
      if (tmpl.startsWith(p)) {
        const parent = tmpl.slice(p.length);
        img.onerror = () => {
          img.onerror = () => replaceWithFallback(img, cls);
          img.src = ICON_BASE + 'topdown_' + tmpl + '.png';
        };
        img.src = ICON_BASE + parent + '.png';
        return;
      }
    }
    // No prefix → try topdown directly
    img.onerror = () => replaceWithFallback(img, cls);
    img.src = ICON_BASE + 'topdown_' + tmpl + '.png';
  };
  return img;
}
function replaceWithFallback(img, cls) {
  const fb = document.createElement('div');
  fb.className = cls === 'obj-icon-img' ? 'obj-icon-fallback' : 'op-item-fallback';
  fb.textContent = '🏗';
  img.replaceWith(fb);
}

async function loadObjectPalette() {
  if (_objectPalette) return _objectPalette;
  const r = await apiGet('/objects/palette');
  _objectPalette = (r && r.ok) ? r.palette : {};
  return _objectPalette;
}

// ── Nature / Vegetation panel ─────────────────────────────────────────────────
let _natureTypes     = null;
let _selectedNature  = null;

async function loadNatureTypes() {
  if (_natureTypes) return _natureTypes;
  const r = await apiGet('/vegetation/types');
  _natureTypes = (r && r.ok) ? r.types : [];
  return _natureTypes;
}

function buildNatureGrid(types, query) {
  const grid = document.getElementById('nature-grid');
  if (!grid) return;
  grid.innerHTML = '';
  const q = (query || '').toLowerCase().trim();
  const filtered = q ? types.filter(t => t.name.toLowerCase().includes(q)) : types;
  if (filtered.length === 0) {
    grid.innerHTML = '<div style="padding:12px;font-size:11px;color:var(--text-hint);text-align:center">No results</div>';
    return;
  }
  for (const t of filtered) {
    const el = document.createElement('div');
    el.className = 'nature-item' + (t.name === _selectedNature ? ' active' : '');
    el.dataset.name = t.name;
    el.title = t.name;
    const img = document.createElement('img');
    img.className = 'obj-icon-img';
    img.src = t.icon;
    img.alt = '';
    img.onerror = () => {
      const fb = document.createElement('div');
      fb.className = 'obj-icon-fallback';
      fb.textContent = '🌿';
      img.replaceWith(fb);
    };
    const lbl = document.createElement('span');
    lbl.className = 'obj-icon-label';
    lbl.textContent = t.name;
    el.appendChild(img);
    el.appendChild(lbl);
    el.addEventListener('click', () => selectNatureType(t.name));
    grid.appendChild(el);
  }
}

function selectNatureType(name) {
  _selectedNature = name;
  const disp = document.getElementById('nature-selected-display');
  if (disp) { disp.textContent = name || '(nothing selected)'; disp.classList.toggle('has-value', !!name); }
  document.querySelectorAll('.nature-item').forEach(el => {
    el.classList.toggle('active', el.dataset.name === name);
  });
}

// ── Grove brush state ─────────────────────────────────────────────────────────
let _groveDragStart = null;   // {x,y} screen pixels where drag began

function groveRadius() { return parseInt(document.getElementById('grove-radius')?.value || 80); }
function groveDensity() { return parseInt(document.getElementById('grove-density')?.value || 5) / 100.0; }

function estimateTrees(radiusPx) {
  // Convert screen pixels to world units (rough: 1 tile ≈ 10 world units, viewport is full screen)
  // We send world-unit radius to C++; density val slider 1-20 maps to 0.01-0.20
  const r = groveRadius(); // world units from slider
  const d = groveDensity();
  return Math.max(1, Math.round(Math.PI * r * r * d / 400));
}

function updateGroveValLabels() {
  const r = groveRadius();
  document.getElementById('grove-radius-val').textContent = r;
  document.getElementById('grove-density-val').textContent = '~' + estimateTrees(r);
}

function drawGroveCircle(sx, sy, radiusPx) {
  const canvas = document.getElementById('grove-canvas');
  if (!canvas) return;
  canvas.width  = window.innerWidth;
  canvas.height = window.innerHeight;
  canvas.style.display = 'block';
  const ctx = canvas.getContext('2d');
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  // Filled circle
  ctx.beginPath();
  ctx.arc(sx, sy, radiusPx, 0, 2 * Math.PI);
  ctx.fillStyle   = 'rgba(80,180,80,0.12)';
  ctx.fill();
  ctx.strokeStyle = 'rgba(80,200,80,0.7)';
  ctx.lineWidth   = 1.5;
  ctx.stroke();
  // Tree count label
  const n = estimateTrees(radiusPx);
  ctx.font = 'bold 12px monospace';
  ctx.fillStyle = 'rgba(80,220,80,0.9)';
  ctx.fillText(`~${n} trees`, sx + radiusPx + 6, sy + 4);
}

function clearGroveCanvas() {
  const canvas = document.getElementById('grove-canvas');
  if (!canvas) return;
  canvas.style.display = 'none';
  const ctx = canvas.getContext('2d');
  ctx.clearRect(0, 0, canvas.width, canvas.height);
}

// Convert screen-pixel radius to world-unit radius using the WB view scale
// We use the grove-radius slider value (world units) directly — no screen conversion needed.
// The circle overlay is drawn at 1px = 1 world unit visually, which is approximate.

async function initNaturePanel() {
  const types = await loadNatureTypes();
  buildNatureGrid(types, '');

  const searchEl = document.getElementById('nature-search');
  if (searchEl) searchEl.addEventListener('input', () => buildNatureGrid(types, searchEl.value));

  // Slider live feedback
  document.getElementById('grove-radius')?.addEventListener('input', updateGroveValLabels);
  document.getElementById('grove-density')?.addEventListener('input', updateGroveValLabels);
  updateGroveValLabels();
}

function getFactionIcon(faction) {
  if (['USA','Air Force','Laser','Superweapon'].includes(faction)) return '🦅';
  if (['China','Infantry','Nuclear','Tank'].includes(faction))     return '🐉';
  if (['GLA','Demo','Stealth','Toxin'].includes(faction))          return '☠';
  if (faction === 'Boss')      return '💀';
  if (faction === 'Special')   return '🕵';
  if (faction === 'Cutscene')  return '🎬';
  if (faction === 'Civilian')  return '🏘';
  return '🌿'; // Props
}

// Flatten all items from palette, optionally filtered by faction + query + unitOnly
function getFilteredItems(palette, faction, query, unitOnly = false) {
  const q = (query || '').toLowerCase().trim();
  const items = [];
  for (const [fac, sections] of Object.entries(palette)) {
    if (faction) {
      if (fac !== faction) continue;
    } else {
      if (fac === 'Cutscene') continue; // hidden unless explicitly selected
    }
    if (unitOnly && UNIT_PICKER_FAC_EXCLUDE.has(fac)) continue;
    for (const tmplList of Object.values(sections)) {
      for (const tmpl of tmplList) {
        if (unitOnly && !isUnitTemplate(tmpl, fac)) continue;
        if (!q || tmpl.toLowerCase().includes(q) || getFriendlyName(tmpl).toLowerCase().includes(q)) items.push({ tmpl, fac });
      }
    }
  }
  return items;
}

// ── Sidebar: recently used + quick-search results ─────────────────────────────
// The sidebar no longer tries to be a full catalog (that's the picker modal);
// it shows the user's recent picks, or live results while the search box has text.
const OBJ_RECENT_KEY = 'obj_recent_templates';
const OBJ_RECENT_MAX = 16;

function _objRecents() {
  try { return JSON.parse(localStorage.getItem(OBJ_RECENT_KEY)) || []; }
  catch { return []; }
}

function _objAddRecent(tmpl) {
  const list = [tmpl, ..._objRecents().filter(t => t !== tmpl)].slice(0, OBJ_RECENT_MAX);
  localStorage.setItem(OBJ_RECENT_KEY, JSON.stringify(list));
}

function renderObjSidebar(query) {
  const grid  = document.getElementById('obj-grid');
  const label = document.getElementById('obj-grid-label');
  if (!grid || !_objectPalette) return;
  const q = (query || '').trim();

  let items;
  if (q) {
    if (label) label.textContent = 'RESULTS';
    items = getFilteredItems(_objectPalette, null, q).slice(0, 60);
  } else {
    if (label) label.textContent = 'RECENTLY USED';
    // Drop recents that no longer exist in the palette (e.g. dead-object filter)
    const byTmpl = new Map(getFilteredItems(_objectPalette, null, '').map(i => [i.tmpl, i.fac]));
    items = _objRecents().filter(t => byTmpl.has(t)).map(t => ({ tmpl: t, fac: byTmpl.get(t) }));
  }

  grid.innerHTML = '';
  if (items.length === 0) {
    grid.innerHTML = `<div style="padding:12px;font-size:11px;color:var(--text-hint);text-align:center">${
      q ? 'No objects found' : 'Nothing used yet — Browse All Objects or search above'}</div>`;
    return;
  }
  for (const { tmpl, fac } of items) {
    const el = document.createElement('div');
    el.className = 'obj-icon-item' + (tmpl === _selectedTemplate ? ' active' : '');
    el.dataset.template = tmpl;
    el.title = tmpl;
    el.appendChild(makeObjIcon(tmpl, 'obj-icon-img'));
    const lbl = document.createElement('span');
    lbl.className = 'obj-icon-label';
    lbl.textContent = getFriendlyName(tmpl);
    el.appendChild(lbl);
    const facEl = document.createElement('span');
    facEl.className = 'obj-icon-fac';
    facEl.textContent = getFactionIcon(fac) + ' ' + fac;
    el.appendChild(facEl);
    el.addEventListener('click', () => selectObjectTemplate(tmpl));
    grid.appendChild(el);
  }
}

const FACTION_GROUPS = [
  ['USA','Air Force','Laser','Superweapon'],
  ['China','Infantry','Nuclear','Tank'],
  ['GLA','Demo','Stealth','Toxin'],
  ['Boss','Special','Props','Civilian','Cutscene'],
];

function selectObjectTemplate(tmpl) {
  _selectedTemplate = tmpl;
  const search = document.getElementById('obj-search');
  if (tmpl) {
    _objAddRecent(tmpl);
    // Refresh the recents list unless the user is mid-search
    if (!search || !search.value.trim()) renderObjSidebar('');
  } else if (search && search.value) {
    // Deselect (after placing, or right-click) → reset search back to recents
    search.value = '';
    renderObjSidebar('');
  }
  const disp = document.getElementById('obj-selected-display');
  if (disp) {
    disp.textContent = tmpl || '(nothing selected)';
    disp.classList.toggle('has-value', !!tmpl);
  }
  document.querySelectorAll('.obj-icon-item, .op-item').forEach(el => {
    el.classList.toggle('active', el.dataset.template === tmpl);
  });
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
  // Close obj picker modal if open (normal object-place mode only)
  if (!_objPickerCallback) closeObjPicker();
}

// ── Object picker modal (full-screen visual) ──────────────────────────────────
let _objPickerFaction  = null;
let _objPickerCallback = null; // set when picker is opened for unit-slot selection
let _objPickerUnitOnly = false;

// Factions that contain no combat units (props, decorations, cutscenes)
const UNIT_PICKER_FAC_EXCLUDE = new Set(['Props', 'Civilian', 'Cutscene']);
// Building-like name patterns — excluded when unit-only filter is active
const UNIT_BUILDING_RE = /Building|Barracks|CommandCenter|SupplyCenter|SupplyWarehouse|Airfield|Palace|Tunnel(?:Network)?|Bunker|PowerPlant|(?:Toxin|Tech|Gla|America|China)Factory|BlackMarket|ArmsDealer|StingerSite|ScudStorm|NukeAttack|ParticleCannon|SignalFire|DetentionCamp|FloodLight|SentryDroneSpawn|CombatCycle(?=Cage)|^GLAWall|^AmericaWall|^ChinaWall/i;

function isUnitTemplate(tmpl, fac) {
  if (UNIT_PICKER_FAC_EXCLUDE.has(fac)) return false;
  return !UNIT_BUILDING_RE.test(tmpl);
}

function openObjPicker(palette, opts = {}) {
  _objPickerCallback = opts.callback || null;
  _objPickerUnitOnly = opts.unitOnly || false;
  _objPickerFaction  = null;
  ipcRenderer.send('viewport-mouse', false); // picker covers full screen — no passthrough
  const modal = document.getElementById('obj-picker');
  modal.classList.remove('hidden');
  document.getElementById('obj-picker-title').textContent = _objPickerUnitOnly ? 'Select Unit' : 'Place Object';
  document.getElementById('obj-picker-search').placeholder = _objPickerUnitOnly ? 'Search units...' : 'Search...';
  document.getElementById('obj-picker-search').value = '';
  buildObjPickerFactions(palette);
  buildObjPickerGrid(palette, null, '');
  setTimeout(() => document.getElementById('obj-picker-search').focus(), 50);
}

function closeObjPicker() {
  document.getElementById('obj-picker').classList.add('hidden');
  _objPickerCallback = null;
  _objPickerUnitOnly = false;
}

function buildObjPickerFactions(palette) {
  const bar = document.getElementById('obj-picker-factions');
  bar.innerHTML = '';
  const all = document.createElement('button');
  all.className = 'op-faction-chip' + (!_objPickerFaction ? ' active' : '');
  all.textContent = 'All';
  all.addEventListener('click', () => { _objPickerFaction = null; refreshObjPicker(palette); });
  bar.appendChild(all);

  for (const group of FACTION_GROUPS) {
    for (const fac of group) {
      if (!palette[fac]) continue;
      if (_objPickerUnitOnly && UNIT_PICKER_FAC_EXCLUDE.has(fac)) continue;
      const chip = document.createElement('button');
      chip.className = 'op-faction-chip' + (_objPickerFaction === fac ? ' active' : '') + (fac === 'Cutscene' ? ' cutscene' : '');
      chip.textContent = getFactionIcon(fac) + ' ' + fac;
      chip.addEventListener('click', () => { _objPickerFaction = fac; refreshObjPicker(palette); });
      bar.appendChild(chip);
    }
    const br = document.createElement('div');
    br.style.cssText = 'width:100%;height:0;margin:0';
    bar.appendChild(br);
  }
}

function buildObjPickerGrid(palette, faction, query) {
  const grid = document.getElementById('obj-picker-grid');
  grid.innerHTML = '';
  const items = getFilteredItems(palette, faction, query, _objPickerUnitOnly);
  for (const { tmpl, fac } of items) {
    const el = document.createElement('div');
    el.className = 'op-item' + (tmpl === _selectedTemplate ? ' active' : '');
    el.dataset.template = tmpl;
    el.title = tmpl;
    el.appendChild(makeObjIcon(tmpl, 'op-item-img'));
    const lbl = document.createElement('span');
    lbl.textContent = getFriendlyName(tmpl);
    el.appendChild(lbl);
    const facEl = document.createElement('span');
    facEl.className = 'op-item-fac';
    facEl.textContent = getFactionIcon(fac) + ' ' + fac;
    el.appendChild(facEl);
    el.addEventListener('click', () => {
      if (_objPickerCallback) {
        _objPickerCallback(tmpl);
        closeObjPicker();
      } else {
        selectObjectTemplate(tmpl);
      }
    });
    grid.appendChild(el);
  }
}

function refreshObjPicker(palette) {
  buildObjPickerFactions(palette);
  buildObjPickerGrid(palette, _objPickerFaction, document.getElementById('obj-picker-search').value);
}

async function initObjectsPanel() {
  const palette = await loadObjectPalette();
  renderObjSidebar('');

  document.getElementById('obj-search').addEventListener('input', function() {
    renderObjSidebar(this.value);
  });

  document.getElementById('btn-obj-expand').addEventListener('click', () => openObjPicker(palette));

  // Obj picker modal events
  document.getElementById('obj-picker-close').addEventListener('click', closeObjPicker);
  document.getElementById('obj-picker-search').addEventListener('input', function() {
    buildObjPickerGrid(palette, _objPickerFaction, this.value);
  });
  document.getElementById('obj-picker').addEventListener('click', e => {
    if (e.target.id === 'obj-picker') closeObjPicker();
  });
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape' && !document.getElementById('obj-picker').classList.contains('hidden')) {
      closeObjPicker();
    }
  });
}

