const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..', '..');
const fixture = JSON.parse(fs.readFileSync(path.join(__dirname, 'wpf-fixture.json'), 'utf8'));
const html = fs.readFileSync(path.join(root, 'public', 'index.html'), 'utf8');

for (const panel of fixture.panels) {
  if (!html.includes(panel)) {
    throw new Error(`Missing web panel label from WPF fixture: ${panel}`);
  }
}

for (const value of Object.values(fixture.values)) {
  if (!html.toLowerCase().includes(String(value).toLowerCase())) {
    throw new Error(`Missing web value from WPF fixture: ${value}`);
  }
}

console.log('Desktop/web LLM label parity fixture passed.');
