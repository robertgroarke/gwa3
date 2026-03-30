const fs = require('fs');
const path = require('path');

class IPCReader {
  constructor(ipcDir) {
    this.ipcDir = path.resolve(ipcDir);
    this.cache = new Map(); // character -> last status JSON
    this.listeners = new Set(); // SSE clients
  }

  /** Read all bot status files and return array of status objects */
  readAll() {
    const statuses = [];
    if (!fs.existsSync(this.ipcDir)) return statuses;

    for (const dir of fs.readdirSync(this.ipcDir, { withFileTypes: true })) {
      if (!dir.isDirectory()) continue;
      const status = this.readBot(dir.name);
      if (status) statuses.push(status);
    }
    return statuses;
  }

  /** Read a single bot's status.json */
  readBot(botId) {
    const statusFile = path.join(this.ipcDir, botId, 'status.json');
    try {
      const stat = fs.statSync(statusFile);
      const raw = fs.readFileSync(statusFile, 'utf8');
      const status = JSON.parse(raw);
      status._id = botId;

      // Mark as stale if FILE was last modified more than 15 seconds ago
      const fileAgeSec = (Date.now() - stat.mtimeMs) / 1000;
      if (fileAgeSec > 15) {
        status._stale = true;
        if (status.state !== 'offline') status.state = 'stale';
      }

      return status;
    } catch {
      return null;
    }
  }

  /** Start polling for changes and notify SSE listeners */
  startPolling(intervalMs = 2000) {
    setInterval(() => {
      const statuses = this.readAll();
      const json = JSON.stringify(statuses);
      const cached = this.cache.get('_all');

      if (json !== cached) {
        this.cache.set('_all', json);
        this.notify(statuses);
      }
    }, intervalMs);
  }

  /** Register an SSE client */
  addListener(res) {
    this.listeners.add(res);
    res.on('close', () => this.listeners.delete(res));

    // Send current state immediately
    const statuses = this.readAll();
    res.write(`data: ${JSON.stringify(statuses)}\n\n`);
  }

  /** Push update to all SSE clients */
  notify(statuses) {
    const data = `data: ${JSON.stringify(statuses)}\n\n`;
    for (const res of this.listeners) {
      try { res.write(data); } catch { this.listeners.delete(res); }
    }
  }
}

module.exports = IPCReader;
