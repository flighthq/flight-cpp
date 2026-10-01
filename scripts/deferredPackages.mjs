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
    for (const field of ['package', 'includePrefix', 'reason', 'evidence', 'owner', 'unblockedBy', 'deferredOn']) {
      if (typeof entry[field] !== 'string' || entry[field].length === 0) {
        throw new Error(`Deferred package ${entry.package ?? '<unnamed>'} is missing required field ${field}`);
      }
    }
  }
  return parsed.deferred;
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
