const { spawn, execSync } = require('child_process');
const path = require('path');

class BotLauncher {
  constructor(autoitExe, autoitDir) {
    this.autoitExe = autoitExe;
    this.autoitDir = path.resolve(autoitDir);
  }

  /** Launch a bot instance in headless mode */
  launch(characterName, script = 'Froggy_HM_v1.6.au3') {
    const scriptPath = path.join(this.autoitDir, script);
    const proc = spawn(this.autoitExe, [scriptPath, '-autolaunch', characterName], {
      cwd: this.autoitDir,
      detached: true,
      stdio: 'ignore',
    });
    proc.unref();
    return { pid: proc.pid, character: characterName, script };
  }

  /** Kill a bot by its AutoIt PID */
  kill(pid) {
    try {
      execSync(`taskkill /F /PID ${pid}`, { stdio: 'ignore' });
      return true;
    } catch {
      return false;
    }
  }

  /** Kill a GW client by PID */
  killGw(pid) {
    try {
      execSync(`taskkill /F /PID ${pid}`, { stdio: 'ignore' });
      return true;
    } catch {
      return false;
    }
  }

  /** Get available scripts */
  getScripts() {
    const fs = require('fs');
    const files = fs.readdirSync(this.autoitDir);
    return files.filter(f => f.endsWith('.au3') && !f.startsWith('test_'));
  }

  /** Get available hero configs */
  getHeroConfigs() {
    const fs = require('fs');
    const configDir = path.join(this.autoitDir, 'hero_configs');
    if (!fs.existsSync(configDir)) return [];
    return fs.readdirSync(configDir)
      .filter(f => f.endsWith('.txt'))
      .map(f => f.replace('.txt', ''));
  }

  /** Get character names from Accounts.json (no credentials) */
  getCharacterNames() {
    const fs = require('fs');
    const accountsFile = path.join(this.autoitDir, 'Accounts.json');
    try {
      const accounts = JSON.parse(fs.readFileSync(accountsFile, 'utf8'));
      return accounts.map(a => a.character || a.name).filter(Boolean);
    } catch {
      return [];
    }
  }
}

module.exports = BotLauncher;
