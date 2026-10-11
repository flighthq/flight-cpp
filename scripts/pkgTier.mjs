import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { packageOutputDirectory, parsePackageArguments, portable } from './packageScope.mjs';

// `npm run pkg:tier -- <package>` tiers ONE package from the report `pkg:compile` wrote for it.
//
// The tier rules are scripts/sdkPackageTiers.mjs's, unchanged; this only narrows them to the package and
// points them at that package's own compile report, so a lane's tier is never read from -- or written
// over -- another lane's evidence.
//
//   --generated=DIR    the inventory to read (default: generated)

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
let parsed;
try {
  parsed = parsePackageArguments(process.argv.slice(2), { valued: ['--generated'] });
} catch (error) {
  process.stderr.write(`pkg:tier: ${error.message}\n`);
  process.exit(1);
}
const packageName = parsed.package;
const output = packageOutputDirectory(root, packageName);
const compilation = path.join(output, 'header-compilation.json');
if (!existsSync(compilation)) {
  process.stdout.write(
    `No compile report for ${packageName} at ${portable(path.relative(root, compilation))}; ` +
      `run \`npm run pkg:compile -- ${packageName}\` first. Tiering without one cannot call it ready.\n`,
  );
}
const result = spawnSync(
  process.execPath,
  [
    path.join(root, 'scripts', 'sdkPackageTiers.mjs'),
    `--package=${packageName}`,
    `--compilation=${compilation}`,
    `--report=${path.join(output, 'tiers.json')}`,
    ...(parsed.values.has('--generated') ? [`--generated=${parsed.values.get('--generated')}`] : []),
  ],
  { cwd: root, stdio: 'inherit' },
);
process.exit(result.status ?? 1);
