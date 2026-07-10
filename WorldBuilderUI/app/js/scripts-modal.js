// ─────────────────────────────────────────────────────────────────────────────
// SCRIPTS MODAL
// ─────────────────────────────────────────────────────────────────────────────

// ── Condition/Action type lookups — generated from COND_DATA/ACT_DATA (ZH Scripts.h enum) ──
// COND_DATA and ACT_DATA are loaded from _script_data.js before the js/ modules
const COND_TYPES = {};
const ACT_TYPES  = {};
if (typeof COND_DATA !== 'undefined') {
  COND_DATA.forEach(e => { if (e.t >= 0) COND_TYPES[e.t] = e.p[e.p.length - 1]; });
}
if (typeof ACT_DATA !== 'undefined') {
  ACT_DATA.forEach(e => { if (e.t >= 0) ACT_TYPES[e.t] = e.p[e.p.length - 1]; });
}

// COND_CATEGORIES / ACT_CATEGORIES no longer needed — picker uses COND_DATA/ACT_DATA directly
const COND_CATEGORIES = [];
const ACT_CATEGORIES  = [];

// Fast type → iname lookups for sentence rendering
const ACT_INAME  = {};
const COND_INAME = {};
if (typeof ACT_DATA  !== 'undefined') ACT_DATA.forEach(e  => { if (e.t >= 0) ACT_INAME[e.t]  = e.i; });
if (typeof COND_DATA !== 'undefined') COND_DATA.forEach(e => { if (e.t >= 0) COND_INAME[e.t] = e.i; });

// Build a human-readable label with param values filled in.
// Falls back to plain name when no template exists.
function _buildLabel(typeId, params, inameMap, fallbackTypes) {
  const name   = fallbackTypes[typeId] || ('Type ' + typeId);
  const iname  = inameMap[typeId];
  const tmpl   = iname && typeof SCRIPT_PARAMS !== 'undefined' ? SCRIPT_PARAMS[iname] : null;
  if (!tmpl || !tmpl.u || !tmpl.u.length) return name;
  const uStr = tmpl.u;
  let out = '';
  uStr.forEach((s, i) => {
    out += s;
    if (i < (params || []).length) {
      const v = String((params[i]?.value ?? params[i]?.v) ?? '');
      out += v !== '' ? v : '…';
    }
  });
  return out.trim();
}

function condLabel(c) {
  if (!c) return '(empty)';
  return _buildLabel(c.type, c.params, COND_INAME, COND_TYPES);
}
function actLabel(a) {
  if (!a) return '(empty)';
  return _buildLabel(a.type, a.params, ACT_INAME, ACT_TYPES);
}

// ── Param dropdown helpers ────────────────────────────────────────────────────
let _scpWaypointCache = null; // populated async when condition modal opens
let _scpObjectCache   = null; // named map objects, populated async
let _scpTriggerCache  = null; // polygon trigger areas, populated async

const _DROPDOWN_TYPES = new Set([
  'BOOLEAN','TEAM','SIDE','WAYPOINT','WAYPOINT_PATH','SKIRMISH_WAYPOINT_PATH',
  'SCRIPT','SCRIPT_SUBROUTINE','UNIT','TRIGGER_AREA',
  'COMPARISON','RELATION','AI_MOOD','BUILDABLE','SURFACES_ALLOWED',
  'LEFT_OR_RIGHT','FACTION_NAME','SCIENCE_AVAILABILITY',
  'RADAR_EVENT_TYPE','SHAKE_INTENSITY','OBJECT_STATUS','KIND_OF_PARAM',
  'COORD3D','ANGLE','COLOR',
  'BOUNDARY','OBJECT_PANEL_FLAG','BRIDGE',
  'SOUND','DIALOG','MUSIC','SPECIAL_POWER','SCIENCE','UPGRADE'
]);

// ── INI-backed caches (filled from server on first use) ──────────────────────
const _iniLists = {};
async function _fetchIniList(key, endpoint) {
  if (_iniLists[key]) return _iniLists[key];
  try {
    const r = await fetch(`http://127.0.0.1:8099${endpoint}`);
    const j = await r.json();
    _iniLists[key] = (j.names || []);
  } catch { _iniLists[key] = []; }
  return _iniLists[key];
}

// Returns a searchable select pre-filled with a loading placeholder,
// then replaces options once the fetch resolves.
function _makeIniSelect(ptype, idx, endpoint, pval) {
  const key = ptype.toLowerCase();
  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex;flex-direction:column;flex:1;min-width:0;gap:3px';
  const filterEl = document.createElement('input');
  filterEl.type = 'text';
  filterEl.placeholder = 'Loading…';
  filterEl.style.cssText = 'font-size:11px;padding:2px 6px;box-sizing:border-box;width:100%;border:1px solid var(--border,#374151);background:var(--input-bg,#1f2937);color:var(--text,#e5e7eb);border-radius:3px';
  const sel = document.createElement('select');
  sel.className = 'msc-input';
  sel.dataset.ptype = ptype;
  sel.dataset.idx   = String(idx);
  sel.size = 6;
  sel.style.cssText = 'font-size:11px;flex:1;border:1px solid var(--border,#374151);background:var(--input-bg,#1f2937);color:var(--text,#e5e7eb);border-radius:3px';

  function populate(entries, q) {
    const prev = sel.value;
    sel.innerHTML = '';
    const lq = q.toLowerCase();
    entries.forEach(name => {
      if (lq && !name.toLowerCase().includes(lq)) return;
      const opt = document.createElement('option');
      opt.value = name; opt.textContent = name;
      if (name === pval || name === prev) opt.selected = true;
      sel.appendChild(opt);
    });
    if (!sel.value && entries.length) sel.options[0].selected = true;
  }

  // Placeholder until loaded
  const loading = document.createElement('option');
  loading.textContent = 'Loading…';
  sel.appendChild(loading);

  _fetchIniList(key, endpoint).then(names => {
    filterEl.placeholder = 'Filter…';
    populate(names, filterEl.value);
    filterEl.addEventListener('input', () => populate(names, filterEl.value));
  });

  wrap.append(filterEl, sel);
  return wrap;
}

const _OBJECT_STATUS_NAMES = [
  'NONE','DESTROYED','CAN_ATTACK','UNDER_CONSTRUCTION','UNSELECTABLE','NO_COLLISIONS',
  'NO_ATTACK','AIRBORNE_TARGET','PARACHUTING','REPULSOR','HIJACKED','AFLAME','BURNED',
  'WET','IS_FIRING_WEAPON','IS_BRAKING','STEALTHED','DETECTED','CAN_STEALTH','SOLD',
  'UNDERGOING_REPAIR','RECONSTRUCTING','MASKED','IS_ATTACKING','USING_ABILITY',
  'IS_AIMING_WEAPON','NO_ATTACK_FROM_AI','IGNORING_STEALTH','IS_CARBOMB',
  'DECK_HEIGHT_OFFSET','STATUS_RIDER1','STATUS_RIDER2','STATUS_RIDER3','STATUS_RIDER4',
  'STATUS_RIDER5','STATUS_RIDER6','STATUS_RIDER7','STATUS_RIDER8','FAERIE_FIRE',
  'KILLING_SELF','REASSIGN_PARKING','BOOBY_TRAPPED','IMMOBILE','DISGUISED','DEPLOYED'
];

const _KIND_OF_NAMES = [
  'OBSTACLE','SELECTABLE','IMMOBILE','CAN_ATTACK','STICK_TO_TERRAIN_SLOPE','CAN_CAST_REFLECTIONS',
  'SHRUBBERY','STRUCTURE','INFANTRY','VEHICLE','AIRCRAFT','HUGE_VEHICLE','DOZER','HARVESTER',
  'COMMANDCENTER','LINEBUILD','SALVAGER','WEAPON_SALVAGER','TRANSPORT','BRIDGE','LANDMARK_BRIDGE',
  'BRIDGE_TOWER','PROJECTILE','PRELOAD','NO_GARRISON','WAVEGUIDE','WAVE_EFFECT','NO_COLLIDE',
  'REPAIR_PAD','HEAL_PAD','STEALTH_GARRISON','CASH_GENERATOR','DRAWABLE_ONLY',
  'MP_COUNT_FOR_VICTORY','REBUILD_HOLE','SCORE','SCORE_CREATE','SCORE_DESTROY','NO_HEAL_ICON',
  'CAN_RAPPEL','PARACHUTABLE','CAN_BE_REPULSED','MOB_NEXUS','IGNORED_IN_GUI','CRATE',
  'CAPTURABLE','CLEARED_BY_BUILD','SMALL_MISSILE','ALWAYS_VISIBLE','UNATTACKABLE','MINE',
  'CLEANUP_HAZARD','PORTABLE_STRUCTURE','ALWAYS_SELECTABLE','ATTACK_NEEDS_LINE_OF_SIGHT',
  'WALK_ON_TOP_OF_WALL','DEFENSIVE_WALL','FS_POWER','FS_FACTORY','FS_BASE_DEFENSE','FS_TECHNOLOGY',
  'AIRCRAFT_PATH_AROUND','LOW_OVERLAPPABLE','FORCEATTACKABLE','AUTO_RALLYPOINT','TECH_BUILDING',
  'POWERED','PRODUCED_AT_HELIPAD','DRONE','CAN_SEE_THROUGH_STRUCTURE','BALLISTIC_MISSILE',
  'CLICK_THROUGH','SUPPLY_SOURCE_ON_PREVIEW','PARACHUTE','GARRISONABLE_UNTIL_DESTROYED','BOAT',
  'IMMUNE_TO_CAPTURE','HULK','SHOW_PORTRAIT_WHEN_CONTROLLED','SPAWNS_ARE_THE_WEAPONS',
  'CANNOT_BUILD_NEAR_SUPPLIES','SUPPLY_SOURCE','REVEAL_TO_ALL','DISGUISER','INERT','HERO',
  'IGNORES_SELECT_ALL','DONT_AUTO_CRUSH_INFANTRY','CLIFF_JUMPER','FS_SUPPLY_DROPZONE',
  'FS_SUPERWEAPON','FS_BLACK_MARKET','FS_SUPPLY_CENTER','FS_STRATEGY_CENTER','MONEY_HACKER',
  'ARMOR_SALVAGER','REVEALS_ENEMY_PATHS','BOOBY_TRAP','FS_FAKE','FS_INTERNET_CENTER',
  'BLAST_CRATER','PROP','OPTIMIZED_TREE','FS_ADVANCED_TECH','FS_BARRACKS','FS_WARFACTORY',
  'FS_AIRFIELD','AIRCRAFT_CARRIER','NO_SELECT','REJECT_UNMANNED','CANNOT_RETALIATE',
  'TECH_BASE_DEFENSE','EMP_HARDENED','DEMOTRAP','CONSERVATIVE_BUILDING','IGNORE_DOCKING_BONES'
];

const PARAM_TYPE_HINTS = {
  INT:                  'Whole number (e.g. 5)',
  REAL:                 'Decimal number (e.g. 1.5)',
  PERCENT:              'Percentage value (1–100)',
  COUNTER:              'Counter name, defined in script logic (e.g. wave_counter)',
  FLAG:                 'Flag name, boolean variable (e.g. boss_spawned)',
  TEXT_STRING:          'Free text string',
  MOVIE:                'Movie filename without extension (e.g. EA_logo)',
  OBJECT_TYPE:          'Unit or building template name (e.g. GLATankBuggy)',
  TEAM_STATE:           'Team AI state name (e.g. IDLE, ATTACK, GUARD)',
  LOCALIZED_TEXT:       'Localization key from .str file (e.g. GUI:OK)',
  ATTACK_PRIORITY_SET:  'Attack priority set name (defined in TeamTemplate)',
  COMMANDBUTTON_ABILITY:'Command button slot number (1–12)',
  COMMANDBUTTON_ALL_ABILITIES: 'Command button name (all abilities variant)',
  COMMAND_BUTTON:       'CommandButton name from CommandSet INI',
  FONT_NAME:            'Font name (e.g. Arial)',
  EMOTICON:             'Emoticon asset name (e.g. Emoticons/Anger)',
  FACTION_NAME:         'Faction name: GLA, America, China or Civilian',
  OBJECT_TYPE_LIST:     'Comma-separated list of object template names',
  REVEALNAME:           'Unique name for this map-reveal operation',
  TIMER_NAME:           'Timer name, matches a SET_TIMER or SET_MILLISECOND_TIMER name',
};

function _getAllScriptNames(subroutineOnly) {
  const names = [];
  for (const player of (_sidelistData?.players || [])) {
    for (const group of (player.scriptGroups || [])) {
      for (const script of (group.scripts || [])) {
        if (subroutineOnly && !script.subroutine) continue;
        if (!subroutineOnly && script.subroutine) continue;
        names.push(script.name);
      }
    }
  }
  return names.sort();
}

function _makeEnumSelect(ptype, idx, entries, selectedVal) {
  const sel = document.createElement('select');
  sel.className = 'msc-input';
  sel.dataset.ptype = ptype;
  sel.dataset.idx   = String(idx);
  entries.forEach(([v, l]) => {
    const opt = document.createElement('option');
    opt.value = v; opt.textContent = l;
    if (String(selectedVal) === v || (selectedVal === '' && entries[0][0] === v)) opt.selected = true;
    sel.appendChild(opt);
  });
  return sel;
}

// Searchable select: text filter input above a multi-row <select>
// entries: [[value, label], ...]  — value is what gets stored
function _makeSearchableSelect(ptype, idx, entries, selectedVal) {
  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex;flex-direction:column;flex:1;min-width:0;gap:3px';

  const filterEl = document.createElement('input');
  filterEl.type = 'text';
  filterEl.placeholder = 'Filter…';
  filterEl.style.cssText = 'font-size:11px;padding:2px 6px;box-sizing:border-box;width:100%;background:var(--bg2,#181825);color:var(--text,#cdd6f4);border:1px solid var(--border,#444);border-radius:3px;outline:none';

  const sel = document.createElement('select');
  sel.className = 'msc-input';
  sel.dataset.ptype = ptype;
  sel.dataset.idx   = String(idx);
  sel.size = 6;
  sel.style.cssText = 'width:100%;font-size:11px;background:var(--bg2,#181825);color:var(--text,#cdd6f4);border:1px solid var(--border,#444);border-radius:3px';

  function populate(q) {
    const prev = sel.value;
    sel.innerHTML = '';
    const lq = q.toLowerCase();
    entries.forEach(([v, l]) => {
      if (lq && !l.toLowerCase().includes(lq)) return;
      const opt = document.createElement('option');
      opt.value = v; opt.textContent = l;
      if (String(selectedVal) === v || (prev && prev === v)) opt.selected = true;
      sel.appendChild(opt);
    });
    // auto-select first if nothing selected and we have a selectedVal not in filtered view
    if (!sel.value && entries.length) sel.options[0] && (sel.options[0].selected = true);
  }

  populate('');
  filterEl.addEventListener('input', () => populate(filterEl.value));
  wrap.append(filterEl, sel);
  return wrap;
}

// COORD3D: three number inputs → stores "(x,y,z)" string via hidden input
function _makeCoord3DInput(idx, pval) {
  const m = (pval || '').match(/\(?\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*\)?/);
  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex;align-items:center;gap:3px;flex:1;min-width:0';

  const hidden = document.createElement('input');
  hidden.type = 'hidden'; hidden.className = 'msc-input';
  hidden.dataset.ptype = 'COORD3D'; hidden.dataset.idx = String(idx);
  hidden.value = pval || '';

  const N_STYLE = 'width:56px;font-size:11px;padding:2px 4px;background:var(--bg2,#181825);color:var(--text,#cdd6f4);border:1px solid var(--border,#444);border-radius:3px;text-align:right;min-width:0';
  const L_STYLE = 'font-size:10px;color:var(--text-hint,#888);flex-shrink:0';

  const fields = ['X','Y','Z'].map((lbl, i) => {
    const sp = document.createElement('span'); sp.textContent = lbl; sp.style.cssText = L_STYLE;
    const n = document.createElement('input'); n.type = 'number'; n.step = '1';
    n.value = m ? m[i+1] : '0'; n.placeholder = lbl; n.style.cssText = N_STYLE;
    return { sp, n };
  });

  function sync() {
    hidden.value = `(${fields[0].n.value||0},${fields[1].n.value||0},${fields[2].n.value||0})`;
    hidden.dispatchEvent(new Event('change', {bubbles:true}));
  }
  fields.forEach(({sp, n}) => { n.addEventListener('input', sync); wrap.append(sp, n); });
  wrap.appendChild(hidden);
  return wrap;
}

// ANGLE: degrees (0–360) input → stores radians as string via hidden input
function _makeAngleInput(idx, pval) {
  const rad = parseFloat(pval) || 0;
  const deg = Math.round(rad * 180 / Math.PI * 10) / 10;

  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex;align-items:center;gap:4px;flex:1;min-width:0';

  const degEl = document.createElement('input');
  degEl.type = 'number'; degEl.min = 0; degEl.max = 360; degEl.step = 1;
  degEl.value = deg;
  degEl.style.cssText = 'width:72px;font-size:11px;padding:2px 4px;background:var(--bg2,#181825);color:var(--text,#cdd6f4);border:1px solid var(--border,#444);border-radius:3px;text-align:right';

  const sp = document.createElement('span'); sp.textContent = '°';
  sp.style.cssText = 'font-size:12px;color:var(--text-hint,#888);flex-shrink:0';

  const hidden = document.createElement('input');
  hidden.type = 'hidden'; hidden.className = 'msc-input';
  hidden.dataset.ptype = 'ANGLE'; hidden.dataset.idx = String(idx);
  hidden.value = pval || '0';

  degEl.addEventListener('input', () => {
    const r = (parseFloat(degEl.value) || 0) * Math.PI / 180;
    hidden.value = String(Math.round(r * 100000) / 100000);
    hidden.dispatchEvent(new Event('change', {bubbles:true}));
  });

  wrap.append(degEl, sp, hidden);
  return wrap;
}

// COLOR: native color picker → stores ARGB int as string via hidden input
function _makeColorInput(idx, pval) {
  const argb = (parseInt(pval) || 0xFFFFFFFF) >>> 0;
  const r = (argb >> 16) & 0xFF, g = (argb >> 8) & 0xFF, b = argb & 0xFF;
  const hex = '#' + [r,g,b].map(c => c.toString(16).padStart(2,'0')).join('');

  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex;align-items:center;gap:6px;flex:1;min-width:0';

  const picker = document.createElement('input');
  picker.type = 'color'; picker.value = hex;
  picker.style.cssText = 'width:36px;height:24px;padding:1px;border:1px solid var(--border,#444);border-radius:3px;background:none;cursor:pointer';

  const lbl = document.createElement('span');
  lbl.textContent = pval || String(argb);
  lbl.style.cssText = 'font-size:10px;color:var(--text-hint,#888);font-family:monospace;min-width:0;overflow:hidden;text-overflow:ellipsis';

  const hidden = document.createElement('input');
  hidden.type = 'hidden'; hidden.className = 'msc-input';
  hidden.dataset.ptype = 'COLOR'; hidden.dataset.idx = String(idx);
  hidden.value = pval || String(argb);

  picker.addEventListener('input', () => {
    const hx = picker.value.slice(1);
    const ri = parseInt(hx.slice(0,2),16), gi = parseInt(hx.slice(2,4),16), bi = parseInt(hx.slice(4,6),16);
    const val = ((255 << 24) | (ri << 16) | (gi << 8) | bi) >>> 0;
    hidden.value = String(val); lbl.textContent = String(val);
    hidden.dispatchEvent(new Event('change', {bubbles:true}));
  });

  wrap.append(picker, lbl, hidden);
  return wrap;
}

function _makeSelect(ptype, idx, options, selectedVal) {
  const sel = document.createElement('select');
  sel.className = 'msc-input';
  sel.dataset.ptype = ptype;
  sel.dataset.idx   = idx;
  const emptyOpt = document.createElement('option');
  emptyOpt.value = '';
  emptyOpt.textContent = '-- select --';
  sel.appendChild(emptyOpt);
  options.forEach(name => {
    const opt = document.createElement('option');
    opt.value = name; opt.textContent = name;
    if (name === selectedVal) opt.selected = true;
    sel.appendChild(opt);
  });
  if (selectedVal && !options.includes(selectedVal)) {
    const opt = document.createElement('option');
    opt.value = selectedVal;
    opt.textContent = selectedVal + ' (not in map)';
    opt.selected = true;
    sel.appendChild(opt);
  }
  return sel;
}

function _makePercentSlider(pval, idx) {
  const val = pval !== '' && pval !== undefined ? Number(pval) : 100;

  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex;align-items:center;gap:8px;flex:1;min-width:0';

  const range = document.createElement('input');
  range.type = 'range'; range.min = 1; range.max = 100; range.value = val;
  range.style.cssText = 'flex:1;min-width:0;accent-color:#3b82f6;cursor:pointer';
  range.dataset.ptype = 'INT';
  range.dataset.idx   = String(idx);
  range.className = 'msc-input';

  const lbl = document.createElement('span');
  lbl.textContent = val + '%';
  lbl.style.cssText = 'flex-shrink:0;width:36px;text-align:right;font-size:11px;color:var(--text);font-variant-numeric:tabular-nums';

  range.addEventListener('input', () => { lbl.textContent = range.value + '%'; });

  wrap.append(range, lbl);
  wrap.addEventListener = (type, fn) => range.addEventListener(type, fn);
  return wrap;
}

function _makeParamInput(ptype, pval, idx) {
  if (ptype === 'BOOLEAN') {
    return _makeSelect(ptype, idx, ['true', 'false'], pval || 'true');
  }
  if (ptype === 'TEAM') {
    const teams = (_sidelistData?.teams || []).map(t => t.name);
    return _makeSelect(ptype, idx, teams, pval);
  }
  if (ptype === 'SIDE') {
    const sides = (_sidelistData?.players || []).map(p => p.name).filter(n => n);
    return _makeSelect(ptype, idx, sides, pval);
  }
  if (ptype === 'WAYPOINT') {
    const wps = (_scpWaypointCache || []).map(w => w.name || w).filter(Boolean);
    return _makeSelect(ptype, idx, wps, pval);
  }
  if (ptype === 'WAYPOINT_PATH' || ptype === 'SKIRMISH_WAYPOINT_PATH') {
    // Collect unique path names from cached waypoints
    const pathSet = new Set();
    for (const wp of (_scpWaypointCache || [])) {
      for (const p of (wp.paths || [])) if (p) pathSet.add(p);
    }
    return _makeSelect(ptype, idx, [...pathSet].sort(), pval);
  }
  if (ptype === 'COMPARISON') {
    const COMP_OPS = [
      { v: '0', l: '< less than' },
      { v: '1', l: '≤ less or equal' },
      { v: '2', l: '= equal' },
      { v: '3', l: '≥ greater or equal' },
      { v: '4', l: '> greater than' },
      { v: '5', l: '≠ not equal' },
    ];
    const sel = document.createElement('select');
    sel.className = 'msc-input';
    sel.dataset.ptype = ptype;
    sel.dataset.idx   = idx;
    const defaultVal  = pval !== '' && pval !== undefined ? String(pval) : '2';
    COMP_OPS.forEach(op => {
      const opt = document.createElement('option');
      opt.value = op.v;
      opt.textContent = op.l;
      if (op.v === defaultVal) opt.selected = true;
      sel.appendChild(opt);
    });
    return sel;
  }
  if (ptype === 'TRIGGER_AREA') {
    const names = (_scpTriggerCache || []).map(t => t.name);
    return _makeSelect(ptype, idx, names, pval);
  }
  if (ptype === 'UNIT') {
    const names = (_scpObjectCache || [])
      .filter(o => o.name && o.name.trim())
      .map(o => o.name);
    return _makeSelect(ptype, idx, names, pval);
  }
  if (ptype === 'SCRIPT' || ptype === 'SCRIPT_SUBROUTINE') {
    const names = _getAllScriptNames(ptype === 'SCRIPT_SUBROUTINE');
    return _makeSelect(ptype, idx, names, pval);
  }
  if (ptype === 'RELATION') {
    // Stored as int: 0=Enemy, 1=Neutral, 2=Friend
    return _makeEnumSelect(ptype, idx, [['0','Enemy'],['1','Neutral'],['2','Friend']], pval);
  }
  if (ptype === 'AI_MOOD') {
    // Stored as int: -2=Sleep, -1=Passive, 0=Normal, 1=Alert, 2=Aggressive
    return _makeEnumSelect(ptype, idx, [['-2','Sleep'],['-1','Passive'],['0','Normal'],['1','Alert'],['2','Aggressive']], pval);
  }
  if (ptype === 'BUILDABLE') {
    // Stored as int: 0=Yes, 1=Ignore_Prerequisites, 2=No
    return _makeEnumSelect(ptype, idx, [['0','Yes'],['1','Ignore Prerequisites'],['2','No']], pval);
  }
  if (ptype === 'SURFACES_ALLOWED') {
    // Stored as int: 1=Ground, 2=Air, 3=Ground or Air
    return _makeEnumSelect(ptype, idx, [['1','Ground'],['2','Air'],['3','Ground or Air']], pval);
  }
  if (ptype === 'LEFT_OR_RIGHT') {
    // Stored as int: 1=Left, 2=Right, 3=Center
    return _makeEnumSelect(ptype, idx, [['1','Left'],['2','Right'],['3','Center (default)']], pval);
  }
  if (ptype === 'FACTION_NAME') {
    return _makeSelect(ptype, idx, ['GLA','America','China','Civilian'], pval);
  }
  if (ptype === 'SCIENCE_AVAILABILITY') {
    return _makeSelect(ptype, idx, ['Available','Disabled','Hidden'], pval);
  }
  if (ptype === 'RADAR_EVENT_TYPE') {
    // Stored as int: 1=Construction, 2=Upgrade, 3=Under Attack, 4=Information
    return _makeEnumSelect(ptype, idx, [['1','Construction'],['2','Upgrade'],['3','Under Attack'],['4','Information']], pval);
  }
  if (ptype === 'SHAKE_INTENSITY') {
    // Stored as int: 0=Subtle, 1=Normal, 2=Strong, 3=Severe, 4=Cine_Extreme, 5=Cine_Insane
    return _makeEnumSelect(ptype, idx, [['0','Subtle'],['1','Normal'],['2','Strong'],['3','Severe'],['4','Cine_Extreme'],['5','Cine_Insane']], pval);
  }
  if (ptype === 'OBJECT_STATUS') {
    return _makeSearchableSelect(ptype, idx, _OBJECT_STATUS_NAMES.map(n => [n, n]), pval);
  }
  if (ptype === 'KIND_OF_PARAM') {
    return _makeSearchableSelect(ptype, idx, _KIND_OF_NAMES.map((n, i) => [String(i), n]), pval);
  }
  if (ptype === 'COORD3D') {
    return _makeCoord3DInput(idx, pval);
  }
  if (ptype === 'ANGLE') {
    return _makeAngleInput(idx, pval);
  }
  if (ptype === 'COLOR') {
    return _makeColorInput(idx, pval);
  }
  if (ptype === 'BOUNDARY') {
    // Stored as int 0-7, maps to border color names
    return _makeEnumSelect(ptype, idx, [
      ['0','Orange'],['1','Green'],['2','Blue'],['3','Cyan'],
      ['4','Magenta'],['5','Yellow'],['6','Purple'],['7','Pink']
    ], pval);
  }
  if (ptype === 'OBJECT_PANEL_FLAG') {
    return _makeSelect(ptype, idx, [
      'Enabled','Powered','Indestructible','Unsellable',
      'Selectable','AI Recruitable','Player Targetable'
    ], pval);
  }
  if (ptype === 'BRIDGE') {
    // Bridge objects — FLAG_BRIDGE_POINT1 (0x10); deduplicated by name
    const seen = new Set();
    const names = (_scpObjectCache || [])
      .filter(o => o.name && o.name.trim() && (o.flags & 0x10))
      .filter(o => { if (seen.has(o.name)) return false; seen.add(o.name); return true; })
      .map(o => o.name);
    return _makeSelect(ptype, idx, names, pval);
  }
  if (ptype === 'SOUND') {
    return _makeIniSelect(ptype, idx, '/api/ini/sounds', pval);
  }
  if (ptype === 'DIALOG') {
    return _makeIniSelect(ptype, idx, '/api/ini/dialog', pval);
  }
  if (ptype === 'MUSIC') {
    return _makeIniSelect(ptype, idx, '/api/ini/music', pval);
  }
  if (ptype === 'SPECIAL_POWER') {
    return _makeIniSelect(ptype, idx, '/api/ini/specialpowers', pval);
  }
  if (ptype === 'SCIENCE') {
    return _makeIniSelect(ptype, idx, '/api/ini/sciences', pval);
  }
  if (ptype === 'UPGRADE') {
    return _makeIniSelect(ptype, idx, '/api/ini/upgrades', pval);
  }
  // Number or free text
  const inp = document.createElement('input');
  inp.type = (ptype === 'INT' || ptype === 'REAL' || ptype === 'PERCENT') ? 'number' : 'text';
  if (ptype === 'REAL' || ptype === 'PERCENT') inp.step = '0.1';
  inp.className = 'msc-input';
  inp.value = pval;
  inp.dataset.ptype = ptype;
  inp.dataset.idx   = idx;
  inp.placeholder   = ptype;
  if (PARAM_TYPE_HINTS[ptype]) inp.title = PARAM_TYPE_HINTS[ptype];
  return inp;
}

// ── Modal state ──────────────────────────────────────────────────────────────
let _mscOpen            = false;
let _mscOnSaveCallback  = null; // set by wizards; called with savedName after mscSaveScript succeeds
let _mscSelPlayer = null;
let _mscSelGroup  = null;
let _mscSelScript = null;
let _mscEditConds  = [];   // deep copy while editing
let _mscEditActT   = [];
let _mscEditActF   = [];

// C++ serializes params as {pt, v}; JS uses {type, value} — normalize on load
function _normParam(p) {
  return {
    type:  p.type  || p.pt || 'TEXT_STRING',
    value: p.value !== undefined ? String(p.value) : (p.v !== undefined ? String(p.v) : '')
  };
}
function _normAct(a)  { return { type: a.type, params: (a.params  || []).map(_normParam) }; }
function _normCond(c) { return { type: c.type, params: (c.params  || []).map(_normParam) }; }
let _mscSelCondKey = null; // '{orIdx}:{andIdx}'
let _mscSelActTKey = null; // '{idx}'
let _mscSelActFKey = null;
let _mscCondModalCb = null; // callback(condOrAction)
let _mscCondModalMode = 'cond'; // 'cond' | 'act'
let _mscSelCatType = null; // currently selected type in sub-modal

// ── Open / close ─────────────────────────────────────────────────────────────
async function openScriptsModal() {
  _mscOpen = true;
  _scpWaypointCache = null; // reset so dropdowns reflect current map on next open
  _scpObjectCache   = null;
  _scpTriggerCache  = null;
  document.getElementById('modal-scripts').classList.remove('hidden');
  startSlPolling();
  await fetchSideList();
  mscRefreshPlayerSel();
}
function closeScriptsModal() {
  _mscOpen = false;
  _mscOnSaveCallback = null;
  document.getElementById('modal-scripts').classList.add('hidden');
}

function mscRefreshPlayerSel() {
  const players = _sidelistData?.players || [];
  // Auto-expand first player if nothing is expanded yet
  if (_mscExpandedPlayers.size === 0 && players.length) {
    const firstName = players[0].name ?? '';
    _mscExpandedPlayers.add(firstName);
    if (_mscSelPlayer === null || _mscSelPlayer === undefined) _mscSelPlayer = firstName;
  }
  mscRenderTree();
}

function mscSyncWizardPlayers() {
  const sel = document.getElementById('msc-wiz-player');
  if (!sel) return;
  const prev = sel.value;
  const players = _sidelistData?.players || [];
  sel.innerHTML = players.map(p => `<option value="${p.name}">${p.name}</option>`).join('');
  if (prev && players.find(p => p.name === prev)) sel.value = prev;
}

// ── Tree rendering ────────────────────────────────────────────────────────────
// _mscExpandedGroups keys: "playerName::groupName"
let _mscExpandedGroups  = new Set();
let _mscExpandedPlayers = new Set(); // which player root nodes are open

function mscMakeEl(tag, cls, text, style) {
  const el = document.createElement(tag);
  if (cls)   el.className = cls;
  if (text)  el.textContent = text;
  if (style) el.style.cssText = style;
  return el;
}

function mscRenderTree() {
  const tree   = document.getElementById('msc-tree');
  const status = document.getElementById('msc-tree-status');
  tree.innerHTML = '';

  if (!_sidelistData?.players?.length) {
    tree.appendChild(mscMakeEl('div', '', '(No data — start WorldBuilder and click ↻ Refresh)',
      'padding:12px 10px;font-size:11px;color:var(--text-hint);font-style:italic'));
    if (status) status.textContent = 'Not connected to WorldBuilder';
    return;
  }
  if (status) status.textContent = '';

  for (const player of _sidelistData.players) {
    const pName      = player.name ?? '';
    const isSelP     = _mscSelPlayer === pName;
    const pExpanded  = _mscExpandedPlayers.has(pName);
    const displayLbl = player.displayName || pName || '(neutral)';

    // ── Player root folder ────────────────────────────────────────────────────
    const playerRow = document.createElement('div');
    playerRow.className = 'msc-group-item' + (isSelP && !_mscSelGroup && !_mscSelScript ? ' active' : '');
    const pArrow = mscMakeEl('span', 'msc-group-arrow' + (pExpanded ? ' open' : ''), '▶');
    const pIcon  = mscMakeEl('span', '', '📂', 'font-size:12px;flex-shrink:0');
    const pLbl   = mscMakeEl('span', '', displayLbl,
      'flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-weight:500');
    playerRow.append(pArrow, pIcon, pLbl);
    playerRow.addEventListener('click', () => {
      if (_mscExpandedPlayers.has(pName)) _mscExpandedPlayers.delete(pName);
      else _mscExpandedPlayers.add(pName);
      _mscSelPlayer = pName;
      _mscSelGroup = null; _mscSelScript = null;
      mscRenderTree(); mscUpdateSelDesc();
    });
    tree.appendChild(playerRow);

    if (!pExpanded) continue;

    // ── Script groups (sub-folders) ───────────────────────────────────────────
    const groups = player.scriptGroups || [];
    if (!groups.length) {
      tree.appendChild(mscMakeEl('div', '', '(no script folders yet)',
        'padding:4px 10px 4px 28px;font-size:10px;color:var(--text-hint);font-style:italic'));
      continue;
    }

    for (const group of groups) {
      const gKey      = pName + '::' + group.name;
      const gExpanded = _mscExpandedGroups.has(gKey);
      const isSelG    = isSelP && _mscSelGroup?.name === group.name;

      const groupRow = document.createElement('div');
      groupRow.className = 'msc-group-item' + (isSelG && !_mscSelScript ? ' active' : '');
      groupRow.style.paddingLeft = '18px';

      const gArrow = mscMakeEl('span', 'msc-group-arrow' + (gExpanded ? ' open' : ''), '▶');
      const gIcon  = mscMakeEl('span', '', '📁', 'font-size:12px;flex-shrink:0');
      const flags  = (group.subroutine ? '[s]' : '') + (group.active ? '' : '[off]');
      const gLbl   = mscMakeEl('span', '', (flags ? flags + ' ' : '') + group.name,
        'flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap');
      if (!group.active) gLbl.style.opacity = '0.5';
      groupRow.append(gArrow, gIcon, gLbl);
      groupRow.addEventListener('click', () => {
        _mscSelPlayer = pName;
        mscSelectGroup(group, gKey);
      });
      tree.appendChild(groupRow);

      if (!gExpanded) continue;

      // ── Scripts ─────────────────────────────────────────────────────────────
      const scripts = group.scripts || [];
      if (!scripts.length) {
        tree.appendChild(mscMakeEl('div', '', '(empty folder)',
          'padding:3px 10px 3px 44px;font-size:10px;color:var(--text-hint);font-style:italic'));
        continue;
      }
      for (const s of scripts) {
        const isSelS = isSelG && _mscSelScript?.name === s.name;
        const sRow   = document.createElement('div');
        sRow.className = 'msc-script-item' + (isSelS ? ' active' : '') + (!s.active ? ' inactive' : '');
        const sIcon  = mscMakeEl('span', '', s.active ? '▶' : '⏸', 'font-size:9px;flex-shrink:0;color:var(--text-hint)');
        const sLbl   = mscMakeEl('span', '', s.name, 'flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap');
        sRow.append(sIcon, sLbl);
        sRow.addEventListener('click', e => {
          e.stopPropagation();
          _mscSelPlayer = pName;
          mscSelectScript(group, s);
        });
        sRow.addEventListener('dblclick', e => {
          e.stopPropagation();
          _mscSelPlayer = pName;
          mscSelectScript(group, s);
          mscEditSelected();
        });
        tree.appendChild(sRow);
      }
    }
  }
}

function mscSelectGroup(group, gKey) {
  const key = gKey || (_mscSelPlayer + '::' + group.name);
  if (_mscSelGroup?.name === group.name && _mscExpandedGroups.has(key)) {
    _mscExpandedGroups.delete(key);
    _mscSelGroup = null;
  } else {
    _mscExpandedGroups.add(key);
    _mscSelGroup = group;
  }
  _mscSelScript = null;
  mscRenderTree();
  mscUpdateSelDesc();
}

function mscSelectScript(group, script) {
  _mscSelGroup  = group;
  _mscSelScript = script;
  mscRenderTree();
  mscUpdateSelDesc();
}

function mscEditSelected() {
  if (_mscSelScript) mscOpenScriptModal(_mscSelScript);
}

// ── Panel state helpers ───────────────────────────────────────────────────────
// ── Selection description (right column) ─────────────────────────────────────
function mscUpdateSelDesc() {
  const el = document.getElementById('msc-sel-desc');
  if (!el) return;
  if (_mscSelScript) {
    const condCount = (_mscSelScript.conditions || []).reduce((n, or) => n + (or||[]).length, 0);
    const actCount  = (_mscSelScript.actionsTrue || []).length + (_mscSelScript.actionsFalse || []).length;
    const flags     = [_mscSelScript.easy&&'Easy', _mscSelScript.normal&&'Normal', _mscSelScript.hard&&'Hard'].filter(Boolean).join(', ');
    el.textContent  = `${_mscSelScript.name}\n${flags}\n${condCount} condition(s)\n${actCount} action(s)`;
  } else if (_mscSelGroup) {
    const cnt = (_mscSelGroup.scripts || []).length;
    el.textContent  = `${_mscSelGroup.name}\n${cnt} script(s)`;
  } else {
    el.textContent = '';
  }
}

// ── Script editor sub-modal ───────────────────────────────────────────────────
function mscOpenScriptModal(s) {
  document.getElementById('msc-s-editor-title').textContent = s.name || 'Script';
  document.getElementById('msc-s-name').value     = s.name || '';
  document.getElementById('msc-s-sub').checked    = !!s.subroutine;
  document.getElementById('msc-s-active').checked = !!s.active;
  document.getElementById('msc-s-oneshot').checked= !!s.oneShot;
  document.getElementById('msc-s-easy').checked   = s.easy   !== false;
  document.getElementById('msc-s-normal').checked = s.normal !== false;
  document.getElementById('msc-s-hard').checked   = s.hard   !== false;
  document.getElementById('msc-s-comment').value  = s.comment || '';

  if (s.evalSecs && s.evalSecs > 0) {
    document.getElementById('msc-s-eval-secs').checked = true;
    document.getElementById('msc-s-eval-n').value = s.evalSecs;
  } else {
    document.getElementById('msc-s-eval-frame').checked = true;
  }

  const rawConds = s.conditions && s.conditions.length ? s.conditions : [[{type:3, params:[]}]];
  _mscEditConds  = rawConds.map(orGroup => orGroup.map(_normCond));
  _mscEditActT   = (s.actionsTrue  || []).map(_normAct);
  _mscEditActF   = (s.actionsFalse || []).map(_normAct);
  _mscSelCondKey = null; _mscSelActTKey = null; _mscSelActFKey = null;

  // Reset to Properties tab
  document.querySelectorAll('#msc-script-modal-box .ms-inner-tab').forEach((b,i) => b.classList.toggle('active', i===0));
  document.querySelectorAll('#msc-script-modal-box .ms-inner-body').forEach((b,i) => b.classList.toggle('hidden', i!==0));

  mscRenderCondList();
  mscRenderActList('true');
  mscRenderActList('false');
  document.getElementById('msc-script-modal').classList.remove('hidden');
  // Disable the parent modal so it doesn't intercept clicks
  document.getElementById('modal-scripts').style.pointerEvents = 'none';
}

function mscCloseScriptModal() {
  document.getElementById('msc-script-modal').classList.add('hidden');
  document.getElementById('modal-scripts').style.pointerEvents = '';
}

// ── Conditions list ───────────────────────────────────────────────────────────
function mscRenderCondList() {
  const list = document.getElementById('msc-cond-list');
  list.innerHTML = '';
  _mscEditConds.forEach((orGroup, oi) => {
    const grpEl = document.createElement('div');
    grpEl.className = 'msc-or-group';
    const lbl = document.createElement('div');
    lbl.className = 'msc-or-label';
    lbl.textContent = oi === 0 ? 'if' : '— or if';
    grpEl.appendChild(lbl);
    (orGroup || []).forEach((cond, ai) => {
      const key = oi + ':' + ai;
      const el = document.createElement('div');
      el.className = 'msc-cond-entry and-item' + (_mscSelCondKey === key ? ' active' : '');
      el.textContent = (ai > 0 ? 'and ' : '') + condLabel(cond);
      el.addEventListener('click', () => {
        _mscSelCondKey = key;
        mscRenderCondList();
        document.getElementById('msc-cond-desc').textContent = condLabel(cond);
      });
      el.addEventListener('dblclick', () => mscOpenCondModal('cond', cond, updated => {
        _mscEditConds[oi][ai] = updated;
        mscRenderCondList();
      }));
      grpEl.appendChild(el);
    });
    list.appendChild(grpEl);
  });
}

function mscRenderActList(which) {
  const id   = which === 'true' ? 'msc-act-true-list' : 'msc-act-false-list';
  const descId = which === 'true' ? 'msc-act-true-desc' : 'msc-act-false-desc';
  const acts = which === 'true' ? _mscEditActT : _mscEditActF;
  const selKey = which === 'true' ? _mscSelActTKey : _mscSelActFKey;
  const list = document.getElementById(id);
  list.innerHTML = '';
  acts.forEach((act, i) => {
    const el = document.createElement('div');
    el.className = 'msc-act-entry' + (selKey === String(i) ? ' active' : '');
    el.textContent = actLabel(act);
    el.addEventListener('click', () => {
      if (which === 'true') _mscSelActTKey = String(i);
      else _mscSelActFKey = String(i);
      mscRenderActList(which);
      document.getElementById(descId).textContent = actLabel(act);
    });
    el.addEventListener('dblclick', () => mscOpenCondModal('act', act, updated => {
      if (which === 'true') _mscEditActT[i] = updated;
      else _mscEditActF[i] = updated;
      mscRenderActList(which);
    }));
    list.appendChild(el);
  });
}

// ── Full-screen Script Picker ─────────────────────────────────────────────────
let _scpSelCat = 'All';

function scpData() {
  return _mscCondModalMode === 'cond'
    ? (typeof COND_DATA !== 'undefined' ? COND_DATA : [])
    : (typeof ACT_DATA  !== 'undefined' ? ACT_DATA  : []);
}

async function mscOpenCondModal(mode, existing, callback) {
  _mscCondModalMode = mode;
  _mscCondModalCb   = callback;
  // Use existing type if editing, otherwise use first valid type from data
  const data0 = mode === 'cond' ? (typeof COND_DATA !== 'undefined' ? COND_DATA : [])
                                : (typeof ACT_DATA  !== 'undefined' ? ACT_DATA  : []);
  const validFirst = data0.find(e => e.t >= 0)?.t ?? 0;
  _mscSelCatType = existing?.type ?? validFirst;
  _scpSelCat     = 'All';
  document.getElementById('msc-cond-modal-title').textContent = mode === 'cond' ? 'Select Condition' : 'Select Action';
  document.getElementById('scp-tab-cond').classList.toggle('active', mode === 'cond');
  document.getElementById('scp-tab-cond').classList.toggle('hidden', mode !== 'cond');
  document.getElementById('scp-tab-act').classList.toggle('active', mode !== 'cond');
  document.getElementById('scp-tab-act').classList.toggle('hidden', mode === 'cond');
  document.getElementById('scp-search').value = '';
  document.getElementById('btn-msc-cond-ok').disabled = false; // scpValidateOk() re-checks after params render
  document.getElementById('msc-cond-modal').classList.remove('hidden');
  document.getElementById('msc-script-modal').style.pointerEvents = 'none';
  // Pre-fetch waypoints + named objects for dropdowns (cached after first call)
  if (!_scpWaypointCache) {
    const r = await apiGet('/map/waypoints');
    _scpWaypointCache = r?.waypoints || [];
  }
  if (!_scpObjectCache) {
    const r = await apiGet('/map/objects');
    _scpObjectCache = (r?.objects || []).filter(o => o.name && o.name.trim());
  }
  if (!_scpTriggerCache) {
    const r = await apiGet('/map/triggers');
    _scpTriggerCache = (r?.triggers || []).filter(t => t.name);
  }
  scpRender();
  scpShowParams(existing?.params || []);
}

function scpRender() {
  const q        = document.getElementById('scp-search')?.value.toLowerCase().trim() || '';
  const showUnused = document.getElementById('scp-show-unused')?.checked || false;
  const allData  = scpData();

  // Filter
  const entries = allData.filter(e => {
    const uiPath = e.p.join('/').toLowerCase();
    if (!showUnused && uiPath.startsWith('unused')) return false;
    if (!q) return true;
    const paramTypes = ((typeof SCRIPT_PARAMS !== 'undefined' && SCRIPT_PARAMS[e.i]?.p) || []).join(' ').toLowerCase();
    return uiPath.includes(q) || e.i.toLowerCase().includes(q) || paramTypes.includes(q);
  });

  // Chips
  const chips = document.getElementById('scp-chips');
  chips.innerHTML = '';
  const cats = ['All', ...new Set(entries.map(e => e.p[0]))].sort((a, b) =>
    a === 'All' ? -1 : b === 'All' ? 1 : a.localeCompare(b));
  for (const cat of cats) {
    const cnt = cat === 'All' ? entries.length : entries.filter(e => e.p[0] === cat).length;
    if (cnt === 0 && cat !== 'All') continue;
    const chip = document.createElement('div');
    chip.className = 'scp-chip' + (_scpSelCat === cat ? ' active' : '');
    chip.textContent = cat + (cat !== 'All' ? ` (${cnt})` : '');
    chip.onclick = () => { _scpSelCat = cat; scpRender(); };
    chips.appendChild(chip);
  }

  // Grid
  const grid = document.getElementById('scp-grid-area');
  grid.innerHTML = '';
  const show = _scpSelCat === 'All' ? entries : entries.filter(e => e.p[0] === _scpSelCat);
  if (!show.length) {
    grid.innerHTML = '<div style="padding:40px;text-align:center;color:#444;font-size:12px">No results</div>';
    return;
  }

  // Group by category → subcategory
  const byCat = {};
  for (const e of show) {
    const cat  = e.p[0];
    const sub  = e.p.length >= 3 ? e.p.slice(1, -1).join(' › ') : '__root__';
    if (!byCat[cat]) byCat[cat] = {};
    if (!byCat[cat][sub]) byCat[cat][sub] = [];
    byCat[cat][sub].push(e);
  }

  for (const [cat, subs] of Object.entries(byCat).sort(([a],[b]) => a.localeCompare(b))) {
    const sec = document.createElement('div');
    sec.className = 'scp-sec';
    const total = Object.values(subs).flat().length;
    const head = document.createElement('div');
    head.className = 'scp-sec-head';
    head.innerHTML = `${cat} <span class="scp-cnt">${total}</span>`;
    sec.appendChild(head);

    const subKeys = Object.keys(subs).sort((a,b) => a === '__root__' ? -1 : b === '__root__' ? 1 : a.localeCompare(b));
    for (const subKey of subKeys) {
      if (subKey !== '__root__' && subKeys.length > 1) {
        const sh = document.createElement('div');
        sh.className = 'scp-sub-head';
        sh.textContent = subKey;
        sec.appendChild(sh);
      }
      const cards = document.createElement('div');
      cards.className = 'scp-cards';
      for (const e of subs[subKey]) {
        const label = e.p[e.p.length - 1];
        const card  = document.createElement('div');
        card.className = 'scp-card' + (e.t === _mscSelCatType ? ' sel' : '');

        const labelEl = document.createElement('div');
        labelEl.textContent = label;
        card.appendChild(labelEl);

        // Param type chips
        const tmplParams = (typeof SCRIPT_PARAMS !== 'undefined' && SCRIPT_PARAMS[e.i]?.p) || [];
        if (tmplParams.length) {
          const chips = document.createElement('div');
          chips.style.cssText = 'display:flex;flex-wrap:wrap;gap:2px;margin-top:3px';
          const ABBREV = { BOOLEAN:'BOOL', WAYPOINT:'WP', TRIGGER_AREA:'AREA', SPECIAL_POWER:'SP',
                           SKIRMISH_WAYPOINT_PATH:'SKP', WAYPOINT_PATH:'WPP', SCRIPT_SUBROUTINE:'SUB',
                           OBJECT_STATUS:'OSTATUS', KIND_OF_PARAM:'KINDOF', COORD3D:'XYZ',
                           LOCALIZED_TEXT:'LOC', ATTACK_PRIORITY_SET:'ATTK', COMMANDBUTTON_ABILITY:'CBTN' };
          const COLOR = { MUSIC:'#7c3aed', SOUND:'#7c3aed', DIALOG:'#7c3aed',
                          SPECIAL_POWER:'#059669', SCIENCE:'#059669', UPGRADE:'#059669',
                          BOOLEAN:'#b45309', COMPARISON:'#b45309', RELATION:'#b45309',
                          TEAM:'#1d4ed8', UNIT:'#1d4ed8', WAYPOINT:'#1d4ed8', TRIGGER_AREA:'#1d4ed8' };
          for (const pt of tmplParams) {
            const chip = document.createElement('span');
            const short = ABBREV[pt] || (pt.length > 7 ? pt.slice(0,6)+'…' : pt);
            chip.textContent = short;
            chip.title = pt;
            const bg = COLOR[pt] || '#374151';
            chip.style.cssText = `font-size:8px;padding:1px 3px;border-radius:2px;background:${bg};color:#e5e7eb;white-space:nowrap;line-height:1.4`;
            chips.appendChild(chip);
          }
          card.appendChild(chips);
        }

        card.title = e.i + ' (id=' + e.t + ')';
        card.onclick = () => {
          const prevType = _mscSelCatType;
          const prevParams = prevType === e.t ? mscGetCondFromModal().params : [];
          grid.querySelectorAll('.scp-card').forEach(c => c.classList.remove('sel'));
          card.classList.add('sel');
          _mscSelCatType = e.t;
          document.getElementById('scp-sel-info').innerHTML =
            `<strong style="color:#93c5fd">${label}</strong> <span style="color:#333;font-size:10px">[${e.i}]</span>`;
          document.getElementById('btn-msc-cond-ok').disabled = false;
          scpShowParams(prevParams);
        };
        card.ondblclick = () => {
          _mscSelCatType = e.t;
          scpConfirm();
        };
        cards.appendChild(card);
      }
      sec.appendChild(cards);
    }
    grid.appendChild(sec);
  }

  // Update footer OK state
  document.getElementById('btn-msc-cond-ok').disabled = (_mscSelCatType < 0);
}

function scpShowParams(existingParams) {
  const paramsEl = document.getElementById('msc-cond-params');
  paramsEl.innerHTML = '';
  const preview  = document.getElementById('msc-cond-preview');

  // Look up entry in data
  const allData = scpData();
  const entry   = allData.find(e => e.t === _mscSelCatType);
  const label   = entry ? entry.p[entry.p.length - 1] : ('Type ' + _mscSelCatType);
  const iname   = entry?.i || '';

  // Look up template (parameter definitions from ScriptEngine.cpp)
  const tmpl = (typeof SCRIPT_PARAMS !== 'undefined') ? SCRIPT_PARAMS[iname] : null;
  const paramTypes = tmpl?.p || [];   // e.g. ['COUNTER','REAL']
  const uiStrings  = tmpl?.u || [];   // e.g. ['Set timer ',' to expire in ',' seconds.']

  // Show the action label / sentence preview
  if (preview) {
    if (uiStrings.length > 0) {
      // Build sentence: ui[0] __ ui[1] __ ...
      let sentence = '';
      uiStrings.forEach((s, i) => {
        sentence += s;
        if (i < paramTypes.length) sentence += '[' + paramTypes[i] + ']';
      });
      preview.textContent = sentence;
    } else {
      preview.textContent = label;
    }
  }

  if (paramTypes.length === 0 && (!existingParams || existingParams.length === 0)) {
    paramsEl.innerHTML = '<div style="color:var(--text-hint);font-size:11px;padding:8px 0">No parameters</div>';
    return;
  }

  // Build one input per parameter
  const count = Math.max(paramTypes.length, existingParams?.length || 0);
  for (let i = 0; i < count; i++) {
    const ptype  = paramTypes[i] || 'TEXT_STRING';
    const pval   = existingParams?.[i]?.value ?? '';
    const prefix = uiStrings[i] ? uiStrings[i].trim() : ('Param ' + (i+1));
    const suffix = uiStrings[i+1] || '';

    const row = document.createElement('div');
    row.className = 'msc-param-row';

    const lbl = document.createElement('label');
    // Show ptype as fallback label only if prefix is truly absent; trim empty UI strings to nothing
    lbl.textContent = prefix || '';
    lbl.title = ptype;

    const isPercentInt = ptype === 'INT' && suffix.toLowerCase().includes('percent');
    const ctrl = isPercentInt
      ? _makePercentSlider(pval, i)
      : _makeParamInput(ptype, pval, i);
    ctrl.addEventListener('change', scpValidateOk);
    ctrl.addEventListener('input',  scpValidateOk);

    row.append(lbl, ctrl);

    // ? hint badge for free-text types
    const hint = PARAM_TYPE_HINTS[ptype];
    if (hint && !_DROPDOWN_TYPES.has(ptype)) {
      const badge = document.createElement('span');
      badge.textContent = '?';
      badge.title = hint;
      badge.style.cssText = 'flex-shrink:0;width:16px;height:16px;border-radius:50%;background:var(--accent,#3b82f6);color:#fff;font-size:10px;font-weight:bold;display:inline-flex;align-items:center;justify-content:center;cursor:help;margin-left:2px;opacity:0.8';
      row.appendChild(badge);
    }

    // + / ↺ button for dropdown types that need map data
    const plusBtn = _scpMakePlusBtn(ptype, i);
    if (plusBtn) row.appendChild(plusBtn);

    if (suffix.trim()) {
      const sfx = document.createElement('span');
      sfx.textContent = suffix.trim();
      sfx.style.cssText = 'font-size:10px;color:var(--text-hint);flex-shrink:0';
      row.appendChild(sfx);
    }
    paramsEl.appendChild(row);
  }
  scpValidateOk();
}

// Returns a + action button for a param type, or null if not applicable.
// idx is the param index so we can auto-select the newly created value.
function _scpMakePlusBtn(ptype, idx) {
  const isTeam     = ptype === 'TEAM';
  const isWaypoint = ['WAYPOINT','WAYPOINT_PATH','SKIRMISH_WAYPOINT_PATH'].includes(ptype);
  const isUnit     = ptype === 'UNIT';
  const isArea     = ptype === 'TRIGGER_AREA';
  const isBridge   = ptype === 'BRIDGE';
  if (!isTeam && !isWaypoint && !isUnit && !isArea && !isBridge) return null;

  const btn = document.createElement('button');
  btn.className = 'scp-plus-btn';
  btn.textContent = '+';

  if (isTeam) {
    btn.title = 'Create a new team — auto-fills this field';
    btn.addEventListener('click', async () => {
      const name = prompt('New team name:');
      if (!name || !name.trim()) return;
      const player = _mscSelPlayer;
      if (player === null || player === undefined) return;
      const trimmed = name.trim();
      await api('/sidelist/team', { player, name: trimmed, active: true });
      await fetchSideList();
      // Rebuild params, auto-selecting the new team in this slot
      const cur = mscGetCondFromModal();
      if (!cur.params[idx]) cur.params[idx] = {};
      cur.params[idx] = { type: 'TEAM', value: trimmed };
      scpShowParams(cur.params);
    });
  } else if (isWaypoint) {
    const wpMode = (ptype === 'WAYPOINT_PATH' || ptype === 'SKIRMISH_WAYPOINT_PATH') ? 'path' : 'point';
    btn.title = wpMode === 'path'
      ? 'Go to Waypoints panel (path mode) — create a path, then return'
      : 'Go to Waypoints panel — place a waypoint, then return';
    btn.addEventListener('click', async () => {
      // Snapshot existing path names so we can detect the new one on return
      const existingPaths = new Set();
      for (const wp of (_scpWaypointCache || []))
        for (const p of (wp.paths || [])) if (p) existingPaths.add(p);

      _scpPendingReturn = {
        mode: _mscCondModalMode,
        selType: _mscSelCatType,
        params: mscGetCondFromModal().params,
        autoSelectIdx: idx,             // which param slot to auto-fill
        autoSelectPtype: ptype,         // WAYPOINT or WAYPOINT_PATH
        existingPaths,                  // to detect the newly added path
      };
      document.getElementById('msc-cond-modal').classList.add('hidden');
      document.getElementById('msc-script-modal').style.pointerEvents = '';
      document.getElementById('msc-script-modal').classList.add('hidden');
      document.getElementById('modal-scripts').classList.add('hidden');
      document.getElementById('modal-scripts').style.pointerEvents = '';
      switchPanel('waypoints');
      setWpMode(wpMode);
      await api('/tool', { tool: 'waypoints' });
      _scpShowReturnBanner();
    });
  } else if (isUnit) {
    btn.title = 'Go to Objects panel to place a named object — your progress is saved';
    btn.addEventListener('click', async () => {
      _scpPendingReturn = {
        mode: _mscCondModalMode,
        selType: _mscSelCatType,
        params: mscGetCondFromModal().params,
        autoSelectIdx: idx,
        autoSelectPtype: ptype,
        existingObjects: [...(_scpObjectCache || [])],
      };
      document.getElementById('msc-cond-modal').classList.add('hidden');
      document.getElementById('msc-script-modal').style.pointerEvents = '';
      document.getElementById('msc-script-modal').classList.add('hidden');
      document.getElementById('modal-scripts').classList.add('hidden');
      document.getElementById('modal-scripts').style.pointerEvents = '';
      switchPanel('objects');
      await api('/tool', { tool: 'pointer' });
      _scpShowReturnBanner();
    });
  } else if (isArea) {
    btn.title = 'Go to Waypoints panel (Area mode) — create a trigger area, then return';
    btn.addEventListener('click', async () => {
      const existingAreaNames = new Set((_scpTriggerCache || []).map(t => t.name));
      _scpPendingReturn = {
        mode: _mscCondModalMode,
        selType: _mscSelCatType,
        params: mscGetCondFromModal().params,
        autoSelectIdx: idx,
        autoSelectPtype: ptype,
        existingAreaNames,
      };
      document.getElementById('msc-cond-modal').classList.add('hidden');
      document.getElementById('msc-script-modal').style.pointerEvents = '';
      document.getElementById('msc-script-modal').classList.add('hidden');
      document.getElementById('modal-scripts').classList.add('hidden');
      document.getElementById('modal-scripts').style.pointerEvents = '';
      switchPanel('waypoints');
      setWpMode('area');
      await api('/tool', { tool: 'waypoints' });
      _scpShowReturnBanner();
    });
  } else if (isBridge) {
    btn.title = 'Go to Roads & Bridges panel — place a bridge, then return';
    btn.addEventListener('click', async () => {
      _scpPendingReturn = {
        mode: _mscCondModalMode,
        selType: _mscSelCatType,
        params: mscGetCondFromModal().params,
        autoSelectIdx: idx,
        autoSelectPtype: ptype,
        existingObjects: [...(_scpObjectCache || [])],
      };
      document.getElementById('msc-cond-modal').classList.add('hidden');
      document.getElementById('msc-script-modal').style.pointerEvents = '';
      document.getElementById('msc-script-modal').classList.add('hidden');
      document.getElementById('modal-scripts').classList.add('hidden');
      document.getElementById('modal-scripts').style.pointerEvents = '';
      _rdKind = 'bridge'; // rdInitPanel reads this, no race condition
      switchPanel('roads');
      _scpShowReturnBanner();
    });
  }
  return btn;
}

let _scpPendingReturn = null; // saved state while user creates a waypoint/object

function _scpShowReturnBanner() {
  const old = document.getElementById('scp-return-banner');
  if (old) old.remove();

  const bar = document.createElement('div');
  bar.id = 'scp-return-banner';
  bar.innerHTML = `
    <span>Add your item in the map, then click:</span>
    <button id="scp-return-btn" class="action-btn primary" style="padding:4px 14px;flex:unset">&#8617; Back to Script Editor</button>
  `;
  document.getElementById('toolbar').appendChild(bar);

  document.getElementById('scp-return-btn').addEventListener('click', async () => {
    bar.remove();
    // Force-refresh all caches so new items appear in dropdowns
    _scpWaypointCache = null;
    _scpObjectCache   = null;
    _scpTriggerCache  = null;
    const [r1, r2, r3] = await Promise.all([apiGet('/map/waypoints'), apiGet('/map/objects'), apiGet('/map/triggers')]);
    _scpWaypointCache = r1?.waypoints || [];
    _scpObjectCache   = (r2?.objects  || []).filter(o => o.name && o.name.trim());
    _scpTriggerCache  = (r3?.triggers || []).filter(t => t.name);
    await fetchSideList();

    const saved = _scpPendingReturn;
    _scpPendingReturn = null;

    // Auto-select newly added item in the right param slot
    if (saved.autoSelectIdx !== undefined) {
      const pt = saved.autoSelectPtype;
      if (pt === 'WAYPOINT_PATH' || pt === 'SKIRMISH_WAYPOINT_PATH') {
        // Find path names that didn't exist before
        const newPaths = [];
        for (const wp of _scpWaypointCache)
          for (const p of (wp.paths || [])) if (p && !saved.existingPaths.has(p)) newPaths.push(p);
        if (newPaths.length > 0) {
          saved.params[saved.autoSelectIdx] = { type: pt, value: newPaths[0] };
        }
      } else if (pt === 'WAYPOINT') {
        // Find waypoint names that didn't exist before
        const oldNames = new Set([...(saved.existingPaths)]);
        const newWp = _scpWaypointCache.find(w => w.name && !oldNames.has(w.name));
        if (newWp) saved.params[saved.autoSelectIdx] = { type: pt, value: newWp.name };
      } else if (pt === 'UNIT' || pt === 'BRIDGE') {
        const oldObjs = new Set((saved.existingObjects || []).map(o => o.name));
        const newObj = _scpObjectCache.find(o => o.name && !oldObjs.has(o.name));
        if (newObj) saved.params[saved.autoSelectIdx] = { type: pt, value: newObj.name };
      } else if (pt === 'TRIGGER_AREA') {
        const newArea = _scpTriggerCache.find(t => t.name && !saved.existingAreaNames?.has(t.name));
        if (newArea) saved.params[saved.autoSelectIdx] = { type: pt, value: newArea.name };
      }
    }

    // Re-open both the script modal and the condition picker
    document.getElementById('modal-scripts').classList.remove('hidden');
    document.getElementById('msc-script-modal').classList.remove('hidden');
    document.getElementById('msc-script-modal').style.pointerEvents = '';
    mscRenderCondList();
    mscRenderActList('true');
    mscRenderActList('false');

    await mscOpenCondModal(saved.mode, { type: saved.selType, params: saved.params }, _mscCondModalCb);
  });
}

// Disable OK when required dropdown params are not yet selected.
function scpValidateOk() {
  const okBtn = document.getElementById('btn-msc-cond-ok');
  if (!okBtn) return;
  let allFilled = true;
  document.querySelectorAll('#msc-cond-params .msc-param-row select').forEach(sel => {
    if (!sel.value) allFilled = false;
  });
  okBtn.disabled = !allFilled;
  okBtn.title = allFilled ? '' : 'Fill in all required fields first';
}

function scpConfirm() {
  if (_mscCondModalCb) { _mscCondModalCb(mscGetCondFromModal()); _mscCondModalCb = null; }
  const closeCond = () => {
    document.getElementById('msc-cond-modal').classList.add('hidden');
    document.getElementById('msc-script-modal').style.pointerEvents = '';
  };
  closeCond();
}

// Legacy alias used by old code
function mscBuildCatTree() {}
function mscSelectCatType(type, existingParams) {
  _mscSelCatType = type;
  scpShowParams(existingParams);
}

function mscGetCondFromModal() {
  const type = _mscSelCatType;
  const params = [];
  document.querySelectorAll('#msc-cond-params .msc-param-row [data-idx]').forEach(inp => {
    params.push({ type: inp.dataset.ptype || 'TEXT_STRING', value: inp.value });
  });
  return { type, params };
}

// ── Save functions ────────────────────────────────────────────────────────────
async function mscSaveGroup() {
  const name   = document.getElementById('msc-g-name').value.trim();
  const active = document.getElementById('msc-g-active').checked;
  if (!name || !_mscSelPlayer) return;
  const r = await api('/sidelist/group', { player: _mscSelPlayer, name, active });
  if (r?.ok) { _mscExpandedGroups.add(name); await mscRefreshAfterChange(); }
}

async function mscSaveScript() {
  if (!_mscSelScript || !_mscSelGroup || _mscSelPlayer === null || _mscSelPlayer === undefined) {
    mscStatus('Cannot save: no script/group/player selected', true); return;
  }
  const name      = document.getElementById('msc-s-name').value.trim() || _mscSelScript.name;
  const active    = document.getElementById('msc-s-active').checked;
  const sub       = document.getElementById('msc-s-sub').checked;
  const oneshot   = document.getElementById('msc-s-oneshot').checked;
  const easy      = document.getElementById('msc-s-easy').checked;
  const normal    = document.getElementById('msc-s-normal').checked;
  const hard      = document.getElementById('msc-s-hard').checked;
  const evalSecs  = document.getElementById('msc-s-eval-secs').checked
    ? (parseFloat(document.getElementById('msc-s-eval-n').value) || 1) : 0;

  const body = {
    player: _mscSelPlayer, group: _mscSelGroup.name,
    name, active, subroutine: sub, oneShot: oneshot,
    easy, normal, hard,
    evalSecs,
    condCount: 0, actCount: 0, actFalseCount: 0,
  };

  // Flatten conditions
  let ci = 0;
  _mscEditConds.forEach((orGroup, oi) => {
    (orGroup || []).forEach((cond, ai) => {
      body['cond' + ci + '_or']     = oi;
      body['cond' + ci + '_type']   = cond.type;
      body['cond' + ci + '_pCount'] = (cond.params || []).length;
      (cond.params || []).forEach((p, pi) => {
        body['cond' + ci + '_p' + pi + '_pt'] = p.type || 'TEXT_STRING';
        body['cond' + ci + '_p' + pi + '_v']  = String(p.value || '');
      });
      ci++;
    });
  });
  body.condCount = ci;

  // Flatten true actions
  _mscEditActT.forEach((act, ai) => {
    body['act' + ai + '_type']   = act.type;
    body['act' + ai + '_pCount'] = (act.params || []).length;
    (act.params || []).forEach((p, pi) => {
      body['act' + ai + '_p' + pi + '_pt'] = p.type || 'TEXT_STRING';
      body['act' + ai + '_p' + pi + '_v']  = String(p.value || '');
    });
  });
  body.actCount = _mscEditActT.length;

  // Flatten false actions
  _mscEditActF.forEach((act, ai) => {
    body['actF' + ai + '_type']   = act.type;
    body['actF' + ai + '_pCount'] = (act.params || []).length;
    (act.params || []).forEach((p, pi) => {
      body['actF' + ai + '_p' + pi + '_pt'] = p.type || 'TEXT_STRING';
      body['actF' + ai + '_p' + pi + '_v']  = String(p.value || '');
    });
  });
  body.actFalseCount = _mscEditActF.length;

  const r = await api('/sidelist/script', body);
  if (r?.ok) {
    mscCloseScriptModal();
    await mscRefreshAfterChange();
    const cb = _mscOnSaveCallback;
    _mscOnSaveCallback = null;
    if (cb) cb(name);
  } else {
    mscStatus('Save failed: ' + (r?.error || 'WB returned error'), true);
  }
}

// ── Status message in tree ────────────────────────────────────────────────────
function mscStatus(msg, isError = false) {
  const el = document.getElementById('msc-tree-status');
  if (!el) return;
  el.textContent = msg;
  el.style.color = isError ? '#e74c3c' : 'var(--text-hint)';
  // Errors stay until cleared; non-errors auto-clear after 4s
  if (msg && !isError) setTimeout(() => { if (el.textContent === msg) el.textContent = ''; }, 4000);
}

// Catch unhandled JS errors and show them in the scripts status div
window.addEventListener('error', e => {
  console.error('[renderer error]', e.message, e.filename, e.lineno);
  mscStatus('JS Error: ' + e.message, true);
});
window.addEventListener('unhandledrejection', e => {
  console.error('[unhandled promise]', e.reason);
  mscStatus('JS Error: ' + (e.reason?.message || String(e.reason)), true);
});

// ── Get current player name — set by clicking a player row in the tree ───────
function mscGetPlayer() {
  if (_mscSelPlayer !== null && _mscSelPlayer !== undefined) return _mscSelPlayer;
  const p0 = _sidelistData?.players?.[0];
  if (p0) return p0.name ?? '';
  return null;
}

// ── Refresh after pipe command (PostMessage is async — need to wait for WB) ───
async function mscRefreshAfterChange(delay = 500) {
  await new Promise(r => setTimeout(r, delay));
  await fetchSideList();
  mscRenderTree();
  if (_mscSelGroup) {
    const player = (_sidelistData?.players || []).find(p => p.name === _mscSelPlayer);
    const group  = (player?.scriptGroups || []).find(g => g.name === _mscSelGroup.name);
    if (group) { _mscSelGroup = group; mscUpdateSelDesc(); }
  }
}

// ── Inline name input (prompt() doesn't work in Electron) ────────────────────
let _mscInlineMode = null; // 'folder' | 'script'

function mscShowInlineInput(mode) {
  _mscInlineMode = mode;
  const box = document.getElementById('msc-inline-input');
  const lbl = document.getElementById('msc-inline-label');
  const inp = document.getElementById('msc-inline-name');
  if (lbl) lbl.textContent = mode === 'folder' ? 'New folder name:' : 'New script name:';
  box.style.display = '';
  inp.value = '';
  inp.placeholder = mode === 'folder' ? 'MyScriptGroup' : 'MyScript';
  setTimeout(() => inp.focus(), 30);
  inp.onkeydown = e => { if (e.key === 'Enter') mscInlineOk(); if (e.key === 'Escape') mscHideInlineInput(); };
}

function mscHideInlineInput() {
  document.getElementById('msc-inline-input').style.display = 'none';
  _mscInlineMode = null;
}

async function mscInlineOk() {
  const name = document.getElementById('msc-inline-name').value.trim();
  const mode = _mscInlineMode;
  mscHideInlineInput();
  if (!name) return;
  if (mode === 'folder') await mscCreateFolder(name);
  else if (mode === 'script') await mscCreateScript(name);
}

async function mscCreateFolder(name) {
  const player = mscGetPlayer();
  // player can be "" for the neutral player — only block if truly null/undefined
  if (player === null || player === undefined) {
    mscStatus('No player selected', true); return;
  }
  mscStatus('Creating folder...');
  const r = await api('/sidelist/group', { player, name, active: true });
  console.log('[msc] create folder', name, 'player:', JSON.stringify(player), '→', r);
  if (!r?.ok) { mscStatus('Failed: ' + (r?.error || (r ? 'WB returned error' : 'pipe not available')), true); return; }
  mscStatus('Folder created, reloading...');
  _mscExpandedGroups.add(name);
  _mscPlayerExpanded = true;
  await mscRefreshAfterChange();
  const players = _sidelistData?.players || [];
  const p2    = players.find(p => p.name === _mscSelPlayer) || players[0];
  const group = (p2?.scriptGroups || []).find(g => g.name === name);
  if (group) { _mscSelGroup = group; mscUpdateSelDesc(); }
  mscStatus('');
}

async function mscCreateScript(name) {
  const player = mscGetPlayer();
  if (player === null || player === undefined) {
    mscStatus('No player selected', true); return;
  }
  mscStatus('Creating script...');

  // Auto-create "Scripts" folder if no groups exist
  let p      = (_sidelistData?.players || []).find(p => p.name === player);
  let groups = p?.scriptGroups || [];
  if (!groups.length) {
    const fg = await api('/sidelist/group', { player, name: 'Scripts', active: true });
    console.log('[msc] auto-create folder:', fg);
    await new Promise(r => setTimeout(r, 500));
    await fetchSideList();
    p      = (_sidelistData?.players || []).find(p => p.name === player);
    groups = p?.scriptGroups || [];
  }

  const groupName = _mscSelGroup?.name || groups[0]?.name || 'Scripts';
  const r = await api('/sidelist/script', {
    player, group: groupName, name,
    active: true, easy: true, normal: true, hard: true,
    oneShot: true, subroutine: false,
    condCount: 0, actCount: 0, actFalseCount: 0,
  });
  console.log('[msc] create script', name, 'group', groupName, '→', r);
  if (!r?.ok) { mscStatus('Failed: ' + (r?.error || (r ? 'WB returned error' : 'pipe not available')), true); return; }
  mscStatus('Script created, reloading...');
  _mscExpandedGroups.add(groupName);
  _mscPlayerExpanded = true;
  await mscRefreshAfterChange();
  const p2 = (_sidelistData?.players || []).find(p => p.name === _mscSelPlayer);
  const g2 = (p2?.scriptGroups || []).find(g => g.name === groupName);
  const s2 = (g2?.scripts || []).find(s => s.name === name);
  if (g2 && s2) { mscSelectScript(g2, s2); mscOpenScriptModal(s2); }
  else if (g2)  { _mscSelGroup = g2; }
  mscStatus('');
}

async function mscDeleteSelected() {
  if (_mscSelScript && _mscSelGroup) {
    const r = await api('/sidelist/script/delete', {
      player: _mscSelPlayer, group: _mscSelGroup.name, name: _mscSelScript.name
    });
    if (r?.ok) {
      _mscSelScript = null;
      await mscRefreshAfterChange();
      mscUpdateSelDesc();
    }
  }
}

// ── Condition/Action list editing helpers ────────────────────────────────────
function mscCondNew() {
  mscOpenCondModal('cond', null, cond => {
    if (!_mscEditConds.length) _mscEditConds.push([]);
    _mscEditConds[_mscEditConds.length - 1].push(cond);
    mscRenderCondList();
  });
}
function mscCondOr() {
  _mscEditConds.push([{ type: 3, params: [] }]);
  mscRenderCondList();
}
function mscCondDel() {
  if (!_mscSelCondKey) return;
  const [oi, ai] = _mscSelCondKey.split(':').map(Number);
  if (_mscEditConds[oi]) {
    _mscEditConds[oi].splice(ai, 1);
    if (!_mscEditConds[oi].length) _mscEditConds.splice(oi, 1);
  }
  _mscSelCondKey = null;
  mscRenderCondList();
}
function mscCondEdit() {
  if (!_mscSelCondKey) return;
  const [oi, ai] = _mscSelCondKey.split(':').map(Number);
  const cond = _mscEditConds[oi]?.[ai];
  if (!cond) return;
  mscOpenCondModal('cond', cond, updated => {
    _mscEditConds[oi][ai] = updated;
    mscRenderCondList();
  });
}
function mscActNew(which) {
  mscOpenCondModal('act', null, act => {
    if (which === 'true') _mscEditActT.push(act);
    else _mscEditActF.push(act);
    mscRenderActList(which);
  });
}
function mscActDel(which) {
  const key = which === 'true' ? _mscSelActTKey : _mscSelActFKey;
  if (key === null) return;
  const idx = parseInt(key);
  if (which === 'true') { _mscEditActT.splice(idx, 1); _mscSelActTKey = null; }
  else                  { _mscEditActF.splice(idx, 1); _mscSelActFKey = null; }
  mscRenderActList(which);
}
function mscActEdit(which) {
  const key = which === 'true' ? _mscSelActTKey : _mscSelActFKey;
  if (key === null) return;
  const idx = parseInt(key);
  const act = which === 'true' ? _mscEditActT[idx] : _mscEditActF[idx];
  if (!act) return;
  mscOpenCondModal('act', act, updated => {
    if (which === 'true') _mscEditActT[idx] = updated;
    else _mscEditActF[idx] = updated;
    mscRenderActList(which);
  });
}
function mscMove(which, dir) {
  const key = which === 'cond' ? _mscSelCondKey
    : which === 'true' ? _mscSelActTKey : _mscSelActFKey;
  if (key === null) return;
  if (which === 'cond') {
    const [oi, ai] = key.split(':').map(Number);
    const arr = _mscEditConds[oi];
    if (!arr) return;
    const ni = ai + dir;
    if (ni < 0 || ni >= arr.length) return;
    [arr[ai], arr[ni]] = [arr[ni], arr[ai]];
    _mscSelCondKey = oi + ':' + ni;
    mscRenderCondList();
  } else {
    const arr = which === 'true' ? _mscEditActT : _mscEditActF;
    const idx = parseInt(key);
    const ni  = idx + dir;
    if (ni < 0 || ni >= arr.length) return;
    [arr[idx], arr[ni]] = [arr[ni], arr[idx]];
    if (which === 'true') _mscSelActTKey = String(ni);
    else _mscSelActFKey = String(ni);
    mscRenderActList(which);
  }
}

// ── Inner tab switching (script sub-modal) ────────────────────────────────────
function initScriptEditorTabs() {
  document.querySelectorAll('#msc-script-modal-box .ms-inner-tab').forEach(btn => {
    btn.addEventListener('click', () => {
      const tab = btn.dataset.stab;
      document.querySelectorAll('#msc-script-modal-box .ms-inner-tab').forEach(b => b.classList.toggle('active', b === btn));
      document.querySelectorAll('#msc-script-modal-box .ms-inner-body').forEach(b => b.classList.toggle('hidden', b.id !== 'stab-' + tab));
    });
  });
}

// ── Wizard helpers for modal ──────────────────────────────────────────────────
let _mscWizTemplate = null;

function mscShowWizardFields(tpl) {
  _mscWizTemplate = tpl;
  const container = document.getElementById('msc-wiz-fields');
  container.innerHTML = '';

  const titles = { spawn:'Spawn Units', win:'Victory', lose:'Defeat', timer:'Timer',
    area:'Area Trigger', enable:'Enable/Disable Script', camera:'Camera Path', counter:'Counter' };
  document.getElementById('msc-wiz-title').textContent = (titles[tpl] || tpl) + ' Wizard';

  // Shared: script name
  const mkRow = (lbl, html) => {
    const row = document.createElement('div');
    row.className = 'msc-field-row'; row.style.marginBottom = '6px';
    row.innerHTML = `<label class="msc-label" style="width:110px">${lbl}</label>${html}`;
    container.appendChild(row);
    return row;
  };
  mkRow('Script Name', `<input type="text" class="msc-input" id="msc-wiz-name" style="flex:1" placeholder="MyScript">`);

  if (tpl === 'spawn') {
    mkRow('Team',     `<input type="text" class="msc-input" id="msc-wiz-team"     style="flex:1" placeholder="teamSkirmishGLA">`);
    mkRow('Waypoint', `<input type="text" class="msc-input" id="msc-wiz-wp"       style="flex:1" placeholder="SpawnPoint_1">`);
  } else if (tpl === 'win') {
    mkRow('Condition', `<input type="text" class="msc-input" id="msc-wiz-cond" style="flex:1" placeholder="(always true)">`);
  } else if (tpl === 'lose') {
    mkRow('Trigger Area', `<input type="text" class="msc-input" id="msc-wiz-area" style="flex:1" placeholder="LoseZone">`);
  } else if (tpl === 'timer') {
    mkRow('Timer Name', `<input type="text" class="msc-input" id="msc-wiz-timer" style="flex:1" placeholder="WaveTimer">`);
    mkRow('Seconds',    `<input type="number" class="msc-input" id="msc-wiz-secs" style="width:80px" value="30">`);
  } else if (tpl === 'area') {
    mkRow('Area Name', `<input type="text" class="msc-input" id="msc-wiz-area" style="flex:1" placeholder="MyTriggerArea">`);
    mkRow('Team',      `<input type="text" class="msc-input" id="msc-wiz-team" style="flex:1" placeholder="teamSkirmishGLA">`);
  } else if (tpl === 'enable') {
    mkRow('Script to toggle', `<input type="text" class="msc-input" id="msc-wiz-target" style="flex:1" placeholder="OtherScript">`);
    mkRow('Action', `<select class="msc-input" id="msc-wiz-action" style="width:120px">
      <option value="enable">Enable</option><option value="disable">Disable</option></select>`);
  } else if (tpl === 'counter') {
    mkRow('Counter Name', `<input type="text" class="msc-input" id="msc-wiz-counter" style="flex:1" placeholder="WaveCount">`);
    mkRow('Value',        `<input type="number" class="msc-input" id="msc-wiz-val" style="width:80px" value="1">`);
  }
}

async function mscApplyWizard() {
  const tpl    = _mscWizTemplate;
  const player = document.getElementById('msc-wiz-player')?.value;
  const name   = document.getElementById('msc-wiz-name')?.value?.trim();
  const fb     = document.getElementById('msc-wiz-feedback');
  if (!name) { fb.textContent = 'Enter a script name.'; return; }
  if (!player) { fb.textContent = 'Select a player.'; return; }

  // Ensure group exists
  await api('/sidelist/group', { player, name: 'Script Group 1', active: true });

  const body = {
    player, group: 'Script Group 1', name,
    active: true, easy: true, normal: true, hard: true,
    oneShot: true, subroutine: false,
    condCount: 0, actCount: 0, actFalseCount: 0,
  };

  // Add default condition (True) and action based on template
  body.condCount = 1;
  body['cond0_or']   = 0; body['cond0_type'] = 3; body['cond0_pCount'] = 0;

  if (tpl === 'win') {
    body.actCount = 1;
    body['act0_type'] = 6; body['act0_pCount'] = 1;
    body['act0_p0_pt'] = 'PLAYER'; body['act0_p0_v'] = player;
  } else if (tpl === 'lose') {
    body.actCount = 1;
    body['act0_type'] = 7; body['act0_pCount'] = 1;
    body['act0_p0_pt'] = 'PLAYER'; body['act0_p0_v'] = player;
  }

  const r = await api('/sidelist/script', body);
  if (r?.ok) {
    fb.textContent = 'Script "' + name + '" created!';
    await fetchSideList();
    mscSyncWizardPlayers();
    setTimeout(() => { fb.textContent = ''; }, 3000);
  } else {
    fb.textContent = 'Error: ' + (r?.error || 'unknown');
  }
}

// ── Wire everything up ────────────────────────────────────────────────────────
function initScriptsModal() {
  document.getElementById('btn-modal-scripts-close').addEventListener('click', closeScriptsModal);
  document.getElementById('modal-scripts').addEventListener('click', e => {
    if (e.target.id === 'modal-scripts') closeScriptsModal();
  });
  document.getElementById('btn-msc-refresh').addEventListener('click', async () => {
    document.getElementById('msc-tree-status').textContent = 'Refreshing...';
    await fetchSideList();
    mscRefreshPlayerSel();
  });

  // ── Player selector ──────────────────────────────────────────────────────
  document.getElementById('msc-player-sel').addEventListener('change', () => {
    _mscSelGroup = null; _mscSelScript = null;
    _mscExpandedGroups.clear();
    _mscPlayerExpanded = true;
    mscUpdateSelDesc();
    mscRenderTree();
  });

  initScriptEditorTabs();

  // Inline input (tree bottom)
  document.getElementById('btn-msc-inline-ok').addEventListener('click', mscInlineOk);
  document.getElementById('btn-msc-inline-cancel').addEventListener('click', mscHideInlineInput);
  // Right-column action buttons
  document.getElementById('btn-msc-new-folder').addEventListener('click', () => {
    // Auto-name: "Script Group 1", "Script Group 2", etc. — same as OG WB
    const player = (_sidelistData?.players || []).find(p => p.name === mscGetPlayer());
    const groups = player?.scriptGroups || [];
    const autoName = 'Script Group ' + (groups.length + 1);
    mscCreateFolder(autoName);
  });
  document.getElementById('btn-msc-new-script').addEventListener('click', () => {
    // Auto-name: "Script1", "Script2", … — count all scripts across all groups for this player
    const player = (_sidelistData?.players || []).find(p => p.name === mscGetPlayer());
    const total  = (player?.scriptGroups || []).reduce((n, g) => n + (g.scripts?.length || 0), 0);
    mscCreateScript('Script' + (total + 1));
  });
  document.getElementById('btn-msc-edit-script').addEventListener('click', mscEditSelected);
  document.getElementById('btn-msc-delete').addEventListener('click', mscDeleteSelected);
  document.getElementById('btn-msc-copy-script').addEventListener('click', async () => {
    if (!_mscSelScript || !_mscSelGroup) return;
    const s = _mscSelScript;
    const r = await api('/sidelist/script', {
      player: _mscSelPlayer, group: _mscSelGroup.name, name: s.name + '_copy',
      active: s.active, subroutine: s.subroutine, oneShot: s.oneShot,
      easy: s.easy, normal: s.normal, hard: s.hard,
      condCount: 0, actCount: 0, actFalseCount: 0,
    });
    if (r?.ok) await mscRefreshAfterChange();
  });

  // Script sub-modal
  document.getElementById('btn-msc-script-modal-close').addEventListener('click', mscCloseScriptModal);
  document.getElementById('btn-msc-cancel-script').addEventListener('click', mscCloseScriptModal);
  document.getElementById('btn-msc-save-script').addEventListener('click', mscSaveScript);
  document.getElementById('btn-msc-del-script').addEventListener('click', async () => {
    await mscDeleteSelected();
    mscCloseScriptModal();
  });

  // Conditions buttons
  document.getElementById('btn-msc-cond-new').addEventListener('click', mscCondNew);
  document.getElementById('btn-msc-cond-edit').addEventListener('click', mscCondEdit);
  document.getElementById('btn-msc-cond-copy').addEventListener('click', () => {
    if (!_mscSelCondKey) return;
    const [oi, ai] = _mscSelCondKey.split(':').map(Number);
    const copy = JSON.parse(JSON.stringify(_mscEditConds[oi]?.[ai] || {type:3,params:[]}));
    _mscEditConds[oi].push(copy); mscRenderCondList();
  });
  document.getElementById('btn-msc-cond-del').addEventListener('click', mscCondDel);
  document.getElementById('btn-msc-cond-or').addEventListener('click', mscCondOr);
  document.getElementById('btn-msc-cond-up').addEventListener('click', () => mscMove('cond', -1));
  document.getElementById('btn-msc-cond-down').addEventListener('click', () => mscMove('cond', 1));

  // True actions
  document.getElementById('btn-msc-atrue-new').addEventListener('click', () => mscActNew('true'));
  document.getElementById('btn-msc-atrue-edit').addEventListener('click', () => mscActEdit('true'));
  document.getElementById('btn-msc-atrue-copy').addEventListener('click', () => {
    if (_mscSelActTKey === null) return;
    _mscEditActT.push(JSON.parse(JSON.stringify(_mscEditActT[parseInt(_mscSelActTKey)])));
    mscRenderActList('true');
  });
  document.getElementById('btn-msc-atrue-del').addEventListener('click', () => mscActDel('true'));
  document.getElementById('btn-msc-atrue-up').addEventListener('click', () => mscMove('true', -1));
  document.getElementById('btn-msc-atrue-down').addEventListener('click', () => mscMove('true', 1));

  // False actions
  document.getElementById('btn-msc-afalse-new').addEventListener('click', () => mscActNew('false'));
  document.getElementById('btn-msc-afalse-edit').addEventListener('click', () => mscActEdit('false'));
  document.getElementById('btn-msc-afalse-copy').addEventListener('click', () => {
    if (_mscSelActFKey === null) return;
    _mscEditActF.push(JSON.parse(JSON.stringify(_mscEditActF[parseInt(_mscSelActFKey)])));
    mscRenderActList('false');
  });
  document.getElementById('btn-msc-afalse-del').addEventListener('click', () => mscActDel('false'));
  document.getElementById('btn-msc-afalse-up').addEventListener('click', () => mscMove('false', -1));
  document.getElementById('btn-msc-afalse-down').addEventListener('click', () => mscMove('false', 1));

  // Script picker (full-screen condition/action)
  const closePicker = () => {
    document.getElementById('msc-cond-modal').classList.add('hidden');
    document.getElementById('msc-script-modal').style.pointerEvents = '';
  };
  document.getElementById('btn-msc-cond-modal-close').addEventListener('click', closePicker);
  document.getElementById('btn-msc-cond-cancel').addEventListener('click', closePicker);
  document.getElementById('btn-msc-cond-ok').addEventListener('click', scpConfirm);

  // Tab switching inside picker
  document.getElementById('scp-tab-cond').addEventListener('click', () => {
    _mscCondModalMode = 'cond'; _scpSelCat = 'All';
    document.getElementById('scp-tab-cond').classList.add('active');
    document.getElementById('scp-tab-act').classList.remove('active');
    scpRender();
  });
  document.getElementById('scp-tab-act').addEventListener('click', () => {
    _mscCondModalMode = 'act'; _scpSelCat = 'All';
    document.getElementById('scp-tab-act').classList.add('active');
    document.getElementById('scp-tab-cond').classList.remove('active');
    scpRender();
  });
  document.getElementById('scp-search').addEventListener('input', scpRender);
  document.getElementById('scp-show-unused').addEventListener('change', scpRender);
}
