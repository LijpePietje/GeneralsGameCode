# WorldBuilder Modern UI

A modern, AI-drivable interface for the C&C Generals: Zero Hour WorldBuilder.
An Electron app embeds the real WorldBuilder engine (built from this repo, with
an added named-pipe control server) behind a new UI — and exposes everything as
a REST API on `http://127.0.0.1:8099`, so scripts and AI agents can build maps.

See [app/AI-DRIVING.md](app/AI-DRIVING.md) for the API guide.

## Download (no build needed)

Prebuilt binaries are on the
[**Releases page**](https://github.com/LijpePietje/GeneralsGameCode/releases) —
grab `WorldBuilderUI.exe`, `WorldBuilderZH.exe` and `Editor-Molds.zip`, follow
the 3-step install in the release notes, and skip the build sections below
entirely.

## What's in this folder

```
WorldBuilderUI/
  app/                  The Electron app (UI + REST API + WB process embedding)
  building_icons/       Object palette icons  (served by the API)
  terrain_textures/     Texture swatches      (served by the API)
  REAL_OBJECT_NAMES.txt Object palette source data
```

The layout matters: `app/` resolves its assets one level up (`../building_icons`
etc.), both in dev and when packaging.

## Build the engine (WorldBuilderZH.exe)

This UI drives the WorldBuilder built from **this branch** — it needs the
`WbPipeServer` (named pipe `\\.\pipe\wb-engine`) that lives in
`GeneralsMD/Code/Tools/WorldBuilder/`. A stock WorldBuilder won't respond.

1. Configure/build this repo as usual (CMake, VC6/MSVC — see the root README),
   target `z_worldbuilder`, config Release.
2. Copy the result into your Zero Hour install folder:
   `build/win32/GeneralsMD/Release/WorldBuilderZH.exe` →
   `<game folder>/WorldBuilderZH.exe`
3. Copy the mesh molds: `app/data/molds/*.w3d` →
   `<game folder>/Data/Editor/Molds/`

## Run the UI

**Dev mode:**

```
cd app
npm install
npm start
```

Note: the dev fallback path for `WorldBuilderZH.exe` points at a default Origin
install; if yours lives elsewhere, drop a packaged build in your game folder
instead (below), or adjust `_WB_DEV_FALLBACK` in `app/main.js`.

**Shareable single exe:**

```
cd app
npm install
npm run dist
```

Produces `app/dist/WorldBuilderUI.exe` (~67 MB — it bundles Chromium/Node;
that's Electron's floor). Put it in the game folder **next to
WorldBuilderZH.exe** and double-click. It finds the engine by sitting next to
it; `debug.log` and settings are written next to the exe.

## Verify

```
curl http://127.0.0.1:8099/api/status
→ {"ok":true,"wbReady":true,"pipeReady":true,...}
```

Both flags true (~15s after launch) = the whole chain works.
