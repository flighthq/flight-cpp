import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// Execute the actual original public contract and its original registry/parsers. No SDK generation.
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const at = arg.indexOf('='); assert.ok(at > 0, 'expected --typescript=path --output=path');
  return [arg.slice(0, at), arg.slice(at + 1)];
}));
assert.ok(args['--typescript'] && args['--output']);
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'])));
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const upstream = path.join(root, '.dependencies/flight/packages');
const fixture = path.join(root, 'scripts/fixtures/particlePublicConsumer.ts');
const scratch = fs.mkdtempSync(path.join(os.tmpdir(), 'particle-public-consumer-'));
const hashes = {}, cache = new Map(), counts = {};
const check = (group, condition, message) => { assert.ok(condition, message); counts[group] = (counts[group] ?? 0) + 1; };
const sourceOf = file => {
  const source = fs.readFileSync(file, 'utf8');
  if (file.startsWith(upstream + path.sep)) hashes[path.relative(upstream, file).split(path.sep).join('/')] = createHash('sha256').update(source).digest('hex');
  return source;
};
// Dependency contracts are relocated to re-exports of their actual original declarations/bodies.
// The particles-formats public contract itself is loaded in full, unchanged.
const exportsOf = {
  types: ['ImportDiagnostic', 'ParticleFormatKind', 'ParticleConfigParse', 'ParticleFormatCodec',
    'ParticleFormatDescriptor', 'ParticleEmitterConfig', 'ParticleCurve', 'ParticleSerializeResult',
    'NonEntityCreateResult', 'LibgdxParticleSchema', 'ParticleDesignerSchema', 'PixiParseResult',
    'SpineParticleSchema', 'StarlingPexSchema', 'UnitySchema'],
  particles: ['particleEmitterConfig', 'curve'],
  importdiagnostics: ['importDiagnosticCollector'],
  math: ['constants'],
};
const contracts = {};
for (const [name, files] of Object.entries(exportsOf)) {
  const contract = path.join(scratch, name + '.ts'); contracts['@flighthq/' + name + '/contract'] = contract;
  fs.writeFileSync(contract, files.map(file => `export * from ${JSON.stringify(path.join(upstream, name, 'src', file + '.ts'))};`).join('\n') + '\n');
}
contracts['@flighthq/particles-formats/contract'] = path.join(upstream, 'particles-formats/src/contract.ts');
const load = file => {
  file = path.resolve(file); if (cache.has(file)) return cache.get(file).exports;
  const module = { exports: {} }; cache.set(file, module);
  const code = ts.transpileModule(sourceOf(file), { compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 } }).outputText;
  const require = specifier => {
    const target = contracts[specifier] ?? (specifier.startsWith('.') || path.isAbsolute(specifier) ? path.resolve(path.dirname(file), specifier) : undefined);
    assert.ok(target, 'unmapped original import: ' + specifier + ' from ' + file); return load(target);
  };
  new Function('exports', 'require', 'module', code)(module.exports, require, module);
  return module.exports;
};
const clear = api => { for (const kind of api.getRegisteredParticleFormats()) api.unregisterParticleFormat(kind); };
try {
  const options = { strict: true, noEmit: true, types: [], skipLibCheck: true, allowImportingTsExtensions: true,
    target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ESNext, moduleResolution: ts.ModuleResolutionKind.Bundler,
    baseUrl: root, paths: Object.fromEntries(Object.entries(contracts).map(([name, file]) => [name, [file]])) };
  const program = ts.createProgram([fixture], options);
  const diagnostics = ts.getPreEmitDiagnostics(program).map(d => ({ file: d.file?.fileName, message: ts.flattenDiagnosticMessageText(d.messageText, '\n') }));
  assert.deepEqual(diagnostics, [], 'strict original declaration/consumer graph');
  const api = load(contracts['@flighthq/particles-formats/contract']);
  const consumer = load(fixture);
  check('installation', api.getRegisteredParticleFormats().length === 0, 'importing the complete public contract installs nothing');
  check('identity', consumer.namedCallbacksRetained(), 'all eight actual descriptor callbacks are original named functions');
  for (const options of [{}, { formats: undefined }, { formats: [] }]) {
    consumer.installOptions(options); check('installation', api.getRegisteredParticleFormats().length === 0, 'absent/undefined/empty formats installs nothing');
  }
  let formatReads = 0;
  api.applyParticleImportOptions({ get formats() { ++formatReads; return undefined; } });
  check('installation', formatReads === 1 && api.getRegisteredParticleFormats().length === 0, 'absent fallback reads original options once');
  consumer.install([api.unityParticleFormat, api.spineParticleFormat]);
  const overlap = '{"continuous":true,"startLifetime":{"mode":"constant"}}';
  check('installation', api.detectParticleFormat(overlap) === 'Unity', 'original overlapping detectors honor forward registration order');
  api.registerParticleFormat('Unity', api.spineParticleFormat.codec);
  check('installation', api.getParticleFormatCodec('Unity') === api.spineParticleFormat.codec && api.getRegisteredParticleFormats().join(',') === 'Unity,Spine', 'replacement keeps original codec owner and original key position');
  clear(api); consumer.install([api.spineParticleFormat, api.unityParticleFormat]);
  check('installation', api.detectParticleFormat(overlap) === 'Spine', 'original overlapping detectors honor reversed registration order');
  clear(api); api.registerBuiltInParticleFormats(); api.registerBuiltInParticleFormats();
  check('installation', api.getRegisteredParticleFormats().join(',') === 'Libgdx,StarlingPex,ParticleDesigner,Unity,Pixi,Spine', 'explicit built-in installation is idempotent and ordered');
  for (const descriptor of api.particleAllFormats) check('identity', api.getParticleFormatCodec(descriptor.kind) === descriptor.codec, descriptor.kind + ': registry retains exact original descriptor codec');
  check('identity', api.getParticleFormatCodec('Pixi').serialize === undefined, 'Pixi remains parse-only');
  for (const [descriptor, cases] of [
    [api.unityParticleFormat, [['bad', false], ['null', false], ['[]', false], ['{}', false], ['{"startLifetime":{"mode":"constant"}}', true], ['{"startLifetime":{"mode":3}}', false], ['{"gravityModifier":0}', true], ['{"looping":true,"startLifetime":null}', true]]],
    [api.pixiParticleFormat, [['bad', false], ['null', false], ['[]', false], ['{}', false], ['{"pos":{},"alpha":null}', false], ['{"pos":{},"alpha":{"start":1}}', true], ['{"pos":{},"alpha":{"end":0}}', true], ['{"alpha":{"start":1}}', false]]],
    [api.spineParticleFormat, [['bad', false], ['null', false], ['[]', false], ['{}', false], ['{"continuous":false}', true], ['{"emission":{"low":1,"high":2}}', true], ['{"life":{"low":null,"high":2}}', false], ['{"life":{"low":0,"high":0}}', true]]],
    [api.libgdxParticleFormat, [['', false], ['Particle Effect', true], [' \tParticle Effect \r\nmore', true], ['x\nParticle Effect', false], ['Particle EffectX', false]]],
    [api.particleDesignerParticleFormat, [['<plist><dict/></plist>', false], ['<plist><key>particleLifespanVariance</key></plist>', false], ['<plist><key>particleLifespan</key></plist>', true]]],
  ]) for (const [input, expected] of cases) check('detectors', descriptor.codec.detect(input) === expected, descriptor.kind + ': original detector ' + input);
  const families = [
    { kind: 'StarlingPex', config: 'parseStarlingPex', document: 'parseStarlingPexDocument', input: '<particleEmitterConfig/>', key: 'textureSize', field: 'scaleMin', expected: value => 32 / value, fallback: 1, invalid: 'bad', documentReadsBeforeInvalid: 0 },
    { kind: 'Unity', config: 'parseUnityParticle', document: 'parseUnityParticleDocument', input: '{"gravityModifier":0}', key: 'pixelsPerUnit', field: 'speedMin', expected: value => 5 * value, fallback: 100, invalid: 'bad', documentReadsBeforeInvalid: 1 },
    { kind: 'ParticleDesigner', config: 'parseParticleDesignerPlist', document: 'parseParticleDesignerPlistDocument', input: '<plist><dict><key>maxParticles</key><integer>100</integer></dict></plist>', key: 'textureSize', field: 'scaleMin', expected: value => 32 / value, fallback: 1 },
    { kind: 'Libgdx', config: 'parseLibgdxParticle', document: 'parseLibgdxParticleDocument', input: 'Particle Effect\nScale\nlowMin: 32\nlowMax: 32\nhighMin: 32\nhighMax: 32\n', key: 'textureSize', field: 'scaleMin', expected: value => 32 / value, fallback: 1, invalid: '', documentReadsBeforeInvalid: 0 },
  ];
  for (const family of families) {
    const codec = api.getParticleFormatCodec(family.kind);
    check('identity', codec.parseToConfig === api[family.config] && codec.parseToDocument === api[family.document], family.kind + ': both callbacks survive actual registry lookup');
    check('dispatch', api.detectParticleFormat(family.input) === family.kind, family.kind + ': real registered detector selects valid input');
    let value = 2, reads = 0, evaluations = 0;
    const options = { extra: 9, get [family.key]() { ++reads; return value; } };
    const produce = () => { ++evaluations; return options; };
    let config = consumer.publicConfig(family.input, produce());
    check('options', config[family.field] === family.expected(2) && reads === 1 && evaluations === 1, family.kind + ': public config evaluates owner/getter once');
    value = 4; let before = reads;
    const document = consumer.publicDocument(family.input, options);
    check('options', document.config[family.field] === family.expected(4) && document.format === family.kind && reads === before + 1, family.kind + ': public document observes live owner');
    before = reads; const registered = consumer.registeredDocument(family.input, family.kind, options);
    check('options', registered.config[family.field] === family.expected(4) && registered.format === family.kind && reads === before + 1, family.kind + ': direct registered route reads same owner once');
    for (const absent of [undefined, {}, { [family.key]: undefined }]) {
      check('options', consumer.publicConfig(family.input, absent)[family.field] === family.expected(family.fallback), family.kind + ': public config preserves omitted/undefined default');
      check('options', consumer.publicDocument(family.input, absent).config[family.field] === family.expected(family.fallback), family.kind + ': public document preserves omitted/undefined default');
    }
    const full = codec.parseToDocument(family.input, options), alias = full, owner = full.document;
    const replacement = api[family.config](family.input);
    alias.config = replacement;
    const diagnostics = []; alias.diagnostics = diagnostics;
    check('result', full === alias && full.config === replacement && full.document === owner && full.diagnostics === diagnostics, family.kind + ': actual callback result is writable and retains wider document');
    check('result', consumer.writeCallbackResult(family.input, family.kind, replacement), family.kind + ': ordinary source consumer writes the actual callback result');
    const sentinel = new Error(family.kind + ' options sentinel'); let thrown;
    try { codec.parseToConfig(family.input, { get [family.key]() { throw sentinel; } }); } catch (error) { thrown = error; }
    check('errors', thrown === sentinel, family.kind + ': actual stored config callback propagates original accessor error');
    thrown = undefined;
    try { codec.parseToDocument(family.input, { get [family.key]() { throw sentinel; } }); } catch (error) { thrown = error; }
    check('errors', thrown === sentinel, family.kind + ': actual stored document callback propagates original accessor error');
    const errorOptions = { get [family.key]() { throw sentinel; } };
    check('errors', consumer.publicConfig(family.input, errorOptions).maxParticles === api.parseParticleConfig('unknown').maxParticles, family.kind + ': public config catches the original callback error');
    for (const [entry, call] of [['parseParticleConfigDocument', () => consumer.publicDocument(family.input, errorOptions)], ['parseRegisteredParticleFormat', () => consumer.registeredDocument(family.input, family.kind, errorOptions)]]) {
      const result = call(), crumb = result.diagnostics.find(item => item.kind === 'particles.parse-error');
      check('errors', crumb?.severity === 'Reject' && crumb.origin === entry && crumb.detail?.message === sentinel.message && result.format === family.kind, family.kind + ': ' + entry + ' preserves diagnostic origin/message');
    }
    if (family.invalid !== undefined) {
      before = reads; const invalid = consumer.registeredDocument(family.invalid, family.kind, options);
      check('errors', invalid.diagnostics.some(item => item.kind === 'particles.parse-error') && reads === before + family.documentReadsBeforeInvalid, family.kind + ': direct registered invalid input preserves original option-read order');
      before = reads; const rejected = consumer.publicDocument(family.invalid, options);
      check('errors', rejected.format === null && rejected.diagnostics.some(item => item.kind === 'particles.unknown-format') && reads === before, family.kind + ': public detection rejects before options are read');
    }
  }
  clear(api);
  const unknown = consumer.registeredDocument('anything', 'missing.Custom');
  check('dispatch', unknown.format === 'missing.Custom' && unknown.diagnostics.some(item => item.kind === 'particles.unknown-format' && item.origin === 'parseRegisteredParticleFormat'), 'missing registered kind retains requested kind and diagnostic origin');
  check('installation', api.getRegisteredParticleFormats().length === 0, 'consumer leaves the original registry empty');
  const report = { strictOriginalConsumerGraph: 'pass', originalPublicContractAndBodies: true, checks: Object.values(counts).reduce((a, b) => a + b, 0), groups: counts, sourceHashes: hashes,
    nativeConsumer: { fixture: 'scripts/fixtures/particlePublicConsumer.cpp', sha256: createHash('sha256').update(fs.readFileSync(path.join(root, 'scripts/fixtures/particlePublicConsumer.cpp'))).digest('hex'), compiled: false, status: 'prepared; qualification pending actual shared Types and authorized particles output' }, normalSdk: false,
    scope: 'Actual original TypeScript public registration/detection/registry/callback/parser edge controls. No compiler emission, installed native callback/header, SDK or ledger claim.' };
  const output = path.resolve(args['--output']); fs.mkdirSync(path.dirname(output), { recursive: true }); fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n'); console.log(JSON.stringify(report));
} finally { fs.rmSync(scratch, { recursive: true, force: true }); }
