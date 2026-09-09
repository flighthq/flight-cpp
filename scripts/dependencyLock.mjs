import { readFileSync } from 'node:fs';
import path from 'node:path';

// The lock is the only place a sibling repository's identity is written down. Rehydration and every
// gate that reads a rehydrated checkout resolve paths through here, so a pin can never be half
// applied: one file names the repository, the branch it tracks, and the exact commit in use.

const COMMIT_PATTERN = /^[0-9a-f]{40}$/u;
const SCHEMA = 'flight-dependency-lock/1';

export function readDependencyLock(root) {
  const lockFile = path.join(root, 'dependencies.lock.json');
  const lock = JSON.parse(readFileSync(lockFile, 'utf8'));
  const failures = [];

  if (lock.schema !== SCHEMA) failures.push(`unsupported lock schema ${String(lock.schema)}, expected ${SCHEMA}`);
  if (typeof lock.directory !== 'string' || lock.directory.length === 0) {
    failures.push('lock does not name a rehydration directory');
  }
  if (!Array.isArray(lock.dependencies) || lock.dependencies.length === 0) {
    failures.push('lock declares no dependencies');
  }

  const dependencies = [];
  const seen = new Set();
  let previous = '';
  for (const dependency of lock.dependencies ?? []) {
    const name = String(dependency.name ?? '');
    if (name.length === 0) failures.push('a dependency has no name');
    if (name < previous) failures.push('dependencies must be sorted by name');
    previous = name;
    if (seen.has(name)) failures.push(`duplicate dependency ${name}`);
    seen.add(name);
    if (!String(dependency.repository ?? '').startsWith('https://')) {
      failures.push(`${name} must be pinned to an https repository`);
    }
    if (!COMMIT_PATTERN.test(String(dependency.commit ?? ''))) {
      failures.push(`${name} is not pinned to a full 40-character commit`);
    }
    if (String(dependency.branch ?? '').length === 0) failures.push(`${name} does not name a tracking branch`);
    dependencies.push({
      branch: String(dependency.branch ?? ''),
      commit: String(dependency.commit ?? ''),
      directory: path.join(root, String(lock.directory ?? '.dependencies'), name),
      name,
      repository: String(dependency.repository ?? ''),
    });
  }

  if (failures.length > 0) {
    process.stderr.write(`dependencies.lock.json is invalid:\n`);
    for (const failure of failures) process.stderr.write(`- ${failure}\n`);
    process.exit(1);
  }

  return { dependencies, directory: String(lock.directory), lockFile };
}

export function resolveDependency(root, name) {
  const dependency = readDependencyLock(root).dependencies.find((entry) => entry.name === name);
  if (!dependency) throw new Error(`dependencies.lock.json does not pin ${name}`);
  return dependency;
}
