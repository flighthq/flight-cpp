import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// Run untouched public parser bodies and their actual dependency bodies. Contract imports are
// resolved lazily through their original re-exports, so unrelated browser modules are not executed.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const equals = arg.indexOf('=');
  assert.ok(equals > 0, 'arguments use --name=value');
  return [arg.slice(0, equals), arg.slice(equals + 1)];
}));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'] ??
  path.join(root, '.dependencies/flight-compiler/node_modules/typescript/lib/typescript.js'))));
const packages = path.join(root, '.dependencies/flight/packages');
const parsedFiles = new Map(), modules = new Map(), origins = new Map();
function sourceFile(file) {
  if (!fs.existsSync(file)) file = file.replace(/\.js$/u, '.ts');
  assert.ok(fs.existsSync(file), 'missing original source: ' + file);
  return file;
}
function resolve(specifier, from) {
  return sourceFile(specifier.startsWith('@flighthq/')
    ? path.join(packages, specifier.slice('@flighthq/'.length).replace(/\/contract$/u, '/src/contract.ts'))
    : path.resolve(path.dirname(from), specifier));
}
function ast(file) {
  if (!parsedFiles.has(file)) parsedFiles.set(file,
    ts.createSourceFile(file, fs.readFileSync(file, 'utf8'), ts.ScriptTarget.Latest, true));
  return parsedFiles.get(file);
}
function origin(file, name, seen = new Set()) {
  const key = file + '#' + name;
  if (origins.has(key)) return origins.get(key);
  if (seen.has(key)) return undefined;
  seen = new Set(seen).add(key);
  for (const statement of ast(file).statements) {
    if (ts.isExportDeclaration(statement) && !statement.isTypeOnly) {
      if (statement.exportClause && ts.isNamedExports(statement.exportClause)) {
        for (const entry of statement.exportClause.elements) {
          if (entry.isTypeOnly || entry.name.text !== name) continue;
          const local = (entry.propertyName ?? entry.name).text;
          const result = statement.moduleSpecifier
            ? origin(resolve(statement.moduleSpecifier.text, file), local, seen) : { file, name };
          if (result) { origins.set(key, result); return result; }
        }
      } else if (!statement.exportClause && statement.moduleSpecifier) {
        const result = origin(resolve(statement.moduleSpecifier.text, file), name, seen);
        if (result) { origins.set(key, result); return result; }
      }
    }
    if (!statement.modifiers?.some(modifier => modifier.kind === ts.SyntaxKind.ExportKeyword)) continue;
    const names = ts.isVariableStatement(statement)
      ? statement.declarationList.declarations.map(declaration => declaration.name.getText(ast(file)))
      : statement.name ? [statement.name.text] : [];
    if (names.includes(name) && !ts.isInterfaceDeclaration(statement) && !ts.isTypeAliasDeclaration(statement)) {
      const result = { file, name }; origins.set(key, result); return result;
    }
  }
  return undefined;
}
function proxy(file) {
  return new Proxy({}, { get(_target, name) {
    if (name === '__esModule') return true;
    const target = origin(file, name);
    assert.ok(target, `missing original export ${String(name)} from ${file}`);
    return load(target.file)[target.name];
  } });
}
function load(file) {
  if (modules.has(file)) return modules.get(file).exports;
  const module = { exports: {} }; modules.set(file, module);
  const code = ts.transpileModule(fs.readFileSync(file, 'utf8'), {
    compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS },
  }).outputText;
  new Function('require', 'module', 'exports', code)(specifier => proxy(resolve(specifier, file)), module, module.exports);
  return module.exports;
}
const parserFile = path.join(packages, 'spritesheet-formats/src/asepriteParse.ts');
const api = load(parserFile);
let checks = 0;
const labels = [];
const check = (condition, message) => { assert.ok(condition, message); ++checks; labels.push(message); };
const sourceTests = [];
function expect(actual) {
  return {
    toBe(expected) { assert.equal(actual, expected); ++checks; },
    toEqual(expected) { assert.deepEqual(actual, expected); ++checks; },
    toHaveLength(length) { assert.equal(actual.length, length); ++checks; },
    toBeDefined() { assert.notEqual(actual, undefined); ++checks; },
    toBeUndefined() { assert.equal(actual, undefined); ++checks; },
    toBeNull() { assert.equal(actual, null); ++checks; },
    not: { toThrow() { assert.doesNotThrow(actual); ++checks; } },
  };
}
// The whole pinned source test file is executed, including its own fixtures. This harness supplies
// only test assertions; every production import is resolved to its real original source module.
const testsFile = path.join(packages, 'spritesheet-formats/src/asepriteParse.test.ts');
const testCode = ts.transpileModule(fs.readFileSync(testsFile, 'utf8'), {
  compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS },
}).outputText;
new Function('require', 'exports', 'describe', 'it', 'expect', testCode)(
  specifier => proxy(resolve(specifier, testsFile)), {}, (_name, run) => run(),
  (name, run) => { run(); sourceTests.push(name); }, expect);

function frame(filename = 'frame0', duration = 80) {
  return { filename, duration, frame: { x: 2, y: 3, w: 8, h: 9 }, rotated: false, trimmed: false,
    spriteSourceSize: { x: 0, y: 0, w: 8, h: 9 }, sourceSize: { w: 8, h: 9 }, extraFrame: { kept: true } };
}
function document(array = true) {
  return { frames: array ? [frame()] : { frame0: frame() },
    meta: { app: 'https://www.aseprite.org/', format: 'RGBA8888', image: 'sprite.png',
      version: '1.3', scale: '1', size: { w: 16, h: 16 }, frameTags: [],
      layers: [{ name: 'Layer', opacity: 255, blendMode: 'normal', extraLayer: 7 }],
      slices: [{ extraSlice: [null, { preserved: true }] }], extraMeta: 'retained' },
    extraRoot: { order: [3, 1, 2] }, explicitNull: null };
}
// Observe the actual built-in parse result, without replacing a parsed owner or any production body.
function capturedParse(input, configure = () => {}) {
  const original = JSON.parse;
  let parsedRoot, parseCalls = 0, result, error;
  const diagnostics = [];
  JSON.parse = function(...values) {
    ++parseCalls; parsedRoot = Reflect.apply(original, this, values); configure(parsedRoot); return parsedRoot;
  };
  try { result = api.parseAsepriteSpritesheetDocument(input, diagnostics); }
  catch (caught) { error = caught; }
  finally { JSON.parse = original; }
  check(parseCalls === 1, 'one built-in parse: ' + input.slice(0, 20));
  return { root: parsedRoot, result, error, diagnostics };
}
for (const array of [true, false]) {
  const input = JSON.stringify(document(array));
  const parsed = capturedParse(input);
  check(!parsed.error && parsed.diagnostics.length === 0, 'valid union arm maps without diagnostics');
  check(parsed.result.document === parsed.root, 'public document is exact built-in root');
  check(Array.isArray(parsed.result.document.frames) === array, 'array/hash distinction survives public result');
  const alias = parsed.result.document, meta = alias.meta, frames = alias.frames;
  const entry = array ? frames[0] : frames.frame0;
  check(entry.extraFrame.kept && meta.layers[0].extraLayer === 7 && meta.extraMeta === 'retained',
    'nested extra fields retained');
  check(alias.extraRoot.order.join(',') === '3,1,2' && alias.explicitNull === null && !('absent' in alias),
    'unknown root fields, null and absence remain distinct');
  check(meta.slices[0].extraSlice[1].preserved, 'unknown nested slice owner retained');
  entry.duration = 125; meta.image = 'changed.png';
  check(parsed.root.meta === meta && (array ? parsed.root.frames[0] : parsed.root.frames.frame0).duration === 125,
    'nested writes reach original raw owner');
  check(parsed.result.data.imageFile === 'sprite.png', 'mapped data remains authored snapshot after raw document write');
  alias.frames = array ? { later: frame('later') } : [frame('later')];
  check(Array.isArray(parsed.root.frames) === !array && alias.frames !== frames,
    'discriminator mutation changes later discrimination on same root');
  check((array ? frames[0] : frames.frame0) === entry, 'previously read nested value keeps its owner after replacement');
  check(JSON.parse(JSON.stringify(alias)).meta.image === 'changed.png', 'write visible to raw serialization');
  const again = capturedParse(input);
  check(again.result.document !== alias && again.result.document.meta !== meta, 'separate parses produce separate owners');
}
for (const array of [true, false]) {
  let reads = 0;
  const parsed = capturedParse(JSON.stringify(document(array)), value => {
    const frames = value.frames;
    Object.defineProperty(value, 'frames', { configurable: true, enumerable: true, get() { ++reads; return frames; } });
  });
  check(!parsed.error && reads === 4, 'original two consumers make four authored frames reads');
  const sentinel = {};
  const failure = capturedParse(JSON.stringify(document(array)), value => {
    Object.defineProperty(value, 'frames', { get() { throw sentinel; } });
  });
  check(failure.error === sentinel && failure.diagnostics.length === 0,
    'frames accessor throw propagates outside syntax-only catch with exact error owner');
  const mutated = capturedParse(JSON.stringify(document(array)), value => {
    value.frames = array ? { changedBeforeDiscrimination: frame('differentFilename') } : [frame('arrayAfterParse')];
  });
  check(!mutated.error && Array.isArray(mutated.result.document.frames) === !array &&
    mutated.result.data.frames[0].name === (array ? 'changedBeforeDiscrimination' : 'arrayAfterParse'),
    'mutation between parsing and authored discrimination chooses current frames shape');
}
for (const text of ['null', '0', 'false', '"text"', '[]', '{}', '{"frames":null}', '{"frames":[]}']) {
  const parsed = capturedParse(text);
  check(parsed.error instanceof TypeError && parsed.diagnostics.length === 0,
    'valid JSON root/schema error occurs at source use outside catch: ' + text);
}
for (const text of ['{bad', '', '[', '{"frames":']) {
  const parsed = capturedParse(text);
  check(!parsed.error && parsed.diagnostics.length === 1, 'syntax failure returns sentinel and one diagnostic');
  check(parsed.diagnostics[0].kind === 'spritesheet.aseprite.malformed-json' &&
    parsed.diagnostics[0].severity === 'Reject' && parsed.diagnostics[0].origin === 'parseAsepriteSpritesheetDocument',
    'syntax diagnostic shape and origin');
  check(Array.isArray(parsed.result.document.frames) && parsed.result.document.frames.length === 0,
    'syntax sentinel is actual empty array document');
}
const ordered = document(false);
ordered.frames = { tail: frame('tail'), 9: frame('nine'), 2: frame('two'), '01': frame('leading') };
const orderResult = capturedParse(JSON.stringify(ordered));
check(orderResult.result.data.frames.map(value => value.name).join(',') === '2,9,tail,01',
  'hash frame names follow integer-first then insertion ordering');
for (const frames of [true, 3, 'ab']) {
  const raw = document(); raw.frames = frames;
  const parsed = capturedParse(JSON.stringify(raw));
  check(!parsed.error && parsed.result.data.frames.length === 0 && parsed.result.document.frames === frames,
    'non-schema frames retained until authored Object.entries/property operations');
}
for (const frameTags of [undefined, null, [], { length: 0 }]) {
  const raw = document(); raw.meta.frameTags = frameTags;
  const parsed = capturedParse(JSON.stringify(raw));
  check(!parsed.error && parsed.result.data.animations.length === 0, 'source optional tag guard is not a schema validator');
}
const nullable = document(); nullable.meta.layers = null; delete nullable.meta.slices;
const nullableResult = capturedParse(JSON.stringify(nullable));
check(nullableResult.result.document.meta.layers === null && !('slices' in nullableResult.result.document.meta),
  'nested optional null remains distinct from missing');
const wrongScalar = document(); wrongScalar.frames[0].duration = '80';
wrongScalar.meta.frameTags = [{ name: 'tag', from: 0, to: 0, direction: 'forward' }];
const wrongScalarResult = capturedParse(JSON.stringify(wrongScalar));
check(wrongScalarResult.result.data.animations[0].frameDuration === '80',
  'source wrong native scalar remains raw; native escape must be explicit partial/error');

const sourceHashes = Object.fromEntries([...new Set([...modules.keys(), testsFile])].map(file =>
  [path.relative(packages, file), createHash('sha256').update(fs.readFileSync(file)).digest('hex')]));
const report = { checks, upstreamTests: sourceTests.length, sourceTests, controls: labels, sourceHashes,
  untouchedProductionBodies: true, lazyOriginalContractResolution: true,
  scope: 'Original public TypeScript consumer and source semantic controls only; no emitted native/public ABI or four-header qualification.' };
const output = path.resolve(args['--output'] ?? path.join(root, 'out/aseprite-json-document-source.json'));
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({ checks, upstreamTests: sourceTests.length, output, scope: report.scope }));
