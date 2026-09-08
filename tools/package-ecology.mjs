import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile, writeFile, mkdir, copyFile } from 'node:fs/promises';
const root = new URL('../firmware/wireless-ecology/build/', import.meta.url);
const merged = await readFile(new URL('merged-binary.bin', root));
const app = await readFile(new URL('folotoy-wireless-ecology.bin', root));
assert.ok(app.length <= 0x300000 && merged.length <= 0x310000, 'image must stop before protected Flash regions');
assert.equal(merged[0], 0xe9); assert.equal(merged[0x10000], 0xe9);
assert.deepEqual(merged.subarray(0x10000, 0x10000 + app.length), app);
const partitions = new Map(); let end = 0x9000, md5 = false;
for (let offset = 0x8000; offset < 0x8c00; offset += 32) {
  const entry = merged.subarray(offset, offset + 32), magic = entry.readUInt16LE();
  if (magic === 0xebeb) { assert.deepEqual(entry.subarray(16), createHash('md5').update(merged.subarray(0x8000, offset)).digest()); md5 = true; break; }
  assert.equal(magic, 0x50aa);
  const name = entry.subarray(12, 28).toString('ascii').replace(/\0.*$/, '');
  const start = entry.readUInt32LE(4), size = entry.readUInt32LE(8);
  assert.ok(start >= end && start + size <= 0x800000, 'partitions must not overlap'); end = start + size;
  partitions.set(name, [start, size]);
}
assert.ok(md5);
assert.deepEqual(partitions.get('factory'), [0x10000, 0x300000]);
assert.deepEqual(partitions.get('cardid'), [0x356000, 0x4000]);
assert.deepEqual(partitions.get('recovery'), [0x700000, 0x100000]);
assert.equal(partitions.size, 5);
const output = new URL('../public/assets/firmware/wireless-ecology.bin', import.meta.url);
await writeFile(output, merged);
const release = new URL('../artifacts/wireless-ecology/', import.meta.url); await mkdir(release, { recursive: true });
for (const [source, target] of [['bootloader/bootloader.bin','bootloader.bin'], ['partition_table/partition-table.bin','partition-table.bin'], ['folotoy-wireless-ecology.bin','application.bin'], ['merged-binary.bin','wireless-ecology.bin']]) await copyFile(new URL(source, root), new URL(target, release));
await copyFile(new URL('../firmware/wireless-ecology/README.zh_CN.md', import.meta.url), new URL('README.zh_CN.md', release));
await copyFile(new URL('../firmware/wireless-ecology/README.md', import.meta.url), new URL('README.md', release));
await copyFile(new URL('../firmware/wireless-ecology/components/bsp/LICENSE', import.meta.url), new URL('LICENSE', release));
await writeFile(new URL('manifest.json', release), JSON.stringify({ idf: '5.5.3', target: 'esp32c3', upstream: 'df3990726e3751fadaaaa703a480dbba6e13c61b', appBytes: app.length, mergedBytes: merged.length, sha256: createHash('sha256').update(merged).digest('hex'), offsets: { 'bootloader.bin': '0x0', 'partition-table.bin': '0x8000', 'application.bin': '0x10000' } }, null, 2) + '\n');
console.log(`Firmware PASS: ${app.length} byte app, ${merged.length} byte merged image; partition MD5, protected layout and image contents verified. Packaged in artifacts/wireless-ecology/.`);

