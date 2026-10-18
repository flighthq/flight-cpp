import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Validate retained evidence only. This does not run or qualify the native Sprite factory.
const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const proof = JSON.parse(fs.readFileSync(path.join(repo, 'artifacts/sprite-factory-emission-mechanism.json')));
const digest = bytes => createHash('sha256').update(bytes).digest('hex');
const archive = process.argv[2] ?? proof.proofArchive.path;
assert.equal(digest(fs.readFileSync(archive)), proof.proofArchive.sha256, 'proof archive changed');
const members = execFileSync('tar', ['-tzf', archive], { encoding: 'utf8', maxBuffer: 8 * 1024 * 1024 }).trim().split('\n');
assert.equal(members.length, proof.proofArchive.memberCount);
assert(members.every(member => !path.isAbsolute(member) && !member.split('/').includes('..')));
const extracted = fs.mkdtempSync(path.join(os.tmpdir(), 'sprite-emission-proof-'));
try {
  execFileSync('tar', ['-xzf', archive, '-C', extracted]);
  const json = relative => JSON.parse(fs.readFileSync(path.join(extracted, relative)));
  const checksums = json('checksums.json');
  assert.equal(checksums.files.length, proof.proofArchive.checksummedFileCount);
  for (const file of checksums.files) {
    const bytes = fs.readFileSync(path.join(extracted, file.path));
    assert.equal(bytes.length, file.bytes, file.path);
    assert.equal(digest(bytes), file.sha256, file.path);
  }
  const patch = fs.readFileSync(path.join(repo, proof.inactiveCompilerPatch.path));
  assert.equal(digest(patch), proof.inactiveCompilerPatch.sha256);
  assert.equal(proof.inactiveCompilerPatch.archivedContextPatchSha256,
    digest(fs.readFileSync(path.join(extracted, 'experiments/inactive-constrained-native-literal.patch'))));
  const backend = 'packages/compiler-backend-cpp/src/cppCompilerBackend.ts';
  const baselineCompiler = path.join(extracted, 'baseline-compiler136');
  assert.equal(digest(fs.readFileSync(path.join(baselineCompiler, backend))), proof.inactiveCompilerPatch.baselineBackendSha256);
  execFileSync('git', ['apply', '--unidiff-zero', path.join(repo, proof.inactiveCompilerPatch.path)], { cwd: baselineCompiler });
  const patched = digest(fs.readFileSync(path.join(baselineCompiler, backend)));
  assert.equal(patched, proof.inactiveCompilerPatch.candidateBackendSha256);
  assert.equal(patched, digest(fs.readFileSync(path.join(extracted, 'candidate-compiler136', backend))));
  const native = json('experiments/native-receipts.json');
  assert.equal(native.compilerStrictBuild.exitCode, 0);
  assert.equal(native.baselineLiteralNativeCompile.exitCode, 1);
  assert.equal(native.candidateLiteralNativeCompile.exitCode, 0);
  assert.equal(native.candidateLiteralNativeRun.exitCode, 0);
  assert.match(native.candidateLiteralNativeRun.stdout, /PASS 11 native owner\/cell\/callback\/lifetime checks/);
  assert.equal(native.wrongCallbackNativeCompile.exitCode, 1);
  assert.equal(native.sourceOrder.runExitCode, 0);
  assert.equal(native.readonlyDataCounterexample.runExitCode, 0);
  assert.match(native.readonlyDataCounterexample.stdout, /first data read: std::bad_cast/);
  const controls = json('experiments/negative-control-results.json');
  assert.equal(controls.length, 18);
  for (const original of controls.filter(control => control.label === 'baseline')) {
    const candidate = controls.find(control => control.label === 'candidate' && control.id === original.id);
    assert(candidate);
    assert.equal(candidate.emitted, original.emitted, original.id);
    if (original.id === 'wrong_callback_owner') {
      assert.equal(candidate.newHelperUsed, true);
    } else {
      assert.equal(candidate.sha256, original.sha256, original.id);
      assert.equal(candidate.rule, original.rule, original.id);
      assert.equal(candidate.newHelperUsed, original.newHelperUsed, original.id);
    }
  }
  const strict = json('experiments/v3-typescript.json');
  assert.equal(strict.tokenEqual.length, 3);
  assert(strict.tokenEqual.every(file => file.transpiledJavaScriptTokensEqual));
  assert.deepEqual(strict.variants.map(variant => [variant.diagnostics.length, variant.owned.length]), [[220, 2], [220, 2]]);
  const emission = json('builder4-sprite-mechanism-probe-v3/emission.json');
  assert.equal(emission.sourceCount, 1211);
  assert.equal(emission.files.length, 3);
  assert.deepEqual(emission.files.map(file => file.refused.length), [0, 2, 2]);
  const emitted = relative => fs.readFileSync(path.join(extracted, 'builder4-sprite-mechanism-probe-v3/include', relative), 'utf8');
  assert.match(emitted('flight/node/node.hpp'), /inline R create_node_runtime\(/);
  assert.match(emitted('flight/scene2d/display_object.hpp'), /inline N create_node_2d\(/);
  assert.match(emitted('flight/scene2d/display_object.hpp'), /inline R create_node_2d_runtime\(/);
  assert.match(emitted('flight/scene2d/sprite.hpp'), /inline flight::Ref<flight::types::Sprite> create_sprite\(/);
  assert.match(emitted('flight/scene2d/sprite.hpp'), /inline flight::Ref<flight::types::SpriteRuntime> create_sprite_runtime\(/);
  assert.deepEqual(json('experiments/four-original-refusals.json'), proof.fourOriginalRefusals);
  assert.equal(proof.actualNativeSpriteFactory, 'NOTRUN');
  assert.equal(proof.actualNativeImage39, 'NOTRUN');
  console.log(`PASS ${checksums.files.length} proof file hashes and bounded mechanism receipts; actual native Sprite/image39 remain NOTRUN`);
} finally {
  fs.rmSync(extracted, { recursive: true, force: true });
}
