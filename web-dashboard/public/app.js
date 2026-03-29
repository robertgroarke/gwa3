const API = '';
let token = localStorage.getItem('gwa-token');
let selectedBot = null;
let eventSource = null;

// Map IDs to human-readable names
const MAP_NAMES = {
  0: 'Unknown', 638: "Gadd's Encampment", 495: 'Sparkfly Swamp',
  857: 'Embark Beach', 558: 'Bogroot Growths Lv1', 559: 'Bogroot Growths Lv2',
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
  loadCharacters();
  connectSSE();
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

function renderBots(bots) {
  const grid = document.getElementById('bot-grid');

  // Filter out test entries
  const realBots = bots.filter(b => b.character && b._id !== 'testbot');

  grid.innerHTML = realBots.map(bot => {
    const state = bot.state || 'offline';
    const mapName = MAP_NAMES[bot.map_id] || `Map ${bot.map_id}`;
    const gold = bot.gold || {};
    const stats = bot.stats || {};
    const settings = bot.settings || {};
    const logLines = (bot.log || []).join('\n');

    return `
    <div class="bot-card ${selectedBot === bot._id ? 'selected' : ''}">
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
        ${bot.bot_running
          ? `<button onclick="sendCommand('${bot._id}', 'stop')">Stop</button>`
          : `<button class="btn-success" onclick="sendCommand('${bot._id}', 'start')">Start</button>`
        }
        <button class="btn-danger" onclick="sendCommand('${bot._id}', 'kill')">Kill</button>
      </div>
    </div>
    <div class="bot-log" id="log-${bot._id}"><pre>${escapeHtml(logLines)}</pre></div>`;
  }).join('');

  // Auto-scroll all log boxes to bottom
  for (const el of document.querySelectorAll('.bot-log pre')) {
    el.scrollTop = el.scrollHeight;
  }

  // Aggregate stats
  const totalRuns = realBots.reduce((s, b) => s + ((b.stats || {}).run_count || 0), 0);
  const online = realBots.filter(b => b.state !== 'offline' && b.state !== 'stale').length;
  document.getElementById('agg-runs').textContent = totalRuns;
  document.getElementById('agg-online').textContent = `${online}/${realBots.length}`;
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
  const chars = await res.json();
  const sel = document.getElementById('launch-character');
  sel.innerHTML = chars.map(c => `<option value="${c}">${c}</option>`).join('');
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
