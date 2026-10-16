import assert from 'node:assert/strict';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { after, describe, test } from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  EXCLUDED_TREES,
  IDENTITY_SOURCE_PATH,
  applyRename,
  createScope,
  formatReport,
  harvestIdentifiers,
  main,
  newSnakeCase,
  oldSnakeCase,
  parseArguments,
  planRefusal,
  planRename,
  runConverterSelfCheck,
  validateTrees,
} from './renameIdentifiers.mjs';

// Run with `npm run pkg:test`. Every case but the two pinned-checkout ones builds a throwaway tree, so
// nothing here reads or writes the committed repository.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const scratch = mkdtempSync(path.join(tmpdir(), 'flight-cpp-rename-identifiers-'));
after(() => rmSync(scratch, { force: true, recursive: true }));

// A throwaway repository with the given trees. Everything the scan reads lives under it.
function fixture(name, files) {
  const directory = path.join(scratch, name);
  for (const [relative, contents] of Object.entries(files)) {
    const file = path.join(directory, relative);
    mkdirSync(path.dirname(file), { recursive: true });
    writeFileSync(file, contents);
  }
  return directory;
}

const read = (file) => readFileSync(path.join(scratch, file), 'utf8');
// A stub of the pinned Flight source: the harvest reads identifiers from it, so a case that drives the
// command itself has a map to work from. The real checkout is read only by the pins' own cases.
const pinned = (identifiers) => ({ '.dependencies/flight/packages/widget/src/widget.ts': `${identifiers.join('\n')}\n` });
// The tool reports on stdout; a case that exercises the command itself keeps the test output clean.
function quietly(run) {
  const original = process.stdout.write;
  process.stdout.write = () => true;
  try {
    return run();
  } finally {
    process.stdout.write = original;
  }
}

describe('converter port', () => {
  test('the declared chains are checked against the pinned TypeScript, or reported as skipped', () => {
    const check = runConverterSelfCheck(root);
    assert.notEqual(check.status, 'fail', JSON.stringify(check.failures, null, 2));
    if (check.status === 'pass') {
      assert.deepEqual(check.results.map((result) => result.steps), [4, 7]);
      assert.ok(check.examples.every((example) => example.ok));
    } else {
      // A fresh clone carries no pinned checkout; the tool must stay runnable and say so.
      assert.equal(check.status, 'skipped');
      assert.match(check.reason, /flight-compiler/u);
    }
  });

  test('the converter pairs the pin move is specified by are reproduced', () => {
    assert.equal(oldSnakeCase('camera2D'), 'camera2_d');
    assert.equal(newSnakeCase('camera2D'), 'camera_2d');
    assert.equal(oldSnakeCase('texImage2D'), 'tex_image2_d');
    assert.equal(newSnakeCase('texImage2D'), 'tex_image_2d');
    assert.equal(oldSnakeCase('scene3DResource'), 'scene3_dresource');
    assert.equal(newSnakeCase('scene3DResource'), 'scene_3d_resource');
  });
});

describe('whole-identifier matching', () => {
  test('foo2_data and foo2_database are untouched while a genuine mapping in the same file is reported', () => {
    const repo = fixture('whole-identifier', {
      'src/widget.hpp': ['inline double foo2_d() { return 1.0; }', 'double foo2_data = 2.0;', 'double foo2_database = 3.0;', 'double foo2_dx = 4.0;', ''].join('\n'),
    });
    const scope = createScope(['foo2D']);
    assert.equal(scope.lookup('foo2_d').to, 'foo_2d');

    const plan = planRename({ repoRoot: repo, scope, trees: ['src'] });
    assert.equal(plan.contentChanges.length, 1);
    assert.equal(plan.contentChanges[0].occurrences, 1);
    assert.equal(plan.collisions.length, 0);
    assert.match(plan.contentChanges[0].text, /inline double foo_2d\(\)/u);
    // The longer tokens are maximal runs of identifier characters, so no key matches inside them and no
    // substring replacement happens.
    for (const untouched of ['foo2_data', 'foo2_database', 'foo2_dx']) {
      assert.match(plan.contentChanges[0].text, new RegExp(`double ${untouched} = `, 'u'));
    }
  });
});

describe('filename renames', () => {
  test('camera2_d.hpp renames and camera2_d_extra.hpp does not', () => {
    const repo = fixture('filenames', {
      'include/flight/camera2_d.hpp': '#pragma once\n',
      'include/flight/camera2_d_extra.hpp': '#pragma once\n',
      'include/flight/camera2_darray.hpp': '#pragma once\n',
    });
    const scope = createScope(['camera2D', 'camera2DArray']);
    const plan = planRename({ repoRoot: repo, scope, trees: ['include'] });

    const renamed = plan.fileRenames.map((rename) => `${rename.from} -> ${rename.to}`).sort();
    assert.deepEqual(renamed, [
      'flight/camera2_d.hpp -> flight/camera_2d.hpp',
      'flight/camera2_darray.hpp -> flight/camera_2d_array.hpp',
    ]);
    // `camera2_d_extra` is one token, never the key `camera2_d`.
    assert.ok(!plan.fileRenames.some((rename) => rename.from.includes('camera2_d_extra')));
  });
});

describe('collisions', () => {
  test('two different old identifiers reaching one new name refuse the apply', () => {
    // Two source names the old converter spelled differently and the new one spells alike.
    const merged = ['a2DTest', 'a2D_Test'];
    assert.notEqual(oldSnakeCase(merged[0]), oldSnakeCase(merged[1]));
    assert.equal(newSnakeCase(merged[0]), newSnakeCase(merged[1]));

    const contents = 'void a2_dtest();\nvoid a2_d_test();\n';
    const repo = fixture('merge', { ...pinned(merged), 'src/thing.hpp': contents });
    const scope = createScope(merged);
    assert.equal(scope.merges.length, 1);
    assert.equal(scope.merges[0].newForm, 'a_2d_test');

    const plan = planRename({ repoRoot: repo, scope, trees: ['src'] });
    const refusal = planRefusal(plan, scope);
    assert.equal(refusal.length, 2);
    assert.match(refusal[0], /both rename to a_2d_test/u);
    assert.equal(refusal[1], 'content-merge: a2_dtest and a2_d_test both rename to a_2d_test');

    // End to end: the command refuses on the collision rather than letting the last write win.
    const status = quietly(() => main(['--repo', repo, '--tree', 'src', '--apply', '--in-place']));
    assert.equal(status, 1);
    assert.equal(read('merge/src/thing.hpp'), contents);
  });

  test('a filename rename landing on an existing file refuses the apply', () => {
    const repo = fixture('filename-collision', {
      ...pinned(['camera2D']),
      'include/flight/camera2_d.hpp': '// the file that renames\n',
      'include/flight/camera_2d.hpp': '// an unrelated file already at the target name\n',
    });
    const scope = createScope(['camera2D']);
    const plan = planRename({ repoRoot: repo, scope, trees: ['include'] });
    assert.equal(plan.fileRenames.length, 1);
    assert.equal(plan.collisions.length, 1);
    assert.equal(plan.collisions[0].kind, 'filename-exists');

    const status = quietly(() => main(['--repo', repo, '--tree', 'include', '--apply', '--in-place']));
    assert.equal(status, 1);
    assert.ok(existsSync(path.join(repo, 'include/flight/camera2_d.hpp')));
    assert.equal(read('filename-collision/include/flight/camera_2d.hpp'), '// an unrelated file already at the target name\n');
  });

  test('a filename rename whose target is vacated by another rename is not a collision', () => {
    // `camera_2d.hpp` becomes `camera_2d_extra.hpp`, so the first rename may take its place.
    const repo = fixture('filename-chain', {
      'include/flight/camera2_d.hpp': '// a\n',
      'include/flight/camera_2d.hpp': '// b\n',
    });
    const scope = createScope(['camera2D']);
    // `camera_2d.hpp` moves out of the way under the same run, so the first rename may take its name.
    scope.atomic.set('camera_2d', 'camera_2d_extra');
    const plan = planRename({ repoRoot: repo, scope, trees: ['include'] });
    assert.equal(plan.fileRenames.length, 2);
    assert.deepEqual(plan.collisions, []);
  });
});

describe('apply is opt-in and rerunnable', () => {
  test('apply requires --in-place, and an output directory inside the repository is refused', () => {
    const repo = fixture('opt-in', { 'src/a.hpp': 'double camera2_d = 1.0;\n' });
    assert.equal(quietly(() => main(['--repo', repo, '--tree', 'src', '--apply'])), 2);
    assert.equal(quietly(() => main(['--repo', repo, '--tree', 'src', '--out', path.join(repo, 'out')])), 2);
    assert.equal(quietly(() => main(['--repo', repo, '--tree', 'src', '--contents-only'])), 2);
    // None of the three wrote anything.
    assert.equal(read('opt-in/src/a.hpp'), 'double camera2_d = 1.0;\n');
  });

  test('--filenames-only applies the rename without rewriting contents', () => {
    const repo = fixture('filenames-only', {
      'include/flight/camera2_d.hpp': 'double camera2_d = 1.0;\n',
    });
    const scope = createScope(['camera2D']);
    const plan = planRename({ repoRoot: repo, scope, trees: ['include'], contents: false, filenames: true });
    assert.equal(plan.contentChanges.length, 0);
    assert.equal(plan.fileRenames.length, 1);
    applyRename(plan, { contents: false, filenames: true });
    assert.equal(read('filenames-only/include/flight/camera_2d.hpp'), 'double camera2_d = 1.0;\n');
  });

  test('a second apply is a no-op, because the map is keyed on the old spelling', () => {
    const repo = fixture('idempotent', {
      'include/flight/camera2_d.hpp': 'inline double camera2_d();\n',
      'src/widget.hpp': 'void widget(int camera2_d);\n',
    });
    const scope = createScope(['camera2D']);
    const first = planRename({ repoRoot: repo, scope, trees: ['include', 'src'] });
    assert.equal(first.contentChanges.length, 2);
    assert.equal(first.fileRenames.length, 1);
    const applied = applyRename(first);
    assert.deepEqual(applied, { filesWritten: 2, filesRenamed: 1 });
    assert.equal(read('idempotent/include/flight/camera_2d.hpp'), 'inline double camera_2d();\n');
    assert.equal(read('idempotent/src/widget.hpp'), 'void widget(int camera_2d);\n');

    // The old tokens are gone, so the map keyed on them finds nothing: the run is a no-op.
    const second = planRename({ repoRoot: repo, scope, trees: ['include', 'src'] });
    assert.deepEqual(second.contentChanges, []);
    assert.deepEqual(second.fileRenames, []);
    assert.deepEqual(second.collisions, []);
    assert.deepEqual(applyRename(second), { filesWritten: 0, filesRenamed: 0 });
  });

  test('the dry run is byte-identical across runs', () => {
    const repo = fixture('deterministic', {
      'include/flight/camera2_d.hpp': 'inline double camera2_d();\n',
      'include/flight/scene3_dresource.hpp': 'void scene3_dresource();\n',
      'src/widget.hpp': 'void widget(int camera2_d, int scene3_dresource);\n',
    });
    const scope = createScope(['camera2D', 'scene3DResource']);
    const options = { repoRoot: repo, scope, trees: ['include', 'src'] };
    const first = planRename(options);
    const second = planRename(options);
    assert.deepEqual(
      planRename(options).contentChanges.map((change) => [change.relative, change.text]),
      first.contentChanges.map((change) => [change.relative, change.text]),
    );
    assert.deepEqual(second.fileRenames, first.fileRenames);
    const report = formatReport(first, scope);
    assert.equal(formatReport(planRename(options), scope), report);
  });
});

describe('source-patches is out of scope', () => {
  test('a patch tree is never read, reported or written', () => {
    const repo = fixture('out-of-scope', {
      'include/flight/camera2_d.hpp': 'inline double camera2_d();\n',
      'source-patches/camera2D.patch': '--- a/camera2_d.ts\n+++ b/camera2_d.ts\n+const camera2_d = 1;\n',
    });
    const patch = read('out-of-scope/source-patches/camera2D.patch');
    const scope = createScope(['camera2D']);
    const plan = planRename({ repoRoot: repo, scope, trees: ['include'] });
    assert.ok(plan.contentChanges.every((change) => !change.relative.includes('source-patches')));
    assert.ok(plan.fileRenames.every((rename) => !rename.from.includes('source-patches')));

    // Naming it is refused outright rather than silently skipped.
    assert.throws(() => validateTrees(['source-patches']), /excluded/u);
    assert.ok(EXCLUDED_TREES.includes('source-patches'));
    assert.ok(!plan.trees.includes('source-patches'));

    const paths = [...plan.contentChanges, ...plan.fileRenames];
    assert.equal(paths.length, 2);
    applyRename(plan);
    assert.equal(read('out-of-scope/source-patches/camera2D.patch'), patch);
  });
});

describe('compound decomposition', () => {
  test('an ambiguous token resolves to the coarsest decomposition, not the one that invents parts', () => {
    // `clone_texture2_d` re-reads as clone + texture2_d (the emitted name of the union member type) or
    // as clone + texture2 + d (two incidental source bindings). The coarsest reading is the one the
    // emitter produced.
    const scope = createScope(['clone', 'cloneTexture', 'Texture2D', 'texture2', 'd', 'createTexture2D', 'from', 'options']);
    assert.deepEqual(scope.lookup('clone_texture2_d'), {
      kind: 'compound',
      from: 'clone_texture2_d',
      to: 'clone_texture_2d',
      basis: 'coarsest-tiling',
    });
    assert.deepEqual(scope.lookup('create_texture2_d_from_options').to, 'create_texture_2d_from_options');

    // The same at depth: a compound whose parts are an emitted function name and an emitted type name.
    const scope2 = createScope(['framebufferTexture2D', 'framebuffer', 'texture2', 'd']);
    assert.equal(scope2.lookup('framebuffer_texture2_d').to, 'framebuffer_texture_2d');
  });

  test('a tie at the coarsest depth is reported rather than broken by a rule of thumb', () => {
    // Two two-part readings of `x2_d_z` survive at the same depth and disagree: `x2_d` + `z` renames,
    // `x2` + `d_z` does not. Neither is preferred by a rule of thumb, so neither is applied.
    const scope = createScope(['x2D', 'x2', 'd', 'z', 'd_z']);
    const ambiguity = scope.lookup('x2_d_z');
    assert.equal(ambiguity.kind, 'ambiguous');
    assert.deepEqual(ambiguity.candidates, ['x2_d_z', 'x_2d_z']);

    const repo = fixture('tie', { 'src/a.hpp': 'void x2_d_z();\n' });
    const plan = planRename({ repoRoot: repo, scope, trees: ['src'] });
    assert.deepEqual(plan.contentChanges, []);
    assert.equal(plan.ambiguousTokens.length, 1);
    applyRename(plan);
    assert.equal(read('tie/src/a.hpp'), 'void x2_d_z();\n');
  });

  test("an emitted struct's member list settles a split the tilings alone tie on", () => {
    // The emitter builds an anonymous shape's name as `properties.map(snakeCase).join('_')`, and the
    // struct it emits declares one member per property, in order. Where that list is present it states
    // the split the emitter produced: members `x2_d` and `z` rename, while the competing reading
    // `x2` + `d_z` does not.
    const identifiers = ['x2D', 'x2', 'd', 'z', 'd_z'];
    const token = 'x2_d_z_0123456789abcdef';

    const open = createScope(identifiers);
    assert.equal(open.lookup(token).kind, 'ambiguous');

    const confirmed = createScope(identifiers);
    confirmed.noteStructMembers(token, ['x2_d', 'z']);
    assert.deepEqual(confirmed.lookup(token), {
      kind: 'compound',
      from: token,
      to: 'x_2d_z_0123456789abcdef',
      basis: 'emitted-struct-members',
    });
  });

  test("an include guard renames with the struct name it is derived from", () => {
    const identifiers = ['alphaTransform2D', 'alpha', 'transform2D'];
    const scope = createScope(identifiers);
    const name = 'alpha_transform2_d';
    const hash = '0123456789abcdef';
    scope.noteStructMembers(`${name}_${hash}`, ['alpha', 'transform2_d']);

    assert.deepEqual(scope.lookup(`${name}_${hash}`), {
      kind: 'compound',
      from: `${name}_${hash}`,
      to: `alpha_transform_2d_${hash}`,
      basis: 'emitted-struct-members',
    });
    const guard = `FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_PKG_${`${name}_${hash}`.toUpperCase()}`;
    const renamed = scope.lookup(guard);
    assert.equal(renamed.kind, 'compound');
    assert.equal(renamed.basis, 'emitted-struct-guard');
    assert.equal(renamed.to, `FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_PKG_ALPHA_TRANSFORM_2D_${hash.toUpperCase()}`);
  });
});

describe('arguments', () => {
  test('the apply and filename switches are parsed, and an unknown option is refused', () => {
    const parsed = parseArguments(['--repo', '/tmp/x', '--tree', 'generated', '--tree=overrides', '--apply', '--in-place']);
    assert.equal(parsed.repo, '/tmp/x');
    assert.deepEqual(parsed.trees, ['generated', 'overrides']);
    assert.equal(parsed.apply, true);
    assert.equal(parsed.inPlace, true);
    assert.equal(parsed.contents, true);
    assert.equal(parsed.filenames, true);
    assert.equal(parseArguments(['--filenames-only']).contents, false);
    assert.equal(parseArguments(['--contents-only']).filenames, false);
    assert.throws(() => parseArguments(['--nonsense']), /unknown argument/u);
  });
});

describe('the harvest reaches the names the emitter invents', () => {
  // The emitter materializes an open base contract as a union it calls `<Name>Value`, and no Flight token
  // spells that name. The pin move left `material3_dvalue.hpp` unrenamed for exactly this reason, and the
  // next regeneration renamed the file out from under two overrides that included it.
  test('a module name that appears only in an emitted header still reaches the map', () => {
    const tree = fixture('synthesized-module-name', {
      ...pinned(['SurfaceGizmo', 'gizmoWidth']),
      'generated/include/flight/types/surface_gizmo3_dvalue.hpp':
        '// Generated by @flighthq/tool-compiler from @flighthq/types/packages/types/src/SurfaceGizmo3DValue.ts#SurfaceGizmo3DValue at a62784923f3be814463286c9fb28edfe6ab15789. Do not edit.\n#pragma once\n',
    });
    const harvest = harvestIdentifiers(tree);
    assert.equal(harvest.status, 'ok');
    assert.ok(harvest.identifiers.includes('SurfaceGizmo3DValue'), 'the emitted declaration name is harvested');
    assert.equal(harvest.synthesizedCount, 1);
    const scope = createScope(harvest.identifiers);
    // The compound the emitter wrote, which no `_`-split can resolve because `dvalue` is not a part.
    assert.equal(scope.lookup('surface_gizmo3_dvalue').to, 'surface_gizmo_3d_value');
  });

  test('a tree with no emitted inventory harvests from the sources alone', () => {
    const tree = fixture('no-emitted-inventory', pinned(['Widget3D']));
    const harvest = harvestIdentifiers(tree);
    assert.equal(harvest.status, 'ok');
    assert.equal(harvest.synthesizedCount, 0);
    assert.equal(createScope(harvest.identifiers).atomic.get('widget3_d'), 'widget_3d');
  });
});

describe('candidate resolution on the pinned checkout', () => {
  test('the identifiers the pin move had to resolve are all resolved', () => {
    const harvest = harvestIdentifiers(root);
    if (harvest.status === 'skipped') return; // no pinned checkout: nothing to resolve against.
    const scope = createScope(harvest.identifiers);
    for (const [token, expected] of [
      ['clone_texture2_d', 'clone_texture_2d'],
      ['create_texture2_d_from_options', 'create_texture_2d_from_options'],
      ['framebuffer_texture2_d', 'framebuffer_texture_2d'],
    ]) {
      assert.equal(scope.lookup(token).to, expected, token);
    }
    // A key of the form `<x>2_d` is one token: `texture2_d` is never touched by `texture2_d`'s parts.
    assert.equal(scope.lookup('texture2_d').kind, 'atomic');
    // A synthesized union name has no `_`-split, so it is only reachable through the emitted inventory.
    assert.equal(scope.lookup('material3_dvalue').to, 'material_3d_value');
  });

  // The tool is documented to be re-runnable over a branch that already carries the rename, and that is
  // only true if the self-check reads the OLD converter from its own revision: after the pin move the
  // checkout's working tree holds the NEW one, and reading it as "old" threw the check out of the tool.
  test('the check still passes once the checkout working tree carries the new converter', () => {
    const checkout = path.join(root, '.dependencies', 'flight-compiler');
    if (!existsSync(path.join(checkout, '.git'))) return; // no pinned checkout: nothing to prove here.
    const working = readFileSync(path.join(checkout, IDENTITY_SOURCE_PATH), 'utf8');
    if (working.includes('function snakeCase(')) return; // still at the old pin: the other case covers it.
    const check = runConverterSelfCheck(root);
    assert.equal(check.status, 'pass', JSON.stringify(check.failures, null, 2));
  });
});
