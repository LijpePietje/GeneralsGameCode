// ── AI Agent panel ────────────────────────────────────────────────────────────
// Surfaces the REST API (the editor's real differentiator) inside the UI itself:
// connection test, a copy-paste starter prompt for coding agents, and a link to
// the full AI-DRIVING.md guide on GitHub.

// Lives in the public SuperHackers fork (the ProjectWorldbuilder repo is private)
const AI_GUIDE_URL = 'https://github.com/LijpePietje/GeneralsGameCode/blob/feature/shapefill-tool/WorldBuilderUI/app/AI-DRIVING.md';

const AI_STARTER_PROMPT = `A C&C Generals Zero Hour map editor is running with a REST API on
http://127.0.0.1:8099 - 110+ endpoints covering the full editor (terrain,
textures, objects, waypoints, roads, players/teams/scripts, camera). The list
below is only a starter kit; the full endpoint map is in the guide:
${AI_GUIDE_URL}

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
%USERPROFILE%\\Documents\\Command and Conquer Generals Zero Hour Data\\Maps\\<Name>\\<Name>.map`;

function initAiPanel() {
  const testBtn = document.getElementById('ai-btn-test');
  if (!testBtn || testBtn.dataset.wired) return;
  testBtn.dataset.wired = '1';

  testBtn.addEventListener('click', async () => {
    const out = document.getElementById('ai-test-result');
    out.textContent = 'Testing…';
    try {
      const r = await apiGet('/status');
      out.textContent = (r && r.ok && r.wbReady && r.pipeReady)
        ? '✓ API is live - engine connected and ready'
        : `⚠ API responds but not ready yet (wbReady:${r?.wbReady} pipeReady:${r?.pipeReady})`;
    } catch {
      out.textContent = '✗ No response - is the editor fully started?';
    }
  });

  document.getElementById('ai-btn-copy').addEventListener('click', async () => {
    const out = document.getElementById('ai-copy-result');
    try {
      await navigator.clipboard.writeText(AI_STARTER_PROMPT);
      out.textContent = '✓ Copied - paste it into your agent\'s prompt or CLAUDE.md';
    } catch {
      out.textContent = '✗ Clipboard unavailable';
    }
    setTimeout(() => { out.textContent = ''; }, 5000);
  });

  document.getElementById('ai-btn-guide').addEventListener('click', () => {
    const { shell } = require('electron');
    shell.openExternal(AI_GUIDE_URL);
  });
}
