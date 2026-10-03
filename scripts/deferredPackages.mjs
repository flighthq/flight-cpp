import { readFileSync } from 'node:fs';
import path from 'node:path';

// Reads the declared SDK package deferrals. A deferral lets a package's generated headers fail the
// compile gate without failing the build, which is how a package nothing needs stops holding the SDK
// hostage. Nothing here suppresses output: the headers are still emitted and still compiled, so the
// gate can report exactly what it parked and un-deferring costs one line.
//
// The check that makes this honest is `refusedDeferrals`. A package that some non-deferred header
// still includes cannot be deferred, because its break is not parked -- it reaches its consumer
// anyway, and deferring it would only move the failure somewhere harder to read.

const SCHEMA = 'flight-cpp-deferred-packages/1';
// Why a package is deferred, which is a different question from whether it compiles.
//
//   not-applicable  the profile cannot provide the capability the package exists to implement, so the
//                   package is not broken and does not belong in the shippable denominator
//   defect          the package should work on this profile and does not; it stays in the denominator
//
// Keeping these apart is what makes the shippable fraction mean something. A Canvas 2D renderer counted
// as "blocked" on a host with no canvas reads as outstanding work forever.
const KINDS = new Set(['not-applicable', 'defect']);

export function loadDeferredPackages(root) {
  const file = path.join(root, 'deferred-packages.json');
  let parsed;
  try {
    parsed = JSON.parse(readFileSync(file, 'utf8'));
  } catch (error) {
    if (error.code === 'ENOENT') return [];
    throw new Error(`Deferred package declaration ${portable(file)} is not readable JSON: ${error.message}`);
  }
  if (parsed.schema !== SCHEMA) {
    throw new Error(`Deferred package declaration ${portable(file)} has schema ${parsed.schema}, expected ${SCHEMA}`);
  }
  if (!Array.isArray(parsed.deferred)) {
    throw new Error(`Deferred package declaration ${portable(file)} has no deferred array`);
  }
  for (const entry of parsed.deferred) {
    for (const field of ['package', 'includePrefix', 'kind', 'reason', 'evidence', 'owner', 'unblockedBy', 'deferredOn']) {
      if (typeof entry[field] !== 'string' || entry[field].length === 0) {
        throw new Error(`Deferred package ${entry.package ?? '<unnamed>'} is missing required field ${field}`);
      }
    }
    if (entry.generation !== undefined && entry.generation !== 'does-not-terminate') {
      throw new Error(
        `Deferred package ${entry.package} has unknown generation value ${entry.generation}; the only ` +
          'value is "does-not-terminate".',
      );
    }
    if (entry.generation === 'does-not-terminate' && entry.kind !== 'defect') {
      throw new Error(
        `Deferred package ${entry.package} claims generation does-not-terminate but is kind ${entry.kind}. ` +
          'A package the generator cannot finish is a defect we owe, never not-applicable.',
      );
    }
    if (!KINDS.has(entry.kind)) {
      throw new Error(`Deferred package ${entry.package} has unknown kind ${entry.kind}`);
    }
  }
  return parsed.deferred;
}

// The host environments this profile can actually run. A package declaring any other environment in its
// own package.json is not applicable here. Returns a Set so the caller can ask directly.
// The packages generation cannot even attempt, because the compiler does not terminate over them.
//
// This is the ONE case where a deferral changes generated output, and it is worth being explicit that it
// is an exception rather than how deferral works. Every other deferral still emits its headers and still
// compiles them, so progress stays visible and un-deferring is a one-line change. These cannot: there is
// no output to show, because the run that would produce it never ends. Excluding them is the difference
// between a reproducible committed inventory and none at all.
//
// It is declared per entry rather than inferred, and `loadDeferredPackages` refuses the claim on anything
// but a `defect`, because a package the generator cannot finish is debt we owe and never a capability
// this profile lacks.
export function nonTerminatingPackages(deferred) {
  return deferred.filter((entry) => entry.generation === 'does-not-terminate').map((entry) => entry.package);
}

export function loadApplicableEnvironments(root) {
  const file = path.join(root, 'deferred-packages.json');
  try {
    const parsed = JSON.parse(readFileSync(file, 'utf8'));
    const declared = parsed.applicableEnvironments;
    if (!Array.isArray(declared)) return new Set();
    return new Set(declared.filter((entry) => typeof entry === 'string'));
  } catch (error) {
    if (error.code === 'ENOENT') return new Set();
    throw error;
  }
}

export function isDeferredHeader(deferred, header) {
  return deferred.some((entry) => header.startsWith(`${entry.includePrefix}/`));
}

// A deferral is only real when nothing outside the deferred set includes it. Returns one record per
// deferral that a required header still reaches, naming the consumers, so the gate can refuse it
// rather than report a false green.
export function refusedDeferrals(deferred, headers, readHeader) {
  const refused = [];
  for (const entry of deferred) {
    const pattern = new RegExp(`^\\s*#include\\s*<${escape(entry.includePrefix)}/`, 'mu');
    const consumers = [];
    for (const header of headers) {
      if (isDeferredHeader(deferred, header)) continue;
      if (pattern.test(readHeader(header))) consumers.push(header);
    }
    if (consumers.length > 0) refused.push({ consumers, package: entry.package });
  }
  return refused;
}

function escape(value) {
  return value.replaceAll(/[.*+?^${}()|[\]\\]/gu, String.raw`\$&`);
}

function portable(filename) {
  return filename.split(path.sep).join('/');
}
