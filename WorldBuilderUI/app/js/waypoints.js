// ── Waypoints panel ───────────────────────────────────────────────────────────
let _wpPollTimer   = null;
let _wpMode        = 'selector';   // 'point' | 'path' | 'area' | 'selector'
let _wpPathNodes   = [];        // world coords of nodes placed in current path session
let _wpAreaNodes   = [];        // temp waypoints placed while drawing an area polygon
let _wpAutoCounter = 1;
let _wpLastWaypoints = [];      // last fetched waypoints, used for path label lookup
const _wpExpandedPaths = new Set(); // path names that are currently expanded in the tree

// ── Player Starts ─────────────────────────────────────────────────────────────
let _wpPsActive = false;  // placing a player start right now
let _wpPsNext   = 0;      // player number being placed (1-8), 0 = idle

function _wpPsName(n) { return `Player_${n}_Start`; }

function _wpPsPlaced() {
  const names = new Set(_wpLastWaypoints.map(w => w.name));
  return Array.from({length: 8}, (_, i) => names.has(_wpPsName(i + 1)));
}

// Player-starts UI is mounted in multiple panels: waypoints tab + Map Setup
// (skirmish and co-op sections). Each mount has <prefix>-grid/-hint/-cancel.
const _PS_MOUNTS = ['wp-ps', 'ms-ps', 'ms-coop-ps'];

function renderPlayerStartsGrid() {
  const placed = _wpPsPlaced();
  const html = placed.map((ok, i) => {
    const n    = i + 1;
    const active = _wpPsActive && _wpPsNext === n;
    const dot  = ok ? `<span style="color:#5cb85c">&#10003;</span>` : `<span style="color:#e05c5c">&#9679;</span>`;
    const btn  = ok
      ? `<button class="action-btn" onclick="startPlacePlayerStart(${n})" style="font-size:10px;padding:2px 5px;flex:none">Re-place</button>`
      : `<button class="action-btn${active ? ' primary' : ''}" onclick="startPlacePlayerStart(${n})" style="font-size:10px;padding:2px 5px;flex:none">Place</button>`;
    return `<div style="display:flex;align-items:center;gap:5px;padding:3px 4px;border-radius:4px;background:${active ? 'rgba(74,144,226,0.12)' : 'rgba(255,255,255,0.03)'}">
      ${dot}<span style="flex:1;font-size:11px">P${n}</span>${btn}
    </div>`;
  }).join('');
  for (const m of _PS_MOUNTS) {
    const grid = document.getElementById(`${m}-grid`);
    if (grid) grid.innerHTML = html;
  }
}

function startPlacePlayerStart(n) {
  _wpPsActive = true;
  _wpPsNext   = n;
  for (const m of _PS_MOUNTS) {
    const hint   = document.getElementById(`${m}-hint`);
    const cancel = document.getElementById(`${m}-cancel`);
    if (hint)   { hint.textContent = `↓ Click on map to place Player ${n} Start ↓`; hint.style.display = ''; }
    if (cancel) cancel.style.display = '';
  }
  renderPlayerStartsGrid();
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

function cancelPlayerStartPlace() {
  _wpPsActive = false;
  _wpPsNext   = 0;
  for (const m of _PS_MOUNTS) {
    const hint   = document.getElementById(`${m}-hint`);
    const cancel = document.getElementById(`${m}-cancel`);
    if (hint)   hint.style.display = 'none';
    if (cancel) cancel.style.display = 'none';
  }
  renderPlayerStartsGrid();
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

async function handlePlayerStartPlace(screenX, screenY) {
  const n = _wpPsNext;
  _wpPsActive = false;
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);

  const wc = await screenToWorldAsync(screenX, screenY);
  if (!wc) { _wpPsActive = true; updatePassthrough(_lastOverViewport); return; }

  const name = _wpPsName(n);
  // Delete existing waypoint with this name first (re-place case)
  const exists = _wpLastWaypoints.find(w => w.name === name);
  if (exists) await api('/delete/waypoint', { name });

  const r = await api('/place/waypoint', { name, wx: wc.wx, wy: wc.wy });
  if (!r?.ok) {
    showFeedback('wp-place-feedback', r?.error || 'Failed', false);
    cancelPlayerStartPlace();
    return;
  }
  syncWaypointsState();
  if (typeof _msFetchData === 'function') _msFetchData(); // refresh Map Setup status row

  // Auto-advance to next unplaced player start (up to 8)
  const newPlaced = new Set(_wpLastWaypoints.map(w => w.name));
  newPlaced.add(name);
  const nextUnplaced = Array.from({length: 8}, (_, i) => i + 1).find(i => !newPlaced.has(_wpPsName(i)));
  if (nextUnplaced) {
    startPlacePlayerStart(nextUnplaced);
  } else {
    cancelPlayerStartPlace();
    showFeedback('wp-place-feedback', 'All 8 player starts placed', true);
  }
}

function startWpPolling() {
  stopWpPolling();
  syncWaypointsState();
  _wpPollTimer = setInterval(syncWaypointsState, 4000);
}
function stopWpPolling() {
  if (_wpPollTimer) { clearInterval(_wpPollTimer); _wpPollTimer = null; }
}

// Palette for path line colors (cycles through for multiple paths)
const _WP_PATH_COLORS = ['#e74c3c','#3498db','#2ecc71','#f39c12','#9b59b6','#1abc9c','#e67e22','#e91e63'];

async function syncWaypointsState() {
  const [r, rt] = await Promise.all([apiGet('/map/waypoints'), apiGet('/map/triggers')]);
  if (!r || !r.ok) return;

  const waypoints = r.waypoints || [];
  const areas     = (rt?.triggers || []).filter(t => t.name);
  _scpTriggerCache = areas; // keep trigger cache fresh

  const filterQ   = (document.getElementById('wp-search').value || '').toLowerCase();
  const list      = document.getElementById('wp-list');
  const key       = waypoints.map(w => w.name + (w.paths||[]).join('')).join(',')
                  + '|' + areas.map(a => a.name + (a.points||[]).length).join(',');
  if (list.dataset.key === key && !filterQ) {
    _wpLastWaypoints = waypoints;
    renderPlayerStartsGrid();
    _wpDrawCanvas(waypoints);
    return;
  }
  list.dataset.key = key;
  list.innerHTML   = '';
  _wpLastWaypoints = waypoints;
  renderPlayerStartsGrid();

  // Auto-fill path name only when empty (first load)
  const nameEl = document.getElementById('wp-path-name');
  if (nameEl && !nameEl.value) {
    const usedNums = new Set();
    for (const wp of waypoints) {
      for (const p of _wpResolvePaths(wp)) {
        const m = p.match(/^Path_(\d+)$/);
        if (m) usedNums.add(Number(m[1]));
      }
    }
    let next = 1;
    while (usedNums.has(next)) next++;
    nameEl.value = `Path_${next}`;
  }

  // Resolve effective paths per waypoint:
  // - New WB exe: wp.paths is an array (may be empty)
  // - Old WB exe: wp.paths is undefined → fall back to name convention "Base_N"
  function _wpResolvePaths(wp) {
    if (wp.paths && wp.paths.length > 0) return wp.paths;
    if (wp.paths !== undefined) return []; // new API, genuinely standalone
    // Fallback: detect "BaseName_N" convention
    const m = (wp.name || '').match(/^(.+)_(\d+)$/);
    return m ? [m[1]] : [];
  }

  // Collect unique path names → ordered node lists
  const pathMap = {}; // pathName -> [{...wp}, ...]
  const standalone = [];
  for (const wp of waypoints) {
    const paths = _wpResolvePaths(wp);
    if (paths.length > 0) {
      for (const p of paths) (pathMap[p] = pathMap[p] || []).push(wp);
    } else {
      standalone.push(wp);
    }
  }

  // Update _scpWaypointCache path names for dropdowns
  if (_scpWaypointCache !== null) {
    _scpWaypointCache = waypoints; // keep fresh
  }

  const pathEntries = Object.entries(pathMap);
  // Area vertex waypoints share the path group name with their area — suppress them from path rendering
  const areaNameSet = new Set(areas.map(a => a.name));
  const visiblePathEntries = pathEntries.filter(([pname]) => !areaNameSet.has(pname));
  document.getElementById('wp-count').textContent = standalone.length + visiblePathEntries.length + areas.length;

  // Render paths (excluding those that belong to trigger areas)
  const colorMap = {};
  visiblePathEntries.forEach(([pname, nodes], idx) => {
    colorMap[pname] = _WP_PATH_COLORS[idx % _WP_PATH_COLORS.length];
    if (filterQ && !pname.toLowerCase().includes(filterQ) &&
        !nodes.some(n => (n.name||'').toLowerCase().includes(filterQ))) return;

    const isOpen = _wpExpandedPaths.has(pname);

    // Group wrapper
    const group = document.createElement('div');
    group.className = 'wp-tree-group';

    // Header row
    const header = document.createElement('div');
    header.className = 'wp-tree-header' + (isOpen ? ' open' : '');

    const toggle = document.createElement('span');
    toggle.className = 'wp-tree-toggle';
    toggle.textContent = '+';

    const dot = document.createElement('span');
    dot.style.cssText = `display:inline-block;width:8px;height:8px;border-radius:50%;background:${colorMap[pname]};flex-shrink:0`;

    const label = document.createElement('span');
    label.style.flex = '1';
    label.textContent = `${pname}  (${nodes.length} nodes)`;

    header.appendChild(toggle);
    header.appendChild(dot);
    header.appendChild(label);

    // Children container
    const children = document.createElement('div');
    children.className = 'wp-tree-children' + (isOpen ? ' open' : '');

    nodes.forEach((wp, ni) => {
      const node = document.createElement('div');
      node.className = 'wp-tree-node';
      node.textContent = `${ni + 1}. ${wp.name || ''}`;
      if (wp.wx != null) node.title = `World (${Math.round(wp.wx)}, ${Math.round(wp.wy)})`;
      node.addEventListener('click', e => {
        e.stopPropagation();
        if (wp.name) api('/object/select', { name: wp.name });
      });
      children.appendChild(node);
    });

    // Toggle open/close on header click
    header.addEventListener('click', () => {
      const open = header.classList.toggle('open');
      children.classList.toggle('open', open);
      if (open) _wpExpandedPaths.add(pname);
      else       _wpExpandedPaths.delete(pname);
    });

    group.appendChild(header);
    group.appendChild(children);
    list.appendChild(group);
  });

  // Render standalone waypoints
  for (const wp of standalone) {
    if (filterQ && !(wp.name || '').toLowerCase().includes(filterQ)) continue;
    const el = document.createElement('div');
    el.className = 'wp-standalone';
    el.textContent = `· ${wp.name || wp}`;
    if (wp.wx != null) el.title = `World (${Math.round(wp.wx)}, ${Math.round(wp.wy)})`;
    el.addEventListener('click', () => {
      if (wp.name) api('/object/select', { name: wp.name });
    });
    list.appendChild(el);
  }

  // Render trigger areas (polygon triggers) — collapsible like paths
  for (const area of areas) {
    if (filterQ && !area.name.toLowerCase().includes(filterQ)) continue;
    const areaKey = '__area__' + area.name;
    const isOpen  = _wpExpandedPaths.has(areaKey);

    const group  = document.createElement('div');
    group.className = 'wp-tree-group';

    const header = document.createElement('div');
    header.className = 'wp-tree-header' + (isOpen ? ' open' : '');

    const toggle = document.createElement('span');
    toggle.className = 'wp-tree-toggle';
    toggle.textContent = '+';

    const icon = document.createElement('span');
    icon.textContent = '⬡';
    icon.style.cssText = 'font-size:12px;flex-shrink:0;margin:0 3px;opacity:0.8';

    const label = document.createElement('span');
    label.style.flex = '1';
    label.textContent = `${area.name}  (${(area.points||[]).length} pts)`;

    const delBtn = document.createElement('button');
    delBtn.textContent = '✕';
    delBtn.className = 'ms-btn danger';
    delBtn.style.cssText = 'padding:2px 5px;font-size:10px;flex-shrink:0';
    delBtn.addEventListener('click', async e => {
      e.stopPropagation();
      // Delete the trigger area AND its vertex waypoints (they share the same pathLabel)
      await api('/trigger/delete', { name: area.name });
      for (const vwp of (pathMap[area.name] || [])) {
        await api('/delete/waypoint', { name: vwp.name });
      }
      syncWaypointsState();
    });

    header.append(toggle, icon, label, delBtn);

    const children = document.createElement('div');
    children.className = 'wp-tree-children' + (isOpen ? ' open' : '');

    // Prefer real waypoints (selectable in WB) as children; fall back to coordinate strings
    const vertexWaypoints = pathMap[area.name] || [];
    if (vertexWaypoints.length > 0) {
      vertexWaypoints.forEach((wp, ni) => {
        const node = document.createElement('div');
        node.className = 'wp-tree-node';
        node.textContent = `${ni + 1}. ${wp.name}`;
        if (wp.wx != null) node.title = `World (${Math.round(wp.wx)}, ${Math.round(wp.wy)})`;
        node.addEventListener('click', e => {
          e.stopPropagation();
          if (wp.name) api('/object/select', { name: wp.name });
        });
        children.appendChild(node);
      });
    } else {
      (area.points || []).forEach((pt, i) => {
        const node = document.createElement('div');
        node.className = 'wp-tree-node';
        node.textContent = `${i + 1}. (${Math.round(pt.x)}, ${Math.round(pt.y)})`;
        children.appendChild(node);
      });
    }

    header.addEventListener('click', () => {
      const open = header.classList.toggle('open');
      children.classList.toggle('open', open);
      if (open) _wpExpandedPaths.add(areaKey);
      else       _wpExpandedPaths.delete(areaKey);
    });

    group.append(header, children);
    list.appendChild(group);
  }

  _wpDrawCanvas(waypoints);
}

function _wpDrawCanvas(waypoints) {
  const canvas = document.getElementById('wp-canvas');
  if (!canvas) return;
  // Size canvas to panel width, 4:3-ish ratio
  const W = canvas.offsetWidth || 220;
  const H = Math.round(W * 0.75);
  canvas.width  = W;
  canvas.height = H;
  const ctx = canvas.getContext('2d');
  ctx.clearRect(0, 0, W, H);

  if (!waypoints || waypoints.length === 0) {
    ctx.fillStyle = '#444';
    ctx.font = '11px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText('No waypoints', W/2, H/2);
    return;
  }

  // Compute bounds
  let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
  for (const wp of waypoints) {
    if (wp.wx < minX) minX = wp.wx; if (wp.wx > maxX) maxX = wp.wx;
    if (wp.wy < minY) minY = wp.wy; if (wp.wy > maxY) maxY = wp.wy;
  }
  const pad = 18;
  const rangeX = maxX - minX || 1, rangeY = maxY - minY || 1;
  const scaleX = (W - pad*2) / rangeX, scaleY = (H - pad*2) / rangeY;
  const scale  = Math.min(scaleX, scaleY);
  const offX   = pad + ((W - pad*2) - rangeX * scale) / 2;
  const offY   = pad + ((H - pad*2) - rangeY * scale) / 2;
  const toC    = wp => ({ x: offX + (wp.wx - minX) * scale, y: offY + (wp.wy - minY) * scale });

  // Build path groups — same fallback logic as syncWaypointsState
  const pathMap = {};
  for (const wp of waypoints) {
    let paths = wp.paths && wp.paths.length > 0 ? wp.paths
              : wp.paths !== undefined ? []
              : (() => { const m = (wp.name||'').match(/^(.+)_(\d+)$/); return m ? [m[1]] : []; })();
    for (const p of paths) (pathMap[p] = pathMap[p] || []).push(wp);
  }

  // Draw path lines
  const pathNames = Object.keys(pathMap);
  pathNames.forEach((pname, idx) => {
    const color = _WP_PATH_COLORS[idx % _WP_PATH_COLORS.length];
    const nodes = pathMap[pname];
    ctx.beginPath();
    ctx.strokeStyle = color;
    ctx.lineWidth   = 1.5;
    nodes.forEach((wp, i) => {
      const c = toC(wp);
      if (i === 0) ctx.moveTo(c.x, c.y); else ctx.lineTo(c.x, c.y);
    });
    ctx.stroke();
    // Draw arrowhead at last segment
    if (nodes.length >= 2) {
      const a = toC(nodes[nodes.length-2]), b = toC(nodes[nodes.length-1]);
      const angle = Math.atan2(b.y - a.y, b.x - a.x);
      ctx.beginPath();
      ctx.fillStyle = color;
      ctx.moveTo(b.x, b.y);
      ctx.lineTo(b.x - 7*Math.cos(angle-0.4), b.y - 7*Math.sin(angle-0.4));
      ctx.lineTo(b.x - 7*Math.cos(angle+0.4), b.y - 7*Math.sin(angle+0.4));
      ctx.closePath();
      ctx.fill();
    }
    // Path label near midpoint
    const mid = nodes[Math.floor(nodes.length/2)];
    const mc = toC(mid);
    ctx.font = '9px sans-serif';
    ctx.fillStyle = color;
    ctx.textAlign = 'left';
    ctx.fillText(pname, mc.x + 4, mc.y - 3);
  });

  // Draw waypoint dots (paths on top of standalone)
  for (const wp of waypoints) {
    const c = toC(wp);
    const inPath = (wp.paths || []).length > 0;
    ctx.beginPath();
    ctx.arc(c.x, c.y, inPath ? 3.5 : 2.5, 0, Math.PI*2);
    ctx.fillStyle = inPath ? '#fff' : '#888';
    ctx.fill();
  }
}

// Called by the map mousedown handler (waypoints tool, left-click on viewport)
async function handleWaypointMapClick(screenX, screenY) {
  if (_wpMode === 'selector') return;
  const wc = await screenToWorldAsync(screenX, screenY);
  if (!wc) return;

  if (_wpMode === 'point') {
    const nameEl = document.getElementById('wp-name');
    let name = nameEl.value.trim();
    if (!name) name = `Waypoint_${_wpAutoCounter}`;
    const r = await api('/place/waypoint', { name, wx: wc.wx, wy: wc.wy });
    showFeedback('wp-place-feedback', r?.ok ? `Placed: ${name}` : (r?.error || 'Failed'), !!r?.ok);
    if (r?.ok) {
      // Auto-increment: bump the trailing number in the name
      _wpAutoCounter++;
      const bumped = name.replace(/(\d+)$/, n => String(Number(n) + 1));
      if (bumped !== name) nameEl.value = bumped;
      syncWaypointsState();
    }
  } else if (_wpMode === 'area') {
    // Area mode: place a temp waypoint in WB grouped under the area name (like path mode)
    const areaName = document.getElementById('wp-area-name').value.trim() || 'TriggerArea_1';
    const idx  = _wpAreaNodes.length;
    const name = `${areaName}_${idx}`;
    const r = await api('/place/waypoint', { name, wx: wc.wx, wy: wc.wy, pathLabel: areaName });
    if (r?.ok) {
      if (idx > 0) {
        await api('/link/waypoints', { name1: _wpAreaNodes[idx - 1].name, name2: name });
      }
      _wpAreaNodes.push({ name, x: wc.wx, y: wc.wy });
      _updateAreaUI();
    }
  } else {
    // Path mode: accumulate nodes with real path label + create 3D link to previous node
    const pathName = (document.getElementById('wp-path-name').value.trim()) || 'Path_1';
    const idx  = _wpPathNodes.length;
    const name = `${pathName}_${idx}`;
    const r = await api('/place/waypoint', { name, wx: wc.wx, wy: wc.wy, pathLabel: pathName });
    if (r?.ok) {
      // Link to previous node so WB draws the 3D path line
      if (idx > 0) {
        await api('/link/waypoints', { name1: _wpPathNodes[idx-1].name, name2: name });
      }
      _wpPathNodes.push({ name, wx: wc.wx, wy: wc.wy });
      const statusEl = document.getElementById('wp-path-status');
      statusEl.textContent = `${_wpPathNodes.length} node(s) placed · right-click to finish`;
      showFeedback('wp-path-feedback', `Node ${idx}: ${name}`, true);
      syncWaypointsState();
    } else {
      showFeedback('wp-path-feedback', r?.error || 'Failed', false);
    }
  }
}

function finishWpPath() {
  if (_wpMode !== 'path' || _wpPathNodes.length === 0) return;
  const n = _wpPathNodes.length;
  _wpPathNodes = [];
  document.getElementById('wp-path-status').textContent = 'Click to add nodes · right-click to finish';
  showFeedback('wp-path-feedback', `Path finished (${n} nodes)`, true);
  // Bump the trailing number so next path gets a fresh name
  const nameEl = document.getElementById('wp-path-name');
  nameEl.value = nameEl.value.replace(/(\d+)$/, m => String(Number(m) + 1));
}

async function finishWpArea() {
  if (_wpMode !== 'area') return;
  if (_wpAreaNodes.length < 3) {
    // Not enough vertices — delete temp waypoints and stay in area mode so user can retry
    await _wpAreaDelete();
    _updateAreaUI();
    const statusEl = document.getElementById('wp-area-status');
    if (statusEl) { statusEl.textContent = 'Need ≥3 vertices — click on map to start again'; statusEl.style.color = '#ff5555'; }
    return;
  }
  // Switch mode BEFORE any awaits so passthrough updates on the very next mousemove
  _wpMode = 'selector';
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);

  const nameEl = document.getElementById('wp-area-name');
  const name = nameEl.value.trim() || 'TriggerArea_1';
  const pts = _wpAreaNodes.map(nd => ({ x: nd.x, y: nd.y }));
  // Create the trigger area — keep the vertex waypoints on the map so they're selectable
  const res = await api('/trigger', { name, points: pts });
  _wpAreaClearLocal(); // only clear tracking array, waypoints stay in WB
  _updateAreaUI();
  if (res?.ok) {
    nameEl.value = name.replace(/(\d+)$/, m => String(Number(m) + 1));
  }
  await syncAreasState();
  setWpMode('selector');
}

function setWpMode(mode) {
  _wpMode = mode;
  _wpPathNodes = [];
  _wpAreaDelete(); // delete any in-progress area vertices when switching modes
  _updateAreaUI();
  document.getElementById('wp-selector-section').style.display = mode === 'selector' ? '' : 'none';
  document.getElementById('wp-point-section').style.display    = mode === 'point'    ? '' : 'none';
  document.getElementById('wp-path-section').style.display     = mode === 'path'     ? '' : 'none';
  document.getElementById('wp-area-section').style.display     = mode === 'area'     ? '' : 'none';
  document.querySelectorAll('.wp-tool-btn[data-mode]').forEach(b =>
    b.classList.toggle('active', b.dataset.mode === mode));
  syncWaypointsState();
}

function _updateAreaUI() {
  const statusEl = document.getElementById('wp-area-status');
  const cancelEl = document.getElementById('wp-area-cancel');
  if (!statusEl) return;
  const n = _wpAreaNodes.length;
  if (n === 0) {
    statusEl.textContent = 'Click on map to place polygon vertices · right-click to close';
    statusEl.style.color = '#8be9fd';
    cancelEl.style.display = 'none';
  } else {
    statusEl.textContent = `${n} vertex${n === 1 ? '' : 'es'} placed · right-click to close (need ≥3)`;
    statusEl.style.color = n >= 3 ? '#50fa7b' : '#ffb86c';
    cancelEl.textContent = `Cancel (${n} vertices)`;
    cancelEl.style.display = '';
  }
}

// Delete area vertex waypoints from WB and clear the node list (used on cancel)
async function _wpAreaDelete() {
  const nodes = _wpAreaNodes.splice(0);
  for (const nd of nodes) {
    await api('/delete/waypoint', { name: nd.name });
  }
}

// Just clear the local tracking array — leave waypoints in WB (used after finalize)
function _wpAreaClearLocal() {
  _wpAreaNodes.splice(0);
}

async function initWaypointsPanel() {
  document.querySelectorAll('.wp-tool-btn[data-mode]').forEach(btn => {
    btn.addEventListener('click', () => setWpMode(btn.dataset.mode));
  });
  document.getElementById('wp-search').addEventListener('input', () => syncWaypointsState());

  // Area mode: cancel button deletes in-progress vertex waypoints and resets
  document.getElementById('wp-area-cancel').addEventListener('click', async () => {
    await _wpAreaDelete();
    _updateAreaUI();
  });
}

async function syncAreasState() {
  // Areas are now rendered inside the unified ON MAP list via syncWaypointsState()
  await syncWaypointsState();
}

// ── Shared utility ────────────────────────────────────────────────────────────
function showFeedback(elId, msg, isOk) {
  const el = document.getElementById(elId);
  if (!el) return;
  el.textContent = msg;
  el.className = 'feedback-text ' + (isOk ? 'ok' : 'fail');
  setTimeout(() => { el.textContent = ''; el.className = 'feedback-text'; }, 3500);
}

// ── Map info ─────────────────────────────────────────────────────────────────
let _mapInfo = null;

async function refreshMapInfo(expectedPath) {
  // `map/load` (and `new_map`) are fire-and-forget (PostMessage) on the WB side — the
  // pipe replies ok:true before the file is actually loaded. If a caller just triggered
  // a load, wait for /map/info to actually reflect the new file before trusting it,
  // otherwise the title bar/size briefly (or permanently, if never re-polled) show the
  // PREVIOUS map's data.
  let r = await apiGet('/map/info');
  if (expectedPath) {
    for (let i = 0; i < 8 && !(r && r.ok && r.filePath === expectedPath); i++) {
      await new Promise(res => setTimeout(res, 300));
      r = await apiGet('/map/info');
    }
  }
  if (r && r.ok) {
    _mapInfo = r;
    const pw = r.width - 2 * (r.borderSize || 0);
    const ph = r.height - 2 * (r.borderSize || 0);
    const sizeEl = document.getElementById('status-mapsize');
    if (sizeEl) sizeEl.textContent = `${pw} × ${ph} tiles`;
    // Update title bar
    const nameEl = document.getElementById('map-name');
    if (nameEl) {
      const fp = r.filePath || '';
      const base = fp ? fp.replace(/.*[/\\]/, '') : 'Untitled';
      nameEl.textContent = base;
    }
    const dotEl = document.getElementById('unsaved-dot');
    if (dotEl) dotEl.style.display = r.isDirty ? '' : 'none';
  }
}

// Ask WB to convert a screen pixel directly to world coords via viewToDocCoords.
// Returns {wx, wy} or null on failure.
async function screenToWorldAsync(screenX, screenY) {
  const r = await api('/view/screen_to_world', { sx: Math.round(screenX), sy: Math.round(screenY) });
  if (!r || !r.ok) {
    if (r && r.error) console.log('[coords] screen_to_world failed:', r.error);
    return null;
  }
  if (!isFinite(r.wx) || !isFinite(r.wy)) {
    console.log('[coords] screen_to_world returned non-finite:', r.wx, r.wy);
    return null;
  }
  return { wx: Math.round(r.wx), wy: Math.round(r.wy) };
}

