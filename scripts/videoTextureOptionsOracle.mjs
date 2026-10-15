import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const lock = JSON.parse(readFileSync(path.join(root, 'dependencies.lock.json'), 'utf8'));
const checkout = path.join(root, lock.directory, 'flight');
const parser = path.join(root, lock.directory, 'flight-compiler', 'node_modules/typescript/lib/typescript.js');
if (!existsSync(path.join(checkout, '.git')) || !existsSync(parser)) {
  process.stdout.write('Video options oracle skipped: pinned checkout/compiler parser is absent.\n');
  process.exit(0);
}
const revision = lock.dependencies.find((entry) => entry.name === 'flight').commit;
const ts = (await import(pathToFileURL(parser).href)).default;
function pinned(file) {
  const result = spawnSync('git', ['show', `${revision}:${file}`], { cwd: checkout, encoding: 'utf8' });
  assert.equal(result.status, 0, result.stderr);
  return result.stdout;
}
const declarations = [];
for (const [file, names] of [
  ['packages/entity/src/entity.ts', ['allocateEntity', 'finishEntity']],
  ['packages/geometry/src/vector2.ts', ['cloneVector2', 'createVector2', 'initializeVector2']],
  ['packages/texture/src/sampler.ts', ['cloneSampler', 'createSampler', 'initializeSampler']],
  ['packages/texture/src/texture.ts', ['createTexture', 'createTexture2D', 'initializeTexture2D', 'createCommonTextureFields', 'applyCommonTextureFields', 'attachTextureToResource']],
  ['packages/texture/src/videoTexture.ts', ['createVideoTexture', 'createVideoImageResource', 'initializeVideoImageResource', 'updateVideoImageSize']],
]) {
  const text = pinned(file);
  const parsed = ts.createSourceFile(file, text, ts.ScriptTarget.Latest, true);
  for (const name of names) {
    const implementations = parsed.statements.filter((node) => ts.isFunctionDeclaration(node) && node.name?.text === name && node.body);
    assert.equal(implementations.length, 1, `re-derive oracle when ${name} implementation changes`);
    declarations.push(implementations[0].getText(parsed).replace(/^export /u, ''));
  }
}
const key = pinned('packages/types/src/Entity.ts').match(/^export const EntityRuntimeKey = .+;$/mu)?.[0];
const version = pinned('packages/texture/src/videoTexture.ts').match(/^const INITIAL_VIDEO_VERSION = .+;$/mu)?.[0];
const kind = pinned('packages/types/src/TextureSourceKind.ts').match(/^export const ImageTextureSourceKind = .+;$/mu)?.[0];
assert.ok(key && version && kind);
const source = `${key.replace(/^export /u, '')}\n${kind.replace(/^export /u, '')}\n${version}\n${declarations.join('\n')}`;
const js = ts.transpileModule(source, { compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.None } }).outputText;
const api = new Function(`${js}\nreturn {createVideoTexture,createSampler,createVector2,EntityRuntimeKey};`)();
const element = { borrowed: true };
const resource = { element };
const common = () => ({
  colorSpace: 'linear', flipX: true, flipY: false, version: 17,
  sampler: api.createSampler({ anisotropy: 7 }), uvOffset: api.createVector2(0.25, 0.5),
  uvScale: api.createVector2(2, 3), uvRotation: 0.75,
});
let widthCalls = 0, heightCalls = 0;
const host = {
  getWidth(received) { ++widthCalls; assert.equal(received, element); return 23; },
  getHeight(received) { ++heightCalls; assert.equal(received, element); return 11; },
};
for (const dimension of ['2d', 'cube', '2d-array', '3d']) {
  const input = { ...common(), dimension, ...(dimension === 'cube' || dimension === '2d-array' ? { sources: [null] } : { source: { old: true } }) };
  const oldSource = input.source, oldSources = input.sources;
  const out = api.createVideoTexture(host, resource, input);
  assert.equal(out.dimension, '2d');
  assert.equal(out.version, 0xffffffff);
  assert.equal(out.source.version, 0xffffffff);
  assert.equal(out.source.source, element);
  assert.equal(out.source.width, 23); assert.equal(out.source.height, 11);
  assert.equal(out.colorSpace, input.colorSpace); assert.equal(out.flipX, true); assert.equal(out.flipY, false);
  assert.equal(out.uvRotation, 0.75);
  for (const name of ['sampler', 'uvOffset', 'uvScale']) assert.notEqual(out[name], input[name]);
  assert.equal(out.sampler.anisotropy, 7); assert.equal(out.uvOffset.x, 0.25); assert.equal(out.uvScale.y, 3);
  assert.equal(input.dimension, dimension); assert.equal(input.version, 17);
  assert.equal(input.source, oldSource); assert.equal(input.sources, oldSources);
}
assert.equal(widthCalls, 4); assert.equal(heightCalls, 4);
const partial = api.createVideoTexture(host, resource, { colorSpace: '', flipX: false, uvRotation: 0, version: 123 });
assert.equal(partial.colorSpace, ''); assert.equal(partial.flipX, false); assert.equal(partial.uvRotation, 0);
assert.equal(partial.version, 0xffffffff); assert.equal(partial.uvOffset.x, 0); assert.equal(partial.uvScale.x, 1);
const defaults = api.createVideoTexture({}, resource);
assert.equal(defaults.source.width, 0); assert.equal(defaults.source.height, 0);
assert.equal(defaults.colorSpace, 'srgb'); assert.equal(defaults.sampler.mipmaps, true);
const beforeNull = [widthCalls, heightCalls];
const absent = api.createVideoTexture(host, { element: null }, { colorSpace: '' });
assert.equal(absent.source, null); assert.equal(absent.version, 0xffffffff); assert.equal(absent.colorSpace, '');
assert.deepEqual([widthCalls, heightCalls], beforeNull);

const trace = [];
let color = 'before', optionThrows = false;
const uv = api.createVector2(0.5, 0.75);
const values = { version: 123, sources: [null], dimension: 'cube', uvOffset: uv, colorSpace: 'before',
  sampler: api.createSampler(), uvScale: uv, uvRotation: 0, flipX: false, flipY: true };
const options = {};
for (const [name, value] of Object.entries(values)) {
  Object.defineProperty(options, name, { enumerable: true, get() {
    trace.push(name);
    if (name === 'uvOffset' && optionThrows) throw new Error('option read failed');
    return name === 'colorSpace' ? color : value;
  } });
}
Object.defineProperty(options, api.EntityRuntimeKey, { enumerable: true, get() { trace.push('entityRuntime'); return undefined; } });
const orderedHost = {
  getWidth() {
    trace.push('width'); color = 'after';
    orderedHost.getHeight = () => { trace.push('height'); return 19; };
    return 17;
  },
  getHeight() { trace.push('old-height'); return -1; },
};
const ordered = api.createVideoTexture(orderedHost, resource, options);
assert.deepEqual(trace, ['width', 'height', ...Object.keys(values), 'entityRuntime']);
assert.equal(ordered.colorSpace, 'after'); assert.equal(ordered.source.height, 19);
assert.notEqual(ordered.uvOffset, uv); assert.notEqual(ordered.uvScale, uv); assert.notEqual(ordered.uvOffset, ordered.uvScale);
trace.length = 0; optionThrows = true;
assert.throws(() => api.createVideoTexture(orderedHost, resource, options), { message: 'option read failed' });
assert.deepEqual(trace, ['width', 'height', 'version', 'sources', 'dimension', 'uvOffset']);
trace.length = 0; optionThrows = false;
orderedHost.getWidth = () => { trace.push('width'); throw new Error('host width failed'); };
assert.throws(() => api.createVideoTexture(orderedHost, resource, options), { message: 'host width failed' });
assert.deepEqual(trace, ['width']);
process.stdout.write('Pinned video constructor: four partial arms, readonly callback routing, defaults/overwrites, source and nested-owner identity, string/symbol spread order, callback replacement and exception ordering pass.\n');
