// ── Mouse passthrough to embedded WB ─────────────────────────────────────────
// Elements NOT in this list let clicks fall through to WB behind the window.
// New fullscreen screen or modal? Add its id here — or give it class="ui-capture".
const UI_SELECTORS = '#title-bar, #side-panel, #toolbar, #status-bar, #wb-overlay, #btn-launch-wb, #tex-picker, #obj-picker, #view-popover, #obj-props-panel, #modal-setup, #modal-scripts, #msc-script-modal, #msc-cond-modal, #scp-box, #modal-load, #modal-saveas, #modal-newmap, #modal-resize, #panel-roads, #wiz-team-backdrop, #start-screen, .ui-capture';

function isOverUI(x, y) {
  const el = document.elementFromPoint(x, y);
  if (!el || el === document.documentElement || el === document.body) return false;
  return el.closest(UI_SELECTORS) !== null;
}

let _lastOverViewport  = null;
let _coordThrottleTimer = null;
let _lastCoordX = -1, _lastCoordY = -1;
let _lastPassthrough = null;

// Drag-to-rotate state (objects tool)
let _dragOrigin = null;         // {x, y} where mousedown fired
let _dragAngle  = 0;            // current angle accumulator during drag
let _rotateTimer = null;        // throttle timer for rotate_selected calls
let _placementPromise = Promise.resolve();  // resolves when place_object completes

// Wheel debounce
let _wheelTimer = null;
let _wheelAccum = 0;

function updatePassthrough(overViewport) {
  // Capture clicks in Electron when: waypoints tool, or objects tool WITH template selected.
  // Without template, pass clicks through to WB so the user can select/move existing objects.
  const clickCaptureTool = _wizWpQuickPlace ||
                           _wizPathDrawActive ||
                           _wizAreaDrawActive ||
                           _wpPsActive ||
                           !!_msQpActive ||
                           (_currentTool === 'waypoints' && (_wpMode === 'path' || _wpMode === 'area' || _wpMode === 'point')) ||
                           (_currentTool === 'objects' && !!_selectedTemplate) ||
                           (_currentTool === 'nature'  && !!_selectedNature && overViewport) ||
                           false; // roads: WB handles map interaction natively
  const passthrough = overViewport && !clickCaptureTool;
  if (overViewport !== _lastOverViewport || passthrough !== _lastPassthrough) {
    _lastOverViewport = overViewport;
    _lastPassthrough  = passthrough;
    ipcRenderer.send('viewport-mouse', passthrough);
  }
}

document.addEventListener('mousemove', e => {
  const overViewport = !isOverUI(e.clientX, e.clientY);
  updatePassthrough(overViewport);
  if (overViewport) {
    _lastCoordX = e.clientX;
    _lastCoordY = e.clientY;
    // Throttle: update status bar coords at most every 80ms via WB query
    if (!_coordThrottleTimer) {
      _coordThrottleTimer = setTimeout(async () => {
        _coordThrottleTimer = null;
        const wc = await screenToWorldAsync(_lastCoordX, _lastCoordY);
        if (wc) {
          document.getElementById('status-coords').textContent = `X: ${wc.wx}  Y: ${wc.wy}`;
        }
      }, 80);
    }
  }
});

// ── Left mousedown: place object immediately, start drag-to-rotate ───────────
document.addEventListener('mousedown', e => {
  if (e.button !== 0) return;
  if (isOverUI(e.clientX, e.clientY)) return;
  if (_wizWpQuickPlace) {
    handleWizQuickWaypointPlace(e.clientX, e.clientY);
    return;
  }
  if (_wizPathDrawActive) {
    handleWizPathClick(e.clientX, e.clientY);
    return;
  }
  if (_wizAreaDrawActive) {
    handleWizAreaClick(e.clientX, e.clientY);
    return;
  }
  if (_wpPsActive) {
    handlePlayerStartPlace(e.clientX, e.clientY);
    return;
  }
  if (_msQpActive) {
    // Same drag-to-rotate flow as the objects tool: place at angle 0, then
    // the shared mousemove/mouseup machinery rotates the fresh selection.
    _dragOrigin = { x: e.clientX, y: e.clientY };
    _dragAngle  = 0;
    _placementPromise = msHandleQuickPlace(e.clientX, e.clientY);
    return;
  }
  if (_currentTool === 'waypoints') {
    handleWaypointMapClick(e.clientX, e.clientY);
    return;
  }
  if (_currentTool === 'objects' && _selectedTemplate) {
    const tmpl = _selectedTemplate;
    const team = undefined;
    _dragOrigin = { x: e.clientX, y: e.clientY };
    _dragAngle  = 0;
    // Place immediately at angle=0; building appears in WB and is auto-selected.
    // Store the promise so rotation updates wait for placement to complete.
    _placementPromise = (async () => {
      const wc = await screenToWorldAsync(e.clientX, e.clientY);
      if (!wc) return;
      const r = await api('/place/object', { template: tmpl, wx: wc.wx, wy: wc.wy, angle: 0, team });
      showFeedback('obj-place-feedback', r?.ok ? `Placed: ${tmpl}` : (r?.error || 'Failed'), r?.ok);
    })();
  }
  if (_currentTool === 'nature' && _selectedNature) {
    // Start grove drag — record origin, show circle overlay
    _groveDragStart = { x: e.clientX, y: e.clientY };
    drawGroveCircle(e.clientX, e.clientY, groveRadius() * 0.5); // approximate visual
  }
});

// ── Mousemove: grove overlay + rotate_selected while dragging ────────────────
document.addEventListener('mousemove', e => {
  // Right-drag pan forwarding for nature mode (passthrough=false, so WB needs synthetic events)
  if (_rPanActive && (e.buttons & 2)) {
    api('/view/synthetic_rmouse', { action: 'move', sx: e.screenX, sy: e.screenY });
  }
  // Grove drag: update circle overlay as user moves mouse
  if (_currentTool === 'nature' && _groveDragStart && (e.buttons & 1)) {
    // Use drag distance to set the radius visually
    const dx = e.clientX - _groveDragStart.x, dy = e.clientY - _groveDragStart.y;
    const pixelR = Math.max(20, Math.sqrt(dx * dx + dy * dy));
    drawGroveCircle(_groveDragStart.x, _groveDragStart.y, pixelR);
  }
  if (!_dragOrigin) return;
  if (!(e.buttons & 1)) {
    _dragOrigin = null;
    clearTimeout(_rotateTimer);
    _rotateTimer = null;
    return;
  }
  const dx = e.clientX - _dragOrigin.x;
  const dy = e.clientY - _dragOrigin.y;
  if (Math.sqrt(dx * dx + dy * dy) > 3) {
    // Negate dy: screen Y-down → world Y-up (WB convention)
    _dragAngle = Math.round(Math.atan2(-dy, dx) * 180 / Math.PI);
    if (!_rotateTimer) {
      _rotateTimer = setTimeout(async () => {
        _rotateTimer = null;
        // Wait for placement before rotating; object must be selected first
        await _placementPromise;
        if (_dragOrigin) {  // still dragging (not released)
          api('/object/rotate_selected', { angle: _dragAngle });
        }
      }, 50);
    }
  }
});

// ── Mouseup: grove plant or finalize rotation ────────────────────────────────
document.addEventListener('mouseup', async e => {
  if (e.button !== 0) return;

  // Grove brush: plant on mouseup
  if (_currentTool === 'nature' && _groveDragStart && _selectedNature) {
    const start = _groveDragStart;
    _groveDragStart = null;
    clearGroveCanvas();

    // Drag distance → world-unit radius (pixels are approximate — use slider for precise control)
    const dx = e.clientX - start.x, dy = e.clientY - start.y;
    const pixelDist = Math.sqrt(dx * dx + dy * dy);
    // If no drag (simple click), use slider radius. If drag, radius = slider * (pixelDist/80 clamped)
    const sliderR = groveRadius();
    const radius  = pixelDist < 10 ? sliderR : Math.max(20, Math.min(300, sliderR));

    const wc = await screenToWorldAsync(start.x, start.y);
    if (!wc) return;
    await api('/vegetation/grove', {
      type:        _selectedNature,
      cx:          wc.wx,
      cy:          wc.wy,
      radius,
      density:     groveDensity(),
      min_spacing: 15,
      skip_water:  document.getElementById('grove-skip-water')?.checked ? 1 : 0,
      skip_steep:  document.getElementById('grove-skip-steep')?.checked ? 1 : 0,
      rand_rot:    document.getElementById('grove-rand-rot')?.checked   ? 1 : 0,
      mix:         document.getElementById('grove-mix')?.checked        ? 1 : 0,
    });
    return;
  }

  if (!_dragOrigin) return;
  const finalAngle = _dragAngle;
  _dragOrigin = null;           // null before await so timer callbacks skip rotate
  clearTimeout(_rotateTimer);
  _rotateTimer = null;

  if (_msQpActive) {
    // Apply the final drag angle to the just-placed quick-place object
    await _placementPromise;
    if (finalAngle !== 0) {
      await api('/object/rotate_selected', { angle: finalAngle });
    }
    return;
  }

  if (_currentTool === 'objects' && _selectedTemplate) {
    // Wait for placement then apply the final drag angle
    await _placementPromise;
    if (finalAngle !== 0) {
      await api('/object/rotate_selected', { angle: finalAngle });
    }
    if (!e.ctrlKey) {
      selectObjectTemplate(null);
    }
  }
});

// ── Right-click: finish path / deselect template ─────────────────────────────
document.addEventListener('contextmenu', e => {
  if (isOverUI(e.clientX, e.clientY)) return;
  if (_wizPathDrawActive) {
    e.preventDefault();
    _finishWizPathDraw();
    return;
  }
  if (_wizAreaDrawActive) {
    e.preventDefault();
    _finishWizAreaDraw();
    return;
  }
  // Player start placement: right-click cancels, regardless of active tab
  if (_wpPsActive) {
    e.preventDefault();
    cancelPlayerStartPlace();
    return;
  }
  // Quick placement (Map Setup: Income/Extras): right-click stops placing
  if (_msQpActive) {
    e.preventDefault();
    msCancelQuickPlace();
    return;
  }
  if (_currentTool === 'waypoints') {
    e.preventDefault();
    if (_wpMode === 'path') finishWpPath();
    if (_wpMode === 'area') { finishWpArea(); return; }
    if (_wpMode !== 'selector') setWpMode('selector');
    return;
  }
  if (_currentTool === 'nature' && !!_selectedNature) { e.preventDefault(); return; }
  if (_currentTool !== 'objects') return;
  e.preventDefault();
  if (_selectedTemplate) {
    selectObjectTemplate(null);
    api('/tool', { tool: 'pointer' });
  }
});

// ── Right-mouse forwarding: pan in WB when passthrough=false ─────────────────
// Tools that capture left-clicks (nature, waypoints) also need right-drag pan
// forwarded to WbView3d via synthetic WM_R* messages.
// Exception: waypoints path/area mode uses right-click to finish the polygon (no pan).
let _rPanActive = false;

function _rightClickShouldPan() {
  if (_currentTool === 'nature' && !!_selectedNature) return true;
  return false;
}

document.addEventListener('mousedown', e => {
  if (e.button !== 2) return;
  if (isOverUI(e.clientX, e.clientY)) return;
  if (_rightClickShouldPan()) {
    _rPanActive = true;
    api('/view/synthetic_rmouse', { action: 'down', sx: e.screenX, sy: e.screenY });
  }
});

document.addEventListener('mouseup', e => {
  if (e.button !== 2) return;
  if (_rPanActive) {
    _rPanActive = false;
    api('/view/synthetic_rmouse', { action: 'up', sx: e.screenX, sy: e.screenY });
  }
});

// ── Mouse wheel: forward to WB for zoom (works in all modes) ─────────────────
document.addEventListener('wheel', e => {
  if (isOverUI(e.clientX, e.clientY)) return;
  e.preventDefault();
  _wheelAccum += e.deltaY < 0 ? 120 : -120;
  clearTimeout(_wheelTimer);
  _wheelTimer = setTimeout(() => {
    const delta = _wheelAccum > 0 ? 120 : -120;
    _wheelAccum = 0;
    api('/view/mouse_wheel', { delta });
  }, 60);
}, { passive: false });

document.addEventListener('mouseleave', () => {
  _lastOverViewport = null;
  _lastPassthrough  = null;
  _dragOrigin = null;
  clearTimeout(_rotateTimer);
  _rotateTimer = null;
  document.body.style.cursor = '';
  ipcRenderer.send('viewport-mouse', false);
});

