// Re-emit original particle modules and the diagnostic pair into an explicit scratch overlay.
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
const { lowerTypeScriptSources } = await import(new URL('../../compiler-semantic/src/index.js', backendUrl));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'])));
const input = fs.readFileSync(path.resolve(args['--context']));
const context = JSON.parse(input);

// Refresh whole original modules from the pinned source plus the declared Flight patches.
// An old saved graph can predate a package's approved local type aliases or own-key reads.
const codec = 'packages/types/src/ParticleFormatCodec.ts';
const patch = 'source-patches/particle-codec-document-result-owned-view.patch';
const diagnosticSources = ['packages/types/src/ImportDiagnostic.ts', 'packages/importdiagnostics/src/importDiagnosticCollector.ts'];
const refresh = context.modules;
const refreshPaths = new Set(refresh.map(module => module.source));
for (const module of refresh) {
  const destination = path.join(scratch, 'source', module.source);
  fs.mkdirSync(path.dirname(destination), { recursive: true });
  fs.copyFileSync(path.join(root, '.dependencies/flight', module.source), destination);
}
const sourcePatches = [];
for (const entry of JSON.parse(fs.readFileSync(path.join(root, 'source-patches/manifest.json'))).patches) {
  if (entry.dependency !== 'flight') continue;
  const patchPath = path.join(root, 'source-patches', entry.patch);
  const bytes = fs.readFileSync(patchPath);
  const paths = [...bytes.toString().matchAll(/^\+\+\+ b\/(.+)$/gm)].map(match => match[1]).filter(file => refreshPaths.has(file));
  if (paths.length === 0) continue;
  const applied = spawnSync('git', ['apply', ...paths.map(file => '--include=' + file), patchPath], { cwd: path.join(scratch, 'source'), encoding: 'utf8' });
  assert.equal(applied.status, 0, entry.patch + ': ' + applied.stderr);
  sourcePatches.push({ file: entry.patch, sha256: hash(bytes), paths });
}
const sources = refresh.map(module => ({ file: module.source,
  rawSha256: hash(fs.readFileSync(path.join(root, '.dependencies/flight', module.source))),
  patchedSha256: hash(fs.readFileSync(path.join(scratch, 'source', module.source))),
}));
const lowered = lowerTypeScriptSources(refresh.map(module => ({
  packageName: module.packageName, upstreamDirectory: '/flight',
  sourceFile: ts.createSourceFile('/flight/' + module.source, fs.readFileSync(path.join(scratch, 'source', module.source), 'utf8'), ts.ScriptTarget.Latest, true),
})), context.moduleResolution, {
  profileTypeNames: context.options.externalBindings.bindings.filter(binding => binding.space === 'type').map(binding => binding.sourceName),
});
for (const result of lowered) {
  if (result.module.packageName === '@flighthq/particles-formats' || diagnosticSources.includes(result.module.source)) assert.deepEqual(result.diagnostics, []);
}
context.modules = lowered.map(result => result.module);
const selected = context.modules.filter(module => module.packageName === '@flighthq/particles-formats' || diagnosticSources.includes(module.source));
assert.equal(selected.length, 20, '18 complete original particle modules plus the original diagnostic pair');
const session = createCppCompilerBackend().createEmissionSession(context);
const emitted = selected.flatMap(module => session.emitModule(module).map(file => ({ ...file, source: module.source, moduleIrSha256: hash(JSON.stringify(module)) })));
assert.equal(emitted.length, 20);
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
const aliases = aliasDuplicateStructuralStructs([...providers, ...emitted], packageGraph);
const includes = canonicalizeGeneratedIncludes([...providers, ...emitted]);
providers.forEach((file, index) => assert.equal(hash(file.contents), providerHashes[index], 'scratch repair changed an installed provider: ' + file.path));
for (const file of emitted) {
  const destination = path.join(scratch, 'include', file.path);
  fs.mkdirSync(path.dirname(destination), { recursive: true }); fs.writeFileSync(destination, file.contents);
}
const report = {
  normalSdk: false, inventoryWritten: false,
  scope: 'All 18 original particle modules and the original diagnostic type/collector emitted in one current compiler session; existing declared repairs only. Scratch closure on a frozen inventory, with no installed header or package promotion.',
  runtimeCommit: spawnSync('git', ['rev-parse', 'HEAD'], { cwd: root, encoding: 'utf8' }).stdout.trim(),
  compilerBackendSha256: hash(fs.readFileSync(path.resolve(args['--compiler']))),
  inputContextSha256: hash(input), sourcePatches, sources, declaredTypeOnlyPatch: { file: patch, sha256: hash(fs.readFileSync(path.join(root, patch))) },
  suppliedProvenance, diagnosticParity, repairs, aliases, canonicalIncludes: includes,
  files: emitted.map((file, index) => ({ ...raw[index], sha256: hash(file.contents) })),
  frozenProviders: providers.map((file, index) => ({ file: file.path, sha256: providerHashes[index] })),
  providerBytesUnchanged: true,
};
fs.writeFileSync(path.join(scratch, 'emission.json'), JSON.stringify(report, null, 2) + '\n');
const output = path.resolve(args['--output']); fs.mkdirSync(path.dirname(output), { recursive: true });
if (args['--cxx']) {
  const result = spawnSync(process.execPath, [path.join(root, 'scripts/particlePublicNativeConsumerOracle.mjs'),
    ...['--compiler', '--typescript', '--context', '--inventory', '--cxx'].map(key => key + '=' + path.resolve(args[key])),
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
