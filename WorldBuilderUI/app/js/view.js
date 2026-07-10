// ── View popover ──────────────────────────────────────────────────────────────
const VIEW_ITEMS = [
  { header: 'MAP CONTENT' },
  { name: 'show_terrain',         label: 'Terrain',          on: true,  tip: 'Show/hide terrain height rendering' },
  { name: 'show_objects',         label: 'Object icons',     on: true,  tip: 'Show/hide 2D icon overlays on objects (not the 3D models)' },
  { name: 'show_waypoints',       label: 'Waypoints',        on: true,  tip: 'Show/hide waypoint markers' },
  { name: 'show_triggers',        label: 'Triggers',         on: true,  tip: 'Show/hide polygon trigger zones' },
  { name: 'show_labels',          label: 'Labels',           on: true,  tip: 'Show object template names for objects without a 3D model. Waypoint names are controlled by the Waypoints toggle.' },
  { name: 'show_brush_feedback',  label: 'Brush feedback',   on: true,  tip: 'Show/hide brush cursor preview while painting' },
  { sep: true },
  { header: 'OVERLAYS' },
  { name: 'show_impassable',      label: 'Impassable areas', on: false, tip: 'Overlay impassable terrain in red' },
  { name: 'show_map_boundaries',  label: 'Map boundaries',   on: true,  tip: 'Show/hide playable area boundary lines' },
  { name: 'show_contours',        label: 'Contours',         on: false, tip: 'Show terrain height as contour lines' },
  { name: 'show_shadows',         label: 'Shadows',          on: true,  tip: 'Show/hide object drop shadows (default: on)' },
  { name: 'show_wireframe',       label: 'Wireframe',        on: false, tip: 'Render terrain as wireframe mesh' },
  { sep: true },
  { header: 'OBJECTS' },
  { name: 'show_models',          label: 'Models (3D)',      on: true,  tip: 'Show/hide actual 3D building/unit meshes' },
  { name: 'show_bounding_boxes',  label: 'Bounding boxes',   on: false, tip: 'Show collision bounding boxes around objects' },
  { name: 'show_sight_ranges',    label: 'Sight ranges',     on: true,  tip: 'Show unit sight range circles' },
  { name: 'show_weapon_ranges',   label: 'Weapon ranges',    on: true,  tip: 'Show unit weapon range circles' },
  { name: 'show_garrisoned',      label: 'Garrisoned',       on: true,  tip: 'Show indicator for garrisonable buildings' },
  { sep: true },
  { header: 'AUDIO / UI' },
  { name: 'show_sound_flags',     label: 'Sound flags',      on: true,  tip: 'Show ambient sound source markers' },
  { name: 'show_sound_circles',   label: 'Sound circles',    on: true,  tip: 'Show ambient sound radius circles' },
  { name: 'show_letterbox',       label: 'Letterbox',        on: false, tip: 'Show cinematic letterbox bars' },
  { name: 'highlight_test_art',   label: 'Highlight test art', on: false, tip: 'Highlight placeholder/test art assets' },
  { sep: true },
  { header: 'ATMOSPHERE' },
  { name: 'show_extra_blends',    label: '3-Way blends in white', on: false, tip: 'Debug: render 3-way texture blend areas in white' },
  { name: 'show_soft_water',      label: 'Soft water',       on: true,  tip: 'Show soft water edge blending' },
  { name: 'show_clouds',          label: 'Clouds',           on: false, tip: 'Show cloud shadow map on terrain' },
  { name: 'show_macrotexture',    label: 'Macrotexture',     on: true,  tip: 'Show large-scale macro texture overlay' },
  { sep: true },
  { header: 'RENDER' },
  { name: 'show_all_3d',          label: 'Show all 3D',      on: true,  tip: 'Render full map without tile limit (default: on)' },
  { name: 'show_top_down',        label: 'Top-down',         on: false, tip: 'Switch to orthographic top-down projection' },
  { sep: true },
  { header: 'PARTIAL RENDER' },
  { name: 'partial_96',           label: '96 × 96 tiles',    on: false, tip: 'Limit render to center 96×96 tiles (performance)' },
  { name: 'partial_128',          label: '128 × 128 tiles',  on: false, tip: 'Limit render to center 128×128 tiles (performance)' },
  { name: 'partial_160',          label: '160 × 160 tiles',  on: false, tip: 'Limit render to center 160×160 tiles (performance)' },
  { name: 'partial_192',          label: '192 × 192 tiles',  on: false, tip: 'Limit render to center 192×192 tiles (performance)' },
  { sep: true },
  { name: 'snap_to_grid',         label: 'Snap to grid',     on: false, tip: 'Snap object placement to tile grid cells' },
  { sep: true },
  { name: 'reload_textures',      label: 'Reload textures',  action: true, tip: 'Reload all terrain textures from disk' },
];

// Programmatic access to view toggles. Most toggles only have a blind flip
// command in WB, so the UI-side `on` flag is the assumed state; show_impassable
// has an absolute pipe command and is resynced with the engine at boot.
function _applyViewItemUI(name, on) {
  const it = VIEW_ITEMS.find(i => i.name === name);
  if (!it) return;
  it.on = !!on;
  const el = document.querySelector(`#view-popover .vp-item[data-name="${name}"]`);
  if (el) {
    el.classList.toggle('on', it.on);
    el.querySelector('.vp-check').textContent = it.on ? '✓' : '';
  }
}

function getViewToggle(name) {
  const it = VIEW_ITEMS.find(i => i.name === name);
  return it ? !!it.on : false;
}

function setViewToggle(name, on) {
  const it = VIEW_ITEMS.find(i => i.name === name);
  if (!it || !!it.on === !!on) return;
  _applyViewItemUI(name, on);
  if (name === 'show_impassable') api('/view/impassable', { state: on ? 1 : 0 });
  else                            api('/view/toggle', { name });
}

function initViewPopover() {
  const popover = document.getElementById('view-popover');
  const btn     = document.getElementById('btn-view-popover');

  // Build popover content
  for (const item of VIEW_ITEMS) {
    if (item.sep) {
      const sep = document.createElement('div');
      sep.className = 'vp-sep';
      popover.appendChild(sep);
      continue;
    }
    if (item.header) {
      const hdr = document.createElement('div');
      hdr.className = 'vp-header';
      hdr.textContent = item.header;
      popover.appendChild(hdr);
      continue;
    }
    if (item.action) {
      const el = document.createElement('div');
      el.className = 'vp-action';
      el.innerHTML = `<span class="vp-check">↺</span><span class="vp-label">${item.label}</span>${item.tip ? `<span class="vp-tip" title="${item.tip}">?</span>` : ''}`;
      el.addEventListener('click', () => {
        api('/view/toggle', { name: item.name });
        closeViewPopover();
      });
      popover.appendChild(el);
      continue;
    }
    const el = document.createElement('div');
    el.className = 'vp-item' + (item.on ? ' on' : '');
    el.dataset.name = item.name;
    el.innerHTML = `<span class="vp-check">${item.on ? '✓' : ''}</span><span class="vp-label">${item.label}</span>${item.tip ? `<span class="vp-tip" title="${item.tip}">?</span>` : ''}`;
    el.addEventListener('click', (e) => {
      if (e.target.classList.contains('vp-tip')) return;
      item.on = !item.on;
      el.classList.toggle('on', item.on);
      el.querySelector('.vp-check').textContent = item.on ? '✓' : '';
      if (item.name === 'show_impassable') api('/view/impassable', { state: item.on ? 1 : 0 });
      else                                 api('/view/toggle', { name: item.name });
    });
    popover.appendChild(el);
  }

  // Resync impassable-overlay state with the engine. After a renderer refresh
  // WB keeps running with its own state while VIEW_ITEMS resets to defaults —
  // without this the checkbox ends up inverted vs. what's on screen.
  let _impSyncTries = 0;
  const _impSync = async () => {
    const r = await apiGet('/view/impassable');
    if (r?.ok && typeof r.state === 'number') {
      _applyViewItemUI('show_impassable', r.state === 1);
      return;
    }
    if (++_impSyncTries < 24) setTimeout(_impSync, 5000); // pipe needs ~16s after cold start
  };
  _impSync();

  btn.addEventListener('click', (e) => {
    e.stopPropagation();
    const willOpen = !popover.classList.contains('open');
    if (willOpen) {
      const rect = btn.getBoundingClientRect();
      popover.style.bottom = (window.innerHeight - rect.top + 8) + 'px';
      popover.style.right  = (window.innerWidth - rect.right) + 'px';
      popover.style.left   = 'auto';
    }
    popover.classList.toggle('open', willOpen);
    btn.classList.toggle('open', willOpen);
  });

  document.addEventListener('click', (e) => {
    if (!popover.contains(e.target) && e.target !== btn) closeViewPopover();
  });
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') closeViewPopover();
  });
}

function closeViewPopover() {
  document.getElementById('view-popover').classList.remove('open');
  document.getElementById('btn-view-popover').classList.remove('open');
}

// ── Global keyboard shortcuts ─────────────────────────────────────────────────
document.addEventListener('keydown', e => {
  const tag = document.activeElement ? document.activeElement.tagName : '';
  const editable = tag === 'INPUT' || tag === 'TEXTAREA' || document.activeElement.isContentEditable;
  if (editable) return;
  if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'z') {
    e.preventDefault();
    api('/map/undo', {});
  } else if (e.ctrlKey && e.shiftKey && e.key.toLowerCase() === 'z') {
    e.preventDefault();
    api('/map/redo', {});
  } else if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'y') {
    e.preventDefault();
    api('/map/redo', {});
  } else if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'c') {
    // Ctrl+C — shapefill: copy shape; elsewhere: WB-native copy of selected objects
    e.preventDefault();
    if (_currentTool === 'shapefill') sfAction('copy');
    else api('/edit', { op: 'copy' });
  } else if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'v') {
    // Ctrl+V — shapefill: paste shape (offset); elsewhere: WB-native paste
    e.preventDefault();
    if (_currentTool === 'shapefill') sfAction('paste');
    else api('/edit', { op: 'paste' });
  } else if (e.key === 'Delete') {
    e.preventDefault();
    if (_currentTool === 'shapefill') {
      sfAction('delete');   // consistent: Delete pakt de geselecteerde shape ook vanuit de UI
    } else if (_currentTool === 'waypoints') {
      api('/object/delete_selected', {}).then(r => {
        if (r?.ok) { hideObjProps(); syncWaypointsState(); }
      });
    } else if (_currentTool === 'roads' && _rdSelected.kind !== null) {
      const { kind, x1, y1 } = _rdSelected;
      if (kind === 'road') rdDeleteRoad(x1, y1);
      else rdDeleteBridge(x1, y1);
    }
  }
});

