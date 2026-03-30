const { Router } = require('express');

function botsRouter(reader, writer, launcher) {
  const router = Router();

  // GET /api/bots — all bot statuses
  router.get('/', (req, res) => {
    res.json(reader.readAll());
  });

  // GET /api/bots/:id — single bot status
  router.get('/:id', (req, res) => {
    const status = reader.readBot(req.params.id);
    if (!status) return res.status(404).json({ error: 'Bot not found' });
    res.json(status);
  });

  // GET /api/bots/:id/log — last 200 log lines
  router.get('/:id/log', (req, res) => {
    const status = reader.readBot(req.params.id);
    if (!status) return res.status(404).json({ error: 'Bot not found' });
    res.json(status.log || []);
  });

  // POST /api/bots/:id/command — send command to bot
  router.post('/:id/command', (req, res) => {
    const { action, settings } = req.body;
    if (!action) return res.status(400).json({ error: 'Missing action' });

    const extra = {};
    if (action === 'update_settings' && settings) extra.settings = settings;

    const cmd = writer.sendCommand(req.params.id, action, extra);

    // For kill: also force-kill the process since the bot may not read command.json in time
    if (action === 'kill') {
      const status = reader.readBot(req.params.id);
      if (status && status.pid) {
        setTimeout(() => launcher.kill(status.pid), 2000); // Give IPC 2s, then force kill
      }
    }

    res.json({ ok: true, command: cmd });
  });

  // POST /api/bots/launch — launch a new bot
  router.post('/launch', (req, res) => {
    const { character, script } = req.body;
    if (!character) return res.status(400).json({ error: 'Missing character' });

    const result = launcher.launch(character, script);
    res.json({ ok: true, ...result });
  });

  // POST /api/bots/:id/kill — kill bot process
  router.post('/:id/kill', (req, res) => {
    const status = reader.readBot(req.params.id);
    if (!status) return res.status(404).json({ error: 'Bot not found' });

    // Send kill command via IPC (graceful)
    writer.sendCommand(req.params.id, 'kill');

    // Also force-kill if PID is known
    if (status.pid) launcher.kill(status.pid);

    res.json({ ok: true });
  });

  return router;
}

module.exports = botsRouter;
