import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { resolveDependency } from './dependencyLock.mjs';
import { applySourcePatches, loadSourcePatches, revertSourcePatches } from './sourcePatches.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const flight = resolveDependency(root, 'flight');
const compiler = resolveDependency(root, 'flight-compiler');
const typescriptPath = path.join(compiler.directory, 'node_modules', 'typescript');
if (!existsSync(path.join(flight.directory, '.git')) || !existsSync(typescriptPath)) {
  process.stdout.write('Scene3D KindUsage source oracle skipped: pinned Flight or compiler TypeScript installation is absent.\n');
  process.exit(0);
}
for (const args of [['rev-parse', 'HEAD'], ['status', '--porcelain']]) {
  const result = spawnSync('git', args, { cwd: flight.directory, encoding: 'utf8' });
  if (result.status !== 0 || result.stdout.trim() !== (args[0] === 'rev-parse' ? flight.commit : '')) {
    throw new Error('Scene3D KindUsage oracle requires the clean, exact pinned Flight checkout');
  }
}
const fixture = path.join(root, 'scripts/fixtures/scene3d-kind-usage');
function run(mutation = '') {
  return spawnSync(process.execPath, ['--test', path.join(fixture, 'cases.mjs')], {
    cwd: root, encoding: 'utf8',
    env: { ...process.env, FLIGHT_KIND_SOURCE: flight.directory, FLIGHT_KIND_TYPESCRIPT: typescriptPath,
      FLIGHT_KIND_LOADER: path.join(fixture, 'loader.mjs'), FLIGHT_KIND_MUTATION: mutation },
  });
}
function passing(label) {
  const result = run();
  process.stdout.write(`${label}\n${result.stdout}`);
  if (result.status !== 0) { process.stderr.write(result.stderr); throw new Error(`${label} failed`); }
}
passing('Exact pinned Flight KindUsage control');
const patches = loadSourcePatches(root).filter((entry) => entry.dependency === 'flight');
try {
  applySourcePatches(patches, flight);
  passing('KindUsage with the complete corrected source-patch stack');
  if (process.argv.includes('--mutations')) {
    for (const mutation of ['double-read', 'nullish-skip', 'own-only']) {
      const result = run(mutation);
      if (result.status === 0 || !result.stdout.includes('ERR_ASSERTION')) {
        process.stderr.write(`${result.stdout}${result.stderr}`);
        throw new Error(`KindUsage oracle did not detect ${mutation}`);
      }
      process.stdout.write(`Detected KindUsage mutation: ${mutation}\n`);
    }
  }
} finally {
  revertSourcePatches(patches, flight);
}
process.stdout.write('Scene3D KindUsage source semantics preserved; this oracle makes no native compilation claim.\n');
