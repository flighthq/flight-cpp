import { readFileSync } from 'node:fs';
import path from 'node:path';

// Mechanical repairs applied to compiler output, so a package can ship while an emitter defect is
// still open upstream. This is deliberately the narrowest mechanism that does the job: a repair
// inserts a C++ declaration into generated text and nothing else. It cannot change behavior, because
// a forward declaration has none.
//
// The reason the mechanism is safe to have at all is the expiry rule. `applyEmissionRepairs` reports
// which repairs matched nothing, and `sdk:check` treats a repair that matches nothing as a failure.
// A repair therefore cannot quietly outlive the bug it answers, which is the failure mode that turns
// a patched build into a silent fork of the generator.

const SCHEMA = 'flight-cpp-emission-repairs/1';
const KINDS = new Set(['insert-forward-declaration', 'insert-using-declaration']);
// When a repair is expected to match. A module that only emits once external bindings are applied
// cannot need its repair in the unbound inventory, so claiming the repair is obsolete there would be
// wrong -- `withBindings` says to judge expiry only on a profiled run.
const EXPECTATIONS = new Set(['always', 'withBindings']);

export function loadEmissionRepairs(root) {
  const file = path.join(root, 'repairs', 'emission-repairs.json');
  let parsed;
  try {
    parsed = JSON.parse(readFileSync(file, 'utf8'));
  } catch (error) {
    if (error.code === 'ENOENT') return [];
    throw new Error(`Emission repair declaration ${portable(file)} is not readable JSON: ${error.message}`);
  }
  if (parsed.schema !== SCHEMA) {
    throw new Error(`Emission repair declaration ${portable(file)} has schema ${parsed.schema}, expected ${SCHEMA}`);
  }
  if (!Array.isArray(parsed.repairs)) {
    throw new Error(`Emission repair declaration ${portable(file)} has no repairs array`);
  }
  for (const repair of parsed.repairs) {
    for (const field of ['id', 'kind', 'appliesTo', 'symbol', 'declaration', 'diagnostic', 'defect', 'expires']) {
      if (typeof repair[field] !== 'string' || repair[field].length === 0) {
        throw new Error(`Emission repair ${repair.id ?? '<unnamed>'} is missing required field ${field}`);
      }
    }
    if (!KINDS.has(repair.kind)) {
      throw new Error(`Emission repair ${repair.id} has unknown kind ${repair.kind}`);
    }
    repair.expectedWhen ??= 'always';
    if (!EXPECTATIONS.has(repair.expectedWhen)) {
      throw new Error(`Emission repair ${repair.id} has unknown expectedWhen ${repair.expectedWhen}`);
    }
  }
  return parsed.repairs;
}

// Rewrites `files` in place where a repair applies. Returns one record per repair naming the files it
// touched, so the caller can record them in the manifest and fail when a repair matched nothing.
export function applyEmissionRepairs(repairs, files) {
  const applied = repairs.map((repair) => ({ expectedWhen: repair.expectedWhen, files: [], id: repair.id }));
  if (repairs.length === 0) return applied;
  for (const file of files) {
    for (const [index, repair] of repairs.entries()) {
      if (!file.path.startsWith(repair.appliesTo)) continue;
      const contents = typeof file.contents === 'string' ? file.contents : String(file.contents);
      const repaired =
        repair.kind === 'insert-using-declaration'
          ? insertUsingDeclaration(contents, repair)
          : insertForwardDeclaration(contents, repair);
      if (repaired === undefined) continue;
      file.contents = repaired;
      applied[index].files.push(file.path);
    }
  }
  return applied;
}

// Brings a name from flight::types into the package's own namespace, for a file that refers to it
// UNQUALIFIED. `flight/log/log.hpp` says `LogSink` inside `namespace flight::log`, where the type is
// `flight::types::LogSink`; gcc even names the fix in its diagnostic. A forward declaration cannot help
// here, because these are type ALIASES and an alias has no forward declaration, and an include cannot
// help either, because the name is already visible under a different qualification. A using-declaration
// introduces the name and nothing else, so it stays inside the rule that a repair adds no behavior.
function insertUsingDeclaration(contents, repair) {
  const { symbol } = repair;
  // Unqualified use only. A file that always writes types::X is already correct.
  if (!new RegExp(`(?<!types::)\\b${symbol}\\b`, 'u').test(contents)) return undefined;
  if (contents.includes(repair.declaration)) return undefined;
  // The declaration needs the name to EXIST, and the emitter does not always include the header that
  // defines it -- flight/log/log.hpp names four flight::types aliases and includes none of them, so a
  // bare using-declaration fails with "'flight::types' has not been declared". A repair that introduces
  // a name has to bring its definition with it.
  const withInclude =
    repair.include === undefined || contents.includes(`#include <${repair.include}>`)
      ? contents
      : hoistInclude(contents, repair.include);
  if (withInclude === undefined) return undefined;
  // Inside the package namespace, which is where the unqualified name is looked up.
  const anchor = /^namespace flight::[a-z0-9_]+ \{\n/mu.exec(withInclude);
  if (anchor === null) return undefined;
  const at = anchor.index + anchor[0].length;
  return `${withInclude.slice(0, at)}\n${repair.declaration}\n${withInclude.slice(at)}`;
}

// At file scope, on the emitter's own boundary between the include prologue and the declarations. It
// cannot go inside the namespace block: an include parsed in there resolves every name it declares as
// flight::<package>::flight::…, which is the mistake that cost three iterations on the struct alias.
function hoistInclude(contents, header) {
  const anchor = /^static_assert\(flight::runtime_contract\.cpp_abi == \d+,[^\n]*\n/mu.exec(contents);
  if (anchor === null) return undefined;
  const at = anchor.index + anchor[0].length;
  return `${contents.slice(0, at)}\n#include <${header}>\n${contents.slice(at)}`;
}

// Returns the repaired text, or undefined when this file needs no repair. A file needs the
// declaration when it names the symbol as a qualified member of the namespace but neither declares
// nor defines it. The file that actually defines the symbol is left alone, as is one that already
// carries its own forward declaration.
function insertForwardDeclaration(contents, repair) {
  const { symbol } = repair;
  // The emitter writes the name both qualified and bare -- a header inside namespace flight::types
  // says `Ref<Node<Any>>`, a sibling says `flight::types::Node<...>` -- so the reference test is
  // unqualified. Word boundaries keep it off longer names: \bNode\b does not match Node2D or NodeData.
  const names = new RegExp(`\\b${symbol}\\b`).test(contents);
  if (!names) return undefined;
  const declares = new RegExp(`\\bstruct ${symbol}\\s*[;:{]`).test(contents);
  if (declares) return undefined;
  if (contents.includes(repair.declaration)) return undefined;
  // The ABI assertion block is the emitter's own boundary between the include prologue and the
  // declarations, which is exactly where its forward declarations already go.
  const anchor = /^static_assert\(flight::runtime_contract\.cpp_abi == \d+,[^\n]*\n/m.exec(contents);
  if (anchor === null) return undefined;
  const at = anchor.index + anchor[0].length;
  return `${contents.slice(0, at)}\n${repair.declaration}\n${contents.slice(at)}`;
}

// The expiry check. A repair that matched nothing is either fixed upstream or no longer reachable;
// either way carrying it is how a patched build drifts into a fork.
export function obsoleteRepairs(applied, profiles) {
  const bound = profiles.length > 0;
  return applied
    .filter((record) => record.files.length === 0)
    .filter((record) => record.expectedWhen !== 'withBindings' || bound)
    .map((record) => record.id);
}

function portable(filename) {
  return filename.split(path.sep).join('/');
}

// ---------------------------------------------------------------------------------------------------
// The duplicate structural struct repair.
//
// An anonymous object shape in the source -- `{ x: number; y: number }` -- is emitted as a struct named
// by its members and a structural hash, and it is emitted ONCE PER PACKAGE that mentions it, each behind
// its own include guard. `x_y_8365950bd60f783f` is defined five times at the current pin, under
// flight::collision, flight::mesh, flight::screen, flight::shape and flight::types, with byte-identical
// bodies. C++ makes those five unrelated types, so a value produced by one package and consumed by
// another does not convert, and the emitted header does not compile:
//
//   no match for call to '(const std::function<shared_ptr<flight::types::x_y_8365950bd60f783f>(…)>)
//                        (flight::Ref<flight::screen::x_y_8365950bd60f783f>&)'
//
// This repair makes the duplicates name one type: the definition in flight::types is kept and each
// other package's copy becomes an alias to it. An alias introduces no operation and no storage -- it
// makes two spellings denote the same type -- so it stays inside the rule that a repair may only add
// text with no behavior of its own.
//
// The precondition is strict and checked, not assumed: the bodies must be BYTE-IDENTICAL. Two shapes
// that merely hash alike would be a different claim entirely, and this repair does not make it.
//
// Only `flight::types` is used as the canonical home, because it is the package every other one already
// depends on; aliasing toward any other package could introduce an include it does not have.

const STRUCT_DEFINITION = /^(#ifndef (FLIGHT_COMPILER_ANONYMOUS__[A-Z0-9_]+)\n#define \2\n)(struct ([a-z0-9_]+_[0-9a-f]{16}) : public flight::ReferenceEnabled \{\n(?:[^}]*?)\n\};\n)(#endif \/\/ \2\n)/gmu;

// Indexes every anonymous structural struct definition by name, recording the body text per package.
function indexStructuralDefinitions(files) {
  const index = new Map();
  for (const file of files) {
    const package_ = packageOf(file.path);
    if (package_ === undefined) continue;
    const contents = typeof file.contents === 'string' ? file.contents : String(file.contents);
    for (const match of contents.matchAll(STRUCT_DEFINITION)) {
      const [, , , body, name] = match;
      if (!index.has(name)) index.set(name, new Map());
      // The defining file is recorded too: an alias is only usable if the header that defines the
      // canonical struct is included, and only this index knows which header that is.
      index.get(name).set(package_, { body, path: file.path });
    }
  }
  return index;
}

// Rewrites each non-canonical copy of a duplicated struct into an alias to the flight::types one.
// Returns the files touched, for the manifest and for the expiry report.
export function aliasDuplicateStructuralStructs(files) {
  const index = indexStructuralDefinitions(files);
  const canonical = new Map();
  for (const [name, byPackage] of index) {
    if (byPackage.size < 2) continue;
    const types = byPackage.get('types');
    if (types === undefined) continue;
    // Every other copy must be byte-identical to the canonical one, or this struct is left alone
    // entirely -- a partial alias would be worse than none.
    if (![...byPackage].every(([, copy]) => copy.body === types.body)) continue;
    canonical.set(name, types);
  }
  if (canonical.size === 0) return { files: [], structs: [] };
  const touched = [];
  const aliased = new Set();
  for (const file of files) {
    const package_ = packageOf(file.path);
    if (package_ === undefined || package_ === 'types') continue;
    const contents = typeof file.contents === 'string' ? file.contents : String(file.contents);
    let changed = false;
    const needed = new Set();
    const repaired = contents.replaceAll(STRUCT_DEFINITION, (whole, open, _guard, body, name, close) => {
      const target = canonical.get(name);
      if (target === undefined || target.body !== body) return whole;
      changed = true;
      aliased.add(name);
      // The include CANNOT go here. These guard blocks sit inside the package's `namespace flight::x`,
      // and an include placed in one is parsed inside that namespace -- which makes every name in the
      // included header resolve as flight::x::flight::… and nothing compiles. The include is collected
      // and hoisted to file scope below; only the alias stays here.
      needed.add(target.path);
      return `${open}using ${name} = flight::types::${name};\n${close}`;
    });
    if (!changed) continue;
    const hoisted = hoistIncludes(repaired, needed);
    if (hoisted === undefined) continue;
    file.contents = hoisted;
    touched.push(file.path);
  }
  return { files: touched, structs: [...aliased].sort() };
}

// Puts the canonical headers at file scope, on the emitter's own boundary between the include prologue
// and the declarations. Returns undefined when that boundary is not found, so a file whose shape this
// repair does not recognize is left exactly as emitted.
function hoistIncludes(contents, needed) {
  if (needed.size === 0) return contents;
  const anchor = /^static_assert\(flight::runtime_contract\.cpp_abi == \d+,[^\n]*\n/mu.exec(contents);
  if (anchor === null) return undefined;
  const absent = [...needed].filter((header) => !contents.includes(`#include <${header}>`)).sort();
  if (absent.length === 0) return contents;
  const at = anchor.index + anchor[0].length;
  const block = `\n${absent.map((header) => `#include <${header}>`).join('\n')}\n`;
  return `${contents.slice(0, at)}${block}${contents.slice(at)}`;
}

function packageOf(filePath) {
  const parts = filePath.split('/');
  return parts.length >= 3 && parts[0] === 'flight' ? parts[1] : undefined;
}
