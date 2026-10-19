import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Validate bounded retained evidence; this does not qualify the native Sprite factory.
const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const readJson = file => JSON.parse(fs.readFileSync(file));
const proof = readJson(path.join(repo, 'artifacts/sprite-original-allocation-boundary.json'));
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const archive = process.argv[2] ?? proof.proofArchive.path;
assert.equal(sha(fs.readFileSync(archive)), proof.proofArchive.sha256);
const members = execFileSync('tar', ['-tzf', archive], { encoding: 'utf8' }).trim().split('\n');
assert.equal(members.length, proof.proofArchive.memberCount);
assert(members.every(name => !path.isAbsolute(name) && !name.split('/').includes('..')));
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'sprite-boundary-proof-'));
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
  assert.equal(sha(bytes('allocation-boundary/original-entity-ir.json')), proof.originalFunctions.IRSha256);
  assert.equal(sha(bytes('allocation-boundary/candidate-original-entity.hpp')), proof.originalFunctions.candidateHeaderSha256);
  assert.deepEqual(json('allocation-boundary/strict-original-source.json').diagnostics, []);
  const native = json('allocation-boundary/native-v2-results.json');
  assert.deepEqual(native, proof.native);
  assert(native.every(row => row.compileExitCode === 0));
  assert.equal(native[0].runExitCode, 0);
  assert.match(native[0].stdout, /PASS 58 ORIGINAL allocate\/finish/);
  assert.equal(native[1].runExitCode, 1);
  assert.match(native[1].stderr, /FAIL after 34 checks/);
  assert.equal(native[2].runExitCode, 1);
  assert.match(native[2].stderr, /FAIL after 1 checks/);
  const patch = proof.inactiveNativeEscapePatch;
  assert.equal(sha(fs.readFileSync(path.join(repo, patch.path))), patch.sha256);
  assert.equal(sha(bytes('baseline-inputs/include/flight/structural_ref.hpp')), patch.baselineHeaderSha256);
  assert.equal(sha(bytes('allocation-boundary/runtime/include/flight/structural_ref.hpp')), patch.candidateHeaderSha256);
  const reconstruct = path.join(temporary, 'reconstruct/include/flight');
  fs.mkdirSync(reconstruct, { recursive: true });
  fs.writeFileSync(path.join(reconstruct, 'structural_ref.hpp'), bytes('baseline-inputs/include/flight/structural_ref.hpp'));
  execFileSync('git', ['apply', path.join(repo, patch.path)], { cwd: path.join(temporary, 'reconstruct') });
  assert.equal(sha(fs.readFileSync(path.join(reconstruct, 'structural_ref.hpp'))), patch.candidateHeaderSha256);
  const proposal = proof.nestedDataABIReview;
  assert.equal(sha(fs.readFileSync(path.join(repo, proposal.proposal))), proposal.sha256);
  assert.equal(sha(bytes('abi-review-proposal.json')), proposal.sha256);
  const counterexample = json('abi-policy-counterexample-result.json');
  assert.equal(counterexample.compileExitCode, 0);
  assert.equal(counterexample.runExitCode, 0);
  assert.match(counterexample.stdout, /policy-mismatch=YES/);
  assert.match(counterexample.qualification, /DESIGN GAP/);
  const whole = json('whole-original-factory-v5/emission.json');
  assert.equal(whole.sourceCount, 1211);
  assert.deepEqual(whole.files.map(file => file.refused.length), [0, 0, 2, 2]);
  for (const source of proof.wholeOriginalEmissionV5.sourceEqualityV4) {
    assert.equal(source.v4Sha256, source.v5Sha256, source.path);
    assert.equal(sha(bytes('whole-original-factory-v5/' + source.path)), source.v5Sha256, source.path);
  }
  for (const variant of json('whole-original-factory-v5/strict-owned-diagnostics.json').variants) {
    assert.equal(variant.total, 220);
    assert.equal(variant.owned.length, 2);
    assert.equal(variant.owned[0].file, 'packages/scene2d/src/displayObject.ts');
    assert.equal(variant.owned[1].file, 'packages/scene2d/src/sprite.ts');
  }
  for (const flag of ['coreAbiActivation', 'typesChanges', 'sdkOrCallerActivation', 'valueABIMappingActivated']) {
    assert.equal(proof[flag], false, flag);
  }
  assert.equal(proof.sourceBodyReplacements, 0);
  assert.equal(proof.actualNativeSpriteFactory, 'NOTRUN');
  assert.equal(proof.actualNativeImage39, 'NOTRUN');
  console.log(`PASS ${files.length} proof hashes, allocation58/negative controls, exact ABI design gap and four-module emission; native Sprite/image39 NOTRUN`);
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
