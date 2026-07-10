const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const { spawn } = require('child_process');
const path = require('path');
const fs   = require('fs');

// Where to look for WorldBuilderZH.exe - resolved so the SAME code works in dev
// (`npm start`, __dirname = this source folder) and in a packaged portable .exe
// dropped into someone else's game folder.
// electron-builder's "portable" Windows target self-extracts to a temp folder at
// runtime, so process.execPath there points at that temp copy, not the real
// location the user placed the .exe - it sets PORTABLE_EXECUTABLE_DIR specifically
// so packaged code can find the user's actual folder (see electron-builder docs).
function getAppDir() {
  if (process.env.PORTABLE_EXECUTABLE_DIR) return process.env.PORTABLE_EXECUTABLE_DIR;
  if (app.isPackaged) return path.dirname(process.execPath);
  return __dirname;
}
const APP_DIR = getAppDir();

// Where to WRITE (debug.log, settings). The game folder often lives under
// Program Files, which UAC makes read-only for normal users - on such machines
// any write there is EPERM and must never take the app down. Probe once with a
// real file write (a directory access check isn't reliable for this on Windows),
// and fall back to Electron's per-user data folder, which is always writable.
function pickWritableDir(preferred) {
  try {
    const probe = path.join(preferred, '.wb-write-test');
    fs.writeFileSync(probe, '');
    fs.unlinkSync(probe);
    return preferred;
  } catch {
    try { return app.getPath('userData'); } catch { return require('os').tmpdir(); }
  }
}
const DATA_DIR = pickWritableDir(APP_DIR);
// hwnd-bridge.js and api-server.js write logs/settings too - hand them the
// resolved dir via env so all three agree without a shared module.
process.env.WB_UI_DATA_DIR = DATA_DIR;

const bridge    = require('./hwnd-bridge');
const apiServer = require('./api-server');

// Log file next to the launcher exe when possible, else in the user-data dir.
const LOG_FILE = path.join(DATA_DIR, 'debug.log');
try { fs.writeFileSync(LOG_FILE, `=== Session ${new Date().toISOString()} ===\nAPP_DIR=${APP_DIR}\nDATA_DIR=${DATA_DIR}\n`); } catch {}

function log(...args) {
  const line = args.map(a => (typeof a === 'object' ? JSON.stringify(a) : String(a))).join(' ');
  console.log(line);
  try { fs.appendFileSync(LOG_FILE, line + '\n'); } catch {}
}

// Patch console.error too
const _origError = console.error.bind(console);
console.error = (...args) => {
  _origError(...args);
  const line = args.map(a => (typeof a === 'object' ? JSON.stringify(a) : String(a))).join(' ');
  try { fs.appendFileSync(LOG_FILE, '[ERR] ' + line + '\n'); } catch {}
};

// The whole point of a shareable build: this launcher sits IN the user's game
// folder, right next to their own WorldBuilderZH.exe - so look there first.
// Falls back to this dev machine's known install so `npm start` here keeps working
// unchanged; a packaged build for someone else will only ever hit the first branch.
const _WB_IN_APP_DIR = path.join(APP_DIR, 'WorldBuilderZH.exe');
const _WB_DEV_FALLBACK = 'C:\\Program Files (x86)\\Origin Games\\Command and Conquer Generals Zero Hour\\Command and Conquer Generals Zero Hour\\WorldBuilderZH.exe';
const WB_EXE = fs.existsSync(_WB_IN_APP_DIR) ? _WB_IN_APP_DIR : _WB_DEV_FALLBACK;
const WB_CWD = path.dirname(WB_EXE);
log(`[main] WorldBuilderZH.exe resolved to: ${WB_EXE}`);

let mainWin;
let wbProcess  = null;
let wbHwnd     = null;   // WB main frame (for WM_COMMAND / tool switching)
let wbViewHwnd = null;   // Inner view window (the actual DirectX render surface)

function getScreenRect() {
  const b = mainWin.getBounds();
  return { x: b.x, y: b.y, w: b.width, h: b.height };
}

async function embedWorldBuilder() {
  if (!wbProcess || !mainWin) return;

  log(`[main] embedWorldBuilder start, WB PID=${wbProcess.pid}`);

  // Embedded mode: no splash, but WB still loads INI/textures — wait a bit
  log('[main] Waiting 2s for WB to initialize...');
  await sleep(2000);

  // Poll until WorldBuilder's main window appears (max 20s)
  let hwnd = null;
  for (let i = 0; i < 40; i++) {
    await sleep(500);
    log(`[main] poll attempt ${i + 1}/40...`);
    hwnd = bridge.findMainWindowByPid(wbProcess.pid);
    if (hwnd) break;
  }

  if (!hwnd) {
    console.error('[main] WorldBuilder window not found after 20s');
    mainWin.webContents.send('wb-status', 'error');
    return;
  }

  wbHwnd = hwnd;
  const eHwnd = bridge.getElectronHwnd(mainWin);
  const sr    = getScreenRect();

  log(`[main] Electron HWND=${eHwnd}, WB HWND=${wbHwnd}`);
  log(`[main] Floating borderless WB behind Electron at (${sr.x},${sr.y}) ${sr.w}x${sr.h}`);

  // In embedded mode WB is already borderless — position it directly behind Electron
  bridge.floatBehindElectron(wbHwnd, eHwnd, sr.x, sr.y, sr.w, sr.h);

  mainWin.webContents.send('wb-status', 'ready');
  log('[main] WorldBuilder embedded successfully');
}

function launchWorldBuilder() {
  if (wbProcess) return;

  mainWin.webContents.send('wb-status', 'loading');

  try {
    log(`[main] Spawning: ${WB_EXE} /embedded`);
    wbProcess = spawn(WB_EXE, ['/embedded'], { cwd: WB_CWD, detached: false });
    log(`[main] WB process PID=${wbProcess.pid}`);
    wbProcess.on('exit', (code) => {
      log(`[main] WB exited with code ${code}`);
      wbProcess  = null;
      wbHwnd     = null;
      wbViewHwnd = null;
      mainWin && mainWin.webContents.send('wb-status', 'closed');
    });
    embedWorldBuilder().catch(e => {
      console.error('[main] embedWorldBuilder threw:', e.message);
      mainWin && mainWin.webContents.send('wb-status', 'error');
    });
  } catch (e) {
    console.error('[main] Failed to launch WorldBuilder:', e.message);
    mainWin.webContents.send('wb-status', 'error');
  }
}

function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

function createWindow() {
  mainWin = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 1024,
    minHeight: 640,
    frame: false,
    transparent: true,        // lets embedded WB show through transparent viewport
    backgroundColor: '#00000000',
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false
    }
  });

  mainWin.loadFile('index.html');

  // Re-embed WB after any renderer reload (CTRL+R / CTRL+SHIFT+R) so the view is never lost
  mainWin.webContents.on('did-finish-load', () => {
    if (!wbHwnd) return;
    log('[main] renderer reloaded — re-positioning WB');
    const sr    = getScreenRect();
    const eHwnd = bridge.getElectronHwnd(mainWin);
    bridge.floatBehindElectron(wbHwnd, eHwnd, sr.x, sr.y, sr.w, sr.h);
    mainWin.webContents.send('wb-status', 'ready');
  });

  // DevTools only in dev (`npm start`) - a shared build shouldn't confront users
  // with a debugger window on launch.
  if (!app.isPackaged) {
    mainWin.webContents.openDevTools({ mode: 'detach' });
  }

  // Realign WB after move/resize (debounced: spawnSync PS call takes ~200ms)
  let realignTimer = null;
  const realignWB = () => {
    if (!wbHwnd) return;
    clearTimeout(realignTimer);
    realignTimer = setTimeout(() => {
      const sr    = getScreenRect();
      const eHwnd = bridge.getElectronHwnd(mainWin);
      bridge.repositionView(wbHwnd, eHwnd, sr.x, sr.y, sr.w, sr.h);
    }, 150);
  };
  mainWin.on('resize', realignWB);
  mainWin.on('move',   realignWB);

  // Restore WB position/visibility when Electron gains focus or is un-minimized
  mainWin.on('focus', () => {
    if (!wbHwnd) return;
    const eHwnd = bridge.getElectronHwnd(mainWin);
    bridge.fixZOrder(wbHwnd, eHwnd);   // put WB just behind Electron again
  });
  mainWin.on('minimize', () => {
    if (wbHwnd) bridge.showWb(wbHwnd, false);
  });
  mainWin.on('restore', () => {
    if (!wbHwnd) return;
    const sr    = getScreenRect();
    const eHwnd = bridge.getElectronHwnd(mainWin);
    bridge.showWb(wbHwnd, true);
    bridge.repositionView(wbHwnd, eHwnd, sr.x, sr.y, sr.w, sr.h);
    bridge.fixZOrder(wbHwnd, eHwnd);
  });
}

// ── IPC handlers ──
ipcMain.on('win-minimize',       () => mainWin && mainWin.minimize());
ipcMain.on('win-maximize',       () => mainWin && (mainWin.isMaximized() ? mainWin.unmaximize() : mainWin.maximize()));
ipcMain.on('win-close',          () => { wbProcess && wbProcess.kill(); mainWin && mainWin.close(); });
ipcMain.on('launch-worldbuilder',() => launchWorldBuilder());
ipcMain.on('panel-toggle', () => { /* panel is overlay — no WB resize needed */ });

// Mouse passthrough: when mouse is over the transparent viewport (not UI panels),
// let events pass through Electron to the embedded WB window.
ipcMain.on('viewport-mouse', (_, overViewport) => {
  if (!mainWin) return;
  mainWin.setIgnoreMouseEvents(overViewport, { forward: overViewport });
});

// Native folder picker for "Open Map" scan-directory management (dialog module only
// exists in the main process — the renderer asks for a path via invoke/handle).
ipcMain.handle('pick-folder', async () => {
  if (!mainWin) return null;
  const result = await dialog.showOpenDialog(mainWin, { properties: ['openDirectory'] });
  if (result.canceled || !result.filePaths.length) return null;
  return result.filePaths[0];
});

app.whenReady().then(() => {
  createWindow();
  // Start REST API — single control interface for UI + AI
  apiServer.start(bridge, () => wbHwnd);
  // Auto-launch WB — no button needed, starts as standalone app
  setTimeout(() => launchWorldBuilder(), 800);
});
app.on('window-all-closed', () => {
  apiServer.stop();
  wbProcess && wbProcess.kill();
  app.quit();
});
