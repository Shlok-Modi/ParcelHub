// Reading, checking and summarising benchmark reports (the JSON written by parcelhub_bench).
export const BENCHMARKS = Object.freeze([
  { id: 'dijkstra', label: 'Dijkstra', long: 'Shortest paths (Dijkstra)', unit: 'hubs', graph: true },
  { id: 'prim', label: 'Prim', long: 'Minimum spanning tree (Prim)', unit: 'hubs', graph: true },
  { id: 'dispatch', label: 'Dispatch', long: 'Dispatch queue', unit: 'parcels', graph: false },
  { id: 'merge', label: 'Merge', long: 'Merging two hub queues', unit: 'entries', graph: false },
]);

export const HEAP_SERIES = Object.freeze([
  { id: 'binary', name: 'Binary Heap', marker: 'circle' },
  { id: 'binomial', name: 'Binomial Heap', marker: 'square' },
  { id: 'fibonacci', name: 'Fibonacci Heap', marker: 'triangle' },
]);
const HEAP_IDS = Object.fromEntries(HEAP_SERIES.map((h) => [h.name, h.id]));

const MAX_BYTES = 20 * 1024 * 1024;
const isNum = (v) => typeof v === 'number' && Number.isFinite(v);

// Validates the shape of a report and returns it; throws an Error with a plain-language message.
export function parseReport(input) {
  let report = input;
  if (typeof input === 'string') {
    if (input.length > MAX_BYTES) throw new Error('That file is too large to be a benchmark report.');
    try { report = JSON.parse(input); } catch { throw new Error('That file is not valid JSON.'); }
  }
  if (!report || typeof report !== 'object' || !Array.isArray(report.rows)) {
    throw new Error('That file is not a ParcelHub benchmark report (it has no "rows" list).');
  }
  report.rows.forEach((r, i) => {
    const bad = (why) => { throw new Error(`Row ${i + 1} of the report is not valid: ${why}.`); };
    if (!r || typeof r !== 'object') bad('it is not an object');
    if (!BENCHMARKS.some((b) => b.id === r.benchmark)) bad('unknown benchmark');
    if (!(r.heap in HEAP_IDS)) bad('unknown heap');
    if (!isNum(r.size) || r.size < 1) bad('size must be a positive number');
    if (!r.time_ms || !isNum(r.time_ms.median) || !isNum(r.time_ms.mean) || !isNum(r.time_ms.stddev)) bad('timing values are missing');
    if (!r.ops || !isNum(r.ops.comparisons)) bad('operation counts are missing');
  });
  return { meta: report.meta ?? {}, rows: report.rows, failures: Array.isArray(report.failures) ? report.failures : [] };
}

// Averages the instances of each (benchmark, density, size, heap) into one point.
export function aggregate(rows) {
  const groups = new Map();
  for (const r of rows) {
    const key = [r.benchmark, r.density ?? '', r.size, r.heap].join('|');
    let g = groups.get(key);
    if (!g) {
      g = { benchmark: r.benchmark, density: r.density ?? '', size: r.size, heap: HEAP_IDS[r.heap], n: 0, medianMs: 0, meanMs: 0, stddevMs: 0, comparisons: 0, edges: 0, upgrades: 0 };
      groups.set(key, g);
    }
    g.n += 1;
    g.medianMs += r.time_ms.median; g.meanMs += r.time_ms.mean; g.stddevMs += r.time_ms.stddev;
    g.comparisons += r.ops.comparisons;
    g.edges += r.edges ?? 0; g.upgrades += r.upgrades ?? 0;
  }
  return [...groups.values()].map((g) => ({
    ...g,
    medianMs: g.medianMs / g.n, meanMs: g.meanMs / g.n, stddevMs: g.stddevMs / g.n,
    comparisons: g.comparisons / g.n, edges: g.edges / g.n, upgrades: g.upgrades / g.n,
  }));
}

// Which benchmarks and densities the report contains, so the filters only offer real choices.
export function available(points) {
  const benchmarks = BENCHMARKS.filter((b) => points.some((p) => p.benchmark === b.id));
  const densities = {};
  for (const b of benchmarks) {
    densities[b.id] = ['sparse', 'medium', 'dense'].filter((d) => points.some((p) => p.benchmark === b.id && p.density === d));
  }
  return { benchmarks, densities };
}

// One slice of the report (a benchmark, plus a density for graph benchmarks) ready to chart.
export function slice(points, benchmark, density) {
  const chosen = points.filter((p) => p.benchmark === benchmark && (density ? p.density === density : true));
  const sizes = [...new Set(chosen.map((p) => p.size))].sort((a, b) => a - b);
  const series = HEAP_SERIES
    .map((h) => ({ ...h, points: chosen.filter((p) => p.heap === h.id).sort((a, b) => a.size - b.size) }))
    .filter((s) => s.points.length > 0);
  return { sizes, series };
}

// How each heap did at the largest size, relative to the Binary Heap.
export function largestSizeSummary({ sizes, series }) {
  if (sizes.length === 0) return null;
  const size = sizes[sizes.length - 1];
  const rows = series.map((s) => ({ ...s, point: s.points.find((p) => p.size === size) })).filter((r) => r.point);
  const binary = rows.find((r) => r.id === 'binary')?.point.medianMs;
  const fastest = rows.reduce((best, r) => (!best || r.point.medianMs < best.point.medianMs ? r : best), null);
  return {
    size,
    fastest: fastest?.id ?? null,
    rows: rows.map((r) => ({ id: r.id, name: r.name, medianMs: r.point.medianMs, comparisons: r.point.comparisons, vsBinary: binary ? r.point.medianMs / binary : null })),
  };
}

// True when the largest value is so much bigger than the smallest that a linear axis flattens the small ones.
export function spreadIsWide(series, pick, ratio = 50) {
  const values = series.flatMap((s) => s.points.map(pick)).filter((v) => Number.isFinite(v) && v > 0);
  if (values.length < 2) return false;
  return Math.max(...values) / Math.min(...values) > ratio;
}
