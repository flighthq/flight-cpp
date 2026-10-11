import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  applyPackageMerge,
  parsePackageArguments,
  planPackageMerge,
  portable,
  rowProvenance,
} from './packageScope.mjs';

// `npm run pkg:generate -- <package>` regenerates ONE package into the committed inventory.
//
// The package's dependency closure is generated into a temporary tree -- a package compiled without its
// dependencies measures their absence, not the package -- and only the requested package's slice of that
// tree is spliced into generated/; see scripts/packageScope.mjs for exactly what that slice is. Another
// lane's package is never rewritten, even when the closure regenerated it differently.
//
//   --check            write nothing; fail if the package's slice of generated/ is not current
//   --progress         pass through to scripts/sdkGeneration.mjs
//   --generated=DIR    the inventory to splice into (default: generated)

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
let parsed;
try {
  parsed = parsePackageArguments(process.argv.slice(2), {
    flags: ['--check', '--progress'],
    valued: ['--generated'],
  });
} catch (error) {
  process.stderr.write(`pkg:generate: ${error.message}\n`);
  process.exit(1);
}
const packageName = parsed.package;
const check = parsed.flags.has('--check');
const generatedRoot = path.resolve(root, parsed.values.get('--generated') ?? 'generated');
if (!existsSync(path.join(generatedRoot, 'manifest.json'))) {
  process.stderr.write(`pkg:generate: ${portable(path.relative(root, generatedRoot))} has no manifest.json to splice into.\n`);
  process.exit(1);
}
// Same rule as `sdk:check`: a check on a fresh clone without the pinned siblings reports and skips.
if (check && !existsSync(path.join(root, '.dependencies', 'flight', '.git'))) {
  process.stdout.write(
    `pkg:generate --check skipped for ${packageName}: the pinned checkouts are absent. Run \`npm run rehydrate\`.\n`,
  );
  process.exit(0);
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-pkg-generate-'));
try {
  const candidateRoot = path.join(temporary, 'generated');
  process.stdout.write(`Generating ${packageName} with its dependency closure into a temporary tree...\n`);
  const generation = spawnSync(
    process.execPath,
    [
      path.join(root, 'scripts', 'sdkGeneration.mjs'),
      `--package=${packageName}`,
      `--output=${candidateRoot}`,
      ...(parsed.flags.has('--progress') ? ['--progress'] : []),
    ],
    { cwd: root, stdio: 'inherit' },
  );
  if (generation.status !== 0) {
    process.stderr.write(`pkg:generate: generation of ${packageName}'s closure failed; generated/ is untouched.\n`);
    process.exit(generation.status ?? 1);
  }

  const plan = planPackageMerge({ candidateRoot, committedRoot: generatedRoot, packageName });
  const relative = portable(path.relative(root, generatedRoot));
  if (check) {
    if (plan.writes.size === 0) {
      process.stdout.write(`${packageName} is current in ${relative}.\n`);
    } else {
      process.stderr.write(`${packageName} differs from ${relative} in ${String(plan.writes.size)} path(s):\n`);
      const committed = JSON.parse(readFileSync(path.join(generatedRoot, 'manifest.json'), 'utf8'));
      const committedRow = committed.packages.find((row) => row.package === packageName);
      const recorded = committedRow === undefined ? undefined : rowProvenance(committed, committedRow).revision;
      if (recorded !== undefined && recorded !== plan.provenance.revision) {
        process.stderr.write(
          `  (it is recorded at ${recorded.slice(0, 7)}; the pin is ${plan.provenance.revision.slice(0, 7)}, so it has not moved yet)\n`,
        );
      }
      for (const file of [...plan.writes.keys()].slice(0, 20)) process.stderr.write(`- ${file}\n`);
      if (plan.writes.size > 20) process.stderr.write(`- … and ${String(plan.writes.size - 20)} more\n`);
      process.stderr.write(`Run \`npm run pkg:generate -- ${packageName}\` and commit the result.\n`);
      process.exitCode = 1;
    }
  } else {
    applyPackageMerge(plan, generatedRoot);
    const row = plan.row;
    process.stdout.write(
      `${packageName}: ${String(row.emittedModules)}/${String(row.sourceModules)} modules emitted, ` +
        `${String(row.refusedModules)} refused at ${plan.provenance.revision.slice(0, 7)}; ` +
        `${String(plan.changedHeaders.length)} header(s) changed, ${String(plan.writes.size)} path(s) written in ${relative}.\n`,
    );
  }
  reportTransition(JSON.parse(readFileSync(path.join(generatedRoot, 'manifest.json'), 'utf8')), plan, check);
  for (const warning of plan.warnings) process.stdout.write(`warning: ${warning}\n`);
  if (plan.problems.length > 0) {
    process.stderr.write(`${String(plan.problems.length)} shared artifact(s) this package cannot update on its own:\n`);
    for (const problem of plan.problems) process.stderr.write(`- ${problem}\n`);
    process.exitCode = 1;
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

// Says where the inventory stands in a pin transition: which revision this package is at, and how many
// rows still sit at another one. In --check mode the inventory was not written, so it reports what is.
function reportTransition(manifest, plan, checking) {
  const byRevision = new Map();
  for (const row of manifest.packages) {
    const { revision } = rowProvenance(manifest, row);
    byRevision.set(revision, (byRevision.get(revision) ?? 0) + 1);
  }
  if (byRevision.size === 1) {
    if (!checking) process.stdout.write(`Inventory is uniform at ${[...byRevision.keys()][0].slice(0, 7)}.\n`);
    return;
  }
  const behind = manifest.packages.length - (byRevision.get(plan.provenance.revision) ?? 0);
  process.stdout.write(
    `Inventory is mixed during a pin transition: ${[...byRevision].map(([revision, count]) => `${String(count)} at ${revision.slice(0, 7)}`).join(', ')}; ` +
      `${String(behind)} package(s) are not at ${plan.provenance.revision.slice(0, 7)}. It returns to a single revision when every row matches.\n`,
  );
}
