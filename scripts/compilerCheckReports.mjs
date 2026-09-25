import { createHash } from 'node:crypto';
import { existsSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';

const artifactDirectory = 'artifacts';
const reportDirectory = 'out';
const provenancePath = path.join(artifactDirectory, 'flight-compiler-cpp-provenance.json');
const outputPath = path.join(artifactDirectory, 'flight-compiler-cpp-worklist.json');
const environments = ['default', 'web'];
const reportPaths = Object.fromEntries(environments.map((environment) => [
  environment,
  path.join(reportDirectory, `flight-compiler-cpp-${environment}.json`),
]));
const missingReports = Object.values(reportPaths).filter((filename) => !existsSync(filename));

if (missingReports.length > 0) {
  console.log(`Skipping compiler report summary; missing ignored input(s): ${missingReports.join(', ')}`);
  process.exit(0);
}

const provenance = readJson(provenancePath);
const bindingProfiles = readBindingProfiles('bindings');
const declaredBindings = new Map();
for (const profile of bindingProfiles) {
  for (const binding of profile.bindings) {
    const key = bindingKey(binding.sourceName, binding.space);
    const profiles = declaredBindings.get(key) ?? [];
    profiles.push(profile.profile);
    declaredBindings.set(key, profiles);
  }
}

const reports = Object.fromEntries(environments.map((environment) => {
  const filename = reportPaths[environment];
  return [environment, summarizeReport(environment, filename, readJson(filename))];
}));

const upstreamRevisions = new Set(Object.values(reports).map((entry) => entry.provenance.upstream.revision));
if (upstreamRevisions.size !== 1 || !upstreamRevisions.has(provenance.flight.revision)) {
  throw new Error('Report upstream revisions do not match flight-compiler-cpp-provenance.json');
}

const output = {
  schema: 'flight-cpp-compiler-check-worklist/1',
  provenance,
  bindingProfiles: bindingProfiles.map((profile) => ({
    bindings: profile.bindings.length,
    identity: profile.identity,
    profile: profile.profile,
  })),
  reports,
};
writeFileSync(outputPath, `${JSON.stringify(output, null, 2)}\n`);
console.log(`Wrote ${outputPath}`);

function summarizeReport(environment, filename, run) {
  if (run.schema !== 'flight-compiler-check-run/1') {
    throw new Error(`${filename} has unsupported schema ${String(run.schema)}`);
  }
  if (run.report?.schema !== 'flight-compiler-check-report/1') {
    throw new Error(`${filename} has unsupported report schema ${String(run.report?.schema)}`);
  }

  const findingsByIdentity = new Map(run.report.directFindings.map((finding) => [finding.identity, finding]));
  const runtimeFindings = run.report.directFindings.filter((finding) => finding.policyClass === 'target-runtime');
  const runtimeRequirements = new Map();
  const runtimeRequirementsByFinding = new Map();
  const completelyDeclared = new Set();

  for (const finding of runtimeFindings) {
    const requirements = parseRuntimeRequirements(finding, filename);
    runtimeRequirementsByFinding.set(finding.identity, requirements);
    if (requirements.every((requirement) => declaredBindings.has(requirement))) {
      completelyDeclared.add(finding.identity);
    }
    for (const requirement of requirements) {
      const entry = runtimeRequirements.get(requirement) ?? {
        directFindingIdentities: new Set(),
        modules: new Set(),
      };
      entry.directFindingIdentities.add(finding.identity);
      entry.modules.add(finding.module.source);
      runtimeRequirements.set(requirement, entry);
    }
  }

  const ruleGroups = new Map();
  const stableGroups = new Map();
  for (const finding of run.report.directFindings) {
    const diagnostic = finding.rule === undefined ? normalizedDiagnostic(finding) : undefined;
    const key = JSON.stringify([finding.policyClass, finding.code, finding.rule ?? null, diagnostic ?? null]);
    const entry = ruleGroups.get(key) ?? {
      code: finding.code,
      diagnostic,
      directFindingIdentities: new Set(),
      modules: new Set(),
      policyClass: finding.policyClass,
      rule: finding.rule,
    };
    entry.directFindingIdentities.add(finding.identity);
    entry.modules.add(finding.module.source);
    ruleGroups.set(key, entry);

    const stableKey = JSON.stringify([finding.policyClass, finding.code, finding.rule ?? null]);
    const stableEntry = stableGroups.get(stableKey) ?? {
      code: finding.code,
      directFindingIdentities: new Set(),
      modules: new Set(),
      policyClass: finding.policyClass,
      rule: finding.rule,
    };
    stableEntry.directFindingIdentities.add(finding.identity);
    stableEntry.modules.add(finding.module.source);
    stableGroups.set(stableKey, stableEntry);
  }

  return {
    file: filename,
    sha256: sha256(readFileSync(filename)),
    exitCode: run.exitCode,
    eligiblePackages: run.eligiblePackageNames.length,
    excludedRoots: run.excludedRoots,
    provenance: run.report.provenance,
    totals: run.report.totals,
    policyClasses: countBy(run.report.directFindings, (finding) => finding.policyClass),
    codes: countBy(run.report.directFindings, (finding) => finding.code),
    runtimeBindingCoverage: {
      directFindingsCompletelyDeclared: completelyDeclared.size,
      directFindingsPartiallyOrNotDeclared: runtimeFindings.length - completelyDeclared.size,
      cascadesAffected: affectedCascadeCount(run.report.cascades, completelyDeclared),
      cascadesExclusivelyDependingOnDeclaredRuntimeFindings: exclusiveCascadeCount(
        run.report.cascades,
        completelyDeclared,
      ),
    },
    bindingProfileImpact: bindingProfiles.map((profile) => {
      const keys = new Set(profile.bindings.map((binding) => bindingKey(binding.sourceName, binding.space)));
      const identities = new Set([...runtimeRequirementsByFinding.entries()]
        .filter(([, requirements]) => requirements.some((requirement) => keys.has(requirement)))
        .map(([identity]) => identity));
      return {
        profile: profile.profile,
        directFindings: identities.size,
        cascadesAffected: affectedCascadeCount(run.report.cascades, identities),
      };
    }).sort(impactOrder),
    runtimeRequirements: [...runtimeRequirements.entries()]
      .map(([key, entry]) => {
        const { sourceName, space } = parseBindingKey(key);
        return {
          key,
          sourceName,
          space,
          declaredByProfiles: declaredBindings.get(key) ?? [],
          directFindings: entry.directFindingIdentities.size,
          cascadesAffected: affectedCascadeCount(run.report.cascades, entry.directFindingIdentities),
          sampleModules: [...entry.modules].sort().slice(0, 5),
        };
      })
      .sort(impactOrder),
    stableGroups: [...stableGroups.values()]
      .map((entry) => ({
        policyClass: entry.policyClass,
        code: entry.code,
        ...(entry.rule === undefined ? {} : { rule: entry.rule }),
        directFindings: entry.directFindingIdentities.size,
        cascadesAffected: affectedCascadeCount(run.report.cascades, entry.directFindingIdentities),
        cascadesExclusivelyDependingOnGroup: exclusiveCascadeCount(
          run.report.cascades,
          entry.directFindingIdentities,
        ),
        sampleModules: [...entry.modules].sort().slice(0, 5),
      }))
      .sort(impactOrder),
    ruleGroups: [...ruleGroups.values()]
      .map((entry) => ({
        policyClass: entry.policyClass,
        code: entry.code,
        ...(entry.rule === undefined ? {} : { rule: entry.rule }),
        ...(entry.diagnostic === undefined ? {} : { diagnostic: entry.diagnostic }),
        directFindings: entry.directFindingIdentities.size,
        cascadesAffected: affectedCascadeCount(run.report.cascades, entry.directFindingIdentities),
        cascadesExclusivelyDependingOnGroup: exclusiveCascadeCount(
          run.report.cascades,
          entry.directFindingIdentities,
        ),
        sampleModules: [...entry.modules].sort().slice(0, 5),
      }))
      .sort(impactOrder),
    unrecognizedCascadeFindingIdentities: [...new Set(
      run.report.cascades.flatMap((cascade) => cascade.directFindingIdentities)
        .filter((identity) => !findingsByIdentity.has(identity)),
    )].sort(),
  };
}

function parseRuntimeRequirements(finding, filename) {
  const requirements = new Set();
  for (const occurrence of finding.occurrences) {
    const match = occurrence.message.match(/\(missing: (.*)\)$/);
    if (match === null) {
      throw new Error(`${filename}: target-runtime finding has no parseable missing list: ${occurrence.message}`);
    }
    for (const requirement of match[1].split(', ')) requirements.add(requirement);
  }
  return [...requirements].sort();
}

function normalizedDiagnostic(finding) {
  const diagnostics = [...new Set(finding.occurrences.map((occurrence) => occurrence.message
    .replace(/^cpp emission failed for [^:]+: /, '')))].sort();
  return diagnostics.join(' | ');
}

function readBindingProfiles(directory) {
  return readdirSync(directory)
    .filter((filename) => filename.endsWith('.json'))
    .sort()
    .map((filename) => readJson(path.join(directory, filename)))
    .map((profile) => {
      if (profile.schema !== 'flight-cpp-external-bindings/1') {
        throw new Error(`Unsupported binding profile schema ${String(profile.schema)}`);
      }
      return profile;
    });
}

function affectedCascadeCount(cascades, identities) {
  return cascades.filter((cascade) => cascade.directFindingIdentities.some((identity) => identities.has(identity))).length;
}

function exclusiveCascadeCount(cascades, identities) {
  return cascades.filter((cascade) => cascade.directFindingIdentities.length > 0
    && cascade.directFindingIdentities.every((identity) => identities.has(identity))).length;
}

function impactOrder(left, right) {
  return right.directFindings - left.directFindings
    || right.cascadesAffected - left.cascadesAffected
    || (left.key ?? left.rule ?? left.diagnostic ?? '').localeCompare(right.key ?? right.rule ?? right.diagnostic ?? '');
}

function countBy(values, select) {
  const counts = {};
  for (const value of values) {
    const key = select(value);
    counts[key] = (counts[key] ?? 0) + 1;
  }
  return Object.fromEntries(Object.entries(counts).sort(([left], [right]) => left.localeCompare(right)));
}

function bindingKey(sourceName, space) {
  return `${sourceName}[${space}]`;
}

function parseBindingKey(key) {
  const match = key.match(/^(.*)\[(type|value)\]$/);
  if (match === null) throw new Error(`Invalid binding key ${key}`);
  return { sourceName: match[1], space: match[2] };
}

function sha256(contents) {
  return createHash('sha256').update(contents).digest('hex');
}

function readJson(filename) {
  return JSON.parse(readFileSync(filename, 'utf8'));
}
