import { spawnSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// The fully collapsed quality alias, and the only command a contributor must remember.
//
// EVERY GATE RUNS, whatever the ones before it did. These gates are independent — a drifted ABI
// snapshot says nothing about build-system parity — so stopping at the first failure would hide the
// rest and turn one fix into several round trips.
//
// The native build is deliberately absent: CMake and ctest are the runtime's own gate and are run
// directly, not wrapped. This sweep covers what a reader cannot see by building.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const gates = [
  { arguments: [], name: 'abortOracle' },
  { arguments: [], name: 'abiHealth' },
  { arguments: [], name: 'arrayLikeOracle' },
  { arguments: [], name: 'base64Oracle' },
  { arguments: [], name: 'blobOracle' },
  { arguments: [], name: 'buildHealth' },
  { arguments: [], name: 'releaseHealth' },
  { arguments: [], name: 'exampleHealth' },
  { arguments: [], name: 'conditionalFacetOracle' },
  { arguments: [], name: 'headlessProfileOracle' },
  { arguments: [], name: 'runtimeOracle' },
  { arguments: [], name: 'sdlGlProfileOracle' },
  { arguments: [], name: 'structuralProxyOracle' },
  { arguments: [], name: 'streamOracle' },
  { arguments: [], name: 'uriOracle' },
  { arguments: ['--check'], name: 'sdkGeneration' },
  { arguments: [], name: 'emittedSourceCompile' },
];
const failed = [];

for (const gate of gates) {
  process.stdout.write(`\n### ${gate.name}\n`);
  const result = spawnSync(process.execPath, [path.join(root, 'scripts', `${gate.name}.mjs`), ...gate.arguments], {
    cwd: root,
    stdio: 'inherit',
  });
  if (result.status !== 0) failed.push(gate.name);
}

if (failed.length > 0) {
  process.stderr.write(`\n${String(failed.length)} of ${String(gates.length)} check gates failed: ${failed.join(', ')}\n`);
  process.exit(1);
}

process.stdout.write(`\n${String(gates.length)} check gates passed.\n`);
