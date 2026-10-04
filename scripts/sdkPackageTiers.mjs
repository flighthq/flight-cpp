import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadApplicableEnvironments, loadDeferredPackages } from './deferredPackages.mjs';

// What of the SDK is actually usable, counted per package.
//
// The repository has long reported one number -- modules emitted out of modules in the source -- and
// that number cannot answer the only question a consumer asks, which is whether the package they want
// works. A package is usable when every one of its modules emitted AND every header it produced
// compiles on its own. Emission alone is not it: @flighthq/statusbar emits all three of its modules
// and not one of its headers compiles.
//
// So each package lands in exactly one tier:
//
//   ready       every module emitted, every header compiles, nothing was patched or repaired
//   assisted    the same, but a declared source patch or emission repair was involved
//   uncompiled  every module emitted, but no header of it was actually compiled
//   partial     some modules emitted, some refused, or a header fails to compile
//   blocked     nothing emitted
//   deferred    declared a DEFECT in deferred-packages.json; reported, never fails the build, and still
//               counted in the denominator because it is work this profile owes
//   n/a         the package DECLARES a host environment this profile does not run -- `flight.environment`
//               in its own package.json -- so it is excluded from the denominator entirely
//
// Applicability is read from the pinned source, never inferred. Guessing it from a package name would
// have parked @flighthq/webcam, which is shippable; guessing it from web symbols in a refusal would have
// parked @flighthq/render-wgpu, which this profile needs.
//
// The denominator matters. Counting a Canvas 2D renderer as outstanding work on a host with no canvas
// makes the shippable fraction permanently unreachable and tells the reader nothing.
//
// `uncompiled` exists because "we did not check" is not "it works", and an interrupted or partial
// header sweep would otherwise promote a package that has never been compiled at all. It is also not
// "it is broken", which is why it does not collapse into `partial`.
//
// `ready` plus `assisted` is the shippable surface, and it is meant to be watched as it climbs.
//
//   --generated=DIR   the generated tree to read (default: generated)
//   --report=FILE     where the tier report is written
//   --compilation=FILE  the header compile report (default: alongside the generated tree)

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const options = process.argv.slice(2);
const valued = ['--generated=', '--report=', '--compilation='];
const unknown = options.filter((option) => !valued.some((prefix) => option.startsWith(prefix)));
if (unknown.length > 0) {
  process.stderr.write(`Unknown SDK package tier option(s): ${unknown.join(', ')}\n`);
  process.exit(1);
}
const valueOf = (name) => {
  const hit = options.find((option) => option.startsWith(name));
  return hit?.slice(name.length);
};

// Default: `generated`, the COMMITTED inventory. It used to be `out/sdk-sdl`, which was the SDL-profiled
// side output back when `generated/` held the unbound tree. `sdk:generate` now applies the SDL profile set
// by default, so those two trees have identical content and `out/` is gitignored -- a default pointing
// there fails on a fresh clone for no reason.
const generatedRoot = path.resolve(root, valueOf('--generated=') ?? 'generated');
const manifestFile = path.join(generatedRoot, 'manifest.json');
if (!existsSync(manifestFile)) {
  process.stdout.write(
    `SDK package tiers skipped: ${portable(path.relative(root, manifestFile))} is absent. ` +
      'Run `npm run sdk:generate:sdl` first.\n',
  );
  process.exit(0);
}
const manifest = JSON.parse(readFileSync(manifestFile, 'utf8'));
const compilationFile = path.resolve(
  root,
  valueOf('--compilation=') ?? `${portable(path.relative(root, generatedRoot))}-header-compilation.json`,
);
const compilation = existsSync(compilationFile) ? JSON.parse(readFileSync(compilationFile, 'utf8')) : undefined;
const reportFile = path.resolve(root, valueOf('--report=') ?? path.join('out', 'sdk-package-tiers.json'));
// `--report=` is where this script WRITES, and `--compilation=` is what it reads. Both scripts in this
// pair spell their output `--report=`, so passing the header compile report here reads as "use this as
// input" and is in fact "overwrite this". That destroyed a two-hour full-tree compile report once; the
// tier report landed at out/sdk-sdl-header-compilation.json, every package then showed zero headers, and
// the only clue was the tier output insisting no compile report existed.
//
// Refusing to write over a header compile report costs nothing and makes the mistake unrepeatable. It is
// identified by its own schema rather than by filename, so renaming the file does not defeat it.
if (existsSync(reportFile)) {
  let existingSchema;
  try {
    existingSchema = JSON.parse(readFileSync(reportFile, 'utf8')).schema;
  } catch {
    existingSchema = undefined;
  }
  if (typeof existingSchema === 'string' && existingSchema.startsWith('flight-sdk-header-compilation/')) {
    process.stderr.write(
      `--report=${portable(path.relative(root, reportFile))} is a header compile report, not somewhere to ` +
        'write the tier report. Did you mean --compilation= to READ it?\n',
    );
    process.exit(1);
  }
}
const deferred = loadDeferredPackages(root);
const applicableEnvironments = loadApplicableEnvironments(root);

// A package is only `ready` if its headers were actually tried. An unattempted header is not a pass,
// so a package nobody compiled is `assisted` at best and is reported as uncompiled either way.
const failedPrefixes = new Set((compilation?.failures ?? []).map((failure) => failure.header));
const attempted = new Set([...(compilation?.passed ?? []), ...failedPrefixes]);
const repairedByPrefix = new Map();
for (const record of manifest.emissionRepairs ?? []) {
  for (const file of record.files) repairedByPrefix.set(file, record.id);
}
const patchedPackages = new Map();
for (const patch of manifest.sourcePatches ?? []) {
  if (patch.package === undefined) continue;
  patchedPackages.set(patch.package, [...(patchedPackages.get(patch.package) ?? []), patch.id]);
}

const tiers = manifest.packages.map((package_) => {
  const prefix = `${package_.cppIncludePrefix}/`;
  const headers = [...attempted].filter((header) => header.startsWith(prefix));
  const failures = headers.filter((header) => failedPrefixes.has(header));
  const repairs = [...new Set([...repairedByPrefix].filter(([file]) => file.startsWith(prefix)).map(([, id]) => id))];
  const patches = patchedPackages.get(package_.package) ?? [];
  const deferral = deferred.find((entry) => entry.package === package_.package);
  const isDeferred = deferral !== undefined;
  const foreignEnvironment =
    package_.environment !== undefined && !applicableEnvironments.has(package_.environment)
      ? package_.environment
      : undefined;
  const complete = package_.emittedModules === package_.sourceModules && package_.refusedModules === 0;
  const tier = foreignEnvironment !== undefined
    ? 'n/a'
    : isDeferred
      ? deferral.kind === 'not-applicable'
        ? 'n/a'
        : 'deferred'
    : package_.emittedModules === 0
      ? 'blocked'
      : !complete || failures.length > 0
        ? 'partial'
        : headers.length === 0
          ? 'uncompiled'
          : repairs.length > 0 || patches.length > 0
            ? 'assisted'
            : 'ready';
  return {
    emittedModules: package_.emittedModules,
    ...(package_.environment === undefined ? {} : { environment: package_.environment }),
    failingHeaders: failures,
    headers: headers.length,
    package: package_.package,
    repairs,
    patches,
    sourceModules: package_.sourceModules,
    tier,
  };
});

const order = ['ready', 'assisted', 'uncompiled', 'partial', 'blocked', 'deferred', 'n/a'];
const counts = Object.fromEntries(order.map((tier) => [tier, tiers.filter((row) => row.tier === tier).length]));
const shippable = counts.ready + counts.assisted;
const applicable = tiers.length - counts['n/a'];

mkdirSync(path.dirname(reportFile), { recursive: true });
writeFileSync(
  reportFile,
  `${JSON.stringify(
    {
      schema: 'flight-cpp-sdk-package-tiers/1',
      compilation: compilation === undefined ? 'absent' : path.relative(root, compilationFile),
      applicable,
      counts,
      generated: path.relative(root, generatedRoot),
      packages: tiers.sort((left, right) => order.indexOf(left.tier) - order.indexOf(right.tier) || left.package.localeCompare(right.package)),
      shippable,
      source: manifest.source,
    },
    undefined,
    2,
  )}\n`,
);

process.stdout.write(
  `${String(shippable)} of ${String(applicable)} applicable SDK packages are shippable ` +
    `(${String(counts.ready)} ready, ${String(counts.assisted)} assisted); ` +
    `${String(counts.uncompiled)} emitted but never compiled, ${String(counts.partial)} partial, ` +
    `${String(counts.blocked)} blocked, ${String(counts.deferred)} deferred. ` +
    `${String(counts['n/a'])} of ${String(tiers.length)} package(s) are not applicable to this profile and ` +
    'are excluded from that count.\n',
);
if (compilation === undefined) {
  process.stdout.write(
    'No header compile report was found, so no package can be called ready. ' +
      'Run `npm run sdk:compile:sdl` and re-run this.\n',
  );
}
if (counts.uncompiled > 0) {
  process.stdout.write(
    `\n${String(counts.uncompiled)} package(s) emitted completely but no header of theirs was compiled, ` +
      'so they are not counted as shippable. Run `npm run sdk:compile:sdl` over the whole tree.\n',
  );
}
for (const tier of ['ready', 'assisted']) {
  const named = tiers.filter((row) => row.tier === tier);
  if (named.length === 0) continue;
  process.stdout.write(`\n${tier}:\n`);
  for (const row of named) {
    const how = [...row.patches, ...row.repairs];
    process.stdout.write(
      `- ${row.package} (${String(row.emittedModules)} modules, ${String(row.headers)} headers)` +
        `${how.length > 0 ? ` via ${how.join(', ')}` : ''}\n`,
    );
  }
}
const deferredRows = tiers.filter((row) => row.tier === 'deferred');
if (deferredRows.length > 0) {
  process.stdout.write('\ndeferred defects (still owed by this profile, do not fail the build):\n');
  for (const row of deferredRows) {
    process.stdout.write(`- ${row.package}: ${String(row.failingHeaders.length)} failing header(s)\n`);
  }
}
const notApplicableRows = tiers.filter((row) => row.tier === 'n/a');
if (notApplicableRows.length > 0) {
  process.stdout.write('\nnot applicable to this profile (excluded from the denominator):\n');
  for (const row of notApplicableRows) {
    const entry = deferred.find((candidate) => candidate.package === row.package);
    const why =
      row.environment !== undefined
        ? `declares flight.environment "${row.environment}"`
        : (entry?.owner ?? 'declared not applicable');
    process.stdout.write(`- ${row.package}: ${why}\n`);
  }
}
process.stdout.write(`\nReport: ${portable(path.relative(root, reportFile))}\n`);

function portable(filename) {
  return filename.split(path.sep).join('/');
}
