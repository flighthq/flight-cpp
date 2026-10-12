import assert from 'node:assert/strict';
import { test } from 'node:test';

import { aliasDuplicateStructuralStructs, equivalentStructuralBody } from './emissionRepairs.mjs';

// Run with `npm run pkg:test`.

const struct = (namespace, member) =>
  `#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_${namespace.toUpperCase()}_END_START_B3D703E568B779F1\n` +
  `#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_${namespace.toUpperCase()}_END_START_B3D703E568B779F1\n` +
  'struct end_start_b3d703e568b779f1 : public flight::ReferenceEnabled {\n' +
  `  flight::Ref<${member}> end;\n  flight::Ref<${member}> start;\n};\n` +
  `#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_${namespace.toUpperCase()}_END_START_B3D703E568B779F1\n`;
const header = (namespace, body) =>
  `#pragma once\nstatic_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");\n\nnamespace flight::${namespace} {\n\n${body}\n} // namespace flight::${namespace}\n`;

test('a copy that differs only by flight::types qualification is the same declaration', () => {
  const canonical = 'struct s : public flight::ReferenceEnabled {\n  flight::Ref<Rect> end;\n};\n';
  assert.ok(equivalentStructuralBody('struct s : public flight::ReferenceEnabled {\n  flight::Ref<flight::types::Rect> end;\n};\n', canonical));
  // A bare name in the copy resolves in the copy's own namespace, so it is never equated.
  assert.ok(!equivalentStructuralBody('struct s : public flight::ReferenceEnabled {\n  flight::Ref<Rect> end;\n  flight::Ref<flight::types::Rect> start;\n};\n', canonical));
  assert.ok(!equivalentStructuralBody('struct s : public flight::ReferenceEnabled {\n  flight::Ref<flight::types::Other> end;\n};\n', canonical));
});

test('the qualified copy in another package is aliased to the flight::types definition', () => {
  const files = [
    { contents: header('types', struct('types', 'SwfTagRectangle')), path: 'flight/types/swf_tag_parse_state.hpp' },
    { contents: header('swf', struct('swf', 'flight::types::SwfTagRectangle')), path: 'flight/swf/swf_bounded_definition.hpp' },
  ];
  const result = aliasDuplicateStructuralStructs(files);
  assert.deepEqual(result.structs, ['end_start_b3d703e568b779f1']);
  assert.match(files[1].contents, /using end_start_b3d703e568b779f1 = flight::types::end_start_b3d703e568b779f1;/u);
  assert.match(files[1].contents, /#include <flight\/types\/swf_tag_parse_state\.hpp>/u);
});
