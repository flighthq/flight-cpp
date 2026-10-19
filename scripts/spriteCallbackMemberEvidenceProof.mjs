import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const proof = JSON.parse(fs.readFileSync(path.join(repo, 'artifacts/sprite-callback-member-evidence-proof.json')));
const archive = process.argv[2] ?? proof.proofArchive.path;
assert.equal(sha(fs.readFileSync(archive)), proof.proofArchive.sha256);
const members = execFileSync('tar', ['-tzf', archive], { encoding: 'utf8' }).trim().split('\n');
assert.equal(members.length, proof.proofArchive.memberCount);
assert(members.every(name => !path.isAbsolute(name) && !name.split('/').includes('..')));
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'sprite-callback-proof-'));
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
  const relative = 'packages/compiler-semantic/src/typeScriptSemanticLowering.ts';
  const patch = path.join(repo, proof.compiler.patch);
  assert.equal(sha(fs.readFileSync(patch)), proof.compiler.sha256);
  assert.equal(sha(bytes('compiler/baseline/' + relative)), proof.compiler.semanticBaselineSha256);
  assert.equal(sha(bytes('compiler/candidate/' + relative)), proof.compiler.semanticCandidateSha256);
  const reconstructed = path.join(temporary, 'reconstruct', relative);
  fs.mkdirSync(path.dirname(reconstructed), { recursive: true });
  fs.writeFileSync(reconstructed, bytes('compiler/baseline/' + relative));
  execFileSync('git', ['apply', '--unidiff-zero', patch], { cwd: path.join(temporary, 'reconstruct') });
  assert.equal(sha(fs.readFileSync(reconstructed)), proof.compiler.semanticCandidateSha256);
  assert.equal(json('callback/compiler-build-result.json').exitCode, 0);
  const minimal = json('callback/results.json');
  assert.deepEqual(minimal.strict, []);
  assert(minimal.rows.every(row => row.emitted));
  assert(minimal.rows.find(row => row.label === 'baseline' && row.source.includes('/node/')).containsAny);
  assert(!minimal.rows.find(row => row.label === 'candidate' && row.source.includes('/node/')).containsAny);
  const native = json('callback/native-results.json');
  assert.equal(native[0].compileExit, 0);
  assert.equal(native[0].runExit, 1);
  assert.match(native[0].stderr, /FAIL after 4 checks/);
  assert.equal(native[1].compileExit, 0);
  assert.equal(native[1].runExit, 0);
  assert.match(native[1].stdout, /PASS 30 /);
  const parity = json('callback/parity.json');
  assert.deepEqual(parity.newFailures, []);
  assert(parity.rows.every(row => row.pass === 15 && row.fail === 1));
  assert.deepEqual(parity.rows[0].failures, parity.rows[1].failures);
  const original = json('callback/original-source-candidate-ir.json');
  assert.equal(original.sourceCount, 1211);
  assert.equal(json('callback/original-source-inputs.json').files.length, 1211);
  assert.equal(original.diagnostics.length, 2);
  assert(original.diagnostics.every(diagnostic => diagnostic.severity === 'warning'));
  const expression = json('callback/original-callback-candidate-expression-ir.json');
  assert.equal(expression.length, 1);
  const currentTraits = original.createNodeRuntime.typeParameters.find(parameter => parameter.binding.name === 'Traits');
  assert(currentTraits);
  for (const parameter of expression[0].optionalChain.valueType.parameters) {
    assert.equal(parameter.type.reference.binding.name, 'NodeOf');
    assert.equal(parameter.type.typeArguments[0].reference.binding.id, currentTraits.binding.id);
  }
  assert.equal(expression[0].type.kind, 'union');
  assert(expression[0].type.types.some(type => type.kind === 'undefined'));
  assert.match(bytes('original-baseline/callback-expression-ir.json').toString(), /"kind": "unknown"/);
  assert.equal(sha(bytes('request.md')), sha(fs.readFileSync(path.join(repo, proof.compiler.request))));
  for (const flag of ['runtimeOrCompilerProductChanges', 'typesChanges', 'sdkOrCallerActivation']) {
    assert.equal(proof[flag], false);
  }
  assert.equal(proof.actualNativeSpriteFactory, 'NOTRUN');
  assert.equal(proof.actualNativeImage39, 'NOTRUN');
  console.log(`PASS ${files.length} evidence hashes, actual emitted callback30, original1211 Traits binding retained, semantic16 zero new; factory/image39 unqualified`);
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
