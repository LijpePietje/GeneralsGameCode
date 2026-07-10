// ── Start screen ──────────────────────────────────────────────────────────────
// Fullscreen opening screen: New Map / Load Map. New Map shows a fullscreen
// setup form (size, game mode, subtype); both paths land on the Map Setup tab.

let _startMode = localStorage.getItem('mapsetup_mode') || 'skirmish';
let _startPvp  = localStorage.getItem('mapsetup_pvp')  || '2v2';
let _startCoop = localStorage.getItem('mapsetup_coop') || 'aod';

function showStartScreen() {
  startShowChoice();
  document.getElementById('start-screen').classList.remove('hidden');
  ipcRenderer.send('viewport-mouse', false);
}

function hideStartScreen() {
  document.getElementById('start-screen').classList.add('hidden');
}

function startScreenVisible() {
  return !document.getElementById('start-screen').classList.contains('hidden');
}

function startShowChoice() {
  document.getElementById('start-choice').style.display = '';
  document.getElementById('start-newmap').style.display = 'none';
}

function startShowNewMap() {
  document.getElementById('start-choice').style.display = 'none';
  document.getElementById('start-newmap').style.display = '';
  _startUpdateModeUI();
  _startUpdatePlayable();
}

function _startUpdateModeUI() {
  document.querySelectorAll('[data-startmode]').forEach(b =>
    b.classList.toggle('active', b.dataset.startmode === _startMode));
  document.getElementById('start-sub-skirmish').style.display = _startMode === 'skirmish' ? '' : 'none';
  document.getElementById('start-sub-coop').style.display     = _startMode === 'coop'     ? '' : 'none';
  document.getElementById('start-sub-mission').style.display  = _startMode === 'mission'  ? '' : 'none';
  document.querySelectorAll('[data-startpvp]').forEach(b =>
    b.classList.toggle('active', b.dataset.startpvp === _startPvp));
  document.querySelectorAll('[data-startcoop]').forEach(b =>
    b.classList.toggle('active', b.dataset.startcoop === _startCoop));
}

function _startUpdatePlayable() {
  const x = parseInt(document.getElementById('start-nm-x').value) || 200;
  const y = parseInt(document.getElementById('start-nm-y').value) || 200;
  const b = parseInt(document.getElementById('start-nm-border').value) || 10;
  document.getElementById('start-nm-playable').textContent = `${x - 2 * b} × ${y - 2 * b}`;
  // Highlight matching preset (if any)
  document.querySelectorAll('[data-startsize]').forEach(btn =>
    btn.classList.toggle('active', parseInt(btn.dataset.startsize) === x && x === y));
}

async function startCreateMap() {
  const x      = parseInt(document.getElementById('start-nm-x').value);
  const y      = parseInt(document.getElementById('start-nm-y').value);
  const border = parseInt(document.getElementById('start-nm-border').value);
  const height = parseInt(document.getElementById('start-nm-height').value);
  const st     = document.getElementById('start-nm-status');
  if (!x || !y) { st.textContent = 'Enter a map size'; return; }

  const btn = document.getElementById('start-nm-create');
  btn.disabled = true;
  st.textContent = 'Creating map…';
  const r = await api('/map/new', { x, y, border, height });
  btn.disabled = false;
  if (!r || !r.ok) {
    st.textContent = 'WorldBuilder is not ready yet — try again in a moment';
    return;
  }
  st.textContent = '';

  // Sync chosen mode/subtype into the Map Setup panel via its own buttons
  // (their handlers update state, localStorage and UI in one go)
  document.querySelector(`[data-msmode="${_startMode}"]`)?.click();
  if (_startMode === 'skirmish') document.querySelector(`[data-pvp="${_startPvp}"]`)?.click();
  if (_startMode === 'coop')     document.querySelector(`[data-coop="${_startCoop}"]`)?.click();

  autoAddSkirmishPlayers();
  setTimeout(refreshTerrainMapSize, 1200);
  hideStartScreen();
  startGotoMapSetup();
}

function startGotoMapSetup() {
  document.querySelector('.tool-btn[data-tool="mapsetup"]')?.click();
}

// Called from loadSelectedMap() after a map load completes
function startScreenOnMapLoaded() {
  if (!startScreenVisible()) return;
  hideStartScreen();
  startGotoMapSetup();
}

function initStartScreen() {
  document.getElementById('start-new').addEventListener('click', startShowNewMap);
  document.getElementById('start-load').addEventListener('click', () => openLoadModal());
  document.getElementById('start-skip').addEventListener('click', hideStartScreen);
  document.getElementById('start-nm-back').addEventListener('click', startShowChoice);
  document.getElementById('start-nm-create').addEventListener('click', startCreateMap);

  // Size presets fill the inputs (square maps; custom via the inputs)
  document.querySelectorAll('[data-startsize]').forEach(btn => {
    btn.addEventListener('click', () => {
      document.getElementById('start-nm-x').value = btn.dataset.startsize;
      document.getElementById('start-nm-y').value = btn.dataset.startsize;
      _startUpdatePlayable();
    });
  });
  ['start-nm-x', 'start-nm-y', 'start-nm-border'].forEach(id =>
    document.getElementById(id).addEventListener('input', _startUpdatePlayable));

  document.querySelectorAll('[data-startmode]').forEach(btn => {
    btn.addEventListener('click', () => { _startMode = btn.dataset.startmode; _startUpdateModeUI(); });
  });
  document.querySelectorAll('[data-startpvp]').forEach(btn => {
    btn.addEventListener('click', () => { _startPvp = btn.dataset.startpvp; _startUpdateModeUI(); });
  });
  document.querySelectorAll('[data-startcoop]').forEach(btn => {
    btn.addEventListener('click', () => { _startCoop = btn.dataset.startcoop; _startUpdateModeUI(); });
  });

  // Esc in the New Map form goes back to the choice screen
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape' && startScreenVisible()
        && document.getElementById('start-newmap').style.display !== 'none'
        && document.getElementById('modal-load').classList.contains('hidden')) {
      startShowChoice();
    }
  });
}
