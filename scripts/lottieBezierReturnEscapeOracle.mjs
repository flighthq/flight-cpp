// Execute complete original modules; measure raw values beyond the Bezier callback return.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const at = arg.indexOf('='); assert.ok(at > 0); return [arg.slice(0, at), arg.slice(at + 1)];
}));
assert.ok(args['--typescript'] && args['--output']);
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'])));
const upstream = path.join(root, '.dependencies/flight');
const cache = new Map(), hashes = {};
const hash = text => createHash('sha256').update(text).digest('hex');
function load(input) {
  let file = path.resolve(input);
  if (!fs.existsSync(file) && fs.existsSync(file + '.ts')) file += '.ts';
  if (cache.has(file)) return cache.get(file).exports;
  const module = { exports: {} }; cache.set(file, module);
  const require = specifier => {
    if (specifier.startsWith('@flighthq/')) {
      const [name, ...sub] = specifier.slice('@flighthq/'.length).split('/');
      return load(path.join(upstream, 'packages', name, 'src', (sub.join('/') || 'index') + '.ts'));
    }
    return specifier.startsWith('.')
      ? load(path.resolve(path.dirname(file), specifier.replace(/\.js$/, '.ts')))
      : createRequire(file)(specifier);
  };
  const source = fs.readFileSync(file, 'utf8');
  hashes[path.relative(upstream, file)] = hash(source);
  const code = ts.transpileModule(source, {
    compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 },
  }).outputText;
  new Function('require', 'module', 'exports', '__filename', '__dirname', code)(require, module, module.exports, file, path.dirname(file));
  return module.exports;
}
const api = load(path.join(upstream, 'packages/scene2d-formats/src/lottieBezierPath.ts'));
const document = load(path.join(upstream, 'packages/scene2d-formats/src/lottieDocument.ts'));
const animation = load(path.join(upstream, 'packages/animation/src/animationTrack.ts'));
assert.equal(hashes['packages/scene2d-formats/src/lottieBezierPath.ts'], 'da3f167f812fc52463da4e6de9c9ae346373312f18c7c6f50f5f40c713115ce2');
const cases = [];
const shape = coordinate => ({ v: [[coordinate, 2]], i: [], o: [], c: false });
function trackFor(value) {
  const context = { frameOffset: 0, frameScale: 1, document: { ip: 0, fr: 1 } };
  return document.createLottieTrack([{ t: 0, s: value }], 2, context,
    input => api.flattenLottieShapePath(api.toLottieShapePath(input)), 0);
}
{
  const track = trackFor(shape('x')), sampled = [];
  animation.sampleAnimationTrack(sampled, track, 0);
  assert.deepEqual(Array.from(track.values), ['x', 2]);
  assert.deepEqual(sampled, ['x', 2]);
  cases.push({ name: 'original-track-callback-storage-and-sample', values: Array.from(track.values), sampled, types: sampled.map(value => typeof value) });
}
{
  const path = api.createLottieBezierPath(shape('x'));
  assert.deepEqual(path.data, ['x', 2]);
  cases.push({ name: 'original-path-data-return', data: path.data, types: path.data.map(value => typeof value) });
}
{
  const marker = { value: 1 }, source = shape(marker), track = trackFor(source), sampled = [];
  animation.sampleAnimationTrack(sampled, track, 0);
  assert.equal(track.values[0], marker); assert.equal(sampled[0], marker);
  marker.value = 9; assert.equal(sampled[0].value, 9);
  cases.push({ name: 'object-coordinate-through-original-callback', sameOwner: track.values[0] === marker && sampled[0] === marker, liveValue: sampled[0].value });
}
{
  const source = shape('x'), flat = api.flattenLottieShapePath(source);
  const restored = api.unflattenLottieShapePath(source, flat);
  assert.deepEqual(restored.v, [['x', 2]]);
  cases.push({ name: 'original-unflatten-result', vertices: restored.v });
}
{
  const order = [], sentinel = new RangeError('original i getter');
  const value = { get v() { order.push('v'); return 7; }, get i() { order.push('i'); throw sentinel; }, get o() { order.push('o'); return []; } };
  assert.equal(api.toLottieShapePath(value), value); assert.deepEqual(order, []);
  assert.throws(() => api.flattenLottieShapePath(value), error => error === sentinel);
  assert.deepEqual(order, ['v', 'i']);
  cases.push({ name: 'read-members-before-invalid-v-iteration', order, error: sentinel.name, sameError: true });
}
{
  const value = { v: '🙂', i: [], o: [] }, flat = api.flattenLottieShapePath(api.toLottieShapePath(value));
  assert.deepEqual(flat, ['\ud83d', '\ude42']);
  cases.push({ name: 'string-iterator-and-utf16-point-index', values: flat, codeUnits: flat.map(value => value.charCodeAt(0)) });
}
{
  const points = [], point = { get 0() { points.push([3, 4]); return 1; }, 1: 2 };
  points.push(point);
  const flat = api.flattenLottieShapePath({ v: points, i: [], o: [] });
  assert.deepEqual(flat, [1, 2, 3, 4]);
  cases.push({ name: 'live-array-iterator-after-point-getter', values: flat });
}
{
  const flat = api.flattenLottieShapePath({ v: [[false, null]], i: [], o: [] });
  assert.deepEqual(flat, [false, 0]);
  cases.push({ name: 'only-nullish-coordinates-default', values: flat, types: flat.map(value => typeof value) });
}
const report = { schema: 'flight-lottie-bezier-return-escape/1', wholeOriginalModules: true, replacementBodies: false,
  pin: 'a62784923f3be814463286c9fb28edfe6ab15789', loadedModuleCount: cache.size, sourceHashes: hashes,
  cases: cases.length, failures: 0, observations: cases, nativeBezierQualified: false,
  scope: 'Original source return and callback domain counterexamples only; no product ABI, compiler, runtime, SDK or native Bezier implementation.' };
const output = path.resolve(args['--output']); fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({ cases: report.cases, failures: 0, loadedModuleCount: cache.size, observations: cases }));
