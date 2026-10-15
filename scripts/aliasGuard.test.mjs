// End-to-end tests for the INCOMING alias guard in the package-merge path.
//
// Target: `reportIncomingAliasMismatches` in scripts/packageScope.mjs, called from `planPackageMerge`
// immediately after `restoreAliasesWithoutCommittedCanonical`. It is REPORT-ONLY: it never writes, and it
// only ever pushes into the plan's `problems` (or `warnings`, for an alias the committed floor already
// cannot satisfy).
//
// Direction (easy to get backwards): the guard fires when a NON-MOVING package's committed header aliases
// a struct whose canonical owner IS the moving package. It re-reads the alias headers named by the
// COMMITTED manifest's `duplicateStructuralStructAliases.files`; for each alias whose target namespace is a
// moving prefix it compares the committed owner body with the merged owner body, where "merged" is read
// from the plan's `writes` when a write exists for that header (moving side only).
//
// Run with:  cd /tmp/aliasguard && node --test aliasGuard.test.mjs
// The runtime under test is the PATCHED worktree at /tmp/ag (the guard does not exist in the plain repo).
// Override with ALIAS_GUARD_SCRIPTS=<dir containing packageScope.mjs> and ALIAS_GUARD_GENERATED=<generated dir>.

import assert from 'node:assert/strict';
import { cpSync, existsSync, lstatSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { after, before, describe, test } from 'node:test';

// Defaults are the test's OWN location, so the file is runnable from any checkout: `scripts/` beside it and
// the repository's generated tree one level up. The env overrides are kept so a mutation run can still point
// the harness at a patched copy, which is what they were for -- but a default that names someone's scratch
// worktree makes the file unrunnable anywhere else, and that is what was sent.
const HERE = path.dirname(fileURLToPath(import.meta.url));
const SCRIPTS = process.env.ALIAS_GUARD_SCRIPTS ?? HERE;
const GENERATED = process.env.ALIAS_GUARD_GENERATED ?? path.join(HERE, '..', 'generated');

const {
  applyPackageMerge,
  compareText,
  finalizeProvenance,
  planPackageMerge,
  planPackageRetirement,
  readmeWithProvenance,
  reportIncomingAliasMismatches,
  rowProvenance,
  sdkBuildMetadata,
} = await import(path.join(SCRIPTS, 'packageScope.mjs'));

const MOVER = '@flighthq/math';
const MOVER_PREFIX = 'flight/math/';
const ALIAS_PACKAGE = '@flighthq/geometry';
const ALIAS_PREFIX = 'flight/geometry/';

const scratch = mkdtempSync(path.join(tmpdir(), 'flight-cpp-incoming-alias-'));
let BASE;

// A synthetic canonical body in exactly the shape the guard's own matcher requires:
// /^struct (\w+) : public flight::ReferenceEnabled \{\n[\s\S]*?^\};$/gmu -- the opening brace ends the
// first line and `};` sits alone at column 0 on the last.
function body(name, member) {
  return `struct ${name} : public flight::ReferenceEnabled {\n  ${member};\n};`;
}

function headerText(name, member) {
  return `#pragma once\n\n${body(name, member)}\n`;
}

// `using X = flight::math::X;` -- matched by the guard's /^using (\w+) = (flight::[a-z0-9_:]+)::\1;$/mu.
function aliasText(name) {
  return `#pragma once\n\nusing ${name} = flight::math::${name};\n`;
}

function put(root, file, text) {
  mkdirSync(path.dirname(path.join(root, file)), { recursive: true });
  writeFileSync(path.join(root, file), text);
}

function appendTo(root, file, text) {
  const filename = path.join(root, file);
  writeFileSync(filename, `${readFileSync(filename, 'utf8')}${text}`);
}

function editManifest(root, edit) {
  const file = path.join(root, 'manifest.json');
  const value = JSON.parse(readFileSync(file, 'utf8'));
  edit(value);
  writeFileSync(file, `${JSON.stringify(value, undefined, 2)}\n`);
}

// A per-test working tree: a private copy of the fixture base that a test may edit freely.
function tree(name) {
  const target = path.join(scratch, name);
  mkdirSync(target, { recursive: true });
  cpSync(BASE, target, { recursive: true });
  return target;
}

// Whether a package's tree defines the named canonical body anywhere under its include prefix.
function definesStruct(root, prefix, name) {
  const pattern = new RegExp(`^struct ${name} : public flight::ReferenceEnabled \\{`, 'mu');
  return includeFilesUnder(root, prefix).some((header) => pattern.test(readFileSync(path.join(root, 'include', header), 'utf8')));
}

// Include-relative paths under `prefix` (default: the whole tree), sorted, symlinks skipped.
function includeFilesUnder(root, prefix = '') {
  const includeRoot = path.join(root, 'include');
  const start = path.join(includeRoot, ...prefix.split('/').filter(Boolean));
  if (!existsSync(start)) return [];
  const walk = (directory) =>
    readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
      const filename = path.join(directory, entry.name);
      if (lstatSync(filename).isSymbolicLink()) return [];
      if (entry.isDirectory()) return walk(filename);
      return [filename];
    });
  return walk(start)
    .map((filename) => path.relative(includeRoot, filename).split(path.sep).join('/'))
    .sort(compareText);
}

// The committed alias files the fixture carries, and the probe headers that hold each canonical body.
const PROBES = {
  changed: { file: `${ALIAS_PREFIX}incoming_changed.hpp`, name: 'incoming_probe_changed', body: 'double alpha' },
  missing: { file: `${ALIAS_PREFIX}incoming_missing.hpp`, name: 'incoming_probe_missing', body: 'double beta' },
  identical: { file: `${ALIAS_PREFIX}incoming_identical.hpp`, name: 'incoming_probe_identical', body: 'double gamma' },
  planned: { file: `${ALIAS_PREFIX}incoming_planned.hpp`, name: 'incoming_probe_planned', body: 'double delta' },
};
const mathHeaderOf = (probe) => `include/flight/math/${probe.file.split('/').pop()}`;

before(() => {
  BASE = path.join(scratch, 'base');
  mkdirSync(BASE, { recursive: true });
  cpSync(GENERATED, BASE, { recursive: true });

  // planPackageMerge refuses a candidate that is not a single-revision generation, and the committed tree
  // is read the same way, so both sides are collapsed onto the revision most rows already hold.
  const manifestFile = path.join(BASE, 'manifest.json');
  const refusalsFile = path.join(BASE, 'refusals.json');
  const manifest = JSON.parse(readFileSync(manifestFile, 'utf8'));
  if (manifest.schema === 'flight-generated-sdk/2') {
    const counts = new Map();
    for (const row of manifest.packages) {
      const key = JSON.stringify(rowProvenance(manifest, row));
      counts.set(key, (counts.get(key) ?? 0) + 1);
    }
    const [majority] = [...counts].sort(([left, leftCount], [right, rightCount]) => rightCount - leftCount || compareText(left, right))[0];
    const source = JSON.parse(majority);
    manifest.packages = manifest.packages.map(({ bindingProfileSet: _set, ...row }) => ({ ...row, source }));
    const refusals = JSON.parse(readFileSync(refusalsFile, 'utf8'));
    finalizeProvenance(manifest, refusals);
    writeFileSync(manifestFile, `${JSON.stringify(manifest, undefined, 2)}\n`);
    writeFileSync(refusalsFile, `${JSON.stringify(refusals, undefined, 2)}\n`);
    writeFileSync(path.join(BASE, 'README.md'), readmeWithProvenance(readFileSync(path.join(BASE, 'README.md'), 'utf8'), manifest));
  }

  // Every non-moving alias file the fixture needs, each named in the committed manifest so the guard's
  // loop reaches it, plus the canonical owner body for each in the moving package's committed headers.
  for (const probe of Object.values(PROBES)) {
    put(BASE, `include/${probe.file}`, aliasText(probe.name));
    put(BASE, mathHeaderOf(probe), headerText(probe.name, probe.body));
  }
  editManifest(BASE, (value) => {
    value.duplicateStructuralStructAliases.files = [
      ...new Set([...value.duplicateStructuralStructAliases.files, ...Object.values(PROBES).map((probe) => probe.file)]),
    ].sort(compareText);
  });

  // The added headers are part of the included file set, so the derived build metadata must already name
  // them; otherwise an unchanged merge would rewrite it and every plan would carry unrelated writes.
  const metadata = sdkBuildMetadata(includeFilesUnder(BASE));
  writeFileSync(path.join(BASE, 'cmake', 'FlightSdkHeaders.cmake'), metadata.cmake);
  writeFileSync(path.join(BASE, 'BUILD.bazel'), metadata.bazel);
});

after(() => rmSync(scratch, { force: true, recursive: true }));

describe('incoming alias guard', () => {
  test('control: the fixture base merges into itself with no problems, warnings or writes', () => {
    const candidate = tree('control');
    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: BASE, packageName: MOVER });
    assert.deepEqual(plan.problems, []);
    assert.deepEqual(plan.warnings, []);
    assert.deepEqual([...plan.writes.keys()], []);
  });

  // CASE 1 -- canonical block CHANGED. The aliasing (non-moving) package's committed alias names a struct
  // whose canonical body the move changes, so the move silently falsifies the alias: it is REPORTED.
  test('1. a changed canonical body of a moving package is reported against the non-moving alias file', () => {
    const candidate = tree('changed');
    put(candidate, mathHeaderOf(PROBES.changed), headerText(PROBES.changed.name, 'double changed'));

    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: BASE, packageName: MOVER });

    assert.deepEqual(plan.problems, [
      'flight/geometry/incoming_changed.hpp (flight/geometry) aliases incoming_probe_changed to flight::math: ' +
        'this move changes the canonical body at include/flight/math/incoming_changed.hpp, ' +
        'so flight/geometry must move or be regenerated before this lands.',
    ]);
    assert.deepEqual(plan.warnings, []);

    // Control 1: the diagnostic names the ALIASING file with ITS OWN package and the canonical OWNER's file
    // path. A version that labelled the non-moving file with the MOVING prefix is the bug this pins.
    const problem = plan.problems[0];
    assert.match(problem, new RegExp(`^${PROBES.changed.file.replace(/[.*+?^${}()|[\]\\]/gu, '\\$&')} \\(flight/geometry\\) `, 'u'));
    assert.ok(problem.includes('include/flight/math/incoming_changed.hpp'));
    assert.ok(!problem.includes('flight/geometry.hpp (flight/math)'));
    // Report-only: the plan never rewrites the non-moving package's header, before or after an apply.
    assert.ok(!plan.writes.has(`include/${PROBES.changed.file}`));
    const aliasBefore = readFileSync(path.join(candidate, `include/${PROBES.changed.file}`), 'utf8');
    applyPackageMerge(plan, candidate);
    assert.equal(readFileSync(path.join(candidate, `include/${PROBES.changed.file}`), 'utf8'), aliasBefore);
    assert.match(readFileSync(path.join(candidate, mathHeaderOf(PROBES.changed)), 'utf8'), /double changed;/u);
  });

  // CASE 2 -- canonical block MISSING from the candidate: the move REMOVES the canonical body the committed
  // alias names. Reported, and the diagnostic names the COMMITTED canonical's file: the moving tree has
  // no body left to point at, so the path to print is the one the alias still names today.
  test('2. removing the canonical body is reported and never rewritten', () => {
    const candidate = tree('missing');
    put(candidate, mathHeaderOf(PROBES.missing), `#pragma once\n\n// the canonical body is gone\n`);

    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: BASE, packageName: MOVER });

    assert.equal(plan.problems.length, 1, plan.problems.join('\n'));
    assert.match(plan.problems[0], /incoming_missing\.hpp \(flight\/geometry\) aliases incoming_probe_missing to flight::math/u);
    // Demands the FILE, not the bare directory: the removal fix exists so the message names a path a
    // reader can open, and a fallback to the namespace would pass a looser assertion while proving less.
    assert.match(plan.problems[0], /this move removes the canonical body at include\/flight\/math\/incoming_missing\.hpp/u);
    // The removed body is nowhere in the merged slice, so the merge has nothing to restore from.
    assert.ok(!plan.writes.has(`include/${PROBES.missing.file}`));
  });

  // CASE 3 -- THE PLANNED-WRITE CONTROL. The candidate tree on disk still emits an OUTGOING alias for the
  // name; the plan's `writes` Map is what carries a canonical body for that owner header, because
  // restoreAliasesWithoutCommittedCanonical rewrites the outgoing alias in place. The guard must read the
  // MOVING side through `writes`; two mutations of the guard are pinned here:
  //   * reading the candidate tree instead of the write sees no body at all -> it says "removes" instead
  //     of "changes" (asserted below);
  //   * reading `writes` on BOTH sides compares the move against itself -> no problem at all.
  test('3. a body delivered only through the plan writes is judged on the moving side', () => {
    const candidate = tree('planned');
    // The candidate's own file holds an outgoing alias, not a body: the move aliases the name out to the
    // non-moving geometry package, whose canonical the committed tree does not have.
    put(
      candidate,
      mathHeaderOf(PROBES.planned),
      `#pragma once\n\nusing ${PROBES.planned.name} = flight::${ALIAS_PREFIX.replace(/^flight\//u, '').replace(/\/$/u, '')}::${PROBES.planned.name};\n`,
    );
    assert.doesNotMatch(readFileSync(path.join(candidate, mathHeaderOf(PROBES.planned)), 'utf8'), /struct incoming_probe_planned/u);
    // The canonical the restored alias is rewritten from lives only in the candidate's geometry tree, and
    // committed geometry does not define it, so the alias is undone rather than kept.
    appendTo(candidate, 'include/flight/geometry/aabb.hpp', `\n${body(PROBES.planned.name, 'int fromGeometry')}\n`);

    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: BASE, packageName: MOVER });

    const written = plan.writes.get(mathHeaderOf(PROBES.planned));
    assert.ok(typeof written === 'string', 'the owner header is written by this merge');
    assert.match(written, /struct incoming_probe_planned : public flight::ReferenceEnabled/u);

    assert.equal(plan.problems.length, 1, plan.problems.join('\n'));
    assert.match(plan.problems[0], /incoming_planned\.hpp \(flight\/geometry\) aliases incoming_probe_planned to flight::math/u);
    assert.match(plan.problems[0], /this move changes the canonical body at include\/flight\/math\/incoming_planned\.hpp/u);
    // Pins "read the write": a candidate-tree read yields no body, and the guard would say "removes".
    assert.doesNotMatch(plan.problems[0], /removes/u);
  });

  // CASE 4 -- IDENTICAL. The committed owner already defines the candidate's body byte-for-byte, so the
  // alias is still true and the guard is silent -- while a real merge is nonetheless happening.
  test('4. a byte-identical canonical body is quiet', () => {
    const candidate = tree('identical');
    // An unrelated real change, so the merge is a merge and not a no-op.
    appendTo(candidate, 'include/flight/math/hash.hpp', '\n// unrelated candidate change\n');
    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: BASE, packageName: MOVER });
    assert.ok(plan.changedHeaders.includes('flight/math/hash.hpp'));

    assert.deepEqual(plan.problems, []);
    assert.deepEqual(plan.warnings, []);
    // The identical probe header is untouched, so no alias line was rewritten and no body restored.
    assert.ok(!plan.writes.has(mathHeaderOf(PROBES.identical)));
    assert.ok(!plan.writes.has(`include/${PROBES.identical.file}`));
  });

  // CASE 5 -- MOVING TOGETHER. A co-moved package's alias is its own direction, so the guard skips it; that
  // skip must depend on the ALIASING package being in the group, not on any package being there.
  test('5. moving the aliasing package together suppresses the report; any other group member does not', () => {
    const candidate = tree('together');
    put(candidate, mathHeaderOf(PROBES.changed), headerText(PROBES.changed.name, 'double changed'));

    const blocked = planPackageMerge({ candidateRoot: candidate, committedRoot: BASE, packageName: MOVER });
    assert.match(blocked.problems.join('\n'), /incoming_probe_changed/u, 'without the group the mismatch is reported');

    const allowed = planPackageMerge({
      candidateRoot: candidate,
      committedRoot: BASE,
      movingTogether: [ALIAS_PACKAGE],
      packageName: MOVER,
    });
    assert.deepEqual(allowed.problems, [], 'the aliasing package moves too, so its alias is its own direction');

    // Populating the group with a package that does NOT own the alias file must not suppress anything:
    // without this, the "allowed" case above would pass for the wrong reason.
    const wrongGroup = planPackageMerge({
      candidateRoot: candidate,
      committedRoot: BASE,
      movingTogether: ['@flighthq/types'],
      packageName: MOVER,
    });
    assert.match(wrongGroup.problems.join('\n'), /incoming_probe_changed/u);
  });

  // CASE 6 -- UNCHANGED LEGACY. The committed tree defines no canonical of that name, so there is nothing
  // this move caused or worsened; the code warns and does NOT push a problem.
  test('6. an alias the committed floor cannot define is a warning, not a problem', () => {
    const legacy = { file: `${ALIAS_PREFIX}incoming_legacy.hpp`, name: 'incoming_probe_legacy' };
    const committed = tree('legacy-committed');
    put(committed, `include/${legacy.file}`, aliasText(legacy.name));
    editManifest(committed, (value) => {
      value.duplicateStructuralStructAliases.files = [...value.duplicateStructuralStructAliases.files, legacy.file].sort(compareText);
    });
    const candidate = tree('legacy-candidate');
    // Committed defines no `incoming_probe_legacy` body anywhere in the moving package; the alias is
    // already unsatisfied, so the guard takes its committed.length === 0 branch.
    assert.ok(!definesStruct(committed, MOVER_PREFIX, legacy.name), 'sanity: the committed floor has no such canonical');

    const plan = planPackageMerge({ candidateRoot: candidate, committedRoot: committed, packageName: MOVER });

    assert.deepEqual(plan.problems, []);
    assert.deepEqual(plan.warnings, [
      `${legacy.file} aliases ${legacy.name} to flight::math, which the committed tree does not define; unchanged by this move.`,
    ]);
  });

  // CASE 7 -- CORRUPT MANIFEST. The guard's own branch is a report, not a silent switch-off: it pushes the
  // manifest path and the parse error and returns immediately.
  test('7a. through the merge, a corrupt committed manifest stops the plan before the guard is reached', () => {
    const committed = tree('corrupt-committed');
    writeFileSync(path.join(committed, 'manifest.json'), '{ this is not json\n');
    const before = readFileSync(path.join(committed, 'manifest.json'), 'utf8');

    assert.throws(
      () => planPackageMerge({ candidateRoot: BASE, committedRoot: committed, packageName: MOVER }),
      (error) => {
        // planPackageMerge parses the committed manifest itself, before it calls the guard, so the failure
        // is a JSON parse error and NOT the guard's own message.
        assert.ok(error instanceof SyntaxError, `expected SyntaxError, got ${error.constructor.name}: ${error.message}`);
        assert.doesNotMatch(error.message, /incoming alias check could not read/u);
        return true;
      },
    );
    // Nothing was applied and nothing was planned: the corrupt tree is untouched.
    assert.equal(readFileSync(path.join(committed, 'manifest.json'), 'utf8'), before);
  });

  test('7b. the guard itself reports an unreadable manifest and short-circuits', () => {
    const committed = tree('corrupt-guard');
    writeFileSync(path.join(committed, 'manifest.json'), '{ this is not json\n');
    const problems = [];
    const warnings = [];

    reportIncomingAliasMismatches({
      candidateRoot: BASE,
      committedRoot: committed,
      group: new Set(),
      packageName: MOVER,
      prefix: MOVER_PREFIX,
      problems,
      warnings,
      writes: new Map(),
    });

    assert.equal(problems.length, 1, problems.join('\n'));
    assert.equal(
      problems[0],
      `the incoming alias check could not read ${committed}/manifest.json: Expected property name or '}' in JSON at position 2 (line 1 column 3)`,
    );
    assert.ok(problems[0].startsWith(`the incoming alias check could not read ${committed}/manifest.json: `));
    // Short-circuits: no alias diagnostic is produced even though the fixture has aliases to judge.
    assert.deepEqual(warnings, []);
  });

  test('7c. a missing manifest is the same failure, naming the path', () => {
    const committed = tree('missing-manifest');
    rmSync(path.join(committed, 'manifest.json'));
    const problems = [];
    reportIncomingAliasMismatches({
      candidateRoot: BASE,
      committedRoot: committed,
      group: new Set(),
      packageName: MOVER,
      prefix: MOVER_PREFIX,
      problems,
      warnings: [],
      writes: new Map(),
    });
    assert.equal(problems.length, 1, problems.join('\n'));
    assert.ok(problems[0].startsWith(`the incoming alias check could not read ${committed}/manifest.json: `));
    assert.match(problems[0], /ENOENT/u);
  });

  // CASE 8 -- RETIREMENT PATH. planPackageRetirement does not call the incoming-alias guard, and it cannot
  // reach this case: its pre-existing dangling-reference check refuses a package any remaining header still
  // names, and an incoming alias header necessarily names the retiree's namespace.
  test('8. the retirement path cannot reach this case: it refuses before any alias judgement', () => {
    const leaf = findRetirableLeaf();
    assert.ok(leaf !== undefined, 'the fixture base has a package nothing uses');

    const committed = tree('retire');
    const relative = leaf.slice('@flighthq/'.length);
    const namespace = relative.replaceAll('-', '_');
    const probeFile = `${ALIAS_PREFIX}incoming_retire_probe.hpp`;
    put(committed, `include/${probeFile}`, `#pragma once\n\nusing retire_probe = flight::${namespace}::retire_probe;\n`);
    editManifest(committed, (value) => {
      value.duplicateStructuralStructAliases.files = [...value.duplicateStructuralStructAliases.files, probeFile].sort(compareText);
    });

    assert.throws(
      () => retire(committed, leaf),
      (error) => {
        assert.match(error.message, /is still referenced, so retiring it would leave dangling uses/u);
        assert.match(error.message, new RegExp(`${probeFile.replace(/[.*+?^${}()|[\]\\]/gu, '\\$&')} \\(names flight::`, 'u'));
        // The refusal is the dangling-reference check, not an incoming-alias diagnostic.
        assert.doesNotMatch(error.message, /must move or be regenerated before this lands/u);
        return true;
      },
    );
  });
});

const NO_DECLARATIONS = { deferred: [], overrides: [], patches: [], repairs: [] };

function sourceCheckout(name, packageNames) {
  const target = path.join(scratch, name);
  for (const packageName of packageNames) {
    const directory = path.join(target, 'packages', packageName.slice('@flighthq/'.length));
    mkdirSync(path.join(directory, 'src'), { recursive: true });
    writeFileSync(path.join(directory, 'package.json'), `${JSON.stringify({ name: packageName })}\n`);
  }
  return target;
}

function retire(committedRoot, packageName) {
  const manifest = JSON.parse(readFileSync(path.join(committedRoot, 'manifest.json'), 'utf8'));
  return planPackageRetirement({
    committedRoot,
    declarations: NO_DECLARATIONS,
    packageName,
    referenceRoots: [],
    sourceRoot: sourceCheckout(
      `source-${String(Math.random()).slice(2)}`,
      manifest.packages.map((row) => row.package).filter((name) => name !== packageName),
    ),
  });
}

function findRetirableLeaf() {
  const manifest = JSON.parse(readFileSync(path.join(BASE, 'manifest.json'), 'utf8'));
  return manifest.packages
    .map((row) => row.package)
    .find((name) => {
      try {
        retire(BASE, name);
        return true;
      } catch {
        return false;
      }
    });
}

// 12. A PLANNED DELETION of the canonical header must be judged as a deletion, not read back off disk.
// The bug this pins: `writes.get(file)` returns null for a deletion, and `null ?? readFileSync(...)` treats
// that null as absent and falls back to the disk copy -- so the check read the very body the move removes and
// found it equal, staying quiet. Presence is `writes.has`, absence is undefined, and null means the header is
// not in the merged result at all. Written against the export so it does not depend on the plan deleting a
// file from disk early.
test('12. a planned deletion of the canonical is reported as a removal, not read back off disk', async () => {
  const { reportIncomingAliasMismatches } = await import(path.join(SCRIPTS, 'packageScope.mjs'));
  const root = mkdtempSync(path.join(tmpdir(), 'alias-guard-deletion-'));
  const write = (relative, text) => {
    mkdirSync(path.dirname(path.join(root, relative)), { recursive: true });
    writeFileSync(path.join(root, relative), text);
  };
  const name = 'x_y_8365950bd60f783f';
  const body = `struct ${name} : public flight::ReferenceEnabled {\n  std::optional<flight::Ref<flight::types::Foo>> f;\n};\n`;
  write('include/flight/math/canon.hpp', body);
  write('include/flight/swf/swf_filter.hpp', `#pragma once\nnamespace flight::swf {\nusing ${name} = flight::math::${name};\n}\n`);
  write('manifest.json', JSON.stringify({ duplicateStructuralStructAliases: { files: ['flight/swf/swf_filter.hpp'] } }));
  const run = (writes) => {
    const problems = [];
    const warnings = [];
    reportIncomingAliasMismatches({ candidateRoot: root, committedRoot: root, group: [], packageName: '@flighthq/math', prefix: 'flight/math/', problems, warnings, writes });
    return problems;
  };
  // The old body is still on disk, so falling back to it would find the bodies equal and report nothing.
  const problems = run(new Map([['include/flight/math/canon.hpp', null]]));
  assert.equal(problems.length, 1, 'a planned deletion must be reported, not compared against the disk copy');
  assert.match(problems[0], /this move removes the canonical body/u);
  assert.deepEqual(run(new Map()), [], 'and nothing is reported when nothing is planned');
});
