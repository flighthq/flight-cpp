import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Verify the frozen evidence, including failures; never turn a source-only pass
// or an earlier native component into a public factory/consumer qualification.
const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const proof = JSON.parse(fs.readFileSync(path.join(repo, 'artifacts/sprite-image-consumer-preparation.json')));
const archive = process.argv[2] ?? proof.proofArchive.path;
const digest = bytes => createHash('sha256').update(bytes).digest('hex');
assert.equal(digest(fs.readFileSync(archive)), proof.proofArchive.sha256);
const members = execFileSync('tar', ['-tzf', archive], {encoding:'utf8'}).trim().split('\n');
assert.equal(members.length, proof.proofArchive.members);
assert(members.every(name => !path.isAbsolute(name) && !name.split('/').includes('..')));
const scratch = fs.mkdtempSync(path.join(path.dirname(path.resolve(archive)), 'image-proof-verify-'));
try {
  execFileSync('tar', ['-xzf', archive, '-C', scratch]);
  const read = name => fs.readFileSync(path.join(scratch, name));
  const json = name => JSON.parse(read(name));
  const hashes = json('checksums.json').files;
  assert.equal(hashes.length, proof.proofArchive.checksummedFiles);
  for (const item of hashes) {
    const bytes = read(item.path);
    assert.equal(bytes.length, item.bytes, item.path);
    assert.equal(digest(bytes), item.sha256, item.path);
  }
  const native = json('evidence/native-baseline-result.json');
  assert.equal(native.atCommit, proof.testedCommit);
  assert.equal(native.compileExitCode, 1);
  assert.equal(native.inputsUnchanged, true);
  assert.match(read('evidence/native-baseline.log').toString(),
    /address of overloaded function 'create_node_2d_runtime' cannot be static_cast to type 'flight::Any'/u);
  const index = json('evidence/native-snapshot-index.json');
  assert.equal(index.length, proof.nativeInputHashCount);
  for (const item of index) assert.equal(digest(read(item.path)), item.sha256, item.path);
  for (const [name, count] of [['factory-raw',39],['factory-frozen',39],
    ['image-source-raw',16],['image-frozen',16]]) {
    const report = json('results/'+name+'.json');
    assert.equal(report.checks,count);
    assert.equal(report.failures,0);
    for (const item of report.executedSources) {
      const variant = name.endsWith('raw') ? 'raw' : 'frozen';
      assert.equal(digest(read('sources/'+variant+'/'+item.source)),item.sha256);
    }
  }
  for (const variant of ['raw','frozen']) {
    const report = json('results/'+(variant==='raw'?'image-source-raw':'image-frozen')+'.json');
    for (const item of report.sourceInputs)
      assert.equal(digest(read('sources/'+variant+'/'+item.source)),item.sha256);
  }
  const emitted = json('input/whole-emission-summary.json');
  assert.equal(emitted.sourceCount,1211);
  assert.deepEqual(emitted.files.map(file=>file.refused.length),[0,0,2,2]);
  assert(json('input/strict-owned-diagnostics.json').variants.every(variant=>
    variant.total===220 && variant.owned.length===2));
  assert(json('evidence/source-equality.json').every(item=>item.matchesSuppliedBuilder4SourceBytes));
  assert.deepEqual(json('evidence/source-equality.json').map(item=>item.rawTranspiledJavaScriptTokensEqual),
    [false,true,false]);
  assert.equal(proof.exactTypedReadonlyPartialDataGetter.reached,false);
  assert.equal(proof.firstFailure.runtimeChecksExecuted,0);
  assert.equal(proof.firstFailure.run,'NOTRUN');
  assert.equal(proof.generationRuns,0);
  assert.equal(proof.sourceBodiesRewritten,0);
  assert.equal(proof.nativeOwnersFabricatedByFixture,0);
  for (const [local, saved] of [
    ['artifacts/sprite-image-public-consumer.cpp','consumer/sprite-image-public-consumer.cpp'],
    ['scripts/spriteImageConsumerSourceOracle.mjs','scripts/spriteImageConsumerSourceOracle.mjs'],
    ['scripts/spriteFactorySourceOracle.mjs','scripts/spriteFactorySourceOracle.mjs'],
  ]) assert.equal(digest(fs.readFileSync(path.join(repo,local))),digest(read(saved)),local);
  console.log(`PASS ${hashes.length} proof hashes, ${index.length} frozen native inputs, source39+39/16+16; native compile FAIL and data getter NOT REACHED retained`);
} finally {
  fs.rmSync(scratch,{recursive:true,force:true});
}
