const { test, expect } = require('@playwright/test');
const { spawn } = require('child_process');
const http = require('http');
const path = require('path');

const AUTH_TOKEN = 'pw-token';

function listen(server) {
  return new Promise(resolve => {
    server.listen(0, '127.0.0.1', () => resolve(server.address().port));
  });
}

function waitForDashboard(port) {
  return new Promise((resolve, reject) => {
    const deadline = Date.now() + 5000;
    const check = () => {
      http.get(`http://127.0.0.1:${port}/api/health?token=${AUTH_TOKEN}`, res => {
        res.resume();
        if (res.statusCode === 200) resolve();
        else retry();
      }).on('error', retry);
    };
    const retry = () => {
      if (Date.now() > deadline) reject(new Error('dashboard did not start'));
      else setTimeout(check, 100);
    };
    check();
  });
}

function createBridgeStub() {
  const clients = new Set();
  const emit = (event, data) => {
    for (const res of clients) {
      res.write(`event: ${event}\n`);
      res.write(`data: ${JSON.stringify(data)}\n\n`);
    }
  };
  const server = http.createServer((req, res) => {
    if (req.url === '/api/llm/state') {
      res.setHeader('content-type', 'application/json');
      res.end(JSON.stringify({
        bridge: { status: 'stopped', lane: 'beastrit', observers: 0 },
        profile: {
          name: 'qwen-safe',
          supervisor_mode: 'deterministic',
          executor_mode: 'health-check',
          planner_mode: 'async',
          prompt_mode: 'delta',
          planner_model: 'qwen3.5:cloud',
          executor_model: 'qwen3.5:cloud',
        },
        plan: { phase: 'idle', intent: 'wait', next_step: 'wait', deviation: '-' },
        last_tool_calls: [],
        run_summaries: [],
      }));
      return;
    }
    if (req.url === '/api/llm/stream') {
      res.writeHead(200, {
        'content-type': 'text/event-stream',
        'cache-control': 'no-cache',
        connection: 'keep-alive',
      });
      clients.add(res);
      req.on('close', () => clients.delete(res));
      return;
    }

    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      res.setHeader('content-type', 'application/json');
      if (req.url === '/api/llm/launch') {
        emit('bridge.status', {
          status: 'connected',
          lane: 'beastrit',
          observers: 2,
          profile: {
            name: 'qwen-safe',
            supervisor_mode: 'deterministic',
            executor_mode: 'health-check',
            planner_mode: 'async',
            prompt_mode: 'delta',
            planner_model: 'qwen3.5:cloud',
          },
        });
        res.end(JSON.stringify({ ok: true }));
      } else if (req.url === '/api/llm/chat') {
        const message = JSON.parse(body || '{}').message || '';
        emit('chat.user', { message });
        emit('tool.call', { role: 'planner', name: 'set_bot_state', args: { state: 'llm_controlled' }, request_id: 'req-1' });
        emit('tool.result', { request_id: 'req-1', success: true, error: null });
        emit('chat.assistant', { message: `ack: ${message}` });
        res.end(JSON.stringify({ ok: true }));
      } else if (req.url === '/api/llm/stop') {
        emit('bridge.status', { status: 'stopped', lane: 'beastrit' });
        res.end(JSON.stringify({ ok: true }));
      } else {
        res.statusCode = 404;
        res.end(JSON.stringify({ error: 'not_found' }));
      }
    });
  });
  return { server, emit };
}

test('LLM panel proxies launch, chat, stream, and stop', async ({ page }) => {
  const bridge = createBridgeStub();
  const bridgePort = await listen(bridge.server);
  const dashboardPort = 3862;
  const dashboard = spawn(process.execPath, ['server/index.js'], {
    cwd: path.resolve(__dirname, '..'),
    env: {
      ...process.env,
      PORT: String(dashboardPort),
      AUTH_TOKEN,
      LLM_BRIDGE_URL: `http://127.0.0.1:${bridgePort}`,
    },
    stdio: 'ignore',
  });

  try {
    await waitForDashboard(dashboardPort);
    await page.goto(`http://127.0.0.1:${dashboardPort}`);
    await page.fill('#token-input', AUTH_TOKEN);
    await page.getByText('Login').click();
    await expect(page.locator('.llm-workspace')).toContainText('Lane Launcher');
    await expect(page.locator('.llm-workspace')).toContainText('Live Chat Panel');
    await expect(page.locator('.llm-workspace')).toContainText('Plan + State Panel');
    await expect(page.locator('.llm-workspace')).toContainText('Run History Panel');
    await expect(page.locator('#llm-profile-name')).toHaveText('qwen-safe');
    await expect(page.locator('#llm-supervisor-mode')).toHaveText('deterministic');
    await expect(page.locator('#llm-executor-mode')).toHaveText('health-check');

    await page.locator('#llm-launch-btn').click();
    await expect(page.locator('#llm-status')).toHaveText(/connected/i);
    await expect(page.locator('#llm-observers')).toHaveText(/2 observers connected/i);

    await page.locator('#llm-chat-input').fill('status check');
    await page.locator('.chat-form button[type="submit"]').click();
    await expect(page.locator('#llm-chat-log')).toContainText('ack: status check');
    await expect(page.locator('#llm-chat-log')).toContainText('[planner -> set_bot_state]');
    await expect(page.locator('#llm-tool-list')).toContainText('req-1');

    await page.locator('#llm-stop-btn').click();
    await expect(page.locator('#llm-status')).toHaveText(/stopped/i);
  } finally {
    dashboard.kill();
    bridge.server.close();
  }
});
