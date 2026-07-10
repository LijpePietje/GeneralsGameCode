// ── SidesList panel (Players · Teams · Scripts) ───────────────────────────────
let _sidelistData    = null;   // last fetched {players, teams}
let _slPollTimer     = null;
let _slActiveTab     = 'wizard';

function startSlPolling() {
  stopSlPolling();
  fetchSideList();
  _slPollTimer = setInterval(fetchSideList, 5000);
}
function stopSlPolling() {
  if (_slPollTimer) { clearInterval(_slPollTimer); _slPollTimer = null; }
}

let _wbWasConnected = false;

async function fetchSideList() {
  const r = await apiGet('/sidelist');
  if (!r || !r.ok) {
    const st = document.getElementById('sl-status');
    if (st) st.textContent = r?.error || 'Not connected';
    _wbWasConnected = false;
    if (_mscOpen) mscRenderTree();
    return;
  }
  const firstConnect = !_wbWasConnected;
  _wbWasConnected = true;
  _sidelistData = r;
  const st = document.getElementById('sl-status');
  if (st) st.textContent = '';
  renderSlTab(_slActiveTab);
  renderSetupTab();
  if (!document.getElementById('modal-setup').classList.contains('hidden')) renderSetupModal();
  if (_mscOpen) mscRefreshPlayerSel();
  // On first connection to WB, ensure all skirmish players are present
  if (firstConnect) {
    autoAddSkirmishPlayers();
  }
}

function renderSlTab(tab) {
  if (!_sidelistData) return;
  if (tab === 'wizard')  renderSlWizardPlayerList();
  else                   renderSlScripts();
}


function renderSlScripts() {
  const players = _sidelistData.players || [];
  // Populate player dropdown
  const sel = document.getElementById('sl-script-player');
  const prevVal = sel.value;
  sel.innerHTML = players.map(p => `<option value="${p.name}">${p.name}</option>`).join('');
  if (prevVal) sel.value = prevVal;
  const playerName = sel.value;
  const player = players.find(p => p.name === playerName);
  if (!player) return;

  const q = (document.getElementById('sl-script-search').value || '').toLowerCase();
  const list = document.getElementById('sl-script-list');
  list.innerHTML = '';

  for (const group of player.scriptGroups || []) {
    // Group header
    const gh = document.createElement('div');
    gh.className = 'section-label';
    gh.style.cssText = 'padding:4px 8px;margin-top:4px;background:#2a2000;font-size:10px';
    gh.textContent = `${group.name} ${group.active ? '' : '(disabled)'}`;
    list.appendChild(gh);

    for (const s of group.scripts || []) {
      if (q && !s.name.toLowerCase().includes(q)) continue;
      const el = document.createElement('div');
      el.className = 'shape-item';
      el.style.flexDirection = 'column';
      el.style.alignItems    = 'stretch';
      el.style.cursor        = 'default';

      const condCount = (s.conditions || []).reduce((n, or) => n + (or||[]).length, 0);
      const actCount  = (s.actionsTrue || []).length + (s.actionsFalse || []).length;
      const flags = [s.easy && 'E', s.normal && 'N', s.hard && 'H'].filter(Boolean).join('');

      // Header row
      const hdr = document.createElement('div');
      hdr.style.cssText = 'display:flex;align-items:center;gap:4px';
      hdr.innerHTML = `<span>${s.active ? '▶' : '⏸'}</span>`
        + `<strong style="flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">${s.name}</strong>`
        + `<span class="hint-text" style="font-size:10px;white-space:nowrap">${condCount}C/${actCount}A ${flags}</span>`;
      el.appendChild(hdr);

      // Action buttons row
      const btns = document.createElement('div');
      btns.style.cssText = 'display:flex;gap:4px;margin-top:4px';

      const btnToggle = document.createElement('button');
      btnToggle.className = 'action-btn';
      btnToggle.style.cssText = 'flex:1;font-size:10px;padding:2px';
      btnToggle.textContent = s.active ? 'Disable' : 'Enable';
      btnToggle.addEventListener('click', async (e) => {
        e.stopPropagation();
        const r = await api('/sidelist/script', {
          player: playerName, group: group.name, name: s.name,
          active: !s.active, easy: s.easy, normal: s.normal, hard: s.hard,
          oneShot: s.oneShot, subroutine: s.subroutine,
          condCount: 0, actCount: 0, actFalseCount: 0
        });
        if (r?.ok) fetchSideList();
      });
      btns.appendChild(btnToggle);

      const btnDel = document.createElement('button');
      btnDel.className = 'action-btn danger';
      btnDel.style.cssText = 'flex:1;font-size:10px;padding:2px';
      btnDel.textContent = 'Delete';
      btnDel.addEventListener('click', async (e) => {
        e.stopPropagation();
        const r = await api('/sidelist/script/delete', { player: playerName, group: group.name, name: s.name });
        if (r?.ok) fetchSideList();
      });
      btns.appendChild(btnDel);

      el.appendChild(btns);
      list.appendChild(el);
    }
  }
  if (list.children.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'hint-text';
    empty.style.padding = '8px';
    empty.textContent = 'No scripts';
    list.appendChild(empty);
  }
}

function initSideListPanel() {
  // Tab switching
  document.querySelectorAll('input[name="sl-tab"]').forEach(radio => {
    radio.addEventListener('change', () => {
      _slActiveTab = radio.value;
      ['wizard','scripts'].forEach(t => {
        const el = document.getElementById(`sl-${t}-section`);
        if (el) el.style.display = (_slActiveTab === t) ? '' : 'none';
      });
      document.querySelectorAll('.draw-mode-btn[id^="sl-tab"]').forEach(l => l.classList.remove('active'));
      radio.closest('.draw-mode-btn').classList.add('active');
      renderSlTab(_slActiveTab);
    });
  });

  // Open Script Editor buttons (panel top + scripts tab bottom)
  document.getElementById('sl-open-editor').addEventListener('click', openScriptsModal);
  document.getElementById('sl-scripts-open-editor').addEventListener('click', openScriptsModal);

  // Inline team creation modal
  document.getElementById('wt-cancel').addEventListener('click', closeWizTeamModal);
  document.getElementById('wt-create').addEventListener('click', createWizTeam);
  document.getElementById('wt-add-unit').addEventListener('click', () => addWizTeamUnitRow());
  document.getElementById('wiz-team-backdrop').addEventListener('click', e => {
    if (e.target === e.currentTarget) closeWizTeamModal();
  });

  // Quick Script wizard buttons
  document.querySelectorAll('[data-wiz]').forEach(btn => {
    btn.addEventListener('click', () => {
      _wizTemplate = btn.dataset.wiz;
      openWizForm(_wizTemplate);
    });
  });

  document.getElementById('sl-wiz-cancel').addEventListener('click', () => {
    document.getElementById('sl-wiz-form').style.display = 'none';
    _wizTemplate = null;
    _editingInstanceId = null;
    renderWizInstances();
  });

  document.getElementById('sl-wiz-apply').addEventListener('click', () => applyWizard(_wizTemplate));

  // Search + player picker re-render
  document.getElementById('sl-script-search').addEventListener('input', () => renderSlScripts());
  document.getElementById('sl-script-player').addEventListener('change', () => renderSlScripts());
}

