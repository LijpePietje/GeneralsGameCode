# WorldBuilder Modern UI — Installation Guide

This Electron app embeds the original WorldBuilder editor (WorldBuilderZH.exe) and adds a modern sidebar for terrain painting, waypoints, objects, scripts, and more.

---

## Requirements

| Requirement | Version | Notes |
|---|---|---|
| Windows | 10 or 11 | Required — WB is a Win32 app |
| C&C Generals Zero Hour | any | EA App, Origin, or disc install |
| Node.js | 18+ | [nodejs.org](https://nodejs.org) |
| WorldBuilderZH.exe | patched | See step 2 below |

---

## Step 1 — Clone or download the repo

```powershell
git clone https://github.com/LijpePietje/ProjectWorldbuilder.git
cd ProjectWorldbuilder
```

Or download as ZIP via **GitHub → Code → Download ZIP** and extract.

---

## Step 2 — Install the patched WorldBuilderZH.exe

The custom exe adds the pipe server that the UI needs to communicate with WorldBuilder. It is **not included** in the repo (binary file).

**Option A — Copy from a build PC**

Copy `WorldBuilderZH.exe` to your Zero Hour installation folder:

```
C:\Program Files (x86)\Origin Games\Command and Conquer Generals Zero Hour\
  Command and Conquer Generals Zero Hour\WorldBuilderZH.exe
```

> Replace the existing file. Keep a backup of the original if needed.

**Option B — Build it yourself**

Requires Visual Studio 2022 + CMake. See [TheSuperHackers/README.md](../TheSuperHackers/README.md) for full build instructions. After building, copy:

```powershell
Copy-Item "TheSuperHackers\build\win32\GeneralsMD\Release\WorldBuilderZH.exe" `
    "C:\Program Files (x86)\Origin Games\Command and Conquer Generals Zero Hour\Command and Conquer Generals Zero Hour\WorldBuilderZH.exe" -Force
```

---

## Step 3 — Install Node dependencies

```powershell
cd worldbuilder-ui-poc
npm install
```

---

## Step 4 — Start the app

```powershell
npm start
```

The Electron window opens, WorldBuilder starts automatically in the background and is embedded in the viewport. The pipe connection is established within a few seconds.

---

## First use

1. Use **File → Open** inside WorldBuilder (or the toolbar) to load a `.map` file
2. The sidebar panels (Terrain, Texture, Shapefill, Objects, Waypoints, Scripts…) become active once a map is loaded
3. Changes are saved via the WB native save (Ctrl+S in WB or the save button)

---

## Troubleshooting

**"Pipe not ready / commands skipped"**
WorldBuilder is still starting. Wait 3–5 seconds after the WB window appears.

**Buttons not responding after deploying a new exe**
WorldBuilder is still running with the old exe. Close WB completely, then restart the Electron app (`npm start`).

**CTRL+SHIFT+R**
After any JS/HTML change, do a hard refresh in the Electron window (CTRL+SHIFT+R) to bypass the browser cache. CTRL+R alone is not enough.

**WorldBuilder not found**
Verify the exe path in `worldbuilder-ui-poc/main.js` matches your Zero Hour install location.

---

## For AI agents continuing this project

- Architecture and C++ conventions: [`CLAUDE.md`](../CLAUDE.md) and [`WORLDBUILDER_CLAUDE.md`](../WORLDBUILDER_CLAUDE.md)
- Pipe protocol and message IDs: [`worldbuilder-cpp/ShapeFillTool/include/WbPipeServer.h`](../worldbuilder-cpp/ShapeFillTool/include/WbPipeServer.h)
- API endpoints: [`worldbuilder-ui-poc/api-server.js`](api-server.js)
- UI panels and event logic: [`worldbuilder-ui-poc/js/`](js/) — modules loaded in order, `boot.js` last
