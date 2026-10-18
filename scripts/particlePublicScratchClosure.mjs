// Re-emit original registration and the diagnostic pair into an explicit scratch overlay.
// This does not run SDK generation or write an installed inventory.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { loadEmissionRepairs, applyEmissionRepairs, aliasDuplicateStructuralStructs } from './emissionRepairs.mjs';
import { canonicalizeGeneratedIncludes } from './generatedIncludes.mjs';

const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const at = arg.indexOf('='); assert.ok(at > 0, 'expected explicit --name=path arguments');
  return [arg.slice(0, at), arg.slice(at + 1)];
}));
for (const key of ['--compiler', '--typescript', '--context', '--inventory', '--diagnostics', '--scratch', '--output']) assert.ok(args[key], key + ' required');
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const scratch = path.resolve(args['--scratch']);
const inventory = path.resolve(args['--inventory']);
assert.ok(!scratch.startsWith(root + path.sep) && scratch !== root, 'scratch must be outside the repository');
assert.ok(!scratch.startsWith(inventory + path.sep) && scratch !== inventory, 'scratch must be outside the selected inventory');
assert.ok(!fs.existsSync(path.join(scratch, 'include')), 'use a fresh scratch overlay');
fs.mkdirSync(scratch, { recursive: true });
const hash = input => createHash('sha256').update(input).digest('hex');
const backendUrl = pathToFileURL(path.resolve(args['--compiler']));
const { createCppCompilerBackend } = await import(backendUrl);
const { lowerTypeScriptSource } = await import(new URL('../../compiler-semantic/src/index.js', backendUrl));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'])));
const input = fs.readFileSync(path.resolve(args['--context']));
const context = JSON.parse(input);

// Preserve the qualified original project graph, including its semantic evidence and helper
// identities. Re-lowering a different project is a separate generation input, not this closure.
const qualified = JSON.parse(fs.readFileSync(path.join(root, 'artifacts/particle-named-result-view.json')));
assert.equal(hash(input), qualified.originalSavedGraph.sha256, 'use the qualified original graph');
const patch = 'source-patches/particle-codec-document-result-owned-view.patch';
const diagnosticSources = ['packages/types/src/ImportDiagnostic.ts', 'packages/importdiagnostics/src/importDiagnosticCollector.ts'];
const codec = 'packages/types/src/ParticleFormatCodec.ts';
const destination = path.join(scratch, 'source', codec);
fs.mkdirSync(path.dirname(destination), { recursive: true });
fs.copyFileSync(path.join(root, '.dependencies/flight', codec), destination);
const applied = spawnSync('git', ['apply', path.join(root, patch)], { cwd: path.join(scratch, 'source'), encoding: 'utf8' });
assert.equal(applied.status, 0, applied.stderr);
const lowered = lowerTypeScriptSource(ts.createSourceFile('/flight/' + codec, fs.readFileSync(destination, 'utf8'), ts.ScriptTarget.Latest, true), { packageName: '@flighthq/types', upstreamDirectory: '/flight' });
assert.deepEqual(lowered.diagnostics, []);
context.modules = context.modules.map(module => module.source === codec ? lowered.module : module);
const freshContext = JSON.stringify(context);
const freshContextPath = path.join(scratch, 'context.json');
fs.writeFileSync(freshContextPath, freshContext);
const selected = context.modules.filter(module => module.source === 'packages/particles-formats/src/registerBuiltInParticleFormats.ts' || diagnosticSources.includes(module.source));
assert.equal(selected.length, 3, 'complete original registration module plus the original diagnostic pair');
assert.equal(hash(JSON.stringify(selected.find(module => module.packageName === '@flighthq/particles-formats'))), qualified.originalSavedGraph.registration.sourceModuleIrSha256);
const sources = selected.map(module => ({ file: module.source, rawSha256: hash(fs.readFileSync(path.join(root, '.dependencies/flight', module.source))) }));
const session = createCppCompilerBackend().createEmissionSession(context);
const emitted = selected.flatMap(module => session.emitModule(module).map(file => ({ ...file, source: module.source, moduleIrSha256: hash(JSON.stringify(module)) })));
assert.equal(emitted.length, 3);
for (const file of emitted) assert.ok(!file.contents.includes('NOT GENERATED'), file.path + ' contains an omitted body');

// Check the independently supplied original pair against this exact emission session, rather
// than treating another compiler's patch count as proof of an identical declaration floor.
const diagnostics = path.resolve(args['--diagnostics']);
const suppliedProvenance = JSON.parse(fs.readFileSync(path.join(diagnostics, 'provenance.json')));
const diagnosticParity = emitted.filter(file => diagnosticSources.includes(file.source)).map(file => {
  const supplied = fs.readFileSync(path.join(diagnostics, 'include', file.path));
  assert.equal(hash(supplied), suppliedProvenance.headerSha256[file.path]);
  assert.equal(hash(file.contents), hash(supplied), 'current compiler disagrees with supplied original ' + file.path);
  return { file: file.path, sha256: hash(supplied), byteIdentical: true };
});

// Existing inventory definitions are immutable witnesses for the ordinary derived duplicate
// struct pass. Its byte-identity/dependency guards select canonical declarations; no ABI alias
// or generated function body is supplied by this oracle.
const selectedPaths = new Set(emitted.map(file => file.path));
const providers = [];
const visit = directory => {
  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    const file = path.join(directory, entry.name);
    if (entry.isDirectory()) visit(file);
    else if (entry.name.endsWith('.hpp')) {
      const relative = path.relative(path.join(inventory, 'include'), file).split(path.sep).join('/');
      if (!selectedPaths.has(relative)) providers.push({ path: relative, contents: fs.readFileSync(file, 'utf8') });
    }
  }
};
visit(path.join(inventory, 'include'));
const providerHashes = providers.map(file => hash(file.contents));
const raw = emitted.map(file => ({ file: file.path, source: file.source, moduleIrSha256: file.moduleIrSha256, rawSha256: hash(file.contents) }));
const repairs = applyEmissionRepairs(loadEmissionRepairs(root), emitted).filter(repair => repair.files.length > 0);
const packageGraph = new Map();
for (const name of Object.keys(context.options.packageTargets)) {
  const manifest = JSON.parse(fs.readFileSync(path.join(root, '.dependencies/flight/packages', name.slice('@flighthq/'.length), 'package.json')));
  const target = context.options.packageTargets[name];
  packageGraph.set(target.includePrefix.split('/')[1], {
    namespace: target.namespace,
    dependencies: new Set(Object.keys(manifest.dependencies ?? {}).filter(name => name.startsWith('@flighthq/')).map(name => name.slice('@flighthq/'.length).replaceAll('-', '_'))),
  });
}
const canonicalProviders = providers.filter(file => file.path.startsWith('flight/types/') || file.path.startsWith('flight/particles/'));
const aliases = aliasDuplicateStructuralStructs([...canonicalProviders, ...emitted], packageGraph);
const includes = canonicalizeGeneratedIncludes([...providers.map(file => ({ path: file.path, contents: '' })), ...emitted]);
providers.forEach((file, index) => assert.equal(hash(file.contents), providerHashes[index], 'scratch repair changed an installed provider: ' + file.path));
for (const file of emitted) {
  const destination = path.join(scratch, 'include', file.path);
  fs.mkdirSync(path.dirname(destination), { recursive: true }); fs.writeFileSync(destination, file.contents);
}
const report = {
  normalSdk: false, inventoryWritten: false,
  scope: 'Complete original registration module and diagnostic type/collector emitted in one current compiler session, over unchanged original headers from the completed normal run; existing declared repairs only. Full public consumer scratch closure, with no installed header or package promotion.',
  runtimeCommit: spawnSync('git', ['rev-parse', 'HEAD'], { cwd: root, encoding: 'utf8' }).stdout.trim(),
  compilerBackendSha256: hash(fs.readFileSync(path.resolve(args['--compiler']))),
  inputContextSha256: hash(input), freshContextSha256: hash(freshContext), originalRegistrationIrUnchanged: true, sources, declaredTypeOnlyPatch: { file: patch, sha256: hash(fs.readFileSync(path.join(root, patch))) },
  suppliedProvenance, diagnosticParity, repairs, aliases, canonicalIncludes: includes,
  files: emitted.map((file, index) => ({ ...raw[index], sha256: hash(file.contents) })),
  frozenProviders: providers.map((file, index) => ({ file: file.path, sha256: providerHashes[index] })),
  providerBytesUnchanged: true,
};
fs.writeFileSync(path.join(scratch, 'emission.json'), JSON.stringify(report, null, 2) + '\n');
const output = path.resolve(args['--output']); fs.mkdirSync(path.dirname(output), { recursive: true });
if (args['--cxx']) {
  const result = spawnSync(process.execPath, [path.join(root, 'scripts/particlePublicNativeConsumerOracle.mjs'),
    ...['--compiler', '--typescript', '--inventory', '--cxx'].map(key => key + '=' + path.resolve(args[key])), '--context=' + freshContextPath,
    '--overlay=' + scratch, '--output=' + output + '.native.json'], { cwd: root, encoding: 'utf8', maxBuffer: 16 * 1024 * 1024 });
  fs.writeFileSync(output + '.driver.log', result.stdout + result.stderr);
  report.nativeExit = result.status;
  if (fs.existsSync(output + '.native.json')) report.native = JSON.parse(fs.readFileSync(output + '.native.json'));
  process.exitCode = result.status ?? 1;
}
// Re-read disk witnesses after native compilation as well as guarding the in-memory repair pass.
providers.forEach((file, index) => assert.equal(hash(fs.readFileSync(path.join(inventory, 'include', file.path))), providerHashes[index]));
fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({ output, emitted: emitted.length, diagnosticParity, nativeExit: report.nativeExit, native: report.native?.native }));
