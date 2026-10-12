import { createHash } from 'node:crypto';
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import path from 'node:path';

// Hand-written headers that shadow the generated tree.
//
// The whole mechanism is an include path: `overrides/include` goes before the generated include
// directory, so the generated file is never edited and the set of files under overrides/include IS the
// modification. Nothing has to be computed to answer "what did we change", which is the property a
// copy-and-edit workflow loses.
//
// What a copy does cost is drift. The generated file an override was derived from will change when the
// Flight or compiler pin moves, and an override written against the old shape can then be silently
// wrong -- it still compiles, it just no longer reflects what it replaced. So each entry records
// `derivedFrom`, the sha256 of the generated file at the time it was written, and `driftedOverrides`
// reports every entry whose source no longer hashes to it.

const SCHEMA = 'flight-cpp-overrides/1';
const REQUIRED = [
  'id',
  'path',
  'package',
  'derivedFrom',
  'reason',
  'change',
  'equivalence',
  'whyNotAPatchOrRepair',
  'unblocks',
  'expires',
  'authoredOn',
  'status',
];
// `complete` compiles and owes nothing; `partial` compiles and supplies a module but deliberately
// leaves a declared surface or behavior gap (so `remaining` must say what); `incomplete` means the
// override is not a usable supply at all and may not name `supplies`.
const STATUSES = new Set(['complete', 'incomplete', 'partial']);

export function overridesIncludeDirectory(root) {
  return path.join(root, 'overrides', 'include');
}

export function loadOverrides(root) {
  const file = path.join(root, 'overrides', 'manifest.json');
  let parsed;
  try {
    parsed = JSON.parse(readFileSync(file, 'utf8'));
  } catch (error) {
    if (error.code === 'ENOENT') return [];
    throw new Error(`Override manifest ${portable(file)} is not readable JSON: ${error.message}`);
  }
  if (parsed.schema !== SCHEMA) {
    throw new Error(`Override manifest ${portable(file)} has schema ${parsed.schema}, expected ${SCHEMA}`);
  }
  if (!Array.isArray(parsed.overrides)) throw new Error(`Override manifest ${portable(file)} has no overrides array`);
  for (const entry of parsed.overrides) {
    for (const field of REQUIRED) {
      if (typeof entry[field] !== 'string' || entry[field].length === 0) {
        throw new Error(`Override ${entry.id ?? '<unnamed>'} is missing required field ${field}`);
      }
    }
    if (!STATUSES.has(entry.status)) {
      throw new Error(`Override ${entry.id} has unknown status ${entry.status}`);
    }
    // `supplies` names the refused module this override provides a FAITHFUL implementation of, by its
    // source path in the pinned checkout. It is a stronger claim than `status`, and separate from it on
    // purpose: `status: complete` only means the override compiles, which a file full of functions that
    // throw also does. flight/log/log.hpp is exactly that -- it compiles, and nine of its functions
    // throw -- so it must never carry `supplies`, or the tier gate would call @flighthq/log shippable.
    //
    // Only an override that actually implements the module gets it, and only then does the tier gate
    // count the package's refusal as answered.
    if (entry.supplies !== undefined) {
      if (typeof entry.supplies !== 'string' || entry.supplies.length === 0) {
        throw new Error(`Override ${entry.id} has a non-string supplies path`);
      }
      if (entry.status === 'incomplete') {
        throw new Error(
          `Override ${entry.id} claims to supply ${entry.supplies} but its status is incomplete. ` +
            'An override that does not compile cannot be supplying anything.',
        );
      }
      if (entry.status === 'complete' &&
          (entry.remaining === undefined || !/^None\b/iu.test(entry.remaining.trim()))) {
        throw new Error(
          `Override ${entry.id} claims to faithfully supply ${entry.supplies} but does not explicitly ` +
            `record that no behavior remains (remaining: ${entry.remaining ?? '<absent>'})`,
        );
      }
      // A `partial` supply is the middle case the other two cannot express: the override compiles and
      // implements the module, but declared surface or behavior is deliberately not there yet. It must
      // SAY what is missing -- an unqualified supply claim is exactly the thing being prevented.
      if (entry.status === 'partial' &&
          (entry.remaining === undefined || entry.remaining.trim().length === 0 ||
           /^None\b/iu.test(entry.remaining.trim()))) {
        throw new Error(
          `Override ${entry.id} is partial and must say what surface or behavior remains ` +
            `(remaining: ${entry.remaining ?? '<absent>'})`,
        );
      }
    }
    if (entry.status === 'partial' &&
        (typeof entry.remaining !== 'string' || entry.remaining.trim().length === 0 ||
         /^None\b/iu.test(entry.remaining.trim()))) {
      throw new Error(`Override ${entry.id} is partial and must say what surface or behavior remains`);
    }
    if (entry.status === 'incomplete' && (typeof entry.remaining !== 'string' || entry.remaining.length === 0)) {
      throw new Error(`Override ${entry.id} is incomplete and must say what remains`);
    }
    const file_ = path.join(overridesIncludeDirectory(root), entry.path);
    if (!existsSync(file_)) {
      throw new Error(`Override ${entry.id} declares ${entry.path}, which is absent from overrides/include`);
    }
    // A fail-loud host boundary is preferable to fabricating a successful native result, but it is
    // still missing runtime behavior. Keep that distinction machine-readable: otherwise a compiling
    // override whose body says the operation is unavailable can silently promote its package to the
    // shippable tier.
    const contents = readFileSync(file_, 'utf8');
    if (/unavailable in this profile/iu.test(contents) &&
        (entry.remaining === undefined || /^None\b/iu.test(entry.remaining.trim()))) {
      throw new Error(
        `Override ${entry.id} contains an unavailable native profile boundary and must say what remains`,
      );
    }
  }
  return parsed.overrides;
}

// Every override whose generated source no longer hashes to what it was derived from, plus every one
// whose generated source has disappeared. Either way the override needs re-deriving or dropping.
export function driftedOverrides(overrides, generatedRoot) {
  const drifted = [];
  for (const entry of overrides) {
    const source = path.join(generatedRoot, 'include', entry.path);
    if (!existsSync(source)) {
      drifted.push({ id: entry.id, reason: `${entry.path} is no longer generated` });
      continue;
    }
    const digest = createHash('sha256').update(readFileSync(source)).digest('hex');
    if (digest !== entry.derivedFrom) {
      drifted.push({
        id: entry.id,
        reason: `${entry.path} now hashes to ${digest.slice(0, 12)}, derived from ${entry.derivedFrom.slice(0, 12)}`,
      });
    }
  }
  return drifted;
}

function portable(filename) {
  return filename.split(path.sep).join('/');
}

// Emitted text names the anonymous structural shapes it carries by a hash of their members --
// `binding_uid_06e64a362b012722`, `reason_update_457a6ef5d21e9081`. Those hashes are NOT stable: a pin
// move mints new ones, and the same shape can be named differently in the package that uses it and in
// `flight/types`. So an override can name a type that exists nowhere while `driftedOverrides` stays
// green, because the file it shadows is byte-identical -- which is exactly what happened to
// @flighthq/updater and @flighthq/shortcut at a627849, where four references had to be re-pointed and
// the module failed to compile until they were.
//
// This check is deliberately one-directional and exact. It reports an identifier that appears NOWHERE
// in the generated tree, which is a fact about the tree; it says nothing about an identifier that is
// mentioned but never declared, which the compile gate already decides and which a text scan cannot
// judge without becoming the kind of check that fails on its own false positives.
const HASH_SUFFIXED_IDENTIFIER = /\b([a-z_0-9]+_[0-9a-f]{16})\b/gu;

export function hashSuffixedIdentifiers(text) {
  return [...new Set([...text.matchAll(HASH_SUFFIXED_IDENTIFIER)].map((match) => match[1]))];
}

// Whether the override DECLARES this identifier itself. Hand-written bodies build their own shapes --
// `struct entity_runtime_key_..._b54dc1bffa369663 : public flight::ReferenceEnabled { ... }` -- and those
// are not references to anything the tree owes them, so they must not be reported. A `using X = ...`
// alias is deliberately NOT a declaration here: it names a type that has to exist somewhere else, which
// is precisely the stale case (`using binding_uid_... = flight::types::binding_uid_...;`), and an alias
// whose target is live passes the tree test anyway.
//
// This is a text heuristic, and its failure direction is deliberate: a pattern it does not recognize
// under-reports, which leaves the defect in place, rather than over-reports, which teaches a reader to
// ignore the gate.
function declaredInOverride(text, identifier) {
  const escaped = identifier.replace(/[.*+?^${}()|[\]\\]/gu, '\\$&');
  const patterns = [
    `\\b(?:struct|class)\\s+${escaped}\\b`,
    `\\bauto\\s+${escaped}\\b`,
    `\\binline\\s+[^;\\n]*\\b${escaped}\\s*[(=]`,
    // A bare assignment defines a local; `using X = ...` does not, which the lookbehind keeps apart.
    `(?:^|[^:.>\\w])(?<!using )${escaped}\\s*=`,
  ];
  return patterns.some((pattern) => new RegExp(pattern, 'u').test(text));
}

// Every hash-suffixed identifier an override references that the generated tree does not carry at all and
// the override does not declare itself, one record per identifier so a reader can act on each without
// re-reading the file.
export function unresolvedHashReferences(overrides, includeDirectory, generatedRoot) {
  const available = generatedIdentifiers(generatedRoot);
  const unresolved = [];
  for (const entry of overrides) {
    const text = readFileSync(path.join(includeDirectory, entry.path), 'utf8');
    for (const identifier of hashSuffixedIdentifiers(text)) {
      if (available.has(identifier) || declaredInOverride(text, identifier)) continue;
      unresolved.push({ id: entry.id, path: entry.path, identifier });
    }
  }
  return unresolved;
}

// What the generated tree mentions at all, definitions and references alike. Asking for definitions
// specifically would need C++ parsing to be right, and being wrong here has a cost on the other side:
// a gate that flags a live reference teaches its reader to ignore it.
function generatedIdentifiers(generatedRoot) {
  const identifiers = new Set();
  const collect = (directory) => {
    for (const entry of readdirSync(directory, { withFileTypes: true })) {
      const full = path.join(directory, entry.name);
      if (entry.isDirectory()) {
        collect(full);
        continue;
      }
      if (!entry.name.endsWith('.hpp')) continue;
      for (const identifier of hashSuffixedIdentifiers(readFileSync(full, 'utf8'))) identifiers.add(identifier);
    }
  };
  const include = path.join(generatedRoot, 'include');
  if (existsSync(include)) collect(include);
  return identifiers;
}
