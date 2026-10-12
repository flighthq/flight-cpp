import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { after, test } from 'node:test';

import { applySourcePatches, recoverStalePatches, revertSourcePatches } from './sourcePatches.mjs';

// Run with `npm run pkg:test`. Each case builds a throwaway pinned checkout, so nothing here touches
// .dependencies/.

const scratch = mkdtempSync(path.join(tmpdir(), 'flight-cpp-source-patches-'));
after(() => rmSync(scratch, { force: true, recursive: true }));

function git(directory, ...args) {
  const result = spawnSync('git', args, { cwd: directory, encoding: 'utf8' });
  assert.equal(result.status, 0, result.stderr);
  return result.stdout.trim();
}

// A checkout with one source file, plus a patch adding `line` after its first line.
function fixture(name) {
  const root = path.join(scratch, name);
  const directory = path.join(root, 'flight');
  mkdirSync(directory, { recursive: true });
  git(directory, 'init', '-q');
  git(directory, 'config', 'user.email', 'test@example.invalid');
  git(directory, 'config', 'user.name', 'test');
  writeFileSync(path.join(directory, 'module.ts'), 'export const a = 1;\nexport const b = 2;\nexport const c = 3;\n');
  git(directory, 'add', '.');
  git(directory, 'commit', '-qm', 'base');
  const commit = git(directory, 'rev-parse', 'HEAD');
  const patchFile = (line) => {
    const file = path.join(root, `${line}.patch`);
    writeFileSync(
      file,
      `diff --git a/module.ts b/module.ts\n--- a/module.ts\n+++ b/module.ts\n@@ -1,3 +1,4 @@\n export const a = 1;\n+${line}\n export const b = 2;\n export const c = 3;\n`,
    );
    return file;
  };
  const dependency = { commit, directory, name: 'flight' };
  const patch = (id, file) => ({ baseCommit: commit, dependency: 'flight', file, id, module: 'module.ts', package: '@flighthq/x' });
  return { dependency, directory, patch, patchFile, root };
}

const status = (directory) => git(directory, 'status', '--porcelain');

test('a patch file rewritten while the patch is applied still reverts cleanly', () => {
  const { dependency, directory, patch, patchFile } = fixture('rewritten');
  const file = patchFile('export const applied = true;');
  const patches = [patch('p', file)];
  applySourcePatches(patches, dependency);
  assert.match(readFileSync(path.join(directory, 'module.ts'), 'utf8'), /applied/u);
  // A fast-forward replaces the declared patch with a different version mid-run.
  writeFileSync(file, readFileSync(patchFile('export const rewritten = true;')));
  revertSourcePatches(patches, dependency);
  assert.equal(status(directory), '');
});

test('a dead run is recovered even after its patch was renamed in the manifest', () => {
  const { dependency, directory, patch, patchFile, root } = fixture('retired');
  applySourcePatches([patch('old-id', patchFile('export const old = true;'))], dependency);
  // The run dies (SIGKILL): its marker names a pid that no longer exists.
  const marker = path.join(root, '.flight-source-patches.json');
  const owner = JSON.parse(readFileSync(marker, 'utf8'));
  writeFileSync(marker, JSON.stringify({ ...owner, pid: 2 ** 22 + 12345 }));
  // The manifest now declares the same change under another id, at different bytes.
  const current = [patch('new-id', patchFile('export const renamed = true;'))];
  const recovery = recoverStalePatches(current, dependency);
  assert.deepEqual(recovery.recovered, ['old-id']);
  assert.equal(status(directory), '');
});
