const express = require('express');
const path = require('path');
const fs = require('fs');

// Load .env manually (no dotenv dependency)
const envFile = path.join(__dirname, '..', '.env');
if (fs.existsSync(envFile)) {
  for (const line of fs.readFileSync(envFile, 'utf8').split('\n')) {
    const match = line.match(/^([^#=]+)=(.*)$/);
    if (match) {
      const key = match[1].trim();
      if (process.env[key] === undefined) process.env[key] = match[2].trim();
    }
  }
}

const IPCReader = require('./ipc/reader');
const IPCWriter = require('./ipc/writer');
const BotLauncher = require('./ipc/launcher');
const authMiddleware = require('./auth');
const botsRouter = require('./routes/bots');
const configRouter = require('./routes/config');
const eventsRouter = require('./routes/events');
const llmRouter = require('./routes/llm');

const PORT = process.env.PORT || 3847;
const AUTH_TOKEN = process.env.AUTH_TOKEN || 'changeme';
const IPC_DIR = path.resolve(__dirname, '..', process.env.IPC_DIR || '../GWA Censured/ipc');
const AUTOIT_DIR = path.resolve(__dirname, '..', process.env.AUTOIT_DIR || '../GWA Censured');
const AUTOIT_EXE = process.env.AUTOIT_EXE || 'C:/Program Files (x86)/AutoIt3/AutoIt3.exe';

// Initialize components
const reader = new IPCReader(IPC_DIR);
const writer = new IPCWriter(IPC_DIR);
const launcher = new BotLauncher(AUTOIT_EXE, AUTOIT_DIR);

const app = express();
app.use(express.json());
app.use(authMiddleware(AUTH_TOKEN));

// API routes
app.post('/api/auth/login', (req, res) => {
  const { token } = req.body;
  if (token === AUTH_TOKEN) {
    res.json({ ok: true });
  } else {
    res.status(401).json({ error: 'Invalid token' });
  }
});

app.get('/api/health', (req, res) => {
  const bots = reader.readAll();
  res.json({
    status: 'ok',
    bots_online: bots.filter(b => b.state !== 'offline' && !b._stale).length,
    bots_total: bots.length,
  });
});

app.use('/api/bots', botsRouter(reader, writer, launcher));
app.use('/api/config', configRouter(launcher));
app.use('/api/events', eventsRouter(reader));
app.use('/api/llm', llmRouter());

// Serve static frontend
app.use(express.static(path.join(__dirname, '..', 'public')));

// Start polling and server
reader.startPolling(2000);

app.listen(PORT, () => {
  console.log(`GWA Bot Dashboard running at http://localhost:${PORT}`);
  console.log(`IPC directory: ${IPC_DIR}`);
  console.log(`AutoIt directory: ${AUTOIT_DIR}`);
});
