import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  cpSync,
  existsSync,
  lstatSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  rmSync,
  writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import { loadDeferredPackages, nonTerminatingPackages } from './deferredPackages.mjs';
import { resolveDependency } from './dependencyLock.mjs';
import {
  aliasDuplicateStructuralStructs,
  applyEmissionRepairs,
  loadEmissionRepairs,
  obsoleteRepairs,
  referenceAliasIdentityProof,
} from './emissionRepairs.mjs';
import { canonicalizeGeneratedIncludes } from './generatedIncludes.mjs';
import { generatedReadmeProblems, isMixedManifest, verifyMixedInventory } from './packageScope.mjs';
import { overrideHeaderFiles, reportComputedCells, structuralMemberTable } from './structuralMembers.mjs';
import {
  applySourcePatches,
  ineffectivePatches,
  loadPendingSourcePatches,
  loadSourcePatches,
  recoverStalePatches,
  revertSourcePatches,
} from './sourcePatches.mjs';

// Materializes the part of the pinned Flight SDK the pinned compiler can emit today. Refusals are
// output too: this tree is an honest compiler bring-up inventory, not a claim that Flight::Sdk can
// already be built. Each generated header has one canonical home shared by examples and tests.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const options = process.argv.slice(2);
const check = options.includes('--check');
// Opt-in because a complete SDK run emits thousands of events. The trace is deliberately written
// before each synchronous compiler phase begins, so if lowering stops making progress its final line
// names the exact upstream module still in flight.
const traceProgress = options.includes('--progress');
// Best-effort generation asks the compiler to emit everything it can, writing a replaceable stub where a
// module is refused for its OWN reasons and real output where a module was only held back by an inherited
// refusal. The compiler's report then names, per module, whether the file on disk is output or a
// placeholder, which consumers a hand-written replacement must satisfy, and the declaration fingerprints
// an overlay watches to notice the module moving underneath it between pins.
//
// It is the DEFAULT, because it is what the committed inventory is generated with, and the committed
// inventory has to be reproducible by `npm run sdk:generate` with no arguments. Making it a flag instead
// would mean three call sites -- that script, `sdk:check`, and scripts/check.mjs's own argument list --
// each having to remember it, which is exactly the kind of agreement that silently stops holding.
// `--no-best-effort` asks for the old strict behaviour, which is now only useful for comparing the two.
const bestEffort = !options.includes('--no-best-effort');
// Generate a subset of the SDK instead of the whole dependency closure. `analyzeFlightWorkspace`
// resolves each named package's own closure, so naming one package still compiles what it needs. This
// exists both for per-package delivery and for isolating compiler performance or lowering regressions.
// It was also the instrument that disproved the old render-gl nontermination diagnosis: traced render-gl
// and scene3d-gl closures now complete, so the default inventory excludes nothing on their account.
const requestedPackages = options
  .filter((option) => option.startsWith('--package='))
  .map((option) => option.slice('--package='.length))
  .filter((name) => name.length > 0);
// Diagnostic output scoping supplied directly to the compiler. All package sources are still lowered and
// available for type/module resolution; only matching source paths enter the expensive C++ emission pass.
// This deliberately does not change --package's dependency-closed output contract.
const emitPathPrefixes = options
  .filter((option) => option.startsWith('--emit-path-prefix='))
  .map((option) => option.slice('--emit-path-prefix='.length))
  .filter((prefix) => prefix.length > 0);
const bindingProfileOptions = options
  .filter((option) => option.startsWith('--binding-profile='))
  .map((option) => option.slice('--binding-profile='.length));
const outputOption = options.find((option) => option.startsWith('--output='));
const unknown = options.filter(
  (option) =>
    option !== '--check' &&
    // Still accepted, and now a no-op: it names the default. Rejecting it would break every command
    // line recorded in docs/generation-state.md for no gain.
    option !== '--best-effort' &&
    option !== '--no-best-effort' &&
    option !== '--progress' &&
    !option.startsWith('--package=') &&
    !option.startsWith('--emit-path-prefix=') &&
    !option.startsWith('--binding-profile=') &&
    !option.startsWith('--output='),
);
if (unknown.length > 0) {
  process.stderr.write(`Unknown SDK generation option(s): ${unknown.join(', ')}\n`);
  process.exit(1);
}

const flight = resolveDependency(root, 'flight');
const compiler = resolveDependency(root, 'flight-compiler');
const generatedRoot = outputOption
  ? path.resolve(root, outputOption.slice('--output='.length))
  : path.join(root, 'generated');
// Checked first and independently of generation: it needs no pinned checkout, so it holds on a fresh clone
// where the rest of the check is skipped, and it names the stale sentence instead of a whole-tree diff.
// A failure EXITS here, before patch recovery, checkout validation or generation: there is no reason to
// spend a whole-SDK run confirming a tree whose own README already contradicts it.
if (check) {
  const readmeProblems = generatedReadmeProblems(generatedRoot);
  if (readmeProblems.length > 0) {
    const shown = path.relative(root, generatedRoot);
    const where = shown.startsWith('..') || path.isAbsolute(shown) ? portable(generatedRoot) : portable(shown) || '.';
    process.stderr.write(`${where}/README.md is not derived from its manifest.json:\n`);
    for (const problem of readmeProblems) process.stderr.write(`- ${problem}\n`);
    process.stderr.write('Re-derive it with readmeWithSummary and readmeWithProvenance from scripts/packageScope.mjs; a scoped merge does this itself.\n');
    process.exit(1);
  }
}
// The profile set the committed inventory is generated with, and the reason it is a DEFAULT rather than
// something the npm scripts pass.
//
// flight-cpp targets a native SDL host, and without these profiles the compiler has no binding for
// `console`, `performance`, `setInterval` or `clearInterval`, so it refuses whole modules that depend on
// them. Measured: the unbound 150-package inventory emits 1907 of 2709 modules and refuses 802, and
// @flighthq/log is refused ENTIRELY -- a 149-line placeholder reading "no part of it was generated".
// Generated with these profiles, log.ts is 1081 lines of real output, and log's four-package closure is
// 1015 of 1018 modules with THREE refusals.
//
// An unbound inventory can therefore never be a working SDK, and it is not what any of this repository's
// repairs and overrides are measured against: both committed log overrides hash to the profiled file,
// not the unbound one. Committing the unbound tree once already produced an inventory whose own
// overrides did not match it.
//
// An explicit --binding-profile list still wins, so sdk:generate:headless and friends keep generating
// their own narrower profiles into out/. The default exists so that the three call sites that must agree
// -- sdk:generate, sdk:check and scripts/check.mjs's argument list -- cannot drift apart.
const SDL_PROFILE_SET = [
  'bindings/runtime.json',
  'bindings/headless.json',
  'bindings/web-types.json',
  'bindings/sdl-image.json',
  'bindings/sdl-gl.json',
  'bindings/sdl-wgpu.json',
  'bindings/sdl-app.json',
];
const bindingProfiles = loadBindingProfiles(
  bindingProfileOptions.length > 0 ? bindingProfileOptions : SDL_PROFILE_SET,
);
// Before the integrity check, not after: a run killed with SIGKILL -- which the generation hang has to
// be, since it ignores SIGTERM -- leaves its patches applied, and the integrity check would then refuse
// with "flight has uncommitted changes" and no hint about the cause. Recovery reverts only dirt a
// declared patch provably put there; see recoverStalePatches.
// Loaded HERE, before the long compile, rather than where it is used after it. Generation over 73
// packages takes 27 minutes, and loadEmissionRepairs validates the declaration file from disk; editing
// repairs/emission-repairs.json while a run is in flight meant a 27-minute compile finished and then
// threw on a declaration the already-loaded module did not recognise. Validating up front turns that
// into an immediate error instead of a wasted half hour.
const declaredRepairs = loadEmissionRepairs(root);
const deferredPackages = loadDeferredPackages(root);
const sourcePatches = loadSourcePatches(root);
reportPendingSourcePatches(loadPendingSourcePatches(root));
for (const dependency of [compiler, flight]) {
  const recovery = recoverStalePatches(sourcePatches, dependency);
  if (recovery.recovered.length > 0) {
    process.stdout.write(
      `Recovered ${String(recovery.recovered.length)} source patch(es) left applied to ${dependency.name} by an interrupted run: ${recovery.recovered.join(', ')}\n`,
    );
  }
  if (recovery.heldBy !== undefined) {
    process.stderr.write(
      `Another generation run (pid ${String(recovery.heldBy)}) holds the pinned ${dependency.name} checkout patched. ` +
        'Wait for it to finish rather than running two generations against one checkout.\n',
    );
    process.exit(1);
  }
}
const inputFailure = validateInput(flight, compiler);
if (inputFailure !== undefined) {
  const message = `${inputFailure} Run \`npm run rehydrate\` and \`npm ci --prefix .dependencies/flight-compiler\`.\n`;
  if (check) {
    process.stdout.write(`SDK generation check skipped: ${message}`);
    process.exit(0);
  }
  process.stderr.write(message);
  process.exit(1);
}

const compilerEntry = path.join(
  compiler.directory,
  'packages',
  'tool-compiler',
  'dist',
  'packages',
  'tool-compiler',
  'src',
  'index.js',
);

const temporaryRoot = mkdtempSync(path.join(tmpdir(), 'flight-cpp-sdk-generation-'));
const candidateRoot = path.join(temporaryRoot, 'generated');

// Patches are applied only after validateInput has confirmed both checkouts are clean and at their
// pinned revisions, and reverted in the `finally` below, so each pinned checkout is rewritten for the
// length of one run and never longer. Compiler patches must precede its build; Flight patches precede
// source analysis. Both remain ordinary, pinned, provenance-recorded source patches.
// A `finally` does not run when the process is signalled, and a long generation run gets interrupted:
// the first SIGTERM'd run left all three patches applied in the pinned checkout, and the next run
// refused with "flight has uncommitted changes". Reverting on the way out of a signal keeps the
// checkout clean whichever way the run ends.
for (const signal of ['SIGINT', 'SIGTERM', 'SIGHUP']) {
  process.on(signal, () => {
    revertSourcePatches(sourcePatches, flight);
    revertSourcePatches(sourcePatches, compiler);
    // The patched compiler build is an output outside Git, so reverting sources alone leaves later
    // oracles loading code that no longer corresponds to the clean checkout. Force the next consumer
    // to rebuild from the restored source when a signal prevents normal cleanup.
    rmSync(path.join(compiler.directory, 'packages', 'tool-compiler', 'dist'), {
      force: true,
      recursive: true,
    });
    process.exit(signal === 'SIGINT' ? 130 : 143);
  });
}
try {
  const compilerPatches = applySourcePatches(sourcePatches, compiler);
  runCompilerBuild(compiler.directory);
  if (!existsSync(compilerEntry)) {
    throw new Error(`Pinned compiler build did not produce ${compilerEntry}`);
  }
  const appliedPatches = [...compilerPatches, ...applySourcePatches(sourcePatches, flight)];
  const result = await generateSdk(candidateRoot, flight, compiler, compilerEntry, bindingProfiles, appliedPatches);
  if (check && isMixedInventory(generatedRoot)) {
    // A mixed inventory is mid pin transition: see scripts/packageScope.mjs. Rows at this pin must
    // reproduce package by package; rows at another revision are the transition's pending work.
    const verdict = verifyMixedInventory({ candidateRoot, committedRoot: generatedRoot });
    const pendingRevisions = [...new Set(verdict.pending.map((entry) => entry.revision.slice(0, 7)))].join(', ');
    if (verdict.failed.length > 0) {
      process.stderr.write(
        `${String(verdict.failed.length)} package(s) recorded at ${verdict.revision.slice(0, 7)} do not reproduce:\n`,
      );
      for (const entry of verdict.failed.slice(0, 20)) {
        process.stderr.write(`- ${entry.package}: ${entry.reason ?? entry.files.slice(0, 4).join(', ')}\n`);
      }
      process.stderr.write('Run `npm run pkg:generate -- <package>` for each and commit the result.\n');
      process.exitCode = 1;
    }
    for (const problem of verdict.problems) process.stderr.write(`- ${problem}\n`);
    if (verdict.problems.length > 0) process.exitCode = 1;
    process.stdout.write(
      `Mixed SDK inventory: ${String(verdict.reproduced.length)} package(s) at ${verdict.revision.slice(0, 7)} reproduce; ` +
        `${String(verdict.pending.length)} pending at ${pendingRevisions || 'no other revision'}` +
        `${verdict.missing.length > 0 ? `; ${String(verdict.missing.length)} new at this pin and not yet generated: ${verdict.missing.join(', ')}` : ''}.\n`,
    );
    if (reportObsoleteRepairs(result)) process.exitCode = 1;
    if (reportIneffectivePatches(result)) process.exitCode = 1;
  } else if (check) {
    const drift = compareTrees(candidateRoot, generatedRoot);
    if (drift.length > 0) {
      process.stderr.write(`Generated SDK inventory differs in ${String(drift.length)} path(s):\n`);
      for (const filename of drift.slice(0, 20)) process.stderr.write(`- ${filename}\n`);
      if (drift.length > 20) process.stderr.write(`- … and ${String(drift.length - 20)} more\n`);
      process.stderr.write('Run `npm run sdk:generate` and commit the resulting generated/ tree.\n');
      process.exitCode = 1;
    } else {
      process.stdout.write(summary('Generated SDK inventory is current', result));
    }
    if (reportObsoleteRepairs(result)) process.exitCode = 1;
    if (reportIneffectivePatches(result)) process.exitCode = 1;
  } else {
    rmSync(generatedRoot, { force: true, recursive: true });
    cpSync(candidateRoot, generatedRoot, { recursive: true });
    process.stdout.write(summary('Generated SDK inventory updated', result));
    reportObsoleteRepairs(result);
    reportIneffectivePatches(result);
  }
} finally {
  revertSourcePatches(sourcePatches, flight);
  revertSourcePatches(sourcePatches, compiler);
  // Source patches are generation-local. Restore the compiler build as well as its source so later
  // profile/oracle runs cannot inherit a patched dist tree from whichever generation ran last.
  try {
    runCompilerBuild(compiler.directory);
  } finally {
    rmSync(temporaryRoot, { force: true, recursive: true });
  }
}

async function generateSdk(outputRoot, flightDependency, compilerDependency, compilerModule, profiles, appliedPatches) {
  const {
    analyzeFlightWorkspace,
    compileTypeScriptPackageGraph,
    createCompilerModuleResolutionPlan,
    createCppCompilerBackend,
    parseTypeScriptSource,
  } = await import(pathToFileURL(compilerModule));
  const sdkPackageFile = path.join(flightDependency.directory, 'packages', 'sdk', 'package.json');
  const sdkPackage = JSON.parse(readFileSync(sdkPackageFile, 'utf8'));
  const sdkPackageNames = Object.keys(sdkPackage.dependencies ?? {})
    .filter((name) => name.startsWith('@flighthq/'))
    .sort();
  const unknownRequested = requestedPackages.filter((name) => !sdkPackageNames.includes(name));
  if (unknownRequested.length > 0) {
    throw new Error(
      `--package named ${unknownRequested.join(', ')}, which @flighthq/sdk does not depend on. ` +
        'Name a package the SDK actually includes.',
    );
  }
  // A named package is generated WITH ITS TRANSITIVE DEPENDENCIES. `analyzeFlightWorkspace` takes the
  // target list literally and resolves no closure of its own, so naming @flighthq/math alone compiles
  // math against nothing and turns four modules into placeholders purely because @flighthq/types was
  // absent. A subset that omits a dependency does not measure the subset, it measures the omission.
  // The default target set is every SDK package EXCEPT any explicitly declared non-terminating package and
  // every package whose closure reaches one. The current declaration is empty: render-gl and scene3d-gl
  // both finish. The closure rule remains so a future measured regression can be isolated by naming only
  // its trigger rather than manually maintaining every dependent package.
  //
  // A named --package set is honoured as given, including a deliberately isolated regression target.
  const nonTerminating = new Set(nonTerminatingPackages(deferredPackages));
  const reaches = (name) =>
    transitivePackageClosure([name], flightDependency.directory, sdkPackageNames).some((dependency) =>
      nonTerminating.has(dependency),
    );
  const generable = sdkPackageNames.filter((name) => !reaches(name));
  const excluded = sdkPackageNames.filter((name) => reaches(name));
  if (requestedPackages.length === 0 && excluded.length > 0) {
    process.stdout.write(
      `Excluded ${String(excluded.length)} package(s) generation cannot finish: ${excluded.join(', ')}\n`,
    );
  }
  const packageNames =
    requestedPackages.length > 0
      ? transitivePackageClosure(requestedPackages, flightDependency.directory, sdkPackageNames)
      : generable;
  const inventory = analyzeFlightWorkspace({
    targetPackageNames: packageNames,
    upstreamDirectory: flightDependency.directory,
  });
  const includedPackages = new Set(packageNames);
  const packageDescriptors = inventory.packages.map((package_) => {
    const root = path.join(flightDependency.directory, package_.directory);
    return {
      dependencies: package_.dependencies.filter((dependency) => includedPackages.has(dependency)).sort(compareText),
      // Flight declares a package's host environment itself, and that declaration -- not a guess from
      // the package name or from which symbols its refusals mention -- is what decides whether a
      // package can apply to this profile at all. Environment-agnostic packages declare nothing.
      environment: declaredEnvironment(root),
      name: package_.name,
      root,
      sources: listSourceFiles(path.join(root, 'src')),
      target: {
        includePrefix: `flight/${cppPackageName(package_.name)}`,
        namespace: `flight::${cppPackageName(package_.name)}`,
      },
    };
  });
  const sources = packageDescriptors.flatMap((package_) =>
    package_.sources.map((source) => ({
      packageName: package_.name,
      packageRoot: package_.root,
      sourceFile: parseTypeScriptSource(source.sourcePath, source.contents),
      upstreamDirectory: flightDependency.directory,
    })),
  );
  const compilation = compileTypeScriptPackageGraph({
    backend: createCppCompilerBackend(),
    ...(bestEffort ? { bestEffort: true } : {}),
    backendOptions: {
      ...(profiles.length > 0
        ? {
            externalBindings: mergeBindingProfiles(profiles),
          }
        : {}),
      packageTargets: Object.fromEntries(packageDescriptors.map((package_) => [package_.name, package_.target])),
      runtimeProfile: 'flight-cpp',
      upstreamCommit: flightDependency.commit,
    },
    graph: {
      entries: [],
      moduleDependencies: [],
      packages: packageDescriptors.map((package_) => ({
        dependencies: package_.dependencies,
        name: package_.name,
        root: package_.root,
      })),
      schema: 'flight-compiler-package-graph/1',
    },
    ...(emitPathPrefixes.length > 0 ? { emitPathPrefixes } : {}),
    moduleResolution: createCompilerModuleResolutionPlan(inventory),
    ...(traceProgress
      ? {
          observeProgress(progress) {
            const module =
              'module' in progress
                ? ` ${progress.module.packageName}/${progress.module.source} (${String(progress.completedModules)}/${String(progress.moduleCount)})`
                : ` (${String(progress.moduleCount)} modules)`;
            process.stdout.write(`[sdk-generation] ${progress.phase} ${progress.state}${module}\n`);
          },
        }
      : {}),
    sources,
  });
  // Repairs run between emission and writing, so the committed inventory is what actually compiles
  // and there is no second tree to keep in step. See scripts/emissionRepairs.mjs for the expiry rule.
  const repairs = declaredRepairs;
  const appliedRepairs = applyEmissionRepairs(repairs, compilation.compilation.files);
  // Needs the whole file set rather than one file at a time, because it has to find the canonical
  // definition before it can alias a duplicate to it.
  const aliasedStructs = aliasDuplicateStructuralStructs(compilation.compilation.files);
  // Package-local quoted includes search the generated sibling directory before the override include
  // root. Canonical installed paths preserve the same dependency while allowing an override to shadow
  // nested includes exactly as it shadows a consumer's top-level include.
  const canonicalIncludes = canonicalizeGeneratedIncludes(compilation.compilation.files);
  // The respell repairs claim that `flight::Ref<X>` and the spelling they write are the same type. This
  // turns that claim into static_asserts compiled by the same gate that compiles the headers, so a
  // wrong expansion fails the build instead of quietly changing a signature. It is emitted as a header
  // in the generated tree rather than kept beside the script, because the claim is about THIS tree.
  const identityProof = referenceAliasIdentityProof(repairs);
  if (identityProof !== undefined) {
    compilation.compilation.files.push({
      contents: identityProof,
      path: 'flight/repairs/reference_alias_identity.hpp',
    });
  }
  for (const file of compilation.compilation.files) {
    const target = path.join(outputRoot, 'include', file.path);
    mkdirSync(path.dirname(target), { recursive: true });
    writeFileSync(target, file.contents);
  }
  writeStructuralMemberTable(outputRoot, compilation.compilation.files);
  writeSdkBuildMetadata(outputRoot, compilation.compilation.files);
  const descriptorByName = new Map(packageDescriptors.map((package_) => [package_.name, package_]));
  const packageResults = compilation.report.packages.map((package_) => {
    const descriptor = descriptorByName.get(package_.name);
    if (!descriptor) throw new Error(`Compiler reported unknown package ${package_.name}`);
    return {
      cppIncludePrefix: descriptor.target.includePrefix,
      cppNamespace: descriptor.target.namespace,
      emittedModules: package_.modules.filter((module) => module.status === 'emitted').length,
      ...(descriptor.environment === undefined ? {} : { environment: descriptor.environment }),
      package: package_.name,
      refusedModules: package_.modules.filter((module) => module.status === 'refused').length,
      sourceModules: package_.modules.length,
    };
  });
  const refusals = compilation.report.modules.flatMap((module) =>
    module.refusals.map((refusal) => ({
      code: refusal.code,
      ...(refusal.column === undefined ? {} : { column: refusal.column }),
      ...(refusal.line === undefined ? {} : { line: refusal.line }),
      module: module.module.source,
      package: module.module.packageName,
      reason: refusal.message,
      stage: refusal.stage,
    })),
  );

  const totals = packageResults.reduce(
    (result, item) => ({
      emittedModules: result.emittedModules + item.emittedModules,
      refusedModules: result.refusedModules + item.refusedModules,
      sourceModules: result.sourceModules + item.sourceModules,
    }),
    { emittedModules: 0, refusedModules: 0, sourceModules: 0 },
  );
  const manifest = {
    schema: 'flight-generated-sdk/1',
    compiler: {
      repository: compilerDependency.repository,
      revision: compilerDependency.commit,
      runtimeProfile: 'flight-cpp',
      target: 'cpp',
    },
    bindingProfiles: profiles.map((profile) => ({
      digest: profile.digest,
      identity: profile.identity,
      path: profile.path,
      profile: profile.profile,
      schema: profile.schema,
    })),
    namespaceMapping: {
      desiredRoot: 'flight',
      status: 'applied',
    },
    emissionRepairs: appliedRepairs.map((record) => ({
      files: record.files,
      id: record.id,
    })),
    sourcePatches: appliedPatches,
    duplicateStructuralStructAliases: {
      files: aliasedStructs.files,
      structs: aliasedStructs.structs,
    },
    canonicalPackageLocalIncludes: canonicalIncludes,
    packages: packageResults,
    source: {
      package: String(sdkPackage.name),
      repository: flightDependency.repository,
      revision: flightDependency.commit,
      version: String(sdkPackage.version),
    },
    summary: {
      packages: packageResults.length,
      ...totals,
    },
  };
  const refusalLedger = {
    schema: 'flight-generated-sdk-refusals/2',
    compilerRevision: compilerDependency.commit,
    sourceRevision: flightDependency.commit,
    refusals,
  };

  mkdirSync(outputRoot, { recursive: true });
  writeFileSync(path.join(outputRoot, 'manifest.json'), `${JSON.stringify(manifest, undefined, 2)}\n`);
  writeFileSync(path.join(outputRoot, 'refusals.json'), `${JSON.stringify(refusalLedger, undefined, 2)}\n`);
  writeFileSync(
    path.join(outputRoot, 'initialization.json'),
    `${JSON.stringify(compilation.report.initialization, undefined, 2)}\n`,
  );
  // The best-effort manifest is the compiler's own per-module verdict: which files are real output,
  // which are replaceable stubs, which consumers a hand-written replacement must satisfy, and the
  // declaration fingerprints an overlay watches to notice its module moving between pins. It is written
  // beside the inventory rather than folded into it, because it describes a different thing: not what
  // the SDK contains, but what still has to be written by hand and against what.
  if (compilation.report.bestEffort !== undefined) {
    writeFileSync(
      path.join(outputRoot, 'best-effort.json'),
      `${JSON.stringify(compilation.report.bestEffort, undefined, 2)}\n`,
    );
  }
  writeFileSync(path.join(outputRoot, 'README.md'), generatedReadme(manifest));
  return {
    ...manifest.summary,
    ineffectivePatches: ineffectivePatches(
      appliedPatches,
      new Map(sourcePatches.map((patch) => [patch.id, patch])),
      refusals,
    ),
    obsoleteRepairs: obsoleteRepairs(appliedRepairs, profiles, requestedPackages.length === 0),
    patchedModules: appliedPatches.length,
    aliasedStructs: aliasedStructs.structs.length,
    repairedFiles:
      appliedRepairs.reduce((total, record) => total + record.files.length, 0) + aliasedStructs.files.length,
  };
}

function loadBindingProfiles(filenames) {
  const profiles = filenames.map((filename) => {
    const absolute = path.resolve(root, filename);
    const contents = readFileSync(absolute, 'utf8');
    const profile = JSON.parse(contents);
    if (
      profile?.schema !== 'flight-cpp-external-bindings/1' ||
      typeof profile.identity !== 'string' ||
      profile.identity.length === 0 ||
      typeof profile.profile !== 'string' ||
      profile.profile.length === 0 ||
      !Array.isArray(profile.bindings)
    ) {
      throw new TypeError(`${filename} is not an identified flight-cpp-external-bindings/1 profile`);
    }
    return {
      bindings: profile.bindings,
      digest: `sha256:${createHash('sha256').update(contents).digest('hex')}`,
      identity: profile.identity,
      path: portable(path.relative(root, absolute)),
      profile: profile.profile,
      schema: profile.schema,
    };
  });
  const identities = new Set();
  const symbols = new Set();
  for (const profile of profiles) {
    if (identities.has(profile.identity)) throw new TypeError(`duplicate binding profile ${profile.identity}`);
    identities.add(profile.identity);
    for (const binding of profile.bindings) {
      const symbol = `${String(binding.sourceName)}:${String(binding.space)}`;
      if (symbols.has(symbol)) throw new TypeError(`binding profiles provide ${symbol} more than once`);
      symbols.add(symbol);
    }
  }
  return profiles;
}

function mergeBindingProfiles(profiles) {
  return {
    bindings: profiles.flatMap((profile) => profile.bindings),
    schema: 'flight-cpp-external-bindings/1',
  };
}

function writeSdkBuildMetadata(outputRoot, files) {
  const headers = files.map((file) => file.path).sort(compareText);
  headers.push('flight/sdk/structural_members.hpp');
  const cmakeHeaders = headers
    .map((header) => `  "\${CMAKE_CURRENT_LIST_DIR}/../include/${header}"`)
    .join('\n');
  const cmakeTarget = path.join(outputRoot, 'cmake', 'FlightSdkHeaders.cmake');
  mkdirSync(path.dirname(cmakeTarget), { recursive: true });
  writeFileSync(
    cmakeTarget,
    `# Generated from the Flight SDK package graph. Do not edit.\nset(FLIGHT_SDK_GENERATED_HEADERS\n${cmakeHeaders}\n)\n`,
  );

  const bazelHeaders = headers.map((header) => `        "include/${header}",`).join('\n');
  writeFileSync(
    path.join(outputRoot, 'BUILD.bazel'),
    `# Generated from the Flight SDK package graph. Do not edit.\nload("@rules_cc//cc:defs.bzl", "cc_library")\n\npackage(default_visibility = ["//visibility:public"])\n\ncc_library(\n    name = "sdk_preview",\n    hdrs = [\n${bazelHeaders}\n    ],\n    strip_include_prefix = "include",\n    deps = ["//:cpp"],\n)\n`,
  );
}

function writeStructuralMemberTable(outputRoot, files) {
  const table = structuralMemberTable(files, overrideHeaderFiles(root));
  reportComputedCells(table.computedNames);
  if (table.contents === undefined) {
    // A SUBSET run legitimately reaches no structural row: @flighthq/math has none. Over the whole SDK
    // an empty set means the emitter stopped producing rows, which is worth failing on, so the guard
    // stays for a full run and only relaxes when a package filter narrowed the input.
    if (requestedPackages.length > 0) return;
    throw new Error('compiler output uses no structural row keys');
  }
  const target = path.join(outputRoot, 'include', 'flight', 'sdk', 'structural_members.hpp');
  mkdirSync(path.dirname(target), { recursive: true });
  writeFileSync(target, table.contents);
}

function generatedReadme(manifest) {
  const profiles = manifest.bindingProfiles.length === 0
    ? 'No external binding profile is applied; this is the portable floor.'
    : `Applied binding profiles: ${manifest.bindingProfiles.map((profile) => `\`${profile.identity}\``).join(', ')}.`;
  return `# Generated Flight SDK inventory

This directory is generated from \`${manifest.source.package}\` ${manifest.source.version} at
\`${manifest.source.revision}\` by \`flight-compiler\` at \`${manifest.compiler.revision}\`.
Do not edit it by hand.

${profiles} Exact profile paths and SHA-256 digests are recorded in \`manifest.json\`.

The current compiler emitted ${manifest.summary.emittedModules} of ${manifest.summary.sourceModules} source modules from
${manifest.summary.packages} SDK packages and refused ${manifest.summary.refusedModules}. Emitted headers live under
\`include/flight/<package>/\`; every refusal and its owning module is recorded in \`refusals.json\`.

This is a bring-up inventory. It is intentionally committed before it forms a completely compilable SDK closure.
CMake exposes the full inventory as \`Flight::SdkPreview\`, and Bazel exposes \`//:sdk_preview\`; the preview name
keeps the remaining native compile failures visible. The package graph applies the public C++ \`flight\` namespaces
and installed include prefixes. Package-local imports are written with their canonical installed paths so nested
includes honor the override directory's precedence. \`initialization.json\` records the compiler's dependency and
module-evaluation plan.

Regenerate and verify the tree from the repository root:

\`\`\`sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm run sdk:generate
npm run sdk:check
\`\`\`

Generate a profile-specific inventory outside the committed portable tree with:

\`\`\`sh
node scripts/sdkGeneration.mjs --binding-profile=bindings/runtime.json --binding-profile=bindings/headless.json --output=out/sdk-headless
\`\`\`

Add \`--progress\` to print the compiler phase and module before every synchronous lowering or
emission step. If an interrupted run stops advancing, its final line identifies the work in flight.
`;
}

function filesUnder(directory) {
  const files = [];
  for (const entry of readdirSync(directory).sort()) {
    const filename = path.join(directory, entry);
    const status = lstatSync(filename);
    if (status.isSymbolicLink()) continue;
    if (status.isDirectory()) files.push(...filesUnder(filename));
    else files.push(filename);
  }
  return files;
}

function listSourceFiles(directory) {
  if (!existsSync(directory)) return [];
  return filesUnder(directory)
    .filter((filename) => filename.endsWith('.ts') && !filename.endsWith('.d.ts') && !filename.endsWith('.test.ts'))
    .map((sourcePath) => ({ contents: readFileSync(sourcePath, 'utf8'), sourcePath }));
}

// Every named package plus everything it depends on, transitively, restricted to packages the SDK
// includes. Sorted, so a subset run is reproducible.
function transitivePackageClosure(names, upstreamDirectory, available) {
  const includable = new Set(available);
  const closure = new Set();
  const pending = [...names];
  while (pending.length > 0) {
    const name = pending.pop();
    if (closure.has(name) || !includable.has(name)) continue;
    closure.add(name);
    const file = path.join(upstreamDirectory, 'packages', name.replace('@flighthq/', ''), 'package.json');
    if (!existsSync(file)) continue;
    for (const dependency of Object.keys(JSON.parse(readFileSync(file, 'utf8')).dependencies ?? {})) {
      if (dependency.startsWith('@flighthq/')) pending.push(dependency);
    }
  }
  return [...closure].sort(compareText);
}

// The `flight.environment` a package declares for itself, or undefined when it is environment-agnostic.
function declaredEnvironment(packageRoot) {
  const file = path.join(packageRoot, 'package.json');
  if (!existsSync(file)) return undefined;
  const environment = JSON.parse(readFileSync(file, 'utf8')).flight?.environment;
  return typeof environment === 'string' && environment.length > 0 ? environment : undefined;
}

function cppPackageName(packageName) {
  return packageName.slice('@flighthq/'.length).replaceAll('-', '_');
}

function isMixedInventory(directory) {
  const file = path.join(directory, 'manifest.json');
  return existsSync(file) && isMixedManifest(JSON.parse(readFileSync(file, 'utf8')));
}

function compareTrees(expectedRoot, actualRoot) {
  if (!existsSync(actualRoot)) {
    return filesUnder(expectedRoot).map((filename) => portable(path.relative(expectedRoot, filename)));
  }
  const expected = new Map(
    filesUnder(expectedRoot).map((filename) => [portable(path.relative(expectedRoot, filename)), readFileSync(filename)]),
  );
  const actual = new Map(
    filesUnder(actualRoot).map((filename) => [portable(path.relative(actualRoot, filename)), readFileSync(filename)]),
  );
  const paths = [...new Set([...expected.keys(), ...actual.keys()])].sort();
  return paths.filter(
    (filename) =>
      !expected.has(filename) || !actual.has(filename) || !expected.get(filename).equals(actual.get(filename)),
  );
}

function validateInput(flightDependency, compilerDependency) {
  for (const dependency of [flightDependency, compilerDependency]) {
    if (!existsSync(path.join(dependency.directory, '.git'))) return `${dependency.name} is not rehydrated.`;
    const head = spawnSync('git', ['rev-parse', 'HEAD'], { cwd: dependency.directory, encoding: 'utf8' });
    if (head.status !== 0 || head.stdout.trim() !== dependency.commit) {
      return `${dependency.name} is not at pinned revision ${dependency.commit.slice(0, 7)}.`;
    }
    const status = spawnSync('git', ['status', '--porcelain'], { cwd: dependency.directory, encoding: 'utf8' });
    if (status.status !== 0 || status.stdout.length > 0) return `${dependency.name} has uncommitted changes.`;
  }
  if (!existsSync(path.join(compilerDependency.directory, 'node_modules', 'typescript'))) {
    return 'flight-compiler dependencies are not installed.';
  }
  if (!existsSync(path.join(flightDependency.directory, 'packages', 'sdk', 'package.json'))) {
    return 'the pinned Flight checkout has no @flighthq/sdk package.';
  }
  return undefined;
}

function runCompilerBuild(directory) {
  const npm = process.platform === 'win32' ? 'npm.cmd' : 'npm';
  const result = spawnSync(npm, ['run', 'build', '--silent'], { cwd: directory, encoding: 'utf8' });
  if (result.status !== 0) {
    process.stderr.write(result.stdout ?? '');
    process.stderr.write(result.stderr ?? '');
    throw new Error('Pinned flight-compiler build failed.');
  }
}

function portable(filename) {
  return filename.split(path.sep).join('/');
}

function compareText(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}

function summary(prefix, result) {
  const patched = result.patchedModules > 0 ? `; ${String(result.patchedModules)} source patch(es) applied` : '';
  const repaired =
    result.repairedFiles > 0 ? `; ${String(result.repairedFiles)} header(s) repaired after emission` : '';
  const aliased =
    result.aliasedStructs > 0 ? `; ${String(result.aliasedStructs)} duplicated structural struct(s) aliased` : '';
  return `${prefix}: ${String(result.emittedModules)}/${String(result.sourceModules)} modules emitted across ${String(result.packages)} packages; ${String(result.refusedModules)} refusals recorded${patched}${repaired}${aliased}.\n`;
}

// A repair that no longer matches any emitted header has outlived the emitter defect it answers.
// Reporting that as a failure is the whole reason the repair mechanism is safe to have: carrying a
// repair nobody needs is how a patched build becomes an unacknowledged fork of the generator.
// A patch whose module is still refused for the reason the patch names is not doing what it claims.
// Saying so is the difference between a declared workaround and a forgotten one.
function reportIneffectivePatches(result) {
  if (result.ineffectivePatches.length === 0) return false;
  process.stderr.write(
    `${String(result.ineffectivePatches.length)} source patch(es) did not remove the refusal they name:\n`,
  );
  for (const id of result.ineffectivePatches) process.stderr.write(`- ${id}\n`);
  process.stderr.write('Either the rewrite is wrong or the refusal has another cause; do not carry it unexamined.\n');
  return true;
}

function reportObsoleteRepairs(result) {
  if (result.obsoleteRepairs.length === 0) return false;
  process.stderr.write(
    `${String(result.obsoleteRepairs.length)} emission repair(s) no longer match any generated header:\n`,
  );
  for (const id of result.obsoleteRepairs) process.stderr.write(`- ${id}\n`);
  process.stderr.write('Delete them from repairs/emission-repairs.json; the defect they stood in for is gone.\n');
  return true;
}

// A pending patch is declared debt, not a silent skip: every run names how many are outstanding and which
// lane owns each group, so a generation never reads as though the whole patch set was applied.
function reportPendingSourcePatches(pending) {
  if (pending.length === 0) return;
  const byLane = new Map();
  for (const patch of pending) byLane.set(patch.pendingRebase.lane, [...(byLane.get(patch.pendingRebase.lane) ?? []), patch.id]);
  process.stderr.write(`${String(pending.length)} source patch(es) pending rebase and NOT applied:\n`);
  for (const [lane, ids] of [...byLane].sort(([left], [right]) => (left < right ? -1 : left > right ? 1 : 0))) {
    process.stderr.write(`- ${lane}: ${ids.join(', ')}\n`);
  }
}
