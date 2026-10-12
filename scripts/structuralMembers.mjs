import { existsSync, lstatSync, readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';

// The structural member table shared by every generated package. Extracted from scripts/sdkGeneration.mjs
// so that scripts/packageScope.mjs renders it with the same code; see structuralMemberTable.

// The override headers that contribute row-key vocabulary, as { contents, sourcePath }.
export function overrideHeaderFiles(root) {
  const overrideDirectory = path.join(root, 'overrides', 'include');
  return existsSync(overrideDirectory)
    ? filesUnder(overrideDirectory)
        .filter((filename) => filename.endsWith('.hpp'))
        .map((sourcePath) => ({ contents: readFileSync(sourcePath, 'utf8'), sourcePath }))
    : [];
}

// Renders flight/sdk/structural_members.hpp from the emitted headers and the override headers. It is a pure
// function of that file set, which is what lets a scoped merge recompute it exactly as a full run does.
// Returns undefined contents when no file uses a structural row key.
export function structuralMemberTable(files, overrideFiles) {
  // These construction-time discriminant/owner fields can be absent from every retained RowKey
  // precisely when the declaration that initializes them was refused. Keep them in the table so
  // an override can materialize the refused construction without a circular dependency on that
  // declaration having emitted successfully first.
  const names = new Set(['dimension', 'sampler']);
  const rowKey = /flight::RowKey<"(?<name>(?:[^"\\]|\\.)*)">/gu;
  // Overrides are part of the installed SDK surface and can contain the only retained use of a
  // RowKey when the generated declaration that used it was refused wholesale. Omitting those keys
  // makes an otherwise complete override fail at row_get even though its native owner declares the
  // member. Include their vocabulary in the same deterministic table; requires-expressions keep a
  // key inert for every owner that does not declare the corresponding member.
  for (const file of [...files, ...overrideFiles]) {
    for (const match of file.contents.matchAll(rowKey)) {
      if (match.groups?.name !== undefined) names.add(JSON.parse(`"${match.groups.name}"`));
    }
  }
  const sortedNames = [...names].sort(compareText);
  const cases = sortedNames
    .map((name) => {
      const member = safeCppMemberName(name);
      return `  else if constexpr (Key::name.view() == std::string_view(${JSON.stringify(name)}) && requires { object.${member}; }) return (object.${member});`;
    });
  const typeCases = sortedNames.map((name) => {
    const member = safeCppMemberName(name);
    return `  else if constexpr (Key::name.view() == std::string_view(${JSON.stringify(name)}) && requires(Object& object) { object.${member}; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().${member})>>{};`;
  });
  const bindings = sortedNames.map((name) => {
    const member = safeCppMemberName(name);
    return `  if constexpr (requires { object->${member}; }) owner.bind_named(${JSON.stringify(name)}, [object]() -> decltype(auto) { return (object->${member}); });`;
  });
  // One line per key for the widening proof. A macro keeps it to one line: the alternative spells
  // the same three checks out 600-odd times and adds a quarter of a megabyte that every translation
  // unit including the SDK would have to parse.
  const wideningKeys = sortedNames.map((name) => `  FLIGHT_SDK_ROW_WIDENS(${safeCppMemberName(name)})`);
  const computedNames = computedCellNames(files, new Set(sortedNames.map(safeCppMemberName)));
  const computedCells = computedNames.map((name) => `  FLIGHT_SDK_ROW_COMPUTED(${name})`);
  if (cases.length === 0) return { computedNames, contents: undefined };
  cases[0] = cases[0].replace('  else if', '  if');
  typeCases[0] = typeCases[0].replace('  else if', '  if');
  return {
    computedNames,
    contents: `// Generated from the structural keys used by the emitted Flight SDK. Do not edit.\n#pragma once\n\n#include <flight/structural_ref.hpp>\n\n#include <concepts>\n#include <cstddef>\n#include <memory>\n#include <string_view>\n#include <type_traits>\n#include <utility>\n\nnamespace flight::detail {\n\ntemplate <typename Key, typename Object>\ndecltype(auto) generated_row_member(Object& object) {\n${cases.join('\n')}\n  else static_assert(dependent_false<Key>, "Flight SDK row key has no compatible generated C++ member");\n}\n\ntemplate <typename Key, typename Object>\nconsteval auto generated_row_member_type_identity() {\n${typeCases.join('\n')}\n  else return std::type_identity<void>{};\n}\n\ntemplate <typename Key, typename Object>\nusing generated_row_member_t = typename decltype(generated_row_member_type_identity<Key, Object>())::type;\n\ntemplate <typename Object>\nvoid bind_generated_row_members(RowOwner& owner, const std::shared_ptr<Object>& object) {\n${bindings.join('\n')}\n}\n\n${wideningPredicate(wideningKeys, computedCells)}\n} // namespace flight::detail\n`,
  };
}

// The subject members the emitted SDK reaches through a `Symbol` rather than through a string row
// key -- `[EntityRuntimeKey]: EntityRuntime | undefined` and its kin.
//
// They are found by the only evidence the emitted output carries: the compiler declares a computed
// key as an `inline const flight::Symbol` constant and names the member it produces after that same
// constant, so a symbol constant whose spelling also appears as a member declaration IS a computed
// cell. Anything spelled like a string row key is excluded -- the two key spaces are separate, and a
// property called `altKey` is a string property that happens to end in the same three letters.
//
// This matters to the widening proof because no `RowKey` ever names a computed cell, so the key
// table below cannot see one. Two subjects that agree on every string key but disagree about a
// computed cell are not interchangeable, and before these lines existed the proof approved them.
function computedCellNames(files, rowKeyMembers) {
  const symbols = new Set();
  const declaration = /\binline const flight::Symbol (?<name>[A-Za-z_][A-Za-z0-9_]*)\s*=/gu;
  for (const file of files) {
    for (const match of file.contents.matchAll(declaration)) {
      if (match.groups?.name !== undefined) symbols.add(match.groups.name);
    }
  }
  const found = [];
  for (const name of [...symbols].sort(compareText)) {
    if (rowKeyMembers.has(name)) continue;
    const member = new RegExp(`^[ \\t]+[A-Za-z_][^;=()]*[ \\t*&]${name};[ \\t]*$`, 'mu');
    if (files.some((file) => member.test(file.contents))) found.push(name);
  }
  return found;
}

// The runtime reaches a computed cell through `row_get`/`row_set`/`row_has` over a `Symbol` only for
// the cells its own `bind_computed_row_cells` knows. The proof below compares every cell the emitted
// SDK declares, known or not, so a disagreement always fails closed -- but a cell the runtime cannot
// reach is still a hole on the READ side, and that is a compiler/runtime contract gap worth naming
// out loud rather than leaving for someone to discover as a null.
export function reportComputedCells(names) {
  const reachable = new Set(['entity_runtime_key']);
  if (names.length === 0) {
    process.stdout.write('Computed cells: none declared by the emitted SDK.\n');
    return;
  }
  process.stdout.write(`Computed cells compared by the widening proof: ${names.join(', ')}\n`);
  const unreachable = names.filter((name) => !reachable.has(name));
  if (unreachable.length > 0) {
    process.stdout.write(
      `Computed cells the runtime's Symbol overloads cannot read: ${unreachable.join(', ')}\n` +
        '  Widening across a disagreement about these is refused, but a symbol read of one returns\n' +
        "  the attachment's answer rather than the member. See docs/compiler-requests/computed-symbol-cell-bindings.md.\n",
    );
  }
}

// The structural assignability proof behind row widening.
//
// `Derived` may be read through `Base`'s row when every key `Base` declares is a key `Derived`
// declares at the same type. Only keys the emitted SDK actually uses as row keys are checked,
// because only those can be asked for: a member no `RowKey` ever names cannot be read through a
// row, so it cannot make a widened read fail.
//
// At least one key must match. Without that a type that declares none of these keys would "prove"
// against anything, which is the unrelated-row conversion this proof exists to reject.
//
// The proof is published as a CONSTRAINED PARTIAL SPECIALIZATION of the runtime's
// `flight::detail::GeneratedRowWidening`, whose primary answers no. That is what keeps the two
// halves of the contract separable: the runtime always declares the trait, this table only ever
// adds the yes cases, and a table older than the contract simply contributes none instead of
// leaving the name undeclared.
function wideningPredicate(keys, computedCells) {
  return [
    '// A computed cell that BOTH subjects declare must be declared at the same type, and unlike a row',
    '// key a disagreement is fatal to the proof rather than merely uncounted. The owner holds one',
    '// cell of one type, so a read through the other row asks for a type the typed lookup cannot',
    '// find and is handed a default instead of the value sitting on the object -- silently.',
    '//',
    '// A cell only ONE subject declares is not a disagreement. Computed cells are declared optional',
    '// in the source, so an object without one is assignable to a row that names it, and both cases',
    '// read honestly: the subject that has the member answers from it, the one that does not answers',
    '// from its attachment.',
    '#define FLIGHT_SDK_ROW_COMPUTED(member)                                                        \\',
    '  if constexpr (requires(Base& base) { base.member; } &&                                       \\',
    '                requires(Derived& derived) { derived.member; }) {                              \\',
    '    if constexpr (!std::same_as<std::remove_cvref_t<decltype(std::declval<Base&>().member)>,   \\',
    '                               std::remove_cvref_t<decltype(std::declval<Derived&>().member)>>) \\',
    '      return false;                                                                            \\',
    '  }',
    '',
    '// Agreement between two declared member types is decided by the runtime, not spelled out here:',
    '// `row_member_widens_v` accepts identical types, and additionally a base key of std::optional<T>',
    '// satisfied by a derived member of plain T -- TypeScript assigns T to T | undefined. The reverse stays',
    '// refused. A bare std::same_as refused `textlayout`\'s row over {auto_size, height, width, word_wrap}',
    '// from a RichTextData subject, which agrees on three keys and differs only in that optionality.',
    '#define FLIGHT_SDK_ROW_WIDENS(member)                                                          \\',
    '  if constexpr (requires(Base& base) { base.member; }) {                                       \\',
    '    if constexpr (!requires(Derived& derived) { derived.member; }) return false;                \\',
    '    else if constexpr (!row_member_widens_v<std::remove_cvref_t<decltype(std::declval<Base&>().member)>, \\',
    '                                     std::remove_cvref_t<decltype(std::declval<Derived&>().member)>>) \\',
    '      return false;                                                                            \\',
    '    else ++matched;                                                                            \\',
    '  }',
    '',
    'template <typename Base, typename Derived>',
    'consteval bool generated_row_widening_matches() {',
    ...(computedCells.length > 0 ? computedCells : ['  // The emitted SDK declares no computed cells.']),
    '  std::size_t matched = 0;',
    ...keys,
    '  return matched > 0;',
    '}',
    '',
    '#undef FLIGHT_SDK_ROW_WIDENS',
    '#undef FLIGHT_SDK_ROW_COMPUTED',
    '',
    '// The only specialization of the runtime trait: yes, for the pairs proven above.',
    'template <typename Base, typename Derived>',
    '  requires(generated_row_widening_matches<Base, Derived>())',
    'struct GeneratedRowWidening<Base, Derived> : std::true_type {};',
    '',
  ].join('\n');
}

function safeCppMemberName(name) {
  const snake = name
    .replace(/([a-z0-9])([A-Z])/gu, '$1_$2')
    .replace(/[^A-Za-z0-9]+/gu, '_')
    .replace(/^_+|_+$/gu, '')
    .toLowerCase();
  return isCppKeyword(snake) ? `${snake}_` : snake;
}

function isCppKeyword(value) {
  return new Set([
  'alignas', 'alignof', 'and', 'and_eq', 'asm', 'atomic_cancel', 'atomic_commit',
  'atomic_noexcept', 'auto', 'bitand', 'bitor', 'bool', 'break', 'case', 'catch',
  'char', 'char8_t', 'char16_t', 'char32_t', 'class', 'compl', 'concept', 'const',
  'consteval', 'constexpr', 'constinit', 'const_cast', 'continue', 'co_await',
  'co_return', 'co_yield', 'decltype', 'default', 'delete', 'do', 'double',
  'dynamic_cast', 'else', 'enum', 'explicit', 'export', 'extern', 'false', 'float',
  'for', 'friend', 'goto', 'if', 'inline', 'int', 'long', 'mutable', 'namespace',
  'new', 'noexcept', 'not', 'not_eq', 'nullptr', 'operator', 'or', 'or_eq',
  'private', 'protected', 'public', 'reflexpr', 'register', 'reinterpret_cast',
  'requires', 'return', 'short', 'signed', 'sizeof', 'static', 'static_assert',
  'static_cast', 'struct', 'switch', 'synchronized', 'template', 'this',
  'thread_local', 'throw', 'true', 'try', 'typedef', 'typeid', 'typename', 'union',
  'unsigned', 'using', 'virtual', 'void', 'volatile', 'wchar_t', 'while', 'xor',
  'xor_eq',
  ]).has(value);
}


function filesUnder(directory) {
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

function compareText(left, right) {
  return left < right ? -1 : left > right ? 1 : 0;
}
