import { readFileSync } from 'node:fs';
import path from 'node:path';

// Reads the declared structural-value carrier sites. A value narrowed from a base interface by a user
// predicate may be any object declaring the derived type's members, so it has no faithful Ref to the
// derived type. At exactly the sites declared here the compiler carries it as the same object through a
// runtime view instead (flight::NominalOrStructuralView); every other use keeps the nominal owner.
//
// Sites are declared, never inferred, and each must still name something in the pinned source: a site the
// pin no longer has would silently stop applying, so `unmatchedStructuralValueCarrierSites` reports it and
// generation refuses. The compiler option this feeds exists only with the carrier source patch applied.

const SCHEMA = 'flight-cpp-structural-value-carriers/1';
const IDENTIFIER = /^[A-Za-z_$][\w$]*$/u;

export function loadStructuralValueCarriers(root) {
  const file = path.join(root, 'structural-value-carriers.json');
  let parsed;
  try {
    parsed = JSON.parse(readFileSync(file, 'utf8'));
  } catch (error) {
    if (error.code === 'ENOENT') return [];
    throw new Error(`Structural value carrier declaration ${portable(file)} is not readable JSON: ${error.message}`);
  }
  if (parsed.schema !== SCHEMA) {
    throw new Error(`Structural value carrier declaration ${portable(file)} has schema ${parsed.schema}, expected ${SCHEMA}`);
  }
  if (!Array.isArray(parsed.carriers)) {
    throw new Error(`Structural value carrier declaration ${portable(file)} has no carriers array`);
  }
  const ids = new Set();
  for (const carrier of parsed.carriers) {
    const where = `${portable(file)} carrier ${carrier?.id ?? '<unnamed>'}`;
    for (const field of ['id', 'nominal', 'base', 'target', 'reason', 'request']) {
      if (typeof carrier[field] !== 'string' || carrier[field].length === 0) throw new Error(`${where} has no ${field}`);
    }
    if (ids.has(carrier.id)) throw new Error(`${where} is declared twice`);
    ids.add(carrier.id);
    for (const name of [carrier.nominal, carrier.base]) {
      if (!IDENTIFIER.test(name)) throw new Error(`${where} names ${name}, which is not a source type name`);
    }
    if (!carrier.target.startsWith('flight::NominalOrStructuralView<')) {
      throw new Error(`${where} target must be a flight::NominalOrStructuralView, not ${carrier.target}`);
    }
    const sites = [
      ...(carrier.parameters ?? []).map((site) => ['parameter', site, ['package', 'function', 'parameter']]),
      ...(carrier.fields ?? []).map((site) => ['field', site, ['package', 'owner', 'field']]),
      ...(carrier.cells ?? []).map((site) => ['shared cell', site, ['package', 'owner', 'baseOwner', 'field']]),
      ...(carrier.callbackParameters ?? []).map((site) => ['callback parameter', site, ['package', 'owner', 'field']]),
    ];
    if (sites.length === 0) throw new Error(`${where} declares no sites`);
    for (const [kind, site, keys] of sites) {
      for (const key of keys) {
        if (typeof site[key] !== 'string' || site[key].length === 0) throw new Error(`${where} has a ${kind} site with no ${key}`);
      }
      if (kind === 'callback parameter' && !(Number.isInteger(site.position) && site.position >= 0)) {
        throw new Error(`${where} callback parameter ${site.owner}.${site.field} has no position`);
      }
    }
  }
  return parsed.carriers;
}

// The compiler option: the declaration minus its bookkeeping.
export function structuralValueCarrierOptions(carriers) {
  return carriers.map(({ base, callbackParameters, cells, fields, nominal, parameters, target }) => ({
    base,
    ...(cells ? { cells } : {}),
    ...(callbackParameters ? { callbackParameters } : {}),
    ...(fields ? { fields } : {}),
    nominal,
    ...(parameters ? { parameters } : {}),
    target,
  }));
}

// Every declared site whose package is part of this run but whose source no longer declares it. Only
// packages in the run are judged, so a scoped generation does not report sites it never read.
export function unmatchedStructuralValueCarrierSites(carriers, packageSources) {
  const unmatched = [];
  for (const carrier of carriers) {
    const check = (site, description, patterns) => {
      const sources = packageSources.get(site.package);
      if (sources === undefined) return;
      if (!patterns.every((pattern) => sources.some((text) => pattern.test(text)))) {
        unmatched.push(`${carrier.id}: ${site.package} ${description}`);
      }
    };
    for (const site of carrier.parameters ?? []) {
      check(site, `function ${site.function} parameter ${site.parameter}`, [
        new RegExp(`function\\s+${site.function}\\s*(?:<[^>]*>)?\\s*\\([^)]*\\b${site.parameter}\\s*:[^)]*\\b${carrier.nominal}\\b`, 'su'),
      ]);
    }
    for (const site of carrier.cells ?? []) {
      check(site, `${site.owner}.${site.field} shared with ${site.baseOwner}`, [
        new RegExp(`\\binterface\\s+${site.owner}\\s+extends\\s+${site.baseOwner}\\b`, 'u'),
        new RegExp(`\\b(?:interface|type)\\s+${site.baseOwner}\\b`, 'u'),
        new RegExp(`\\b${site.field}\\b`, 'u'),
      ]);
    }
    for (const site of [...(carrier.fields ?? []), ...(carrier.callbackParameters ?? [])]) {
      check(site, `${site.owner}.${site.field}`, [
        new RegExp(`\\b(?:interface|type)\\s+${site.owner}\\b`, 'u'),
        new RegExp(`\\b${site.field}\\b`, 'u'),
      ]);
    }
  }
  return unmatched;
}

function portable(file) {
  return file.split(path.sep).join('/');
}
