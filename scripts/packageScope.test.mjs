import assert from 'node:assert/strict';
import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { after, describe, test } from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  compareInitializationModules,
  compareText,
  ctestLabelOf,
  mergeSortedRows,
  normalizePackageName,
  packageHeaders,
  parseJobs,
  parsePackageArguments,
  planPackageMerge,
  replaceRowsInPlace,
  replaceSortedRows,
  testTargetOf,
} from './packageScope.mjs';

// Run with `npm run pkg:test`. Needs nothing but the committed generated/ tree.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const generated = path.join(root, 'generated');
const json = (file) => JSON.parse(readFileSync(path.join(generated, file), 'utf8'));
const manifest = json('manifest.json');
const packages = manifest.packages.map((row) => row.package);
const scratch = mkdtempSync(path.join(tmpdir(), 'flight-cpp-package-scope-'));
after(() => rmSync(scratch, { force: true, recursive: true }));

describe('arguments', () => {
  test('exactly one package is required, in any accepted spelling', () => {
    assert.equal(parsePackageArguments(['math']).package, '@flighthq/math');
    assert.equal(parsePackageArguments(['--package=@flighthq/math']).package, '@flighthq/math');
    assert.equal(parsePackageArguments(['--package', 'render-gl']).package, '@flighthq/render-gl');
    assert.throws(() => parsePackageArguments([]), /exactly one package/u);
    assert.throws(() => parsePackageArguments(['math', 'types']), /exactly one package/u);
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
    const [kept, dropped] = packageHeaders(candidate, target);
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

  test('a changed evaluation group set is reported for a full run and left as committed', () => {
    const committed = copyTree('groups-committed');
    const candidate = copyTree('groups-candidate');
    editJson(candidate, 'initialization.json', (value) => {
      value.groups.find((group) => group.modules[0].packageName === '@flighthq/math').cyclic = true;
    });
    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: '@flighthq/math' });
    assert.equal(plan.problems.length, 1);
    assert.match(plan.problems[0], /evaluation groups changed/u);
    assert.ok(!plan.writes.has('initialization.json'));
  });

  test('a row key absent from the shared table is reported, not silently written', () => {
    const committed = copyTree('rowkey-committed');
    const candidate = copyTree('rowkey-candidate');
    const [header] = packageHeaders(candidate, '@flighthq/math');
    appendTo(candidate, `include/${header}`, '\n// flight::RowKey<"definitelyNotAKeyAnywhere">\n');
    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: '@flighthq/math' });
    assert.equal(plan.problems.length, 1);
    assert.match(plan.problems[0], /definitelyNotAKeyAnywhere/u);
  });

  test('a candidate at another pin is refused outright', () => {
    const committed = copyTree('pin-committed');
    const candidate = copyTree('pin-candidate');
    editJson(candidate, 'manifest.json', (value) => {
      value.source.revision = '0'.repeat(40);
    });
    assert.throws(
      () => planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: '@flighthq/math' }),
      /source differs/u,
    );
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
