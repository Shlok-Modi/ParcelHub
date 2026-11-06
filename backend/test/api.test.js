import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { after, before, describe, test } from 'node:test';
import { createRunner } from '../src/runner.js';
import { SAMPLE } from '../src/sample.js';
import { lcg, post, randomGraph, refDistances, refMst, sampleGraph, startServer, tmpDir } from './helpers.js';

let s;
before(async () => { s = await startServer(); });
after(async () => { await s.close(); });

const get = async (p, headers = {}) => {
  const res = await fetch(s.url + p, { headers });
  const text = await res.text();
  let json = null; try { json = JSON.parse(text); } catch { /* not json */ }
  return { status: res.status, json, text, headers: res.headers };
};

describe('basics', () => {
  test('health reports the binary', async () => {
    const r = await get('/api/health');
    assert.equal(r.status, 200);
    assert.deepEqual(r.json, { ok: true, binary: true, heaps: ['binary', 'binomial', 'fibonacci'] });
  });

  test('sample data is the demo city and works as a request', async () => {
    const r = await get('/api/sample');
    assert.equal(r.status, 200);
    assert.equal(r.json.graph.nodes, 8);
    assert.equal(r.json.parcels.length, 6);
    const route = await post(`${s.url}/api/route`, { graph: r.json.graph, source: 1, destination: 7 });
    assert.equal(route.status, 200);
    const dispatch = await post(`${s.url}/api/dispatch`, { graph: r.json.graph, parcels: r.json.parcels, upgrades: r.json.upgrades });
    assert.equal(dispatch.status, 200);
  });

  test('security headers, no x-powered-by, no caching', async () => {
    const r = await get('/api/health');
    assert.equal(r.headers.get('x-powered-by'), null);
    assert.equal(r.headers.get('x-content-type-options'), 'nosniff');
    assert.equal(r.headers.get('cache-control'), 'no-store');
  });

  test('the root path lists the endpoints instead of a dead end', async () => {
    const r = await get('/');
    assert.equal(r.status, 200);
    assert.equal(r.json.service, 'ParcelHub API');
    assert.ok(r.json.endpoints.includes('POST /api/route'));
  });

  test('unknown routes are JSON 404s', async () => {
    for (const p of ['/api/nope', '/etc/passwd', '/api/../secret', '/index.html']) {
      const r = await get(p);
      assert.equal(r.status, 404, p);
      assert.equal(r.json.error.code, 'not_found');
    }
    const wrongMethod = await fetch(`${s.url}/api/route`);
    assert.equal(wrongMethod.status, 404);
  });
});

describe('route', () => {
  test('matches the C++ demo output (Phase 3)', async () => {
    const r = await post(`${s.url}/api/route`, { heap: 'fibonacci', graph: sampleGraph(), source: 1, destination: 7 });
    assert.equal(r.status, 200);
    assert.deepEqual(r.json.distances, [0, 7, 9, 20, 20, 11, 23, null]);
    assert.deepEqual(r.json.path, [1, 3, 6, 5, 7]);
    assert.equal(r.json.cost, 23);
    assert.equal(r.json.reachable, true);
    assert.equal(r.json.heap, 'Fibonacci Heap');
    assert.deepEqual(r.json.heapOps, { inserts: 7, extracts: 7, decreaseKeys: 2, merges: 0, comparisons: 10 });
  });

  test('unreachable destination', async () => {
    const r = await post(`${s.url}/api/route`, { graph: sampleGraph(), source: 1, destination: 8 });
    assert.equal(r.status, 200);
    assert.equal(r.json.reachable, false);
    assert.deepEqual(r.json.path, []);
    assert.equal(r.json.cost, null);
  });

  test('without a destination only distances are returned', async () => {
    const r = await post(`${s.url}/api/route`, { graph: sampleGraph(), source: 3 });
    assert.equal(r.json.destination, undefined);
    assert.deepEqual(r.json.distances, [9, 10, 0, 11, 11, 2, 14, null]);
  });

  test('single hub and no roads', async () => {
    const r = await post(`${s.url}/api/route`, { graph: { nodes: 1, edges: [] }, source: 1, destination: 1 });
    assert.equal(r.status, 200);
    assert.deepEqual(r.json.distances, [0]);
    assert.deepEqual(r.json.path, [1]);
    assert.equal(r.json.cost, 0);
  });

  test('heap "all" runs the three heaps and they agree', async () => {
    const r = await post(`${s.url}/api/route`, { heap: 'all', graph: sampleGraph(), source: 1, destination: 7 });
    assert.equal(r.status, 200);
    assert.equal(r.json.identical, true);
    assert.deepEqual(r.json.results.map((x) => x.heap), ['Binary Heap', 'Binomial Heap', 'Fibonacci Heap']);
    assert.deepEqual(r.json.results.map((x) => x.heapOps.comparisons), [7, 7, 10]);   // Phase 3 table
  });

  test('50 random graphs: distances equal an independent JavaScript Dijkstra on every heap', async () => {
    const rand = lcg(2024);
    for (let i = 0; i < 50; i++) {
      const n = 2 + Math.floor(rand() * 40);
      const graph = randomGraph(rand, n, Math.floor(rand() * n * 3));
      const source = 1 + Math.floor(rand() * n);
      const r = await post(`${s.url}/api/route`, { heap: 'all', graph, source });
      assert.equal(r.status, 200, JSON.stringify(r.json));
      assert.equal(r.json.identical, true);
      const expected = refDistances(graph, source);
      for (const res of r.json.results) assert.deepEqual(res.distances, expected, `case ${i} ${res.heap}`);
    }
  });
});

describe('mst', () => {
  test('matches the C++ demo: spanning forest of weight 36', async () => {
    const r = await post(`${s.url}/api/mst`, { heap: 'binomial', graph: sampleGraph(), start: 1 });
    assert.equal(r.status, 200);
    assert.equal(r.json.totalWeight, 36);
    assert.equal(r.json.components, 2);
    assert.equal(r.json.isSpanningTree, false);       // H8 is isolated: a forest, never labelled a tree
    assert.deepEqual(r.json.edges.map((e) => [e.from, e.to, e.weight]),
      [[1, 2, 7], [1, 3, 9], [3, 6, 2], [6, 5, 9], [5, 7, 3], [5, 4, 6]]);
  });

  test('connected graph is a spanning tree', async () => {
    const r = await post(`${s.url}/api/mst`, { graph: { nodes: 3, edges: [{ from: 1, to: 2, weight: 1 }, { from: 2, to: 3, weight: 2 }, { from: 1, to: 3, weight: 5 }] } });
    assert.equal(r.json.isSpanningTree, true);
    assert.equal(r.json.totalWeight, 3);
  });

  test('50 random graphs: weight and components equal an independent Kruskal on every heap', async () => {
    const rand = lcg(99);
    for (let i = 0; i < 50; i++) {
      const n = 1 + Math.floor(rand() * 40);
      const graph = randomGraph(rand, n, Math.floor(rand() * n * 3));
      const r = await post(`${s.url}/api/mst`, { heap: 'all', graph, start: 1 + Math.floor(rand() * n) });
      assert.equal(r.status, 200, JSON.stringify(r.json));
      const ref = refMst(graph);
      for (const res of r.json.results) {
        assert.equal(res.totalWeight, ref.total, `case ${i} ${res.heap}`);
        assert.equal(res.components, ref.components);
        assert.equal(res.edges.length, n - ref.components);
        assert.equal(res.isSpanningTree, ref.components === 1);
      }
    }
  });
});

describe('dispatch', () => {
  const body = (extra = {}) => ({ graph: SAMPLE.graph, parcels: SAMPLE.parcels, upgrades: SAMPLE.upgrades, ...extra });

  test('matches the C++ demo dispatch order and routes', async () => {
    const r = await post(`${s.url}/api/dispatch`, body({ heap: 'fibonacci' }));
    assert.equal(r.status, 200);
    assert.deepEqual(r.json.dispatches.map((d) => d.id), [102, 104, 106, 103, 101, 105]);
    const first = r.json.dispatches[0];
    assert.deepEqual([first.level, first.deadline, first.cost, first.path], ['Express', '14:30', 23, [1, 3, 6, 5, 7]]);
    const last = r.json.dispatches[5];
    assert.deepEqual([last.reachable, last.path, last.cost], [false, [], null]);
    assert.equal(r.json.dispatches[2].level, 'Express');      // P106 was upgraded
    assert.equal(r.json.heapOps.decreaseKeys, 1);
  });

  test('heap "all": identical dispatch order and routes on every heap', async () => {
    const r = await post(`${s.url}/api/dispatch`, body({ heap: 'all' }));
    assert.equal(r.status, 200);
    assert.equal(r.json.identical, true);
    assert.equal(r.json.results.length, 3);
  });

  test('no parcels is fine', async () => {
    const r = await post(`${s.url}/api/dispatch`, { graph: SAMPLE.graph, parcels: [] });
    assert.equal(r.status, 200);
    assert.deepEqual(r.json.dispatches, []);
  });

  test('ties are broken by arrival time, then id', async () => {
    const parcels = [3, 1, 2].map((id, i) => ({ id, source: 1, destination: 2, level: 'standard', deadline: '10:00', arrival: i === 0 ? 5 : 1 }));
    const r = await post(`${s.url}/api/dispatch`, { graph: SAMPLE.graph, parcels });
    assert.deepEqual(r.json.dispatches.map((d) => d.id), [1, 2, 3]);
  });

  test('parcel ids rejected by the validator never reach the program', async () => {
    const r = await post(`${s.url}/api/dispatch`, body({ parcels: [{ ...SAMPLE.parcels[0], id: '101; rm -rf /' }] }));
    assert.equal(r.status, 400);
  });
});

describe('invalid input', () => {
  const route = (b, opts) => post(`${s.url}/api/route`, b, opts);

  test('structured 400s with a useful message', async () => {
    const r = await route({ graph: sampleGraph(), source: 99 });
    assert.equal(r.status, 400);
    assert.equal(r.json.error.code, 'invalid_request');
    assert.match(r.json.error.message, /source must be a whole number from 1 to 8/);
  });

  test('malformed JSON', async () => {
    const r = await route('{"graph": ', { raw: true });
    assert.equal(r.status, 400);
    assert.equal(r.json.error.code, 'invalid_json');
    assert.doesNotMatch(r.text, /at .*\.js|node_modules/);   // no stack traces
  });

  test('non-object JSON bodies', async () => {
    for (const raw of ['[]', '5', '"x"', 'null', 'true']) {
      const r = await route(raw, { raw: true });
      assert.equal(r.status, 400, raw);
    }
  });

  test('wrong or missing content type', async () => {
    const text = await route(JSON.stringify({ graph: sampleGraph(), source: 1 }), { raw: true, headers: { 'content-type': 'text/plain' } });
    assert.equal(text.status, 415);
    const form = await route('source=1', { raw: true, headers: { 'content-type': 'application/x-www-form-urlencoded' } });
    assert.equal(form.status, 415);
  });

  test('oversized body is rejected with 413', async () => {
    const r = await route({ graph: { nodes: 2, edges: [] }, source: 1, padding: 'x'.repeat(300_000) });
    assert.equal(r.status, 413);
    assert.equal(r.json.error.code, 'payload_too_large');
  });

  test('prototype pollution attempt is a 400 and changes nothing', async () => {
    const r = await route('{"graph":{"nodes":2,"edges":[]},"source":1,"__proto__":{"polluted":true},"constructor":{"prototype":{"polluted":true}}}', { raw: true });
    assert.equal(r.status, 400);
    assert.equal({}.polluted, undefined);
  });

  test('shell and path injection strings are rejected as plain invalid values', async () => {
    for (const evil of ['1; cat /etc/passwd', '$(whoami)', '`id`', '1 && echo hi', '../../etc/passwd', '1\nroute binary']) {
      const r = await route({ graph: sampleGraph(), source: evil });
      assert.equal(r.status, 400, evil);
    }
    const heap = await route({ heap: 'binary; id', graph: sampleGraph(), source: 1 });
    assert.equal(heap.status, 400);
  });

  test('numbers that are not safe integers are rejected', async () => {
    for (const raw of ['{"graph":{"nodes":2,"edges":[]},"source":1e400}', '{"graph":{"nodes":2,"edges":[]},"source":9007199254740993}', '{"graph":{"nodes":2.0000001,"edges":[]},"source":1}']) {
      const r = await route(raw, { raw: true });
      assert.equal(r.status, 400, raw);
    }
  });

  test('errors never leak internals', async () => {
    const r = await route({ graph: sampleGraph(), source: 'x' });
    assert.doesNotMatch(r.text, /parcelhub_api|\/home\/|\\Users\\|stack/i);
  });
});

describe('CORS', () => {
  test('allowed origin gets headers, others do not', async () => {
    const ok = await get('/api/health', { Origin: 'http://localhost:5173' });
    assert.equal(ok.headers.get('access-control-allow-origin'), 'http://localhost:5173');
    const bad = await get('/api/health', { Origin: 'http://evil.example' });
    assert.equal(bad.headers.get('access-control-allow-origin'), null);
  });

  test('preflight for an allowed origin lists only GET and POST', async () => {
    const res = await fetch(`${s.url}/api/route`, { method: 'OPTIONS', headers: { Origin: 'http://localhost:5173', 'Access-Control-Request-Method': 'POST', 'Access-Control-Request-Headers': 'content-type' } });
    assert.equal(res.status, 204);
    assert.equal(res.headers.get('access-control-allow-methods'), 'GET,POST');
  });
});

describe('rate limiting', () => {
  test('429 after the limit, with a JSON body', async () => {
    const limited = await startServer({ rateLimitMax: 3 });
    try {
      const codes = [];
      for (let i = 0; i < 5; i++) codes.push((await fetch(`${limited.url}/api/health`)).status);
      assert.deepEqual(codes, [200, 200, 200, 429, 429]);
      const r = await fetch(`${limited.url}/api/health`);
      assert.equal((await r.json()).error.code, 'rate_limited');
    } finally { await limited.close(); }
  });
});

describe('runner failures are mapped safely', () => {
  const nodeRunner = (script, extra = {}) => createRunner({ command: process.execPath, args: ['-e', script], timeoutMs: 400, maxOutputBytes: 5000, maxConcurrent: 2, maxQueue: 0, ...extra });
  const body = { graph: sampleGraph(), source: 1 };

  test('program missing -> 503', async () => {
    const srv = await startServer({ apiBinary: '/no/such/program' });
    try {
      const h = await fetch(`${srv.url}/api/health`);
      assert.equal(h.status, 503);
      const r = await post(`${srv.url}/api/route`, body);
      assert.equal(r.status, 503);
      assert.equal(r.json.error.code, 'unavailable');
      assert.doesNotMatch(r.text, /no\/such/);
    } finally { await srv.close(); }
  });

  test('timeout -> 504', async () => {
    const srv = await startServer({}, { runner: nodeRunner('setInterval(() => {}, 1000)') });
    try {
      const r = await post(`${srv.url}/api/route`, body);
      assert.equal(r.status, 504);
      assert.equal(r.json.error.code, 'timeout');
    } finally { await srv.close(); }
  });

  test('runaway output -> 502', async () => {
    const srv = await startServer({}, { runner: nodeRunner("setInterval(() => process.stdout.write('x'.repeat(4000)), 1)", { timeoutMs: 5000 }) });
    try {
      const r = await post(`${srv.url}/api/route`, body);
      assert.equal(r.status, 502);
      assert.equal(r.json.error.code, 'output_too_large');
    } finally { await srv.close(); }
  });

  test('crash or garbage output -> generic 500 without the program\'s stderr', async () => {
    for (const script of ["console.error('SECRET-INTERNAL-DETAIL'); process.exit(3)", "console.log('not json')", "console.log('{\"ok\":false}'); process.exit(0)"]) {
      const srv = await startServer({}, { runner: nodeRunner(script) });
      try {
        const r = await post(`${srv.url}/api/route`, body);
        assert.equal(r.status, 500, script);
        assert.equal(r.json.error.code, 'calculation_failed');
        assert.doesNotMatch(r.text, /SECRET-INTERNAL-DETAIL/);
      } finally { await srv.close(); }
    }
  });

  test('the program rejecting a request (exit 2) is reported as 400', async () => {
    const srv = await startServer({}, { runner: nodeRunner("console.log('{\"ok\":false,\"error\":\"parcel id exists twice\"}'); process.exit(2)") });
    try {
      const r = await post(`${srv.url}/api/route`, body);
      assert.equal(r.status, 400);
      assert.match(r.json.error.message, /parcel id exists twice/);
    } finally { await srv.close(); }
  });

  test('too many simultaneous requests -> 503 busy, then recovers', async () => {
    const srv = await startServer({}, { runner: nodeRunner('setTimeout(() => console.log(JSON.stringify({ok:true})), 300)', { timeoutMs: 5000, maxConcurrent: 1, maxQueue: 0 }) });
    try {
      const [a, b] = await Promise.all([post(`${srv.url}/api/route`, body), new Promise((r) => setTimeout(r, 80)).then(() => post(`${srv.url}/api/route`, body))]);
      assert.deepEqual([a.status, b.status], [200, 503]);
      assert.equal(b.json.error.code, 'busy');
      assert.equal((await post(`${srv.url}/api/route`, body)).status, 200);
    } finally { await srv.close(); }
  });

  test('concurrent real requests all succeed', async () => {
    // 12 requests x 3 heaps = 36 process launches; give the queue room so this tests correctness under load,
    // not the (separately tested) busy limit. Slower machines (Windows) take longer to launch processes.
    const srv = await startServer({ calcMaxQueue: 100 });
    try {
      const rs = await Promise.all(Array.from({ length: 12 }, () => post(`${srv.url}/api/route`, { heap: 'all', graph: sampleGraph(), source: 1 })));
      assert.deepEqual(rs.map((r) => r.status), Array(12).fill(200));
      assert.ok(rs.every((r) => r.json.identical));
    } finally { await srv.close(); }
  });
});

describe('benchmark endpoints', () => {
  test('results: 404 when none exist, then served as json and csv', async () => {
    const dir = tmpDir();
    const srv = await startServer({ resultsDir: dir });
    try {
      const none = await fetch(`${srv.url}/api/benchmark/results`);
      assert.equal(none.status, 404);
      fs.writeFileSync(path.join(dir, 'benchmark_results.json'), '{"meta":{},"rows":[],"failures":[]}');
      fs.writeFileSync(path.join(dir, 'benchmark_results.csv'), 'benchmark,heap\n');
      const j = await fetch(`${srv.url}/api/benchmark/results`);
      assert.equal(j.status, 200);
      assert.match(j.headers.get('content-type'), /application\/json/);
      assert.deepEqual(await j.json(), { meta: {}, rows: [], failures: [] });
      const c = await fetch(`${srv.url}/api/benchmark/results?format=csv`);
      assert.match(c.headers.get('content-type'), /text\/csv/);
      assert.equal(await c.text(), 'benchmark,heap\n');
      assert.equal((await fetch(`${srv.url}/api/benchmark/results?format=../../etc/passwd`)).status, 400);
    } finally { await srv.close(); fs.rmSync(dir, { recursive: true, force: true }); }
  });

  test('run: small benchmark returns a verified report', async () => {
    const r = await post(`${s.url}/api/benchmark/run`, { seed: 11, repeats: 2, warmups: 0, instances: 1, graphSizes: [40], parcelSizes: [300], densities: ['sparse', 'dense'], benchmarks: ['dijkstra', 'prim', 'dispatch', 'merge'] });
    assert.equal(r.status, 200, r.text.slice(0, 300));
    assert.deepEqual(r.json.failures, []);
    assert.equal(r.json.meta.seed, 11);
    assert.equal(r.json.rows.length, 3 * (2 + 2 + 1 + 1));     // 3 heaps x (dijkstra 2 + prim 2 + dispatch 1 + merge 1)
    assert.ok(r.json.rows.every((row) => row.verified === true && row.time_ms.median > 0));
  });

  test('run with defaults-only body and empty body', async () => {
    const r = await post(`${s.url}/api/benchmark/run`, { graphSizes: [20], parcelSizes: [100], benchmarks: ['merge'], repeats: 1, warmups: 0 });
    assert.equal(r.status, 200);
    assert.equal(r.json.rows.length, 3);
  });

  test('run rejects out-of-range settings before starting anything', async () => {
    for (const bad of [{ repeats: 50 }, { graphSizes: [100000] }, { benchmarks: ['rm -rf /'] }, { seed: 'x' }]) {
      assert.equal((await post(`${s.url}/api/benchmark/run`, bad)).status, 400);
    }
  });

  test('only one benchmark at a time (second gets 409), then it works again', async () => {
    const heavy = { repeats: 5, warmups: 2, instances: 2, graphSizes: [2000, 2000, 2000], densities: ['sparse', 'medium'], benchmarks: ['dijkstra', 'prim'] };
    const first = post(`${s.url}/api/benchmark/run`, heavy);
    await new Promise((r) => setTimeout(r, 300));
    const second = await post(`${s.url}/api/benchmark/run`, { benchmarks: ['merge'], parcelSizes: [50], repeats: 1, warmups: 0 });
    assert.equal(second.status, 409);
    assert.equal(second.json.error.code, 'busy');
    assert.equal((await first).status, 200);
    assert.equal((await post(`${s.url}/api/benchmark/run`, { benchmarks: ['merge'], parcelSizes: [50], repeats: 1, warmups: 0 })).status, 200);
  });

  test('benchmark endpoint has its own, tighter rate limit', async () => {
    const srv = await startServer({ benchRateLimitMax: 2 });
    try {
      const quick = { benchmarks: ['merge'], parcelSizes: [20], repeats: 1, warmups: 0 };
      const codes = [];
      for (let i = 0; i < 4; i++) codes.push((await post(`${srv.url}/api/benchmark/run`, quick)).status);
      assert.deepEqual(codes, [200, 200, 429, 429]);
    } finally { await srv.close(); }
  });
});
