import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// A diagnostic/design proof. No SDK generation, source rewriting or product activation.
const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const proof = JSON.parse(fs.readFileSync(path.join(repo, 'artifacts/sprite-source-generic-contracts.json')));
const positional = process.argv.slice(2).filter(arg => !arg.startsWith('--'));
assert(positional.length <= 1, 'usage: node scripts/spriteSourceGenericContractProof.mjs [archive] [--typescript=path] [--cxx=path]');
const options = Object.fromEntries(process.argv.slice(2).filter(arg => arg.startsWith('--')).map(arg => {
  const equals = arg.indexOf('=');
  assert(equals > 2, 'options use --name=value');
  const name = arg.slice(2, equals);
  assert(['typescript', 'cxx'].includes(name), 'unknown option: ' + name);
  return [name, path.resolve(arg.slice(equals + 1))];
}));
const archive = path.resolve(repo, positional[0] ?? proof.proofArchive.path);
const digest = value => createHash('sha256').update(value).digest('hex');
assert.equal(digest(fs.readFileSync(archive)), proof.proofArchive.sha256, 'archive SHA-256');
const members = execFileSync('tar', ['-tzf', archive], { encoding: 'utf8' }).trim().split('\n');
assert.equal(members.length, proof.proofArchive.memberCount);
assert(members.every(member => !path.isAbsolute(member) && !member.split('/').includes('..')));
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'sprite-source-contracts-'));
const replayed = [];
try {
  execFileSync('tar', ['-xzf', archive, '-C', temporary]);
  const read = name => fs.readFileSync(path.join(temporary, name));
  const json = name => JSON.parse(read(name));
  const files = json('checksums.json').files;
  assert.equal(files.length, proof.proofArchive.checksummedFileCount);
  assert.deepEqual(members.toSorted(), [...files.map(file => file.path), 'checksums.json'].toSorted());
  for (const file of files) {
    const bytes = read(file.path);
    assert.equal(bytes.length, file.bytes, file.path);
    assert.equal(digest(bytes), file.sha256, file.path);
  }
  const fixture = fs.readFileSync(path.join(repo, 'artifacts/sprite-source-generic-contracts.ts'));
  assert.equal(digest(fixture), digest(read('sprite-source-generic-contracts.ts')));
  for (const input of proof.receivedInputs) assert.equal(digest(read(input.path)), input.sha256);

  const strict = json('frozen-strict220/v3-typescript.json');
  const owned = new Set(proof.ownedSourceModules);
  const key = diagnostic => JSON.stringify([diagnostic.file, diagnostic.code, diagnostic.message]);
  const foreign = variant => variant.diagnostics.filter(row => !owned.has(row.file)).map(key).toSorted();
  assert.deepEqual(strict.variants.map(row => [row.variant, row.diagnostics.length, row.owned.length]),
    [['baseline', 220, 2], ['candidate', 220, 2]]);
  assert.deepEqual(foreign(strict.variants[0]), foreign(strict.variants[1]), 'foreign diagnostic multiset');
  assert.equal(foreign(strict.variants[0]).length, 218);
  assert(strict.variants.every(variant => variant.owned.every(row => row.code === 2345)));
  assert(strict.tokenEqual.every(row => row.transpiledJavaScriptTokensEqual));
  const comparison = json('strict220-comparison.json');
  assert.equal(comparison.rerun, false);
  assert.deepEqual(comparison.foreignAdded, []);
  assert.deepEqual(comparison.foreignRemoved, []);

  const minimal = json('minimal-contract-results.json');
  assert.equal(minimal.rows.length, 12);
  assert(minimal.rows.every(row => row.pass));
  for (const name of ['generic_finish', 'generic_finish_explicit_control']) {
    const header = read(`minimal/emitted/${name}/probe/${name}.hpp`).toString();
    assert.match(header, /finish_entity<N>\(out\)/u);
  }
  assert.match(read('minimal/emitted/derived_finish_widening_control/probe/derived_finish_widening_control.hpp').toString(),
    /finish_entity<flight::Ref<probe_types::Derived>>\(out\)/u);
  const reducedMethod = minimal.rows.find(row => row.name === 'method_bivariance_is_not_proof');
  assert.deepEqual(reducedMethod.diagnostics, []);
  const first = json('effective-types-results.json.v1');
  assert.equal(first.rows.filter(row => row.pass).length, 11);
  assert.deepEqual(first.rows.filter(row => !row.pass).map(row => row.name), ['method_bivariance_is_not_proof']);
  const actual = json('effective-types-results.json');
  assert.equal(actual.rows.length, 18);
  assert(actual.rows.every(row => row.pass));
  assert.equal(actual.rows.reduce((count, row) => count + row.diagnostics.length, 0), 8);
  assert.equal(actual.foreignDiagnostics.length, 203);
  assert.equal(actual.virtualContextFiles.length, 1033);
  const row = name => actual.rows.find(item => item.name === name);
  assert.match(row('sprite_to_bounds_full_runtime').diagnostics[0].message, /EntityRuntimeKey/u);
  assert.match(row('bounds_to_sprite_nullable_data').diagnostics[0].message, /null/u);
  assert.deepEqual(row('sprite_to_bounds_without_runtime_control').diagnostics, []);
  assert.equal(row('parameterizing_methods_by_R_is_insufficient').diagnostics[0].code, 2345);

  const behavior = json('source-contract-results.json');
  const oracle = json('source-original-oracle.json');
  assert.equal(oracle.checks, 39);
  assert.equal(oracle.failures, 0);
  assert.equal(behavior.sourceContractChecks, 10);
  assert.equal(behavior.cases.length, 10);
  for (const source of behavior.selected) {
    assert.equal(source.sameFrozen, true);
    assert.equal(digest(read('frozen-builder4/original-source/' + source.source)), source.sha256);
    assert.equal(digest(read('source-replay-inputs/packages/' + source.source)), source.sha256);
  }
  for (const source of oracle.executedSources) {
    assert.equal(digest(read('source-replay-inputs/' + source.source)), source.sha256);
  }
  assert.match(read('finish-owner-native-run.log').toString(), /10 original emitted finish owner checks, 0 failures/u);
  assert.equal(read('finish-owner-native-build.log').length, 0);
  const native = json('native-results.json');
  assert.equal(native.buildExitCode, 0);
  assert.equal(native.runExitCode, 0);
  assert.equal(proof.coreAbiActivation, false);
  assert.equal(proof.compilerImplementationChanges, false);
  assert.equal(proof.typesChanges, false);
  assert.equal(proof.sdkGeneration, false);
  assert.equal(proof.sourceBodyReplacements, 0);

  if (options.typescript) {
    const { default: ts } = await import(pathToFileURL(options.typescript));
    assert.equal(ts.version, actual.typescript);
    const effective = path.join(temporary, 'frozen-supplement/effective');
    const fixturePath = path.join(repo, 'artifacts/sprite-source-generic-contracts.ts');
    const host = ts.createCompilerHost(actual.options);
    host.resolveModuleNames = (names, from) => names.map(name => {
      if (name.startsWith('@flighthq/')) {
        const [pkg, ...rest] = name.slice('@flighthq/'.length).split('/');
        const source = pkg === 'entity' && rest.join('/') === 'contract' ? 'entity' : rest.join('/') || 'index';
        const target = path.join(effective, 'packages', pkg, 'src', source + '.ts');
        return fs.existsSync(target) ? { resolvedFileName: target, extension: ts.Extension.Ts } : undefined;
      }
      return ts.resolveModuleName(name, from, actual.options, host).resolvedModule;
    });
    const program = ts.createProgram([fixturePath], actual.options, host);
    const diagnostics = ts.getPreEmitDiagnostics(program);
    const source = program.getSourceFile(fixturePath);
    const observed = actual.rows.map(expected => {
      const name = expected.name === 'method_bivariance_is_not_proof'
        ? 'method_syntax_still_rejected_with_full_runtime' : expected.name;
      const declaration = source.statements.find(statement => ts.isModuleDeclaration(statement) && statement.name.text === name);
      assert(declaration, name);
      const errors = diagnostics.filter(error => error.file === source && error.start >= declaration.getStart(source) && error.start < declaration.end);
      assert.deepEqual(errors.map(error => [error.code, ts.flattenDiagnosticMessageText(error.messageText, '\n')]),
        expected.diagnostics.map(error => [error.code, error.message]), name);
      return errors.length;
    });
    assert.equal(observed.reduce((sum, count) => sum + count, 0), 8);
    assert.equal(diagnostics.filter(error => error.file === source).length, 8);
    const freshForeign = diagnostics.filter(error => error.file !== source).map(error => ({
      file: error.file && path.relative(effective, error.file.fileName), code: error.code,
      message: ts.flattenDiagnosticMessageText(error.messageText, '\n'),
    }));
    assert.deepEqual(freshForeign.map(key).toSorted(), actual.foreignDiagnostics.map(key).toSorted());
    replayed.push('exact Types18 (8 expected owned diagnostics, 203 foreign retained)');
    execFileSync(process.execPath, [path.join(temporary, 'probe-source-behavior.mjs'),
      '--typescript=' + options.typescript, '--flight=' + path.join(temporary, 'source-replay-inputs')], { stdio: 'pipe' });
    assert.deepEqual(json('source-contract-results.json').cases, behavior.cases);
    assert.deepEqual(json('source-original-oracle.json').executedSources, oracle.executedSources);
    replayed.push('original source39 + caller contracts10');
  }
  if (options.cxx) {
    const binary = path.join(temporary, 'finish-owner-replay');
    execFileSync(options.cxx, ['-std=c++20', '-g0', '-w',
      '-I' + temporary, '-I' + path.join(temporary, 'minimal/emitted/generic_finish'),
      '-I' + path.join(temporary, 'native-inputs/include'),
      '-I' + path.join(temporary, 'native-inputs/generated/include'),
      path.join(temporary, 'finish-owner-native.cpp'), '-o', binary], { stdio: 'pipe' });
    assert.match(execFileSync(binary, { encoding: 'utf8' }), /10 original emitted finish owner checks, 0 failures/u);
    replayed.push('emitted finishEntity native10');
  }
  console.log(`PASS ${files.length} hashes; strict220/220 with unchanged foreign218; reduced12/effective18, source39+10/native10 receipts`);
  if (replayed.length) console.log('REPLAY PASS ' + replayed.join('; '));
  console.log('Original Sprite factory/image native closure remains unqualified; full220 receipt was compared, not rerun.');
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
