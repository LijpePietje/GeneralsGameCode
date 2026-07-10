// ── Setup panel (Players + Teams) ─────────────────────────────────────────────
let _suActiveTab   = 'players';
let _suSelPlayer   = null;   // currently selected player name

function initSetupPanel() {
  // Tab switching
  document.querySelectorAll('input[name="su-tab"]').forEach(radio => {
    radio.addEventListener('change', () => {
      _suActiveTab = radio.value;
      document.getElementById('su-players-section').style.display = _suActiveTab === 'players' ? '' : 'none';
      document.getElementById('su-teams-section').style.display   = _suActiveTab === 'teams'   ? '' : 'none';
      document.querySelectorAll('.draw-mode-btn[id^="su-tab"]').forEach(l => l.classList.remove('active'));
      radio.closest('.draw-mode-btn').classList.add('active');
      renderSetupTab();
    });
  });

  document.getElementById('btn-add-skirmish').addEventListener('click', async () => {
    const r = await api('/sidelist/add_skirmish', {});
    showFeedback('su-skirmish-feedback',
      r?.ok ? `Added ${r.added ?? 0} player(s) · validateSides done` : (r?.error || 'Failed'),
      !!r?.ok);
    if (r?.ok) fetchSideList();
  });

  document.getElementById('su-team-search').addEventListener('input', renderSuTeams);

  document.getElementById('btn-su-team-new').addEventListener('click', () => openTeamForm(null));

  document.getElementById('btn-su-t-add-unit').addEventListener('click', () => {
    addTeamUnitRow(document.getElementById('su-t-units'), '', 1);
  });

  document.getElementById('btn-su-team-cancel').addEventListener('click', () => {
    document.getElementById('su-team-form').style.display = 'none';
    _suSelTeam = null;
  });

  document.getElementById('btn-su-team-save').addEventListener('click', async () => {
    const name  = document.getElementById('su-t-name').value.trim();
    const owner = document.getElementById('su-t-owner').value;
    const home  = document.getElementById('su-t-home').value.trim();
    if (!name) { showFeedback('su-team-feedback', 'Name is required', false); return; }

    const body = { name, owner, home, maxInstances: 1 };
    const rows = document.getElementById('su-t-units').querySelectorAll('.form-row');
    rows.forEach((row, i) => {
      const inputs = row.querySelectorAll('input');
      const tpl = inputs[0]?.value.trim();
      const cnt = parseInt(inputs[1]?.value) || 1;
      if (tpl) {
        body[`unit${i}_template`] = tpl;
        body[`unit${i}_count`]    = cnt;
      }
    });

    const r = await api('/sidelist/team', body);
    showFeedback('su-team-feedback', r?.ok ? `Saved: ${name}` : (r?.error || 'Failed'), !!r?.ok);
    if (r?.ok) {
      fetchSideList();
      document.getElementById('su-team-form').style.display = 'none';
      _suSelTeam = null;
    }
  });

  document.getElementById('btn-su-team-del').addEventListener('click', async () => {
    if (!_suSelTeam?.name) return;
    const r = await api('/sidelist/team/delete', { name: _suSelTeam.name });
    showFeedback('su-team-feedback', r?.ok ? 'Deleted' : (r?.error || 'Failed'), !!r?.ok);
    if (r?.ok) {
      fetchSideList();
      document.getElementById('su-team-form').style.display = 'none';
      _suSelTeam = null;
    }
  });

  document.getElementById('btn-su-player-save').addEventListener('click', async () => {
    if (!_suSelPlayer) return;
    const displayName = document.getElementById('su-p-displayname').value.trim();
    const faction     = document.getElementById('su-p-faction').value;
    const money       = parseInt(document.getElementById('su-p-money').value) || 0;
    const r = await api('/sidelist/player', { name: _suSelPlayer, displayName, faction, money });
    showFeedback('su-player-feedback', r?.ok ? 'Saved' : (r?.error || 'Failed'), !!r?.ok);
    if (r?.ok) fetchSideList();
  });
}

function renderSetupTab() {
  if (!_sidelistData) return;
  if (_suActiveTab === 'players') renderSuPlayers();
  else renderSuTeams();
}

function renderSuPlayers() {
  const players = _sidelistData?.players || [];
  document.getElementById('su-player-count').textContent = players.length;
  const list = document.getElementById('su-player-list');
  list.innerHTML = '';
  for (const p of players) {
    const el = document.createElement('div');
    el.className = 'shape-item' + (p.name === _suSelPlayer ? ' active' : '');
    const icon = { America: '🦅', China: '🐉', GLA: '☪', Civilian: '👤' }[p.faction] || '👤';
    el.innerHTML = `<span style="margin-right:5px">${icon}</span><strong>${p.name}</strong>`
      + `<span class="hint-text" style="margin-left:auto;padding:0">$${(p.money||0).toLocaleString()}</span>`;
    el.addEventListener('click', () => selectSuPlayer(p));
    list.appendChild(el);
  }
}

function selectSuPlayer(p) {
  _suSelPlayer = p.name;
  renderSuPlayers(); // refresh active state
  const form = document.getElementById('su-player-form');
  form.style.display = '';
  document.getElementById('su-player-form-title').textContent = `Edit: ${p.name}`;
  document.getElementById('su-p-displayname').value = p.displayName || p.name;
  document.getElementById('su-p-faction').value     = p.faction || 'America';
  document.getElementById('su-p-money').value       = p.money || 10000;
}

let _suSelTeam = null;  // currently selected team object

function renderSuTeams() {
  const teams = _sidelistData?.teams || [];
  const q = (document.getElementById('su-team-search')?.value || '').toLowerCase();
  document.getElementById('su-team-count').textContent = teams.length;
  const list = document.getElementById('su-team-list');
  list.innerHTML = '';
  for (const t of teams) {
    if (q && !(t.name || '').toLowerCase().includes(q)) continue;
    const el = document.createElement('div');
    el.className = 'shape-item' + (t.name === _suSelTeam?.name ? ' active' : '');
    const unitSummary = (t.units || []).map(u => `${u.count}× ${u.template}`).join(', ');
    el.innerHTML = `<strong>${t.name}</strong>`
      + `<span class="hint-text" style="margin-left:auto;padding:0">${t.owner || '?'}</span>`;
    if (unitSummary) el.title = unitSummary;
    el.addEventListener('click', () => openTeamForm(t));
    list.appendChild(el);
    if (unitSummary) {
      const sub = document.createElement('div');
      sub.className = 'hint-text';
      sub.style.cssText = 'padding:1px 10px 4px;font-size:10px';
      sub.textContent = unitSummary;
      list.appendChild(sub);
    }
  }
}

function openTeamForm(team) {
  _suSelTeam = team || null;
  const isNew = !team;
  document.getElementById('su-team-form').style.display = '';
  document.getElementById('su-team-form-title').textContent = isNew ? 'New Team' : `Edit: ${team.name}`;
  document.getElementById('su-t-name').value  = team?.name  || '';
  document.getElementById('su-t-home').value  = team?.home  || '';
  document.getElementById('btn-su-team-del').style.display = isNew ? 'none' : '';

  // Populate owner dropdown from players
  const ownerSel = document.getElementById('su-t-owner');
  const players = _sidelistData?.players || [];
  ownerSel.innerHTML = players.map(p => `<option value="${p.name}">${p.name}</option>`).join('');
  if (team?.owner) {
    ownerSel.value = team.owner;
  } else if (isNew) {
    // Default new teams to SkirmishGLA (AOD convention)
    const gla = players.find(p => p.name === 'SkirmishGLA');
    if (gla) ownerSel.value = 'SkirmishGLA';
  }

  // Unit slots
  renderTeamUnitSlots(team?.units || []);
}

function renderTeamUnitSlots(units) {
  const container = document.getElementById('su-t-units');
  container.innerHTML = '';
  units.forEach((u, i) => addTeamUnitRow(container, u.template, u.count, i));
}

function addTeamUnitRow(container, template = '', count = 1, idx) {
  const row = document.createElement('div');
  row.className = 'form-row';
  row.style.cssText = 'margin-top:3px;gap:4px';
  row.innerHTML = `<input type="text" class="wb-form-input" placeholder="GLATrooper" value="${template}" style="flex:2">`
    + `<input type="number" class="wb-form-input wb-num" value="${count}" min="1" max="20" style="flex:1;width:48px">`
    + `<button class="action-btn" style="padding:2px 6px;min-width:24px">✕</button>`;
  row.querySelector('button').addEventListener('click', () => row.remove());
  container.appendChild(row);
}

// ── Full-screen Map Setup Modal ───────────────────────────────────────────────
let _msActiveTab   = 'players';
let _msSelPlayer   = null;
let _msSelTeam     = null;
let _editAllies    = new Set();
let _editEnemies   = new Set();

function openSetupModal() {
  document.getElementById('modal-setup').classList.remove('hidden');
  ipcRenderer.send('viewport-mouse', false); // modal blocks entire viewport — no passthrough
  fetchSideList().then(renderSetupModal);
}
function closeSetupModal() {
  document.getElementById('modal-setup').classList.add('hidden');
  // Restore normal passthrough state based on current tool/mouse position
  if (_lastOverViewport !== null) updatePassthrough(_lastOverViewport);
}

function renderSetupModal() {
  if (_msActiveTab === 'players') renderMsPlayers();
  else renderMsTeams();
}

// ── Players tab ───────────────────────────────────────────────────────────────
function renderMsPlayers() {
  const players = _sidelistData?.players || [];
  const list = document.getElementById('ms-player-list');
  list.innerHTML = '';
  for (const p of players) {
    const el = document.createElement('div');
    el.className = 'ms-list-item' + (p.name === _msSelPlayer?.name ? ' active' : '');
    const factionShort = (p.faction || '').replace('Faction', '').replace('Skirmish', '');
    el.innerHTML = `<span>${p.name}</span>`
      + `<span class="ms-li-sub">${factionShort}</span>`;
    el.addEventListener('click', () => selectMsPlayer(p));
    list.appendChild(el);
  }
}

function selectMsPlayer(p) {
  _msSelPlayer = p;
  renderMsPlayers();
  document.getElementById('ms-player-detail').style.display = 'block';
  document.getElementById('ms-player-empty').style.display  = 'none';
  document.getElementById('ms-p-name').textContent       = p.name;
  document.getElementById('ms-p-displayname').value      = p.displayName || p.name;
  document.getElementById('ms-p-faction').value          = p.faction || '';
  document.getElementById('ms-p-money').value            = p.money || 10000;
  document.getElementById('ms-p-color').value            = p.color || '';
  document.getElementById('ms-p-human').checked          = !p.isHuman; // checked = computer-controlled = isHuman:false
  _editAllies  = new Set((p.allies  || '').split(' ').filter(Boolean));
  _editEnemies = new Set((p.enemies || '').split(' ').filter(Boolean));
  renderRelationPanels(p);
}

function renderRelationPanels(player) {
  const players = _sidelistData?.players || [];
  const others  = players.filter(o => o.name !== player.name && o.name !== '(neutral)');

  function fillToggle(id, activeSet, otherSet) {
    const el = document.getElementById(id);
    if (!el) return;
    el.innerHTML = '';
    for (const o of others) {
      const item = document.createElement('div');
      item.className = 'ms-rel-item' + (activeSet.has(o.name) ? ' ms-rel-selected' : '');
      item.textContent = o.name;
      item.addEventListener('click', () => {
        if (activeSet.has(o.name)) { activeSet.delete(o.name); }
        else { activeSet.add(o.name); otherSet.delete(o.name); }
        renderRelationPanels(player);
      });
      el.appendChild(item);
    }
  }

  fillToggle('ms-allies-list',  _editAllies,  _editEnemies);
  fillToggle('ms-enemies-list', _editEnemies, _editAllies);
}

function collectRelations() {
  return {
    allies:  [..._editAllies].join(' '),
    enemies: [..._editEnemies].join(' '),
  };
}

// ── Teams tab ─────────────────────────────────────────────────────────────────
// Hierarchy: left column = players (team owners) → big field = that player's
// teams → clicking a team opens the team setup (detail panel).
let _msSelTeamPlayer = null;   // player whose teams are shown in the overview

function msPlayerLabel(p) { return p?.name || '(neutral)'; }

// Switch the right column between its three states
function msShowTeamPane(which) {  // 'empty' | 'overview' | 'detail'
  document.getElementById('ms-team-empty').style.display    = which === 'empty'    ? '' : 'none';
  document.getElementById('ms-team-overview').style.display = which === 'overview' ? 'flex' : 'none';
  document.getElementById('ms-team-detail').style.display   = which === 'detail'   ? 'flex' : 'none';
}

function renderMsTeams() {
  const players = _sidelistData?.players || [];
  const teams   = _sidelistData?.teams || [];
  const list = document.getElementById('ms-tp-list');
  list.innerHTML = '';
  for (const p of players) {
    const el = document.createElement('div');
    const isSel = _msSelTeamPlayer && (p.name || '') === (_msSelTeamPlayer.name || '');
    el.className = 'ms-list-item' + (isSel ? ' active' : '');
    const count = teams.filter(t => (t.owner || '') === (p.name || '')).length;
    el.innerHTML = `<span>${msPlayerLabel(p)}</span>`
      + `<span class="ms-li-sub">${count} team${count === 1 ? '' : 's'}</span>`;
    el.addEventListener('click', () => selectMsTeamPlayer(p));
    list.appendChild(el);
  }
  if (_msSelTeamPlayer) renderMsTeamOverview();
}

function selectMsTeamPlayer(p) {
  _msSelTeamPlayer = p;
  _msSelTeam = null;
  msShowTeamPane('overview');
  renderMsTeams();
}

function renderMsTeamOverview() {
  const p = _msSelTeamPlayer;
  if (!p) return;
  const q = (document.getElementById('ms-team-search')?.value || '').toLowerCase();
  const teams = (_sidelistData?.teams || []).filter(t => (t.owner || '') === (p.name || ''));
  document.getElementById('ms-team-overview-title').textContent = `Teams of ${msPlayerLabel(p)}`;
  document.getElementById('ms-team-overview-count').textContent = teams.length;
  const list = document.getElementById('ms-team-list');
  list.innerHTML = '';
  for (const t of teams) {
    if (q && !t.name.toLowerCase().includes(q)) continue;
    const el = document.createElement('div');
    el.className = 'ms-list-item' + (t.name === _msSelTeam?.name ? ' active' : '');
    const unitSummary = (t.units || []).map(u => `${u.maxCount ?? u.count}× ${u.template}`).join(', ');
    el.innerHTML = `<span>${t.name}</span>`
      + `<span class="ms-li-sub">${unitSummary || 'no units'}</span>`;
    el.addEventListener('click', () => selectMsTeam(t));
    list.appendChild(el);
  }
  if (!list.children.length) {
    const empty = document.createElement('div');
    empty.className = 'ms-li-sub';
    empty.style.cssText = 'padding:8px 4px;opacity:0.6';
    empty.textContent = q ? 'No teams match the filter' : 'This player has no teams yet';
    list.appendChild(empty);
  }
}

function selectMsTeam(t) {
  _msSelTeam = t;
  renderMsTeams();
  msShowTeamPane('detail');
  document.getElementById('btn-ms-team-del').style.display = t ? '' : 'none';
  document.getElementById('ms-team-back-label').textContent =
    t ? t.name : `New team for ${msPlayerLabel(_msSelTeamPlayer)}`;

  const v = (id, val) => { const el = document.getElementById(id); if (el) el.value = val ?? ''; };
  const c = (id, val) => { const el = document.getElementById(id); if (el) el.checked = !!val; };
  const s = (id, val) => { const el = document.getElementById(id); if (el) el.value = val ?? ''; };

  // Identity
  v('ms-t-name',          t?.name || '');
  v('ms-t-maxinst',       t?.maxInstances || 1);
  v('ms-t-home',          t?.home || '');
  c('ms-t-singleton',     t?.singleton);
  c('ms-t-autoreinforce', t?.autoReinforce);
  c('ms-t-ai-recruit',    t?.aiRecruitable);
  c('ms-t-exec-actions',  t?.execActions);
  v('ms-t-priority',      t?.priority || 0);
  v('ms-t-priority-suc',  t?.prioritySuccess || 0);
  v('ms-t-priority-fail', t?.priorityFailure || 0);
  v('ms-t-build-frames',  t?.buildFrames || 0);
  v('ms-t-prod-cond',     t?.productionCondition || '');
  v('ms-t-desc',          t?.description || '');

  // Owner dropdown — defaults to the player selected in the left column
  const ownerSel = document.getElementById('ms-t-owner');
  ownerSel.innerHTML = (_sidelistData?.players || [])
    .map(p => `<option value="${p.name}">${msPlayerLabel(p)}</option>`).join('');
  if (t?.owner) ownerSel.value = t.owner;
  else if (_msSelTeamPlayer) ownerSel.value = _msSelTeamPlayer.name || '';

  // Unit slots
  const container = document.getElementById('ms-t-units');
  container.innerHTML = '';
  (t?.units || []).forEach(u => addMsUnitRow(container, u.template, u.minCount || 0, u.maxCount || u.count || 1));

  // Reinforcement
  v('ms-t-veterancy',      t?.veterancy || 0);
  v('ms-t-transport',      t?.transport || '');
  v('ms-t-reinf-origin',   t?.reinforceOrigin || '');
  c('ms-t-starts-full',    t?.startsFull);
  c('ms-t-transports-exit',t?.transportsExit);

  // Behavior
  v('ms-t-s-create',       t?.onCreateScript || '');
  v('ms-t-s-enemy',        t?.onEnemySighted || '');
  v('ms-t-s-allclear',     t?.onAllClear || '');
  v('ms-t-destroyed-pct',  t?.destroyedPercent ?? 50);
  v('ms-t-s-destroyed',    t?.onDestroyed || '');
  v('ms-t-s-idle',         t?.onIdleScript || '');
  v('ms-t-s-unitdest',     t?.onUnitDestroyed || '');
  c('ms-t-transports-return', t?.transportsReturn);
  c('ms-t-avoid-threats',  t?.avoidThreats);
  s('ms-t-aggressiveness', t?.aggressiveness ?? 0);
  c('ms-t-common-target',  t?.commonTarget);

  // Generic scripts
  for (let i = 0; i < 16; i++) {
    v(`ms-t-generic-${i}`, t?.[`genericScript${i}`] || '');
  }

  // Object props
  v('ms-t-obj-health',  t?.objHealth ?? '');
  v('ms-t-obj-maxhp',   t?.objMaxHP ?? '');
  c('ms-t-obj-enabled', t?.objEnabled !== false);
  c('ms-t-obj-powered', t?.objPowered);
  c('ms-t-obj-unsellable', t?.objUnsellable);
  c('ms-t-obj-selectable', t?.objSelectable !== false);
  c('ms-t-obj-indestructible', t?.objIndestructible);
  c('ms-t-obj-ai-recruit', t?.objAiRecruit);
  s('ms-t-obj-aggr',    t?.objAggr ?? '');
  s('ms-t-obj-vet',     t?.objVet ?? '');
  s('ms-t-obj-weather', t?.objWeather ?? 0);
  s('ms-t-obj-time',    t?.objTime ?? 0);
  v('ms-t-obj-stop',    t?.objStop ?? '');
  v('ms-t-obj-target',  t?.objTarget ?? '');
  v('ms-t-obj-shroud',  t?.objShroud ?? '');
}

function addMsUnitRow(container, template = '', minCount = 0, maxCount = 1) {
  const row = document.createElement('div');
  row.className = 'ms-unit-row';

  const minIn = document.createElement('input');
  minIn.type = 'number'; minIn.className = 'ms-input';
  minIn.value = minCount; minIn.min = 0; minIn.max = 99;
  minIn.style.cssText = 'width:52px;flex-shrink:0';

  const maxIn = document.createElement('input');
  maxIn.type = 'number'; maxIn.className = 'ms-input';
  maxIn.value = maxCount; maxIn.min = 0; maxIn.max = 99;
  maxIn.style.cssText = 'width:52px;flex-shrink:0';

  const tmplDisp = document.createElement('div');
  tmplDisp.className = 'ms-unit-tmpl-display' + (template ? '' : ' empty');
  tmplDisp.title = template || 'Click to pick unit';
  tmplDisp.textContent = template || 'Click to pick unit…';
  tmplDisp.dataset.template = template;

  const delBtn = document.createElement('button');
  delBtn.className = 'ms-btn'; delBtn.style.cssText = 'padding:2px 6px;flex-shrink:0';
  delBtn.textContent = '✕';

  row.append(minIn, maxIn, tmplDisp, delBtn);

  delBtn.addEventListener('click', () => row.remove());
  tmplDisp.addEventListener('click', async () => {
    const palette = await loadObjectPalette();
    openObjPicker(palette, {
      unitOnly: true,
      callback: tmpl => {
        tmplDisp.textContent = tmpl;
        tmplDisp.title = tmpl;
        tmplDisp.dataset.template = tmpl;
        tmplDisp.classList.remove('empty');
      }
    });
  });

  container.appendChild(row);
}

async function autoSetAllEnemies() {
  const players = _sidelistData?.players || [];
  const combatants = players.filter(p => !p.name.toLowerCase().includes('civilian'));
  const names = combatants.map(p => p.name);
  await Promise.all(combatants.map(p =>
    api('/sidelist/player', {
      name:    p.name,
      allies:  '',
      enemies: names.filter(n => n !== p.name).join(' '),
    })
  ));
  await fetchSideList();
}

function applyMissionMode() {
  const on = document.getElementById('ms-mission-mode').checked;
  document.getElementById('ms-player-detail').classList.toggle('ms-mission-active', on);
  document.querySelectorAll('.ms-mission-only input, .ms-mission-only select').forEach(el => {
    el.disabled = !on;
  });
}

function initSetupModal() {
  document.getElementById('btn-modal-setup-close').addEventListener('click', closeSetupModal);
  document.getElementById('ms-mission-mode').addEventListener('change', applyMissionMode);

  // Inner team tab switching (scoped to team/object panels — excludes script sub-modal which uses data-stab)
  document.querySelectorAll('.ms-inner-tab').forEach(btn => {
    btn.addEventListener('click', () => {
      if (!btn.dataset.ttab) return; // script sub-modal tabs use data-stab, not data-ttab
      document.querySelectorAll('.ms-inner-tab').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      document.querySelectorAll('.ms-inner-body').forEach(c => c.classList.add('hidden'));
      document.getElementById(`ttab-${btn.dataset.ttab}`).classList.remove('hidden');
    });
  });

  // Generate 16 generic script inputs
  const gContainer = document.getElementById('ms-t-generic-scripts');
  for (let i = 0; i < 16; i++) {
    const row = document.createElement('div');
    row.className = 'ms-field-row';
    row.innerHTML = `<span class="ms-label">Script ${i + 1}</span>`
      + `<input type="text" class="ms-input" id="ms-t-generic-${i}" placeholder="<none>" style="flex:1">`;
    gContainer.appendChild(row);
  }

  // Tab switching
  document.querySelectorAll('.ms-tab').forEach(btn => {
    btn.addEventListener('click', () => {
      _msActiveTab = btn.dataset.mtab;
      document.querySelectorAll('.ms-tab').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      document.querySelectorAll('.ms-tab-body').forEach(c => c.classList.add('hidden'));
      document.getElementById(`mtab-${_msActiveTab}`).classList.remove('hidden');
      renderSetupModal();
    });
  });

  // New Player — auto-generates unique name, then selects for editing
  document.getElementById('btn-ms-new-player').addEventListener('click', async () => {
    const players = _sidelistData?.players || [];
    const existing = new Set(players.map(p => p.name));
    let n = 1;
    while (existing.has(`NewPlayer${n}`)) n++;
    const name = `NewPlayer${n}`;
    const r = await api('/sidelist/player/new', { name, faction: 'FactionAmerica' });
    if (r?.ok) {
      await fetchSideList();
      const created = (_sidelistData?.players || []).find(p => p.name === name);
      renderMsPlayers();
      if (created) selectMsPlayer(created);
    } else {
      showFeedback('ms-skirmish-feedback', r?.error || 'Failed', false);
    }
  });

  // Remove Player — removes currently selected player
  document.getElementById('btn-ms-remove-player').addEventListener('click', async () => {
    if (!_msSelPlayer) return;
    const r = await api('/sidelist/player/delete', { name: _msSelPlayer.name });
    if (r?.ok) {
      _msSelPlayer = null;
      document.getElementById('ms-player-detail').style.display = 'none';
      document.getElementById('ms-player-empty').style.display  = '';
      await fetchSideList();
      renderMsPlayers();
    } else {
      showFeedback('ms-player-feedback', r?.error || 'Failed', false);
    }
  });

  // Add Skirmish Players — then auto-set all non-Civilian as enemies of each other
  document.getElementById('btn-ms-add-skirmish').addEventListener('click', async () => {
    const r = await api('/sidelist/add_skirmish', {});
    if (!r?.ok) {
      showFeedback('ms-skirmish-feedback', r?.error || 'Failed', false);
      return;
    }
    await fetchSideList();
    await autoSetAllEnemies();
    renderMsPlayers();
    const added = r.added ?? 0;
    showFeedback('ms-skirmish-feedback', `Added ${added} player(s), relations set`, true);
  });

  // Save player
  document.getElementById('btn-ms-player-save').addEventListener('click', async () => {
    if (!_msSelPlayer) return;
    const { allies, enemies } = collectRelations();
    const missionMode = document.getElementById('ms-mission-mode').checked;
    const body = {
      name:        _msSelPlayer.name,
      displayName: document.getElementById('ms-p-displayname').value.trim(),
      faction:     document.getElementById('ms-p-faction').value,
      isHuman:     !document.getElementById('ms-p-human').checked,
      allies, enemies,
    };
    if (missionMode) {
      body.money = parseInt(document.getElementById('ms-p-money').value) || 0;
      const color = document.getElementById('ms-p-color').value;
      if (color) body.color = color;
    }
    const r = await api('/sidelist/player', body);
    showFeedback('ms-player-feedback', r?.ok ? 'Saved' : (r?.error || 'Failed'), !!r?.ok);
    if (r?.ok) { await fetchSideList(); renderMsPlayers(); }
  });

  // New team button — opens an empty team setup, owner preset to selected player
  document.getElementById('btn-ms-team-new').addEventListener('click', () => {
    selectMsTeam(null);
  });

  // Back from team setup to the player's team overview
  document.getElementById('btn-ms-team-back').addEventListener('click', () => {
    _msSelTeam = null;
    msShowTeamPane(_msSelTeamPlayer ? 'overview' : 'empty');
    renderMsTeams();
  });

  // Add unit slot
  document.getElementById('btn-ms-t-add-unit').addEventListener('click', () =>
    addMsUnitRow(document.getElementById('ms-t-units')));

  // Team search
  document.getElementById('ms-team-search').addEventListener('input', renderMsTeams);

  // Save team — collect all 5 tabs
  document.getElementById('btn-ms-team-save').addEventListener('click', async () => {
    const g = id => document.getElementById(id);
    const gv = id => g(id)?.value?.trim() ?? '';
    const gi = (id, def = 0) => parseInt(g(id)?.value) || def;
    const gb = id => !!g(id)?.checked;

    const name = gv('ms-t-name');
    if (!name) { showFeedback('ms-team-feedback', 'Name is required', false); return; }

    const body = {
      name,
      owner:        gv('ms-t-owner'),
      home:         gv('ms-t-home'),
      maxInstances: gi('ms-t-maxinst', 1),
      // Identity flags
      singleton:     gb('ms-t-singleton'),
      autoReinforce: gb('ms-t-autoreinforce'),
      aiRecruitable: gb('ms-t-ai-recruit'),
      execActions:   gb('ms-t-exec-actions'),
      priority:      gi('ms-t-priority'),
      prioritySuccess: gi('ms-t-priority-suc'),
      priorityFailure: gi('ms-t-priority-fail'),
      buildFrames:   gi('ms-t-build-frames'),
      productionCondition: gv('ms-t-prod-cond'),
      description:   gv('ms-t-desc'),
      // Reinforcement
      veterancy:     gi('ms-t-veterancy'),
      transport:     gv('ms-t-transport'),
      reinforceOrigin: gv('ms-t-reinf-origin'),
      startsFull:    gb('ms-t-starts-full'),
      transportsExit: gb('ms-t-transports-exit'),
      // Behavior
      aggressiveness:  parseInt(g('ms-t-aggressiveness')?.value) || 0,
      destroyedPercent: gi('ms-t-destroyed-pct', 50),
      transportsReturn: gb('ms-t-transports-return'),
      avoidThreats:    gb('ms-t-avoid-threats'),
      commonTarget:    gb('ms-t-common-target'),
      onCreateScript:  gv('ms-t-s-create'),
      onIdleScript:    gv('ms-t-s-idle'),
      onEnemySighted:  gv('ms-t-s-enemy'),
      onDestroyed:     gv('ms-t-s-destroyed'),
      onAllClear:      gv('ms-t-s-allclear'),
      onUnitDestroyed: gv('ms-t-s-unitdest'),
      // Object props
      objEnabled:      gb('ms-t-obj-enabled'),
      objPowered:      gb('ms-t-obj-powered'),
      objUnsellable:   gb('ms-t-obj-unsellable'),
      objSelectable:   gb('ms-t-obj-selectable'),
      objIndestructible: gb('ms-t-obj-indestructible'),
      objAiRecruit:    gb('ms-t-obj-ai-recruit'),
    };

    // Optional obj props (only send if set)
    const objH = gi('ms-t-obj-health', -1); if (objH >= 0) body.objHealth = objH;
    const objM = gi('ms-t-obj-maxhp',  -1); if (objM > 0)  body.objMaxHP  = objM;
    const objA = g('ms-t-obj-aggr')?.value;  if (objA !== '') body.objAggr = parseInt(objA);
    const objV = g('ms-t-obj-vet')?.value;   if (objV !== '') body.objVet  = parseInt(objV);
    const objW = gi('ms-t-obj-weather'); if (objW) body.objWeather = objW;
    const objT = gi('ms-t-obj-time');   if (objT) body.objTime    = objT;
    const objS = gi('ms-t-obj-stop');   if (objS) body.objStop    = objS;
    const objTg = gi('ms-t-obj-target');if (objTg) body.objTarget  = objTg;
    const objSh = gi('ms-t-obj-shroud');if (objSh) body.objShroud  = objSh;

    // Generic scripts
    for (let i = 0; i < 16; i++) {
      const v = gv(`ms-t-generic-${i}`);
      if (v) body[`genericScript${i}`] = v;
      else break;
    }

    // Unit slots (min/max/template)
    let ui = 0;
    g('ms-t-units').querySelectorAll('.ms-unit-row').forEach(row => {
      const inputs  = row.querySelectorAll('input');
      const tpl = row.querySelector('.ms-unit-tmpl-display')?.dataset.template?.trim();
      if (!tpl) return;
      body[`unit${ui}_template`] = tpl;
      body[`unit${ui}_min`]      = parseInt(inputs[0]?.value) || 0;
      body[`unit${ui}_max`]      = parseInt(inputs[1]?.value) || 1;
      ui++;
    });

    const r = await api('/sidelist/team', body);
    showFeedback('ms-team-feedback', r?.ok ? `Saved: ${name}` : (r?.error || 'Failed'), !!r?.ok);
    if (r?.ok) {
      await fetchSideList();
      _msSelTeam = (_sidelistData?.teams || []).find(t => t.name === name) || _msSelTeam;
      document.getElementById('ms-team-back-label').textContent = name;
      document.getElementById('btn-ms-team-del').style.display = '';
      renderMsTeams();
    }
  });

  // Delete team
  document.getElementById('btn-ms-team-del').addEventListener('click', async () => {
    if (!_msSelTeam) return;
    const r = await api('/sidelist/team/delete', { name: _msSelTeam.name });
    if (r?.ok) {
      _msSelTeam = null;
      msShowTeamPane(_msSelTeamPlayer ? 'overview' : 'empty');
      await fetchSideList(); renderMsTeams();
    } else {
      showFeedback('ms-team-feedback', r?.error || 'Failed', false);
    }
  });
}

