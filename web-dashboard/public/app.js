const API = '';
let token = localStorage.getItem('gwa-token');
let selectedBot = null;
let eventSource = null;
let llmEventSource = null;
let allCharacters = [];
let llmState = {
  status: 'stopped',
  observers: 0,
  plan: {},
  snapshot: {},
  toolCalls: [],
  runHistory: [],
  degradation: null,
  profile: {
    name: 'qwen-safe',
    supervisor_mode: 'deterministic',
    executor_mode: 'health-check',
    planner_mode: 'async',
    prompt_mode: 'delta',
    planner_model: 'qwen3.5:cloud',
    executor_model: 'qwen3.5:cloud',
  },
};

// Map IDs to human-readable names
const MAP_NAMES = {
  0: 'Unknown',
  638: "Gadd's Encampment",
  558: 'Sparkfly Swamp',
  615: 'Bogroot Growths Lv1',
  616: 'Bogroot Growths Lv2',
  857: 'Embark Beach',
};

// ---- Auth ----
function api(path, opts = {}) {
  return fetch(API + path, {
    ...opts,
    headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}`, ...opts.headers },
  });
}

async function login() {
  const input = document.getElementById('token-input');
  const err = document.getElementById('login-error');
  const t = input.value.trim();
  if (!t) { err.textContent = 'Enter a token'; return; }

  const res = await fetch(API + '/api/auth/login', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ token: t }),
  });
  if (res.ok) {
    token = t;
    localStorage.setItem('gwa-token', t);
    showDashboard();
  } else {
    err.textContent = 'Invalid token';
  }
}

function logout() {
  token = null;
  localStorage.removeItem('gwa-token');
  if (eventSource) eventSource.close();
  if (llmEventSource) llmEventSource.close();
  document.getElementById('login-page').classList.remove('hidden');
  document.getElementById('dashboard-page').classList.add('hidden');
}

document.getElementById('token-input').addEventListener('keydown', e => {
  if (e.key === 'Enter') login();
});

// ---- Dashboard ----
async function showDashboard() {
  document.getElementById('login-page').classList.add('hidden');
  document.getElementById('dashboard-page').classList.remove('hidden');
  await loadCharacters();
  connectSSE();
  connectLlmSSE();
  await loadLlmState();
  // Initial load
  const res = await api('/api/bots');
  if (res.ok) renderBots(await res.json());
}

function connectSSE() {
  if (eventSource) eventSource.close();
  eventSource = new EventSource(API + `/api/events?token=${token}`);
  eventSource.onmessage = (e) => {
    try { renderBots(JSON.parse(e.data)); } catch {}
  };
  eventSource.onerror = () => {
    setTimeout(connectSSE, 5000);
  };
}

async function loadLlmState() {
  const res = await api('/api/llm/state');
  if (!res.ok) {
    setLlmStatus('degraded');
    return;
  }
  const data = await res.json();
  llmState.status = (data.bridge || {}).status || 'stopped';
  llmState.observers = (data.bridge || {}).observers || 0;
  llmState.plan = data.plan || {};
  llmState.toolCalls = data.last_tool_calls || [];
  llmState.runHistory = data.run_summaries || [];
  llmState.degradation = data.degradation || null;
  llmState.profile = data.profile || llmState.profile;
  renderLlm();
}

function connectLlmSSE() {
  if (llmEventSource) llmEventSource.close();
  llmEventSource = new EventSource(API + `/api/llm/stream?token=${token}`);
  llmEventSource.addEventListener('bridge.status', e => {
    const data = parseEvent(e);
    llmState.observers = data.observers == null ? llmState.observers : data.observers;
    if (data.profile) llmState.profile = data.profile;
    setLlmStatus(data.status || 'stopped');
  });
  llmEventSource.addEventListener('llm.telemetry', e => {
    const data = parseEvent(e);
    if (data.profile) llmState.profile = data.profile;
    renderLlmProfile();
  });
  llmEventSource.addEventListener('chat.user', e => {
    appendChat('Operator', parseEvent(e).message || '');
  });
  llmEventSource.addEventListener('chat.assistant', e => {
    appendChat('Assistant', parseEvent(e).message || '');
  });
  llmEventSource.addEventListener('tool.call', e => {
    const data = parseEvent(e);
    llmState.toolCalls.unshift({ ...data, kind: 'call' });
    llmState.toolCalls = llmState.toolCalls.slice(0, 10);
    appendChat('Tool', `[${data.role || '?'} -> ${data.name || '?'}]`);
    renderLlmTools();
  });
  llmEventSource.addEventListener('tool.result', e => {
    const data = parseEvent(e);
    llmState.toolCalls.unshift({ ...data, kind: 'result' });
    llmState.toolCalls = llmState.toolCalls.slice(0, 10);
    renderLlmTools();
  });
  llmEventSource.addEventListener('plan.updated', e => {
    llmState.plan = parseEvent(e);
    document.querySelector('.plan-panel').classList.add('plan-flash');
    setTimeout(() => document.querySelector('.plan-panel').classList.remove('plan-flash'), 900);
    renderLlm();
  });
  llmEventSource.addEventListener('snapshot.summary', e => {
    llmState.snapshot = parseEvent(e);
    renderLlm();
  });
  llmEventSource.addEventListener('run.summary', e => {
    llmState.runHistory.unshift(parseEvent(e));
    llmState.runHistory = llmState.runHistory.slice(0, 20);
    renderRunHistory();
  });
  llmEventSource.addEventListener('degradation', e => {
    const data = parseEvent(e);
    llmState.degradation = data;
    renderLlmDegradation();
  });
  llmEventSource.onerror = () => {
    setLlmStatus('reconnecting');
    setTimeout(connectLlmSSE, 5000);
  };
}

function parseEvent(e) {
  try { return JSON.parse(e.data || '{}'); } catch { return {}; }
}

function setLlmStatus(status) {
  llmState.status = status;
  renderLlm();
}

async function llmLaunch() {
  const res = await api('/api/llm/launch', {
    method: 'POST',
    body: JSON.stringify({ lane: 'beastrit' }),
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok || data.ok === false) {
    llmState.degradation = {
      reason: data.error || 'launch_failed',
      surface: data.message || data.error || 'Launch failed',
    };
    renderLlmDegradation();
    await loadLlmState();
  } else if (data.status) {
    setLlmStatus(data.status);
  }
}

async function llmStop() {
  document.getElementById('llm-stop-btn').disabled = true;
  await api('/api/llm/stop', { method: 'POST', body: '{}' });
  document.getElementById('llm-stop-btn').disabled = false;
}

async function llmSendChat(event) {
  event.preventDefault();
  const input = document.getElementById('llm-chat-input');
  const message = input.value.trim();
  if (!message) return;
  input.value = '';
  await api('/api/llm/chat', {
    method: 'POST',
    body: JSON.stringify({ message }),
  });
}

function appendChat(author, message) {
  if (!message) return;
  const log = document.getElementById('llm-chat-log');
  const row = document.createElement('div');
  row.className = `chat-row chat-${author.toLowerCase()}`;
  row.innerHTML = `<span>${escapeHtml(author)}</span><p>${escapeHtml(message)}</p>`;
  log.appendChild(row);
  log.scrollTop = log.scrollHeight;
}

function renderLlm() {
  const status = document.getElementById('llm-status');
  if (!status) return;
  status.textContent = llmState.status || 'stopped';
  status.className = `status-badge status-${llmState.status || 'stopped'}`;
  const observers = document.getElementById('llm-observers');
  if (observers) {
    observers.textContent = `${llmState.observers || 0} observers connected`;
  }
  renderLlmProfile();

  const plan = llmState.plan || {};
  document.getElementById('llm-plan-phase').textContent = plan.phase || '--';
  document.getElementById('llm-plan-intent').textContent = plan.intent || '--';
  document.getElementById('llm-plan-next').textContent = plan.next_step || '--';
  document.getElementById('llm-plan-deviation').textContent = plan.deviation || '--';

  const snap = llmState.snapshot || {};
  const mapId = snap.map;
  document.getElementById('llm-snapshot-map').textContent =
    snap.map_name || MAP_NAMES[mapId] || (mapId == null ? '--' : `Map ${mapId}`);
  document.getElementById('llm-snapshot-hp').textContent =
    typeof snap.hp === 'number' ? `${Math.round(snap.hp * 100)}%` : '--';
  const party = snap.party || {};
  document.getElementById('llm-snapshot-party').textContent =
    party.size ? `${party.size} members, ${party.dead || 0} dead` : '--';
  document.getElementById('llm-snapshot-slots').textContent =
    snap.free_slots == null ? '--' : String(snap.free_slots);

  renderLlmTools();
  renderRunHistory();
  renderLlmDegradation();
}

function renderLlmProfile() {
  const profile = llmState.profile || {};
  setText('llm-profile-name', profile.name || 'qwen-safe');
  setText('llm-supervisor-mode', profile.supervisor_mode || '--');
  setText('llm-executor-mode', profile.executor_mode || '--');
  setText('llm-planner-mode', profile.planner_mode || '--');
  setText('llm-planner-model', profile.planner_model || profile.model || '--');
  setText('llm-prompt-mode', profile.prompt_mode || '--');
}

function setText(id, value) {
  const el = document.getElementById(id);
  if (el) el.textContent = value;
}

function renderLlmDegradation() {
  const banner = document.getElementById('llm-degradation');
  const degradation = llmState.degradation;
  if (!banner) return;
  if (!degradation || llmState.status === 'stopped') {
    banner.classList.add('hidden');
    banner.textContent = '';
    return;
  }
  banner.textContent = degradation.surface || degradation.reason || 'LLM bridge degraded';
  banner.classList.remove('hidden');
}

function renderLlmTools() {
  const list = document.getElementById('llm-tool-list');
  list.innerHTML = llmState.toolCalls.slice(0, 10).map(call => {
    if (call.kind === 'result' || Object.prototype.hasOwnProperty.call(call, 'success')) {
      return `<li class="${call.success ? 'ok' : 'bad'}">result ${escapeHtml(call.request_id || '')}: ${call.success ? 'ok' : escapeHtml(call.error || 'error')}</li>`;
    }
    return `<li>[${escapeHtml(call.role || '?')} -> ${escapeHtml(call.name || '?')}]</li>`;
  }).join('');
}

function renderRunHistory() {
  const el = document.getElementById('llm-run-history');
  el.innerHTML = llmState.runHistory.map((run, idx) => `
    <details ${idx === 0 ? 'open' : ''}>
      <summary>${escapeHtml(run.title || run.phase || `Run ${idx + 1}`)}</summary>
      <pre>${escapeHtml(JSON.stringify(run, null, 2))}</pre>
    </details>
  `).join('') || '<p class="muted">No run summaries yet.</p>';
}

function renderBots(bots) {
  const grid = document.getElementById('bot-grid');

  // Merge IPC data with known accounts — show all 5 always
  const ipcMap = {};
  bots.filter(b => b.character && b._id !== 'testbot').forEach(b => { ipcMap[b._id] = b; });

  const allBots = allCharacters.map(name => {
    const id = name.replace(/ /g, '').toLowerCase();
    return ipcMap[id] || { _id: id, character: name, state: 'offline', _offline: true };
  });

  grid.innerHTML = allBots.map(bot => {
    const isOffline = bot._offline || bot.state === 'offline' || bot.state === 'stale';
    const isLive = !bot._offline && bot.state !== 'offline' && bot.state !== 'stale';
    const state = bot.state || 'offline';
    const mapName = MAP_NAMES[bot.map_id] || (bot.map_id ? `Map ${bot.map_id}` : '---');
    const gold = bot.gold || {};
    const stats = bot.stats || {};
    const settings = bot.settings || {};
    const logLines = (bot.log || []).join('\n');

    return `
    <div class="bot-card ${selectedBot === bot._id ? 'selected' : ''} ${isOffline ? 'bot-offline' : ''}">
      <div class="bot-header">
        <span class="bot-name">${bot.character || bot._id}</span>
        <span class="status-badge status-${state}">${state}</span>
      </div>
      <div class="bot-stats">
        <div class="bot-stat"><span class="label">Map</span><span class="value">${mapName}</span></div>
        <div class="bot-stat"><span class="label">Runs</span><span class="value">${stats.run_count || 0}/${stats.fail_count || 0}</span></div>
        <div class="bot-stat"><span class="label">Gold</span><span class="value">${formatGold(gold.character)}/${formatGold(gold.storage)}</span></div>
        <div class="bot-stat"><span class="label">Best</span><span class="value">${stats.best_run_time || '---'}</span></div>
        <div class="bot-stat"><span class="label">Avg</span><span class="value">${stats.avg_run_time || '---'}</span></div>
        <div class="bot-stat"><span class="label">Total</span><span class="value">${stats.total_time || '---'}</span></div>
        <div class="bot-stat"><span class="label">Config</span><span class="value">${settings.hero_config || '---'}</span></div>
        <div class="bot-stat"><span class="label">Uptime</span><span class="value">${formatUptime(bot.uptime_seconds)}</span></div>
      </div>
      <div class="bot-actions" onclick="event.stopPropagation()">
        ${isLive && bot.bot_running
          ? `<button onclick="sendCommand('${bot._id}', 'stop')">Stop</button>`
          : isLive
            ? `<button class="btn-success" onclick="sendCommand('${bot._id}', 'start')">Resume</button>`
            : `<button class="btn-launch" onclick="launchBotDirect('${bot.character}')">Launch</button>`
        }
        ${isLive ? `<button class="btn-danger" onclick="sendCommand('${bot._id}', 'kill')">Kill</button>` : ''}
      </div>
    </div>
    ${logLines ? `<div class="bot-log" id="log-${bot._id}"><pre>${escapeHtml(logLines)}</pre></div>` : ''}`;
  }).join('');

  // Auto-scroll all log boxes to bottom
  for (const el of document.querySelectorAll('.bot-log pre')) {
    el.scrollTop = el.scrollHeight;
  }

  // Aggregate stats
  const totalRuns = allBots.reduce((s, b) => s + ((b.stats || {}).run_count || 0), 0);
  const online = allBots.filter(b => !b._offline && b.state !== 'offline' && b.state !== 'stale').length;
  document.getElementById('agg-runs').textContent = totalRuns;
  document.getElementById('agg-online').textContent = `${online}/${allBots.length}`;
  document.getElementById('health-badge').textContent = `${online} online`;

  // (legacy log panel update removed — each card has its own log now)
  if (false) {
    if (bot && bot.log) updateLog(bot.log);
  }
}

function selectBot(id, log) {
  selectedBot = selectedBot === id ? null : id;
  const panel = document.getElementById('log-panel');
  if (selectedBot) {
    panel.classList.remove('hidden');
    document.getElementById('log-title').textContent = `Log: ${id}`;
    updateLog(log || []);
  } else {
    panel.classList.add('hidden');
  }
}

function updateLog(lines) {
  const el = document.getElementById('log-content');
  el.textContent = lines.join('\n');
  el.scrollTop = el.scrollHeight;
}

function closeLog() {
  selectedBot = null;
  document.getElementById('log-panel').classList.add('hidden');
}

async function sendCommand(botId, action) {
  await api(`/api/bots/${botId}/command`, {
    method: 'POST',
    body: JSON.stringify({ action }),
  });
}

// ---- Launch Modal ----
async function loadCharacters() {
  const res = await api('/api/config/accounts');
  if (!res.ok) return;
  allCharacters = await res.json();
  const sel = document.getElementById('launch-character');
  sel.innerHTML = allCharacters.map(c => `<option value="${c}">${c}</option>`).join('');
}

async function launchBotDirect(character) {
  await api('/api/bots/launch', {
    method: 'POST',
    body: JSON.stringify({ character }),
  });
}

async function launchAllBots() {
  for (const name of allCharacters) {
    await api('/api/bots/launch', {
      method: 'POST',
      body: JSON.stringify({ character: name }),
    });
    await new Promise(r => setTimeout(r, 3000)); // stagger launches
  }
}

async function killAllBots() {
  const res = await api('/api/bots');
  if (!res.ok) return;
  const bots = await res.json();
  for (const bot of bots) {
    if (bot._id && bot._id !== 'testbot' && bot.state !== 'offline') {
      await api(`/api/bots/${bot._id}/command`, {
        method: 'POST',
        body: JSON.stringify({ action: 'kill' }),
      });
    }
  }
}

function showLaunchModal() { document.getElementById('launch-modal').classList.remove('hidden'); }
function closeLaunchModal() { document.getElementById('launch-modal').classList.add('hidden'); }

async function launchBot() {
  const character = document.getElementById('launch-character').value;
  await api('/api/bots/launch', {
    method: 'POST',
    body: JSON.stringify({ character }),
  });
  closeLaunchModal();
}

// ---- Helpers ----
function formatGold(n) {
  if (!n) return '0';
  if (n >= 1000000) return (n / 1000000).toFixed(1) + 'M';
  if (n >= 1000) return (n / 1000).toFixed(0) + 'k';
  return String(n);
}

function formatUptime(secs) {
  if (!secs) return '---';
  const h = Math.floor(secs / 3600);
  const m = Math.floor((secs % 3600) / 60);
  return `${h}h ${m}m`;
}

function escapeHtml(s) {
  s = String(s == null ? '' : s);
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

// ---- Init ----
if (token) {
  // Verify token is still valid
  api('/api/health').then(res => {
    if (res.ok) showDashboard();
    else logout();
  }).catch(() => {
    document.getElementById('login-page').classList.remove('hidden');
  });
} else {
  document.getElementById('login-page').classList.remove('hidden');
}
