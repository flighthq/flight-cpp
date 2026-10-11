import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadApplicableEnvironments, loadDeferredPackages } from './deferredPackages.mjs';
import { driftedOverrides, loadOverrides } from './overrides.mjs';

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
//   assisted    the same, but a declared source patch, emission repair or supplying override TOUCHED it
//
// `assisted` says TOUCHED, not NEEDED, and the difference is not cosmetic. A repair scoped to `flight/`
// rewrites every file where its construct appears, including files that would have compiled without it:
// @flighthq/permissions was `ready` until conditional-absent-branch-optional rewrote a ternary in it --
// plausibly one inside a template nobody instantiates -- and it has been `assisted` since, which
// understates it.
//
// Distinguishing the two needs the package compiled with that repair's effect REMOVED, and repairs are
// applied before the tree is written, so removing one means regenerating: about a hundred minutes per
// repair per package. The cheap approximations are all unreliable -- "the inserted text is referenced" is
// true by construction, because a using-declaration repair only fires where the name is used unqualified.
//
// So the honest handling is to say what is known and not invent the rest. A supplying override IS
// load-bearing by construction (remove it and the module has no body), and that case is reported as such.
// For a repair, the report says it was involved and does not claim the package depends on it.
//   uncompiled  every module emitted, but no header of it was actually compiled
//   partial     some modules emitted, some headers were not attempted or fail to compile, or a
//               declared override is explicitly still partway, behaviorally incomplete, or drifted
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
// `uncompiled` exists because "we did not check" is not "it works". A package with only some of its
// headers attempted is `partial`, never ready: an interrupted sweep must not promote it merely because
// one cheap header happened to compile. A package with none attempted is also not "it is broken",
// which is why that distinct state does not collapse into `partial`.
//
// `ready` plus `assisted` is the shippable surface, and it is meant to be watched as it climbs.
//
//   --generated=DIR   the generated tree to read (default: generated)
//   --report=FILE     where the tier report is written
//   --compilation=FILE  the header compile report (default: sdkHeaderCompile.mjs's matching output)

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
const generatedName = path.basename(generatedRoot);
const defaultCompilation = path.join(
  'out',
  generatedName === 'generated' ? 'sdk-header-compilation.json' : `${generatedName}-header-compilation.json`,
);
const compilationFile = path.resolve(
  root,
  valueOf('--compilation=') ?? defaultCompilation,
);
const compilation = existsSync(compilationFile) ? JSON.parse(readFileSync(compilationFile, 'utf8')) : undefined;
// A refused module can be answered by a hand-written override, and before this the gate could not see
// that: `complete` demanded refusedModules === 0, so a package whose only gap was supplied by an override
// stayed `partial` for ever and the override mechanism could never show progress -- which defeats the
// point of having it as a declared mechanism. An override earns this only by declaring `supplies`, which
// names the refused module it faithfully implements; see scripts/overrides.mjs for why that is a stronger
// claim than `status: complete`.
const bestEffortFile = path.join(generatedRoot, 'best-effort.json');
const bestEffort = existsSync(bestEffortFile) ? JSON.parse(readFileSync(bestEffortFile, 'utf8')) : undefined;
const generatedHeaders = (bestEffort?.modules ?? []).map((record) => record.path);
const overrideInventory = loadOverrides(root);
// An override is a copy of generated output with hand-written corrections. Once its source hash
// moves, successful compilation is no longer evidence that the copy still represents upstream.
// Keep every package touched by such a copy out of the shippable tiers until it is re-derived.
const overrideById = new Map(overrideInventory.map((override) => [override.id, override]));
const driftedOverridesByPackage = new Map();
for (const drift of driftedOverrides(overrideInventory, generatedRoot)) {
  const override = overrideById.get(drift.id);
  if (override === undefined) continue;
  const list = driftedOverridesByPackage.get(override.package) ?? [];
  list.push(drift.id);
  driftedOverridesByPackage.set(override.package, list);
}
const suppliedModules = new Set(
  overrideInventory
    .filter((override) => override.supplies !== undefined)
    .map((override) => override.supplies),
);
const suppliedBy = new Map(
  overrideInventory
    .filter((override) => override.supplies !== undefined)
    .map((override) => [override.supplies, override.id]),
);
const incompleteOverridesByPackage = new Map();
for (const override of overrideInventory.filter((entry) => entry.status === 'incomplete')) {
  const list = incompleteOverridesByPackage.get(override.package) ?? [];
  list.push(override.id);
  incompleteOverridesByPackage.set(override.package, list);
}
// `status` records whether an override compiles, not whether all upstream behavior is present. The
// latter is stated by `remaining`: a compiling profile-boundary throw or conservative no-hit stub is
// still a package-level functional gap and must not be promoted to shippable.
const behavioralCaveatsByPackage = new Map();
for (const override of overrideInventory.filter(
  (entry) => entry.remaining !== undefined && !/^None\b/iu.test(entry.remaining.trim()),
)) {
  const list = behavioralCaveatsByPackage.get(override.package) ?? [];
  list.push(override.id);
  behavioralCaveatsByPackage.set(override.package, list);
}
// Refused modules per package, from the compiler's own per-module verdict.
const refusedByPackage = new Map();
for (const record of bestEffort?.modules ?? []) {
  if (record.status !== 'refused-placeholder') continue;
  const list = refusedByPackage.get(record.module.packageName) ?? [];
  list.push(record.module.source);
  refusedByPackage.set(record.module.packageName, list);
}
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

// A package is only `ready` if every generated header was actually tried. An unattempted header is
// not a pass; a partial sweep cannot promote a package based on whichever header happened to run.
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
  const packageHeaders = generatedHeaders.filter((header) => header.startsWith(prefix));
  // Count only headers in THIS inventory. A report from an older tree may name a removed header
  // under the same package prefix; that stale path is evidence for neither success nor failure now.
  const headers = packageHeaders.filter((header) => attempted.has(header));
  const unattemptedHeaders = packageHeaders.filter((header) => !attempted.has(header));
  const failures = packageHeaders.filter((header) => failedPrefixes.has(header));
  const incompleteOverrides = incompleteOverridesByPackage.get(package_.package) ?? [];
  const behavioralCaveats = behavioralCaveatsByPackage.get(package_.package) ?? [];
  const driftedOverrides = driftedOverridesByPackage.get(package_.package) ?? [];
  const repairs = [...new Set([...repairedByPrefix].filter(([file]) => file.startsWith(prefix)).map(([, id]) => id))];
  const patches = patchedPackages.get(package_.package) ?? [];
  const deferral = deferred.find((entry) => entry.package === package_.package);
  const isDeferred = deferral !== undefined;
  const foreignEnvironment =
    package_.environment !== undefined && !applicableEnvironments.has(package_.environment)
      ? package_.environment
      : undefined;
  // Either nothing was refused, or every refused module is supplied by an override that says so.
  const refusedHere = refusedByPackage.get(package_.package) ?? [];
  const unsuppliedModules = refusedHere.filter((source) => !suppliedModules.has(source));
  const allRefusalsSupplied =
    package_.refusedModules > 0 &&
    refusedHere.length === package_.refusedModules &&
    refusedHere.every((source) => suppliedModules.has(source));
  const overrides = refusedHere.map((source) => suppliedBy.get(source)).filter((id) => id !== undefined);
  const complete =
    package_.emittedModules + (allRefusalsSupplied ? package_.refusedModules : 0) === package_.sourceModules &&
    (package_.refusedModules === 0 || allRefusalsSupplied);
  const tier = foreignEnvironment !== undefined
    ? 'n/a'
    : isDeferred
      ? deferral.kind === 'not-applicable'
        ? 'n/a'
        : 'deferred'
    : package_.emittedModules === 0
      ? 'blocked'
      : !complete || failures.length > 0 || incompleteOverrides.length > 0 ||
          behavioralCaveats.length > 0 || driftedOverrides.length > 0
        ? 'partial'
        : headers.length === 0
          ? 'uncompiled'
          : unattemptedHeaders.length > 0
            ? 'partial'
            : repairs.length > 0 || patches.length > 0 || overrides.length > 0
              ? 'assisted'
              : 'ready';
  return {
    emittedModules: package_.emittedModules,
    ...(package_.environment === undefined ? {} : { environment: package_.environment }),
    behavioralCaveats,
    driftedOverrides,
    failingHeaders: failures,
    generatedHeaders: packageHeaders.length,
    headers: headers.length,
    incompleteOverrides,
    package: package_.package,
    repairs,
    refusedModules: package_.refusedModules,
    overrides,
    patches,
    sourceModules: package_.sourceModules,
    tier,
    unattemptedHeaders,
    unsuppliedModules,
  };
});

const order = ['ready', 'assisted', 'uncompiled', 'partial', 'blocked', 'deferred', 'n/a'];
const counts = Object.fromEntries(order.map((tier) => [tier, tiers.filter((row) => row.tier === tier).length]));
const shippable = counts.ready + counts.assisted;
const applicable = tiers.length - counts['n/a'];
const nativeUnsuppliedModules = tiers
  .filter((row) => row.tier !== 'n/a')
  .reduce((count, row) => count + row.unsuppliedModules.length, 0);
const nativePackagesWithUnsuppliedModules = tiers.filter(
  (row) => row.tier !== 'n/a' && row.unsuppliedModules.length > 0,
).length;
const nativeDriftedOverrides = tiers
  .filter((row) => row.tier !== 'n/a')
  .reduce((count, row) => count + row.driftedOverrides.length, 0);
const nativePackagesWithDriftedOverrides = tiers.filter(
  (row) => row.tier !== 'n/a' && row.driftedOverrides.length > 0,
).length;

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
      nativeDriftedOverrides,
      nativePackagesWithDriftedOverrides,
      nativePackagesWithUnsuppliedModules,
      nativeUnsuppliedModules,
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
if (nativeUnsuppliedModules > 0) {
  process.stdout.write(
    `${String(nativeUnsuppliedModules)} refused native module(s) remain without a faithful supplying override ` +
      `across ${String(nativePackagesWithUnsuppliedModules)} package(s).\n`,
  );
}
if (nativeDriftedOverrides > 0) {
  process.stdout.write(
    `${String(nativeDriftedOverrides)} drifted override(s) shadow native packages across ` +
      `${String(nativePackagesWithDriftedOverrides)} package(s); those packages remain partial.\n`,
  );
}
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
const incompleteOverrideRows = tiers.filter(
  (row) => row.tier !== 'n/a' && row.incompleteOverrides.length > 0,
);
if (incompleteOverrideRows.length > 0) {
  process.stdout.write('\nnative packages with explicitly incomplete overrides:\n');
  for (const row of incompleteOverrideRows) {
    process.stdout.write(`- ${row.package}: ${row.incompleteOverrides.join(', ')}\n`);
  }
}
const driftedOverrideRows = tiers.filter(
  (row) => row.tier !== 'n/a' && row.driftedOverrides.length > 0,
);
if (driftedOverrideRows.length > 0) {
  process.stdout.write('\nnative packages shadowed by drifted overrides:\n');
  for (const row of driftedOverrideRows) {
    process.stdout.write(`- ${row.package}: ${row.driftedOverrides.join(', ')}\n`);
  }
}
const behavioralCaveatRows = tiers.filter(
  (row) => row.tier !== 'n/a' && row.behavioralCaveats.length > 0,
);
if (behavioralCaveatRows.length > 0) {
  process.stdout.write('\nnative packages with declared behavioral gaps:\n');
  for (const row of behavioralCaveatRows) {
    process.stdout.write(`- ${row.package}: ${row.behavioralCaveats.join(', ')}\n`);
  }
}
for (const tier of ['ready', 'assisted']) {
  const named = tiers.filter((row) => row.tier === tier);
  if (named.length === 0) continue;
  process.stdout.write(`\n${tier}:\n`);
  for (const row of named) {
    // An override that declares `supplies` is load-bearing by construction: take it away and the refused
    // module has no body at all. A repair or patch may merely have touched the package, so it is reported
    // with a weaker word -- see the tier comments above for why the stronger claim is not affordable.
    const supplied = row.overrides ?? [];
    const touched = [...row.patches, ...row.repairs];
    const how = [
      ...(supplied.length > 0 ? [`supplied by ${supplied.join(', ')}`] : []),
      ...(touched.length > 0 ? [`repairs touched it: ${touched.join(', ')}`] : []),
    ];
    process.stdout.write(
      `- ${row.package} (${String(row.emittedModules)} modules, ${String(row.headers)} headers)` +
        `${how.length > 0 ? ` -- ${how.join('; ')}` : ''}\n`,
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
