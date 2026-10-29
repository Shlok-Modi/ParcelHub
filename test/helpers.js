import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { createApp } from '../src/app.js';
import { defaultBinaryPath, loadConfig } from '../src/config.js';

export function testConfig(overrides = {}) {
  const config = loadConfig({});
  config.port = 0;
  config.apiBinary = process.env.PARCELHUB_API_BIN || defaultBinaryPath();
  config.rateLimitMax = 100000;     // tests hammer the API; the rate-limit test sets its own
  config.benchRateLimitMax = 100000;
  return Object.assign(config, overrides);
}

export async function startServer(configOverrides = {}, deps = {}) {
  const config = testConfig(configOverrides);
  if (!deps.runner && !configOverrides.apiBinary && !fs.existsSync(config.apiBinary)) {
    throw new Error(`calculation program not found at ${config.apiBinary} - run build.ps1 (or build parcelhub_api) first`);
  }
  const app = createApp({ config, ...deps });
  const server = await new Promise((resolve) => { const s = app.listen(0, '127.0.0.1', () => resolve(s)); });
  const url = `http://127.0.0.1:${server.address().port}`;
  return { url, config, close: () => new Promise((r) => { server.close(r); server.closeAllConnections?.(); }) };
}

export async function post(url, body, { headers = {}, raw = false } = {}) {
  const res = await fetch(url, {
    method: 'POST',
    headers: { 'content-type': 'application/json', ...headers },
    body: raw ? body : JSON.stringify(body),
  });
  const text = await res.text();
  let json = null;
  try { json = JSON.parse(text); } catch { /* not JSON */ }
  return { status: res.status, json, text, headers: res.headers };
}

export const tmpDir = () => fs.mkdtempSync(path.join(os.tmpdir(), 'parcelhub-test-'));

// The sample city from the C++ demo.
export const sampleGraph = () => ({
  nodes: 8,
  edges: [[1, 2, 7], [1, 3, 9], [1, 6, 14], [2, 3, 10], [2, 4, 15], [3, 4, 11], [3, 6, 2], [4, 5, 6], [5, 6, 9], [5, 7, 3]]
    .map(([from, to, weight]) => ({ from, to, weight })),
});

// ---- independent JS reference solutions (no heaps of ours involved) ----
export function refDistances(graph, source) {
  const INF = Infinity;
  const dist = Array(graph.nodes + 1).fill(INF);
  const adj = Array.from({ length: graph.nodes + 1 }, () => []);
  for (const e of graph.edges) { adj[e.from].push([e.to, e.weight]); adj[e.to].push([e.from, e.weight]); }
  const done = Array(graph.nodes + 1).fill(false);
  dist[source] = 0;
  for (;;) {
    let u = -1;
    for (let v = 1; v <= graph.nodes; v++) if (!done[v] && dist[v] < INF && (u === -1 || dist[v] < dist[u])) u = v;
    if (u === -1) break;
    done[u] = true;
    for (const [v, w] of adj[u]) if (dist[u] + w < dist[v]) dist[v] = dist[u] + w;
  }
  return dist.slice(1).map((d) => (d === INF ? null : d));
}

export function refMst(graph) {
  const parent = Array.from({ length: graph.nodes + 1 }, (_, i) => i);
  const find = (x) => { while (parent[x] !== x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; };
  let total = 0, components = graph.nodes;
  for (const e of [...graph.edges].sort((a, b) => a.weight - b.weight)) {
    const a = find(e.from), b = find(e.to);
    if (a !== b) { parent[a] = b; total += e.weight; components--; }
  }
  return { total, components };
}

// Small deterministic random generator so failures are reproducible.
export function lcg(seed) {
  let s = seed >>> 0;
  return () => { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s / 2 ** 32; };
}

export function randomGraph(rand, nodes, edgeCount) {
  const seen = new Set();
  const edges = [];
  for (let tries = 0; edges.length < edgeCount && tries < edgeCount * 20; tries++) {
    const a = 1 + Math.floor(rand() * nodes), b = 1 + Math.floor(rand() * nodes);
    const key = a < b ? `${a}-${b}` : `${b}-${a}`;
    if (a === b || seen.has(key)) continue;
    seen.add(key);
    edges.push({ from: a, to: b, weight: Math.floor(rand() * 50) });   // zero weights included
  }
  return { nodes, edges };
}
