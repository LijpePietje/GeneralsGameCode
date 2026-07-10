// ── Texture panel ─────────────────────────────────────────────────────────────
function initTexturePanel() {
  document.getElementById('btn-tex-fg').addEventListener('click', async () => {
    await loadTextures();
    openTexPicker('tex-fg');
  });
  document.getElementById('btn-tex-bg').addEventListener('click', async () => {
    await loadTextures();
    openTexPicker('tex-bg');
  });

  document.getElementById('btn-tex-swap').addEventListener('click', async () => {
    await api('/texture/swap', {});
    // Swap displayed swatches
    const fgSwatch = document.getElementById('tex-swatch-fg');
    const bgSwatch = document.getElementById('tex-swatch-bg');
    const fgName   = document.getElementById('tex-fg-name');
    const bgName   = document.getElementById('tex-bg-name');
    const tmpBgImg   = fgSwatch.style.backgroundImage;
    const tmpBgColor = fgSwatch.style.backgroundColor;
    const tmpFgName  = fgName.textContent;
    fgSwatch.style.backgroundImage = bgSwatch.style.backgroundImage;
    fgSwatch.style.backgroundColor = bgSwatch.style.backgroundColor;
    fgName.textContent              = bgName.textContent;
    bgSwatch.style.backgroundImage  = tmpBgImg;
    bgSwatch.style.backgroundColor  = tmpBgColor;
    bgName.textContent               = tmpFgName;
  });

  const slWidth = document.getElementById('sl-tex-width');
  const valWidth = document.getElementById('val-tex-width');
  slWidth.addEventListener('input',  () => { valWidth.textContent = slWidth.value; });
  slWidth.addEventListener('change', () => api('/texture/set', { width: Number(slWidth.value) }));

  document.getElementById('tex-mode-group').querySelectorAll('.seg-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.getElementById('tex-mode-group').querySelectorAll('.seg-btn')
        .forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      const mode = Number(btn.dataset.val);
      api('/texture/set', { mode });
      document.getElementById('tex-pathing-section').style.display = mode === 1 ? '' : 'none';
      _texSyncImpassableOverlay();
    });
  });

  document.getElementById('tex-passable-group').querySelectorAll('.seg-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.getElementById('tex-passable-group').querySelectorAll('.seg-btn')
        .forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      api('/texture/set', { passable: Number(btn.dataset.val) });
      _texSyncImpassableOverlay();
    });
  });
}

// ── Impassable-overlay koppeling ──────────────────────────────────────────────
// Pathing painting is blind without the red overlay, so entering Pathing mode
// switches it on automatically. Leaving Pathing reverts to how the user had it:
// only switched back off if this code enabled it — a manual setting via the
// View popover is respected.
let _texImpassableAutoOn = false;

function _texSyncImpassableOverlay() {
  const mode = Number(document.querySelector('#tex-mode-group .seg-btn.active')?.dataset.val ?? 0);
  const want = _currentTool === 'texture' && mode === 1;
  if (want && !getViewToggle('show_impassable')) {
    setViewToggle('show_impassable', true);
    _texImpassableAutoOn = true;
  } else if (!want && _texImpassableAutoOn) {
    setViewToggle('show_impassable', false);
    _texImpassableAutoOn = false;
  }
}

// ── Object Properties panel ───────────────────────────────────────────────────
const AGGRESSIVENESS_LABELS = { '-2': 'Sleep', '-1': 'Passive', '0': 'Normal', '1': 'Alert', '2': 'Aggressive' };
const VETERANCY_LABELS      = { '0': 'Rookie', '1': 'Veteran', '2': 'Elite', '3': 'Heroic' };
const WEATHER_LABELS        = { '0': 'Normal', '1': 'Snowy' };
const TIME_LABELS           = { '0': 'Day', '1': 'Night', '2': 'Dawn', '3': 'Dusk' };

let _propsPollTimer = null;
let _lastPropsType  = null;
let _lastPropsKey   = null;   // identity of the current selection
let _dismissedKey   = null;   // ✕ hides the panel for THIS selection only

function _propsIdentity(p) {
  return `${p.type}:${p.name || ''}:${p.templateName || ''}:${Math.round(p.x1 || 0)},${Math.round(p.y1 || 0)}`;
}

function startPropsPolling() {
  if (_propsPollTimer) return;
  syncObjProps();
  _propsPollTimer = setInterval(syncObjProps, 1500);
}
function stopPropsPolling() {
  if (_propsPollTimer) { clearInterval(_propsPollTimer); _propsPollTimer = null; }
  hideObjProps();
}

function hideObjProps() {
  document.getElementById('obj-props-panel').classList.add('hidden');
  _lastPropsType = null;
}

async function syncObjProps() {
  const r = await apiGet('/object/props');
  if (!r || !r.ok) return;
  if (r.type === 'none') { hideObjProps(); _dismissedKey = null; return; }
  // ✕ dismissed this selection — stay hidden until the selection changes
  _lastPropsKey = _propsIdentity(r);
  if (_dismissedKey) {
    if (_lastPropsKey === _dismissedKey) return;
    _dismissedKey = null;
  }
  // Keep _rdSelected in sync when WB has a bridge selected
  if (r.type === 'bridge') {
    const x1 = Math.round(r.x1 || 0), y1 = Math.round(r.y1 || 0);
    if (_rdSelected.kind !== 'bridge' || _rdSelected.x1 !== x1 || _rdSelected.y1 !== y1) {
      _rdSelected = { kind: 'bridge', x1, y1, data: r };
    }
  }
  renderObjProps(r);
}

function renderObjProps(p) {
  const panel = document.getElementById('obj-props-panel');
  const body  = document.getElementById('obj-props-body');

  // typeKey: bridges keyed by position so switching bridges rebuilds the DOM
  const typeKey = p.type === 'bridge'
    ? `bridge:${Math.round(p.x1 || 0)},${Math.round(p.y1 || 0)}`
    : p.type + (p.isUnit ? ':unit' : p.isStructure ? ':structure' : ':other');

  if (_lastPropsType !== typeKey) {
    _lastPropsType = typeKey;
    body.innerHTML = '';
    if (p.type === 'waypoint')    buildWaypointProps(body);
    else if (p.type === 'bridge') buildBridgeProps(body, p);
    else                          buildObjectProps(body, p);
  }

  // Update values without rebuilding
  if (p.type === 'waypoint') {
    setOpValIfNotFocused('op-wp-name', p.name);
    setOpValIfNotFocused('op-wp-wx', Math.round(p.wx ?? 0));
    setOpValIfNotFocused('op-wp-wy', Math.round(p.wy ?? 0));
    // Player starts are standalone markers — a path makes no sense there
    const isPlayerStart = /^Player_\d+_Start$/i.test(p.name || '');
    const pathRow = document.getElementById('op-wp-path-row');
    if (pathRow) pathRow.style.display = isPlayerStart ? 'none' : '';
    const derivedPath = ((p.name || '').match(/^(.+)_(\d+)$/) || [])[1] || '';
    setOpValIfNotFocused('op-wp-path', isPlayerStart ? '' : (p.pathLabel || derivedPath));
    const nodeMatch = (p.name || '').match(/^(.+)_(\d+)$/);
    const nodeRow = document.getElementById('op-wp-node-row');
    if (nodeRow) {
      nodeRow.style.display = nodeMatch ? '' : 'none';
      if (nodeMatch) setOpVal('op-wp-node', nodeMatch[2]);
    }
  } else if (p.type === 'bridge') {
    const x1 = Math.round(p.x1 || 0), y1 = Math.round(p.y1 || 0);
    const x2 = Math.round(p.x2 || 0), y2 = Math.round(p.y2 || 0);
    const startEl = document.getElementById('rd-op-start');
    const endEl   = document.getElementById('rd-op-end');
    if (startEl) startEl.textContent = `${x1}, ${y1}`;
    if (endEl)   endEl.textContent   = `${x2}, ${y2}`;
    setOpValIfNotFocused('op-name',   p.name);
    setOpValIfNotFocused('op-team',   p.team);
    setOpValIfNotFocused('op-script', p.script);
    setOpValIfNotFocused('op-health', p.health);
    setOpValIfNotFocused('op-maxhp',  p.maxHP);
    setOpSelectIfNotFocused('op-weather', p.weather);
    setOpSelectIfNotFocused('op-time',    p.time);
    setOpCheck('op-enabled',       p.enabled);
    setOpCheck('op-indestructible', p.indestructible);
    setOpCheck('op-unsellable',    p.unsellable);
    setOpCheck('op-targetable',    p.targetable);
    setOpCheck('op-powered',       p.powered);
    setOpCheck('op-selectable',    p.selectable);
  } else {
    setOpVal('op-template',    p.templateName);
    setOpValIfNotFocused('op-wx', p.wx?.toFixed(1));
    setOpValIfNotFocused('op-wy', p.wy?.toFixed(1));
    setOpValIfNotFocused('op-angle',  p.angle?.toFixed(1));
    setOpValIfNotFocused('op-team',   p.team);
    setOpValIfNotFocused('op-name',   p.name);
    setOpValIfNotFocused('op-script', p.script);
    setOpValIfNotFocused('op-health', p.health);
    setOpValIfNotFocused('op-maxhp',  p.maxHP);
    setOpSelectIfNotFocused('op-aggress', p.aggressiveness);
    setOpSelectIfNotFocused('op-veteran', p.veterancy);
    setOpSelectIfNotFocused('op-weather', p.weather);
    setOpSelectIfNotFocused('op-time',    p.time);
    setOpCheck('op-enabled',      p.enabled);
    setOpCheck('op-indestructible', p.indestructible);
    setOpCheck('op-unsellable',    p.unsellable);
    setOpCheck('op-targetable',    p.targetable);
    setOpCheck('op-powered',       p.powered);
    setOpCheck('op-selectable',    p.selectable);
    setOpCheck('op-airecruitable', p.aiRecruitable);
  }

  panel.classList.remove('hidden');
}

function buildBridgeProps(body, p) {
  const x1 = Math.round(p.x1 || 0), y1 = Math.round(p.y1 || 0);
  const x2 = Math.round(p.x2 || 0), y2 = Math.round(p.y2 || 0);
  const title = document.getElementById('obj-props-title');
  if (title) title.textContent = 'Bridge Properties';
  const weOpts = Object.entries(WEATHER_LABELS).map(([v,l]) => `<option value="${v}">${l}</option>`).join('');
  const tiOpts = Object.entries(TIME_LABELS).map(([v,l]) => `<option value="${v}">${l}</option>`).join('');
  body.innerHTML = `
    <div class="op-section">
      <div class="op-section-label">General</div>
      <div class="op-row"><span class="op-label">Type</span><span class="op-value readonly" id="op-template">${p.templateName || '?'}</span></div>
      <div class="op-row"><span class="op-label">Name</span><input class="op-value" id="op-name" type="text" placeholder="(none)"></div>
      <div class="op-row"><span class="op-label">Team</span><input class="op-value" id="op-team" type="text"></div>
      <div class="op-row"><span class="op-label">Script</span><input class="op-value" id="op-script" type="text" placeholder="(none)"></div>
    </div>
    <div class="op-divider"></div>
    <div class="op-section">
      <div class="op-section-label">Position</div>
      <div class="op-row"><span class="op-label">Start</span><span class="op-value readonly" id="rd-op-start">${x1}, ${y1}</span></div>
      <div class="op-row"><span class="op-label">End</span><span class="op-value readonly" id="rd-op-end">${x2}, ${y2}</span></div>
    </div>
    <div class="op-divider"></div>
    <div class="op-section">
      <div class="op-section-label">Logical</div>
      <div class="op-row"><span class="op-label">Health %</span><input class="op-value" id="op-health" type="number" min="0" max="100"></div>
      <div class="op-row"><span class="op-label">Max HP</span><input class="op-value" id="op-maxhp" type="number" min="0"></div>
      <div class="op-checks">
        <label class="op-check-label"><input type="checkbox" id="op-enabled"> Enabled</label>
        <label class="op-check-label"><input type="checkbox" id="op-indestructible"> Indestructible</label>
        <label class="op-check-label"><input type="checkbox" id="op-unsellable"> Unsellable</label>
        <label class="op-check-label"><input type="checkbox" id="op-targetable"> Targetable</label>
        <label class="op-check-label"><input type="checkbox" id="op-powered"> Powered</label>
        <label class="op-check-label"><input type="checkbox" id="op-selectable"> Selectable</label>
      </div>
    </div>
    <div class="op-divider"></div>
    <div class="op-section">
      <div class="op-section-label">Visual</div>
      <div class="op-row"><span class="op-label">Weather</span><select class="op-select" id="op-weather">${weOpts}</select></div>
      <div class="op-row"><span class="op-label">Time</span><select class="op-select" id="op-time">${tiOpts}</select></div>
    </div>`;

  const wireInput = (id, key) => {
    const el = document.getElementById(id);
    if (!el) return;
    el.addEventListener('change', () => api('/object/set_prop', { key, value: el.type === 'number' ? Number(el.value) : el.value }));
  };
  const wireSelect = (id, key) => {
    const el = document.getElementById(id);
    if (!el) return;
    el.addEventListener('change', () => api('/object/set_prop', { key, value: Number(el.value) }));
  };
  const wireCheck = (id, key) => {
    const el = document.getElementById(id);
    if (!el) return;
    el.addEventListener('change', () => api('/object/set_prop', { key, value: el.checked }));
  };
  wireInput('op-name',   'name');
  wireInput('op-team',   'team');
  wireInput('op-script', 'script');
  wireInput('op-health', 'health');
  wireInput('op-maxhp',  'maxHP');
  wireSelect('op-weather', 'weather');
  wireSelect('op-time',    'time');
  wireCheck('op-enabled',        'enabled');
  wireCheck('op-indestructible', 'indestructible');
  wireCheck('op-unsellable',     'unsellable');
  wireCheck('op-targetable',     'targetable');
  wireCheck('op-powered',        'powered');
  wireCheck('op-selectable',     'selectable');
}

function setOpVal(id, v) {
  const el = document.getElementById(id);
  if (el) el.textContent = v ?? '';
}
function setOpValIfNotFocused(id, v) {
  const el = document.getElementById(id);
  if (el && document.activeElement !== el) el.value = v ?? '';
}
function setOpSelectIfNotFocused(id, v) {
  const el = document.getElementById(id);
  if (el && document.activeElement !== el) el.value = String(v ?? 0);
}
function setOpCheck(id, v) {
  const el = document.getElementById(id);
  if (el) el.checked = !!v;
}

function buildWaypointProps(body) {
  body.innerHTML = `
    <div class="op-section">
      <div class="op-section-label">Waypoint</div>
      <div class="op-row"><span class="op-label">Name</span><input class="op-value wb-form-input" id="op-wp-name" style="width:100%"></div>
      <div class="op-row" id="op-wp-path-row"><span class="op-label">Path</span><input class="op-value wb-form-input" id="op-wp-path" placeholder="(none)" style="width:100%"></div>
      <div class="op-row" id="op-wp-node-row" style="display:none"><span class="op-label">Node #</span><span class="op-value readonly" id="op-wp-node"></span></div>
      <div class="op-row"><span class="op-label">World X</span><input class="op-value wb-form-input" id="op-wp-wx" type="number" step="1" style="width:100%"></div>
      <div class="op-row"><span class="op-label">World Y</span><input class="op-value wb-form-input" id="op-wp-wy" type="number" step="1" style="width:100%"></div>
      <div class="op-action-row" style="margin-top:8px;display:flex;gap:6px">
        <button class="action-btn primary" id="op-wp-apply-btn" style="flex:1">Apply</button>
        <button class="action-btn danger"  id="op-wp-delete-btn" style="flex:1">Delete</button>
      </div>
      <div id="op-wp-feedback" class="feedback-text"></div>
    </div>`;

  document.getElementById('op-wp-apply-btn').addEventListener('click', async () => {
    const name = document.getElementById('op-wp-name').value.trim();
    const path = document.getElementById('op-wp-path').value.trim();
    const wx   = Math.round(parseFloat(document.getElementById('op-wp-wx').value));
    const wy   = Math.round(parseFloat(document.getElementById('op-wp-wy').value));
    const calls = [];
    if (name) calls.push(api('/object/set_prop', { key: 'name',       value: name }));
    const pathHidden = document.getElementById('op-wp-path-row')?.style.display === 'none';
    if (!pathHidden) calls.push(api('/object/set_prop', { key: 'pathLabel', value: path }));
    if (!isNaN(wx)) calls.push(api('/object/set_prop', { key: 'wx', value: wx }));
    if (!isNaN(wy)) calls.push(api('/object/set_prop', { key: 'wy', value: wy }));
    const results = await Promise.all(calls);
    const allOk = results.every(r => r?.ok);
    showFeedback('op-wp-feedback', allOk ? 'Applied' : 'Failed', allOk);
    if (allOk) syncWaypointsState();
  });

  document.getElementById('op-wp-delete-btn').addEventListener('click', async () => {
    const r = await api('/object/delete_selected', {});
    showFeedback('op-wp-feedback', r?.ok ? 'Deleted' : (r?.error || 'Failed'), !!r?.ok);
    if (r?.ok) { hideObjProps(); syncWaypointsState(); }
  });
}

function buildObjectProps(body, p) {
  const agOpts = Object.entries(AGGRESSIVENESS_LABELS).map(([v,l]) => `<option value="${v}">${l}</option>`).join('');
  const vtOpts = Object.entries(VETERANCY_LABELS).map(([v,l]) => `<option value="${v}">${l}</option>`).join('');
  const weOpts = Object.entries(WEATHER_LABELS).map(([v,l]) => `<option value="${v}">${l}</option>`).join('');
  const tiOpts = Object.entries(TIME_LABELS).map(([v,l]) => `<option value="${v}">${l}</option>`).join('');

  const showUnit    = !!p.isUnit;
  const showPowered = !!p.isStructure;

  body.innerHTML = `
    <div class="op-section">
      <div class="op-section-label">General</div>
      <div class="op-row"><span class="op-label">Type</span><span class="op-value readonly" id="op-template"></span></div>
      <div class="op-row"><span class="op-label">Name</span><input class="op-value" id="op-name" type="text" placeholder="(none)"></div>
      <div class="op-row"><span class="op-label">Team</span><input class="op-value" id="op-team" type="text"></div>
      <div class="op-row"><span class="op-label">Script</span><input class="op-value" id="op-script" type="text" placeholder="(none)"></div>
    </div>
    <div class="op-divider"></div>
    <div class="op-section">
      <div class="op-section-label">Position</div>
      <div class="op-row"><span class="op-label">World X</span><input class="op-value" id="op-wx" type="number"></div>
      <div class="op-row"><span class="op-label">World Y</span><input class="op-value" id="op-wy" type="number"></div>
      <div class="op-row"><span class="op-label">Angle</span><input class="op-value" id="op-angle" type="number" min="0" max="359"></div>
    </div>
    <div class="op-divider"></div>
    <div class="op-section">
      <div class="op-section-label">Logical</div>
      <div class="op-row"><span class="op-label">Health %</span><input class="op-value" id="op-health" type="number" min="0" max="100"></div>
      <div class="op-row"><span class="op-label">Max HP</span><input class="op-value" id="op-maxhp" type="number" min="0"></div>
      ${showUnit ? `
      <div class="op-row"><span class="op-label">Aggress.</span><select class="op-select" id="op-aggress">${agOpts}</select></div>
      <div class="op-row"><span class="op-label">Veterancy</span><select class="op-select" id="op-veteran">${vtOpts}</select></div>
      ` : ''}
      <div class="op-checks">
        <label class="op-check-label"><input type="checkbox" id="op-enabled"> Enabled</label>
        <label class="op-check-label"><input type="checkbox" id="op-indestructible"> Indestructible</label>
        <label class="op-check-label"><input type="checkbox" id="op-unsellable"> Unsellable</label>
        <label class="op-check-label"><input type="checkbox" id="op-targetable"> Targetable</label>
        ${showPowered ? `<label class="op-check-label"><input type="checkbox" id="op-powered"> Powered</label>` : ''}
        <label class="op-check-label"><input type="checkbox" id="op-selectable"> Selectable</label>
        ${showUnit ? `<label class="op-check-label"><input type="checkbox" id="op-airecruitable"> AI Recruitable</label>` : ''}
      </div>
    </div>
    <div class="op-divider"></div>
    <div class="op-section">
      <div class="op-section-label">Visual</div>
      <div class="op-row"><span class="op-label">Weather</span><select class="op-select" id="op-weather">${weOpts}</select></div>
      <div class="op-row"><span class="op-label">Time</span><select class="op-select" id="op-time">${tiOpts}</select></div>
    </div>`;

  // Wire up editable fields — send on change/blur
  const wireInput = (id, key) => {
    const el = document.getElementById(id);
    if (!el) return;
    el.addEventListener('change', () => api('/object/set_prop', { key, value: el.type === 'number' ? Number(el.value) : el.value }));
  };
  const wireSelect = (id, key) => {
    const el = document.getElementById(id);
    if (!el) return;
    el.addEventListener('change', () => api('/object/set_prop', { key, value: Number(el.value) }));
  };
  const wireCheck = (id, key) => {
    const el = document.getElementById(id);
    if (!el) return;
    el.addEventListener('change', () => api('/object/set_prop', { key, value: el.checked }));
  };

  wireInput('op-name',   'name');
  wireInput('op-team',   'team');
  wireInput('op-script', 'script');
  wireInput('op-wx',     'wx');
  wireInput('op-wy',     'wy');
  wireInput('op-angle',  'angle');
  wireInput('op-health', 'health');
  wireInput('op-maxhp',  'maxHP');
  if (showUnit) {
    wireSelect('op-aggress', 'aggressiveness');
    wireSelect('op-veteran', 'veterancy');
    wireCheck('op-airecruitable', 'aiRecruitable');
  }
  wireSelect('op-weather', 'weather');
  wireSelect('op-time',    'time');
  wireCheck('op-enabled',       'enabled');
  wireCheck('op-indestructible','indestructible');
  wireCheck('op-unsellable',    'unsellable');
  wireCheck('op-targetable',    'targetable');
  if (showPowered) wireCheck('op-powered', 'powered');
  wireCheck('op-selectable', 'selectable');
}

function initObjPropsPanel() {
  document.getElementById('obj-props-close').addEventListener('click', () => {
    hideObjProps();
    // Keep polling: stay hidden for this selection, reappear on the next one
    _dismissedKey = _lastPropsKey;
  });
}

