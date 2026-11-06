// Feeds the C++ program directly, bypassing the Express validation, to prove it is safe on its own.
import assert from 'node:assert/strict';
import { test } from 'node:test';
import { defaultBinaryPath } from '../src/config.js';
import { runProcess } from '../src/runner.js';

const bin = process.env.PARCELHUB_API_BIN || defaultBinaryPath();
const run = (input) => runProcess({ command: bin, input, timeoutMs: 20_000, maxOutputBytes: 5_000_000 });
const rejected = async (input, pattern) => {
  const started = Date.now();
  const r = await run(input);
  assert.equal(r.code, 2, `input ${JSON.stringify(String(input).slice(0, 60))}: exit ${r.code}, stderr ${r.stderr.slice(0, 200)}`);
  const j = JSON.parse(r.stdout);
  assert.equal(j.ok, false);
  if (pattern) assert.match(j.error, pattern);
  assert.ok(Date.now() - started < 5000, 'rejection must be fast');
};

test('empty, unknown and truncated requests', async () => {
  await rejected('', /missing value: command/);
  await rejected('   \n\t ', /missing value/);
  await rejected('shutdown now', /unknown command/);
  await rejected('route', /missing value/);
  await rejected('route binary', /missing value/);
  await rejected('route binary 3', /missing value/);
  await rejected('route binary 3 2 1 2 5', /missing value/);                // fewer roads than announced
  await rejected('route binary 3 1 1 2 5 1', /missing value/);              // no destination
  await rejected('mst fibonacci 3 0', /missing value/);
  await rejected('dispatch binary 3 0 2 1 1 2 1 600 0', /missing value/);    // second parcel missing
  await rejected('bench 1 1 0 1', /missing value/);
});

test('numbers out of range, wrong types, junk tokens', async () => {
  for (const bad of [
    'route binary 0 0 1 0', 'route binary -3 0 1 0', 'route binary 5001 0 1 0', 'route binary 3 -1 1 0', 'route binary 3 100001 1 0',
    'route binary 3 1 1 2 -5 1 0', 'route binary 3 1 1 2 1000000001 1 0', 'route binary 3 1 1 2 5 4 0', 'route binary 3 1 1 2 5 0 0',
    'route binary 3 1 1 4 5 1 0', 'route binary 3 1 1 1 5 1 0', 'route binary 3 0 1 9',
    'route binary 3.5 0 1 0', 'route binary 3x 0 1 0', 'route binary 0x10 0 1 0', 'route binary 1e3 0 1 0', 'route binary +3 0 1 0 junk',
    'route binary 99999999999999999999 0 1 0', 'route binary nan 0 1 0', 'route binary -0 0 1 0',
    'route heap 3 0 1 0', 'route BINARY 3 0 1 0', 'route Binary 3 0 1 0', 'route binary; id 3 0 1 0',
    'route binary 3 0 1 0 extra', 'route binary 3 0 1 0 1 0 1 0',
  ]) await rejected(bad);
});

test('hostile sizes cannot make it allocate or loop', async () => {
  await rejected('route binary 5000 100000');                                   // announces 100000 roads, sends none
  await rejected(`route binary 2 100000 ${'1 2 3 '.repeat(100001)} 1 0`);       // one more road than the cap allows is still bounded by m
  await rejected('dispatch binary 3 0 20001');
  await rejected('dispatch binary 3 0 1 1 1 2 1 600 0 20001');
  await rejected('bench 1 1 0 1 9 1 1 1 1 1 1 1 1 1');                          // list of 9 > cap 8
  await rejected('bench 1 99 0 1 0 0 0 1 merge');                               // repeats too high
  await rejected('bench 1 1 0 1 1 3001 0 0 1 dijkstra');                        // graph too large
  await rejected('bench 1 1 0 1 0 1 50001 0 1 merge');                          // parcels too large
});

test('very long tokens and binary junk', async () => {
  await rejected(`route ${'a'.repeat(100000)} 1 0 1 0`, /too long|heap/);
  await rejected(`${'z'.repeat(1_000_000)}`, /too long/);
  await rejected(Buffer.from([0, 1, 2, 255, 254, 253, 0, 0]).toString('latin1'));
  await rejected('route binary 3 0 1 \u0000 0');
  await rejected('‮\u0000route');
});

test('dispatch work cap rejects requests that would run for minutes', async () => {
  // 20000 parcels on a graph of 3000 hubs / 40000 roads would be ~860M units of work.
  const roads = Array.from({ length: 40000 }, (_, i) => `${(i % 2999) + 1} ${(i % 2999) + 2} 1`).join(' ');
  const parcels = Array.from({ length: 20000 }, (_, i) => `${i + 1} 1 2 3 600 ${i}`).join(' ');
  await rejected(`dispatch binary 3000 40000 ${roads} 20000 ${parcels} 0`, /workload too large/);
});

test('semantic errors from the engine come back as clean 2-exit errors', async () => {
  await rejected('dispatch binary 3 0 2  1 1 2 3 600 0  1 1 2 3 600 1  0', /duplicate parcel id/);
  await rejected('dispatch binary 3 0 1  1 1 2 3 600 0  1  9 1', /unknown parcel id/);
  await rejected('dispatch binary 3 0 1  1 1 2 2 600 0  1  1 3', /not an upgrade/);
  await rejected('dispatch binary 3 0 1  1 1 2 1 1440 0  0', /deadline/);
});

test('valid requests still work at the caps', async () => {
  const bigRoads = Array.from({ length: 5000 }, (_, i) => `${i + 1} ${((i + 1) % 5000) + 1} ${i % 7}`).join(' ');
  const r = await run(`route fibonacci 5000 5000 ${bigRoads} 1 2500\n`);
  assert.equal(r.code, 0);
  const j = JSON.parse(r.stdout);
  assert.equal(j.distances.length, 5000);
  assert.equal(j.reachable, true);
  const m = await run(`mst binomial 5000 5000 ${bigRoads} 1\n`);
  assert.equal(m.code, 0);
  assert.equal(JSON.parse(m.stdout).components, 1);
});

test('output is always a single JSON document', async () => {
  for (const input of ['route binary 3 1 1 2 5 1 3', 'mst binary 2 0 1', 'dispatch binary 2 0 0 0', 'nonsense']) {
    const r = await run(input);
    assert.doesNotThrow(() => JSON.parse(r.stdout), input);
    assert.equal(r.stdout.trim().split('\n').length, 1, input);
  }
});
