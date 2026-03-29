const { Router } = require('express');

function configRouter(launcher) {
  const router = Router();

  // GET /api/config/accounts — character names only (no credentials)
  router.get('/accounts', (req, res) => {
    res.json(launcher.getCharacterNames());
  });

  // GET /api/config/hero-configs
  router.get('/hero-configs', (req, res) => {
    res.json(launcher.getHeroConfigs());
  });

  // GET /api/config/scripts — available AutoIt scripts
  router.get('/scripts', (req, res) => {
    res.json(launcher.getScripts());
  });

  return router;
}

module.exports = configRouter;
