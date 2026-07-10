# Driving WorldBuilder with an AI agent

This editor isn't just a UI — while it runs, it exposes a **full REST API on
`http://127.0.0.1:8099`**. Anything that can send HTTP requests can build maps:
a script, a tool, or an AI coding agent like Claude Code. The UI itself uses the
exact same API, so everything you can click, an agent can do too.

## Quick start

1. Start `WorldBuilderUI.exe` (it launches the WorldBuilder engine automatically).
2. Verify the editor is ready:

```
curl http://127.0.0.1:8099/api/status
→ {"ok":true,"wbReady":true,"pipeReady":true,"viewMode":"2d","activeTool":"shapefill","mapLoaded":true}
```

Wait until both `wbReady` and `pipeReady` are `true` (takes ~15s after launch)
before sending anything else.

## The three rules that prevent 90% of the pain

1. **Know your coordinate system.** The map is a grid of cells; **1 cell = 10
   world units**. Most endpoints take world units (`wx`, `wy`). ShapeFill
   endpoints take **cell coordinates** (`cx`, `cy`) — world ÷ 10. Getting this
   wrong puts everything in the wrong corner of the map.
2. **Validate with data, not eyes.** After every change, read state back with a
   GET endpoint (`/api/map/ground_height`, `/api/map/objects`,
   `/api/map/waypoints`, `/api/map/info`) and check the numbers. Only trust a
   screenshot as secondary confirmation.
3. **Watch for blocking dialogs.** If a request returns `ok:true` but nothing
   changes and later requests time out, a native WorldBuilder dialog is probably
   waiting for a human click. Bring the WB window to front and dismiss it.

## Endpoint map

`api-server.js` is the canonical reference — every route lives there. The
families, with the ones you'll use most:

| Family | Endpoints | What it does |
|---|---|---|
| Status | `/api/status` | Readiness, active tool, view mode |
| Map lifecycle | `/api/map/new`, `/open`, `/load`, `/save`, `/save_to`, `/resize`, `/undo`, `/redo`, `/list` | Create/open/save maps (POST JSON: `{x, y, border, height}` for new) |
| Map state (GET) | `/api/map/info`, `/heightmap`, `/ground_height?wx=&wy=`, `/objects`, `/waypoints`, `/triggers`, `/roads`, `/bridges`, `/teams`, `/texturemap` | Read everything back — your ground truth |
| Terrain | `/api/terrain/mode`, `/terrain/set`, `/api/map/height/set`, `/api/floodfill`, mound/dig/ramp/feather/scorch `/set`+`/state` | Height brushes and fills |
| ShapeFill | `/api/shapefill/create`, `/set`, `/action`, `/state`, `/mode` | Rect/circle/polygon/line shapes with height gradients + textures (**cell coords!**) |
| Mesh Mold | `/api/meshmold/list`, `/activate`, `/set`, `/action`, `/state` | Stamp 3D height-field molds (position in world units via `posX`/`posY`) |
| Textures | `/api/texture/list`, `/set`, `/state`, `/swap`, `/all` | Paint terrain textures |
| Objects | `/api/place/object`, `/api/objects/palette`, `/api/object/select`, `/props`, `/set_prop`, `/delete_selected`, `/rotate_selected` | Place and edit units/structures/props |
| Nature | `/api/vegetation/types`, `/plant`, `/grove` | Trees and vegetation |
| Waypoints & areas | `/api/place/waypoint`, `/api/link/waypoints`, `/api/trigger`, `/trigger/set`, `/trigger/delete` | Player starts, paths, polygon triggers |
| Roads & bridges | `/api/place/road`, `/api/place/bridge`, `/api/ini/roads`, `/api/ini/bridges`, `/api/road/settool` | Road networks, bridges |
| Players/teams/scripts | `/api/sidelist`, `/sidelist/player/new`, `/sidelist/team`, `/sidelist/script`, `/sidelist/add_skirmish` | The full Map Setup layer, including scripts |
| Game data (GET) | `/api/ini/upgrades`, `/sciences`, `/specialpowers`, `/sounds`, `/music` | Enumerate what the game engine knows |
| View/camera | `/api/view/mode`, `/api/camera`, `/api/view/screen_to_world` | 2D/3D view, camera control |

## Worked example: a tiny map from nothing

```bash
# 1. New 200x200-cell map, flat at height 20
curl -X POST localhost:8099/api/map/new -H "Content-Type: application/json" \
  -d '{"x":200,"y":200,"border":10,"height":20}'

# 2. Stamp a hill in the middle (world units: cell 100 = 1000)
curl -X POST localhost:8099/api/meshmold/activate
curl -X POST localhost:8099/api/meshmold/set -H "Content-Type: application/json" \
  -d '{"model":"GaussianHill","posX":1000,"posY":1000,"scale":1.5}'
curl -X POST localhost:8099/api/meshmold/action -d '{"action":"apply"}'

# 3. VERIFY - did the ground actually rise?
curl "localhost:8099/api/map/ground_height?wx=1000&wy=1000"

# 4. Two player starts (world units)
curl -X POST localhost:8099/api/place/waypoint -H "Content-Type: application/json" \
  -d '{"wx":300,"wy":300,"name":"Player_1_Start"}'
curl -X POST localhost:8099/api/place/waypoint -H "Content-Type: application/json" \
  -d '{"wx":1700,"wy":1700,"name":"Player_2_Start"}'

# 5. Skirmish players + save
curl -X POST localhost:8099/api/sidelist/add_skirmish
curl -X POST localhost:8099/api/map/save_to -H "Content-Type: application/json" \
  -d '{"path":"C:\\path\\to\\MyMap\\MyMap.map"}'
```

## Using it with Claude Code

Point Claude Code at the API and let it iterate. A starter instruction that
works well — drop it in your project's `CLAUDE.md` or paste it as a prompt
(the 🤖 AI button in the editor's toolbar copies this same text):

```markdown
A C&C Generals Zero Hour map editor is running with a REST API on
http://127.0.0.1:8099 - 110+ endpoints covering the full editor (terrain,
textures, objects, waypoints, roads, players/teams/scripts, camera). The list
below is only a starter kit; the full endpoint map is in the guide (AI-DRIVING.md).

Workflow rules:
- First GET /api/status and wait until wbReady AND pipeReady are true (~15s
  after launch). POSTs take JSON bodies (Content-Type: application/json).
- Responses say {ok:true|false}. ok:true means "command accepted", NOT "it
  worked" - after EVERY mutation, verify with a GET (/api/map/info,
  /api/map/ground_height?wx=&wy=, /api/map/objects, /api/map/waypoints)
  before moving on.
- Coordinates: 1 cell = 10 world units. Most endpoints take world units
  (wx/wy/posX/posY); ShapeFill endpoints (/api/shapefill/*) take CELL
  coordinates (world / 10).
- If requests hang or time out: a native WorldBuilder dialog is probably
  blocking - ask me to dismiss it instead of retrying.

Starter endpoints: /api/map/new {x,y,border,height} - /api/meshmold/set+action
(stamp height molds, posX/posY world units) - /api/shapefill/create (shapes
with height gradients + textures, cell coords) - /api/floodfill -
/api/place/object {template,wx,wy} - /api/place/waypoint {name,wx,wy}
(Player_1_Start, Player_2_Start... must be sequential, no gaps) -
/api/sidelist/add_skirmish - /api/map/undo - /api/map/save_to {path}.
Save maps where the game finds them:
%USERPROFILE%\Documents\Command and Conquer Generals Zero Hour Data\Maps\<Name>\<Name>.map
```

Then ask for what you want: *"Build a 1v1 desert map with a defensible hill in
the middle, an oil derrick on each flank, and spawn points at least 1600 world
units apart."*

## Pitfalls (learned the hard way)

- **Big mesh molds are slow.** A mold covering a large part of the map can keep
  the engine busy for minutes; the API won't respond until it finishes. It's
  working, not stuck — check the WorldBuilder process CPU if unsure.
- **`ok:true` means "command accepted", not "it worked".** Always verify with a
  GET. Some commands are accepted but silently do nothing when a precondition
  is missing (no map loaded, wrong tool active).
- **Player starts must be sequential.** The game counts `Player_1_Start`,
  `Player_2_Start`, ... and stops at the first gap. Player_1 + Player_3 without
  Player_2 = a 1-player map.
- **Undo works over the API** (`/api/map/undo`) and covers most terrain and
  object operations — recover from mistakes instead of starting over.
