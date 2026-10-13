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

// A duplicated anonymous parameter shape with no flight::types copy: `owner` declares a function taking it,
// and `caller` constructs it to call that function.
const shape = (namespace, field = 'a') =>
  `#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_${namespace.toUpperCase()}_A_B_0123456789ABCDEF\n` +
  `#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_${namespace.toUpperCase()}_A_B_0123456789ABCDEF\n` +
  'struct a_b_0123456789abcdef : public flight::ReferenceEnabled {\n' +
  `  std::optional<double> ${field};\n  std::optional<double> b;\n};\n` +
  `#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_${namespace.toUpperCase()}_A_B_0123456789ABCDEF\n`;
const takes = 'inline double create_thing(std::optional<flight::Ref<a_b_0123456789abcdef>> config = std::nullopt) {\n  return 0.0;\n}\n';
const calls = 'inline double use_thing() {\n  return flight::owner::create_thing(flight::make_ref<a_b_0123456789abcdef>());\n}\n';
const graph = (entries) =>
  new Map(entries.map(([name, dependencies]) => [name, { dependencies: new Set(dependencies), namespace: `flight::${name}` }]));

test('without a flight::types copy, the copy its declaring dependency owns is canonical', () => {
  const files = [
    { contents: header('owner', `${shape('owner')}\n${takes}`), path: 'flight/owner/thing.hpp' },
    { contents: header('caller', `${shape('caller')}\n${calls}`), path: 'flight/caller/use.hpp' },
  ];
  const result = aliasDuplicateStructuralStructs(files, graph([['owner', []], ['caller', ['owner']]]));
  assert.deepEqual(result.structs, ['a_b_0123456789abcdef']);
  assert.deepEqual(result.files, ['flight/caller/use.hpp']);
  assert.match(files[1].contents, /using a_b_0123456789abcdef = flight::owner::a_b_0123456789abcdef;/u);
  assert.match(files[1].contents, /^#include <flight\/owner\/thing\.hpp>$/mu);
  // The owner's own definition is untouched.
  assert.match(files[0].contents, /struct a_b_0123456789abcdef : public flight::ReferenceEnabled/u);
});

test('dependency canonicalization is not attempted without the package graph', () => {
  const files = [
    { contents: header('owner', `${shape('owner')}\n${takes}`), path: 'flight/owner/thing.hpp' },
    { contents: header('caller', `${shape('caller')}\n${calls}`), path: 'flight/caller/use.hpp' },
  ];
  assert.deepEqual(aliasDuplicateStructuralStructs(files).structs, []);
});

test('two packages declaring the shape as a parameter is ambiguous and rejected', () => {
  const files = [
    { contents: header('owner', `${shape('owner')}\n${takes}`), path: 'flight/owner/thing.hpp' },
    { contents: header('caller', `${shape('caller')}\n${takes}`), path: 'flight/caller/use.hpp' },
  ];
  const result = aliasDuplicateStructuralStructs(files, graph([['owner', ['caller']], ['caller', ['owner']]]));
  assert.deepEqual(result.structs, []);
  assert.match(files[1].contents, /struct a_b_0123456789abcdef : public flight::ReferenceEnabled/u);
});

test('a duplicating package that does not depend on the owner rejects the struct', () => {
  const files = [
    { contents: header('owner', `${shape('owner')}\n${takes}`), path: 'flight/owner/thing.hpp' },
    { contents: header('caller', `${shape('caller')}\n${calls}`), path: 'flight/caller/use.hpp' },
    { contents: header('stray', shape('stray')), path: 'flight/stray/other.hpp' },
  ];
  const result = aliasDuplicateStructuralStructs(files, graph([['owner', []], ['caller', ['owner']], ['stray', []]]));
  assert.deepEqual(result.structs, []);
  assert.equal(result.files.length, 0);
});

test('no owner, or copies that are not byte-identical, are rejected', () => {
  const noOwner = [
    { contents: header('owner', shape('owner')), path: 'flight/owner/thing.hpp' },
    { contents: header('caller', `${shape('caller')}\n${calls}`), path: 'flight/caller/use.hpp' },
  ];
  assert.deepEqual(aliasDuplicateStructuralStructs(noOwner, graph([['owner', []], ['caller', ['owner']]])).structs, []);
  const differing = [
    { contents: header('owner', `${shape('owner')}\n${takes}`), path: 'flight/owner/thing.hpp' },
    { contents: header('caller', `${shape('caller', 'c')}\n${calls}`), path: 'flight/caller/use.hpp' },
  ];
  assert.deepEqual(aliasDuplicateStructuralStructs(differing, graph([['owner', []], ['caller', ['owner']]])).structs, []);
});

test('a struct whose members carry braced default initializers is still indexed and aliased', () => {
  const braced = (namespace) =>
    shape(namespace).replace(
      '  std::optional<double> b;\n',
      '  std::variant<flight::String, flight::Null, flight::Undefined> b = std::variant<flight::String, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};\n',
    );
  const files = [
    { contents: header('owner', `${braced('owner')}\n${takes}`), path: 'flight/owner/thing.hpp' },
    { contents: header('caller', `${braced('caller')}\n${calls}`), path: 'flight/caller/use.hpp' },
  ];
  const result = aliasDuplicateStructuralStructs(files, graph([['owner', []], ['caller', ['owner']]]));
  assert.deepEqual(result.structs, ['a_b_0123456789abcdef']);
  assert.doesNotMatch(files[1].contents, /flight::undefined\}/u);
});
