/**
 * hwnd-bridge.js
 * Win32 bridge for embedding WorldBuilder's view behind Electron.
 *
 * Strategy:
 *   1. Find WB's main HWND (after splash screen disappears)
 *   2. Strip ALL chrome: menu, toolbars, status bar, MDI child chrome
 *   3. Find the inner AfxFrameOrView140 window (the actual map render surface)
 *   4. Detach view from MDI, make it a standalone popup behind Electron
 *   5. Hide WB's main frame (off-screen) — still runs for tool logic
 *   6. Electron is transparent; mouse events pass through to the view
 */

const { spawnSync, spawn } = require('child_process'); // spawn for psAsync, spawnSync for ps()
const fs   = require('fs');
const path = require('path');
const { app } = require('electron');

// main.js probes for a writable dir at startup (game folder, falling back to the
// per-user data dir when Program Files is UAC-read-only) and exports the result.
const APP_DIR = process.env.PORTABLE_EXECUTABLE_DIR
  ? process.env.PORTABLE_EXECUTABLE_DIR
  : (app.isPackaged ? path.dirname(process.execPath) : __dirname);
const LOG_FILE = path.join(process.env.WB_UI_DATA_DIR || APP_DIR, 'debug.log');
function log(...args) {
  const line = args.map(a => String(a)).join(' ');
  console.log(line);
  try { fs.appendFileSync(LOG_FILE, line + '\n'); } catch {}
}

function getElectronHwnd(win) {
  const buf = win.getNativeWindowHandle();
  try   { return buf.readBigInt64LE(0); }
  catch { return BigInt(buf.readInt32LE(0)); }
}

function ps(script) {
  const result = spawnSync('powershell.exe', [
    '-NoProfile', '-NonInteractive', '-Command', script
  ], { encoding: 'utf8', timeout: 15000 });

  if (result.error) { log('[bridge] PS error:', result.error.message); throw result.error; }
  if (result.stderr?.trim()) log('[bridge] PS stderr:', result.stderr.trim());
  const out = (result.stdout || '').trim();
  if (out) log('[bridge] PS:', out);
  return out;
}

// ── 1. Find WB main window ────────────────────────────────────────────────────
function findMainWindowByPid(pid) {
  log(`[bridge] Looking for WB main window (PID=${pid})`);
  const script = `
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices; using System.Text;
public class F1 {
  public delegate bool EP(IntPtr h, IntPtr l);
  [DllImport("user32")] public static extern bool EnumWindows(EP cb, IntPtr l);
  [DllImport("user32")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
  [DllImport("user32")] public static extern int  GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32")] public static extern IntPtr GetParent(IntPtr h);
}
"@ -Language CSharp 2>$null
$found = [long]0; $target = [uint32]${pid}
[F1]::EnumWindows({
  param([IntPtr]$h,[IntPtr]$l)
  $p=[uint32]0; [void][F1]::GetWindowThreadProcessId($h,[ref]$p)
  if($p -ne $target) { return $true }
  if(-not [F1]::IsWindowVisible($h)) { return $true }
  if([F1]::GetParent($h) -ne [IntPtr]::Zero) { return $true }
  $s=New-Object System.Text.StringBuilder 512; [void][F1]::GetWindowText($h,$s,512)
  $t=$s.ToString()
  Write-Host "Candidate: $($h.ToInt64()) '$t'"
  if($t -like '*World Builder*' -and $t -notlike '*Loading*') {
    $script:found=$h.ToInt64(); return $false
  }
  return $true
}, [IntPtr]::Zero) | Out-Null
$found
`;
  try {
    const raw   = ps(script);
    const last  = raw.split('\n').map(l => l.trim()).filter(Boolean).pop() || '0';
    const hwnd  = BigInt(last);
    if (hwnd === 0n) { log('[bridge] WB main window not found'); return null; }
    log(`[bridge] WB main HWND=${hwnd}`);
    return hwnd;
  } catch (e) { log('[bridge] findMainWindowByPid error:', e.message); return null; }
}

// ── 2. Find the inner view window (AfxFrameOrView140) ────────────────────────
function findViewWindow(wbMainHwnd) {
  log(`[bridge] Looking for view window inside HWND=${wbMainHwnd}`);
  const script = `
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices; using System.Text;
public class F2 {
  public delegate bool EP(IntPtr h, IntPtr l);
  [DllImport("user32")] public static extern bool EnumChildWindows(IntPtr p, EP cb, IntPtr l);
  [DllImport("user32")] public static extern int  GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
}
"@ -Language CSharp 2>$null
$best = [long]0; $bestArea = 0
[F2]::EnumChildWindows([IntPtr][long]${wbMainHwnd}, {
  param([IntPtr]$h,[IntPtr]$l)
  $cn=New-Object System.Text.StringBuilder 128; [void][F2]::GetClassName($h,$cn,128)
  if($cn.ToString() -like 'AfxFrameOrView*') {
    $r=New-Object F2+RECT; [void][F2]::GetWindowRect($h,[ref]$r)
    $area=($r.R-$r.L)*($r.B-$r.T)
    Write-Host "View candidate: $($h.ToInt64()) cls=$($cn) area=$area"
    if($area -gt $script:bestArea) { $script:bestArea=$area; $script:best=$h.ToInt64() }
  }
  return $true
}, [IntPtr]::Zero) | Out-Null
$best
`;
  try {
    const raw  = ps(script);
    const last = raw.split('\n').map(l => l.trim()).filter(Boolean).pop() || '0';
    const hwnd = BigInt(last);
    if (hwnd === 0n) { log('[bridge] View window not found'); return null; }
    log(`[bridge] View HWND=${hwnd}`);
    return hwnd;
  } catch (e) { log('[bridge] findViewWindow error:', e.message); return null; }
}

// ── 3. Float view behind Electron ─────────────────────────────────────────────
// Detaches the view from WB's MDI frame, makes it a standalone popup,
// positions it behind Electron. WB's main frame is hidden off-screen.
function floatViewBehindElectron(wbMainHwnd, viewHwnd, electronHwnd, sx, sy, w, h) {
  log(`[bridge] Floating view=${viewHwnd} behind Electron at (${sx},${sy}) ${w}x${h}`);
  const script = `
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class F3 {
  [DllImport("user32")] public static extern int  GetWindowLong(IntPtr h, int i);
  [DllImport("user32")] public static extern int  SetWindowLong(IntPtr h, int i, int v);
  [DllImport("user32")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32")] public static extern IntPtr SetParent(IntPtr h, IntPtr p);
  [DllImport("user32")] public static extern bool ShowWindow(IntPtr h, int cmd);
}
"@ -Language CSharp 2>$null

$view = [IntPtr][long]${viewHwnd}
$wb   = [IntPtr][long]${wbMainHwnd}
$el   = [IntPtr][long]${electronHwnd}

$GWL_STYLE = -16; $GWL_EXSTYLE = -20
$WS_POPUP          = [int]0x80000000
$WS_VISIBLE        = 0x10000000
$WS_OVERLAPPEDWINDOW = 0x00CF0000
$WS_EX_NOACTIVATE  = 0x08000000   # clicking view won't steal focus from Electron
$WS_EX_TOOLWINDOW  = 0x00000080   # hide from taskbar

# 1. Detach view from MDI, make it top-level
[F3]::SetParent($view, [IntPtr]::Zero) | Out-Null

# 2. Strip overlapped style, set popup + visible
$s = [F3]::GetWindowLong($view, $GWL_STYLE)
$s = ($s -band (-bnot $WS_OVERLAPPEDWINDOW)) -bor $WS_POPUP -bor $WS_VISIBLE
[F3]::SetWindowLong($view, $GWL_STYLE, $s) | Out-Null

# 3. Add NOACTIVATE + TOOLWINDOW to extended style
$ex = [F3]::GetWindowLong($view, $GWL_EXSTYLE)
[F3]::SetWindowLong($view, $GWL_EXSTYLE, $ex -bor $WS_EX_NOACTIVATE -bor $WS_EX_TOOLWINDOW) | Out-Null

# 4. Position view just below Electron in z-order
$SWP = 0x0020 -bor 0x0040  # SWP_FRAMECHANGED | SWP_SHOWWINDOW
$ok = [F3]::SetWindowPos($view, $el, ${sx}, ${sy}, ${w}, ${h}, $SWP)
Write-Host "View positioned: $ok"

# 5. Hide WB main frame off-screen (keeps process running for tool logic)
[F3]::ShowWindow($wb, 0) | Out-Null
Write-Host "WB main frame hidden"
"done"
`;
  try {
    const out = ps(script);
    log('[bridge] floatViewBehindElectron:', out);
    return out;
  } catch (e) { log('[bridge] floatViewBehindElectron error:', e.message); return null; }
}

// ── Fire-and-forget async PS helper (non-blocking, no return value needed) ────
function psAsync(script) {
  const p = spawn('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command', script],
    { stdio: 'ignore' });
  p.unref();
}

// ── 4. Reposition view (debounced caller in main.js) ──────────────────────────
function repositionView(viewHwnd, electronHwnd, sx, sy, w, h) {
  const script = `
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class F4 {
  [DllImport("user32")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
}
"@ -Language CSharp 2>$null
[F4]::SetWindowPos([IntPtr][long]${viewHwnd}, [IntPtr][long]${electronHwnd}, ${sx}, ${sy}, ${w}, ${h}, 0x0044) | Out-Null
`;
  try { return ps(script); }
  catch (e) { log('[bridge] repositionView error:', e.message); return null; }
}

// Put wbHwnd immediately below electronHwnd in z-order (async, no block)
function fixZOrder(wbHwnd, electronHwnd) {
  psAsync(`
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class FZ { [DllImport("user32")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f); }
"@ -Language CSharp 2>$null
[FZ]::SetWindowPos([IntPtr][long]${wbHwnd},[IntPtr][long]${electronHwnd},0,0,0,0,3) | Out-Null`);
}

// Show (SW_SHOWNOACTIVATE=4) or hide (SW_HIDE=0) WB window (async)
function showWb(wbHwnd, show) {
  const cmd = show ? 4 : 0;
  psAsync(`
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class FS { [DllImport("user32")] public static extern bool ShowWindow(IntPtr h, int n); }
"@ -Language CSharp 2>$null
[FS]::ShowWindow([IntPtr][long]${wbHwnd},${cmd}) | Out-Null`);
}

// ── 5. Send WM_COMMAND to WB main frame (tool switching, undo, save) ──────────
function sendCommand(wbHwnd, commandId) {
  const script = `
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class F5 {
  [DllImport("user32")] public static extern IntPtr PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
}
"@ -Language CSharp 2>$null
[F5]::PostMessage([IntPtr][long]${wbHwnd}, 0x0111, [IntPtr]${commandId}, [IntPtr]0) | Out-Null
"sent"
`;
  try { return ps(script); }
  catch (e) { log('[bridge] sendCommand error:', e.message); return null; }
}

// ── Legacy aliases ─────────────────────────────────────────────────────────────
function floatBehindElectron(wbMainHwnd, electronHwnd, sx, sy, w, h) {
  // Used during initial setup before we have the view HWND
  const script = `
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class F6 {
  [DllImport("user32")] public static extern int  GetWindowLong(IntPtr h, int i);
  [DllImport("user32")] public static extern int  SetWindowLong(IntPtr h, int i, int v);
  [DllImport("user32")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32")] public static extern bool SetMenu(IntPtr h, IntPtr m);
}
"@ -Language CSharp 2>$null
$wb=[IntPtr][long]${wbMainHwnd}; $el=[IntPtr][long]${electronHwnd}
$WS_OVERLAPPEDWINDOW=0x00CF0000; $WS_POPUP=[int]0x80000000; $WS_VISIBLE=0x10000000
$WS_EX_NOACTIVATE=0x08000000
$s=[F6]::GetWindowLong($wb,-16)
[F6]::SetWindowLong($wb,-16,($s -band (-bnot $WS_OVERLAPPEDWINDOW)) -bor $WS_POPUP -bor $WS_VISIBLE) | Out-Null
$ex=[F6]::GetWindowLong($wb,-20)
[F6]::SetWindowLong($wb,-20,$ex -bor $WS_EX_NOACTIVATE) | Out-Null
[F6]::SetMenu($wb,[IntPtr]::Zero) | Out-Null
[F6]::SetWindowPos($wb,$el,${sx},${sy},${w},${h},0x0060) | Out-Null
"positioned"
`;
  try { return ps(script); }
  catch (e) { log('[bridge] floatBehindElectron error:', e.message); return null; }
}

function embedIntoElectron(p, c, x, y, w, h) { return floatBehindElectron(c, p, x, y, w, h); }
function resizeEmbedded(h, x, y, w, h2) { return repositionView(h, 0n, x, y, w, h2); }
function hideWorldBuilderChrome() { return 'noop'; }

module.exports = {
  getElectronHwnd,
  findMainWindowByPid,
  findViewWindow,
  floatViewBehindElectron,
  floatBehindElectron,
  repositionView,
  fixZOrder,
  showWb,
  sendCommand,
  // legacy
  embedIntoElectron,
  resizeEmbedded,
  hideWorldBuilderChrome,
};
