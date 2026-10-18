import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// Reproduce a retained boundary and test existing runtime storage, without implementing an adapter.
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const at = arg.indexOf('='); assert.ok(at > 0, 'expected explicit --compiler --typescript --cxx --output paths');
  return [arg.slice(0, at), arg.slice(at + 1)];
}));
assert.ok(args['--compiler'] && args['--typescript'] && args['--cxx']);
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const backendUrl = pathToFileURL(path.resolve(args['--compiler']));
const { emitIrModuleCpp } = await import(backendUrl);
const { lowerTypeScriptSource } = await import(new URL('../../compiler-semantic/src/index.js', backendUrl));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'])));
const filename = path.join(root, 'scripts/fixtures/particleRegistrationJsonBoundary.ts');
const source = fs.readFileSync(filename, 'utf8');
const upstream = fs.readFileSync(path.join(root, '.dependencies/flight/packages/particles-formats/src/registerBuiltInParticleFormats.ts'), 'utf8');
const original = ts.createSourceFile('register.ts', upstream, ts.ScriptTarget.Latest, true);
const helper = original.statements.find(node => ts.isFunctionDeclaration(node) && node.name?.text === 'parseJsonObject');
assert.ok(helper && source.includes(helper.getText(original)), 'counterexample must retain the exact untouched private helper');
const program = ts.createProgram([filename], { strict: true, noEmit: true, types: [], target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ESNext, skipLibCheck: true });
assert.deepEqual(ts.getPreEmitDiagnostics(program).map(d => ts.flattenDiagnosticMessageText(d.messageText, '\n')), []);
const exports = {};
new Function('exports', ts.transpileModule(source, { compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 } }).outputText)(exports);
const cases = [['null', false], ['0', false], ['true', false], ['"text"', false], ['[]', false], ['{}', true], ['{"marker":3}', true], ['{"marker":null}', true], ['bad', false]];
for (const [input, expected] of cases) assert.deepEqual(exports.acceptsObject(input), expected);
const detectorSource = fs.readFileSync(path.join(root, 'scripts/fixtures/particleRegistrationLibgdxDetector.ts'), 'utf8');
const detectorExports = {};
new Function('exports', ts.transpileModule(detectorSource, { compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 } }).outputText)(detectorExports);
const detectorCases = [['', false], ['Particle Effect', true], [' \tParticle Effect \r\nmore', true], ['x\nParticle Effect', false], ['#comment\nParticle Effect', false], ['Particle EffectX', false], ['\r\nParticle Effect', true]];
for (const [input, expected] of detectorCases) assert.equal(detectorExports.detect(input), expected);
const detector = lowerTypeScriptSource(ts.createSourceFile('/flight/packages/example/src/detector.ts', detectorSource, ts.ScriptTarget.Latest, true), { packageName: '@flighthq/example', upstreamDirectory: '/flight' });
assert.deepEqual(detector.diagnostics, []);
let detectorRefusal;
try { emitIrModuleCpp(detector.module, { runtimeProfile: 'flight-cpp' }); }
catch (error) { assert.equal(error.kind, 'backend-emission'); detectorRefusal = { rule: error.rule, message: error.message }; }
assert.ok(detectorRefusal, 'retained optional trim detector boundary unexpectedly disappeared');
const scratch = fs.mkdtempSync(path.join(os.tmpdir(), 'particle-registration-boundary-'));
try {
  const lowered = lowerTypeScriptSource(ts.createSourceFile('/flight/packages/example/src/boundary.ts', source, ts.ScriptTarget.Latest, true), { packageName: '@flighthq/example', upstreamDirectory: '/flight' });
  assert.deepEqual(lowered.diagnostics, []);
  const emitted = emitIrModuleCpp(lowered.module, { runtimeProfile: 'flight-cpp' }).contents;
  assert.ok(emitted.includes('std::optional<flight::Record<flight::String, flight::Any>> parse_json_object'));
  assert.ok(emitted.includes('flight::named_properties(value)'));
  const includes = ['-I' + path.join(root, 'overrides/include'), '-I' + path.join(root, 'include'), '-I' + path.join(root, 'generated/include')];
  fs.writeFileSync(path.join(scratch, 'counterexample.cpp'), emitted + '\nint main(){return 0;}\n');
  const compile = spawnSync(path.resolve(args['--cxx']), ['-std=c++20', '-ffp-contract=off', ...includes, '-fsyntax-only', path.join(scratch, 'counterexample.cpp')], { encoding: 'utf8' });
  assert.notEqual(compile.status, 0, 'the retained Record/NamedProperties boundary unexpectedly disappeared');
  assert.match(compile.stderr, /error: no matching constructor[^\n]*std::optional<flight::Record/);
  assert.match(compile.stderr, /(?:NamedProperties|named_properties)/);
  const nativeFile = path.join(root, 'scripts/fixtures/particleRegistrationJsonViewControl.cpp');
  const binary = path.join(scratch, 'view-control');
  const nativeBuild = spawnSync(path.resolve(args['--cxx']), ['-std=c++20', '-ffp-contract=off', ...includes, nativeFile, '-o', binary], { encoding: 'utf8' });
  assert.equal(nativeBuild.status, 0, nativeBuild.stderr || nativeBuild.stdout);
  const nativeRun = spawnSync(binary, [], { encoding: 'utf8' });
  assert.equal(nativeRun.status, 0, nativeRun.stderr || nativeRun.stdout);
  const report = { sourceChecks: cases.length, detectorSourceChecks: detectorCases.length, detectorRefusal, exactOriginalPrivateHelper: true,
    helperSha256: createHash('sha256').update(helper.getText(original)).digest('hex'),
    compilerEmittedCounterexample: { nativeBuild: 'fail', retainedBoundaryReproduced: true, diagnostic: compile.stderr },
    existingRuntimeViewControl: 'pass', productionChange: false,
    scope: 'Minimal private-helper counterexample and existing runtime storage control only. No original registration/public parser execution, ABI change, new adapter, SDK or header promotion.' };
  const output = path.resolve(args['--output'] ?? 'out/particle-registration-boundary.json');
  fs.mkdirSync(path.dirname(output), { recursive: true });
  fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n');
  console.log(JSON.stringify({ sourceChecks: cases.length, detectorSourceChecks: detectorCases.length, detectorRefusal: detectorRefusal.rule, retainedBoundaryReproduced: true, existingRuntimeViewControl: 'pass', productionChange: false }));
} finally { fs.rmSync(scratch, { recursive: true, force: true }); }
