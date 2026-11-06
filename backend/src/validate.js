// Strict request validation. Every function returns a normalised object or throws
// ValidationError. Unknown fields are rejected (not ignored), integers must be real
// integers (no "5", 5.5, NaN or booleans), and nothing from the client is ever put on a
// command line: validated numbers are re-serialised into the stdin text protocol that
// the C++ program parses.
import { LIMITS } from './config.js';
import { ValidationError } from './errors.js';

export const HEAPS = Object.freeze(['binary', 'binomial', 'fibonacci']);
const LEVELS = Object.freeze({ express: 1, 'same-day': 2, standard: 3 });
const DENSITIES = Object.freeze(['sparse', 'medium', 'dense']);
const BENCHMARKS = Object.freeze(['dijkstra', 'prim', 'dispatch', 'merge']);

function isPlainObject(v) {
  return v !== null && typeof v === 'object' && !Array.isArray(v) && Object.getPrototypeOf(v) === Object.prototype;
}

function object(value, path, allowed) {
  if (!isPlainObject(value)) throw new ValidationError(`${path} must be an object`);
  for (const key of Object.keys(value)) {
    if (!allowed.includes(key)) throw new ValidationError(`${path}: unknown field "${key.slice(0, 40)}"`);
  }
  return value;
}

function integer(value, path, min, max) {
  if (typeof value !== 'number' || !Number.isInteger(value) || value < min || value > max) {
    throw new ValidationError(`${path} must be a whole number from ${min} to ${max}`);
  }
  return value;
}

function array(value, path, maxLength) {
  if (!Array.isArray(value)) throw new ValidationError(`${path} must be an array`);
  if (value.length > maxLength) throw new ValidationError(`${path} may have at most ${maxLength} entries`);
  return value;
}

function oneOf(value, path, options) {
  if (typeof value !== 'string' || !options.includes(value)) {
    throw new ValidationError(`${path} must be one of: ${options.join(', ')}`);
  }
  return value;
}

function heapField(body, { allowAll = true } = {}) {
  if (body.heap === undefined) return 'binary';
  return oneOf(body.heap, 'heap', allowAll ? [...HEAPS, 'all'] : HEAPS);
}

function graphField(value) {
  const g = object(value, 'graph', ['nodes', 'edges']);
  const nodes = integer(g.nodes, 'graph.nodes', 1, LIMITS.maxHubs);
  const edges = array(g.edges ?? [], 'graph.edges', LIMITS.maxRoads).map((e, i) => {
    const p = `graph.edges[${i}]`;
    const edge = object(e, p, ['from', 'to', 'weight']);
    const from = integer(edge.from, `${p}.from`, 1, nodes);
    const to = integer(edge.to, `${p}.to`, 1, nodes);
    if (from === to) throw new ValidationError(`${p}: a road cannot connect a hub to itself`);
    return { from, to, weight: integer(edge.weight, `${p}.weight`, 0, LIMITS.maxWeight) };
  });
  return { nodes, edges };
}

function graphTokens(graph) {
  const out = [String(graph.nodes), String(graph.edges.length)];
  for (const e of graph.edges) out.push(String(e.from), String(e.to), String(e.weight));
  return out;
}

const finish = (tokens) => tokens.join(' ') + '\n';

export function validateRoute(body) {
  const b = object(body, 'request body', ['heap', 'graph', 'source', 'destination']);
  const heap = heapField(b);
  const graph = graphField(b.graph);
  const source = integer(b.source, 'source', 1, graph.nodes);
  const destination = b.destination === undefined ? 0 : integer(b.destination, 'destination', 1, graph.nodes);
  return {
    heap,
    input: (h) => finish(['route', h, ...graphTokens(graph), String(source), String(destination)]),
  };
}

export function validateMst(body) {
  const b = object(body, 'request body', ['heap', 'graph', 'start']);
  const heap = heapField(b);
  const graph = graphField(b.graph);
  const start = b.start === undefined ? 1 : integer(b.start, 'start', 1, graph.nodes);
  return {
    heap,
    input: (h) => finish(['mst', h, ...graphTokens(graph), String(start)]),
  };
}

function deadlineMinutes(value, path) {
  if (typeof value === 'number') return integer(value, path, 0, 1439);
  const m = typeof value === 'string' ? /^([01]\d|2[0-3]):([0-5]\d)$/.exec(value) : null;
  if (!m) throw new ValidationError(`${path} must be "HH:MM" (00:00-23:59) or minutes since midnight`);
  return Number(m[1]) * 60 + Number(m[2]);
}

export function validateDispatch(body) {
  const b = object(body, 'request body', ['heap', 'graph', 'parcels', 'upgrades']);
  const heap = heapField(b);
  const graph = graphField(b.graph);
  const ids = new Set();
  const parcels = array(b.parcels, 'parcels', LIMITS.maxParcels).map((p, i) => {
    const path = `parcels[${i}]`;
    const o = object(p, path, ['id', 'source', 'destination', 'level', 'deadline', 'arrival']);
    const id = integer(o.id, `${path}.id`, 1, 1_000_000_000);
    if (ids.has(id)) throw new ValidationError(`${path}.id: duplicate parcel id ${id}`);
    ids.add(id);
    return {
      id,
      source: integer(o.source, `${path}.source`, 1, graph.nodes),
      destination: integer(o.destination, `${path}.destination`, 1, graph.nodes),
      level: LEVELS[oneOf(o.level, `${path}.level`, Object.keys(LEVELS))],
      deadline: deadlineMinutes(o.deadline ?? 0, `${path}.deadline`),
      arrival: o.arrival === undefined ? i : integer(o.arrival, `${path}.arrival`, 0, 1_000_000_000),
    };
  });
  const levelOf = new Map(parcels.map((p) => [p.id, p.level]));
  const upgraded = new Set();
  const upgrades = array(b.upgrades ?? [], 'upgrades', LIMITS.maxUpgrades).map((u, i) => {
    const path = `upgrades[${i}]`;
    const o = object(u, path, ['id', 'level']);
    const id = integer(o.id, `${path}.id`, 1, 1_000_000_000);
    if (!levelOf.has(id)) throw new ValidationError(`${path}.id: no parcel with id ${id}`);
    if (upgraded.has(id)) throw new ValidationError(`${path}.id: parcel ${id} is upgraded more than once`);
    const level = LEVELS[oneOf(o.level, `${path}.level`, Object.keys(LEVELS))];
    if (level >= levelOf.get(id)) throw new ValidationError(`${path}: not an upgrade (the new level must be more urgent than the current one)`);
    upgraded.add(id);
    return { id, level };
  });
  return {
    heap,
    input: (h) =>
      finish([
        'dispatch', h, ...graphTokens(graph),
        String(parcels.length),
        ...parcels.flatMap((p) => [p.id, p.source, p.destination, p.level, p.deadline, p.arrival].map(String)),
        String(upgrades.length),
        ...upgrades.flatMap((u) => [u.id, u.level].map(String)),
      ]),
  };
}

function uniqueList(value, path, options, maxLength) {
  const list = array(value, path, maxLength).map((v, i) => oneOf(v, `${path}[${i}]`, options));
  if (new Set(list).size !== list.length) throw new ValidationError(`${path} must not repeat values`);
  return list;
}

export function validateBench(body) {
  const L = LIMITS.bench;
  const b = object(body ?? {}, 'request body', ['seed', 'repeats', 'warmups', 'instances', 'graphSizes', 'parcelSizes', 'densities', 'benchmarks']);
  const seed = b.seed === undefined ? 20240601 : integer(b.seed, 'seed', 0, 4_000_000_000);
  const repeats = b.repeats === undefined ? 3 : integer(b.repeats, 'repeats', 1, L.maxRepeats);
  const warmups = b.warmups === undefined ? 1 : integer(b.warmups, 'warmups', 0, L.maxWarmups);
  const instances = b.instances === undefined ? 1 : integer(b.instances, 'instances', 1, L.maxInstances);
  const sizes = (value, path, max, fallback) => {
    const list = value === undefined ? fallback : array(value, path, L.maxListLength).map((v, i) => integer(v, `${path}[${i}]`, 2, max));
    return list;
  };
  const graphSizes = sizes(b.graphSizes, 'graphSizes', L.maxGraphSize, [100, 500]);
  const parcelSizes = sizes(b.parcelSizes, 'parcelSizes', L.maxParcelSize, [1000, 5000]);
  const densities = b.densities === undefined ? [...DENSITIES] : uniqueList(b.densities, 'densities', DENSITIES, 3);
  const benchmarks = b.benchmarks === undefined ? [...BENCHMARKS] : uniqueList(b.benchmarks, 'benchmarks', BENCHMARKS, 4);
  if (benchmarks.length === 0) throw new ValidationError('benchmarks must list at least one benchmark');
  if (benchmarks.some((x) => x === 'dijkstra' || x === 'prim') && (graphSizes.length === 0 || densities.length === 0)) {
    throw new ValidationError('graph benchmarks need at least one graph size and one density');
  }
  if (benchmarks.some((x) => x === 'dispatch' || x === 'merge') && parcelSizes.length === 0) {
    throw new ValidationError('dispatch and merge benchmarks need at least one parcel size');
  }
  return {
    input: finish([
      'bench', seed, repeats, warmups, instances,
      graphSizes.length, ...graphSizes,
      parcelSizes.length, ...parcelSizes,
      densities.length, ...densities,
      benchmarks.length, ...benchmarks,
    ].map(String)),
  };
}
