import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';

const root = fileURLToPath(new URL('..', import.meta.url));
const arg = (name, fallback) => process.argv.find(value => value.startsWith(`--${name}=`))?.slice(name.length + 3) ?? fallback;
const raw = path.resolve(arg('raw', path.join(root, '.dependencies/flight')));
const patched = path.resolve(arg('patched', raw));
const compiler = path.resolve(arg('compiler', path.join(root, '.dependencies/flight-compiler')));
const output = path.resolve(arg('output', path.join(root, 'out/texture-getter-order-oracle.json')));
const pin = 'a62784923f3be814463286c9fb28edfe6ab15789';
const relative = 'packages/texture/src/texture.ts';
const original = spawnSync('git', ['show', `${pin}:${relative}`], { cwd: raw, encoding: 'utf8' });
if (original.status || original.stdout !== fs.readFileSync(path.join(raw, relative), 'utf8')) {
  throw new Error('Raw texture source must be the exact pinned original');
}
const ts = (await import(pathToFileURL(path.join(compiler, 'node_modules/typescript/lib/typescript.js')).href)).default;
function loader(directory) {
  const cache = new Map();
  const load = input => {
    let file = path.resolve(input);
    if (!fs.existsSync(file) && fs.existsSync(`${file}.ts`)) file += '.ts';
    if (cache.has(file)) return cache.get(file).exports;
    const module = { exports: {} };
    cache.set(file, module);
    const require = specifier => {
      if (specifier.startsWith('@flighthq/')) {
        const [name, ...sub] = specifier.slice('@flighthq/'.length).split('/');
        return load(path.join(directory, 'packages', name, 'src', `${sub.join('/') || 'index'}.ts`));
      }
      if (specifier.startsWith('.')) return load(path.resolve(path.dirname(file), specifier.replace(/\.js$/, '.ts')));
      return createRequire(file)(specifier);
    };
    const text = ts.transpileModule(fs.readFileSync(file, 'utf8'), {
      compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 },
    }).outputText;
    new Function('require', 'module', 'exports', '__filename', '__dirname', text)(require, module, module.exports, file, path.dirname(file));
    return module.exports;
  };
  return { api: load(path.join(directory, relative)), sampler: load(path.join(directory, 'packages/texture/src/sampler.ts')), cache };
}
const sources = [loader(raw), loader(patched)];
function run(source, dimension, mode, length = 6) {
  const reads = [], writes = [];
  const first = source.sampler.createSampler(); first.anisotropy = 3;
  const second = source.sampler.createSampler(); second.anisotropy = 7;
  const image = { kind: 'image', width: 32, height: 16 };
  const voxel = { kind: 'voxel', width: 4, height: 4, depth: 4 };
  const offset = { x: .25, y: .5 }, scale = { x: 2, y: 3 };
  const faces = Array.from({ length }, (_, index) => index % 2 ? image : null);
  if (length > 2) delete faces[2];
  const resource = {};
  const target = { dimension, colorSpace: 'linear', flipX: true, flipY: false, sampler: first,
    uvOffset: offset, uvRotation: .75, uvScale: scale, version: 4294967297,
    source: dimension === '3d' ? voxel : image, sources: faces, resource };
  const counts = {};
  const proxy = new Proxy(target, {
    get(object, key) {
      const name = String(key); reads.push(name); counts[name] = (counts[name] ?? 0) + 1;
      if (mode === 'authored-throw' && key === 'colorSpace') throw new RangeError('original getter');
      if (mode === 'dimension-throw' && key === 'dimension') throw new RangeError('original dimension');
      if (mode === 'content-throw' && (key === 'source' || key === 'sources')) throw new RangeError('original content');
      if (key === 'sampler') {
        if (counts[name] === 2 && mode === 'sampler-undefined') return undefined;
        if (counts[name] === 2 && mode === 'sampler-null') return null;
        return counts[name] === 1 ? first : second;
      }
      if (key === 'uvOffset') {
        if (counts[name] === 2 && mode === 'offset-undefined') return undefined;
        second.anisotropy = 99;
      }
      if (key === 'uvScale' && counts[name] === 2 && mode === 'scale-null') return null;
      if (key === 'uvRotation') offset.x = 9;
      if (key === 'dimension') target.colorSpace = 'after-dimension';
      return object[key];
    },
    set(object, key, value) { writes.push(String(key)); object[key] = value; return true; },
  });
  let result;
  try {
    const out = source.api.createTexture(proxy);
    result = { dimension: out.dimension, colorSpace: out.colorSpace, flipX: out.flipX,
      sampler: out.sampler.anisotropy, offsetX: out.uvOffset.x, version: out.version,
      sourceIdentity: out.source === (dimension === '3d' ? voxel : image),
      sources: out.sources ? Array.from({ length: out.sources.length }, (_, index) =>
        !(index in out.sources) ? 'hole' : out.sources[index] === image ? 'image' : out.sources[index] === null ? 'null' : 'undefined') : null,
      newSourceList: out.sources ? out.sources !== faces : null,
      resourceCount: resource.textures?.length ?? 0,
      resourceIdentity: resource.textures?.[0] === out };
  } catch (error) { result = { error: error.name, message: error.message }; }
  return { reads, writes, result };
}
const cases = [];
for (const dimension of ['2d', undefined, '2d-array', '3d', 'cube']) {
  for (const mode of ['changing', 'sampler-undefined', 'sampler-null', 'offset-undefined', 'scale-null',
    'authored-throw', 'dimension-throw', 'content-throw']) {
    const pair = sources.map(source => run(source, dimension, mode));
    cases.push({ dimension: dimension ?? '(default)', mode, raw: pair[0], patched: pair[1], equal: JSON.stringify(pair[0]) === JSON.stringify(pair[1]) });
  }
}
for (const length of [0, 1, 3, 8]) {
  const pair = sources.map(source => run(source, 'cube', 'changing', length));
  cases.push({ dimension: 'cube', mode: `holes-and-length-${length}`, raw: pair[0], patched: pair[1], equal: JSON.stringify(pair[0]) === JSON.stringify(pair[1]) });
}
for (const value of [undefined, null, false, 0, '', {}]) {
  const pair = sources.map(source => {
    try { const out = source.api.createTexture(value); return { dimension: out.dimension, color: out.colorSpace, source: out.source, version: out.version }; }
    catch (error) { return { error: error.name, message: error.message }; }
  });
  cases.push({ mode: `default-${value === undefined ? 'undefined' : JSON.stringify(value)}`, raw: pair[0], patched: pair[1], equal: JSON.stringify(pair[0]) === JSON.stringify(pair[1]) });
}
const hash = directory => createHash('sha256').update(fs.readFileSync(path.join(directory, relative))).digest('hex');
const result = { schema: 'flight-texture-original-getter-order/1', rawPin: pin, rawSourceSha256: hash(raw), patchedSourceSha256: hash(patched),
  wholeOriginalModules: true, replacementBodies: false, loadedModuleCounts: sources.map(source => source.cache.size),
  summary: { cases: cases.length, pass: cases.filter(item => item.equal).length, fail: cases.filter(item => !item.equal).length }, cases };
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, `${JSON.stringify(result, null, 2)}\n`);
console.log(JSON.stringify(result.summary));
if (result.summary.fail) { console.error(JSON.stringify(cases.filter(item => !item.equal), null, 2)); process.exitCode = 1; }
