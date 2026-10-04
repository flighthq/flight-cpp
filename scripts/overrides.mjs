import { createHash } from 'node:crypto';
import { existsSync, readFileSync } from 'node:fs';
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
const STATUSES = new Set(['complete', 'incomplete']);

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
      if (entry.status !== 'complete') {
        throw new Error(
          `Override ${entry.id} claims to supply ${entry.supplies} but its status is ${entry.status}. ` +
            'An override that does not compile cannot be supplying anything.',
        );
      }
    }
    if (entry.status === 'incomplete' && (typeof entry.remaining !== 'string' || entry.remaining.length === 0)) {
      throw new Error(`Override ${entry.id} is incomplete and must say what remains`);
    }
    const file_ = path.join(overridesIncludeDirectory(root), entry.path);
    if (!existsSync(file_)) {
      throw new Error(`Override ${entry.id} declares ${entry.path}, which is absent from overrides/include`);
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
