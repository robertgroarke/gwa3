const { Router } = require('express');
const http = require('http');

const BRIDGE_URL = new URL(process.env.LLM_BRIDGE_URL || 'http://127.0.0.1:8765');

function requestBridge(method, path, body, res) {
  const payload = body ? Buffer.from(JSON.stringify(body)) : null;
  const req = http.request({
    hostname: BRIDGE_URL.hostname,
    port: BRIDGE_URL.port || 80,
    path,
    method,
    headers: {
      'Content-Type': 'application/json',
      ...(payload ? { 'Content-Length': payload.length } : {}),
    },
  }, (bridgeRes) => {
    let data = '';
    bridgeRes.setEncoding('utf8');
    bridgeRes.on('data', chunk => { data += chunk; });
    bridgeRes.on('end', () => {
      res.status(bridgeRes.statusCode || 502);
      try {
        res.json(data ? JSON.parse(data) : {});
      } catch {
        res.type('text/plain').send(data);
      }
    });
  });
  req.on('error', (error) => {
    res.status(502).json({ error: 'bridge_unavailable', message: error.message });
  });
  if (payload) req.write(payload);
  req.end();
}

function llmRouter() {
  const router = Router();

  router.get('/state', (req, res) => requestBridge('GET', '/api/llm/state', null, res));
  router.post('/launch', (req, res) => requestBridge('POST', '/api/llm/launch', req.body, res));
  router.post('/chat', (req, res) => requestBridge('POST', '/api/llm/chat', req.body, res));
  router.post('/stop', (req, res) => requestBridge('POST', '/api/llm/stop', req.body, res));

  router.get('/stream', (req, res) => {
    const bridgeReq = http.request({
      hostname: BRIDGE_URL.hostname,
      port: BRIDGE_URL.port || 80,
      path: '/api/llm/stream',
      method: 'GET',
      headers: { Accept: 'text/event-stream' },
    }, (bridgeRes) => {
      res.writeHead(bridgeRes.statusCode || 200, {
        'Content-Type': 'text/event-stream',
        'Cache-Control': 'no-cache',
        Connection: 'keep-alive',
      });
      bridgeRes.pipe(res);
    });
    bridgeReq.on('error', (error) => {
      res.writeHead(502, { 'Content-Type': 'text/event-stream' });
      res.write(`event: bridge.status\ndata: ${JSON.stringify({ status: 'degraded', error: error.message })}\n\n`);
      res.end();
    });
    req.on('close', () => bridgeReq.destroy());
    bridgeReq.end();
  });

  return router;
}

module.exports = llmRouter;
