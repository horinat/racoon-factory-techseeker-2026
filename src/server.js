const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");
const { Pool } = require("pg");

const port = Number(process.env.PORT || 3000);
const apiKey = process.env.API_KEY;
const databaseUrl = process.env.DATABASE_URL;
const publicDir = path.join(__dirname, "..", "public");
const pool = new Pool({ connectionString: databaseUrl });

if (!apiKey || !databaseUrl) {
  console.error("API_KEY and DATABASE_URL are required.");
  process.exit(1);
}

const rateLimits = new Map();

function sendJson(res, status, body) {
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
  });
  res.end(JSON.stringify(body));
}

function sendFile(res, file, contentType) {
  fs.readFile(path.join(publicDir, file), (error, data) => {
    if (error) return sendJson(res, 500, { error: "file_read_failed" });
    res.writeHead(200, {
      "Content-Type": contentType,
      "Cache-Control": file === "index.html" ? "no-cache" : "public, max-age=3600",
    });
    res.end(data);
  });
}

function isRateLimited(req) {
  const ip = String(req.headers["x-forwarded-for"] || req.socket.remoteAddress)
    .split(",")[0]
    .trim();
  const now = Date.now();
  const recent = (rateLimits.get(ip) || []).filter((time) => now - time < 60_000);
  recent.push(now);
  rateLimits.set(ip, recent);
  return recent.length > 120;
}

function readJson(req) {
  return new Promise((resolve, reject) => {
    let body = "";
    req.on("data", (chunk) => {
      body += chunk;
      if (body.length > 16_384) req.destroy();
    });
    req.on("end", () => {
      try {
        resolve(JSON.parse(body || "{}"));
      } catch {
        reject(new Error("invalid_json"));
      }
    });
    req.on("error", reject);
  });
}

function validText(value, min, max) {
  return typeof value === "string" && value.trim().length >= min && value.trim().length <= max;
}

async function initializeDatabase() {
  await pool.query(`
    CREATE TABLE IF NOT EXISTS scores (
      id BIGSERIAL PRIMARY KEY,
      game_id VARCHAR(80) NOT NULL,
      device_id VARCHAR(80) NOT NULL,
      player_name VARCHAR(40) NOT NULL,
      score INTEGER NOT NULL CHECK (score >= 0 AND score <= 100000),
      duration_seconds INTEGER NOT NULL DEFAULT 30 CHECK (duration_seconds BETWEEN 1 AND 300),
      started_at TIMESTAMPTZ NOT NULL,
      received_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
      UNIQUE (game_id, device_id)
    );
    CREATE INDEX IF NOT EXISTS scores_score_idx ON scores (score DESC, received_at ASC);
    CREATE INDEX IF NOT EXISTS scores_received_at_idx ON scores (received_at DESC);
  `);
}

async function createScore(req, res) {
  if (req.headers.authorization !== `Bearer ${apiKey}`) {
    return sendJson(res, 401, { error: "unauthorized" });
  }

  let body;
  try {
    body = await readJson(req);
  } catch {
    return sendJson(res, 400, { error: "invalid_json" });
  }

  const score = Number(body.score);
  const duration = Number(body.duration_seconds ?? 30);
  const startedAt = new Date(body.started_at);

  if (
    !validText(body.game_id, 1, 80) ||
    !validText(body.device_id, 1, 80) ||
    !validText(body.player_name, 1, 40) ||
    !Number.isInteger(score) ||
    score < 0 ||
    score > 100000 ||
    !Number.isInteger(duration) ||
    duration < 1 ||
    duration > 300 ||
    Number.isNaN(startedAt.getTime())
  ) {
    return sendJson(res, 422, { error: "invalid_score_data" });
  }

  try {
    const result = await pool.query(
      `INSERT INTO scores
       (game_id, device_id, player_name, score, duration_seconds, started_at)
       VALUES ($1, $2, $3, $4, $5, $6)
       ON CONFLICT (game_id, device_id) DO NOTHING
       RETURNING id, received_at`,
      [
        body.game_id.trim(),
        body.device_id.trim(),
        body.player_name.trim(),
        score,
        duration,
        startedAt.toISOString(),
      ],
    );

    if (result.rowCount === 0) {
      return sendJson(res, 409, { error: "duplicate_game" });
    }
    return sendJson(res, 201, { ok: true, ...result.rows[0] });
  } catch (error) {
    console.error(error);
    return sendJson(res, 500, { error: "database_error" });
  }
}

async function getRankings(url, res) {
  const period = url.searchParams.get("period") || "all";
  const allowedPeriods = {
    all: "TRUE",
    today: "received_at >= date_trunc('day', NOW() AT TIME ZONE 'Asia/Tokyo') AT TIME ZONE 'Asia/Tokyo'",
    week: "received_at >= NOW() - INTERVAL '7 days'",
  };
  if (!allowedPeriods[period]) return sendJson(res, 400, { error: "invalid_period" });

  try {
    const result = await pool.query(
      `SELECT player_name, score, device_id, received_at
       FROM scores
       WHERE ${allowedPeriods[period]}
       ORDER BY score DESC, received_at ASC
       LIMIT 100`,
    );
    return sendJson(res, 200, { period, rankings: result.rows });
  } catch (error) {
    console.error(error);
    return sendJson(res, 500, { error: "database_error" });
  }
}

const server = http.createServer(async (req, res) => {
  if (isRateLimited(req)) return sendJson(res, 429, { error: "rate_limited" });

  const url = new URL(req.url, `http://${req.headers.host || "localhost"}`);
  if (req.method === "GET" && url.pathname === "/api/health") {
    try {
      await pool.query("SELECT 1");
      return sendJson(res, 200, { ok: true });
    } catch {
      return sendJson(res, 503, { ok: false });
    }
  }
  if (req.method === "POST" && url.pathname === "/api/scores") return createScore(req, res);
  if (req.method === "GET" && url.pathname === "/api/rankings") return getRankings(url, res);
  if (req.method === "GET" && url.pathname === "/") {
    return sendFile(res, "index.html", "text/html; charset=utf-8");
  }
  if (req.method === "GET" && url.pathname === "/app.js") {
    return sendFile(res, "app.js", "text/javascript; charset=utf-8");
  }
  if (req.method === "GET" && url.pathname === "/style.css") {
    return sendFile(res, "style.css", "text/css; charset=utf-8");
  }
  return sendJson(res, 404, { error: "not_found" });
});

initializeDatabase()
  .then(() => {
    server.listen(port, "0.0.0.0", () => console.log(`Score server listening on ${port}`));
  })
  .catch((error) => {
    console.error("Database initialization failed:", error);
    process.exit(1);
  });

function shutdown() {
  server.close(() => pool.end().finally(() => process.exit(0)));
}

process.on("SIGTERM", shutdown);
process.on("SIGINT", shutdown);
