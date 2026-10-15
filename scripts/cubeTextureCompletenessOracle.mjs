import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Compare against the exact pinned TypeScript body before its temporary source patches.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const lock = JSON.parse(readFileSync(path.join(root, 'dependencies.lock.json'), 'utf8'));
const pin = lock.dependencies.find((entry) => entry.name === 'flight');
const checkout = path.join(root, lock.directory, 'flight');
if (!existsSync(path.join(checkout, '.git'))) {
  process.stdout.write('Cube completeness oracle skipped: pinned Flight checkout is absent.\n');
  process.exit(0);
}
const source = spawnSync('git', ['show', `${pin.commit}:packages/texture/src/cubeTexture.ts`], {
  cwd: checkout, encoding: 'utf8',
});
assert.equal(source.status, 0, source.stderr);
const body = source.stdout.match(
  /export function isCubeTextureComplete\(cube: Readonly<CubeTexture>\): boolean \{([\s\S]*?)\n\}/u,
)?.[1];
assert.ok(body, 'the pinned completeness signature must be re-derived if it changes');
const complete = new Function('getCubeSources', 'cube', body).bind(null, (cube) => cube.sources);
const face = { width: 64 };
assert.equal(complete({ sources: Array(6).fill(null) }), false);
assert.equal(complete({ sources: Array(6).fill(face) }), true);
for (let missing = 0; missing < 6; ++missing) {
  const reads = [];
  const values = Array(6).fill(face);
  values[missing] = null;
  const sources = new Proxy(values, {
    get(target, key, receiver) {
      if (/^\d+$/u.test(String(key))) reads.push(Number(key));
      return Reflect.get(target, key, receiver);
    },
  });
  assert.equal(complete({ sources }), false);
  assert.deepEqual(reads, Array.from({ length: missing + 1 }, (_, index) => index));
}
assert.equal(complete({ sources: [] }), true);
assert.equal(complete({ sources: [face] }), true);
assert.equal(complete({ sources: [null] }), false);
assert.equal(complete({ sources: [...Array(6).fill(face), null] }), false);
// Undefined is not null. Native nullable face elements do not admit an in-range undefined value;
// an out-of-range native Array::get retains undefined in its independent outer optional.
assert.equal(complete({ sources: [undefined] }), true);
assert.equal([][0], undefined);
assert.notEqual([][0], null);
process.stdout.write('Pinned cube completeness: null/undefined, six missing-face positions, short arrays, extra elements and ordered short-circuit checks pass.\n');
