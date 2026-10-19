import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const proof = JSON.parse(fs.readFileSync(path.join(repo, 'artifacts/sprite-readonly-object-activation.json')));
const archive = process.argv[2] ?? proof.archive.path;
assert.equal(sha(fs.readFileSync(archive)), proof.archive.sha256);
const members = execFileSync('tar', ['-tzf', archive], { encoding: 'utf8' }).trim().split('\n');
assert.equal(members.length, proof.archive.members);
assert(members.every(name => !path.isAbsolute(name) && !name.split('/').includes('..')));
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'sprite-activation-proof-'));
try {
  execFileSync('tar', ['-xzf', archive, '-C', temporary]);
  const bytes = name => fs.readFileSync(path.join(temporary, name));
  const json = name => JSON.parse(bytes(name));
  const files = json('checksums.json').files;
  assert.equal(files.length, proof.archive.payloadHashes);
  for (const file of files) {
    assert.equal(bytes(file.path).length, file.bytes, file.path);
    assert.equal(sha(bytes(file.path)), file.sha256, file.path);
  }
  assert.equal(sha(fs.readFileSync(path.join(repo, proof.runtime.path))), proof.runtime.sha256);
  const patch = path.join(repo, proof.compiler.patch);
  assert.equal(sha(fs.readFileSync(patch)), proof.compiler.sha256);
  const manifest = JSON.parse(fs.readFileSync(path.join(repo, 'source-patches/manifest.json')));
  const entry = manifest.patches.find(row => row.id === proof.compiler.manifestId);
  assert(entry && !entry.pendingRebase && entry.equivalence && entry.expires);
  assert.equal(entry.baseCommit, proof.compiler.baseCommit);
  const backend = 'packages/compiler-backend-cpp/src/cppCompilerBackend.ts';
  assert.equal(sha(bytes('compiler/baseline/' + backend)), proof.compiler.baselineBackendSha256);
  assert.equal(sha(bytes('compiler/candidate/' + backend)), proof.compiler.candidateBackendSha256);
  const target = path.join(temporary, 'reconstruct', backend);
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.writeFileSync(target, bytes('compiler/baseline/' + backend));
  execFileSync('git', ['apply', patch], { cwd: path.join(temporary, 'reconstruct') });
  assert.equal(sha(fs.readFileSync(target)), proof.compiler.candidateBackendSha256);
  assert(json('strict-builds.json').rows.every(row => row.exit === 0));
  assert.deepEqual(json('emission-results.json').strict, []);
  for (const row of json('header-comparison.json')) {
    assert(row.identical);
    assert.equal(row.currentSha256, row.reviewedSha256);
    assert.equal(sha(bytes('emission/' + row.variant + '/' + row.fixture + '.hpp')), row.currentSha256);
  }
  const runtime = json('runtime-results.json');
  assert.equal(runtime.length, 9);
  assert(runtime.every(row => row.exit === 0));
  for (const name of ['structural_row', 'merged_row', 'nominal_or_structural_view', 'optional_native_reference_any']) {
    assert.match(bytes('runtime-' + name + '-test.log').toString(), /100% tests passed, 0 tests failed out of 1/);
  }
  const parity = json('paired-parity.json');
  assert.deepEqual(parity.newFailures, []);
  assert(parity.rows.every(row => row.pass === 13 && row.fail === 3));
  assert.deepEqual(parity.rows[0].failures, parity.rows[1].failures);
  assert.equal(proof.review.verdict, 'PASS');
  assert.equal(proof.limits.typesChanges, false);
  assert.equal(proof.limits.sdkGenerated, false);
  console.log(`PASS ${files.length} activation evidence hashes; exact reviewed runtime, managed patch reconstruction, strict builds, project4 and accessor16 zero new (3 existing failures); full Sprite/SDK unqualified`);
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
