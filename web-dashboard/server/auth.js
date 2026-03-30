/** Simple bearer token authentication middleware */
function authMiddleware(token) {
  return (req, res, next) => {
    // Skip auth for login, health, and static files
    if (req.path === '/api/auth/login' || req.path === '/api/health') return next();
    if (!req.path.startsWith('/api/')) return next();

    // Accept token from Authorization header OR query param (for SSE)
    const auth = req.headers.authorization;
    const queryToken = req.query.token;
    if (auth === `Bearer ${token}` || queryToken === token) return next();

    return res.status(401).json({ error: 'Unauthorized' });
  };
}

module.exports = authMiddleware;
