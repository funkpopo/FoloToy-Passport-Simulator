import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { rgb565, nextPage, PIXEL_BYTES } from '../firmware/badge/web/badge-render.js';
import { createAppServer } from '../server.mjs';
test('RGB565 preserves SPI byte order and page buttons wrap correctly', () => {
  assert.deepEqual([...rgb565(new Uint8Array([255,0,0,255,0,255,0,255,0,0,255,255]))], [0xf8,0,7,0xe0,0,31]);
  assert.equal(nextPage(0, 'up'), 2); assert.equal(nextPage(2, 'down'), 0);
  assert.equal(nextPage(0, 'ok'), 1); assert.equal(nextPage(1, 'ok'), 2); assert.equal(nextPage(2, 'ok'), 1);
});
test('standalone badge page and demo API are removed', async t => {
  const server = createAppServer();
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  t.after(() => new Promise(resolve => server.close(resolve)));
  const base = `http://127.0.0.1:${server.address().port}`;
  for (const route of ['/badge.html', '/badge.js', '/badge.css', '/badge-render.js', '/api/badge/state', '/api/badge/connect']) assert.equal((await fetch(base + route)).status, 404, route);
});
test('firmware contains three different offline pages', async () => {
  const sample = await readFile(new URL('../firmware/badge/main/sample.rgb', import.meta.url));
  assert.equal(sample.length, PIXEL_BYTES);
  assert.notDeepEqual(sample.subarray(0, PIXEL_BYTES / 3), sample.subarray(PIXEL_BYTES / 3, 2 * PIXEL_BYTES / 3));
  assert.notDeepEqual(sample.subarray(PIXEL_BYTES / 3, 2 * PIXEL_BYTES / 3), sample.subarray(2 * PIXEL_BYTES / 3));
});
