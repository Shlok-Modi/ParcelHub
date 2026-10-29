import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const projectRoot = path.resolve(here, '..', '..');

// Limits on what the API accepts. They are deliberately lower than the C++ program's own
// hard caps, so a request that passes here can never trip those caps.
export const LIMITS = Object.freeze({
  maxHubs: 200,
  maxRoads: 2000,
  maxWeight: 1_000_000,
  maxParcels: 500,
  maxUpgrades: 500,
  bench: Object.freeze({
    maxGraphSize: 2000,
    maxParcelSize: 20000,
    maxRepeats: 5,
    maxWarmups: 2,
    maxInstances: 2,
    maxListLength: 5,
  }),
});

function intFromEnv(env, name, fallback, min, max) {
  const raw = env[name];
  if (raw === undefined || raw === '') return fallback;
  const n = Number(raw);
  if (!Number.isInteger(n) || n < min || n > max) {
    throw new Error(`${name} must be a whole number from ${min} to ${max}`);
  }
  return n;
}

export function defaultBinaryPath() {
  const exe = process.platform === 'win32' ? 'parcelhub_api.exe' : 'parcelhub_api';
  return path.join(projectRoot, 'build', exe);
}

export function loadConfig(env = process.env) {
  return {
    host: env.HOST || '127.0.0.1', // loopback only unless the operator opts in
    port: intFromEnv(env, 'PORT', 3001, 0, 65535),
    apiBinary: env.PARCELHUB_API_BIN || defaultBinaryPath(),
    resultsDir: env.PARCELHUB_RESULTS_DIR || path.join(projectRoot, 'results'),
    corsOrigins: (env.CORS_ORIGINS || 'http://localhost:5173,http://localhost:3000')
      .split(',')
      .map((s) => s.trim())
      .filter(Boolean),
    bodyLimit: '200kb',
    // Runner limits for the normal calculation endpoints.
    calcTimeoutMs: intFromEnv(env, 'CALC_TIMEOUT_MS', 10_000, 100, 120_000),
    calcMaxConcurrent: intFromEnv(env, 'CALC_MAX_CONCURRENT', 4, 1, 64),
    calcMaxQueue: intFromEnv(env, 'CALC_MAX_QUEUE', 64, 0, 1000),
    maxOutputBytes: intFromEnv(env, 'MAX_OUTPUT_BYTES', 2_000_000, 1000, 50_000_000),
    // Benchmarks are heavy: one at a time, longer timeout, tighter rate limit.
    benchTimeoutMs: intFromEnv(env, 'BENCH_TIMEOUT_MS', 120_000, 1000, 600_000),
    maxBenchOutputBytes: intFromEnv(env, 'MAX_BENCH_OUTPUT_BYTES', 10_000_000, 1000, 100_000_000),
    // Rate limits (requests per window, per client address).
    rateLimitWindowMs: intFromEnv(env, 'RATE_LIMIT_WINDOW_MS', 60_000, 1000, 3_600_000),
    rateLimitMax: intFromEnv(env, 'RATE_LIMIT_MAX', 120, 1, 100_000),
    benchRateLimitWindowMs: intFromEnv(env, 'BENCH_RATE_LIMIT_WINDOW_MS', 600_000, 1000, 86_400_000),
    benchRateLimitMax: intFromEnv(env, 'BENCH_RATE_LIMIT_MAX', 3, 1, 10_000),
  };
}
