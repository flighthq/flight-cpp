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
  process.stdout.write('Instanced-mesh signals source oracle skipped: pinned Flight or compiler TypeScript installation is absent.\n');
  process.exit(0);
}
for (const args of [['rev-parse', 'HEAD'], ['status', '--porcelain']]) {
  const result = spawnSync('git', args, { cwd: flight.directory, encoding: 'utf8' });
  if (result.status !== 0 || result.stdout.trim() !== (args[0] === 'rev-parse' ? flight.commit : '')) {
    throw new Error('Instanced-mesh signals oracle requires the clean, exact pinned Flight checkout');
  }
}
const fixture = path.join(root, 'scripts/fixtures/instanced-signals');
function run(mutation = '') {
  return spawnSync(process.execPath, ['--test', path.join(fixture, 'cases.mjs')], {
    cwd: root, encoding: 'utf8',
    env: { ...process.env, FLIGHT_INSTANCE_SOURCE: flight.directory, FLIGHT_INSTANCE_TYPESCRIPT: typescriptPath,
      FLIGHT_INSTANCE_LOADER: path.join(fixture, 'loader.mjs'), FLIGHT_INSTANCE_MUTATION: mutation },
  });
}
function passing(label) {
  const result = run();
  process.stdout.write(`${label}\n${result.stdout}`);
  if (result.status !== 0) { process.stderr.write(result.stderr); throw new Error(`${label} failed`); }
}
passing('Exact pinned Flight Instanced signals control');
const patches = loadSourcePatches(root).filter((entry) => entry.dependency === 'flight');
try {
  applySourcePatches(patches, flight);
  passing('Instanced signals with the complete corrected source-patch stack');
  if (process.argv.includes('--mutations')) {
    for (const mutation of ['null-only', 'undefined-only', 'eager-factory', 'double-read', 'read-back']) {
      const result = run(mutation);
      if (result.status === 0 || !result.stdout.includes('ERR_ASSERTION')) {
        process.stderr.write(`${result.stdout}${result.stderr}`);
        throw new Error(`Instanced signals oracle did not detect ${mutation}`);
      }
      process.stdout.write(`Detected Instanced signals mutation: ${mutation}\n`);
    }
  }
} finally {
  revertSourcePatches(patches, flight);
}
process.stdout.write('Instanced-mesh signals source semantics preserved; this oracle makes no native compilation claim.\n');
