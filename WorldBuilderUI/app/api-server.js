/**
 * api-server.js
 * Local REST API on localhost:8099
 * Single control interface for UI buttons, AI agents, and external tools.
 *
 * Preferred channel: named pipe \\.\pipe\wb-engine (embedded mode)
 * Fallback channel:  WM_COMMAND via hwnd-bridge (legacy floating-window mode)
 */

const http = require('http');
const net  = require('net');
const fs   = require('fs');
const path = require('path');
// api-server.js is only ever required from main.js, so this runs in the main
// process - electron.app is directly available, no renderer/.remote concerns.
const { app } = require('electron');

// Same resolution as main.js's getAppDir(): the folder next to the actual
// launcher exe (portable build) or this source folder (dev). __dirname alone
// would point inside the read-only asar archive in a packaged build.
const APP_DIR = process.env.PORTABLE_EXECUTABLE_DIR
  ? process.env.PORTABLE_EXECUTABLE_DIR
  : (app.isPackaged ? path.dirname(process.execPath) : __dirname);
// Where to WRITE: main.js probes APP_DIR for writability at startup and exports
// the result (falls back to the per-user data dir when the game folder is a
// UAC-protected Program Files location - writing there is EPERM for normal users).
const DATA_DIR = process.env.WB_UI_DATA_DIR || APP_DIR;
// Bundled read-only assets (icons, textures, object-name lists) live outside this
// folder in the dev repo (parent dir) but get copied under resourcesPath by
// electron-builder's extraResources in a packaged build - see package.json.
const RESOURCES_DIR = app.isPackaged ? process.resourcesPath : path.join(__dirname, '..');

// ── File logger (always writes, even when started via start.bat) ──────────────
// createWriteStream does NOT throw synchronously on a read-only folder - it
// emits an async 'error' event, and an unhandled stream error is an UNCAUGHT
// EXCEPTION that takes down the whole app (seen in the wild as "EPERM: operation
// not permitted, open ...debug.log" on a machine where the game folder wasn't
// writable). Handle the error and degrade to console-only logging.
const LOG_PATH = path.join(DATA_DIR, 'debug.log');
let _logStream = null;
try {
  _logStream = fs.createWriteStream(LOG_PATH, { flags: 'a' });
  _logStream.on('error', () => { _logStream = null; });
} catch { _logStream = null; }
function dbg(...args) {
  const line = '[' + new Date().toISOString() + '] ' + args.join(' ');
  if (_logStream) { try { _logStream.write(line + '\n'); } catch {} }
  process.stdout.write(line + '\n');
}

// ── Open Map dialog — user-managed scan directories ────────────────────────────
// Replaces the old hardcoded MAP_SCAN_DIRS list with a persisted, user-editable one
// (add/remove folders from the "Open Map" dialog instead of editing source).
const SCAN_DIRS_PATH = path.join(DATA_DIR, 'data', 'map-scan-dirs.json');
// A packaged build sits directly in the user's game folder (APP_DIR), so its own
// Maps\ subfolder is a sensible universal default - unlike the old hardcoded list
// below, which only ever made sense on this dev machine. The dev-only extras stay
// so `npm start` here keeps finding this project's own test map corpora; they're
// meaningless (and just clutter/fail silently) on anyone else's machine, so they're
// only included when NOT packaged.
const DEFAULT_SCAN_DIRS = [
  path.join(APP_DIR, 'Maps'),
  ...(app.isPackaged ? [] : [
    'C:\\ProjectWorldbuilder',
    'C:\\ProjectWorldbuilder\\Maps dominator',
    'C:\\ProjectWorldbuilder\\EA_maps_original',
    'C:\\ProjectWorldbuilder\\decompressed_maps_REAL',
    'C:\\Program Files (x86)\\Origin Games\\Command and Conquer Generals Zero Hour\\Command and Conquer Generals Zero Hour\\Maps',
    'C:\\Users\\rudie\\OneDrive\\Documents 1\\Command and Conquer Generals Zero Hour Data\\Maps',
    'C:\\Users\\rudie\\Documents\\Command and Conquer Generals Zero Hour Data\\Maps',
  ]),
];
function getScanDirs() {
  try {
    return JSON.parse(fs.readFileSync(SCAN_DIRS_PATH, 'utf8'));
  } catch {
    // Seeding the default file is best-effort: on a machine where even DATA_DIR
    // writes fail, the dialog still works with in-memory defaults.
    try {
      fs.mkdirSync(path.dirname(SCAN_DIRS_PATH), { recursive: true });
      fs.writeFileSync(SCAN_DIRS_PATH, JSON.stringify(DEFAULT_SCAN_DIRS, null, 1));
    } catch {}
    return DEFAULT_SCAN_DIRS.slice();
  }
}
function saveScanDirs(dirs) {
  try {
    fs.mkdirSync(path.dirname(SCAN_DIRS_PATH), { recursive: true });
    fs.writeFileSync(SCAN_DIRS_PATH, JSON.stringify(dirs, null, 1));
  } catch (e) {
    dbg(`[scandirs] save failed (folder not writable?): ${e.message}`);
  }
}

// ── Object palette — parsed from REAL_OBJECT_NAMES.txt (1869 objects) ─────────
const REAL_NAMES_PATH = path.join(RESOURCES_DIR, 'REAL_OBJECT_NAMES.txt');
let _objectPaletteCache = null;

// Valid ZH templates — generated from INI.big + INIZH.big by tools/build_valid_objects.py.
// Names missing there are dead in Zero Hour (vanilla-only leftovers like
// AmericaDetentionCamp, or invented editor names like DatePalm) and are kept
// out of all palettes so they can never be placed.
const ZH_NAMES_PATH = path.join(__dirname, 'zh_object_names.txt');
let _zhValidSet; // undefined = not loaded yet, null = file missing (filtering off)
function isValidZhObject(name) {
  if (_zhValidSet === undefined) {
    try {
      _zhValidSet = new Set(fs.readFileSync(ZH_NAMES_PATH, 'utf8').split(/\r?\n/).filter(Boolean));
      dbg(`[palette] ${_zhValidSet.size} valid ZH object names loaded`);
    } catch {
      _zhValidSet = null;
      dbg('[palette] zh_object_names.txt missing - dead-object filtering disabled');
    }
  }
  return !_zhValidSet || _zhValidSet.has(name);
}

const FACTION_ORDER = ['USA','Air Force','Laser','Superweapon','China','Infantry','Nuclear','Tank','GLA','Demo','Stealth','Toxin','Boss','Special','Props','Civilian','Cutscene'];

// Vegetation regex — objects handled by the Nature/Vegetation panel, not the Objects palette
const VEGETATION_RE = /^(DatePalm|MesaDatePalm|PalmTree|TreePine|TreeFir|TreeSpruce|TreeOak|TreeMaple|TreeDogwood|TreeBirch|TreeWillow|TreeCherry|TreeCedar|TreeBurned|GenericTree|TreeXmas|Bush0[0-9]|RockCluster|Rocks[0-9]|RocksG[0-9]|Boulders|Haystack|GermanWindMill)/i;

function getObjectGroup(name) {
  // Vegetation — handled by Nature panel, hidden from Objects palette
  if (VEGETATION_RE.test(name))                                       return ['__vegetation__', name];

  // Civilian overrides — specific units that belong in civilian context
  if (/CIAOfficer|^AFG_America/i.test(name))                        return ['Civilian',    name];

  // Props overrides — effect/spawn/debris/prop objects regardless of faction prefix
  if (/DeadHull|RubbleHull|BurntRubble|MoundRubble|NukeCannonHulk|Hulk$/i.test(name)) return ['Props', name];
  if (/TankShell|CannonShell/i.test(name))                          return ['Props', name];
  if (/CrateParachute|AmericaParachute|LargeParachute/i.test(name)) return ['Props', name];
  if (/PointDefense(Drone|LaserBeam)|PointDefenseLaserBeam/i.test(name)) return ['Props', name];
  if (/BarrelDebris|RocketBuggyFullDebris|RocketBuggyMissileDebris/i.test(name)) return ['Props', name];
  if (/CargoPlane/i.test(name))                                      return ['Props', name];
  if (/HelixBlade|WindmillBlade/i.test(name))                        return ['Props', name];
  if (/AngryMob(Molotov|Pistol|Rock)/i.test(name))                  return ['Props', name];
  if (/^(GLALightTank|GLAToxinTank|GLAVehicleBattleBusHighDef)$/i.test(name)) return ['Props', name];
  // Wall structures
  if (/^(AmericaWall|ChinaWall|GLAWall|FortressWall|GreatWall)/i.test(name)) return ['Props', name];
  if (/WallHub$/i.test(name))                                        return ['Props', name];
  // Naval / special mission assets
  if (/AircraftCarrier|BattleShip|BattleshipBogusTarget/i.test(name)) return ['Props', name];
  if (/GuardianDrone|RepairDrone|POWTruck/i.test(name))              return ['Props', name];
  // Special Units — unusual characters that don't belong in regular faction lists
  if (/^AmericaInfantry(BiohazardTech|Officer|SecretService|NavySeal)$/i.test(name)) return ['Special', name];
  if (/^ChinaInfantry(Agent|SecretPolice|ParadeRedGuard|Officer)$/i.test(name))      return ['Special', name];
  if (/^GLAInfantry(VanDozer|JarmenKell|Militia|Rebel|Assassin)$/i.test(name))       return ['Special', name];
  if (/^(Infa_|Nuke_|Tank_)ChinaInfantry(Agent|SecretPolice|ParadeRedGuard)/i.test(name)) return ['Special', name];
  if (/^(Slth_|Chem_|Demo_)GLAInfantry(Saboteur|Assassin)/i.test(name))              return ['Special', name];
  if (/^CINE_/i.test(name))   return ['Cutscene',    name];
  if (/^AirF_/i.test(name))   return ['Air Force',   name];
  if (/^Lazr_/i.test(name))   return ['Laser',       name];
  if (/^SupW_/i.test(name))   return ['Superweapon', name];
  if (/^Boss_/i.test(name))   return ['Boss',        name];
  if (/^America|^AFG_/i.test(name)) return ['USA',   name];
  if (/^Infa_/i.test(name))   return ['Infantry',    name];
  if (/^Nuke_/i.test(name))   return ['Nuclear',     name];
  if (/^Tank_/i.test(name))   return ['Tank',        name];
  if (/^China/i.test(name))   return ['China',       name];
  if (/^Demo_/i.test(name))   return ['Demo',        name];
  if (/^Slth_/i.test(name))   return ['Stealth',     name];
  if (/^Chem_/i.test(name))   return ['Toxin',       name];
  if (/^GLA/i.test(name))     return ['GLA',         name];
  // GC_ = Generals Challenge variants — route to their nested faction
  if (/^GC_/i.test(name))     return getObjectGroup(name.slice(3));
  if (/^(Civilian|Car|Convoy|Police|Firetruck|Tractor|Tanker|Supply(?!Drop)|Train|Mogadishu)/i.test(name))
                               return ['Civilian',    name];
  if (/^Tech/i.test(name))    return ['Civilian',    name];
  return ['Props', name];
}

function buildObjectPalette() {
  if (_objectPaletteCache) return _objectPaletteCache;
  const palette = {};
  try {
    // Primary source: building_icons/*.png — skip topdown_ variants (those are icon-only renders)
    const iconFiles = fs.readdirSync(ICONS_DIR).filter(f => f.endsWith('.png') && !f.startsWith('topdown_'));
    for (const f of iconFiles) {
      const name = f.replace('.png', '');
      if (!isValidZhObject(name)) continue;
      const [faction] = getObjectGroup(name);
      if (!palette[faction])           palette[faction] = {};
      if (!palette[faction]['Objects']) palette[faction]['Objects'] = [];
      palette[faction]['Objects'].push(name);
    }

    // Secondary source: topdown_*.png — each represents a valid placeable object
    const existingSet = new Set(iconFiles.map(f => f.replace('.png', '')));
    const allFiles = fs.readdirSync(ICONS_DIR).filter(f => f.endsWith('.png'));
    for (const f of allFiles) {
      if (!f.startsWith('topdown_')) continue;
      const name = f.replace('.png', '').slice('topdown_'.length);
      if (!name || existingSet.has(name)) continue;
      existingSet.add(name);
      if (!isValidZhObject(name)) continue;
      const [faction] = getObjectGroup(name);
      if (!palette[faction])           palette[faction] = {};
      if (!palette[faction]['Objects']) palette[faction]['Objects'] = [];
      palette[faction]['Objects'].push(name);
    }

    // Tertiary: REAL_OBJECT_NAMES.txt for any remaining named objects without icons
    const content = fs.readFileSync(REAL_NAMES_PATH, 'utf8');
    let section = null;
    for (const raw of content.split(/\r?\n/)) {
      const line = raw.trim();
      if (!line) continue;
      if (/^BUILDINGS\s*\(/.test(line))         { section = 'Buildings'; continue; }
      if (/^UNITS\s*\(/.test(line))             { section = 'Units';     continue; }
      if (/^PROPS\/DECORATIONS\s*\(/.test(line)){ section = 'Props';     continue; }
      if (/^ALL OBJECTS|^NATURE RESKINS/i.test(line)) break;
      if (!section || !line || /[()=\-]/.test(line)) continue;
      if (existingSet.has(line)) continue;
      if (!isValidZhObject(line)) continue;
      const [faction] = getObjectGroup(line);
      if (!palette[faction])            palette[faction] = {};
      if (!palette[faction]['Objects']) palette[faction]['Objects'] = [];
      palette[faction]['Objects'].push(line);
    }
  } catch (e) {
    console.warn('[api] Could not build object palette:', e.message);
    palette['Error'] = { 'Objects': ['palette load failed'] };
  }
  // Return factions in canonical order
  const sorted = {};
  for (const key of FACTION_ORDER) { if (palette[key]) sorted[key] = palette[key]; }
  for (const key of Object.keys(palette)) { if (!sorted[key]) sorted[key] = palette[key]; }
  _objectPaletteCache = sorted;
  return sorted;
}

const ICONS_DIR   = path.join(RESOURCES_DIR, 'building_icons');
const TEX_DIR     = path.join(RESOURCES_DIR, 'terrain_textures');
const TEX_MAP_PATH= path.join(TEX_DIR, 'terrain_texture_map.json');
let _texMap = null; // lazy-loaded: terrain_name -> png_filename
let _groundCache = { key: null, ground: null }; // modal map height, per map file+size
function getTexMap() {
  if (!_texMap) {
    try { _texMap = JSON.parse(fs.readFileSync(TEX_MAP_PATH, 'utf8')); }
    catch { _texMap = {}; }
  }
  return _texMap;
}
function texUrl(file) {
  const p = path.join(TEX_DIR, file);
  const encoded = encodeURIComponent(file);
  try { const mtime = fs.statSync(p).mtimeMs; return `http://127.0.0.1:8099/textures/${encoded}?v=${Math.floor(mtime)}`; }
  catch { return `http://127.0.0.1:8099/textures/${encoded}`; }
}

const PIPE_NAME = '\\\\.\\pipe\\wb-engine';

// ── BIG archive parser ────────────────────────────────────────────────────────
const GAME_DIR     = 'C:/Program Files (x86)/Origin Games/Command and Conquer Generals Zero Hour/Command and Conquer Generals Zero Hour';
const GAME_DIR_GEN = 'C:/Program Files (x86)/Origin Games/Command and Conquer Generals Zero Hour/Command and Conquer Generals';
const BIG_INI      = path.join(GAME_DIR_GEN, 'INI.big');      // Generals base objects incl. bridges
const BIG_INIZH    = path.join(GAME_DIR, 'INIZH.big');
const BIG_PATCHINI = path.join(GAME_DIR, 'PatchINI.big');

// Parse BIG header → { 'data/ini/science.ini': { offset, size }, ... }
function parseBigIndex(bigPath) {
  const buf = fs.readFileSync(bigPath);
  // Magic: 4 bytes; size LE 4; count BE 4; headerEnd BE 4
  const count = buf.readUInt32BE(8);
  let pos = 16;
  const index = {};
  for (let i = 0; i < count; i++) {
    const offset = buf.readUInt32BE(pos);
    const size   = buf.readUInt32BE(pos + 4);
    pos += 8;
    let end = buf.indexOf(0, pos);
    if (end < 0) end = buf.length;
    const name = buf.slice(pos, end).toString('ascii');
    // Normalize: lowercase + backslash → forward slash
    const norm = name.toLowerCase().split('\\').join('/');
    index[norm] = { offset, size };
    pos = end + 1;
  }
  return { buf, index };
}

// Extract one file from a BIG archive by case-insensitive name
function extractFromBig(bigPath, filename) {
  try {
    const { buf, index } = parseBigIndex(bigPath);
    const key = filename.toLowerCase();
    const entry = index[key];
    if (!entry) return null;
    return buf.slice(entry.offset, entry.offset + entry.size).toString('utf8');
  } catch (e) {
    dbg(`[big] ${bigPath}: ${e.message}`);
    return null;
  }
}

// Extract same file from multiple BIG files, merge results (later overrides earlier)
function extractFromBigs(bigPaths, filename) {
  let result = null;
  for (const p of bigPaths) {
    try { const t = extractFromBig(p, filename); if (t) result = t; } catch {}
  }
  return result;
}

// INI parsers — each returns a sorted string array of names
function parseAudioEvents(ini) {
  const names = new Set();
  for (const line of ini.split(/\r?\n/)) {
    const m = line.match(/^AudioEvent\s+(\w+)/i);
    if (m) names.add(m[1]);
  }
  return [...names].sort();
}

function parseMusicTracks(ini) {
  const names = new Set();
  for (const line of ini.split(/\r?\n/)) {
    const m = line.match(/^MusicTrack\s+(\w+)/i);
    if (m) names.add(m[1]);
  }
  return [...names].sort();
}

function parseTopLevel(ini, keyword) {
  const names = new Set();
  const re = new RegExp(`^${keyword}\\s+(\\w+)`, 'im');
  for (const line of ini.split(/\r?\n/)) {
    const m = line.match(re);
    if (m) names.add(m[1]);
  }
  return [...names].sort();
}

// Lazy caches
const _iniCache = {};

function getIniList(cacheKey, loader) {
  if (!_iniCache[cacheKey]) {
    try { _iniCache[cacheKey] = loader(); }
    catch (e) { dbg(`[ini] ${cacheKey}: ${e.message}`); _iniCache[cacheKey] = []; }
  }
  return _iniCache[cacheKey];
}

function getSounds() {
  return getIniList('sounds', () => {
    const ini = extractFromBigs([BIG_INIZH, BIG_PATCHINI], 'data/ini/soundeffects.ini') || '';
    return parseAudioEvents(ini);
  });
}

function getDialog() {
  return getIniList('dialog', () => {
    const ini = extractFromBigs([BIG_INIZH, BIG_PATCHINI], 'data/ini/voice.ini') || '';
    return parseAudioEvents(ini);
  });
}

function getMusic() {
  return getIniList('music', () => {
    const ini = extractFromBigs([BIG_INIZH, BIG_PATCHINI], 'data/ini/music.ini');
    return ini ? parseMusicTracks(ini) : [];
  });
}

function getSpecialPowers() {
  return getIniList('specialpowers', () => {
    const ini = extractFromBigs([BIG_INIZH, BIG_PATCHINI], 'data/ini/specialpower.ini');
    return ini ? parseTopLevel(ini, 'SpecialPower') : [];
  });
}

function getSciences() {
  return getIniList('sciences', () => {
    const ini = extractFromBigs([BIG_INIZH, BIG_PATCHINI], 'data/ini/science.ini');
    return ini ? parseTopLevel(ini, 'Science') : [];
  });
}

function getUpgrades() {
  return getIniList('upgrades', () => {
    const ini = extractFromBigs([BIG_INIZH, BIG_PATCHINI], 'data/ini/upgrade.ini');
    return ini ? parseTopLevel(ini, 'Upgrade') : [];
  });
}

function getRoadTypes() {
  return getIniList('roads', () => {
    const ini = extractFromBigs([BIG_INIZH, BIG_PATCHINI], 'data/ini/roads.ini');
    return ini ? parseTopLevel(ini, 'Road') : [];
  });
}

function getBridgeTypes() {
  return getIniList('bridges', () => {
    // Bridges are defined as "Bridge <name> ... End" blocks in roads.ini,
    // alongside Road entries. Scan both Generals base INI and ZH override.
    const gen = extractFromBig(BIG_INI,    'data/ini/roads.ini') || '';
    const zh  = extractFromBigs([BIG_INIZH, BIG_PATCHINI], 'data/ini/roads.ini') || '';
    const combined = gen + '\n' + zh;
    return parseTopLevel(combined, 'Bridge');
  });
}


// Coordinate conversion helpers (map info: borderSize=9, cellSize=10, height=93)
// world_x = (tile_x - B) * C        world_y = (H - B - 1 - tile_y) * C
// Both can be negative (border area = -90 to -10)
function worldToTile(wx, wy, mapInfo) {
  const B = mapInfo?.borderSize ?? 9;
  const C = mapInfo?.cellSize   ?? 10;
  const H = mapInfo?.height     ?? 93;
  return { tx: Math.round(wx / C) + B, ty: H - B - 1 - Math.round(wy / C) };
}
function tileToWorld(tx, ty, mapInfo) {
  const B = mapInfo?.borderSize ?? 9;
  const C = mapInfo?.cellSize   ?? 10;
  const H = mapInfo?.height     ?? 93;
  return { wx: (tx - B) * C, wy: (H - B - 1 - ty) * C };
}

// WM_COMMAND IDs (fallback / pipe commands use same names)
const WB_CMD = {
  TERRAIN:    32771,
  TEXTURE:    32902,
  OBJECTS:    32918,
  WAYPOINTS:  32964,
  SCRIPTS:    32959,
  SHAPEFILL:  33347,
  POINTER:    32921,
  FILE_NEW:     57600,
  FILE_OPEN:    57601,
  FILE_SAVE:    57603,
  FILE_SAVE_AS: 57604,
  FILE_RESIZE:  32917,
  UNDO:       57643,
  REDO:       57644,
  VIEW_2D:    32944,
  VIEW_3D:    32943,
};

const TOOL_MAP = {
  terrain:   'terrain',
  texture:   'bigtexture',
  objects:   'pointer', // native objects tool causes "Unable to add object" on map click; placement is via pipe
  waypoints: 'waypoints',
  scripts:   'scripts',
  shapefill: 'shapefill',
  road:      'road',
  pointer:   'pointer', // explicit deselect (e.g. right-click in objects tab)
  mapsetup:  'pointer', // Map Setup: native select tool — move starts/waypoints/objects by dragging
};

let _bridge    = null;
let _getWbHwnd = null;
let _server    = null;
let _pipe      = null;
let _pipeReady = false;
let _pendingPipeResolves = [];
let _pipeBuf   = '';   // partial-line buffer for chunked pipe responses
let _terrainMode       = 'raise'; // tracks active terrain sub-mode for terrain/set dispatch
let _terrainBrushShape = 'round'; // persisted so switching to flatten/stamp applies stored shape

// ── Named pipe client ─────────────────────────────────────────────────────────

function connectPipe() {
  if (_pipe) return;

  _pipe = net.createConnection(PIPE_NAME);
  _pipe.setEncoding('utf8');

  _pipe.on('connect', () => {
    _pipeReady = true;
    dbg('[api] Pipe connected to wb-engine');
  });

  _pipe.on('data', (data) => {
    // Buffer partial data — large responses (sidelist >64KB) arrive in multiple chunks
    _pipeBuf += data;
    let nl;
    while ((nl = _pipeBuf.indexOf('\n')) !== -1) {
      const line = _pipeBuf.slice(0, nl).trim();
      _pipeBuf   = _pipeBuf.slice(nl + 1);
      if (!line) continue;
      const resolve = _pendingPipeResolves.shift();
      if (resolve) {
        try { resolve(JSON.parse(line)); }
        catch (e) { resolve({ ok: false, error: 'parse error: ' + e.message }); }
      }
    }
  });

  _pipe.on('error', (err) => {
    _pipeReady = false; _pipe = null; _pipeBuf = '';
    const stale = _pendingPipeResolves.splice(0);
    stale.forEach(r => r(null));
    dbg(`[api] Pipe error: ${err.message} — will retry (pending drained: ${stale.length})`);
    setTimeout(connectPipe, 2000);
  });

  _pipe.on('close', () => {
    _pipeReady = false; _pipe = null; _pipeBuf = '';
    const stale = _pendingPipeResolves.splice(0);
    stale.forEach(r => r(null));
    dbg(`[api] Pipe closed — will retry (pending drained: ${stale.length})`);
    setTimeout(connectPipe, 2000);
  });
}

function pipeCmd(obj) {
  return new Promise((resolve) => {
    if (!_pipeReady || !_pipe) {
      dbg('[api] pipeCmd SKIP (not ready):', obj.cmd);
      resolve(null);
      return;
    }
    dbg('[api] pipeCmd SEND:', obj.cmd);
    _pendingPipeResolves.push(resolve);
    _pipe.write(JSON.stringify(obj) + '\n');
  });
}

// ── WM_COMMAND fallback ───────────────────────────────────────────────────────

function wmCmd(id) {
  const hwnd = _getWbHwnd ? _getWbHwnd() : null;
  if (!hwnd || !id) return { ok: false, reason: id ? 'WB not ready' : 'no command for this action' };
  _bridge.sendCommand(hwnd, id);
  return { ok: true };
}

// ── Unified command dispatcher ────────────────────────────────────────────────

async function dispatchTool(toolName) {
  const pipeName = TOOL_MAP[toolName?.toLowerCase()];
  if (!pipeName) return { ok: false, error: `Unknown tool: ${toolName}` };

  const result = await pipeCmd({ cmd: 'tool', name: pipeName });
  if (result) return result;

  // fallback
  const idMap = {
    terrain: WB_CMD.TERRAIN, texture: WB_CMD.TEXTURE, pointer: WB_CMD.POINTER,
    waypoints: WB_CMD.WAYPOINTS, scripts: WB_CMD.SCRIPTS, shapefill: WB_CMD.SHAPEFILL,
  };
  return wmCmd(idMap[pipeName]);
}

async function dispatchSimple(pipeObj, fallbackId) {
  const result = await pipeCmd(pipeObj);
  if (result) return result;
  return wmCmd(fallbackId);
}

// ── HTTP helpers ──────────────────────────────────────────────────────────────

function parseBody(req) {
  return new Promise((resolve) => {
    let data = '';
    req.on('data', chunk => data += chunk);
    req.on('end', () => {
      try { resolve(JSON.parse(data || '{}')); }
      catch { resolve({}); }
    });
  });
}

function respond(res, status, body) {
  res.writeHead(status, {
    'Content-Type': 'application/json',
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type',
  });
  res.end(JSON.stringify(body));
}

// ── Request handler ───────────────────────────────────────────────────────────

async function handleRequest(req, res) {
  if (req.method === 'OPTIONS') { respond(res, 204, {}); return; }

  const url    = req.url.replace(/\?.*/, '');
  const isPost = req.method === 'POST';
  const body   = isPost ? await parseBody(req) : {};

  if (url === '/api/tool' && req.method === 'POST') {
    respond(res, 200, await dispatchTool(body.tool));
    return;
  }

  if (url === '/api/tool/activate' && req.method === 'POST') {
    const result = await pipeCmd({ cmd: body.tool });
    respond(res, 200, result || { ok: true });
    return;
  }

  if (url === '/api/map/save'  && req.method === 'POST') {
    respond(res, 200, await dispatchSimple({ cmd: 'save' }, WB_CMD.FILE_SAVE));
    return;
  }
  if (url === '/api/map/undo'  && req.method === 'POST') {
    respond(res, 200, await dispatchSimple({ cmd: 'undo' }, WB_CMD.UNDO));
    return;
  }
  if (url === '/api/map/redo'  && req.method === 'POST') {
    respond(res, 200, await dispatchSimple({ cmd: 'redo' }, WB_CMD.REDO));
    return;
  }
  if (url === '/api/map/open'  && req.method === 'POST') {
    respond(res, 200, wmCmd(WB_CMD.FILE_OPEN)); // file picker: WM_COMMAND only
    return;
  }
  if (url === '/api/map/load'  && req.method === 'POST') {
    if (!body.path) { respond(res, 400, { error: 'missing path' }); return; }
    const result = await pipeCmd({ cmd: 'load', path: body.path });
    respond(res, 200, result || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/map/saveas' && req.method === 'POST') {
    respond(res, 200, wmCmd(WB_CMD.FILE_SAVE_AS));
    return;
  }

  // Save to explicit path without native dialog — creates directory if needed
  if (url === '/api/map/save_to' && req.method === 'POST') {
    if (!body.path) { respond(res, 400, { error: 'missing path' }); return; }
    const dir = path.dirname(body.path);
    try { fs.mkdirSync(dir, { recursive: true }); } catch (e) {
      respond(res, 500, { ok: false, error: `mkdir failed: ${e.message}` }); return;
    }
    const result = await pipeCmd({ cmd: 'save_to', path: body.path });
    respond(res, 200, result || { ok: false, error: 'pipe not available' });
    return;
  }

  // Returns user-writable map directories for Save As dialog
  if (url === '/api/map/save_dirs' && req.method === 'GET') {
    const USER_SAVE_DIRS = [
      'C:\\Users\\rudie\\OneDrive\\Documents 1\\Command and Conquer Generals Zero Hour Data\\Maps',
      'C:\\Users\\rudie\\Documents\\Command and Conquer Generals Zero Hour Data\\Maps',
    ].filter(d => { try { fs.accessSync(d, fs.constants.W_OK); return true; } catch { return false; } });
    respond(res, 200, { ok: true, dirs: USER_SAVE_DIRS });
    return;
  }

  if (url === '/api/map/new' && req.method === 'POST') {
    // body: { x, y, border, height } — pipe-driven, no native dialog
    const result = await pipeCmd({ cmd: 'new_map', x: body.x, y: body.y, border: body.border, height: body.height });
    // Seed the ground-level cache: a fresh map is flat at the requested height,
    // so /map/ground_height can answer without fetching the whole heightmap.
    if (result?.ok && body.height != null) {
      const info = await pipeCmd({ cmd: 'map_get_info' });
      if (info?.ok) _groundCache = { key: `${info.filePath}|${info.width}x${info.height}`, ground: Number(body.height) };
    }
    respond(res, 200, result || wmCmd(WB_CMD.FILE_NEW));
    return;
  }

  if (url === '/api/map/resize' && req.method === 'POST') {
    // body: { x, y, border, height, anchor } — anchor = 2-char string e.g. "mc", "tl", "br"
    const result = await pipeCmd({ cmd: 'resize_map', x: body.x, y: body.y, border: body.border, height: body.height, anchor: body.anchor || 'mc' });
    respond(res, 200, result || wmCmd(WB_CMD.FILE_RESIZE));
    return;
  }

  if (url === '/api/map/list' && req.method === 'GET') {
    const scanDirs = getScanDirs();
    const maps = [];
    for (const dir of scanDirs) {
      let entries;
      try { entries = fs.readdirSync(dir, { withFileTypes: true }); } catch { continue; }
      for (const e of entries) {
        if (e.isFile() && e.name.toLowerCase().endsWith('.map')) {
          const full = dir + '\\' + e.name;
          try {
            const st = fs.statSync(full);
            maps.push({ name: e.name.replace(/\.map$/i, ''), path: full, size: st.size, mtime: st.mtimeMs, dir, scanRoot: dir });
          } catch {}
        } else if (e.isDirectory()) {
          let sub;
          try { sub = fs.readdirSync(dir + '\\' + e.name, { withFileTypes: true }); } catch { continue; }
          for (const s of sub) {
            if (!s.isFile() || !s.name.toLowerCase().endsWith('.map')) continue;
            const full = dir + '\\' + e.name + '\\' + s.name;
            try {
              const st = fs.statSync(full);
              maps.push({ name: s.name.replace(/\.map$/i, ''), path: full, size: st.size, mtime: st.mtimeMs, dir: dir + '\\' + e.name, scanRoot: dir });
            } catch {}
          }
        }
      }
    }
    // Attach cached player counts (built by tools/map_analyzer/scan_playercounts.py —
    // parsing 2000+ .map files live on every dialog open would take minutes, so this
    // reads a precomputed JSON cache instead. Missing/stale entries just show no count.
    let playerCounts = {};
    try {
      playerCounts = JSON.parse(fs.readFileSync(
        path.join(__dirname, 'data', 'playercounts.json'), 'utf8'));
    } catch {}
    for (const m of maps) {
      const entry = playerCounts[m.path.toLowerCase()];
      if (entry && !entry.error) {
        m.numPlayers = entry.numPlayers;
        m.spawnGap = entry.spawnGap;
      }
    }

    maps.sort((a, b) => b.mtime - a.mtime);
    respond(res, 200, { ok: true, maps });
    return;
  }

  if (url === '/api/map/scandirs' && req.method === 'GET') {
    respond(res, 200, { ok: true, dirs: getScanDirs() });
    return;
  }
  if (url === '/api/map/scandirs/add' && isPost) {
    if (!body.dir) { respond(res, 200, { ok: false, error: 'missing dir' }); return; }
    if (!fs.existsSync(body.dir)) { respond(res, 200, { ok: false, error: 'folder does not exist' }); return; }
    const dirs = getScanDirs();
    if (!dirs.some(d => d.toLowerCase() === body.dir.toLowerCase())) {
      dirs.push(body.dir);
      saveScanDirs(dirs);
    }
    respond(res, 200, { ok: true, dirs });
    return;
  }
  if (url === '/api/map/scandirs/remove' && isPost) {
    if (!body.dir) { respond(res, 200, { ok: false, error: 'missing dir' }); return; }
    const dirs = getScanDirs().filter(d => d.toLowerCase() !== body.dir.toLowerCase());
    saveScanDirs(dirs);
    respond(res, 200, { ok: true, dirs });
    return;
  }

  // ── Terrain panel — unified dispatcher ──────────────────────────────────────
  // Tracks which sub-mode is active so terrain/set dispatches to the right pipe cmd.
  if (url === '/api/terrain/mode' && isPost) {
    const TERRAIN_TOOL_MAP = {
      raise: 'mound', lower: 'dig', flatten: 'brush', smooth: 'feather', stamp: 'brush',
    };
    const mode = (body.mode || '').toLowerCase();
    const wbTool = TERRAIN_TOOL_MAP[mode];
    if (!wbTool) { respond(res, 200, { ok: false, error: `Unknown terrain mode: ${mode}` }); return; }
    _terrainMode = mode;
    const result = await pipeCmd({ cmd: 'tool', name: wbTool });
    // Re-apply stored brush shape after tool switch
    const shape = _terrainBrushShape === 'square' ? 1 : 0;
    if (wbTool === 'brush') await pipeCmd({ cmd: 'brush_set', shape });
    else if (wbTool === 'mound' || wbTool === 'dig') await pipeCmd({ cmd: 'mound_set', shape });
    respond(res, 200, result || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/terrain/set' && isPost) {
    // Dispatch each property to the correct tool-specific pipe command.
    const mode = _terrainMode;
    const isMound   = mode === 'raise' || mode === 'lower';
    const isBrush   = mode === 'flatten' || mode === 'stamp';
    const isFeather = mode === 'smooth';
    const cmds = [];
    if (body.brushSize != null) {
      if (isMound)        cmds.push(pipeCmd({ cmd: 'mound_set',   width:  body.brushSize }));
      else if (isBrush)   cmds.push(pipeCmd({ cmd: 'brush_set',   width:  body.brushSize }));
      else if (isFeather) cmds.push(pipeCmd({ cmd: 'feather_set', amount: body.brushSize }));
    }
    if (body.feather != null) {
      if (isMound)        cmds.push(pipeCmd({ cmd: 'mound_set', feather: body.feather }));
      else if (isBrush)   cmds.push(pipeCmd({ cmd: 'brush_set', feather: body.feather }));
    }
    if (body.height != null) {
      if (isMound)        cmds.push(pipeCmd({ cmd: 'mound_set', amount: body.height }));
      else if (isBrush)   cmds.push(pipeCmd({ cmd: 'brush_set', height: body.height }));
    }
    if (body.rate   != null && isFeather) cmds.push(pipeCmd({ cmd: 'feather_set', rate:   body.rate }));
    if (body.radius != null && isFeather) cmds.push(pipeCmd({ cmd: 'feather_set', radius: body.radius }));
    if (body.brushShape != null) {
      _terrainBrushShape = body.brushShape;
      const shape = body.brushShape === 'square' ? 1 : 0;
      if (isMound)      cmds.push(pipeCmd({ cmd: 'mound_set', shape }));
      else if (isBrush) cmds.push(pipeCmd({ cmd: 'brush_set', shape }));
    }
    await Promise.all(cmds);
    respond(res, 200, { ok: true });
    return;
  }

  if (url === '/api/shapefill/mode' && req.method === 'POST') {
    const VALID = ['rect','circle','polygon','select','line','fill','edit'];
    const mode = (body.mode || '').toLowerCase();
    if (!VALID.includes(mode)) { respond(res, 200, { ok: false, error: `Unknown mode: ${mode}` }); return; }
    const result = await pipeCmd({ cmd: 'shapefill_mode', mode });
    respond(res, 200, result || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/shapefill/state' && (req.method === 'GET' || isPost)) {
    const result = await pipeCmd({ cmd: 'shapefill_get_state' });
    respond(res, 200, result || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/shapefill/set' && req.method === 'POST') {
    const result = await pipeCmd({ cmd: 'shapefill_set', ...body });
    respond(res, 200, result || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/shapefill/action' && req.method === 'POST') {
    const VALID_ACTIONS = ['apply','delete','duplicate','flip_h','flip_v',
                           'finish_poly','finish_line','clear_lines','rotate','copy','paste'];
    if (!VALID_ACTIONS.includes(body.action)) {
      respond(res, 200, { ok: false, error: `Unknown action: ${body.action}` }); return;
    }
    const result = await pipeCmd({ cmd: 'shapefill_action', action: body.action });
    respond(res, 200, result || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/shapefill/open_tex' && req.method === 'POST') {
    const which = body.which === 'border' ? 'border' : 'inner';
    const result = await pipeCmd({ cmd: 'shapefill_open_tex', which });
    respond(res, 200, result || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/shapefill/select' && req.method === 'POST') {
    const result = await pipeCmd({ cmd: 'shapefill_select', id: body.id, kind: body.kind });
    respond(res, 200, result || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/view/mode' && req.method === 'POST') {
    const mode = body.mode === '3d' ? '3d' : '2d';
    const result = await pipeCmd({ cmd: 'view', mode });
    respond(res, 200, result || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/status' && req.method === 'GET') {
    const pipeStatus = await pipeCmd({ cmd: 'status' });
    respond(res, 200, {
      ok:        true,
      wbReady:   !!(_getWbHwnd ? _getWbHwnd() : null),
      pipeReady: _pipeReady,
      // Flatten pipe fields so test scripts can check them directly
      viewMode:  pipeStatus?.viewMode  ?? null,
      activeTool: pipeStatus?.activeTool ?? null,
      mapLoaded: pipeStatus?.ready     ?? null,
    });
    return;
  }

  // GET /api/texture/all — all 234 known terrain textures (from Terrain.ini, not map-specific)
  if (url === '/api/texture/all' && req.method === 'GET') {
    const texMap = getTexMap();
    const textures = Object.entries(texMap).map(([name, file], i) => ({
      index: i,
      name,
      uiName: name.replace(/([A-Z])/g, ' $1').replace(/Type(\d)/g, ' $1').trim(),
      imgUrl: texUrl(file),
    }));
    respond(res, 200, { ok: true, textures });
    return;
  }

  // GET /api/texture/list — all terrain texture classes for current map + imgUrl
  if (url === '/api/texture/list' && req.method === 'GET') {
    const raw = await pipeCmd({ cmd: 'get_texture_list' });
    if (!raw || !raw.ok) { respond(res, 200, raw || { ok: false, error: 'pipe not available' }); return; }
    const texMap = getTexMap();
    const textures = (raw.textures || []).map(t => ({
      ...t,
      index: t.class,  // C++ sends "class"; normalize to "index" for JS (tex.index used throughout renderer)
      imgUrl: texMap[t.name] ? texUrl(texMap[t.name]) : null,
    }));
    respond(res, 200, { ok: true, textures });
    return;
  }

  // GET /roadtex/:name.png — road/bridge preview textures (tools/extract_road_textures.py)
  if (url.startsWith('/roadtex/') && req.method === 'GET') {
    const name = url.slice('/roadtex/'.length);
    if (/^[A-Za-z0-9_.%-]+\.png$/i.test(name)) {
      const filePath = path.join(__dirname, 'road_textures', name);
      if (fs.existsSync(filePath)) {
        const data = fs.readFileSync(filePath);
        res.writeHead(200, { 'Content-Type': 'image/png', 'Access-Control-Allow-Origin': '*', 'Cache-Control': 'no-cache' });
        res.end(data);
      } else { res.writeHead(404); res.end(); }
    } else { res.writeHead(400); res.end(); }
    return;
  }

  // GET /icons/:name.png — serve object/unit icons from building_icons/
  if (url.startsWith('/icons/') && req.method === 'GET') {
    const name = url.slice('/icons/'.length);
    if (/^[A-Za-z0-9_.%-]+\.png$/i.test(name)) {
      const filePath = path.join(ICONS_DIR, name);
      if (fs.existsSync(filePath)) {
        const data = fs.readFileSync(filePath);
        res.writeHead(200, { 'Content-Type': 'image/png', 'Access-Control-Allow-Origin': '*', 'Cache-Control': 'no-cache' });
        res.end(data);
      } else { res.writeHead(404); res.end(); }
    } else { res.writeHead(400); res.end(); }
    return;
  }

  // GET /textures/:name.png — serve extracted terrain texture thumbnails
  if (url.startsWith('/textures/') && req.method === 'GET') {
    const raw  = url.slice('/textures/'.length).split('?')[0]; // strip ?v= cache-buster
    const name = decodeURIComponent(raw);
    if (/^[A-Za-z0-9_.\-&]+\.png$/.test(name)) {
      const filePath = path.join(TEX_DIR, name);
      if (fs.existsSync(filePath)) {
        const data = fs.readFileSync(filePath);
        res.writeHead(200, { 'Content-Type': 'image/png', 'Access-Control-Allow-Origin': '*', 'Cache-Control': 'no-cache' });
        res.end(data);
      } else {
        res.writeHead(404); res.end();
      }
    } else {
      res.writeHead(400); res.end();
    }
    return;
  }

  // ── Tier A extras ────────────────────────────────────────────────────────

  if (url === '/api/view/toggle' && isPost) {
    if (!body.name) { respond(res, 200, { ok: false, error: 'missing name' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'view_toggle', name: body.name }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/edit' && isPost) {
    if (!body.op) { respond(res, 200, { ok: false, error: 'missing op' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'edit', op: body.op }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // Impassable overlay: absolute state, unlike /api/view/toggle (blind flip).
  // GET = query engine state; POST {state:0|1} = set. Both return {ok, state}.
  if (url === '/api/view/impassable') {
    const state = isPost && body.state != null ? Number(body.state) : -1;
    respond(res, 200, await pipeCmd({ cmd: 'impassable_view', state }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/layer' && isPost) {
    if (!body.op) { respond(res, 200, { ok: false, error: 'missing op' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'layer', op: body.op, name: body.name }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/dialog' && isPost) {
    if (!body.op) { respond(res, 200, { ok: false, error: 'missing op' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'dialog', op: body.op }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Tier B — Brush ────────────────────────────────────────────────────────

  if (url === '/api/brush/state' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'brush_get_state' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/brush/set' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'brush_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Tier B — Mound ────────────────────────────────────────────────────────

  if (url === '/api/mound/state' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'mound_get_state' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/mound/set' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'mound_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Tier B — Texture painter ──────────────────────────────────────────────

  if (url === '/api/texture/state' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'texture_get_state' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/texture/set' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'texture_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/texture/swap' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'texture_swap' }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Tier C — Feather ──────────────────────────────────────────────────────

  if (url === '/api/feather/state' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'feather_get_state' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/feather/set' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'feather_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Tier C — Scorch ───────────────────────────────────────────────────────

  if (url === '/api/scorch/state' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'scorch_get_state' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/scorch/set' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'scorch_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Tier C — MeshMold ────────────────────────────────────────────────────

  if (url === '/api/meshmold/state' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'meshmold_get_state' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/meshmold/set' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'meshmold_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/meshmold/action' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'meshmold_action', action: body.action }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/meshmold/list' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'meshmold_list' }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/meshmold/activate' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'meshmold_activate' }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Tier C — FloodFill ───────────────────────────────────────────────────
  if (url === '/api/floodfill' && isPost) {
    // body: { wx, wy, texClass (optional, -1=fg), exact (optional, 0/1) }
    respond(res, 200, await pipeCmd({ cmd: 'floodfill_at', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Tier C — Water ────────────────────────────────────────────────────────

  if (url === '/api/water/state' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'water_get_state' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/water/set' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'water_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Tier C — Ramp ─────────────────────────────────────────────────────────

  if (url === '/api/ramp/state' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'ramp_get_state' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/ramp/set' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'ramp_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Tier D — Contour ──────────────────────────────────────────────────────

  if (url === '/api/contour/state' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'contour_get_state' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/contour/set' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'contour_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Tier H — Map data read-back ───────────────────────────────────────────

  if (url === '/api/view/screen_to_world' && isPost) {
    if (body.sx == null || body.sy == null) { respond(res, 200, { ok: false, error: 'missing sx/sy' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'screen_to_world', sx: body.sx, sy: body.sy }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/view/mouse_wheel' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'mouse_wheel', delta: body.delta || 0 }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/view/synthetic_rmouse' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'synthetic_rmouse', action: body.action || '', sx: body.sx || 0, sy: body.sy || 0 }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/map/info' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'map_get_info' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/map/heightmap' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'map_get_heightmap' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  // Ground level = modal (most common) height in the heightmap. Cached per map
  // file+size so the ~350KB heightmap fetch happens once per map, not per poll.
  if (url === '/api/map/ground_height' && req.method === 'GET') {
    const info = await pipeCmd({ cmd: 'map_get_info' });
    if (!info?.ok) { respond(res, 200, info || { ok: false, error: 'pipe not available' }); return; }
    const key = `${info.filePath}|${info.width}x${info.height}`;
    if (_groundCache.key !== key) {
      const hm = await pipeCmd({ cmd: 'map_get_heightmap' });
      if (!hm?.ok || !Array.isArray(hm.data) || !hm.data.length) {
        respond(res, 200, { ok: false, error: hm?.error || 'no heightmap' });
        return;
      }
      const counts = new Array(256).fill(0);
      for (const v of hm.data) counts[v & 255]++;
      let ground = 0;
      for (let i = 1; i < 256; i++) if (counts[i] > counts[ground]) ground = i;
      _groundCache = { key, ground };
    }
    respond(res, 200, { ok: true, ground: _groundCache.ground });
    return;
  }
  if (url === '/api/map/texturemap' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'map_get_texturemap' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/map/objects' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'map_get_objects' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/map/waypoints' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'map_get_waypoints' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/map/triggers' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'map_get_triggers' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/trigger' && isPost) {
    if (!body.name || !body.points?.length) { respond(res, 200, { ok: false, error: 'missing name or points' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'trigger_add', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/trigger/delete' && isPost) {
    if (!body.name) { respond(res, 200, { ok: false, error: 'missing name' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'trigger_del', name: body.name }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/trigger/set' && isPost) {
    if (!body.name) { respond(res, 200, { ok: false, error: 'missing name' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'trigger_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/map/teams' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'map_get_teams' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  // ── Global lighting ──
  if (url === '/api/map/lighting' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'lighting_get' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/map/lighting' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'lighting_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/map/lighting/reset' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'lighting_reset' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/map/selected' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'map_get_selected' }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/object/delete_selected' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'edit', op: 'delete' }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // GET /api/map/coords?wx=400&wy=370  or  ?tx=49&ty=46
  if (url.startsWith('/api/map/coords')) {
    const qs = new URLSearchParams(req.url.replace(/^[^?]*/, '').slice(1));
    const info = await pipeCmd({ cmd: 'map_get_info' });
    if (qs.has('wx') || qs.has('wy')) {
      const t = worldToTile(parseFloat(qs.get('wx') ?? '0'), parseFloat(qs.get('wy') ?? '0'), info);
      respond(res, 200, { ok: true, mode: 'world_to_tile', ...t });
    } else {
      const w = tileToWorld(parseInt(qs.get('tx') ?? '0'), parseInt(qs.get('ty') ?? '0'), info);
      respond(res, 200, { ok: true, mode: 'tile_to_world', ...w });
    }
    return;
  }

  // ── Tier I — Programmatic terrain write ──────────────────────────────────

  if (url === '/api/shapefill/create' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'shapefill_create', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/map/height/set' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'map_height_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/place/waypoint' && isPost) {
    if (!body.name) { respond(res, 200, { ok: false, error: 'missing name' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'place_waypoint', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/place/object' && isPost) {
    if (!body.template) { respond(res, 200, { ok: false, error: 'missing template' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'place_object', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/link/waypoints' && isPost) {
    if (!body.name1 || !body.name2) { respond(res, 200, { ok: false, error: 'missing name1/name2' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'link_waypoints', name1: body.name1, name2: body.name2 }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── SidesList wizard (T3) ─────────────────────────────────────────────────
  if (url === '/api/sidelist' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'sidelist_get' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/sidelist/player' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'sidelist_player_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/sidelist/player/new' && isPost) {
    if (!body.name) { respond(res, 200, { ok: false, error: 'missing name' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'sidelist_player_new', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/sidelist/player/delete' && isPost) {
    if (!body.name) { respond(res, 200, { ok: false, error: 'missing name' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'sidelist_player_del', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/sidelist/team' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'sidelist_team_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/sidelist/team/delete' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'sidelist_team_del', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/sidelist/script' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'sidelist_script_set', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/sidelist/script/delete' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'sidelist_script_del', ...body }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/sidelist/group' && isPost) {
    dbg('[api] sidelist/group POST pipeReady=' + _pipeReady + ' body=' + JSON.stringify(body));
    const groupResult = await pipeCmd({ cmd: 'sidelist_group_set', ...body });
    dbg('[api] sidelist/group result=' + JSON.stringify(groupResult));
    respond(res, 200, groupResult || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/sidelist/add_skirmish' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'sidelist_add_skirmish' }) || { ok: false, error: 'pipe not available' });
    return;
  }


  if (url === '/api/place/waypoint' && isPost) {
    if (!body.name) { respond(res, 200, { ok: false, error: 'missing name' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'place_waypoint', name: body.name, wx: body.wx ?? 0, wy: body.wy ?? 0 }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/delete/waypoint' && isPost) {
    if (!body.name) { respond(res, 200, { ok: false, error: 'missing name' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'del_waypoint', name: body.name }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/camera' && isPost) {
    if (!body.action) { respond(res, 200, { ok: false, error: 'missing action' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'camera_action', action: body.action }) || { ok: false, error: 'pipe not available' });
    return;
  }

  if (url === '/api/object/rotate_selected' && isPost) {
    respond(res, 200, await pipeCmd({ cmd: 'rotate_selected', angle: body.angle ?? 0 }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── Object properties ────────────────────────────────────────────────────
  if (url === '/api/object/props' && req.method === 'GET') {
    respond(res, 200, await pipeCmd({ cmd: 'obj_get_props' }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/object/set_prop' && isPost) {
    if (!body.key) { respond(res, 200, { ok: false, error: 'missing key' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'obj_set_prop', key: body.key, value: body.value }) || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/object/select' && isPost) {
    if (!body.name) { respond(res, 200, { ok: false, error: 'missing name' }); return; }
    respond(res, 200, await pipeCmd({ cmd: 'select_map_object', name: body.name }) || { ok: false, error: 'pipe not available' });
    return;
  }

  // GET /api/objects/palette — full object palette from REAL_OBJECT_NAMES.txt
  if (url === '/api/objects/palette' && req.method === 'GET') {
    respond(res, 200, { ok: true, palette: buildObjectPalette() });
    return;
  }

  // GET /api/vegetation/types — list of nature/vegetation objects for the Nature panel
  if (url === '/api/vegetation/types' && req.method === 'GET') {
    const result = [];
    const iconsDir = ICONS_DIR;
    const allFiles = fs.readdirSync(iconsDir);
    const seen = new Set();
    // Direct icons first (higher quality), then topdown fallback
    for (const pass of ['direct', 'topdown']) {
      for (const f of allFiles) {
        const isTopdown = f.startsWith('topdown_');
        if (pass === 'direct' && isTopdown) continue;
        if (pass === 'topdown' && !isTopdown) continue;
        const name = isTopdown ? f.slice('topdown_'.length, -4) : f.slice(0, -4);
        if (!VEGETATION_RE.test(name) || seen.has(name)) continue;
        if (!isValidZhObject(name)) continue;
        seen.add(name);
        const encoded = encodeURIComponent(f);
        result.push({ name, icon: `http://127.0.0.1:8099/icons/${encoded}` });
      }
    }
    result.sort((a, b) => a.name.localeCompare(b.name));
    respond(res, 200, { ok: true, types: result });
    return;
  }

  // POST /api/vegetation/plant — plant a single tree at world coords
  if (url === '/api/vegetation/plant' && isPost) {
    const r = await pipeCmd({ cmd: 'plant_tree', type: body.type || '', wx: body.wx || 0, wy: body.wy || 0, angle: body.angle != null ? body.angle : -1 });
    respond(res, 200, r || { ok: false, error: 'pipe not available' });
    return;
  }

  // POST /api/vegetation/grove — scatter vegetation over circular area (grove brush)
  if (url === '/api/vegetation/grove' && isPost) {
    const r = await pipeCmd({
      cmd:         'plant_grove',
      type:        body.type        || '',
      cx:          body.cx          ?? 0,
      cy:          body.cy          ?? 0,
      radius:      body.radius      ?? 60,
      density:     body.density     ?? 0.3,
      min_spacing: body.min_spacing ?? 15,
      skip_water:  body.skip_water  ?? 1,
      skip_steep:  body.skip_steep  ?? 1,
      rand_rot:    body.rand_rot    ?? 1,
      mix:         body.mix         ?? 0,
    });
    respond(res, 200, r || { ok: false, error: 'pipe not available' });
    return;
  }

  // ── INI data endpoints (parsed from BIG archives) ───────────────────────────
  if (url === '/api/ini/sounds' && req.method === 'GET') {
    respond(res, 200, { ok: true, names: getSounds() });
    return;
  }
  if (url === '/api/ini/dialog' && req.method === 'GET') {
    respond(res, 200, { ok: true, names: getDialog() });
    return;
  }
  if (url === '/api/ini/music' && req.method === 'GET') {
    respond(res, 200, { ok: true, names: getMusic() });
    return;
  }
  if (url === '/api/ini/specialpowers' && req.method === 'GET') {
    respond(res, 200, { ok: true, names: getSpecialPowers() });
    return;
  }
  if (url === '/api/ini/sciences' && req.method === 'GET') {
    respond(res, 200, { ok: true, names: getSciences() });
    return;
  }
  if (url === '/api/ini/upgrades' && req.method === 'GET') {
    respond(res, 200, { ok: true, names: getUpgrades() });
    return;
  }
  if (url === '/api/ini/roads' && req.method === 'GET') {
    respond(res, 200, { ok: true, names: getRoadTypes() });
    return;
  }
  if (url === '/api/ini/bridges' && req.method === 'GET') {
    respond(res, 200, { ok: true, names: getBridgeTypes() });
    return;
  }

  // ── Road and bridge endpoints ────────────────────────────────────────────────
  if (url === '/api/map/roads' && req.method === 'GET') {
    const r = await pipeCmd({ cmd: 'map_get_roads' });
    respond(res, 200, r || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/place/road' && isPost) {
    const r = await pipeCmd({ cmd: 'place_road', ...body });
    respond(res, 200, r || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/road/delete' && isPost) {
    const r = await pipeCmd({ cmd: 'del_road', ...body });
    respond(res, 200, r || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/map/bridges' && req.method === 'GET') {
    const r = await pipeCmd({ cmd: 'map_get_bridges' });
    respond(res, 200, r || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/bridge/setname' && isPost) {
    const r = await pipeCmd({ cmd: 'set_bridge_name', ...body });
    respond(res, 200, r || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/place/bridge' && isPost) {
    const r = await pipeCmd({ cmd: 'place_bridge', ...body });
    respond(res, 200, r || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/road/settool' && isPost) {
    const r = await pipeCmd({ cmd: 'set_road_tool', ...body });
    respond(res, 200, r || { ok: false, error: 'pipe not available' });
    return;
  }
  if (url === '/api/road/select' && isPost) {
    const r = await pipeCmd({ cmd: 'sel_road', ...body });
    respond(res, 200, r || { ok: false, error: 'pipe not available' });
    return;
  }

  respond(res, 404, { error: `Unknown endpoint: ${url}` });
}

// ── Public API ────────────────────────────────────────────────────────────────

function start(bridge, getWbHwnd, port = 8099) {
  dbg('=== API Session ' + new Date().toISOString() + ' ===');
  _bridge    = bridge;
  _getWbHwnd = getWbHwnd;
  _server    = http.createServer(handleRequest);
  _server.listen(port, '127.0.0.1', () => {
    dbg(`[api] REST API listening on http://127.0.0.1:${port}/api`);
  });
  connectPipe();
  return _server;
}

function stop() {
  _pipe?.destroy();
  _pipe = null;
  _server?.close();
}

module.exports = { start, stop, WB_CMD };
