import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { startServer, tmpDir } from './helpers.js';

const fake = { run: async () => ({ code: 0, stdout: '{}', stderr: '' }), stats: () => ({}) };

test('serves the built frontend when the folder exists, and keeps the API', async () => {
  const dist = tmpDir();
  fs.writeFileSync(path.join(dist, 'index.html'), '<!doctype html><title>ParcelHub</title>');
  const s = await startServer({ frontendDist: dist, apiBinary: 'x' }, { runner: fake, benchRunner: fake });
  try {
    const home = await fetch(s.url + '/');
    assert.equal(home.status, 200);
    assert.match(await home.text(), /<title>ParcelHub/);
    const missing = await fetch(s.url + '/api/nope');
    assert.equal(missing.status, 404);
    assert.equal((await missing.json()).error.code, 'not_found');
    const traversal = await fetch(s.url + '/..%2f..%2fetc%2fpasswd');
    assert.notEqual(traversal.status, 200);
  } finally { await s.close(); fs.rmSync(dist, { recursive: true, force: true }); }
});

test('without a built frontend, / is the JSON endpoint index', async () => {
  const s = await startServer({ frontendDist: path.join(tmpDir(), 'none'), apiBinary: 'x' }, { runner: fake, benchRunner: fake });
  try {
    const res = await fetch(s.url + '/');
    assert.equal((await res.json()).service, 'ParcelHub API');
  } finally { await s.close(); }
});
