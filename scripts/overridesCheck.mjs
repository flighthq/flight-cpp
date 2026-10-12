import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { driftedOverrides, loadOverrides, overridesIncludeDirectory, unresolvedHashReferences } from './overrides.mjs';

// Reports every hand-written override whose generated source has moved underneath it, and every one that
// names an anonymous structural shape the generated tree no longer carries.
//
// An override is a COPY with edits, and a copy's hazard is that it keeps compiling after the thing it
// replaced has changed. `derivedFrom` records the sha256 of the generated file the override was written
// against, so a pin move that rewrites that file is visible here instead of being discovered later as a
// behavioral difference nobody can explain.
//
// The hash check answers the case `derivedFrom` cannot see: an override that names an emitted anonymous
// shape (`binding_uid_06e64a362b012722`) is derived from the SAME file byte for byte while the name it
// borrowed from `flight/types` moves underneath it, so nothing has drifted and the override still fails
// to compile. Four such references were found when @flighthq/updater and @flighthq/shortcut were
// regenerated at a627849.
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
const unresolved = unresolvedHashReferences(overrides, overridesIncludeDirectory(root), generatedRoot);
process.stdout.write(
  `${String(overrides.length)} override(s) declared against ${portable(path.relative(root, generatedRoot))}.\n`,
);
for (const entry of overrides) process.stdout.write(`- ${entry.path} (${entry.id})\n`);
if (drifted.length === 0 && unresolved.length === 0) {
  process.stdout.write('Each one still matches the generated file it was derived from.\n');
  process.exit(0);
}
if (drifted.length > 0) {
  process.stderr.write(`\n${String(drifted.length)} override(s) have drifted:\n`);
  for (const entry of drifted) process.stderr.write(`- ${entry.id}: ${entry.reason}\n`);
  process.stderr.write('Re-derive each from the current generated file, or drop it if it is no longer needed.\n');
}
if (unresolved.length > 0) {
  process.stderr.write(
    `\n${String(unresolved.length)} hash-suffixed identifier(s) are referenced and exist nowhere in ` +
      `${portable(path.relative(root, generatedRoot))}:\n`,
  );
  for (const entry of unresolved) process.stderr.write(`- ${entry.id}: ${entry.identifier} (${entry.path})\n`);
  process.stderr.write(
    'Re-point each at the shape the pin names now -- or, better, take it from the public contract ' +
      '(a variant alternative, a row object type) so the next pin move cannot break it again.\n',
  );
}
process.exitCode = 1;

function portable(filename) {
  return filename.split(path.sep).join('/');
}
