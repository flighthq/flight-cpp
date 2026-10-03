import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { driftedOverrides, loadOverrides } from './overrides.mjs';

// Reports every hand-written override whose generated source has moved underneath it.
//
// An override is a COPY with edits, and a copy's hazard is that it keeps compiling after the thing it
// replaced has changed. `derivedFrom` records the sha256 of the generated file the override was written
// against, so a pin move that rewrites that file is visible here instead of being discovered later as a
// behavioral difference nobody can explain.
//
//   --generated=DIR   the generated tree to compare against (default: generated)

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const options = process.argv.slice(2);
const generated = options.find((option) => option.startsWith('--generated='))?.slice('--generated='.length);
// Default: `generated`, the COMMITTED inventory. It used to be `out/sdk-sdl`, which was the SDL-profiled
// side output back when `generated/` held the unbound tree. `sdk:generate` now applies the SDL profile set
// by default, so those two trees have identical content and `out/` is gitignored -- a default pointing
// there fails on a fresh clone for no reason.
const generatedRoot = path.resolve(root, generated ?? 'generated');

const overrides = loadOverrides(root);
if (overrides.length === 0) {
  process.stdout.write('No hand-written overrides are declared.\n');
  process.exit(0);
}
const drifted = driftedOverrides(overrides, generatedRoot);
process.stdout.write(
  `${String(overrides.length)} override(s) declared against ${portable(path.relative(root, generatedRoot))}.\n`,
);
for (const entry of overrides) process.stdout.write(`- ${entry.path} (${entry.id})\n`);
if (drifted.length === 0) {
  process.stdout.write('Each one still matches the generated file it was derived from.\n');
  process.exit(0);
}
process.stderr.write(`\n${String(drifted.length)} override(s) have drifted:\n`);
for (const entry of drifted) process.stderr.write(`- ${entry.id}: ${entry.reason}\n`);
process.stderr.write('Re-derive each from the current generated file, or drop it if it is no longer needed.\n');
process.exitCode = 1;

function portable(filename) {
  return filename.split(path.sep).join('/');
}
