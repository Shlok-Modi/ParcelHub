import { describe, it, expect, vi, afterEach } from 'vitest';
import { layoutGraph, toBoard, fromBoard, BOARD } from '../src/layout.js';
import { demoCity, cityProblems, pruneCity, loadCity, saveCity, routeRequest, dispatchRequest, LIMITS } from '../src/city.js';
import { parseReport, aggregate, slice, largestSizeSummary, spreadIsWide, available } from '../src/bench.js';
import { niceStep, linearTicks, logTicks, compact, formatMs, formatTick, linearPos, logPos } from '../src/charts/scales.js';
import { request, ApiError } from '../src/api.js';

afterEach(() => vi.unstubAllGlobals());

describe('layout', () => {
  it('is deterministic and keeps pinned hubs', () => {
    const edges = [{ from: 1, to: 2, weight: 1 }, { from: 2, to: 3, weight: 1 }];
    const a = layoutGraph(3, edges, { 1: { x: 0.2, y: 0.3 } });
    const b = layoutGraph(3, edges, { 1: { x: 0.2, y: 0.3 } });
    expect(a).toEqual(b);
    expect(a[1]).toEqual({ x: 0.2, y: 0.3 });
    for (const p of Object.values(a)) { expect(p.x).toBeGreaterThan(0); expect(p.x).toBeLessThan(1); }
  });
  it('round-trips board coordinates', () => {
    const p = { x: 0.4, y: 0.6 };
    const b = toBoard(p);
    const back = fromBoard(b.x, b.y);
    expect(back.x).toBeCloseTo(0.4); expect(back.y).toBeCloseTo(0.6);
    expect(b.x).toBeLessThan(BOARD.width);
  });
  it('clamps dragged positions into the board', () => {
    const p = fromBoard(-500, 99999);
    expect(p.x).toBeGreaterThan(0); expect(p.y).toBeLessThan(1);
  });
});

describe('city', () => {
  it('demo city is valid', () => expect(cityProblems(demoCity())).toEqual([]));
  it('reports problems in plain language', () => {
    const c = demoCity();
    c.edges.push({ from: 1, to: 1, weight: 2 }, { from: 1, to: 99, weight: 2 }, { from: 1, to: 2, weight: 1.5 });
    expect(cityProblems(c)).toHaveLength(3);
    expect(cityProblems({ ...c, nodes: LIMITS.maxHubs + 1, edges: [] })[0]).toMatch(/hubs/);
  });
  it('prunes things that point at removed hubs', () => {
    const c = pruneCity({ ...demoCity(), nodes: 4 });
    expect(c.edges.every((e) => e.from <= 4 && e.to <= 4)).toBe(true);
    expect(c.parcels.every((p) => p.source <= 4 && p.destination <= 4)).toBe(true);
    expect(c.upgrades.every((u) => c.parcels.some((p) => p.id === u.id))).toBe(true);
    expect(Object.keys(c.positions)).toHaveLength(4);
  });
  it('builds requests', () => {
    const c = demoCity();
    expect(routeRequest(c, { heap: 'binary', source: 1 })).not.toHaveProperty('destination');
    expect(routeRequest(c, { heap: 'binary', source: 1, destination: 7 }).destination).toBe(7);
    expect(dispatchRequest(c, { heap: 'all' }).parcels).toHaveLength(6);
  });
  it('saves and loads, and survives bad or missing storage', () => {
    const store = new Map();
    const storage = { getItem: (k) => store.get(k) ?? null, setItem: (k, v) => store.set(k, v) };
    const c = { ...demoCity(), nodes: 9 };
    saveCity(c, storage);
    expect(loadCity(storage).nodes).toBe(9);
    store.set('parcelhub.city.v1', '{not json');
    expect(loadCity(storage)).toEqual(demoCity());
    store.set('parcelhub.city.v1', JSON.stringify({ nodes: 5 }));
    expect(loadCity(storage)).toEqual(demoCity());
    const throwing = { getItem() { throw new Error('x'); }, setItem() { throw new Error('x'); } };
    expect(loadCity(throwing)).toEqual(demoCity());
    expect(() => saveCity(c, throwing)).not.toThrow();
  });
});

const row = (o = {}) => ({ benchmark: 'dijkstra', density: 'sparse', size: 100, heap: 'Binary Heap', time_ms: { median: 2, mean: 2, stddev: 0.1 }, ops: { comparisons: 10 }, ...o });

describe('bench', () => {
  it('parses valid reports and rejects bad ones', () => {
    expect(parseReport(JSON.stringify({ rows: [row()] })).rows).toHaveLength(1);
    expect(() => parseReport('nope')).toThrow(/valid JSON/);
    expect(() => parseReport({})).toThrow(/rows/);
    expect(() => parseReport({ rows: [row({ heap: 'Pairing Heap' })] })).toThrow(/Row 1.*heap/);
    expect(() => parseReport({ rows: [row({ size: 0 })] })).toThrow(/size/);
    expect(() => parseReport({ rows: [row({ ops: {} })] })).toThrow(/operation/);
    expect(() => parseReport({ rows: [null] })).toThrow(/Row 1/);
  });
  it('averages instances', () => {
    const pts = aggregate([row(), row({ time_ms: { median: 4, mean: 4, stddev: 0.3 }, ops: { comparisons: 30 } })]);
    expect(pts).toHaveLength(1);
    expect(pts[0].medianMs).toBe(3);
    expect(pts[0].comparisons).toBe(20);
    expect(pts[0].heap).toBe('binary');
  });
  it('slices, summarises and lists what is available', () => {
    const pts = aggregate([
      row({ size: 100 }), row({ size: 500 }),
      row({ heap: 'Binomial Heap', size: 500, time_ms: { median: 1, mean: 1, stddev: 0 } }),
      row({ density: 'dense', size: 100 }),
    ]);
    const s = slice(pts, 'dijkstra', 'sparse');
    expect(s.sizes).toEqual([100, 500]);
    expect(s.series.map((x) => x.id)).toEqual(['binary', 'binomial']);
    const sum = largestSizeSummary(s);
    expect(sum.size).toBe(500);
    expect(sum.fastest).toBe('binomial');
    expect(sum.rows.find((r) => r.id === 'binomial').vsBinary).toBe(0.5);
    expect(largestSizeSummary({ sizes: [], series: [] })).toBeNull();
    expect(available(pts).densities.dijkstra).toEqual(['sparse', 'dense']);
  });
  it('detects wide spreads', () => {
    const mk = (ys) => [{ points: ys.map((y) => ({ y })) }];
    expect(spreadIsWide(mk([1, 100]), (p) => p.y)).toBe(true);
    expect(spreadIsWide(mk([1, 10]), (p) => p.y)).toBe(false);
    expect(spreadIsWide(mk([5]), (p) => p.y)).toBe(false);
  });
});

describe('scales', () => {
  it('chooses nice steps and ticks', () => {
    expect(niceStep(0.7)).toBe(1); expect(niceStep(23)).toBe(25);
    const t = linearTicks(7.3);
    expect(t.ticks[0]).toBe(0); expect(t.max).toBeGreaterThanOrEqual(7.3);
    expect(linearTicks(0).ticks).toEqual([0, 1]);
    expect(logTicks(0.02, 40).ticks).toEqual([0.01, 0.1, 1, 10, 100]);
    expect(logTicks(5, 5).ticks.length).toBeGreaterThan(1);
  });
  it('positions values', () => {
    expect(linearPos(5, 0, 10)).toBe(0.5); expect(logPos(10, 1, 100)).toBeCloseTo(0.5);
    expect(linearPos(1, 1, 1)).toBe(0.5);
  });
  it('formats', () => {
    expect(compact(1500)).toBe('1.5K'); expect(compact(2_000_000)).toBe('2M'); expect(compact(42)).toBe('42');
    expect(formatMs(0.021)).toBe('0.0210 ms'); expect(formatMs(2.5)).toBe('2.50 ms'); expect(formatMs(1200)).toBe('1.2 s');
    expect(formatMs(NaN)).toBe('–');
    expect(formatTick(0)).toBe('0'); expect(formatTick(0.25)).toBe('0.25'); expect(formatTick(5000)).toBe('5K');
  });
});

describe('api request', () => {
  const reply = (status, body, json = true) => vi.fn(async () => ({ ok: status < 400, status, json: async () => { if (!json) throw new Error('x'); return body; }, text: async () => String(body) }));
  it('returns JSON', async () => {
    vi.stubGlobal('fetch', reply(200, { ok: 1 }));
    expect(await request('/x')).toEqual({ ok: 1 });
  });
  it('uses the structured error message', async () => {
    vi.stubGlobal('fetch', reply(400, { error: { code: 'invalid_request', message: 'bad hub' } }));
    await expect(request('/x')).rejects.toMatchObject({ message: 'bad hub', code: 'invalid_request', status: 400 });
  });
  it('treats a bare 502 as the server being unreachable', async () => {
    vi.stubGlobal('fetch', reply(502, '', false));
    await expect(request('/x')).rejects.toMatchObject({ code: 'network' });
  });
  it('keeps a bare 4xx generic', async () => {
    vi.stubGlobal('fetch', reply(404, '', false));
    await expect(request('/x')).rejects.toMatchObject({ code: 'error', status: 404 });
  });
  it('maps fetch failures to a network error', async () => {
    vi.stubGlobal('fetch', vi.fn(async () => { throw new TypeError('failed'); }));
    const err = await request('/x').catch((e) => e);
    expect(err).toBeInstanceOf(ApiError); expect(err.code).toBe('network');
  });
  it('rethrows caller cancellation and reports timeouts', async () => {
    vi.stubGlobal('fetch', (url, { signal }) => new Promise((_, rej) => signal.addEventListener('abort', () => rej(new Error('aborted')))));
    const c = new AbortController();
    const p = request('/x', { signal: c.signal });
    c.abort();
    await expect(p).rejects.toThrow('aborted');
    await expect(request('/x', { timeoutMs: 10 })).rejects.toMatchObject({ code: 'timeout' });
  });
});
