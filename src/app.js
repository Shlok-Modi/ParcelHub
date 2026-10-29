import fs from 'node:fs/promises';
import path from 'node:path';
import cors from 'cors';
import express from 'express';
import rateLimit from 'express-rate-limit';
import { ApiError, BusyError, ValidationError } from './errors.js';
import { createRunner, RunnerError } from './runner.js';
import { SAMPLE } from './sample.js';
import { HEAPS, validateBench, validateDispatch, validateMst, validateRoute } from './validate.js';

const errorBody = (code, message) => ({ error: { code, message } });

// Runs the C++ program and turns its answer or failure into a JSON result or an ApiError.
async function execute(runner, input, { busyStatus = 503, busyMessage = 'The server is busy, try again shortly.' } = {}) {
  let result;
  try {
    result = await runner.run(input);
  } catch (err) {
    if (err instanceof BusyError) throw new ApiError(busyStatus, 'busy', busyMessage);
    if (err instanceof RunnerError) {
      console.error(`[runner] ${err.kind}: ${err.message}`);
      if (err.kind === 'timeout') throw new ApiError(504, 'timeout', 'The calculation took too long and was stopped.');
      if (err.kind === 'output_limit') throw new ApiError(502, 'output_too_large', 'The calculation produced too much output and was stopped.');
      throw new ApiError(503, 'unavailable', 'The calculation program is not available.');
    }
    throw err;
  }
  let parsed = null;
  try { parsed = JSON.parse(result.stdout); } catch { /* handled below */ }
  if (result.code === 0 && parsed && parsed.ok === true) return parsed;
  if (result.code === 2 && parsed && parsed.ok === false && typeof parsed.error === 'string') throw new ValidationError(parsed.error);
  // Never forward the program's stderr or raw output to the client.
  console.error(`[runner] unexpected result: exit ${result.code}, signal ${result.signal}, stderr: ${result.stderr.slice(0, 500)}`);
  throw new ApiError(500, 'calculation_failed', 'The calculation failed.');
}

// Same request on all three heaps; the answers must agree (only the heap name and its counters differ).
function stripHeapSpecific({ heap, heapOps, ...rest }) { return rest; }

async function runOnHeaps(runner, validated) {
  if (validated.heap !== 'all') return execute(runner, validated.input(validated.heap));
  const results = await Promise.all(HEAPS.map((h) => execute(runner, validated.input(h))));
  const first = JSON.stringify(stripHeapSpecific(results[0]));
  const identical = results.every((r) => JSON.stringify(stripHeapSpecific(r)) === first);
  return { ok: true, heap: 'all', identical, results };
}

export function createApp({ config, runner, benchRunner }) {
  const calc = runner ?? createRunner({
    command: config.apiBinary, timeoutMs: config.calcTimeoutMs, maxOutputBytes: config.maxOutputBytes,
    maxConcurrent: config.calcMaxConcurrent, maxQueue: config.calcMaxQueue,
  });
  const bench = benchRunner ?? createRunner({
    command: config.apiBinary, timeoutMs: config.benchTimeoutMs, maxOutputBytes: config.maxBenchOutputBytes,
    maxConcurrent: 1, maxQueue: 0,
  });

  const app = express();
  app.disable('x-powered-by');
  app.set('etag', false);

  app.use((req, res, next) => {
    res.set({ 'X-Content-Type-Options': 'nosniff', 'Cache-Control': 'no-store', 'Referrer-Policy': 'no-referrer' });
    next();
  });
  app.use(cors({
    origin(origin, cb) {
      // No Origin header = not a browser cross-site request (curl, server-to-server): allow.
      if (!origin || config.corsOrigins.includes(origin)) return cb(null, true);
      return cb(null, false); // no CORS headers: the browser blocks the response
    },
    methods: ['GET', 'POST'],
    maxAge: 600,
  }));

  const limiter = (windowMs, max) => rateLimit({
    windowMs, limit: max, standardHeaders: 'draft-7', legacyHeaders: false,
    handler: (req, res) => res.status(429).json(errorBody('rate_limited', 'Too many requests, slow down.')),
  });
  app.use('/api', limiter(config.rateLimitWindowMs, config.rateLimitMax));

  app.use(express.json({ limit: config.bodyLimit, strict: true, type: 'application/json' }));

  const requireJsonBody = (req, res, next) => {
    if (!req.is('application/json')) {
      return res.status(415).json(errorBody('unsupported_media_type', 'Send the request as application/json.'));
    }
    next();
  };

  const endpoints = [
    'GET /api/health', 'GET /api/sample', 'POST /api/route', 'POST /api/mst', 'POST /api/dispatch',
    'GET /api/benchmark/results', 'POST /api/benchmark/run',
  ];
  app.get('/', (req, res) => res.json({ service: 'ParcelHub API', endpoints, docs: 'see README.md, section "Backend API"' }));

  app.get('/api/health', async (req, res) => {
    let binary = false;
    try { await fs.access(config.apiBinary, fs.constants.X_OK); binary = true; } catch { /* reported below */ }
    res.status(binary ? 200 : 503).json({ ok: binary, binary, heaps: HEAPS });
  });

  app.get('/api/sample', (req, res) => res.json(SAMPLE));

  app.post('/api/route', requireJsonBody, async (req, res) => {
    res.json(await runOnHeaps(calc, validateRoute(req.body)));
  });
  app.post('/api/mst', requireJsonBody, async (req, res) => {
    res.json(await runOnHeaps(calc, validateMst(req.body)));
  });
  app.post('/api/dispatch', requireJsonBody, async (req, res) => {
    res.json(await runOnHeaps(calc, validateDispatch(req.body)));
  });

  // Results written by `parcelhub_bench` (Phase 4); read-only.
  app.get('/api/benchmark/results', async (req, res) => {
    const csv = req.query.format === 'csv';
    if (req.query.format !== undefined && !csv && req.query.format !== 'json') {
      throw new ValidationError('format must be json or csv');
    }
    const file = path.join(config.resultsDir, csv ? 'benchmark_results.csv' : 'benchmark_results.json');
    let data;
    try { data = await fs.readFile(file, 'utf8'); }
    catch (err) {
      if (err.code === 'ENOENT') throw new ApiError(404, 'no_results', 'No benchmark results yet. Run parcelhub_bench first.');
      throw err;
    }
    res.type(csv ? 'text/csv' : 'application/json').send(data);
  });

  // Runs a (small) benchmark on demand. One at a time; the result is returned, never saved over the official results.
  app.post('/api/benchmark/run', limiter(config.benchRateLimitWindowMs, config.benchRateLimitMax), requireJsonBody, async (req, res) => {
    const { input } = validateBench(req.body);
    const out = await execute(bench, input, { busyStatus: 409, busyMessage: 'A benchmark is already running.' });
    res.json(out.report);
  });

  app.use('/api', (req, res) => res.status(404).json(errorBody('not_found', 'No such endpoint.')));
  app.use((req, res) => res.status(404).json(errorBody('not_found', 'No such endpoint.')));

  // eslint-disable-next-line no-unused-vars
  app.use((err, req, res, next) => {
    if (err instanceof ApiError) return res.status(err.status).json(errorBody(err.code, err.message));
    if (err.type === 'entity.too.large') return res.status(413).json(errorBody('payload_too_large', 'The request body is too large.'));
    if (err.type === 'entity.parse.failed' || err instanceof SyntaxError) return res.status(400).json(errorBody('invalid_json', 'The request body is not valid JSON.'));
    if (err.type === 'encoding.unsupported' || err.type === 'charset.unsupported') return res.status(415).json(errorBody('unsupported_media_type', 'Unsupported encoding.'));
    console.error('[error]', err);
    return res.status(500).json(errorBody('internal_error', 'Something went wrong.'));
  });

  return app;
}
