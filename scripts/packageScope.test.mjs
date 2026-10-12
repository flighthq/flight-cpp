import assert from 'node:assert/strict';
import { cpSync, existsSync, mkdirSync, readdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { after, describe, test } from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  applyPackageMerge,
  bindingProfileSetId,
  compareInitializationModules,
  danglingIncludes,
  compareText,
  ctestLabelOf,
  evaluationGroups,
  finalizeProvenance,
  mergeSortedRows,
  normalizePackageName,
  packageHeaders,
  parseJobs,
  parsePackageArguments,
  planPackageMerge,
  readmeWithProvenance,
  replaceRowsInPlace,
  replaceSortedRows,
  rowProvenance,
  syncTree,
  testTargetOf,
  treeDifferences,
  validateProvenance,
  verifyMixedInventory,
  structuralTableKeys,
} from './packageScope.mjs';
import { overrideHeaderFiles, structuralMemberTable } from './structuralMembers.mjs';

// Run with `npm run pkg:test`. Needs nothing but the committed generated/ tree.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const scratch = mkdtempSync(path.join(tmpdir(), 'flight-cpp-package-scope-'));
after(() => rmSync(scratch, { force: true, recursive: true }));
// The fixtures are a single-revision inventory, the form a full run writes. The committed tree may be
// mid-transition, so it is collapsed onto the revision most of its rows hold; only provenance changes.
const generated = uniformInventory(path.join(root, 'generated'), path.join(scratch, 'uniform'));
const json = (file) => JSON.parse(readFileSync(path.join(generated, file), 'utf8'));
const manifest = json('manifest.json');
const packages = manifest.packages.map((row) => row.package);

describe('arguments', () => {
  test('exactly one package is required, in any accepted spelling', () => {
    assert.equal(parsePackageArguments(['math']).package, '@flighthq/math');
    assert.equal(parsePackageArguments(['--package=@flighthq/math']).package, '@flighthq/math');
    assert.equal(parsePackageArguments(['--package', 'render-gl']).package, '@flighthq/render-gl');
    assert.throws(() => parsePackageArguments([]), /exactly one package/u);
    assert.throws(() => parsePackageArguments(['math', 'types']), /exactly one package/u);
    assert.deepEqual(parsePackageArguments(['compression', 'swf', '@flighthq/swf'], { allowGroup: true }).packages, [
      '@flighthq/compression',
      '@flighthq/swf',
    ]);
    assert.throws(() => parsePackageArguments([], { allowGroup: true }), /one or more packages/u);
    assert.throws(() => parsePackageArguments(['math', '--all']), /Unknown option --all/u);
  });

  test('jobs default to two and reject nonsense', () => {
    for (const spelling of [['-j', '3'], ['-j3'], ['--jobs', '3'], ['--jobs=3']]) {
      assert.equal(parseJobs(parsePackageArguments(['math', ...spelling]).values.get('--jobs')), 3, spelling.join(' '));
    }
    assert.equal(parseJobs(parsePackageArguments(['math']).values.get('--jobs')), 2);
    assert.equal(parseJobs(undefined), 2);
    assert.equal(parseJobs('1'), 1);
    assert.throws(() => parseJobs('0'));
    assert.throws(() => parseJobs('2x'));
  });

  test('a package label is the unscoped package name', () => {
    assert.equal(ctestLabelOf(normalizePackageName('render-state')), 'pkg:render-state');
  });

  test('package headers are exactly the package include prefix', () => {
    const headers = packageHeaders(generated, '@flighthq/math');
    assert.ok(headers.length > 0);
    assert.ok(headers.every((header) => header.startsWith('flight/math/')));
    // `math` must not swallow a sibling whose name merely starts with it.
    assert.ok(!packageHeaders(generated, '@flighthq/render').some((header) => header.startsWith('flight/render_')));
  });
});

describe('CTest package labels', () => {
  // Every test that includes a package's generated headers carries that package's label, and no label
  // names something that is not an SDK package -- otherwise `ctest -L` silently runs fewer tests.
  test('match the SDK packages each test includes', () => {
    const cmake = readFileSync(path.join(root, 'tests', 'CMakeLists.txt'), 'utf8');
    const byDirectory = new Map(manifest.packages.map((row) => [row.cppIncludePrefix.slice('flight/'.length), row.package]));
    const sources = new Map(
      [...cmake.matchAll(/add_executable\(\s*(\S+)\s+([^)]*)\)/gu)].map((match) => [match[1], match[2].split(/\s+/u).filter(Boolean)]),
    );
    const labels = new Map(
      [...cmake.matchAll(/^[ \t]*flight_cpp_package_test_labels\((\S+) ([^)]*)\)$/gmu)].map((match) => [
        match[1],
        match[2].split(' ').map((name) => `pkg:${name}`).sort(compareText),
      ]),
    );
    let checked = 0;
    for (const [, name, executable] of cmake.matchAll(/add_test\(\s*NAME\s+(\S+)\s+COMMAND\s+([^\s)]+)/gu)) {
      const included = new Set();
      for (const source of sources.get(executable) ?? []) {
        if (!/\.(?:c|cpp)$/u.test(source)) continue;
        const text = readFileSync(path.join(root, 'tests', source), 'utf8');
        // Only a header the generated tree has: the runtime's own include tree reuses names like `host`.
        for (const [, header, directory] of text.matchAll(/^#include <(flight\/([a-z0-9_]+)\/[^>]+)>/gmu)) {
          if (!existsSync(path.join(generated, 'include', header))) continue;
          const package_ = byDirectory.get(directory);
          if (package_ !== undefined) included.add(ctestLabelOf(package_));
        }
      }
      assert.deepEqual(labels.get(name) ?? [], [...included].sort(compareText), name);
      // pkg:compile --ctest builds a labelled test's target by this name before it exists to be listed.
      if (labels.has(name)) assert.equal(testTargetOf(name), executable, name);
      labels.delete(name);
      checked++;
    }
    assert.ok(checked > 20);
    assert.deepEqual([...labels.keys()], [], 'labels on tests that do not exist');
  });
});

describe('ordered ledgers reinsert where a full run puts them', () => {
  // Removing a package's rows and inserting them as if new must reproduce the committed ledger for EVERY
  // package. This is the claim the scoped merge makes when a package gains its first rows.
  const cases = [
    ['manifest packages', manifest.packages, (row) => row.package, (l, r) => compareText(l.package, r.package)],
    ['refusals', json('refusals.json').refusals, (row) => row.package, (l, r) => compareText(l.package, r.package)],
    [
      'best-effort modules',
      json('best-effort.json').modules,
      (row) => row.module.packageName,
      (l, r) => compareText(l.module.packageName, r.module.packageName),
    ],
    [
      'initialization entries',
      json('initialization.json').entries,
      (row) => row.packageName,
      (l, r) => compareText(l.packageName, r.packageName),
    ],
  ];
  for (const [name, rows, owner, compare] of cases) {
    test(name, () => {
      for (const package_ of packages) {
        const own = rows.filter((row) => owner(row) === package_);
        const without = rows.filter((row) => owner(row) !== package_);
        const rebuilt = replaceSortedRows(without, (row) => owner(row) === package_, own, compare);
        assert.deepEqual(rebuilt, rows, `${name} for ${package_}`);
      }
    });
  }

  test('evaluation groups are exactly what the compiler Tarjan walk computes from the records', () => {
    const initialization = json('initialization.json');
    assert.deepEqual(evaluationGroups(initialization), initialization.groups);
  });

  test('initialization modules, which interleave packages', () => {
    const rows = json('initialization.json').modules;
    for (const package_ of packages) {
      const belongs = (row) => row.module.packageName === package_;
      const own = rows.filter(belongs);
      const without = rows.filter((row) => !belongs(row));
      assert.deepEqual(mergeSortedRows(without, belongs, own, compareInitializationModules), rows, package_);
    }
  });

  test('in-place replacement keeps positions and places surplus after the last old row', () => {
    const rows = ['a1', 'b1', 'a2', 'c1'];
    const belongs = (row) => row.startsWith('a');
    assert.deepEqual(replaceRowsInPlace(rows, belongs, ['A1', 'A2']), ['A1', 'b1', 'A2', 'c1']);
    assert.deepEqual(replaceRowsInPlace(rows, belongs, ['A1', 'A2', 'A3']), ['A1', 'b1', 'A2', 'A3', 'c1']);
    assert.deepEqual(replaceRowsInPlace(rows, belongs, ['A1']), ['A1', 'b1', 'c1']);
    assert.deepEqual(replaceRowsInPlace(['b1'], belongs, ['A1']), ['b1', 'A1']);
  });
});

describe('scoped merge', () => {
  test('merging the committed tree into itself changes nothing, for every package', () => {
    for (const package_ of packages) {
      const plan = planPackageMerge({ candidateRoot: generated, committedRoot: generated, packageName: package_ });
      assert.deepEqual([...plan.writes.keys()], [], package_);
      assert.deepEqual(plan.problems, [], package_);
    }
  });

  test('only the requested package subtree and its rows move', () => {
    const committed = copyTree('committed');
    const candidate = copyTree('candidate');
    const target = '@flighthq/math';
    // The dropped header must be one no other package includes, or the move is (rightly) refused.
    const includedElsewhere = (header) =>
      packageHeaders(candidate, '@flighthq/types')
        .concat(...packages.filter((name) => name !== target).map((name) => packageHeaders(candidate, name)))
        .some((other) => readFileSync(path.join(candidate, 'include', other), 'utf8').includes(`<${header}>`));
    const headers = packageHeaders(candidate, target);
    const dropped = headers.find((header) => !header.endsWith('_internal_index.hpp') && !includedElsewhere(header));
    const kept = headers.find((header) => header !== dropped);
    assert.ok(dropped !== undefined, 'math has a header no other package includes');
    // A candidate that changes one header, drops another, adds a third and refuses one more module --
    // and ALSO differs in a package outside the request, which must be ignored.
    appendTo(candidate, `include/${kept}`, '\n// candidate change\n');
    rmSync(path.join(candidate, 'include', dropped));
    writeFileSync(path.join(candidate, 'include', 'flight', 'math', 'added_by_candidate.hpp'), '#pragma once\n');
    appendTo(candidate, 'include/flight/types/contract.hpp', '\n// outside the request\n');
    editJson(candidate, 'manifest.json', (value) => {
      const row = value.packages.find((entry) => entry.package === target);
      row.emittedModules -= 1;
      row.refusedModules += 1;
      value.packages.find((entry) => entry.package === '@flighthq/types').emittedModules += 100;
    });
    const refusal = { code: 'test', module: 'packages/math/src/x.ts', package: target, reason: 'test', stage: 'test' };
    editJson(candidate, 'refusals.json', (value) => {
      value.refusals.push(refusal);
      value.refusals.push({ ...refusal, package: '@flighthq/types' });
    });

    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: target });
    const headerWrites = [...plan.writes.keys()].filter((file) => file.startsWith('include/')).sort(compareText);
    assert.deepEqual(
      headerWrites,
      [`include/${dropped}`, `include/${kept}`, 'include/flight/math/added_by_candidate.hpp'].sort(compareText),
    );
    assert.equal(plan.writes.get(`include/${dropped}`), null);
    const merged = JSON.parse(plan.writes.get('manifest.json'));
    const before = manifest.packages.find((entry) => entry.package === target);
    const row = merged.packages.find((entry) => entry.package === target);
    assert.equal(row.refusedModules, before.refusedModules + 1);
    assert.deepEqual(
      merged.packages.find((entry) => entry.package === '@flighthq/types'),
      manifest.packages.find((entry) => entry.package === '@flighthq/types'),
    );
    assert.equal(merged.summary.refusedModules, manifest.summary.refusedModules + 1);
    assert.equal(merged.summary.emittedModules, manifest.summary.emittedModules - 1);
    const refusals = JSON.parse(plan.writes.get('refusals.json')).refusals;
    assert.equal(refusals.filter((entry) => entry.code === 'test').length, 1);
    assert.equal(refusals.find((entry) => entry.code === 'test').package, target);
    const cmake = plan.writes.get('cmake/FlightSdkHeaders.cmake');
    assert.ok(cmake.includes('flight/math/added_by_candidate.hpp'));
    assert.ok(!cmake.includes(`include/${dropped}"`));
    assert.match(plan.writes.get('README.md'), new RegExp(`refused ${String(merged.summary.refusedModules)}\\.`, 'u'));
  });

  test('closure-only artifacts do not leak: reverse-dependent consumers stay, group order stays', () => {
    const committed = copyTree('closure-committed');
    const candidate = copyTree('closure-candidate');
    const target = '@flighthq/math';
    const outsideConsumer = (consumer) => !consumer.startsWith(`${target}/`);
    // A closure run cannot see reverse dependents, and orders the same evaluation groups differently.
    editJson(candidate, 'best-effort.json', (value) => {
      for (const row of value.modules) {
        if (row.module.packageName === target) row.consumers = row.consumers.filter((c) => !outsideConsumer(c));
      }
    });
    editJson(candidate, 'manifest.json', (value) => {
      value.packages = value.packages.filter((row) => ['@flighthq/math', '@flighthq/types'].includes(row.package));
    });
    editJson(candidate, 'initialization.json', (value) => {
      const own = value.groups.filter((group) => group.modules[0].packageName === target);
      assert.ok(own.length > 1);
      const reversed = [...own].reverse();
      value.groups = value.groups.map((group) => (group.modules[0].packageName === target ? reversed.shift() : group));
    });
    // A repair declared after the last full run that never touches the package is not its to add.
    editJson(candidate, 'manifest.json', (value) => {
      value.emissionRepairs.push({ files: [], id: 'declared-since-last-full-run' });
      value.emissionRepairs.push({ files: ['flight/types/contract.hpp'], id: 'elsewhere-since-last-full-run' });
    });
    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: target });
    assert.deepEqual([...plan.writes.keys()], []);
    assert.deepEqual(plan.problems, []);
    assert.ok(json('best-effort.json').modules.some((row) => row.module.packageName === target && row.consumers.some(outsideConsumer)));
  });

  test('evaluation groups are recomputed, so a package that gains a module keeps every module in one group', () => {
    const target = '@flighthq/math';
    const committed = copyTree('groups-committed');
    const candidate = copyTree('groups-candidate');
    // The candidate grows math by one module that imports angle at runtime, as a new upstream file would.
    editJson(candidate, 'initialization.json', (value) => {
      const angle = value.modules.find((row) => row.module.packageName === target && row.module.source.endsWith('/angle.ts'));
      const added = { name: 'Added', packageName: target, source: 'packages/math/src/added.ts' };
      value.entries.push(added);
      value.modules.push({
        bindings: [],
        dependencies: [{ evaluation: 'runtime', importer: added, specifier: './angle', target: angle.module }],
        module: added,
        steps: [],
      });
    });
    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: target });
    assert.deepEqual(plan.problems, []);
    const merged = JSON.parse(plan.writes.get('initialization.json'));
    const counts = new Map();
    for (const group of merged.groups) {
      for (const module of group.modules) counts.set(`${module.packageName}|${module.source}`, (counts.get(`${module.packageName}|${module.source}`) ?? 0) + 1);
    }
    assert.equal(counts.size, merged.modules.length);
    assert.ok([...counts.values()].every((count) => count === 1));
    assert.equal(counts.get(`${target}|packages/math/src/added.ts`), 1);
    // A dependency's group precedes its importer's.
    const position = (source) => merged.groups.findIndex((group) => group.modules.some((module) => module.source === source));
    assert.ok(position('packages/math/src/angle.ts') < position('packages/math/src/added.ts'));
  });

  test('a package that drops a module another package still uses cannot move alone', () => {
    const committed = copyTree('orphan-committed');
    const candidate = copyTree('orphan-candidate');
    const target = '@flighthq/math';
    // camera-controls imports math's angle module; a candidate without it would strand that import.
    editJson(candidate, 'initialization.json', (value) => {
      value.entries = value.entries.filter((entry) => !(entry.packageName === target && entry.source.endsWith('/angle.ts')));
      value.modules = value.modules.filter((row) => !(row.module.packageName === target && row.module.source.endsWith('/angle.ts')));
    });
    rmSync(path.join(candidate, 'include', 'flight', 'math', 'angle.hpp'));
    assert.throws(
      () => planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: target }),
      /cannot move alone.*@flighthq\/camera-controls/su,
    );
  });

  test('a coordinated group may drop a module its own members import, and lands as one unit', () => {
    const committed = copyTree('group-committed');
    const candidate = copyTree('group-candidate');
    const target = '@flighthq/math';
    const partner = '@flighthq/camera-controls';
    editJson(candidate, 'initialization.json', (value) => {
      value.entries = value.entries.filter((entry) => !(entry.packageName === target && entry.source.endsWith('/angle.ts')));
      value.modules = value.modules
        .filter((row) => !(row.module.packageName === target && row.module.source.endsWith('/angle.ts')))
        .map((row) =>
          [target, partner].includes(row.module.packageName)
            ? { ...row, dependencies: row.dependencies.filter((dependency) => !dependency.target.source.endsWith('math/src/angle.ts')) }
            : row,
        );
    });
    rmSync(path.join(candidate, 'include', 'flight', 'math', 'angle.hpp'));
    for (const header of [...packageHeaders(candidate, partner), ...packageHeaders(candidate, target)]) {
      const filename = path.join(candidate, 'include', header);
      writeFileSync(filename, readFileSync(filename, 'utf8').replaceAll('#include <flight/math/angle.hpp>\n', ''));
    }
    // Alone, math is refused; with camera-controls in the same group it is not, and the merged tree has
    // nothing left that includes the dropped header.
    assert.throws(() => planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: target }), /cannot move alone/u);
    for (const [name, partners] of [[target, [partner]], [partner, [target]]]) {
      applyPackageMerge(planPackageMerge({ candidateRoot: candidate, committedRoot: committed, movingTogether: partners, packageName: name }), committed);
    }
    assert.ok(!existsSync(path.join(committed, 'include', 'flight', 'math', 'angle.hpp')));
    assert.deepEqual(danglingIncludes(committed, [target, partner], [path.join(root, 'include'), path.join(root, 'overrides', 'include')]), new Map());
  });

  test('a moved package that includes a header no root provides is reported', () => {
    const tree = copyTree('dangling-tree');
    const [header] = packageHeaders(tree, '@flighthq/xml');
    appendTo(tree, `include/${header}`, '\n#include <flight/types/does_not_exist.hpp>\n');
    const found = danglingIncludes(tree, ['@flighthq/xml'], [path.join(root, 'include')]);
    assert.deepEqual([...found], [['@flighthq/xml', ['flight/types/does_not_exist.hpp']]]);
  });

  test('tree differences and sync touch only what differs', () => {
    const left = copyTree('sync-left');
    const right = copyTree('sync-right');
    appendTo(left, 'README.md', 'x');
    rmSync(path.join(right, 'refusals.json'));
    writeFileSync(path.join(right, 'extra.txt'), 'y');
    assert.deepEqual(treeDifferences(left, right), ['README.md', 'extra.txt', 'refusals.json']);
    assert.deepEqual(syncTree(left, right), ['README.md', 'extra.txt', 'refusals.json']);
    assert.deepEqual(treeDifferences(left, right), []);
  });

  test('a row key new to the package is added to the shared structural member table', () => {
    const committed = copyTree('rowkey-committed');
    const candidate = copyTree('rowkey-candidate');
    const [header] = packageHeaders(candidate, '@flighthq/math');
    appendTo(candidate, `include/${header}`, '\n// flight::RowKey<"definitelyNotAKeyAnywhere">\n');
    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: '@flighthq/math' });
    assert.deepEqual(plan.problems, []);
    const table = plan.writes.get('include/flight/sdk/structural_members.hpp');
    assert.ok(structuralTableKeys(table).has('definitelyNotAKeyAnywhere'));
    assert.ok([...structuralTableKeys(readFileSync(path.join(committed, 'include/flight/sdk/structural_members.hpp'), 'utf8'))].every((key) => structuralTableKeys(table).has(key)));
  });

  test('the structural member table is exactly what the shared renderer computes from the tree', () => {
    const include = path.join(generated, 'include');
    const files = [];
    for (const name of packages) for (const header of packageHeaders(generated, name)) files.push({ contents: readFileSync(path.join(include, header), 'utf8'), path: header });
    // Headers outside any package directory (the repair identity proof) contribute too.
    const extra = path.join(include, 'flight', 'repairs');
    if (existsSync(extra)) for (const name of readdirSync(extra)) files.push({ contents: readFileSync(path.join(extra, name), 'utf8'), path: `flight/repairs/${name}` });
    const table = structuralMemberTable(files, overrideHeaderFiles(root));
    assert.equal(table.contents, readFileSync(path.join(include, 'flight', 'sdk', 'structural_members.hpp'), 'utf8'));
  });

  test('a different compiler or namespace mapping is still refused', () => {
    const committed = copyTree('shared-committed');
    for (const [field, edit] of [
      ['compiler', (value) => (value.compiler.revision = '1'.repeat(40))],
      ['namespaceMapping', (value) => (value.namespaceMapping.desiredRoot = 'other')],
    ]) {
      const candidate = copyTree(`shared-${field}`);
      editJson(candidate, 'manifest.json', edit);
      assert.throws(
        () => planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: '@flighthq/math' }),
        new RegExp(`${field} differs`, 'u'),
      );
    }
  });
});

describe('mixed-pin transition', () => {
  const OLD = manifest.source.revision;
  const NEW = 'a'.repeat(40);
  const target = '@flighthq/math';

  // A single-revision inventory at NEW, as a closure or full run at the next pin would write it.
  function candidateAt(name, revision) {
    const tree = copyTree(name);
    editJson(tree, 'manifest.json', (value) => {
      value.source.revision = revision;
    });
    editJson(tree, 'refusals.json', (value) => {
      value.sourceRevision = revision;
    });
    const readme = path.join(tree, 'README.md');
    writeFileSync(readme, readmeWithProvenance(readFileSync(readme, 'utf8'), JSON.parse(readFileSync(path.join(tree, 'manifest.json'), 'utf8'))));
    return tree;
  }

  test('the first scoped run at a new pin migrates a uniform inventory to per-row provenance', () => {
    const committed = copyTree('migrate-committed');
    const plan = planPackageMerge({ candidateRoot: candidateAt('migrate-candidate', NEW), committedRoot: committed, packageName: target });
    const merged = JSON.parse(plan.writes.get('manifest.json'));
    assert.equal(merged.schema, 'flight-generated-sdk/2');
    assert.deepEqual(merged.source, { package: manifest.source.package, repository: manifest.source.repository, revisions: [OLD, NEW].sort(compareText) });
    for (const row of merged.packages) {
      assert.deepEqual(row.source, { revision: row.package === target ? NEW : OLD, version: manifest.source.version }, row.package);
    }
    assert.doesNotThrow(() => validateProvenance(merged));
    // Rows keep code-unit key order, `source` before `sourceModules`.
    assert.deepEqual(Object.keys(merged.packages[0]), [...Object.keys(merged.packages[0])].sort(compareText));
    const refusals = JSON.parse(plan.writes.get('refusals.json'));
    assert.equal(refusals.schema, 'flight-generated-sdk-refusals/3');
    assert.deepEqual(refusals.sourceRevisions, [OLD, NEW].sort(compareText));
    assert.equal(refusals.sourceRevision, undefined);
    assert.match(plan.writes.get('README.md'), /during a pin transition/u);
    assert.match(plan.writes.get('README.md'), new RegExp(`\`${NEW}\` \\(1 package\\)`, 'u'));
    // math was generated against types, which stays at OLD in the inventory.
    assert.equal(plan.warnings.length, 1);
    assert.match(plan.warnings[0], /@flighthq\/types/u);
  });

  test('the last row to move collapses the inventory back to one revision, byte for byte', () => {
    const committed = copyTree('collapse-committed');
    // Every row at NEW except math, which is still at OLD.
    editJson(committed, 'manifest.json', (value) => {
      value.packages = value.packages.map((row) => ({
        ...row,
        source: { revision: row.package === target ? OLD : NEW, version: value.source.version },
      }));
    });
    const manifestFile = path.join(committed, 'manifest.json');
    const refusalsFile = path.join(committed, 'refusals.json');
    const mixed = JSON.parse(readFileSync(manifestFile, 'utf8'));
    const mixedRefusals = JSON.parse(readFileSync(refusalsFile, 'utf8'));
    finalizeProvenance(mixed, mixedRefusals);
    assert.equal(mixed.schema, 'flight-generated-sdk/2');
    writeFileSync(manifestFile, `${JSON.stringify(mixed, undefined, 2)}\n`);
    writeFileSync(refusalsFile, `${JSON.stringify(mixedRefusals, undefined, 2)}\n`);
    const readme = path.join(committed, 'README.md');
    writeFileSync(readme, readmeWithProvenance(readFileSync(readme, 'utf8'), mixed));

    const candidate = candidateAt('collapse-candidate', NEW);
    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: target });
    for (const file of ['manifest.json', 'refusals.json', 'README.md']) {
      assert.equal(plan.writes.get(file), readFileSync(path.join(candidate, file), 'utf8'), file);
    }
    assert.deepEqual(plan.warnings, []);
  });

  test('a mixed row without complete provenance is refused', () => {
    const committed = copyTree('incomplete-committed');
    applyPackageMerge(
      planPackageMerge({ candidateRoot: candidateAt('incomplete-candidate', NEW), committedRoot: committed, packageName: target }),
      committed,
    );
    editJson(committed, 'manifest.json', (value) => {
      delete value.packages.find((row) => row.package === '@flighthq/xml').source.version;
    });
    assert.throws(
      () => planPackageMerge({ candidateRoot: candidateAt('incomplete-again', NEW), committedRoot: committed, packageName: target }),
      /without complete provenance.*@flighthq\/xml/u,
    );
  });

  test('sdk:check on a mixed inventory reproduces rows at the pin and lists the rest as pending', () => {
    const committed = copyTree('verify-committed');
    const candidate = candidateAt('verify-candidate', NEW);
    applyPackageMerge(planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: target }), committed);
    const verdict = verifyMixedInventory({ candidateRoot: candidate, committedRoot: committed });
    assert.deepEqual(verdict.reproduced, [target]);
    assert.deepEqual(verdict.failed, []);
    assert.equal(verdict.pending.length, packages.length - 1);
    assert.ok(verdict.pending.every((entry) => entry.revision === OLD));

    const [header] = packageHeaders(committed, target);
    appendTo(committed, `include/${header}`, '\n// hand edit\n');
    const tampered = verifyMixedInventory({ candidateRoot: candidate, committedRoot: committed });
    assert.deepEqual(tampered.failed.map((entry) => entry.package), [target]);
  });
});

describe('binding-profile transition', () => {
  const REVISION = manifest.source.revision;
  const NEW = 'a'.repeat(40);
  const OLD_PROFILES = manifest.bindingProfiles;
  // The same profile list with one profile's bindings changed, as editing bindings/headless.json does.
  const NEW_PROFILES = OLD_PROFILES.map((profile, index) => (index === 0 ? { ...profile, digest: `sha256:${'b'.repeat(64)}` } : profile));
  const OLD_SET = bindingProfileSetId(OLD_PROFILES);
  const NEW_SET = bindingProfileSetId(NEW_PROFILES);
  const target = '@flighthq/math';
  const neighbour = '@flighthq/xml';

  // A single-revision, single-profile inventory, as a closure or full run under these inputs writes it.
  function candidateUnder(name, { profiles = NEW_PROFILES, revision = REVISION } = {}) {
    const tree = copyTree(name);
    editJson(tree, 'manifest.json', (value) => {
      value.source.revision = revision;
      value.bindingProfiles = profiles;
    });
    editJson(tree, 'refusals.json', (value) => {
      value.sourceRevision = revision;
    });
    const readme = path.join(tree, 'README.md');
    writeFileSync(readme, readmeWithProvenance(readFileSync(readme, 'utf8'), JSON.parse(readFileSync(path.join(tree, 'manifest.json'), 'utf8'))));
    return tree;
  }

  function moved(committed, candidate, packageName) {
    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName });
    applyPackageMerge(plan, committed);
    return plan;
  }

  test('assumes the fixture has a binding profile to change', () => {
    assert.ok(OLD_PROFILES.length > 0);
    assert.notEqual(OLD_SET, NEW_SET);
  });

  test('a scoped move under changed profiles records them on the moved row only', () => {
    const committed = copyTree('profiles-committed');
    const before = JSON.parse(readFileSync(path.join(committed, 'manifest.json'), 'utf8'));
    const plan = planPackageMerge({ candidateRoot: candidateUnder('profiles-candidate'), committedRoot: committed, packageName: target });
    const merged = JSON.parse(plan.writes.get('manifest.json'));
    assert.doesNotThrow(() => validateProvenance(merged));
    assert.equal(merged.schema, 'flight-generated-sdk/2');
    assert.equal(merged.bindingProfiles, undefined);
    assert.deepEqual(merged.bindingProfileSets, [
      { id: OLD_SET, profiles: OLD_PROFILES },
      { id: NEW_SET, profiles: NEW_PROFILES },
    ].sort((left, right) => compareText(left.id, right.id)));
    // The profile record keeps the position a full run gives bindingProfiles.
    assert.deepEqual(Object.keys(merged).slice(0, 4), ['schema', 'compiler', 'bindingProfileSets', 'namespaceMapping']);
    // One Flight revision throughout: only the profiles are mixed.
    assert.deepEqual(merged.source.revisions, [REVISION]);
    for (const row of merged.packages) {
      assert.equal(row.bindingProfileSet, row.package === target ? NEW_SET : OLD_SET, row.package);
      assert.deepEqual(Object.keys(row), [...Object.keys(row)].sort(compareText), row.package);
    }
    const refusals = JSON.parse(plan.writes.get('refusals.json'));
    assert.deepEqual(refusals.sourceRevisions, [REVISION]);
    const readme = plan.writes.get('README.md');
    assert.match(readme, /Binding profiles differ between packages during a profile transition/u);
    assert.match(readme, new RegExp(`\`${NEW_SET}\` \\(1 package\\)`, 'u'));
    assert.match(readme, new RegExp(`generated from \`${before.source.package}\` ${before.source.version} at\n\`${REVISION}\``, 'u'));
    // math was generated against types, which stays under the old profiles.
    assert.ok(plan.warnings.some((warning) => /other profiles.*@flighthq\/types/u.test(warning)));
  });

  test('untouched rows keep their row, headers and profile record', () => {
    const committed = copyTree('profiles-untouched');
    const before = JSON.parse(readFileSync(path.join(committed, 'manifest.json'), 'utf8'));
    const plan = moved(committed, candidateUnder('profiles-untouched-candidate'), target);
    const prefix = `include/${before.packages.find((row) => row.package === target).cppIncludePrefix}/`;
    const shared = new Set(['manifest.json', 'refusals.json', 'README.md', 'initialization.json', 'best-effort.json', 'cmake/FlightSdkHeaders.cmake', 'BUILD.bazel', 'include/flight/sdk/structural_members.hpp']);
    assert.deepEqual([...plan.writes.keys()].filter((file) => !file.startsWith(prefix) && !shared.has(file)), []);
    const after = JSON.parse(readFileSync(path.join(committed, 'manifest.json'), 'utf8'));
    for (const row of before.packages.filter((candidate) => candidate.package !== target)) {
      const { bindingProfileSet, source, ...rest } = after.packages.find((candidate) => candidate.package === row.package);
      assert.deepEqual(rest, row, row.package);
      assert.equal(bindingProfileSet, OLD_SET, row.package);
      assert.deepEqual(source, { revision: REVISION, version: before.source.version }, row.package);
    }
  });

  test('profiles and revisions mix independently and collapse only when both agree', () => {
    const committed = copyTree('profiles-mixed');
    moved(committed, candidateUnder('profiles-mixed-math', { revision: NEW }), target);
    moved(committed, candidateUnder('profiles-mixed-xml', { profiles: OLD_PROFILES, revision: NEW }), neighbour);
    const merged = JSON.parse(readFileSync(path.join(committed, 'manifest.json'), 'utf8'));
    assert.doesNotThrow(() => validateProvenance(merged));
    assert.deepEqual(merged.source.revisions, [REVISION, NEW].sort(compareText));
    const of = (name) => merged.packages.find((row) => row.package === name);
    assert.deepEqual([of(target).source.revision, of(target).bindingProfileSet], [NEW, NEW_SET]);
    assert.deepEqual([of(neighbour).source.revision, of(neighbour).bindingProfileSet], [NEW, OLD_SET]);
    assert.deepEqual([of('@flighthq/types').source.revision, of('@flighthq/types').bindingProfileSet], [REVISION, OLD_SET]);
    const readme = readFileSync(path.join(committed, 'README.md'), 'utf8');
    assert.match(readme, /during a pin transition/u);
    assert.match(readme, /during a profile transition/u);
  });

  test('the last row to move under the new profiles collapses to the full run, byte for byte', () => {
    const committed = copyTree('profiles-collapse');
    editJson(committed, 'manifest.json', (value) => {
      value.packages = value.packages.map((row) => ({
        ...row,
        source: { bindingProfiles: row.package === target ? OLD_PROFILES : NEW_PROFILES, revision: REVISION, version: value.source.version },
      }));
    });
    const manifestFile = path.join(committed, 'manifest.json');
    const refusalsFile = path.join(committed, 'refusals.json');
    const mixed = JSON.parse(readFileSync(manifestFile, 'utf8'));
    const mixedRefusals = JSON.parse(readFileSync(refusalsFile, 'utf8'));
    finalizeProvenance(mixed, mixedRefusals);
    assert.equal(mixed.schema, 'flight-generated-sdk/2');
    assert.doesNotThrow(() => validateProvenance(mixed));
    writeFileSync(manifestFile, `${JSON.stringify(mixed, undefined, 2)}\n`);
    writeFileSync(refusalsFile, `${JSON.stringify(mixedRefusals, undefined, 2)}\n`);
    const readme = path.join(committed, 'README.md');
    writeFileSync(readme, readmeWithProvenance(readFileSync(readme, 'utf8'), mixed));

    const candidate = candidateUnder('profiles-collapse-candidate');
    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: target });
    for (const file of ['manifest.json', 'refusals.json', 'README.md']) {
      assert.equal(plan.writes.get(file), readFileSync(path.join(candidate, file), 'utf8'), file);
    }
  });

  test('missing or ambiguous profile provenance is refused by the splice', () => {
    const mixedTree = () => {
      const tree = copyTree(`profiles-refuse-${String(Math.random()).slice(2)}`);
      moved(tree, candidateUnder(`${path.basename(tree)}-candidate`), target);
      return tree;
    };
    for (const [why, edit, message] of [
      ['a row names no set', (value) => delete value.packages.find((row) => row.package === neighbour).bindingProfileSet, /without a listed binding-profile set.*@flighthq\/xml/u],
      ['a row names an unlisted set', (value) => (value.packages.find((row) => row.package === neighbour).bindingProfileSet = `sha256:${'c'.repeat(64)}`), /without a listed binding-profile set.*@flighthq\/xml/u],
      ['global and per-row records together', (value) => (value.bindingProfiles = NEW_PROFILES), /ambiguous/u],
      ['a set that is not the digest of its profiles', (value) => (value.bindingProfileSets[0].profiles = []), /not the digest/u],
      ['one set written per row', (value) => {
        value.bindingProfileSets = value.bindingProfileSets.filter((set) => set.id === OLD_SET);
        for (const row of value.packages) row.bindingProfileSet = OLD_SET;
      }, /at least two distinct sets/u],
      ['a uniform manifest naming sets', (value) => {
        value.schema = 'flight-generated-sdk/1';
      }, /./u],
    ]) {
      const committed = mixedTree();
      editJson(committed, 'manifest.json', edit);
      assert.throws(
        () => planPackageMerge({ candidateRoot: candidateUnder(`${path.basename(committed)}-again`), committedRoot: committed, packageName: target }),
        message,
        why,
      );
    }
  });

  test('a global refresh that relabels untouched rows fails verification instead of passing as pending', () => {
    // The full run under the new profiles emits xml differently, as the console binding changed log.
    const candidate = candidateUnder('profiles-verify-candidate');
    const [header] = packageHeaders(candidate, neighbour);
    appendTo(candidate, `include/${header}`, '\n// emitted differently under the new profiles\n');
    const committed = copyTree('profiles-verify-committed');
    moved(committed, candidate, target);

    const honest = verifyMixedInventory({ candidateRoot: candidate, committedRoot: committed });
    assert.deepEqual(honest.reproduced, [target]);
    assert.deepEqual(honest.failed, []);
    assert.equal(honest.pending.length, packages.length - 1);
    assert.ok(honest.pending.every((entry) => entry.bindingProfileSet === OLD_SET && entry.revision === REVISION));

    // Refreshing the recorded digests globally claims every row was generated under the new profiles. The
    // result is a well-formed uniform manifest; only reproduction can tell that it lies.
    const manifestFile = path.join(committed, 'manifest.json');
    const refusalsFile = path.join(committed, 'refusals.json');
    const refreshed = JSON.parse(readFileSync(manifestFile, 'utf8'));
    const refreshedRefusals = JSON.parse(readFileSync(refusalsFile, 'utf8'));
    refreshed.packages = refreshed.packages.map((row) => ({
      ...row,
      source: { bindingProfiles: NEW_PROFILES, revision: REVISION, version: row.source.version },
    }));
    finalizeProvenance(refreshed, refreshedRefusals);
    assert.equal(refreshed.schema, 'flight-generated-sdk/1');
    assert.doesNotThrow(() => validateProvenance(refreshed));
    writeFileSync(manifestFile, `${JSON.stringify(refreshed, undefined, 2)}\n`);
    writeFileSync(refusalsFile, `${JSON.stringify(refreshedRefusals, undefined, 2)}\n`);
    const readme = path.join(committed, 'README.md');
    writeFileSync(readme, readmeWithProvenance(readFileSync(readme, 'utf8'), refreshed));
    const dishonest = verifyMixedInventory({ candidateRoot: candidate, committedRoot: committed });
    assert.deepEqual(dishonest.failed.map((entry) => entry.package), [neighbour]);
    assert.deepEqual(dishonest.pending, []);
  });
});

function copyTree(name) {
  const target = path.join(scratch, name);
  mkdirSync(target, { recursive: true });
  cpSync(generated, target, { recursive: true });
  return target;
}

function appendTo(treeRoot, file, text) {
  const filename = path.join(treeRoot, file);
  writeFileSync(filename, `${readFileSync(filename, 'utf8')}${text}`);
}

function editJson(treeRoot, file, edit) {
  const filename = path.join(treeRoot, file);
  const value = JSON.parse(readFileSync(filename, 'utf8'));
  edit(value);
  writeFileSync(filename, `${JSON.stringify(value, undefined, 2)}\n`);
}

function uniformInventory(committed, target) {
  mkdirSync(target, { recursive: true });
  cpSync(committed, target, { recursive: true });
  const manifestFile = path.join(target, 'manifest.json');
  const refusalsFile = path.join(target, 'refusals.json');
  const value = JSON.parse(readFileSync(manifestFile, 'utf8'));
  if (value.schema !== 'flight-generated-sdk/2') return target;
  // Revision and binding profiles collapse together, onto the provenance most rows were generated from.
  const counts = new Map();
  for (const row of value.packages) {
    const key = JSON.stringify(rowProvenance(value, row));
    counts.set(key, (counts.get(key) ?? 0) + 1);
  }
  const [majority] = [...counts].sort(([l, left], [r, right]) => right - left || compareText(l, r))[0];
  const source = JSON.parse(majority);
  value.packages = value.packages.map(({ bindingProfileSet: _set, ...row }) => ({ ...row, source }));
  const refusals = JSON.parse(readFileSync(refusalsFile, 'utf8'));
  finalizeProvenance(value, refusals);
  writeFileSync(manifestFile, `${JSON.stringify(value, undefined, 2)}\n`);
  writeFileSync(refusalsFile, `${JSON.stringify(refusals, undefined, 2)}\n`);
  const readme = path.join(target, 'README.md');
  writeFileSync(readme, readmeWithProvenance(readFileSync(readme, 'utf8'), value));
  return target;
}
