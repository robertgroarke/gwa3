const fs = require('fs');
const path = require('path');

class IPCWriter {
  constructor(ipcDir) {
    this.ipcDir = path.resolve(ipcDir);
  }

  /** Write a command.json for a bot to pick up */
  sendCommand(botId, action, extra = {}) {
    const cmdDir = path.join(this.ipcDir, botId);
    if (!fs.existsSync(cmdDir)) {
      fs.mkdirSync(cmdDir, { recursive: true });
    }

    const command = {
      action,
      timestamp: Math.floor(Date.now() / 1000),
      ...extra,
    };

    const cmdFile = path.join(cmdDir, 'command.json');
    fs.writeFileSync(cmdFile, JSON.stringify(command, null, 2));
    return command;
  }
}

module.exports = IPCWriter;
