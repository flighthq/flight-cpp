import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const proof = JSON.parse(fs.readFileSync(path.join(repo, 'artifacts/sprite-readonly-object-fallback-proof.json')));
const archive = process.argv[2] ?? proof.proofArchive.path;
assert.equal(sha(fs.readFileSync(archive)), proof.proofArchive.sha256);
const members = execFileSync('tar', ['-tzf', archive], { encoding: 'utf8' }).trim().split('\n');
assert.equal(members.length, proof.proofArchive.memberCount);
assert(members.every(name => !path.isAbsolute(name) && !name.split('/').includes('..')));
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'sprite-fallback-proof-'));
try {
  execFileSync('tar', ['-xzf', archive, '-C', temporary]);
  const bytes = name => fs.readFileSync(path.join(temporary, name));
  const json = name => JSON.parse(bytes(name));
  const files = json('checksums.json').files;
  assert.equal(files.length, proof.proofArchive.checksummedFileCount);
  for (const file of files) {
    assert.equal(bytes(file.path).length, file.bytes, file.path);
    assert.equal(sha(bytes(file.path)), file.sha256, file.path);
  }
  const backend = 'packages/compiler-backend-cpp/src/cppCompilerBackend.ts';
  for (const [record, source, target, relative, before, after] of [
    [proof.runtime, 'baseline/structural_ref.hpp', 'narrow/runtime/include/flight/structural_ref.hpp',
      'include/flight/structural_ref.hpp', proof.runtime.baselineSha256, proof.runtime.candidateSha256],
    [proof.compiler, 'compiler/baseline/' + backend, 'compiler/candidate/' + backend,
      backend, proof.compiler.baselineBackendSha256, proof.compiler.candidateBackendSha256],
  ]) {
    const patch = path.join(repo, record.patch);
    assert.equal(sha(fs.readFileSync(patch)), record.sha256);
    assert.equal(sha(bytes(source)), before);
    const directory = fs.mkdtempSync(path.join(temporary, 'reconstruct-'));
    fs.mkdirSync(path.dirname(path.join(directory, relative)), { recursive: true });
    fs.writeFileSync(path.join(directory, relative), bytes(source));
    execFileSync('git', ['apply', '--unidiff-zero', patch], { cwd: directory });
    assert.equal(sha(fs.readFileSync(path.join(directory, relative))), after);
    assert.equal(sha(bytes(target)), after);
  }
  const runtimePatch = fs.readFileSync(path.join(repo, proof.runtime.patch), 'utf8');
  assert(!runtimePatch.split('\n').some(line => line.startsWith('-') && !line.startsWith('---')));
  assert.equal(json('narrow/compiler-build-result.json').exitCode, 0);
  const emission = json('narrow/emission-results.json');
  assert.deepEqual(emission.strict, []);
  assert(emission.rows.every(row => row.emitted && row.diagnostics.length === 0 && !row.asAnyOverride));
  const snapshot = / \[\[nodiscard\]\] flight::detail::ReadonlyObjectValue readonly_object_value\(\) const override \{ auto value = getter_\(\); (?:if \(!value.has_value\(\)\) return \{flight::AnyKind::null, true, \{\}, \{\}\}; )?return flight::detail::readonly_nested_object_value\(value\); \}/g;
  for (const name of ['getter', 'getter-controls', 'getter-exact']) {
    assert.equal(bytes('narrow/candidate/' + name + '.hpp').toString().replace(snapshot, ''),
      bytes('narrow/baseline/' + name + '.hpp').toString(), name + ' still adds only the authorized snapshot');
  }
  assert.equal(bytes('narrow/baseline/generated-getter-native.cpp').toString(),
    bytes('narrow/candidate/generated-getter-native.cpp').toString());
  const pass = (result, count) => {
    assert.equal(result.compileExit, 0);
    assert.equal(result.runExit, 0);
    assert.match(result.stdout, new RegExp('PASS ' + count + ' '));
  };
  const fail = (result, count) => {
    assert.equal(result.compileExit, 0);
    assert.equal(result.runExit, 1);
    assert.match(result.stderr, new RegExp('FAIL after ' + count + ' checks'));
  };
  const getter = json('narrow/getter-gate-results.json');
  fail(getter[0], 2); fail(getter[1], 21); pass(getter[2], 21);
  const exact = json('narrow/exact-gate-results.json');
  fail(exact[0], 13); fail(exact[1], 13); pass(exact[2], 13);
  const metadata = json('narrow/metadata-gate-results.json');
  pass(metadata[0], 62); pass(metadata[1], 83);
  const fallback = json('narrow/fallback-gate-results.json');
  pass(fallback[0], 30); pass(fallback[1], 30); pass(fallback[2], 8);
  const final = json('narrow/final-runtime-results.json');
  assert.equal(final[0].compileExit, 0);
  assert.equal(final[0].runExit, 0);
  assert.equal(final[0].stdout, json('historical-v1/native-initial-results.json')[1].stdout);
  pass(final[1], 15); fail(final[2], 81); fail(final[3], 47);
  fail(json('narrow/naive-result.json'), 15);
  const oldAssertion = 'check(bad([&](){(void)f::row_get<f::RowKey<"data">>(outer<XR>(container));}));\n // Empty Owned';
  const correctedAssertion = 'check(f::row_get<f::RowKey<"data">>(outer<XR>(container))->identity()==jo.get());\n // Empty Owned';
  assert.equal(bytes('narrow/metadata-native.cpp').toString(),
    bytes('historical-v1/metadata-native.cpp').toString().replace(oldAssertion, correctedAssertion),
    'only the reviewed check47 expectation changes');
  const parity = json('narrow/paired-parity.json');
  assert.deepEqual(parity.newFailures, []);
  assert(parity.rows.every(row => row.pass === 13 && row.fail === 3));
  assert.deepEqual(parity.rows[0].failures, parity.rows[1].failures);
  const project = json('narrow/runtime-project-parity.json');
  assert.equal(project.inputs.runtime, proof.runtime.baselineSha256);
  assert.equal(project.inputs.candidate, proof.runtime.candidateSha256);
  for (const rows of [project.baselineRows, project.candidateRows]) {
    assert.equal(rows.length, 4);
    assert(rows.every(row => row.buildExit === 0 && row.testExit === 0));
  }
  for (const [name, hash] of Object.entries(project.inputs.tests)) {
    assert.equal(sha(bytes('project-runtime/tests/' + name + '_test.cpp')), hash);
  }
  assert.equal(sha(bytes('request.md')), sha(fs.readFileSync(path.join(repo, proof.runtime.request))));
  for (const flag of ['coreAbiActivation', 'runtimeOrCompilerProductChanges', 'typesChanges', 'sdkOrCallerActivation']) {
    assert.equal(proof[flag], false);
  }
  assert.equal(proof.actualNativeSpriteFactory, 'NOTRUN');
  assert.equal(proof.actualNativeImage39, 'NOTRUN');
  console.log(`PASS ${files.length} evidence hashes; getter21/exact13/metadata62/revised83/generic15/fallback30/JSON8, project4 parity and accessor16 zero new; old expectations fail81/47, naive fallback fails15; activation/factory/image39 unqualified`);
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
