import assert from 'node:assert/strict';
import { test } from 'node:test';

import { spawnSync } from 'node:child_process';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';

import {
  aliasDuplicateStructuralStructs,
  applyEmissionRepairs,
  equivalentStructuralBody,
  loadEmissionRepairs,
  obsoleteRepairs,
} from './emissionRepairs.mjs';

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

// --- declare-refused-function ----------------------------------------------------------------------------------
// A refused source function whose emitted caller survived: the header calls make_thing, refused as makeThing.

const refusedHeader = (marker = 'makeThing', caller = true, defines = false) =>
  `#pragma once\nnamespace flight::pkg {\n\n// NOT GENERATED: function ${marker} -- source line 3\n// refusal: example\n\n` +
  (defines ? 'inline int make_thing(int value) {\n  return value;\n}\n\n' : '') +
  'inline int other() {\n  return 1;\n}\n\n' +
  (caller ? 'inline int use_thing() {\n  return make_thing(2) + 1;\n}\n' : '') +
  '\n} // namespace flight::pkg\n';
const refusedRepair = {
  appliesTo: 'flight/pkg/',
  declaration: 'int make_thing(int value);',
  defect: 'example',
  diagnostic: 'example',
  expectedWhen: 'always',
  expires: 'example',
  id: 'pkg-declare-make-thing',
  kind: 'declare-refused-function',
  refusedFunction: 'makeThing',
  sourceDeclaration: 'example',
  symbol: 'make_thing',
};

test('a refused function its emitted caller still calls is declared before that caller, and only declared', () => {
  const files = [{ contents: refusedHeader(), path: 'flight/pkg/thing.hpp' }];
  const applied = applyEmissionRepairs([refusedRepair], files);
  assert.deepEqual(applied[0].files, ['flight/pkg/thing.hpp']);
  const text = files[0].contents;
  assert.ok(text.indexOf('int make_thing(int value);') < text.indexOf('inline int use_thing()'));
  assert.ok(text.indexOf('inline int other()') < text.indexOf('int make_thing(int value);'));
  assert.doesNotMatch(text, /make_thing\(int value\)\s*\{/u);
  // Idempotent: a second pass finds the declaration and changes nothing.
  assert.deepEqual(applyEmissionRepairs([refusedRepair], files)[0].files, []);
});

test('unrelated refusals, a header with no caller, and a defined symbol are left untouched', () => {
  for (const contents of [refusedHeader('makeOther'), refusedHeader('makeThing', false), refusedHeader('makeThing', true, true)]) {
    const files = [{ contents, path: 'flight/pkg/thing.hpp' }];
    assert.deepEqual(applyEmissionRepairs([refusedRepair], files)[0].files, []);
    assert.equal(files[0].contents, contents);
  }
});

test('a declare-refused-function repair with a body is refused at load', () => {
  const root = mkdtempSync(path.join(tmpdir(), 'flight-repairs-'));
  try {
    const write = (declaration) =>
      writeFileSync(
        path.join(root, 'repairs', 'emission-repairs.json'),
        JSON.stringify({ repairs: [{ ...refusedRepair, declaration }], schema: 'flight-cpp-emission-repairs/1' }),
      );
    spawnSync('mkdir', ['-p', path.join(root, 'repairs')]);
    write('int make_thing(int value);');
    assert.equal(loadEmissionRepairs(root).length, 1);
    write('inline int make_thing(int value) { return value; }');
    assert.throws(() => loadEmissionRepairs(root), /no body/u);
  } finally {
    rmSync(root, { force: true, recursive: true });
  }
});

// With a C++ compiler available (CXX, else c++), prove the claim end to end: the repaired header compiles and
// links while nothing calls the refused function, and a program that does call it fails to LINK -- never compiles
// a silent substitute. Without a compiler the proof is reported as skipped, not passed.
test('the repaired caller compiles and links, and a call of the refused function cannot link', (t) => {
  const cxx = process.env.CXX ?? 'c++';
  if (spawnSync(cxx, ['--version'], { encoding: 'utf8' }).status !== 0) {
    t.skip(`no C++ compiler (${cxx}) to prove compile and link behaviour`);
    return;
  }
  const files = [{ contents: refusedHeader(), path: 'flight/pkg/thing.hpp' }];
  applyEmissionRepairs([refusedRepair], files);
  const dir = mkdtempSync(path.join(tmpdir(), 'flight-refused-'));
  try {
    writeFileSync(path.join(dir, 'thing.hpp'), files[0].contents);
    writeFileSync(path.join(dir, 'unused.cpp'), '#include "thing.hpp"\nint main() { return flight::pkg::other() - 1; }\n');
    writeFileSync(path.join(dir, 'called.cpp'), '#include "thing.hpp"\nint main() { return flight::pkg::use_thing(); }\n');
    const build = (name) => spawnSync(cxx, ['-std=c++20', path.join(dir, `${name}.cpp`), '-o', path.join(dir, name)], { encoding: 'utf8' });
    const unused = build('unused');
    assert.equal(unused.status, 0, unused.stderr);
    const syntax = spawnSync(cxx, ['-std=c++20', '-c', path.join(dir, 'called.cpp'), '-o', path.join(dir, 'called.o')], { encoding: 'utf8' });
    assert.equal(syntax.status, 0, `the caller must compile: ${syntax.stderr}`);
    const called = build('called');
    assert.notEqual(called.status, 0, 'a call of the refused function must not link');
    assert.match(called.stderr + called.stdout, /make_thing/u);
  } finally {
    rmSync(dir, { force: true, recursive: true });
  }
});

// insert-using-declaration must not treat the generated provenance comment as a use of its symbol.
// docs/emission-repair-comment-matching.md
const provenanceHeader = (body) =>
  '// Generated by @flighthq/tool-compiler from @flighthq/x/packages/x/src/renderer.ts#Renderer at abc. Do not edit.\n' +
  '#pragma once\nstatic_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");\n\n' +
  `namespace flight::x {\n\n${body}\n} // namespace flight::x\n`;
const singleUsing = {
  appliesTo: 'flight/x/',
  declaration: 'using flight::types::Renderer;',
  defect: 'd',
  diagnostic: 'd',
  expectedWhen: 'always',
  expires: 'e',
  id: 'single-using',
  include: 'flight/types/renderer.hpp',
  kind: 'insert-using-declaration',
  symbol: 'Renderer',
};
const listUsing = {
  appliesTo: 'flight/x/',
  defect: 'd',
  diagnostic: 'd',
  expectedWhen: 'always',
  expires: 'e',
  id: 'list-using',
  kind: 'insert-using-declaration',
  namespace: 'flight::types',
  symbols: [{ include: 'flight/types/renderer.hpp', name: 'Renderer' }],
};

test('insert-using-declaration ignores a symbol named only by the generated provenance comment', () => {
  for (const repair of [singleUsing, listUsing]) {
    const files = [{ contents: provenanceHeader('inline void f() {}\n'), path: 'flight/x/renderer.hpp' }];
    const before = files[0].contents;
    const applied = applyEmissionRepairs([repair], files);
    assert.deepEqual(applied[0].files, [], repair.id);
    assert.equal(files[0].contents, before, repair.id);
  }
});

test('a repair whose symbol appears only in the provenance comment is reported obsolete on a full run', () => {
  for (const repair of [singleUsing, listUsing]) {
    const applied = applyEmissionRepairs([repair], [{ contents: provenanceHeader('inline void f() {}\n'), path: 'flight/x/renderer.hpp' }]);
    assert.deepEqual(obsoleteRepairs(applied, [], true), [repair.id]);
  }
});

test('insert-using-declaration still fires on an unqualified use in code', () => {
  for (const repair of [singleUsing, listUsing]) {
    const files = [{ contents: provenanceHeader('inline void f(flight::Ref<Renderer> r) { (void)r; }\n'), path: 'flight/x/renderer.hpp' }];
    const applied = applyEmissionRepairs([repair], files);
    assert.deepEqual(applied[0].files, ['flight/x/renderer.hpp'], repair.id);
    assert.match(files[0].contents, /using flight::types::Renderer;/u, repair.id);
    assert.match(files[0].contents, /#include <flight\/types\/renderer\.hpp>/u, repair.id);
  }
});

test('insert-using-declaration is idempotent once it has fired', () => {
  for (const repair of [singleUsing, listUsing]) {
    const files = [{ contents: provenanceHeader('inline void f(flight::Ref<Renderer> r) { (void)r; }\n'), path: 'flight/x/renderer.hpp' }];
    applyEmissionRepairs([repair], files);
    const once = files[0].contents;
    const again = applyEmissionRepairs([repair], files);
    assert.equal(files[0].contents, once, repair.id);
    assert.deepEqual(again[0].files, [], repair.id);
    assert.equal(once.split('using flight::types::Renderer;').length, 2, repair.id);
  }
});

test('a qualified use alone does not fire insert-using-declaration', () => {
  for (const repair of [singleUsing, listUsing]) {
    const files = [{ contents: provenanceHeader('inline void f(flight::Ref<flight::types::Renderer> r) { (void)r; }\n'), path: 'flight/x/renderer.hpp' }];
    assert.deepEqual(applyEmissionRepairs([repair], files)[0].files, [], repair.id);
  }
});

// --- declare-refused-effect-padding-resolvers ---------------------------------------------------------------
// The signature is READ from the retained registration, because the carrier's type names move with the pin.
// This handler once spelled them, and at a627849 that spelling named two types the emitter no longer
// mentions: the declaration stopped matching the std::function it exists to satisfy, and the dead names
// looked used, which made another repair hoist the removed flight/types/render_effect*.hpp includes into
// every header of the family. These cases pin the derivation, not one pin's spelling.

const resolverRegistration = (carrier) =>
  'inline void register_bevel_effect_padding_resolver() {\n' +
  `  std::optional<std::function<${carrier}>>{resolve_bevel_effect_padding};\n` +
  '}\n';

const resolverHeader = (carrier) =>
  '#pragma once\n' +
  'static_assert(flight::runtime_contract.cpp_abi == 1, "x");\n\n' +
  'namespace flight::effects {\n' +
  '// NOT GENERATED: function resolveBevelEffectPadding -- source line 57\n' +
  '// refusal: example\n\n' +
  resolverRegistration(carrier) +
  '\n} // namespace flight::effects\n';

const resolverRepair = (id = 'effects-refused-padding-resolver-declarations') => ({
  appliesTo: 'flight/effects/',
  defect: 'example',
  diagnostic: 'example',
  expectedWhen: 'always',
  expires: 'example',
  id,
  kind: 'declare-refused-effect-padding-resolvers',
  sourceDeclaration: 'example',
});

const CARRIER = 'flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::EffectPadding>>>>(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Effect>>>>)';

test('the refused resolver is declared with the retained registration\'s own carrier, and only declared', () => {
  const files = [{ contents: resolverHeader(CARRIER), path: 'flight/effects/bevel_effect.hpp' }];
  const applied = applyEmissionRepairs([resolverRepair()], files);
  assert.deepEqual(applied[0].files, ['flight/effects/bevel_effect.hpp']);
  const text = files[0].contents;
  // The declaration is the carrier's, spelled back verbatim and given the emitted snake-case name.
  assert.ok(text.includes(`flight::effects {\nflight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::EffectPadding>>>> resolve_bevel_effect_padding(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Effect>>>>);`), text);
  // Its types are forward-declared ahead of the emitter's includes, taken from that same carrier.
  assert.ok(text.includes('namespace flight::types { struct Effect; struct EffectPadding; }\n'), text);
  // A declaration, never a body: the refusal stands and nothing defines the symbol.
  assert.doesNotMatch(text, /resolve_bevel_effect_padding\([^;]*\)\s*\{/u);
  assert.doesNotMatch(text, /(?:inline|static)\s+[^;]*resolve_bevel_effect_padding\([^;]*\)\s*\{/u);
  assert.ok(text.includes('// NOT GENERATED: function resolveBevelEffectPadding'));
  // Idempotent.
  assert.deepEqual(applyEmissionRepairs([resolverRepair()], files)[0].files, []);
});

test('a carrier spelled with the pre-rename owners is declared in those names, not in the current ones', () => {
  const legacy = 'flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RenderEffectPadding>>>>(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RenderEffect>>>>)';
  const files = [{ contents: resolverHeader(legacy), path: 'flight/effects/bevel_effect.hpp' }];
  applyEmissionRepairs([resolverRepair()], files);
  const text = files[0].contents;
  // The point of the regression: no pin's names are baked into the handler.
  assert.ok(text.includes('resolve_bevel_effect_padding(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RenderEffect>>>>);'));
  assert.ok(text.includes('namespace flight::types { struct RenderEffect; struct RenderEffectPadding; }\n'));
  assert.ok(!text.includes('struct Effect; struct EffectPadding;'));
});

test('a header whose registration carries no readable std::function declines, and is left byte-identical', () => {
  for (const contents of [
    resolverHeader(CARRIER).replace(`std::function<${CARRIER}>`, 'ResolverAlias'),
    resolverHeader(CARRIER).replace('{resolve_bevel_effect_padding}', '{some_other_resolver}'),
    `#pragma once\nstatic_assert(flight::runtime_contract.cpp_abi == 1, "x");\nnamespace flight::effects {\n${resolverRegistration(CARRIER)}\n} // namespace flight::effects\n`,
  ]) {
    const files = [{ contents, path: 'flight/effects/bevel_effect.hpp' }];
    assert.deepEqual(applyEmissionRepairs([resolverRepair()], files)[0].files, []);
    assert.equal(files[0].contents, contents);
  }
});
