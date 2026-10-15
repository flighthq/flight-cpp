import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const lock = JSON.parse(readFileSync(path.join(root, 'dependencies.lock.json'), 'utf8'));
const checkout = path.join(root, lock.directory, 'flight');
if (!existsSync(path.join(checkout, '.git'))) {
  process.stdout.write('Cube API oracle skipped: pinned Flight checkout is absent.\n');
  process.exit(0);
}
const revision = lock.dependencies.find((entry) => entry.name === 'flight').commit;
function pinned(file) {
  const result = spawnSync('git', ['show', `${revision}:${file}`], { cwd: checkout, encoding: 'utf8' });
  assert.equal(result.status, 0, result.stderr);
  return result.stdout;
}
// Reuse the parser already installed for the pinned compiler input; no runtime dependency is added.
const typescript = path.join(root, lock.directory, 'flight-compiler', 'node_modules', 'typescript', 'lib', 'typescript.js');
if (!existsSync(typescript)) {
  process.stdout.write('Cube API oracle skipped: the pinned compiler TypeScript parser is absent.\n');
  process.exit(0);
}
const ts = (await import(pathToFileURL(typescript).href)).default;
const declarations = [];
for (const [file, names] of [
  ['packages/entity/src/entity.ts', ['allocateEntity', 'finishEntity']],
  ['packages/geometry/src/vector2.ts', ['cloneVector2', 'copyVector2', 'createVector2', 'initializeVector2']],
  ['packages/texture/src/sampler.ts', ['cloneSampler', 'copySampler', 'createSampler', 'initializeSampler', 'equalsSampler']],
  ['packages/texture/src/texture.ts', ['cloneTexture', 'copyTexture', 'equalsTexture', 'equalsTextureContent', 'applyCommonTextureFields']],
  ['packages/texture/src/cubeTexture.ts', ['getCubeSources', 'cloneCubeTexture', 'copyCubeTexture', 'equalsCubeTexture', 'setCubeTextureFace']],
]) {
  const text = pinned(file);
  for (const name of names) {
    const declaration = text.match(new RegExp(`^(?:export )?function ${name}\\b[\\s\\S]*?^}`, 'm'))?.[0];
    assert.ok(declaration, `re-derive oracle when ${name} changes`);
    declarations.push(declaration.replace(/^export /u, ''));
  }
}
const key = pinned('packages/types/src/Entity.ts').match(/^export const EntityRuntimeKey = .+;$/mu)?.[0];
assert.ok(key);
const js = ts.transpileModule(`${key.replace(/^export /u, '')}\n${declarations.join('\n')}`, { compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.None } }).outputText;
const api = new Function(`${js}\nreturn {cloneCubeTexture,copyCubeTexture,equalsCubeTexture,setCubeTextureFace,createSampler,createVector2};`)();
const makeCube = () => ({
  dimension: 'cube', colorSpace: 'linear', flipX: true, flipY: false,
  sampler: api.createSampler({ anisotropy: 4 }), uvOffset: api.createVector2(0.25, 0.5),
  uvScale: api.createVector2(2, 3), uvRotation: 0.75, version: 7,
  sources: [{ width: 64 }, null, null, null, null, null],
});
const source = makeCube();
const copy = api.cloneCubeTexture(source);
assert.notEqual(copy, source);
for (const field of ['sampler', 'uvOffset', 'uvScale', 'sources']) assert.notEqual(copy[field], source[field]);
assert.equal(copy.sources[0], source.sources[0]);
assert.equal(api.equalsCubeTexture(source, copy), true);
for (const key of ['colorSpace', 'flipX', 'flipY', 'uvRotation', 'version']) {
  const old = copy[key];
  copy[key] = typeof old === 'boolean' ? !old : typeof old === 'number' ? old + 1 : 'srgb';
  assert.equal(api.equalsCubeTexture(source, copy), false);
  copy[key] = old;
}
for (const [owner, key] of [['uvOffset', 'x'], ['uvOffset', 'y'], ['uvScale', 'x'], ['uvScale', 'y'], ['sampler', 'anisotropy']]) {
  const old = copy[owner][key];
  copy[owner][key] = old + 1;
  assert.equal(api.equalsCubeTexture(source, copy), false);
  copy[owner][key] = old;
}
copy.sources[0] = { width: source.sources[0].width };
assert.equal(api.equalsCubeTexture(source, copy), false);
copy.sources[0] = source.sources[0];
const sampler = copy.sampler;
const oldFaces = copy.sources;
api.copyCubeTexture(copy, source);
assert.equal(copy.sampler, sampler);
assert.notEqual(copy.sources, oldFaces);
const selfFaces = source.sources;
api.copyCubeTexture(source, source);
assert.notEqual(source.sources, selfFaces);
source.version = 4294967297;
assert.equal(api.cloneCubeTexture(source).version, 1);
api.copyCubeTexture(copy, source);
assert.equal(copy.version, 1);
source.version = NaN;
assert.equal(api.equalsCubeTexture(source, source), true);
assert.equal(api.equalsCubeTexture(new Proxy(source, {}), source), false);
source.version = 7;
for (const absent of [null, undefined]) {
  assert.equal(api.equalsCubeTexture(source, absent), false);
  assert.equal(api.equalsCubeTexture(absent, source), false);
}
source.dimension = '2d';
const oldColor = copy.colorSpace;
assert.throws(() => api.copyCubeTexture(copy, source), { message: 'copyTexture requires matching dimensions' });
assert.equal(copy.colorSpace, oldColor);
source.dimension = 'cube';
copy.uvOffset = source.uvScale;
api.copyCubeTexture(copy, source);
assert.equal(source.uvScale.x, 0.25);
assert.equal(copy.uvScale.x, 0.25);

// These are successful pinned JavaScript writes, not a supported native setter implementation.
for (const index of [6, 8, -1, 0.5, NaN, Infinity]) {
  const cube = makeCube();
  cube.version = 0xffffffff;
  const original = cube.sources;
  const value = { width: 128 };
  api.setCubeTextureFace(cube, index, value);
  assert.equal(cube.sources, original);
  assert.equal(cube.sources[index], value);
  assert.equal(cube.version, 0);
  assert.equal(cube.sources.length, index === 6 ? 7 : index === 8 ? 9 : 6);
  if (index === 8) {
    assert.equal(6 in cube.sources, false);
    assert.equal(7 in cube.sources, false);
    assert.equal(cube.sources[6], undefined);
    assert.notEqual(cube.sources[6], null);
  }
  if (index < 0 || !Number.isInteger(index)) assert.ok(Object.hasOwn(cube.sources, String(index)));
  api.setCubeTextureFace(cube, index, value);
  assert.equal(cube.version, 0, 'identity-equal repeated setter does not advance version');
}
for (const index of [6, 8, -1, 0.5, NaN, Infinity]) {
  const cube = makeCube();
  cube.version = 0xffffffff;
  api.setCubeTextureFace(cube, index, null);
  assert.equal(cube.version, 0);
  assert.ok(Object.hasOwn(cube.sources, String(index)));
  assert.equal(cube.sources[index], null);
  if (index === 8) {
    assert.equal(6 in cube.sources, false);
    assert.equal(7 in cube.sources, false);
  }
  api.setCubeTextureFace(cube, index, null);
  assert.equal(cube.version, 0);
}
const nullNoop = makeCube();
nullNoop.version = 0xffffffff;
api.setCubeTextureFace(nullNoop, 1, null);
assert.equal(nullNoop.version, 0xffffffff);
process.stdout.write('Pinned cube clone/copy/equals ownership, alias/order, absence, exceptions and setter sparse/named-index domain checks pass.\n');
