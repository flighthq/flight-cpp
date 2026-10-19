import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Retained scratch evidence, never an actual Sprite factory/native ABI qualification.
const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const proof = JSON.parse(fs.readFileSync(path.join(repo, 'artifacts/sprite-factory-owner-contracts.json')));
const digest = value => createHash('sha256').update(value).digest('hex');
const archive = process.argv[2] ?? proof.proofArchive.path;
assert.equal(digest(fs.readFileSync(archive)), proof.proofArchive.sha256);
const members = execFileSync('tar', ['-tzf', archive], { encoding: 'utf8' }).trim().split('\n');
assert.equal(members.length, proof.proofArchive.memberCount);
assert(members.every(member => !path.isAbsolute(member) && !member.split('/').includes('..')));
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'sprite-owner-proof-'));
try {
  execFileSync('tar', ['-xzf', archive, '-C', temporary]);
  const read = name => fs.readFileSync(path.join(temporary, name));
  const json = name => JSON.parse(read(name));
  const checksums = json('checksums.json').files;
  assert.equal(checksums.length, proof.proofArchive.checksummedFileCount);
  for (const file of checksums) {
    const bytes = read(file.path);
    assert.equal(bytes.length, file.bytes, file.path);
    assert.equal(digest(bytes), file.sha256, file.path);
  }
  const backend = 'packages/compiler-backend-cpp/src/cppCompilerBackend.ts';
  for (const [index, source, baseline, target] of [
    [0, 'compiler-snapshots/baseline136/' + backend, '', 'compiler-snapshots/allocation/' + backend],
    [1, 'compiler-snapshots/baseline136/' + backend, 'artifacts/sprite-factory-emission-mechanism-compiler.patch', 'compiler-snapshots/nested-data/' + backend],
    [2, 'baseline-native-inputs/include/flight/structural_ref.hpp', '', 'nested-data/runtime/include/flight/structural_ref.hpp'],
  ]) {
    const patch = proof.inactivePatches[index];
    assert.equal(digest(fs.readFileSync(path.join(repo, patch.path))), patch.sha256);
    const reconstruction = path.join(temporary, 'reconstruct-' + index);
    const relative = index === 2 ? 'include/flight/structural_ref.hpp' : backend;
    fs.mkdirSync(path.dirname(path.join(reconstruction, relative)), { recursive: true });
    fs.writeFileSync(path.join(reconstruction, relative), read(source));
    if (baseline) execFileSync('git', ['apply', '--unidiff-zero', path.join(repo, baseline)], { cwd: reconstruction });
    assert.equal(digest(fs.readFileSync(path.join(reconstruction, relative))), patch.baselineSha256);
    execFileSync('git', ['apply', '--unidiff-zero', path.join(repo, patch.path)], { cwd: reconstruction });
    assert.equal(digest(fs.readFileSync(path.join(reconstruction, relative))), patch.candidateSha256);
    assert.equal(digest(read(target)), patch.candidateSha256);
  }
  const allocation = json('allocation/native-results.json');
  assert.equal(allocation[0].compileExitCode, 0);
  assert.equal(allocation[0].runExitCode, 1);
  assert.equal(allocation[1].runExitCode, 0);
  assert.match(allocation[1].stdout, /PASS 36 ORIGINAL allocate\/finish/);
  assert.deepEqual(json('allocation/strict-original-source.json').diagnostics, []);
  const controls = json('allocation/control-results.json');
  for (const baseline of controls.filter(row => row.label === 'baseline')) {
    const candidate = controls.find(row => row.label === 'candidate' && row.id === baseline.id);
    assert(candidate);
    if (baseline.id === 'empty_writable') assert.equal(candidate.fromOwner, true);
    else {
      assert.equal(candidate.sha256, baseline.sha256, baseline.id);
      assert.equal(candidate.rule, baseline.rule, baseline.id);
    }
  }
  assert.equal(json('nested-data/native-results.json')[0].runExitCode, 1);
  assert.match(json('nested-data/native-results.json')[1].stdout, /PASS 44 nested data/);
  assert.equal(json('nested-data/native-results.json')[1].runExitCode, 0);
  assert.equal(json('nested-data/emitted-candidate-v2-result.json').runExitCode, 0);
  assert.match(json('nested-data/emitted-candidate-v2-result.json').stdout, /PASS 15 actual emitted generic/);
  assert(json('nested-data/strict-minimal-controls.json').every(row => row.diagnostics.length === 0));
  const dataControls = json('nested-data/emission-control-results.json');
  for (const id of ['readonly_to_writable', 'full_native_data', 'unrelated_native_data', 'explicit_null_data', 'incompatible_callback']) {
    const before = dataControls.find(row => row.label === 'baseline' && row.id === id);
    const after = dataControls.find(row => row.label === 'candidate' && row.id === id);
    assert.equal(after.emitted, false, id);
    assert.equal(after.rule, before.rule, id);
  }
  assert.equal(digest(read('nested-data/controls/baseline-ordinary_row.hpp')),
    digest(read('nested-data/controls/candidate-ordinary_row.hpp')));
  const whole = json('whole-original-factory-v4/emission.json');
  assert.equal(whole.sourceCount, 1211);
  assert.deepEqual(whole.files.map(file => file.refused.length), [0, 2, 2]);
  assert.equal(proof.coreAbiActivation, false);
  assert.equal(proof.typesChanges, false);
  assert.equal(proof.sdkOrCallerActivation, false);
  assert.equal(proof.sourceBodyReplacements, 0);
  assert.equal(proof.actualNativeSpriteFactory, 'NOTRUN');
  assert.equal(proof.actualNativeImage39, 'NOTRUN');
  console.log(`PASS ${checksums.length} proof hashes, isolated patch reconstruction, allocation36/data44/emitted15 receipts; actual Sprite/image39 NOTRUN`);
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
