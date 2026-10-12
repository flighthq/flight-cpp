import assert from 'node:assert/strict';
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { test } from 'node:test';

import {
  hashSuffixedIdentifiers,
  loadOverrides,
  unmanifestedOverrideHeaders,
  unresolvedHashReferences,
} from './overrides.mjs';

// The hash-reference check is the one part of override validation that looks at what an override SAYS
// rather than at which file it came from, so it is the part that can be wrong in both directions: a
// missed reference leaves the defect it was written for in place, and a false positive teaches its
// reader to ignore the gate. Both directions are fixtures here.
function fixture(generatedText, overrideText) {
  const root = mkdtempSync(path.join(tmpdir(), 'flight-override-reference-'));
  const modulePath = path.join('flight', 'thing', 'thing.hpp');
  for (const [directory, text] of [
    [path.join(root, 'generated', 'include'), generatedText],
    [path.join(root, 'overrides', 'include'), overrideText],
  ]) {
    const file = path.join(directory, modulePath);
    mkdirSync(path.dirname(file), { recursive: true });
    writeFileSync(file, text);
  }
  return {
    generatedRoot: path.join(root, 'generated'),
    includeDirectory: path.join(root, 'overrides', 'include'),
    root,
  };
}

const OVERRIDE = [{ id: 'fixture', path: 'flight/thing/thing.hpp' }];

test('an override that references a hash the generated tree does not carry is reported', () => {
  const { generatedRoot, includeDirectory, root } = fixture(
    'struct present_0123456789abcdef {\n  int value;\n};\n',
    'using present_0123456789abcdef = flight::types::present_0123456789abcdef;\n' +
      'using absent_fedcba9876543210 = flight::types::absent_fedcba9876543210;\n',
  );
  assert.deepEqual(unresolvedHashReferences(OVERRIDE, includeDirectory, generatedRoot), [
    { id: 'fixture', path: 'flight/thing/thing.hpp', identifier: 'absent_fedcba9876543210' },
  ]);
  rmSync(root, { force: true, recursive: true });
});

test('an override whose hash references all resolve in the generated tree is not reported', () => {
  const { generatedRoot, includeDirectory, root } = fixture(
    'struct present_0123456789abcdef {\n  int value;\n};\n',
    'using present_0123456789abcdef = flight::types::present_0123456789abcdef;\n',
  );
  assert.deepEqual(unresolvedHashReferences(OVERRIDE, includeDirectory, generatedRoot), []);
  rmSync(root, { force: true, recursive: true });
});

test('a mention is enough for a hash to resolve, so a live reference is never flagged', () => {
  // The check asks whether the tree carries the identifier at all, not whether it declares it: asking
  // for declarations would need C++ parsing, and a gate that flags a live reference is worse than the
  // gap it closes. Here the tree only MENTIONS the shape, and the override is still not reported.
  const { generatedRoot, includeDirectory, root } = fixture(
    'inline void use() { (void)sizeof(mentioned_0123456789abcdef); }\n',
    'using mentioned_0123456789abcdef = flight::types::mentioned_0123456789abcdef;\n',
  );
  assert.deepEqual(unresolvedHashReferences(OVERRIDE, includeDirectory, generatedRoot), []);
  rmSync(root, { force: true, recursive: true });
});

test('an override with no hash-suffixed identifiers at all is not reported', () => {
  const { generatedRoot, includeDirectory, root } = fixture(
    'struct Widget {\n  int value;\n};\n',
    'struct Widget {\n  int value;\n};\n',
  );
  assert.deepEqual(unresolvedHashReferences(OVERRIDE, includeDirectory, generatedRoot), []);
  rmSync(root, { force: true, recursive: true });
});

test('hashSuffixedIdentifiers reports each identifier once and ignores everything else', () => {
  const identifiers = hashSuffixedIdentifiers(
    'using a_0123456789abcdef = x; using a_0123456789abcdef = y; using b_fedcba9876543210 = z;\n' +
      'struct ordinary_name {};\nconst char* s = "c_0123456789abcdef";\n',
  );
  assert.deepEqual(identifiers.sort(), ['a_0123456789abcdef', 'b_fedcba9876543210', 'c_0123456789abcdef']);
});

// `status` has three values because two cannot describe a compiling override that deliberately leaves
// surface out. `complete` owes nothing, `incomplete` does not compile and cannot claim to supply
// anything, and `partial` compiles and supplies while still owing something. Both directions are
// fixtures: a partial that is refused would push its author back to claiming `complete`, which is the
// state that made a green header read as a complete module.
function statusFixture(entries) {
  const root = mkdtempSync(path.join(tmpdir(), 'flight-override-status-'));
  const modulePath = path.join('flight', 'thing', 'thing.hpp');
  const file = path.join(root, 'overrides', 'include', modulePath);
  mkdirSync(path.dirname(file), { recursive: true });
  writeFileSync(file, 'inline int thing() { return 1; }\n');
  writeFileSync(
    path.join(root, 'overrides', 'manifest.json'),
    `${JSON.stringify(
      {
        schema: 'flight-cpp-overrides/1',
        overrides: entries.map((entry) => ({
          authoredOn: '2026-10-12',
          change: 'Supplies the refused declaration.',
          derivedFrom: 'a'.repeat(64),
          equivalence: 'Same result for the same inputs.',
          expires: 'when the emitter lowers it.',
          id: 'fixture',
          package: '@flighthq/thing',
          path: modulePath,
          reason: 'The emitter refused the declaration.',
          unblocks: 'The other declarations in the module.',
          whyNotAPatchOrRepair: 'A repair cannot add a body.',
          ...entry,
        })),
      },
      undefined,
      2,
    )}\n`,
  );
  return root;
}

test('a partial override that supplies a module and says what remains is accepted', () => {
  const root = statusFixture([
    { status: 'partial', supplies: 'packages/thing/src/thing.ts', remaining: 'Two accessors are absent.' },
  ]);
  assert.equal(loadOverrides(root).length, 1);
  rmSync(root, { recursive: true, force: true });
});

test('a partial override that claims nothing remains is refused', () => {
  const root = statusFixture([{ status: 'partial', supplies: 'packages/thing/src/thing.ts', remaining: 'None.' }]);
  assert.throws(() => loadOverrides(root), /partial and must say what surface or behavior remains/u);
  rmSync(root, { recursive: true, force: true });
});

test('a partial override with no remaining field at all is refused', () => {
  const root = statusFixture([{ status: 'partial' }]);
  assert.throws(() => loadOverrides(root), /partial and must say what surface or behavior remains/u);
  rmSync(root, { recursive: true, force: true });
});

test('an incomplete override still may not claim to supply a module', () => {
  const root = statusFixture([
    { status: 'incomplete', supplies: 'packages/thing/src/thing.ts', remaining: 'Half written.' },
  ]);
  assert.throws(() => loadOverrides(root), /does not compile cannot be supplying anything/u);
  rmSync(root, { recursive: true, force: true });
});

test('a complete override that supplies a module still owes nothing', () => {
  const root = statusFixture([{ status: 'complete', supplies: 'packages/thing/src/thing.ts', remaining: 'None.' }]);
  assert.equal(loadOverrides(root).length, 1);
  rmSync(root, { recursive: true, force: true });
});

// The unmanifested-header check answers the opposite direction from drift: not "did this declared
// override move" but "is every file that shadows the tree declared at all". An undeclared header still
// wins include precedence, so it is a modification with no drift check and no tier accounting -- which
// is how one sat unnoticed in this tree until a reader found it by hand.
function overrideTreeFixture(files) {
  const root = mkdtempSync(path.join(tmpdir(), 'flight-override-manifest-'));
  for (const [relative, text] of files) {
    const file = path.join(root, 'overrides', 'include', relative);
    mkdirSync(path.dirname(file), { recursive: true });
    writeFileSync(file, text);
  }
  return { includeDirectory: path.join(root, 'overrides', 'include'), root };
}

test('a header the manifest does not name is reported', () => {
  const { includeDirectory, root } = overrideTreeFixture([
    ['flight/thing/thing.hpp', '#pragma once\n'],
    ['flight/thing/extra.hpp', '#pragma once\n'],
  ]);
  assert.deepEqual(unmanifestedOverrideHeaders(OVERRIDE, includeDirectory), ['flight/thing/extra.hpp']);
  rmSync(root, { force: true, recursive: true });
});

test('a manifested header is not reported', () => {
  const { includeDirectory, root } = overrideTreeFixture([['flight/thing/thing.hpp', '#pragma once\n']]);
  assert.deepEqual(unmanifestedOverrideHeaders(OVERRIDE, includeDirectory), []);
  rmSync(root, { force: true, recursive: true });
});

test('non-header files beside the overrides are not reported', () => {
  // The directory may hold a note, a fixture or a data file a reader wants next to an override. Flagging
  // those would teach the reader to ignore the gate, so only headers count.
  const { includeDirectory, root } = overrideTreeFixture([
    ['flight/thing/thing.hpp', '#pragma once\n'],
    ['notes.md', 'scaffolding\n'],
    ['flight/thing/readme.txt', 'prose\n'],
    ['flight/thing/data.json', '{}\n'],
    ['flight/thing/type_map.ts', 'export {};\n'],
  ]);
  assert.deepEqual(unmanifestedOverrideHeaders(OVERRIDE, includeDirectory), []);
  rmSync(root, { force: true, recursive: true });
});

test('every unmanifested header is reported, nested ones included, in a deterministic order', () => {
  // Creation order is deliberately not sorted, and one file is nested: the report has to be the same
  // list every run, because a reader diffs it between runs to see what changed.
  const { includeDirectory, root } = overrideTreeFixture([
    ['flight/zeta/two.hpp', '#pragma once\n'],
    ['flight/alpha/one.hpp', '#pragma once\n'],
    ['flight/alpha/one.h', '#pragma once\n'],
  ]);
  assert.deepEqual(unmanifestedOverrideHeaders(OVERRIDE, includeDirectory), [
    'flight/alpha/one.h',
    'flight/alpha/one.hpp',
    'flight/zeta/two.hpp',
  ]);
  rmSync(root, { force: true, recursive: true });
});

test('a base path with no include directory reports nothing rather than throwing', () => {
  const root = mkdtempSync(path.join(tmpdir(), 'flight-override-empty-'));
  assert.deepEqual(unmanifestedOverrideHeaders(OVERRIDE, path.join(root, 'overrides', 'include')), []);
  rmSync(root, { force: true, recursive: true });
});
