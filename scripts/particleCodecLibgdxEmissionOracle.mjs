import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// An assignment-only diagnostic from a saved normal IR graph; never runs SDK generation.
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const at = arg.indexOf('='); assert.ok(at > 0, 'expected explicit --context --compiler --typescript --output paths');
  return [arg.slice(0, at), arg.slice(at + 1)];
}));
assert.ok(args['--context'] && args['--compiler'] && args['--typescript'] && args['--output']);
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const backendUrl = pathToFileURL(path.resolve(args['--compiler']));
const { createCppCompilerBackend } = await import(backendUrl);
const { lowerTypeScriptSource } = await import(new URL('../../compiler-semantic/src/index.js', backendUrl));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'])));
const input = fs.readFileSync(path.resolve(args['--context']));
const context = JSON.parse(input);
const codecFile = 'packages/types/src/ParticleFormatCodec.ts';
const raw = fs.readFileSync(path.join(root, '.dependencies/flight', codecFile), 'utf8');
const equivalent = raw.replace('  ): {\n    config:', '  ): Required<{\n    config:')
  .replace('    diagnostics: ImportDiagnostic[];\n  };', '    diagnostics: ImportDiagnostic[];\n  }>;');
assert.notEqual(equivalent, raw, 'expected the exact already-required result declaration');
const lower = (file, source, packageName) => {
  const result = lowerTypeScriptSource(ts.createSourceFile('/flight/' + file, source, ts.ScriptTarget.Latest, true), { packageName, upstreamDirectory: '/flight' });
  assert.deepEqual(result.diagnostics, []); return result.module;
};
const codec = lower(codecFile, equivalent, '@flighthq/types');
assert.ok(context.modules.some(module => module.source === codecFile));
context.modules = context.modules.map(module => module.source === codecFile ? codec : module);
const witnessFile = 'packages/particles-formats/src/particleCodecLibgdxCallbacks.ts';
const witnessSource = fs.readFileSync(path.join(root, 'scripts/fixtures/particleCodecLibgdxCallbacks.ts'), 'utf8');
const witness = lower(witnessFile, witnessSource, '@flighthq/particles-formats');
context.modules.push(witness);
const original = context.modules.find(module => module.source === 'packages/particles-formats/src/libgdxParse.ts');
assert.ok(original);
const session = createCppCompilerBackend().createEmissionSession(context);
const output = path.resolve(args['--output']);
const report = { inputContextSha256: createHash('sha256').update(input).digest('hex'), modules: [],
  scope: 'Assignment-only source emission over original libGDX functions and saved normal IR. No detector, full descriptor, registry, normal SDK or installed callback qualification.' };
for (const module of [original, witness]) {
  const files = session.emitModule(module);
  for (const file of files) {
    if (module === original) assert.equal((file.contents.match(/^inline [^\n]*_flight_readonly_options_[^\n]*\([^\n]*\) \{$/gm) ?? []).length, 2);
    else {
      assert.equal((file.contents.match(/NamedFunction</g) ?? []).length, 2);
      assert.equal((file.contents.match(/callable_identity_equal_named</g) ?? []).length, 2);
      assert.ok(file.contents.includes('RowWritable<flight::RowOf<flight::Ref<flight::types::LibgdxParseResult>>>'));
      assert.ok(file.contents.includes('RowRequired<flight::RowOf<flight::Ref<'));
    }
    const target = path.join(output, file.path);
    fs.mkdirSync(path.dirname(target), { recursive: true }); fs.writeFileSync(target, file.contents);
    report.modules.push({ source: module.source, file: file.path, bytes: file.contents.length,
      sha256: createHash('sha256').update(file.contents).digest('hex') });
  }
}
fs.writeFileSync(path.join(output, 'report.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({ result: 'pass', originalBodyHelpers: 2, originalCallbackAssignments: 2, originalNamedIdentityComparisons: 2, normalSdk: false }));
