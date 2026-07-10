// ── Load Map modal ──────────────────────────────────────────────────────────
let _allMaps = [];
let _selectedMapPath = null;
let _activePreset = 'all';   // 'all' or a scanRoot dir path
let _scanDirs = [];

function openLoadModal() {
  _selectedMapPath = null;
  document.getElementById('btn-modal-load-open').disabled = true;
  document.getElementById('modal-load-search').value = '';
  document.getElementById('modal-load-playerfilter').value = '';
  document.getElementById('modal-load-status').textContent = '';
  _activePreset = 'all';
  document.getElementById('modal-load').classList.remove('hidden');
  ipcRenderer.send('viewport-mouse', false);
  fetchScanDirs();
  if (!_allMaps.length) fetchMapList(); else applyLoadFilters();
}

function closeLoadModal() {
  document.getElementById('modal-load').classList.add('hidden');
}

function setLoadPreset(preset) {
  _activePreset = preset;
  document.querySelectorAll('#modal-load-presets .filedlg-preset').forEach(b =>
    b.classList.toggle('active', b.dataset.preset === preset));
  applyLoadFilters();
}

async function fetchScanDirs() {
  try {
    const r = await fetch('http://127.0.0.1:8099/api/map/scandirs');
    const data = await r.json();
    _scanDirs = data.dirs || [];
  } catch { _scanDirs = []; }
  renderFolderChips();
}

// Folder chips live between the static "All" button and the static "+ New folder"
// button — only the chips themselves are rebuilt on each refresh.
function renderFolderChips() {
  const container = document.getElementById('modal-load-presets');
  container.querySelectorAll('.filedlg-preset-chip').forEach(el => el.remove());
  const addBtn = document.getElementById('btn-add-scandir');
  for (const dir of _scanDirs) {
    const short = dir.split('\\').pop() || dir;
    const chip = document.createElement('button');
    chip.className = 'filedlg-preset filedlg-preset-chip' + (_activePreset === dir ? ' active' : '');
    chip.dataset.preset = dir;
    chip.title = dir;
    chip.textContent = short;
    const x = document.createElement('span');
    x.className = 'filedlg-preset-remove';
    x.textContent = '✕';
    x.title = 'Remove this folder from the scan list';
    x.addEventListener('click', async (e) => {
      e.stopPropagation();
      await fetch('http://127.0.0.1:8099/api/map/scandirs/remove', {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ dir }),
      });
      if (_activePreset === dir) _activePreset = 'all';
      await fetchScanDirs();
      fetchMapList();
    });
    chip.appendChild(x);
    // Click-to-select is handled by the delegated .filedlg-preset listener below;
    // only the ✕ needs its own handler (with stopPropagation so it doesn't also select).
    container.insertBefore(chip, addBtn);
  }
}

document.getElementById('btn-add-scandir').addEventListener('click', async () => {
  const dir = await ipcRenderer.invoke('pick-folder');
  if (!dir) return;
  const r = await fetch('http://127.0.0.1:8099/api/map/scandirs/add', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ dir }),
  });
  const data = await r.json();
  if (!data.ok) { document.getElementById('modal-load-status').textContent = data.error || 'Failed to add folder'; return; }
  await fetchScanDirs();
  fetchMapList();
});

function applyLoadFilters() {
  const q = (document.getElementById('modal-load-search').value || '').toLowerCase().trim();
  const pf = document.getElementById('modal-load-playerfilter').value;
  const filtered = _allMaps.filter(m =>
    (_activePreset === 'all' || m.scanRoot === _activePreset) &&
    (!q || m.name.toLowerCase().includes(q) || m.dir.toLowerCase().includes(q)) &&
    (!pf || m.numPlayers === Number(pf))
  );
  renderMapList(filtered);
  document.getElementById('modal-load-status').textContent =
    filtered.length === _allMaps.length ? `${_allMaps.length} maps` : `${filtered.length} of ${_allMaps.length}`;
}

async function fetchMapList() {
  const listEl = document.getElementById('modal-load-list');
  listEl.innerHTML = '<div style="padding:20px;text-align:center;color:var(--text-dim);font-size:12px">Scanning…</div>';
  try {
    const r = await fetch('http://127.0.0.1:8099/api/map/list');
    const data = await r.json();
    _allMaps = data.maps || [];
  } catch { _allMaps = []; }
  applyLoadFilters();
}

function renderMapList(maps) {
  const listEl = document.getElementById('modal-load-list');
  if (!maps.length) {
    listEl.innerHTML = '<div style="padding:20px;text-align:center;color:var(--text-dim);font-size:12px">No maps found</div>';
    return;
  }
  listEl.innerHTML = maps.map(m => {
    const date = new Date(m.mtime);
    const dateStr = date.toLocaleDateString('en-GB', { day:'2-digit', month:'short', year:'numeric' });
    const sizeStr = m.size > 1024*1024 ? (m.size/1024/1024).toFixed(1)+' MB' : Math.round(m.size/1024)+' KB';
    const safePath = (m.path || '').replace(/&/g,'&amp;').replace(/"/g,'&quot;');
    // numPlayers comes from a precomputed cache (scan_playercounts.py) — undefined for
    // not-yet-scanned maps, so just omit the badge rather than show a misleading "0".
    const playerBadge = m.numPlayers != null
      ? `<span class="map-file-players"${m.spawnGap ? ' title="Gap in Player_N_Start sequence — extra starts beyond the gap are ignored by the game"' : ''}>${m.numPlayers}p${m.spawnGap ? ' ⚠' : ''}</span>`
      : '';
    return `<div class="map-file-item" data-path="${safePath}">
      <div class="map-file-icon">▦</div>
      <div class="map-file-info">
        <div class="map-file-name">${m.name} ${playerBadge}</div>
        <div class="map-file-dir">${m.dir}</div>
      </div>
      <div class="map-file-meta">${dateStr}<br>${sizeStr}</div>
    </div>`;
  }).join('');

  listEl.querySelectorAll('.map-file-item').forEach(el => {
    el.addEventListener('click', () => {
      listEl.querySelectorAll('.map-file-item').forEach(x => x.classList.remove('selected'));
      el.classList.add('selected');
      _selectedMapPath = el.dataset.path;
      document.getElementById('btn-modal-load-open').disabled = false;
    });
    el.addEventListener('dblclick', () => {
      _selectedMapPath = el.dataset.path;
      loadSelectedMap();
    });
  });
}

// Auto-add all skirmish players after map load or new map — idempotent, safe to call always
async function autoAddSkirmishPlayers() {
  await new Promise(r => setTimeout(r, 800));
  await api('/sidelist/add_skirmish', {});
  fetchSideList();
}

async function loadSelectedMap() {
  if (!_selectedMapPath) return;
  closeLoadModal();
  await api('/map/load', { path: _selectedMapPath });
  // Title bar and status-bar map size only update via refreshMapInfo() — it's called once
  // at WB startup but was never re-called here, so opening an existing map left both
  // showing stale/placeholder data ("Untitled", the previous map's size). map/load is
  // fire-and-forget, so wait for /map/info to actually reflect this path (see refreshMapInfo).
  await refreshMapInfo(_selectedMapPath);
  autoAddSkirmishPlayers();
  // When loading from the start screen: close it and land on Map Setup
  if (typeof startScreenOnMapLoaded === 'function') startScreenOnMapLoaded();
}

document.getElementById('btn-modal-load-cancel').addEventListener('click', closeLoadModal);
document.getElementById('btn-modal-load-close').addEventListener('click', closeLoadModal);
document.getElementById('btn-modal-load-open').addEventListener('click', loadSelectedMap);
document.getElementById('modal-load-search').addEventListener('input', applyLoadFilters);
document.getElementById('modal-load-playerfilter').addEventListener('change', applyLoadFilters);
document.getElementById('modal-load-search').addEventListener('keydown', e => {
  if (e.key === 'Enter' && _selectedMapPath) loadSelectedMap();
  if (e.key === 'Escape') closeLoadModal();
});
document.getElementById('modal-load').addEventListener('click', e => { if (e.target === e.currentTarget) closeLoadModal(); });
document.getElementById('modal-load-presets').addEventListener('click', e => {
  const btn = e.target.closest('.filedlg-preset');
  if (btn) setLoadPreset(btn.dataset.preset);
});

// ── Save As modal ───────────────────────────────────────────────────────────
let _saveDirs = [];
async function openSaveAsModal() {
  await refreshMapInfo();
  const fp = _mapInfo?.filePath || '';
  const current = fp ? fp.replace(/.*[/\\]/, '') : (document.getElementById('map-name')?.textContent.trim() || '');
  document.getElementById('saveas-current-path').textContent = current || '—';
  const base = current.replace(/\.map$/i, '');
  const inp = document.getElementById('saveas-name-input');
  inp.value = base;
  document.getElementById('modal-saveas').classList.remove('hidden');
  ipcRenderer.send('viewport-mouse', false);

  // Load save dirs
  const dirSel = document.getElementById('saveas-dir-select');
  if (!_saveDirs.length) {
    try {
      const dr = await fetch('http://127.0.0.1:8099/api/map/save_dirs');
      const dd = await dr.json();
      _saveDirs = dd.dirs || [];
    } catch { _saveDirs = []; }
  }
  dirSel.innerHTML = _saveDirs.length
    ? _saveDirs.map((d, i) => {
        const label = d.replace(/^.*\\Command and Conquer Generals Zero Hour Data\\Maps/, 'C&C ZH Maps')
                       .replace(/^.*\\Maps$/, 'C&C ZH Maps');
        return `<option value="${d.replace(/&/g,'&amp;').replace(/"/g,'&quot;')}">${label}</option>`;
      }).join('')
    : '<option value="">No writable maps folder found</option>';

  // Load existing maps for the list
  const listEl = document.getElementById('saveas-file-list');
  listEl.innerHTML = '<div style="padding:8px;color:var(--text-dim);font-size:11px">Loading…</div>';
  try {
    if (!_allMaps.length) {
      const r = await fetch('http://127.0.0.1:8099/api/map/list');
      const d = await r.json();
      _allMaps = d.maps || [];
    }
    const proj = _allMaps.filter(m => !/(Maps dominator|EA_maps_original|Program Files)/i.test(m.dir));
    if (!proj.length) { listEl.innerHTML = '<div style="padding:8px;color:var(--text-dim);font-size:11px">No existing maps found</div>'; }
    else {
      listEl.innerHTML = proj.map(m => {
        const safePath = (m.path||'').replace(/&/g,'&amp;').replace(/"/g,'&quot;');
        return `<div class="map-file-item" data-name="${m.name}" data-path="${safePath}" style="padding:6px 10px">
          <div class="map-file-info">
            <div class="map-file-name" style="font-size:12px">${m.name}</div>
          </div>
        </div>`;
      }).join('');
      listEl.querySelectorAll('.map-file-item').forEach(el => {
        el.addEventListener('click', () => {
          listEl.querySelectorAll('.map-file-item').forEach(x => x.classList.remove('selected'));
          el.classList.add('selected');
          document.getElementById('saveas-name-input').value = el.dataset.name;
        });
      });
    }
  } catch { listEl.innerHTML = ''; }
  inp.select(); inp.focus();
}

function closeSaveAsModal() {
  document.getElementById('modal-saveas').classList.add('hidden');
}

document.getElementById('btn-modal-saveas-cancel').addEventListener('click', closeSaveAsModal);
document.getElementById('btn-modal-saveas-close').addEventListener('click', closeSaveAsModal);
document.getElementById('modal-saveas').addEventListener('click', e => { if (e.target === e.currentTarget) closeSaveAsModal(); });
document.getElementById('saveas-name-input').addEventListener('keydown', e => {
  if (e.key === 'Enter') document.getElementById('btn-modal-saveas-go').click();
  if (e.key === 'Escape') closeSaveAsModal();
});
document.getElementById('btn-modal-saveas-go').addEventListener('click', async () => {
  const name = document.getElementById('saveas-name-input').value.trim().replace(/\.map$/i, '');
  const dir  = document.getElementById('saveas-dir-select').value;
  if (!name) { document.getElementById('saveas-status').textContent = 'Enter a map name'; return; }
  if (!dir)  { document.getElementById('saveas-status').textContent = 'No save folder available'; return; }
  const fullPath = `${dir}\\${name}\\${name}.map`;
  const statusEl = document.getElementById('saveas-status');
  statusEl.textContent = 'Saving…';
  const r = await api('/map/save_to', { path: fullPath });
  if (r?.ok) {
    statusEl.textContent = 'Saved!';
    await refreshMapInfo();
    setTimeout(closeSaveAsModal, 800);
  } else {
    statusEl.textContent = `Error: ${r?.error || 'save failed'}`;
  }
});

