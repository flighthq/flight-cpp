import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// Transpile untouched pinned bodies; only their package import locations change.
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const index = arg.indexOf('=');
  assert.ok(index > 0, 'expected --typescript=path and --output=path');
  return [arg.slice(0, index), arg.slice(index + 1)];
}));
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const compiler = path.resolve(args['--typescript'] ?? '.dependencies/flight-compiler/node_modules/typescript/lib/typescript.js');
const { default: ts } = await import(pathToFileURL(compiler));
const fixture = path.join(root, 'scripts/fixtures/particleCodecOptions.ts');
const program = ts.createProgram([fixture], {
  strict: true, noEmit: true, types: [], target: ts.ScriptTarget.ES2022,
  module: ts.ModuleKind.ESNext, skipLibCheck: true,
});
assert.deepEqual(ts.getPreEmitDiagnostics(program).map(d => ts.flattenDiagnosticMessageText(d.messageText, '\n')), []);
const pin = path.join(root, '.dependencies/flight/packages');
const sources = {
  starling: 'particles-formats/src/starlingPexParse.ts',
  particles: 'particles/src/particleEmitterConfig.ts',
  importdiagnostics: 'importdiagnostics/src/importDiagnosticCollector.ts',
  math: 'math/src/constants.ts',
  types: 'types/src/ImportDiagnostic.ts',
};
const scratch = fs.mkdtempSync(path.join(os.tmpdir(), 'particle-codec-options-'));
const hashes = {};
let checks = 0;
const check = (condition, message) => { assert.ok(condition, message); ++checks; };
try {
  for (const [name, file] of Object.entries(sources)) {
    const source = fs.readFileSync(path.join(pin, file), 'utf8');
    hashes[file] = createHash('sha256').update(source).digest('hex');
    let code = ts.transpileModule(source, {
      compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022 },
    }).outputText;
    for (const dependency of ['importdiagnostics', 'math', 'particles', 'types']) {
      code = code.replaceAll(`'@flighthq/${dependency}/contract'`, `'./${dependency}.mjs'`);
    }
    fs.writeFileSync(path.join(scratch, name + '.mjs'), code);
  }
  const api = await import(pathToFileURL(path.join(scratch, 'starling.mjs')));
  const codec = { parseToConfig: api.parseStarlingPex, parseToDocument: api.parseStarlingPexDocument };
  check(codec.parseToConfig === api.parseStarlingPex, 'config callback retains original function identity');
  check(codec.parseToDocument === api.parseStarlingPexDocument, 'document callback retains original function identity');
  const xml = '<particleEmitterConfig/>';
  let size = 2, reads = 0, producers = 0;
  const options = { pixelsPerUnit: 10, get textureSize() { ++reads; return size; } };
  const producer = () => { ++producers; return options; };
  check(codec.parseToConfig(xml, producer()).scaleMin === 16 && reads === 1 && producers === 1,
        'source-valid literal getter and argument producer each execute once');
  size = 4;
  check(codec.parseToConfig(xml, options).scaleMin === 8 && reads === 2, 'later source call reads changed field');
  check(codec.parseToConfig(xml).scaleMin === 32, 'omitted options default');
  check(codec.parseToConfig(xml, {}).scaleMin === 32, 'omitted optional field defaults');
  check(codec.parseToConfig(xml, { textureSize: undefined }).scaleMin === 32, 'present undefined defaults');
  const beforeInvalid = reads;
  assert.throws(() => codec.parseToConfig('bad', options), { message: 'Invalid Starling PEX: input must be an XML string beginning with <' });
  check(reads === beforeInvalid, 'invalid XML throws before reading options');
  const sentinel = {};
  let thrown;
  try { codec.parseToConfig(xml, { get textureSize() { throw sentinel; } }); }
  catch (error) { thrown = error; }
  check(thrown === sentinel, 'accessor throws the original owner');
  const result = codec.parseToDocument(xml, options);
  check(result.document.startParticleSize === 32 && result.config.scaleMin === 8,
        'actual original document result retains its extra document field');
  const narrowedCopy = { config: result.config, diagnostics: result.diagnostics };
  check(narrowedCopy !== result && !('document' in narrowedCopy), 'copying only declared result fields changes owner and observable surface');
  const wrapped = (...values) => api.parseStarlingPex(...values);
  check(wrapped !== codec.parseToConfig, 'a forwarding lambda is a different source function');
  const report = { strictTypeFixture: 'pass', checks, originalBodies: true, sourceHashes: hashes,
    scope: 'Original-source counterexamples and protocol controls only; no compiler adapter or built-in registry qualification.' };
  const output = path.resolve(args['--output'] ?? 'out/particle-codec-options-source.json');
  fs.mkdirSync(path.dirname(output), { recursive: true });
  fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n');
  console.log(JSON.stringify(report));
} finally {
  fs.rmSync(scratch, { recursive: true, force: true });
}
