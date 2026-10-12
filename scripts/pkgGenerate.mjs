import { spawnSync } from 'node:child_process';
import { cpSync, existsSync, mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  applyPackageMerge,
  danglingIncludes,
  parsePackageArguments,
  planPackageMerge,
  portable,
  rowProvenance,
  syncTree,
  treeDifferences,
} from './packageScope.mjs';

// `npm run pkg:generate -- <package> [<package>...]` regenerates named packages into the committed inventory.
//
// The packages' dependency closure is generated into a temporary tree -- a package compiled without its
// dependencies measures their absence, not the package -- and only the named packages' slices of that tree
// are spliced into generated/; see scripts/packageScope.mjs for exactly what a slice is. Another lane's
// package is never rewritten, even when the closure regenerated it differently.
//
// Naming several packages is a COORDINATED move, for packages that cannot move alone: one drops a module
// the others import, so each is only consistent once all of them are at the new revision. They are merged
// into a staging copy of the inventory as one unit, the guards judge the result, and generated/ changes
// only if all of them succeed.
//
//   --check            write nothing; fail if the named packages' slices of generated/ are not current
//   --progress         pass through to scripts/sdkGeneration.mjs
//   --generated=DIR    the inventory to splice into (default: generated)

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
let parsed;
try {
  parsed = parsePackageArguments(process.argv.slice(2), {
    allowGroup: true,
    flags: ['--check', '--progress'],
    valued: ['--generated'],
  });
} catch (error) {
  process.stderr.write(`pkg:generate: ${error.message}\n`);
  process.exit(1);
}
const packageNames = parsed.packages;
const named = packageNames.join(', ');
const check = parsed.flags.has('--check');
const generatedRoot = path.resolve(root, parsed.values.get('--generated') ?? 'generated');
const relative = portable(path.relative(root, generatedRoot));
if (!existsSync(path.join(generatedRoot, 'manifest.json'))) {
  process.stderr.write(`pkg:generate: ${relative} has no manifest.json to splice into.\n`);
  process.exit(1);
}
// Same rule as `sdk:check`: a check on a fresh clone without the pinned siblings reports and skips.
if (check && !existsSync(path.join(root, '.dependencies', 'flight', '.git'))) {
  process.stdout.write(`pkg:generate --check skipped for ${named}: the pinned checkouts are absent. Run \`npm run rehydrate\`.\n`);
  process.exit(0);
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-pkg-generate-'));
const untouched = () => `${relative} is untouched.`;
try {
  const candidateRoot = path.join(temporary, 'generated');
  process.stdout.write(`Generating ${named} with ${packageNames.length > 1 ? 'their' : 'its'} dependency closure into a temporary tree...\n`);
  const generation = spawnSync(
    process.execPath,
    [
      path.join(root, 'scripts', 'sdkGeneration.mjs'),
      ...packageNames.map((name) => `--package=${name}`),
      `--output=${candidateRoot}`,
      ...(parsed.flags.has('--progress') ? ['--progress'] : []),
    ],
    { cwd: root, stdio: 'inherit' },
  );
  if (generation.status !== 0) {
    process.stderr.write(`pkg:generate: generation of the closure failed; ${untouched()}\n`);
    process.exit(generation.status ?? 1);
  }

  // Every package is merged into a staging copy, so a group lands all at once or not at all.
  const stagingRoot = path.join(temporary, 'staging');
  cpSync(generatedRoot, stagingRoot, { recursive: true });
  const plans = [];
  for (const packageName of packageNames) {
    try {
      const plan = planPackageMerge({
        candidateRoot,
        committedRoot: stagingRoot,
        movingTogether: packageNames.filter((name) => name !== packageName),
        packageName,
      });
      applyPackageMerge(plan, stagingRoot);
      plans.push({ packageName, plan });
    } catch (error) {
      process.stderr.write(`pkg:generate: ${error.message}\n${untouched()}\n`);
      process.exit(1);
    }
  }

  // A package that moved may now include a header its dependencies only have at the new revision.
  const dangling = danglingIncludes(stagingRoot, packageNames, [
    path.join(root, 'include'),
    path.join(root, 'overrides', 'include'),
  ]);
  if (dangling.size > 0) {
    process.stderr.write('pkg:generate: the moved package(s) include headers no include root provides:\n');
    for (const [packageName, headers] of dangling) process.stderr.write(`- ${packageName}: ${headers.join(', ')}\n`);
    process.stderr.write(
      'Those headers come from dependencies still at an older revision. Move those dependencies first (or in the ' +
        `same group), then retry. ${untouched()}\n`,
    );
    process.exit(1);
  }

  const differences = treeDifferences(stagingRoot, generatedRoot);
  if (check) {
    if (differences.length === 0) {
      process.stdout.write(`${named} ${packageNames.length > 1 ? 'are' : 'is'} current in ${relative}.\n`);
    } else {
      process.stderr.write(`${named} differ${packageNames.length > 1 ? '' : 's'} from ${relative} in ${String(differences.length)} path(s):\n`);
      reportStaleRows(plans);
      for (const file of differences.slice(0, 20)) process.stderr.write(`- ${file}\n`);
      if (differences.length > 20) process.stderr.write(`- … and ${String(differences.length - 20)} more\n`);
      process.stderr.write(`Run \`npm run pkg:generate -- ${packageNames.join(' ')}\` and commit the result.\n`);
      process.exitCode = 1;
    }
  } else {
    syncTree(stagingRoot, generatedRoot);
    for (const { packageName, plan } of plans) {
      const row = plan.row;
      process.stdout.write(
        `${packageName}: ${String(row.emittedModules)}/${String(row.sourceModules)} modules emitted, ` +
          `${String(row.refusedModules)} refused at ${plan.provenance.revision.slice(0, 7)}; ` +
          `${String(plan.changedHeaders.length)} header(s) changed.\n`,
      );
    }
    process.stdout.write(`${String(differences.length)} path(s) written in ${relative}.\n`);
  }
  reportTransition(JSON.parse(readFileSync(path.join(stagingRoot, 'manifest.json'), 'utf8')), plans[0].plan);
  for (const { plan } of plans) for (const warning of plan.warnings) process.stdout.write(`warning: ${warning}\n`);
  const problems = plans.flatMap(({ plan }) => plan.problems);
  if (problems.length > 0) {
    process.stderr.write(`${String(problems.length)} shared artifact(s) a scoped run cannot update on its own:\n`);
    for (const problem of problems) process.stderr.write(`- ${problem}\n`);
    process.exitCode = 1;
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

function reportStaleRows(plans) {
  const committed = JSON.parse(readFileSync(path.join(generatedRoot, 'manifest.json'), 'utf8'));
  for (const { packageName, plan } of plans) {
    const row = committed.packages.find((candidate) => candidate.package === packageName);
    const recorded = row === undefined ? undefined : rowProvenance(committed, row).revision;
    if (recorded !== undefined && recorded !== plan.provenance.revision) {
      process.stderr.write(
        `  (${packageName} is recorded at ${recorded.slice(0, 7)}; the pin is ${plan.provenance.revision.slice(0, 7)}, so it has not moved yet)\n`,
      );
    }
  }
}

// Says where the inventory stands in a pin transition, as the staging tree has it after this run.
function reportTransition(manifest, plan) {
  if (manifest.bindingProfileSets !== undefined) {
    const counts = manifest.bindingProfileSets.map((set) => `${String(manifest.packages.filter((row) => row.bindingProfileSet === set.id).length)} under ${set.id.slice(7, 19)}`);
    process.stdout.write(`Binding profiles are mixed during a profile transition: ${counts.join(', ')}.\n`);
  }
  const byRevision = new Map();
  for (const row of manifest.packages) {
    const { revision } = rowProvenance(manifest, row);
    byRevision.set(revision, (byRevision.get(revision) ?? 0) + 1);
  }
  if (byRevision.size === 1) {
    process.stdout.write(`Inventory is uniform at ${[...byRevision.keys()][0].slice(0, 7)}.\n`);
    return;
  }
  const behind = manifest.packages.length - (byRevision.get(plan.provenance.revision) ?? 0);
  process.stdout.write(
    `Inventory is mixed during a pin transition: ${[...byRevision].map(([revision, count]) => `${String(count)} at ${revision.slice(0, 7)}`).join(', ')}; ` +
      `${String(behind)} package(s) are not at ${plan.provenance.revision.slice(0, 7)}. It returns to a single revision when every row matches.\n`,
  );
}
