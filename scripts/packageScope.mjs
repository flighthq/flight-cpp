import { existsSync, lstatSync, readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';

// Package-scoped edits to the committed SDK inventory.
//
// `sdk:generate` rewrites the whole generated/ tree, which costs a full-SDK compile and lands every
// package's drift at once. A package owner needs the opposite: regenerate ONE package, and change
// nothing that package does not own. Generation still has to see the package's dependency closure --
// compiling @flighthq/math against nothing measures the omission, not math -- so the candidate tree is
// produced over the closure and then only the requested package's slice of it is carried across:
//
//   include/flight/<package>/**         replaced wholesale (files the candidate no longer emits go)
//   manifest.json                       the package's row, and the package's share of each per-file list
//   refusals.json, best-effort.json     the package's rows
//   initialization.json                 the package's entries, modules and evaluation groups
//   cmake/FlightSdkHeaders.cmake,
//   BUILD.bazel, README.md              re-derived from the merged tree, never from the candidate
//
// Everything else is shared by every package and is NOT rewritten here: structural_members.hpp is built
// from the row keys of the whole SDK, and the repair identity proof from the whole declaration file. A
// scoped run that would need either to change says so as a failure rather than quietly leaving them stale.
//
// Every ordered ledger is merged by removing the package's rows and re-inserting the candidate's at the
// position a full run would put them. That claim is checked, not assumed: scripts/packageScope.test.mjs
// removes and re-inserts every package of the committed tree and requires the result to be byte-identical.

export const SDK_PREFIX = '@flighthq/';
export const STRUCTURAL_MEMBERS = 'flight/sdk/structural_members.hpp';
export const DEFAULT_JOBS = 2;

export function normalizePackageName(name) {
  const trimmed = String(name ?? '').trim();
  if (trimmed.length === 0) throw new Error('Name a package, e.g. @flighthq/math or math.');
  return trimmed.startsWith(SDK_PREFIX) ? trimmed : `${SDK_PREFIX}${trimmed}`;
}

export function cppPackageName(packageName) {
  return packageName.slice(SDK_PREFIX.length).replaceAll('-', '_');
}

export function includePrefixOf(packageName) {
  return `flight/${cppPackageName(packageName)}/`;
}

// The CTest label carrying a package's semantic tests. One spelling, shared by tests/CMakeLists.txt and
// pkg:compile, so `ctest -L` and the script cannot disagree about what a package's tests are.
export function ctestLabelOf(packageName) {
  return `pkg:${packageName.slice(SDK_PREFIX.length)}`;
}

// The executable target behind a labelled test: `flight_cpp.generated_log` runs `flight_cpp_generated_log_tests`.
export function testTargetOf(testName) {
  return `${testName.replace('.', '_')}_tests`;
}

// `--package=x`, `--package x` and a bare positional are all accepted, once; anything else is refused.
// `-j N`, `-jN`, `--jobs N` and `--jobs=N` all land in values as `--jobs`; parseJobs validates them.
export function parsePackageArguments(argv, { flags = [], valued = [] } = {}) {
  const result = { flags: new Set(), packages: [], values: new Map() };
  for (let index = 0; index < argv.length; index++) {
    const argument = argv[index];
    if (argument === '--package') {
      result.packages.push(argv[++index]);
    } else if (argument.startsWith('--package=')) {
      result.packages.push(argument.slice('--package='.length));
    } else if (argument === '-j' || argument === '--jobs') {
      result.values.set('--jobs', argv[++index]);
    } else if (/^-j\d+$/u.test(argument)) {
      result.values.set('--jobs', argument.slice(2));
    } else if (argument.startsWith('--jobs=')) {
      result.values.set('--jobs', argument.slice('--jobs='.length));
    } else if (flags.includes(argument)) {
      result.flags.add(argument);
    } else if (valued.some((name) => argument.startsWith(`${name}=`))) {
      const name = valued.find((candidate) => argument.startsWith(`${candidate}=`));
      result.values.set(name, argument.slice(name.length + 1));
    } else if (!argument.startsWith('-')) {
      result.packages.push(argument);
    } else {
      throw new Error(`Unknown option ${argument}`);
    }
  }
  if (result.packages.length !== 1 || result.packages[0] === undefined) {
    throw new Error(
      'Name exactly one package. Package-scoped commands never operate on the whole SDK; ' +
        'use the sdk:* scripts for that.',
    );
  }
  return { ...result, package: normalizePackageName(result.packages[0]) };
}

export function parseJobs(raw, fallback = DEFAULT_JOBS) {
  if (raw === undefined || raw === '') return fallback;
  const jobs = Number.parseInt(String(raw), 10);
  if (!Number.isInteger(jobs) || jobs < 1 || String(jobs) !== String(raw).trim()) {
    throw new Error(`-j needs a positive integer, got ${String(raw)}`);
  }
  return jobs;
}

// Where a package's scoped reports live. Gitignored with the rest of out/, and per package so lanes
// working in parallel never overwrite each other's evidence.
export function packageOutputDirectory(root, packageName) {
  return path.join(root, 'out', 'pkg', packageName.slice(SDK_PREFIX.length));
}

// The headers a package owns in a generated tree, as include-relative paths, sorted.
export function packageHeaders(generatedRoot, packageName) {
  const directory = path.join(generatedRoot, 'include', ...includePrefixOf(packageName).split('/').filter(Boolean));
  if (!existsSync(directory)) return [];
  return filesUnder(directory)
    .map((filename) => portable(path.relative(path.join(generatedRoot, 'include'), filename)))
    .filter((header) => header.endsWith('.hpp'))
    .sort(compareText);
}

// Computes, without writing anything, every file the scoped merge changes. Returns a Map of
// generated-root-relative path -> new contents (string) or null (delete), plus what was found.
export function planPackageMerge({ candidateRoot, committedRoot, packageName }) {
  const prefix = includePrefixOf(packageName);
  const read = (root, file) => readFileSync(path.join(root, file), 'utf8');
  const readJson = (root, file) => JSON.parse(read(root, file));
  const committedManifest = readJson(committedRoot, 'manifest.json');
  const candidateManifest = readJson(candidateRoot, 'manifest.json');
  const problems = [];

  // A slice is only meaningful against the inventory it is spliced into. A different pin or binding
  // profile set changes every package at once, and that is a whole-SDK regeneration by definition.
  for (const field of ['compiler', 'source', 'bindingProfiles', 'namespaceMapping', 'schema']) {
    if (stableJson(committedManifest[field]) !== stableJson(candidateManifest[field])) {
      throw new Error(
        `The candidate's ${field} differs from the committed inventory's. A pin or profile change moves every ` +
          'package; regenerate the whole SDK with `npm run sdk:generate` instead.',
      );
    }
  }
  const candidateRow = candidateManifest.packages.find((row) => row.package === packageName);
  if (candidateRow === undefined) throw new Error(`The candidate inventory has no row for ${packageName}.`);

  const writes = new Map();
  const committedHeaders = new Set(includeFilesUnder(committedRoot, prefix));
  const candidateHeaders = includeFilesUnder(candidateRoot, prefix);
  const changedHeaders = [];
  for (const header of candidateHeaders) {
    const contents = read(candidateRoot, `include/${header}`);
    if (!committedHeaders.has(header) || read(committedRoot, `include/${header}`) !== contents) {
      writes.set(`include/${header}`, contents);
      changedHeaders.push(header);
    }
    committedHeaders.delete(header);
  }
  for (const header of committedHeaders) {
    writes.set(`include/${header}`, null);
    changedHeaders.push(header);
  }
  const inPrefix = (file) => file.startsWith(prefix);
  const mergedHeaderText = (header) =>
    writes.has(`include/${header}`) ? writes.get(`include/${header}`) : read(committedRoot, `include/${header}`);

  // manifest.json
  const manifest = structuredClone(committedManifest);
  manifest.packages = replaceSortedRows(
    manifest.packages,
    (row) => row.package === packageName,
    [candidateRow],
    (left, right) => compareText(left.package, right.package),
  );
  manifest.summary = {
    packages: manifest.packages.length,
    emittedModules: sum(manifest.packages, 'emittedModules'),
    refusedModules: sum(manifest.packages, 'refusedModules'),
    sourceModules: sum(manifest.packages, 'sourceModules'),
  };
  manifest.emissionRepairs = mergeRepairRecords(committedManifest.emissionRepairs, candidateManifest.emissionRepairs, inPrefix);
  manifest.sourcePatches = replaceRowsInPlace(
    committedManifest.sourcePatches ?? [],
    (patch) => patch.package === packageName,
    (candidateManifest.sourcePatches ?? []).filter((patch) => patch.package === packageName),
  );
  const aliases = committedManifest.duplicateStructuralStructAliases;
  const aliasFiles = mergeFileList(aliases.files, candidateManifest.duplicateStructuralStructAliases.files, inPrefix);
  manifest.duplicateStructuralStructAliases = {
    files: aliasFiles,
    structs: mergeAliasedStructs({
      aliasFiles,
      committedFiles: aliases.files,
      committedStructs: aliases.structs,
      committedText: (file) => read(committedRoot, `include/${file}`),
      mergedText: mergedHeaderText,
      prefix,
    }),
  };
  const includes = committedManifest.canonicalPackageLocalIncludes;
  const includeFiles = mergeFileList(
    includes.files,
    candidateManifest.canonicalPackageLocalIncludes.files,
    inPrefix,
  );
  // The total counts quoted includes the canonicalizer rewrote, and a rewritten include is
  // indistinguishable from an emitted angle include once written. Unchanged files therefore contribute
  // nothing, and a changed file contributes the difference in its own same-package angle includes,
  // which cancels every include the emitter wrote that way itself.
  let includeDelta = 0;
  for (const header of changedHeaders) {
    const old = includes.files.includes(header)
      ? samePackageIncludes(header, readIfPresent(committedRoot, `include/${header}`) ?? '')
      : 0;
    const now = includeFiles.includes(header) ? samePackageIncludes(header, writes.get(`include/${header}`) ?? '') : 0;
    includeDelta += now - old;
  }
  manifest.canonicalPackageLocalIncludes = { files: includeFiles, includes: includes.includes + includeDelta };
  writes.set('manifest.json', `${JSON.stringify(manifest, undefined, 2)}\n`);

  // refusals.json
  const refusals = readJson(committedRoot, 'refusals.json');
  refusals.refusals = replaceSortedRows(
    refusals.refusals,
    (row) => row.package === packageName,
    readJson(candidateRoot, 'refusals.json').refusals.filter((row) => row.package === packageName),
    (left, right) => compareText(left.package, right.package),
  );
  writes.set('refusals.json', `${JSON.stringify(refusals, undefined, 2)}\n`);

  // best-effort.json
  if (existsSync(path.join(committedRoot, 'best-effort.json')) || existsSync(path.join(candidateRoot, 'best-effort.json'))) {
    if (!existsSync(path.join(committedRoot, 'best-effort.json')) || !existsSync(path.join(candidateRoot, 'best-effort.json'))) {
      throw new Error('Only one of the trees has best-effort.json; regenerate the whole SDK in one mode.');
    }
    const bestEffort = readJson(committedRoot, 'best-effort.json');
    // `consumers` names every module that imports this one, and a reverse dependent is by definition
    // outside the dependency closure the candidate was generated over. Those consumers are kept from the
    // committed record; consumers inside the closure are the candidate's to state.
    const closure = new Set(candidateManifest.packages.map((row) => row.package));
    const committedConsumers = new Map(
      bestEffort.modules
        .filter((row) => row.module.packageName === packageName)
        .map((row) => [row.module.source, row.consumers ?? []]),
    );
    const candidateModules = readJson(candidateRoot, 'best-effort.json')
      .modules.filter((row) => row.module.packageName === packageName)
      .map((row) => {
        const outside = (committedConsumers.get(row.module.source) ?? []).filter(
          (consumer) => !closure.has(consumerPackage(consumer)),
        );
        if (outside.length === 0 || row.consumers === undefined) return row;
        return { ...row, consumers: [...new Set([...row.consumers, ...outside])].sort(compareText) };
      });
    bestEffort.modules = replaceSortedRows(
      bestEffort.modules,
      (row) => row.module.packageName === packageName,
      candidateModules,
      (left, right) => compareText(left.module.packageName, right.module.packageName),
    );
    writes.set('best-effort.json', `${JSON.stringify(bestEffort, undefined, 2)}\n`);
  }

  // initialization.json
  const initialization = readJson(committedRoot, 'initialization.json');
  const candidateInitialization = readJson(candidateRoot, 'initialization.json');
  const owns = (module) => module.packageName === packageName;
  initialization.entries = replaceSortedRows(
    initialization.entries,
    owns,
    candidateInitialization.entries.filter(owns),
    (left, right) => compareText(left.packageName, right.packageName),
  );
  // Unlike the other ledgers these are NOT grouped by package, so each row is placed on its own.
  initialization.modules = mergeSortedRows(
    initialization.modules,
    (row) => owns(row.module),
    candidateInitialization.modules.filter((row) => owns(row.module)),
    compareInitializationModules,
  );
  // Evaluation groups are a topological order over the WHOLE graph, and the closure orders the same groups
  // differently, so where a group sits cannot be decided from the package alone. Each group is
  // single-package (checked). While the package's groups are the same set, their committed placement
  // stands; a changed set is reported for a full run and the committed groups are left alone.
  const groupOwner = (group) => {
    const owners = new Set(group.modules.map((module) => module.packageName));
    if (owners.size !== 1) throw new Error('An initialization group spans packages; scoped merge cannot place it.');
    return [...owners][0];
  };
  const committedGroups = initialization.groups.filter((group) => groupOwner(group) === packageName);
  const candidateGroups = candidateInitialization.groups.filter((group) => groupOwner(group) === packageName);
  const groupKey = (group) => stableJson(group);
  const committedGroupKeys = new Set(committedGroups.map(groupKey));
  const groupsChanged =
    committedGroups.length !== candidateGroups.length ||
    candidateGroups.some((group) => !committedGroupKeys.has(groupKey(group)));
  if (groupsChanged) {
    problems.push(
      `${packageName}'s evaluation groups changed; their place in the whole-SDK topological order of ` +
        'initialization.json needs `npm run sdk:generate`. Those groups were left as committed.',
    );
  }
  writes.set('initialization.json', `${JSON.stringify(initialization, undefined, 2)}\n`);

  // Build metadata and README are pure functions of the merged tree and manifest.
  const mergedIncludeFiles = includeFilesUnder(committedRoot, '')
    .filter((file) => !(writes.has(`include/${file}`) && writes.get(`include/${file}`) === null))
    .concat(candidateHeaders.filter((file) => !existsSync(path.join(committedRoot, 'include', file))));
  const metadata = sdkBuildMetadata(mergedIncludeFiles);
  writes.set('cmake/FlightSdkHeaders.cmake', metadata.cmake);
  writes.set('BUILD.bazel', metadata.bazel);
  writes.set('README.md', readmeWithSummary(read(committedRoot, 'README.md'), manifest.summary));

  // Row keys are vocabulary shared by every package; a key this package introduces has no case in the
  // committed table, and `row_get` on it would fail with a static_assert far from the cause.
  const table = existsSync(path.join(committedRoot, 'include', STRUCTURAL_MEMBERS))
    ? structuralTableKeys(read(committedRoot, `include/${STRUCTURAL_MEMBERS}`))
    : new Set();
  const missingKeys = new Set();
  for (const header of candidateHeaders) {
    for (const key of rowKeysIn(read(candidateRoot, `include/${header}`))) if (!table.has(key)) missingKeys.add(key);
  }
  if (missingKeys.size > 0) {
    problems.push(
      `${packageName} uses row key(s) absent from ${STRUCTURAL_MEMBERS}: ${[...missingKeys].sort(compareText).join(', ')}. ` +
        'That table is shared by every package; it needs `npm run sdk:generate`.',
    );
  }

  // Drop no-op writes so a check reports only real drift and an apply touches only what moved.
  for (const [file, contents] of [...writes]) {
    if (contents !== null && readIfPresent(committedRoot, file) === contents) writes.delete(file);
  }
  return { changedHeaders: changedHeaders.sort(compareText), problems, row: candidateRow, writes };
}

// Removes every row `belongs` selects, then inserts `replacement` rows at their upper bound under
// `compare`, preserving the replacement's own relative order. When the package already had rows, they
// go back exactly where the first old row was; that is what makes a remove-and-reinsert an identity.
export function replaceSortedRows(rows, belongs, replacement, compare) {
  const firstOld = rows.findIndex(belongs);
  const kept = rows.filter((row) => !belongs(row));
  if (replacement.length === 0) return kept;
  let at;
  if (firstOld >= 0) {
    at = rows.slice(0, firstOld).filter((row) => !belongs(row)).length;
  } else {
    at = kept.findIndex((row) => compare(row, replacement[0]) > 0);
    if (at < 0) at = kept.length;
  }
  return [...kept.slice(0, at), ...replacement, ...kept.slice(at)];
}

// For a ledger sorted by a total order that interleaves packages: drop the package's rows, then place
// each replacement row at its own upper bound.
export function mergeSortedRows(rows, belongs, replacement, compare) {
  const result = rows.filter((row) => !belongs(row));
  for (const row of replacement) {
    let at = result.findIndex((existing) => compare(existing, row) > 0);
    if (at < 0) at = result.length;
    result.splice(at, 0, row);
  }
  return result;
}

// Same, for ledgers with no sort order: the replacement takes the old rows' positions one for one, and
// any surplus follows the last of them (or is appended when the package had none).
export function replaceRowsInPlace(rows, belongs, replacement) {
  const result = [];
  let next = 0;
  let lastOld = -1;
  for (const row of rows) {
    if (belongs(row)) {
      if (next < replacement.length) result.push(replacement[next++]);
      lastOld = result.length;
    } else {
      result.push(row);
    }
  }
  const rest = replacement.slice(next);
  if (rest.length === 0) return result;
  const at = lastOld < 0 ? result.length : lastOld;
  return [...result.slice(0, at), ...rest, ...result.slice(at)];
}

// Module evaluation records are ordered by module name, then package, then source, in code-unit order.
export function compareInitializationModules(left, right) {
  return (
    compareText(left.module.name, right.module.name) ||
    compareText(left.module.packageName, right.module.packageName) ||
    compareText(left.module.source, right.module.source)
  );
}

// File lists are sorted except where a repair recorded them in emission order, so the package's files
// go back where its old ones were, and only a package new to the list is placed by sort order.
function mergeFileList(committed, candidate, inPrefix) {
  return replaceSortedRows(committed, inPrefix, candidate.filter(inPrefix), compareText);
}

function mergeRepairRecords(committed, candidate, inPrefix) {
  const candidateById = new Map(candidate.map((record) => [record.id, record]));
  const merged = committed.map((record) => ({
    files: mergeFileList(record.files, candidateById.get(record.id)?.files ?? [], inPrefix),
    id: record.id,
  }));
  const known = new Set(committed.map((record) => record.id));
  // A repair declared since the inventory was last generated in full is shared drift, not this package's;
  // it joins only when it actually touched the package, and then only with the package's files.
  for (const record of candidate) {
    if (known.has(record.id)) continue;
    const files = record.files.filter(inPrefix);
    if (files.length > 0) merged.push({ files, id: record.id });
  }
  return merged;
}

// `structs` names every struct the alias pass turned into an alias anywhere. A name stays when some
// aliased file outside the package still aliases it (or when it cannot be found textually at all, so a
// pattern this function does not recognize never deletes a fact), and the package's own aliases join.
function mergeAliasedStructs({ aliasFiles, committedFiles, committedStructs, committedText, mergedText, prefix }) {
  const aliasedIn = (text) => new Set([...text.matchAll(/\busing (\w+) = flight::types::\1;/gu)].map((match) => match[1]));
  const outside = new Set();
  const seenCommitted = new Set();
  for (const file of committedFiles) {
    const names = aliasedIn(committedText(file));
    for (const name of names) seenCommitted.add(name);
    if (!file.startsWith(prefix)) for (const name of names) outside.add(name);
  }
  const result = new Set(committedStructs.filter((name) => outside.has(name) || !seenCommitted.has(name)));
  for (const file of aliasFiles.filter((candidate) => candidate.startsWith(prefix))) {
    for (const name of aliasedIn(mergedText(file) ?? '')) result.add(name);
  }
  return [...result].sort(compareText);
}

// `@flighthq/camera-controls/packages/camera-controls/src/x.ts` -> `@flighthq/camera-controls`.
function consumerPackage(consumer) {
  const at = consumer.indexOf('/packages/');
  return at < 0 ? consumer : consumer.slice(0, at);
}

function samePackageIncludes(header, contents) {
  const owner = header.split('/')[1];
  return (contents.match(new RegExp(`^#include <flight/${owner}/[^>\\r\\n]+>$`, 'gmu')) ?? []).length;
}

// Mirrors scripts/sdkGeneration.mjs#writeSdkBuildMetadata: every emitted header sorted, then the
// structural member table, which that script writes after the compiler's files.
export function sdkBuildMetadata(includeFiles) {
  const headers = includeFiles.filter((file) => file !== STRUCTURAL_MEMBERS).sort(compareText);
  headers.push(STRUCTURAL_MEMBERS);
  const cmakeHeaders = headers.map((header) => `  "\${CMAKE_CURRENT_LIST_DIR}/../include/${header}"`).join('\n');
  const bazelHeaders = headers.map((header) => `        "include/${header}",`).join('\n');
  return {
    bazel: `# Generated from the Flight SDK package graph. Do not edit.\nload("@rules_cc//cc:defs.bzl", "cc_library")\n\npackage(default_visibility = ["//visibility:public"])\n\ncc_library(\n    name = "sdk_preview",\n    hdrs = [\n${bazelHeaders}\n    ],\n    strip_include_prefix = "include",\n    deps = ["//:cpp"],\n)\n`,
    cmake: `# Generated from the Flight SDK package graph. Do not edit.\nset(FLIGHT_SDK_GENERATED_HEADERS\n${cmakeHeaders}\n)\n`,
  };
}

export function readmeWithSummary(readme, summary) {
  const pattern = /The current compiler emitted \d+ of \d+ source modules from\n\d+ SDK packages and refused \d+\./u;
  if (!pattern.test(readme)) throw new Error('generated/README.md no longer carries the summary sentence this script updates.');
  return readme.replace(
    pattern,
    `The current compiler emitted ${String(summary.emittedModules)} of ${String(summary.sourceModules)} source modules from\n` +
      `${String(summary.packages)} SDK packages and refused ${String(summary.refusedModules)}.`,
  );
}

export function structuralTableKeys(contents) {
  return new Set(
    [...contents.matchAll(/Key::name\.view\(\) == std::string_view\(("(?:[^"\\]|\\.)*")\)/gu)].map((match) =>
      JSON.parse(match[1]),
    ),
  );
}

export function rowKeysIn(contents) {
  return new Set(
    [...contents.matchAll(/flight::RowKey<"((?:[^"\\]|\\.)*)">/gu)].map((match) => JSON.parse(`"${match[1]}"`)),
  );
}

function includeFilesUnder(generatedRoot, prefix) {
  const includeRoot = path.join(generatedRoot, 'include');
  const directory = path.join(includeRoot, ...prefix.split('/').filter(Boolean));
  if (!existsSync(directory)) return [];
  return filesUnder(directory)
    .map((filename) => portable(path.relative(includeRoot, filename)))
    .sort(compareText);
}

function readIfPresent(root, file) {
  const filename = path.join(root, file);
  return existsSync(filename) ? readFileSync(filename, 'utf8') : undefined;
}

export function filesUnder(directory) {
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

function sum(rows, field) {
  return rows.reduce((total, row) => total + row[field], 0);
}

function stableJson(value) {
  return JSON.stringify(value ?? null);
}

export function portable(filename) {
  return filename.split(path.sep).join('/');
}

export function compareText(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}
