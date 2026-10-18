import { readFileSync } from 'node:fs';
import path from 'node:path';

// Reads the declared class/interface adoption pairs. A class satisfies an interface structurally and
// is passed wherever that interface is expected; the two are unrelated C++ types, so the emitter has
// no conversion. At a declared pair the class is emitted with the interface as its native base -- the
// interface's data members ARE the class's cells and its method members forward to the class's methods
// -- and an interface value handed to a class parameter recovers that class's exact owner from the
// allocation record. Both directions are declared, never inferred: a matching name proves nothing, and
// the backend additionally requires every member to be representation-compatible.
//
// Sites are checked against the pinned source like the structural-value carriers. A scoped run only
// judges the packages it read, so a fresh clone stays runnable; a site the pin no longer has reports
// and generation refuses.

const SCHEMA = 'flight-cpp-class-interface-adoptions/1';
const IDENTIFIER = /^[A-Za-z_$][\w$]*$/u;

export function loadClassInterfaceAdoptions(root) {
  const file = path.join(root, 'class-interface-adoptions.json');
  let parsed;
  try {
    parsed = JSON.parse(readFileSync(file, 'utf8'));
  } catch (error) {
    if (error.code === 'ENOENT') return [];
    throw new Error(`Class/interface adoption declaration ${portable(file)} is not readable JSON: ${error.message}`);
  }
  if (parsed.schema !== SCHEMA) {
    throw new Error(`Class/interface adoption declaration ${portable(file)} has schema ${parsed.schema}, expected ${SCHEMA}`);
  }
  if (!Array.isArray(parsed.adoptions)) {
    throw new Error(`Class/interface adoption declaration ${portable(file)} has no adoptions array`);
  }
  const ids = new Set();
  for (const adoption of parsed.adoptions) {
    const where = `${portable(file)} adoption ${adoption?.id ?? '<unnamed>'}`;
    for (const field of ['id', 'interface', 'interfacePackage', 'class', 'classPackage', 'reason', 'request']) {
      if (typeof adoption[field] !== 'string' || adoption[field].length === 0) throw new Error(`${where} has no ${field}`);
    }
    if (ids.has(adoption.id)) throw new Error(`${where} is declared twice`);
    ids.add(adoption.id);
    for (const name of [adoption.interface, adoption.class]) {
      if (!IDENTIFIER.test(name)) throw new Error(`${where} names ${name}, which is not a source type name`);
    }
    if (adoption.derive !== undefined && typeof adoption.derive !== 'boolean') {
      throw new Error(`${where} has a non-boolean derive`);
    }
    if (adoption.parameters !== undefined && !Array.isArray(adoption.parameters)) {
      throw new Error(`${where} has no parameters array`);
    }
    for (const site of adoption.parameters ?? []) {
      for (const key of ['package', 'function', 'parameter']) {
        if (typeof site?.[key] !== 'string' || site[key].length === 0) {
          throw new Error(`${where} has a parameter site with no ${key}`);
        }
      }
    }
  }
  return parsed.adoptions;
}

// The compiler option: the declaration minus its bookkeeping.
export function classInterfaceAdoptionOptions(adoptions) {
  return adoptions.map(({ class: className, derive, interface: interfaceName, parameters }) => ({
    class: className,
    ...(derive === undefined ? {} : { derive }),
    interface: interfaceName,
    parameters: (parameters ?? []).map(({ function: callee, package: packageName, parameter }) => ({
      function: callee,
      package: packageName,
      parameter,
    })),
  }));
}

// Every declared requirement whose package is part of this run but whose source no longer has it. Only
// packages in the run are judged, so a scoped generation does not report requirements it never read.
export function unmatchedClassInterfaceAdoptionSites(adoptions, packageSources) {
  const unmatched = [];
  const allSources = [...packageSources.values()].flat();
  for (const adoption of adoptions) {
    const label = `${adoption.id}`;
    const classSources = packageSources.get(adoption.classPackage);
    if (classSources !== undefined) {
      if (!classSources.some((text) => new RegExp(`\\bclass\\s+${adoption.class}\\b`, 'u').test(text))) {
        unmatched.push(`${label}: ${adoption.classPackage} class ${adoption.class}`);
      } else if (
        adoption.derive === true &&
        classSources.some((text) => new RegExp(`\\bclass\\s+${adoption.class}\\s+extends\\b`, 'u').test(text))
      ) {
        unmatched.push(`${label}: ${adoption.class} declares a base, so a native interface base is not the bounded shape`);
      }
    }
    const interfaceSources = packageSources.get(adoption.interfacePackage);
    if (
      interfaceSources !== undefined &&
      !interfaceSources.some((text) => new RegExp(`\\binterface\\s+${adoption.interface}\\b`, 'u').test(text))
    ) {
      unmatched.push(`${label}: ${adoption.interfacePackage} interface ${adoption.interface}`);
    }
    for (const site of adoption.parameters ?? []) {
      if (packageSources.get(site.package) === undefined) continue;
      const declared = allSources.some((text) =>
        new RegExp(`function\\s+${site.function}\\s*(?:<[^>]*>)?\\s*\\([^)]*\\b${site.parameter}\\s*:`, 'su').test(text),
      );
      if (!declared) unmatched.push(`${label}: ${site.package} function ${site.function} parameter ${site.parameter}`);
    }
  }
  return unmatched;
}

function portable(file) {
  return file.split(path.sep).join('/');
}
