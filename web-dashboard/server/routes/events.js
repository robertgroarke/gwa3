const { Router } = require('express');

function eventsRouter(reader) {
  const router = Router();

  // GET /api/events — Server-Sent Events stream
  router.get('/', (req, res) => {
    res.writeHead(200, {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache',
      Connection: 'keep-alive',
    });

    // Send keepalive comment every 15s to prevent timeout
    const keepalive = setInterval(() => res.write(': keepalive\n\n'), 15000);
    res.on('close', () => clearInterval(keepalive));

    reader.addListener(res);
  });

  return router;
}

module.exports = eventsRouter;
