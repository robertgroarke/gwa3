/** Simple bearer token authentication middleware */
function authMiddleware(token) {
  return (req, res, next) => {
    // Skip auth for login, health, and static files
    if (req.path === '/api/auth/login' || req.path === '/api/health') return next();
    if (!req.path.startsWith('/api/')) return next();

    const auth = req.headers.authorization;
    if (!auth || auth !== `Bearer ${token}`) {
      return res.status(401).json({ error: 'Unauthorized' });
    }
    next();
  };
}

module.exports = authMiddleware;
