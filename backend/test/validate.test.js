import assert from 'node:assert/strict';
import { test } from 'node:test';
import { ValidationError } from '../src/errors.js';
import { validateBench, validateDispatch, validateMst, validateRoute } from '../src/validate.js';
import { sampleGraph } from './helpers.js';

const rejects = (fn, pattern) => assert.throws(fn, (e) => e instanceof ValidationError && (!pattern || pattern.test(e.message)));

test('route: valid request becomes the exact stdin protocol', () => {
  const v = validateRoute({ heap: 'fibonacci', graph: { nodes: 3, edges: [{ from: 1, to: 2, weight: 5 }] }, source: 1, destination: 2 });
  assert.equal(v.heap, 'fibonacci');
  assert.equal(v.input('binary'), 'route binary 3 1 1 2 5 1 2\n');
});

test('route: defaults (heap binary, no destination)', () => {
  const v = validateRoute({ graph: { nodes: 2, edges: [] }, source: 2 });
  assert.equal(v.heap, 'binary');
  assert.equal(v.input('binary'), 'route binary 2 0 2 0\n');
});

test('route: rejects bad numbers, types and ranges', () => {
  const g = (edges) => ({ nodes: 3, edges });
  const base = { graph: g([]), source: 1 };
  for (const bad of [
    { ...base, source: 0 }, { ...base, source: 4 }, { ...base, source: '1' }, { ...base, source: 1.5 },
    { ...base, source: NaN }, { ...base, source: null }, { ...base, source: true }, { ...base, source: [1] },
    { ...base, source: 1e30 }, { ...base, destination: 9 }, { ...base, heap: 'quick' }, { ...base, heap: ['binary'] },
    { ...base, graph: { nodes: 0, edges: [] } }, { ...base, graph: { nodes: 201, edges: [] } },
    { ...base, graph: g([{ from: 1, to: 1, weight: 1 }]) }, { ...base, graph: g([{ from: 1, to: 4, weight: 1 }]) },
    { ...base, graph: g([{ from: 1, to: 2, weight: -1 }]) }, { ...base, graph: g([{ from: 1, to: 2, weight: 1e7 }]) },
    { ...base, graph: g([{ from: 1, to: 2, weight: 1.5 }]) }, { ...base, graph: g([{ from: 1, to: 2 }]) },
    { ...base, graph: g([[1, 2, 3]]) }, { ...base, graph: g('x') }, { ...base, graph: 'x' }, { ...base, graph: null },
    { source: 1 }, { ...base, extra: 1 }, { ...base, graph: { nodes: 3, edges: [], extra: 1 } },
    { ...base, graph: g([{ from: 1, to: 2, weight: 1, label: 'x' }]) },
  ]) rejects(() => validateRoute(bad));
  for (const notObject of [null, undefined, 5, 'x', [], true]) rejects(() => validateRoute(notObject));
});

test('route: size limits are enforced', () => {
  const many = Array.from({ length: 2001 }, () => ({ from: 1, to: 2, weight: 1 }));
  rejects(() => validateRoute({ graph: { nodes: 5, edges: many }, source: 1 }), /at most 2000/);
  const ok = Array.from({ length: 2000 }, () => ({ from: 1, to: 2, weight: 1 }));
  validateRoute({ graph: { nodes: 5, edges: ok }, source: 1 });
});

test('prototype-pollution style keys are rejected, not merged', () => {
  const body = JSON.parse('{"graph":{"nodes":2,"edges":[]},"source":1,"__proto__":{"admin":true}}');
  rejects(() => validateRoute(body), /unknown field/);
  rejects(() => validateRoute(JSON.parse('{"graph":{"nodes":2,"edges":[],"constructor":{}},"source":1}')), /unknown field/);
  assert.equal({}.admin, undefined);
});

test('mst: start defaults to hub 1 and is range checked', () => {
  assert.equal(validateMst({ graph: { nodes: 2, edges: [] } }).input('binomial'), 'mst binomial 2 0 1\n');
  rejects(() => validateMst({ graph: { nodes: 2, edges: [] }, start: 3 }));
});

const parcel = (o = {}) => ({ id: 1, source: 1, destination: 2, level: 'standard', deadline: '17:00', ...o });
const dispatchBody = (o = {}) => ({ graph: { nodes: 3, edges: [] }, parcels: [parcel()], ...o });

test('dispatch: valid request, deadline parsing and default arrival', () => {
  const v = validateDispatch(dispatchBody({ parcels: [parcel({ id: 7, level: 'express', deadline: '14:30' }), parcel({ id: 8, deadline: 60, arrival: 9 })], upgrades: [{ id: 8, level: 'same-day' }] }));
  assert.equal(v.input('binary'), 'dispatch binary 3 0 2 7 1 2 1 870 0 8 1 2 3 60 9 1 8 2\n');
});

test('dispatch: rejects invalid parcels and upgrades', () => {
  for (const bad of [
    dispatchBody({ parcels: 'x' }), dispatchBody({ parcels: [parcel({ id: 0 })] }), dispatchBody({ parcels: [parcel({ id: 1.5 })] }),
    dispatchBody({ parcels: [parcel({ source: 4 })] }), dispatchBody({ parcels: [parcel({ level: 'Express' })] }),
    dispatchBody({ parcels: [parcel({ level: 1 })] }), dispatchBody({ parcels: [parcel({ deadline: '25:00' })] }),
    dispatchBody({ parcels: [parcel({ deadline: '9:00' })] }), dispatchBody({ parcels: [parcel({ deadline: 1440 })] }),
    dispatchBody({ parcels: [parcel({ arrival: -1 })] }), dispatchBody({ parcels: [parcel({ note: 'x' })] }),
    dispatchBody({ parcels: [parcel(), parcel()] }),                                          // duplicate id
    dispatchBody({ upgrades: [{ id: 99, level: 'express' }] }),                              // unknown parcel
    dispatchBody({ upgrades: [{ id: 1, level: 'standard' }] }),                              // not an upgrade
    dispatchBody({ parcels: [parcel({ level: 'express' })], upgrades: [{ id: 1, level: 'same-day' }] }),   // downgrade
    dispatchBody({ upgrades: [{ id: 1, level: 'express' }, { id: 1, level: 'same-day' }] }), // twice
    dispatchBody({ upgrades: 'x' }), dispatchBody({ upgrades: [{ id: 1 }] }),
    { graph: { nodes: 3, edges: [] } },                                                       // parcels missing
  ]) rejects(() => validateDispatch(bad));
  rejects(() => validateDispatch(dispatchBody({ parcels: Array.from({ length: 501 }, (_, i) => parcel({ id: i + 1 })) })), /at most 500/);
});

test('bench: defaults, custom values and limits', () => {
  const d = validateBench({});
  assert.equal(d.input, 'bench 20240601 3 1 1 2 100 500 2 1000 5000 3 sparse medium dense 4 dijkstra prim dispatch merge\n');
  assert.equal(validateBench(undefined).input, d.input);
  const c = validateBench({ seed: 5, repeats: 2, warmups: 0, instances: 2, graphSizes: [50], parcelSizes: [200], densities: ['dense'], benchmarks: ['prim', 'merge'] });
  assert.equal(c.input, 'bench 5 2 0 2 1 50 1 200 1 dense 2 prim merge\n');
  for (const bad of [
    { repeats: 6 }, { repeats: 0 }, { warmups: 3 }, { instances: 3 }, { seed: -1 }, { seed: 1.5 }, { graphSizes: [2001] }, { graphSizes: [1] },
    { graphSizes: [1, 2, 3, 4, 5, 6] }, { parcelSizes: [20001] }, { densities: ['huge'] }, { densities: ['dense', 'dense'] },
    { benchmarks: [] }, { benchmarks: ['sort'] }, { benchmarks: 'prim' }, { unknown: 1 },
    { benchmarks: ['prim'], graphSizes: [] }, { benchmarks: ['prim'], densities: [] }, { benchmarks: ['merge'], parcelSizes: [] },
  ]) rejects(() => validateBench(bad));
  validateBench({ benchmarks: ['merge'], graphSizes: [], densities: [] });   // graph lists may be empty when unused
});

test('every serialised request is only digits, fixed words and spaces', () => {
  const v = validateDispatch(dispatchBody({ upgrades: [] }));
  assert.match(v.input('fibonacci'), /^[a-z0-9 ]+\n$/);
  assert.match(validateRoute({ graph: sampleGraph(), source: 1 }).input('binary'), /^[a-z0-9 ]+\n$/);
});
