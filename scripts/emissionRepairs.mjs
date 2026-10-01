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
const KINDS = new Set(['insert-forward-declaration']);
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
      const repaired = insertForwardDeclaration(contents, repair);
      if (repaired === undefined) continue;
      file.contents = repaired;
      applied[index].files.push(file.path);
    }
  }
  return applied;
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
