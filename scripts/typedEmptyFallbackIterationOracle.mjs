import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
const args = Object.fromEntries(process.argv.slice(2).map(arg => { const at = arg.indexOf('='); assert.ok(at > 0); return [arg.slice(0, at), arg.slice(at + 1)]; }));
for (const key of ['--compiler', '--typescript', '--cxx', '--context', '--output']) assert.ok(args[key], key + ' required');
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const compilerUrl = pathToFileURL(path.resolve(args['--compiler']));
const { emitIrModuleCpp, createCppCompilerBackend } = await import(compilerUrl);
const { lowerTypeScriptSource } = await import(new URL('../../compiler-semantic/src/index.js', compilerUrl));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'])));
const hash = data => createHash('sha256').update(data).digest('hex');
const scratch = fs.mkdtempSync(path.join(os.tmpdir(), 'typed-empty-iteration-'));
try {
  const fixture = path.join(root, 'scripts/fixtures/typedEmptyFallbackIteration.ts');
  const program = ts.createProgram([fixture], { strict: true, noEmit: true, types: [], target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ESNext, skipLibCheck: true });
  assert.deepEqual(ts.getPreEmitDiagnostics(program).map(d => ts.flattenDiagnosticMessageText(d.messageText, '\n')), []);
  const source = fs.readFileSync(fixture, 'utf8');
  const lowered = lowerTypeScriptSource(ts.createSourceFile('/flight/packages/example/src/typedEmptyFallbackIteration.ts', source, ts.ScriptTarget.Latest, true), { packageName: '@flighthq/example', upstreamDirectory: '/flight' });
  assert.deepEqual(lowered.diagnostics, []);
  const emitted = emitIrModuleCpp(lowered.module, { runtimeProfile: 'flight-cpp' }).contents;
  assert.equal((emitted.match(/const auto for_of_source/g) ?? []).length, 6);
  assert.ok(!emitted.includes('flight::Array<flight::Any>'));
  assert.ok(!emitted.includes('.value_or(flight::Array'));
  fs.writeFileSync(path.join(scratch, 'iteration.hpp'), emitted);
  const binary = path.join(scratch, 'iteration');
  const build = spawnSync(path.resolve(args['--cxx']), ['-std=c++20', '-ffp-contract=off', '-I' + scratch,
    ...['overrides/include', 'include', 'generated/include'].map(dir => '-I' + path.join(root, dir)), path.join(root, 'scripts/fixtures/typedEmptyFallbackIteration.cpp'), '-o', binary], { encoding: 'utf8' });
  assert.equal(build.status, 0, build.stderr || build.stdout);
  const run = spawnSync(binary, [], { encoding: 'utf8' }); assert.equal(run.status, 0, run.stderr || run.stdout);
  const native = JSON.parse(run.stdout);
  const input = fs.readFileSync(path.resolve(args['--context']));
  const context = JSON.parse(input);
  const module = context.modules.find(m => m.source === 'packages/particles-formats/src/registerBuiltInParticleFormats.ts'); assert.ok(module);
  const original = module.declarations.find(d => d.kind === 'function' && d.binding.name === 'applyParticleImportOptions'); assert.ok(original);
  const before = JSON.stringify(original);
  const files = createCppCompilerBackend().createEmissionSession(context).emitModule(module);
  const contents = files.map(file => file.contents).join('\n');
  assert.equal(JSON.stringify(original), before, 'original IR body changed');
  assert.ok(contents.includes('inline void apply_particle_import_options('));
  assert.ok(contents.includes('const auto for_of_source'));
  assert.ok(contents.includes('register_particle_format(flight::row_get<flight::RowKey<"kind">>(format), flight::row_get<flight::RowKey<"codec">>(format))'));
  assert.ok(!contents.includes('NOT GENERATED'), 'module retained a refused declaration');
  const report = {
    fixtureStrict: true, emittedNative: native,
    originalSavedGraph: { sha256: hash(input), source: module.source, originalFunctionIrSha256: hash(before), unchanged: true, emittedWithoutRefusedDeclarations: true, files: files.map(file => ({ path: file.path, sha256: hash(file.contents) })) },
    sourceHashes: ['scripts/fixtures/typedEmptyFallbackIteration.ts', 'scripts/fixtures/typedEmptyFallbackIteration.cpp', 'source-patches/compiler-typed-empty-fallback-iteration.patch'].map(file => ({ file, sha256: hash(fs.readFileSync(path.join(root, file))) })),
    normalSdk: false,
    scope: 'Compiler source typing and emitted native family controls. Original complete module emitted from the saved normal graph without IR/body changes. This does not qualify installed particle headers, built-in native callback slots or the public registry consumer.'
  };
  fs.writeFileSync(path.resolve(args['--output']), JSON.stringify(report, null, 2) + '\n'); console.log(JSON.stringify(report));
} finally { fs.rmSync(scratch, { recursive: true, force: true }); }
