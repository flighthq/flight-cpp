import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Validate frozen observations and reconstruct the compiler candidate. This is
// an evidence check, not a replacement for the recorded compiler/native runs.
const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const proof = JSON.parse(fs.readFileSync(path.join(repo, 'artifacts/sprite-generic-function-value-proof.json')));
const archive = process.argv[2] ?? proof.proofArchive.path;
const digest = bytes => createHash('sha256').update(bytes).digest('hex');
assert.equal(digest(fs.readFileSync(archive)), proof.proofArchive.sha256);
const members = execFileSync('tar', ['-tzf', archive], { encoding: 'utf8' }).trim().split('\n');
assert.equal(members.length, proof.proofArchive.members);
assert(members.every(name => !path.isAbsolute(name) && !name.split('/').includes('..')));
const scratch = fs.mkdtempSync(path.join(path.dirname(path.resolve(archive)), 'generic-function-proof-'));
try {
  execFileSync('tar', ['-xzf', archive, '-C', scratch]);
  const read = name => fs.readFileSync(path.join(scratch, name));
  const json = name => JSON.parse(read(name));
  const hashes = json('checksums.json').files;
  assert.equal(hashes.length, proof.proofArchive.checksummedFiles);
  for (const item of hashes) {
    assert.equal(read(item.path).length, item.bytes, item.path);
    assert.equal(digest(read(item.path)), item.sha256, item.path);
  }
  const index = json('evidence/native-snapshot-index.json');
  assert.equal(index.length, proof.native.inputCount);
  for (const item of index) assert.equal(digest(read(item.path)), item.sha256, item.path);
  assert.equal(json('evidence/native-freeze.json').atCommit, proof.native.testedCommit);
  const composition = json('evidence/composition.json');
  assert.equal(composition.pin, proof.composition.compilerPin);
  assert.equal(composition.patches.length, 142);
  for (const patch of composition.patches)
    assert.equal(digest(read('composition-patches/' + patch.id + '.patch')), patch.digest, patch.id);

  const backend = 'packages/compiler-backend-cpp/src/cppCompilerBackend.ts';
  const test = 'packages/compiler-backend-cpp/src/cppGenericFunctionValueAssertion.test.ts';
  assert.equal(digest(read('compiler-baseline/' + backend)), proof.composition.baselineBackendSha256);
  assert.equal(digest(read('compiler-candidate/' + backend)), proof.composition.candidateBackendSha256);
  const reconstructed = path.join(scratch, 'reconstructed');
  fs.mkdirSync(path.dirname(path.join(reconstructed, backend)), { recursive: true });
  fs.writeFileSync(path.join(reconstructed, backend), read('compiler-baseline/' + backend));
  execFileSync('git', ['apply', '--check', path.join(repo, 'artifacts/sprite-generic-function-value-compiler.patch')], { cwd: reconstructed });
  execFileSync('git', ['apply', path.join(repo, 'artifacts/sprite-generic-function-value-compiler.patch')], { cwd: reconstructed });
  for (const name of [backend, test])
    assert.equal(digest(fs.readFileSync(path.join(reconstructed, name))), digest(read('compiler-candidate/' + name)), name);

  const baseline = json('emitted/baseline142/results.json').results;
  const candidate = json('emitted/candidate142/results.json').results;
  assert.equal(baseline.length, 19);
  assert.equal(candidate.length, 19);
  assert([...baseline, ...candidate].every(result => result.strict.length === 0));
  for (const name of proof.semantic.supported) {
    assert(baseline.find(result => result.name === name).emitted);
    assert(candidate.find(result => result.name === name).emitted);
    assert.match(read('emitted/baseline142/probe/' + name + '.hpp').toString(), /static_cast<flight::Any>\(identity\)/u);
    const output = read('emitted/candidate142/probe/' + name + '.hpp').toString();
    assert.match(output, /identity<R(?:, double)?>/u);
    assert.doesNotMatch(output, /static_cast<flight::Any>|generic_native_owner/u);
  }
  for (const name of proof.semantic.refused) {
    const result = candidate.find(result => result.name === name);
    assert.equal(result.emitted, false);
    assert.equal(result.rule, 'cpp-generic-callable-assertion-source-unproven');
  }
  for (const item of json('evidence/unchanged-controls.json')) {
    assert(item.byteIdentical);
    for (const side of ['baseline142', 'candidate142'])
      assert.equal(digest(read('emitted/' + side + '/probe/' + item.name + '.hpp')), item.sha256);
  }
  const native = json('evidence/native-and-regression-results.json');
  for (const [label, exit] of [['native-baseline142-compile', 1], ['native-candidate142-compile', 0], ['native-candidate142-run', 0]])
    assert.equal(native.find(result => result.label === label).exitCode, exit);
  assert.match(read('evidence/native-baseline142-compile.log').toString(), /address of overloaded function 'identity' cannot be static_cast to type 'flight::Any'/u);
  assert.match(read('evidence/native-candidate142-run.log').toString(), /PASS 27 /u);
  assert.match(read('evidence/candidate-focused-tests.log').toString(), /19 passed/u);
  const source = json('evidence/source-oracle-result.json');
  assert.equal(source.checks, 38);
  assert.equal(source.failures, 0);
  for (const item of source.inputs) assert.equal(digest(read('source/' + item.path)), item.sha256);
  const parity = json('evidence/paired-regression.json');
  assert.deepEqual(parity.baseline, parity.candidate);
  assert.equal(parity.candidate.pass, 13);
  assert.equal(parity.candidate.fail, 1);
  assert.deepEqual(parity.newFailures, []);
  assert(parity.inputsUnchanged);
  const staticCheck = json('evidence/static-comparison.json');
  assert.equal(staticCheck.baselineExit, 1);
  assert.equal(staticCheck.candidateExit, 1);
  assert(staticCheck.failedGatesSame && staticCheck.normalizedLintAndTypeDiagnosticsIdentical);
  for (const name of ['sprite-generic-function-value-compiler.patch', 'sprite-generic-function-value-native.cpp'])
    assert.equal(digest(fs.readFileSync(path.join(repo, 'artifacts', name))), digest(read('repo/artifacts/' + name)), name);
  assert.equal(proof.activation.sourcePatchManifestChanged, false);
  assert.equal(proof.originalPublicFactory.nativeQualified, false);
  assert.equal(proof.activation.sdkGenerationRuns, 0);
  console.log(`PASS ${hashes.length} evidence hashes, ${index.length} frozen native inputs, exact compiler patch reconstruction; focused19/native27/source38 and paired13+1 retained`);
} finally {
  fs.rmSync(scratch, { recursive: true, force: true });
}
