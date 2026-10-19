import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const proof = JSON.parse(fs.readFileSync(path.join(repo, 'artifacts/sprite-existing-row-cell-value-path.json')));
const archive = process.argv[2] ?? proof.proofArchive.path;
assert.equal(sha(fs.readFileSync(archive)), proof.proofArchive.sha256);
const members = execFileSync('tar', ['-tzf', archive], { encoding: 'utf8' }).trim().split('\n');
assert.equal(members.length, proof.proofArchive.memberCount);
assert(members.every(name => !path.isAbsolute(name) && !name.split('/').includes('..')));
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'sprite-cell-proof-'));
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
  for (const [patch, source, target, relative, before, after] of [
    [proof.runtime.patch, 'baseline/structural_ref.hpp', 'existing-path/runtime/include/flight/structural_ref.hpp',
      'include/flight/structural_ref.hpp', proof.runtime.baselineSha256, proof.runtime.candidateSha256],
    [proof.emitter.patch, 'compiler/baseline/' + backend, 'compiler/candidate/' + backend,
      backend, proof.emitter.inputs.baselineSha256, proof.emitter.inputs.candidateSha256],
  ]) {
    const contents = fs.readFileSync(path.join(repo, patch));
    assert.equal(sha(contents), patch === proof.runtime.patch ? proof.runtime.sha256 : proof.emitter.sha256);
    assert.equal(sha(bytes(source)), before);
    const directory = fs.mkdtempSync(path.join(temporary, 'reconstruct-'));
    fs.mkdirSync(path.dirname(path.join(directory, relative)), { recursive: true });
    fs.writeFileSync(path.join(directory, relative), bytes(source));
    execFileSync('git', ['apply', path.join(repo, patch)], { cwd: directory });
    assert.equal(sha(fs.readFileSync(path.join(directory, relative))), after);
    assert.equal(sha(bytes(target)), after);
  }
  const reemission = json('existing-path/getter-reemission-results.json');
  assert.deepEqual(reemission.strict, []);
  assert(reemission.rows.every(row => row.emitted && row.diagnostics.length === 0));
  for (const name of ['getter', 'getter-controls']) {
    const baseline = bytes('existing-path/getter-baseline/' + name + '.hpp').toString();
    const candidate = bytes('existing-path/getter-candidate/' + name + '.hpp').toString();
    const erasedOverride = / \[\[nodiscard\]\] std::optional<flight::Any> as_any\(\) const override \{ auto value = getter_\(\); (?:if \(!value.has_value\(\)\) return flight::Any\(flight::null\); )?return flight::detail::row_cell_value_as_any\(value\); \}/g;
    assert.equal(candidate.replace(erasedOverride, ''), baseline, name + ' only adds the approved accessor override');
  }
  assert.equal(bytes('existing-path/getter-baseline/generated-getter-native.cpp').toString(),
    bytes('existing-path/getter-candidate/generated-getter-native.cpp').toString());
  for (const [name, count] of [
    ['emitted-result.json', 15], ['policy-v2-result.json', 83],
    ['getter-candidate/native-result.json', 21], ['getter-candidate/controls-result.json', 29],
  ]) {
    const result = json('existing-path/' + name);
    assert.equal(result.compileExitCode, 0);
    assert.equal(result.runExitCode, 0);
    assert.match(result.stdout, new RegExp('PASS ' + count + ' '));
  }
  const baseline = json('existing-path/generated-getter-result.json');
  assert.equal(baseline.runExitCode, 1);
  assert.match(baseline.stderr, /FAIL after 21 checks/);
  const parity = json('existing-path/getter-tests-paired-parity.json');
  assert.deepEqual(parity.newFailures, []);
  assert(parity.rows.every(row => row.pass === 13 && row.fail === 3));
  assert.deepEqual(parity.rows[0].failures, parity.rows[1].failures);
  const scope = json('existing-path/named-properties-comparison-results.json');
  assert.match(scope.result, /FORBIDDEN/);
  assert.match(scope.rows[0].stdout, /marked=0 represented=0 UnrepresentedProperty=1/);
  assert.match(scope.rows[0].stdout, /marked=1 represented=0 UnrepresentedProperty=1/);
  assert.match(scope.rows[1].stdout, /marked=0 represented=1 UnrepresentedProperty=0/);
  assert.match(scope.rows[1].stdout, /marked=1 represented=1 UnrepresentedProperty=0/);
  for (const [name, count] of [['copied-finish-owner', 12], ['eager-finish-getter', 11]]) {
    const result = json('allocation/' + name + '-result.json');
    assert.equal(result.compileExitCode, 0);
    assert.equal(result.runExitCode, 1);
    assert.match(result.stderr, new RegExp('FAIL after ' + count + ' checks'));
  }
  assert.equal(sha(fs.readFileSync(path.join(repo, proof.nextNarrowDesign.path))), proof.nextNarrowDesign.sha256);
  for (const flag of ['coreAbiActivation', 'runtimeOrCompilerProductChanges', 'typesChanges', 'sdkOrCallerActivation']) {
    assert.equal(proof[flag], false);
  }
  assert.equal(proof.actualNativeSpriteFactory, 'NOTRUN');
  assert.equal(proof.actualNativeImage39, 'NOTRUN');
  console.log(`PASS ${files.length} evidence hashes, emitted15/policy83/getter21/controls29, parity zero new; v1 broadening FORBIDDEN, factory/image39 NOTRUN`);
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
