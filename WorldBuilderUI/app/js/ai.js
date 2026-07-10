// ── AI Agent panel ────────────────────────────────────────────────────────────
// Surfaces the REST API (the editor's real differentiator) inside the UI itself:
// connection test, a copy-paste starter prompt for coding agents, and a link to
// the full AI-DRIVING.md guide on GitHub.

// Lives in the public SuperHackers fork (the ProjectWorldbuilder repo is private)
const AI_GUIDE_URL = 'https://github.com/LijpePietje/GeneralsGameCode/blob/feature/shapefill-tool/WorldBuilderUI/app/AI-DRIVING.md';

const AI_STARTER_PROMPT = `A C&C Generals Zero Hour map editor is running with a REST API on
http://127.0.0.1:8099. Check /api/status first (wbReady and pipeReady must be
true). Coordinates: 1 cell = 10 world units; most endpoints take world units,
ShapeFill endpoints take cell coordinates. After EVERY mutation, verify with a
GET endpoint (/api/map/ground_height, /api/map/objects, /api/map/waypoints)
before moving on - never assume a change worked. If requests start timing out,
tell me to check for a blocking dialog in the WorldBuilder window.

Key endpoints: /api/map/new {x,y,border,height} - /api/meshmold/set+action
(stamp height molds, posX/posY in world units) - /api/floodfill -
/api/place/object {template,wx,wy} - /api/place/waypoint {name,wx,wy}
(Player_1_Start, Player_2_Start... must be sequential, no gaps) -
/api/sidelist/add_skirmish - /api/map/save_to {path} - /api/map/undo.
Full guide: ${AI_GUIDE_URL}`;

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
