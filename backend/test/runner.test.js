import assert from 'node:assert/strict';
import { test } from 'node:test';
import { BusyError } from '../src/errors.js';
import { createRunner, RunnerError, runProcess } from '../src/runner.js';

const node = (script, extra = {}) => ({ command: process.execPath, args: ['-e', script], timeoutMs: 5000, maxOutputBytes: 100_000, ...extra });

test('passes stdin through and returns stdout and exit code', async () => {
  const r = await runProcess({ ...node("process.stdin.pipe(process.stdout)"), input: 'hello 42\n' });
  assert.equal(r.code, 0);
  assert.equal(r.stdout, 'hello 42\n');
});

test('reports a non-zero exit code and stderr without throwing', async () => {
  const r = await runProcess(node("console.error('boom'); process.exit(3)"));
  assert.equal(r.code, 3);
  assert.match(r.stderr, /boom/);
});

test('kills a process that runs too long', async () => {
  const started = Date.now();
  await assert.rejects(
    runProcess(node('setInterval(() => {}, 1000)', { timeoutMs: 300 })),
    (e) => e instanceof RunnerError && e.kind === 'timeout',
  );
  assert.ok(Date.now() - started < 3000);
});

test('kills a process that prints too much', async () => {
  await assert.rejects(
    runProcess(node("setInterval(() => process.stdout.write('x'.repeat(10000)), 1)", { maxOutputBytes: 50_000 })),
    (e) => e instanceof RunnerError && e.kind === 'output_limit',
  );
});

test('a missing program is reported as unavailable', async () => {
  await assert.rejects(
    runProcess({ command: '/definitely/not/here/parcelhub', timeoutMs: 1000, maxOutputBytes: 1000 }),
    (e) => e instanceof RunnerError && e.kind === 'unavailable',
  );
});

test('no shell: metacharacters in arguments stay literal', async () => {
  const r = await runProcess({
    command: process.execPath,
    args: ['-e', 'console.log(process.argv.slice(1).join("|"))', '; echo INJECTED && $(id) `id` > /tmp/pwned', '*'],
    timeoutMs: 5000, maxOutputBytes: 10_000,
  });
  assert.equal(r.stdout.trim(), '; echo INJECTED && $(id) `id` > /tmp/pwned|*');
});

test('does not leak the parent environment to the child', async () => {
  process.env.PARCELHUB_SECRET_FOR_TEST = 'top-secret';
  const r = await runProcess(node('console.log(process.env.PARCELHUB_SECRET_FOR_TEST ?? "unset")'));
  delete process.env.PARCELHUB_SECRET_FOR_TEST;
  assert.equal(r.stdout.trim(), 'unset');
});

test('a program that exits without reading its input does not crash the runner', async () => {
  const r = await runProcess({ ...node('process.exit(0)'), input: 'x'.repeat(5_000_000) });
  assert.equal(r.code, 0);
});

test('concurrency limit, bounded queue and busy rejection', async () => {
  const runner = createRunner({ ...node('setTimeout(() => console.log("ok"), 300)'), maxConcurrent: 2, maxQueue: 1 });
  const jobs = [runner.run(''), runner.run(''), runner.run('')];       // 2 running, 1 queued
  await new Promise((r) => setTimeout(r, 50));
  assert.deepEqual(runner.stats(), { active: 2, waiting: 1 });
  await assert.rejects(runner.run(''), BusyError);                      // queue full
  const results = await Promise.all(jobs);
  assert.ok(results.every((r) => r.stdout.trim() === 'ok'));
  assert.deepEqual(runner.stats(), { active: 0, waiting: 0 });
  assert.equal((await runner.run('')).code, 0);                         // usable again afterwards
});

test('slots are released after failures', async () => {
  const runner = createRunner({ ...node('setInterval(() => {}, 1000)', { timeoutMs: 100 }), maxConcurrent: 1, maxQueue: 0 });
  for (let i = 0; i < 3; i++) await assert.rejects(runner.run(''), RunnerError);
  assert.deepEqual(runner.stats(), { active: 0, waiting: 0 });
});
