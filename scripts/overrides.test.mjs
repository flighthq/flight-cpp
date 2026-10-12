import assert from 'node:assert/strict';
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { test } from 'node:test';

import { hashSuffixedIdentifiers, unresolvedHashReferences } from './overrides.mjs';

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
