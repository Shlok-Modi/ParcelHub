// The city (hubs, roads, parcels, upgrades) shared by all pages, plus helpers to turn it
// into backend requests. Limits mirror the backend's so problems are caught before sending.
export const LIMITS = Object.freeze({ maxHubs: 200, maxRoads: 2000, maxWeight: 1_000_000, maxParcels: 500 });

export const LEVELS = Object.freeze([
  { id: 'express', label: 'Express', rank: 1 },
  { id: 'same-day', label: 'Same-day', rank: 2 },
  { id: 'standard', label: 'Standard', rank: 3 },
]);
export const levelLabel = (id) => LEVELS.find((l) => l.id === id)?.label ?? id;
export const levelRank = (id) => LEVELS.find((l) => l.id === id)?.rank ?? 99;

export const HEAPS = Object.freeze([
  { id: 'binary', label: 'Binary Heap' },
  { id: 'binomial', label: 'Binomial Heap' },
  { id: 'fibonacci', label: 'Fibonacci Heap' },
]);

export const hubLabel = (n) => `H${n}`;

// Same city and parcels as the C++ demo. Positions are fractions of the map (0..1).
export function demoCity() {
  const edges = [[1, 2, 7], [1, 3, 9], [1, 6, 14], [2, 3, 10], [2, 4, 15], [3, 4, 11], [3, 6, 2], [4, 5, 6], [5, 6, 9], [5, 7, 3]]
    .map(([from, to, weight]) => ({ from, to, weight }));
  return {
    nodes: 8,
    edges,
    positions: {
      1: { x: 0.1, y: 0.28 }, 2: { x: 0.34, y: 0.1 }, 3: { x: 0.32, y: 0.44 }, 4: { x: 0.68, y: 0.18 },
      5: { x: 0.82, y: 0.58 }, 6: { x: 0.57, y: 0.88 }, 7: { x: 0.96, y: 0.86 }, 8: { x: 0.08, y: 0.84 },
    },
    parcels: [
      { id: 101, source: 1, destination: 5, level: 'standard', deadline: '17:00', arrival: 0 },
      { id: 102, source: 1, destination: 7, level: 'express', deadline: '14:30', arrival: 1 },
      { id: 103, source: 2, destination: 4, level: 'same-day', deadline: '16:00', arrival: 2 },
      { id: 104, source: 3, destination: 5, level: 'express', deadline: '13:00', arrival: 3 },
      { id: 105, source: 1, destination: 8, level: 'standard', deadline: '18:00', arrival: 4 },
      { id: 106, source: 6, destination: 4, level: 'standard', deadline: '18:00', arrival: 5 },
    ],
    upgrades: [{ id: 106, level: 'express' }],
  };
}

const isInt = (v) => Number.isInteger(v);

// Returns a list of plain-language problems (empty = the city can be sent to the server).
export function cityProblems(city) {
  const problems = [];
  if (!isInt(city.nodes) || city.nodes < 1 || city.nodes > LIMITS.maxHubs) problems.push(`The city needs between 1 and ${LIMITS.maxHubs} hubs.`);
  if (city.edges.length > LIMITS.maxRoads) problems.push(`The city can have at most ${LIMITS.maxRoads} roads.`);
  city.edges.forEach((e, i) => {
    const where = `Road ${i + 1}`;
    if (!isInt(e.from) || !isInt(e.to) || e.from < 1 || e.to < 1 || e.from > city.nodes || e.to > city.nodes) problems.push(`${where} must connect hubs 1 to ${city.nodes}.`);
    else if (e.from === e.to) problems.push(`${where} connects a hub to itself.`);
    if (!isInt(e.weight) || e.weight < 0 || e.weight > LIMITS.maxWeight) problems.push(`${where} needs a whole-number cost from 0 to ${LIMITS.maxWeight.toLocaleString('en')}.`);
  });
  return problems;
}

export const graphBody = (city) => ({ nodes: city.nodes, edges: city.edges.map(({ from, to, weight }) => ({ from, to, weight })) });

export function routeRequest(city, { heap, source, destination }) {
  const body = { heap, graph: graphBody(city), source };
  if (destination) body.destination = destination;
  return body;
}
export const mstRequest = (city, { heap, start }) => ({ heap, graph: graphBody(city), start });
export function dispatchRequest(city, { heap }) {
  return { heap, graph: graphBody(city), parcels: city.parcels, upgrades: city.upgrades };
}

// What changes when the city's shape changes: drop anything that points at a hub that no longer exists.
export function pruneCity(city) {
  const ok = (h) => isInt(h) && h >= 1 && h <= city.nodes;
  const parcels = city.parcels.filter((p) => ok(p.source) && ok(p.destination));
  const ids = new Set(parcels.map((p) => p.id));
  return {
    ...city,
    edges: city.edges.filter((e) => ok(e.from) && ok(e.to)),
    parcels,
    upgrades: city.upgrades.filter((u) => ids.has(u.id)),
    positions: Object.fromEntries(Object.entries(city.positions).filter(([k]) => ok(Number(k)))),
  };
}

const STORAGE_KEY = 'parcelhub.city.v1';

export function loadCity(storage = globalThis.localStorage) {
  try {
    const raw = storage?.getItem(STORAGE_KEY);
    if (!raw) return demoCity();
    const c = JSON.parse(raw);
    const shaped = c && Array.isArray(c.edges) && Array.isArray(c.parcels) && Array.isArray(c.upgrades) && c.positions && typeof c.positions === 'object';
    if (!shaped || cityProblems(c).length) return demoCity();
    return c;
  } catch {
    return demoCity();
  }
}

export function saveCity(city, storage = globalThis.localStorage) {
  try { storage?.setItem(STORAGE_KEY, JSON.stringify(city)); } catch { /* storage unavailable: the city just won't persist */ }
}
