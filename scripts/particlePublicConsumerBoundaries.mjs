import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// A saved-source graph diagnostic and an actual installed-ABI counterexample. Never runs an SDK.
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const at = arg.indexOf('='); assert.ok(at > 0, 'expected explicit --compiler --typescript --context --cxx --output paths');
  return [arg.slice(0, at), arg.slice(at + 1)];
}));
assert.ok(args['--compiler'] && args['--typescript'] && args['--context'] && args['--cxx'] && args['--output']);
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const backendUrl = pathToFileURL(path.resolve(args['--compiler']));
const { createCppCompilerBackend } = await import(backendUrl);
const { lowerTypeScriptSource } = await import(new URL('../../compiler-semantic/src/index.js', backendUrl));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'])));
const input = fs.readFileSync(path.resolve(args['--context']));
const context = JSON.parse(input), codecFile = 'packages/types/src/ParticleFormatCodec.ts';
const original = fs.readFileSync(path.join(root, '.dependencies/flight', codecFile), 'utf8');
const equivalent = original.replace('  ): {\n    config:', '  ): Required<{\n    config:').replace('    diagnostics: ImportDiagnostic[];\n  };', '    diagnostics: ImportDiagnostic[];\n  }>;');
assert.notEqual(equivalent, original);
const lower = (file, source, packageName) => {
  const result = lowerTypeScriptSource(ts.createSourceFile('/flight/' + file, source, ts.ScriptTarget.Latest, true), { packageName, upstreamDirectory: '/flight' });
  assert.deepEqual(result.diagnostics, []); return result.module;
};
context.modules = context.modules.map(module => module.source === codecFile ? lower(codecFile, equivalent, '@flighthq/types') : module);
const consumer = lower('packages/particles-formats/src/particlePublicConsumer.ts', fs.readFileSync(path.join(root, 'scripts/fixtures/particlePublicConsumer.ts'), 'utf8'), '@flighthq/particles-formats');
context.modules.push(consumer);
const session = createCppCompilerBackend().createEmissionSession(context);
const modules = [];
for (const source of ['packages/particles-formats/src/registerBuiltInParticleFormats.ts', consumer.source]) {
  const module = context.modules.find(candidate => candidate.source === source); assert.ok(module);
  try {
    const files = session.emitModule(module);
    modules.push({ source, emitted: true, files: files.map(file => ({ file: file.path, sha256: createHash('sha256').update(file.contents).digest('hex') })),
      ...(module === consumer ? { readonlyOptionsForwardedWithoutNativeConversion: files.some(file => file.contents.includes('parse_particle_config(text, options);')) } : {}) });
  } catch (error) {
    assert.equal(error.kind, 'backend-emission'); modules.push({ source, emitted: false, rule: error.rule, message: error.message });
  }
}
const scratch = fs.mkdtempSync(path.join(os.tmpdir(), 'particle-public-boundaries-'));
try {
  // Compile an object: Zig's syntax-only cache path can report a missing output object instead of
  // a compiler result. This contains only decltype/is_invocable checks of the ACTUAL public APIs.
  const headers = ['flight/particles_formats/parse_particle_config.hpp', 'flight/particles_formats/format_registry.hpp', 'flight/types/particle_config_parse.hpp', 'flight/types/particle_format_codec.hpp'];
  const headerHashes = headers.map(file => ({ file, sha256: createHash('sha256').update(fs.readFileSync(path.join(root, 'generated/include', file))).digest('hex') }));
  const native = spawnSync(path.resolve(args['--cxx']), ['-std=c++20', '-ffp-contract=off',
    ...['overrides/include', 'include', 'generated/include', 'out/tooling/sdl/include', 'out/tooling/gles/include'].map(dir => '-I' + path.join(root, dir)),
    '-c', path.join(root, 'scripts/fixtures/particlePublicReadonlyAbiBoundary.cpp'), '-o', path.join(scratch, 'boundary.o')], { encoding: 'utf8' });
  for (const header of headerHashes) assert.equal(createHash('sha256').update(fs.readFileSync(path.join(root, 'generated/include', header.file))).digest('hex'), header.sha256, 'header changed during ABI control');
  const report = { inputContextSha256: createHash('sha256').update(input).digest('hex'), originalGraphDiagnostic: modules,
    approvedTypeOnlyEquivalent: 'Required on two already-required writable result fields',
    installedAbiCounterexample: { reproduced: native.status === 0, compilerExit: native.status, diagnostic: native.stderr || native.stdout,
      source: 'scripts/fixtures/particlePublicReadonlyAbiBoundary.cpp',
      headers: headerHashes, unchangedDuringBuild: true,
      meaning: 'Object compile PASS means current automatic public config/document functions reject optional readonly structural options, while parseRegisteredParticleFormat accepts them. It is a retained boundary, not positive native consumer qualification.' },
    normalSdk: false, scope: 'Saved original graph and exact current installed ABI diagnostic only. No current normal SDK, actual eight callback/native registry success or header promotion.' };
  const output = path.resolve(args['--output']); fs.mkdirSync(path.dirname(output), { recursive: true }); fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n');
  console.log(JSON.stringify(report)); assert.equal(native.status, 0, native.stderr || native.stdout);
} finally { fs.rmSync(scratch, { recursive: true, force: true }); }
