// Measure complete original rich-text factories and the original SVG text caller; no replacement bodies.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
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
const api = load(path.join(upstream, 'packages/text/src/richText.ts'));
const cases = [];
const sourceDefaults = { autoSize: 'none', height: 100, text: '', verticalAlign: 'top', width: 100,
  background: false, backgroundColor: 0xffffffff, border: false, borderColor: 0xff,
  condenseWhite: false, maxChars: -1, mouseWheelEnabled: true, multiline: true,
  scrollH: 0, scrollV: 1, selectable: true, textColor: 0xff, wordWrap: false };
{
  const data = api.createRichTextData();
  for (const [key, value] of Object.entries(sourceDefaults)) assert.equal(data[key], value);
  assert.notEqual(data.textFormat, data.defaultTextFormat);
  assert.deepEqual(data.textFormat, {}); assert.deepEqual(data.defaultTextFormat, {});
  assert.deepEqual(data.textFormatRanges, []);
  cases.push({ name: 'original-defaults', defaults: sourceDefaults, distinctFormatOwners: true });
}
{
  const format = { size: 21 }, defaultFormat = { size: 19 };
  const supplied = { ...sourceDefaults, width: 310, height: 220, text: 'hello', textFormat: format,
    defaultTextFormat: defaultFormat, background: true, backgroundColor: 12, border: true, borderColor: 23,
    maxChars: 14, multiline: false, textColor: 34, wordWrap: true };
  const data = api.createRichTextData(supplied);
  for (const [key, value] of Object.entries(supplied)) assert.equal(data[key], value);
  assert.notEqual(data, supplied);
  cases.push({ name: 'original-supplied-options', freshData: true, width: data.width, height: data.height,
    text: data.text, formatOwnerRetained: data.textFormat === format && data.defaultTextFormat === defaultFormat });
}
{
  const order = [];
  api.createRichTextData(new Proxy({}, { get(_, key) { order.push(String(key)); return undefined; } }));
  const expected = ['autoSize', 'height', 'text', 'textFormat', 'verticalAlign', 'width',
    'background', 'backgroundColor', 'border', 'borderColor', 'height', 'width', 'condenseWhite',
    'defaultTextFormat', 'maxChars', 'mouseWheelEnabled', 'multiline', 'scrollH', 'scrollV',
    'selectable', 'textColor', 'textFormatRanges', 'wordWrap'];
  assert.deepEqual(order, expected);
  cases.push({ name: 'original-member-read-order', order });
}
{
  let heightReads = 0, widthReads = 0;
  const data = api.createRichTextData({ get height() { return 100 + ++heightReads; },
    get width() { return 200 + ++widthReads; } });
  assert.equal(heightReads, 2); assert.equal(widthReads, 2);
  assert.equal(data.height, 102); assert.equal(data.width, 202);
  cases.push({ name: 'changing-inherited-member-getters', heightReads, widthReads, height: data.height, width: data.width });
}
{
  const error = new RangeError('original background getter'), order = [];
  let caught;
  try { api.createRichTextData(new Proxy({}, { get(_, key) {
    order.push(String(key)); if (key === 'background') throw error; return undefined;
  } })); } catch (value) { caught = value; }
  assert.equal(caught, error);
  assert.deepEqual(order, ['autoSize', 'height', 'text', 'textFormat', 'verticalAlign', 'width', 'background']);
  cases.push({ name: 'getter-error-timing-and-identity', order, sameError: true, error: caught.name });
}
{
  const format = { size: 13 }, range = { end: 4, format, start: 1 }, ranges = [range];
  let reads = 0;
  const data = api.createRichTextData({ get textFormatRanges() { ++reads; return ranges; } });
  assert.equal(reads, 2); assert.notEqual(data.textFormatRanges, ranges);
  assert.notEqual(data.textFormatRanges[0], range); assert.equal(data.textFormatRanges[0].format, format);
  data.textFormatRanges[0].start = 2; assert.equal(range.start, 1);
  cases.push({ name: 'source-authored-range-map', reads, freshArray: true, freshRange: true, sameNestedFormat: true });
}
{
  const data = api.createRichTextData({ width: 0, height: null, multiline: false, text: '', maxChars: 0 });
  assert.equal(data.width, 0); assert.equal(data.height, 100); assert.equal(data.multiline, false);
  assert.equal(data.text, ''); assert.equal(data.maxChars, 0);
  cases.push({ name: 'only-nullish-options-default', width: data.width, height: data.height,
    multiline: data.multiline, text: data.text, maxChars: data.maxChars });
}
{
  const data = { text: 'node data', width: 302, backgroundColor: 35 };
  const node = api.createRichText({ alpha: 0.3, enabled: false, visible: false, name: 'public', data });
  assert.equal(node.kind, 'RichText'); assert.equal(node.alpha, 0.3); assert.equal(node.name, 'public');
  assert.equal(node.enabled, false); assert.equal(node.visible, false);
  assert.notEqual(node.data, data); assert.equal(node.data.text, data.text); assert.equal(node.data.width, 302);
  const runtime = api.getRichTextRuntime(node);
  assert.ok(runtime); assert.equal(api.getRichTextRuntime(node), runtime);
  assert.equal(runtime.selectionBeginIndex, 0); assert.equal(runtime.selectionEndIndex, 0);
  cases.push({ name: 'public-original-node-factory', kind: node.kind, alpha: node.alpha, nodeName: node.name,
    enabled: node.enabled, visible: node.visible, text: node.data.text, width: node.data.width, sameRuntimeOwner: true });
}
{
  const svg = load(path.join(upstream, 'packages/scene2d-formats/src/svgTextElement.ts'));
  const { defaultSvgStyle } = load(path.join(upstream, 'packages/scene2d-formats/src/svgStyle.ts'));
  const { createSvgRegistry } = load(path.join(upstream, 'packages/scene2d-formats/src/svgRegistry.ts'));
  const span = { name: 'tspan', attributes: { fill: '#ff0000' }, children: [], content: ['red'], text: 'red' };
  const element = { name: 'text', attributes: { id: 'svg-text' }, children: [span], content: ['black ', span], text: 'black red' };
  const context = { cssRules: [], diagnostics: [], elementsById: new Map(), gradientsById: new Map(),
    objectBoundingBoxes: new Map(), parentByElement: new Map([[span, element]]), registry: createSvgRegistry(),
    reportedUnsupportedElements: new Set(), resolvedDefinitionStyles: new Map(), resolvingClipUses: new Set(),
    resolvingClips: new Set(), resolvingGradients: new Set(), resolvingUses: new Set() };
  const node = svg.svgTextElementHandler({ element, parentStyle: defaultSvgStyle, import: context });
  assert.equal(node.kind, 'RichText'); assert.equal(node.name, 'svg-text');
  assert.equal(node.data.text, 'black red'); assert.equal(node.data.width, 10000); assert.equal(node.data.height, 20);
  assert.equal(node.data.autoSize, 'left'); assert.equal(node.data.textFormatRanges.length, 1);
  assert.equal(node.data.textFormatRanges[0].start, 6); assert.equal(node.data.textFormatRanges[0].end, 9);
  assert.equal(node.data.textFormatRanges[0].format.color, 0xff0000ff);
  cases.push({ name: 'whole-original-svg-text-rich-branch', kind: node.kind, nodeName: node.name,
    text: node.data.text, width: node.data.width, height: node.data.height, autoSize: node.data.autoSize,
    ranges: node.data.textFormatRanges.map(range => ({ start: range.start, end: range.end, color: range.format.color })) });
}
const pin = JSON.parse(fs.readFileSync(path.join(root, 'dependencies.lock.json')))
  .dependencies.find(dependency => dependency.name === 'flight').commit;
const files = Object.keys(hashes);
const blobs = execFileSync('git', ['-C', upstream, 'cat-file', '--batch'], {
  input: files.map(file => `${pin}:${file}\n`).join(''), maxBuffer: 128 * 1024 * 1024,
});
let offset = 0;
for (const file of files) {
  const end = blobs.indexOf(10, offset);
  const header = blobs.subarray(offset, end).toString('utf8').split(' ');
  assert.equal(header[1], 'blob', `missing raw-pin source: ${file}`);
  const size = Number(header[2]); offset = end + 1;
  assert.equal(hash(blobs.subarray(offset, offset + size)), hashes[file], `source differs from raw pin: ${file}`);
  offset += size + 1;
}
const result = { schema: 'flight-rich-text-factory-source-oracle/1', wholeOriginalModules: true,
  pin, rawPinSourcesVerified: files.length,
  replacementBodies: false, loadedModuleCount: cache.size, sourceHashes: hashes, cases: cases.length,
  failures: 0, observations: cases, nativeRichTextQualified: false, nativeSvgTextQualified: false };
fs.writeFileSync(path.resolve(args['--output']), JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify({ cases: cases.length, failures: 0, loadedModuleCount: cache.size }));
