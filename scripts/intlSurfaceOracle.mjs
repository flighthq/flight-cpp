import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import { resolveDependency } from './dependencyLock.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const compiler = resolveDependency(root, 'flight-compiler');
const compilerEntry = path.join(
  compiler.directory,
  'packages',
  'tool-compiler',
  'dist',
  'packages',
  'tool-compiler',
  'src',
  'index.js',
);
if (!existsSync(compiler.directory)) {
  process.stdout.write('flight-compiler is not rehydrated (Intl surface oracle skipped).\n');
  process.exit(0);
}
if (!existsSync(compilerEntry)) {
  const build = spawnSync('npm', ['run', 'build', '--silent'], { cwd: compiler.directory, encoding: 'utf8' });
  if (build.status !== 0) {
    process.stderr.write(`${build.stdout}${build.stderr}`);
    process.exit(1);
  }
}

const api = await import(pathToFileURL(compilerEntry));
const bindings = JSON.parse(readFileSync(path.join(root, 'bindings', 'runtime.json'), 'utf8'));
const expectedTargets = new Map([
  ['LocalesArgument', 'flight::IntlLocalesArgument'],
  ['CollatorOptions', 'flight::IntlCollatorOptions'],
  ['DateTimeFormatOptions', 'flight::IntlDateTimeFormatOptions'],
  ['ListFormatOptions', 'flight::IntlListFormatOptions'],
  ['NumberFormatOptions', 'flight::IntlNumberFormatOptions'],
  ['PluralRulesOptions', 'flight::IntlPluralRulesOptions'],
  ['RelativeTimeFormatOptions', 'flight::IntlRelativeTimeFormatOptions'],
  ['SegmenterOptions', 'flight::IntlSegmenterOptions'],
  ['Intl.SegmentData', 'flight::IntlSegmentData'],
  ['Intl.Segmenter', 'flight::IntlSegmenter'],
  ['Intl.Segments', 'flight::IntlSegments'],
]);

const plan = api.createCompilerRuntimeExternalSymbolBindingPlanCpp('flight-cpp', bindings);
for (const [sourceName, targetName] of expectedTargets) {
  const target = api.getCompilerRuntimeExternalSymbolTargetCpp(sourceName, 'type', 'flight-cpp', bindings);
  if (target !== targetName) {
    throw new Error(`Intl binding ${sourceName}[type] resolved to ${String(target)}, expected ${targetName}`);
  }
  if (
    !plan.bindings.some(
      (binding) =>
        binding.kind === 'native' &&
        binding.externalSymbol.sourceName === sourceName &&
        binding.externalSymbol.space === 'type',
    )
  ) {
    throw new Error(`Intl binding plan did not retain ${sourceName}[type]`);
  }
}

if (api.getCompilerRuntimeExternalSymbolTargetCpp('Intl.Segmenter', 'value', 'flight-cpp', bindings) !== undefined) {
  throw new Error('Intl.Segmenter must remain a type-only binding until a real Unicode provider exists');
}

const expectedFields = new Map([
  ['CollatorOptions', ['sensitivity', 'numeric', 'caseFirst']],
  ['DateTimeFormatOptions', ['year', 'month', 'day', 'hour', 'minute']],
  ['ListFormatOptions', ['type', 'style']],
  ['NumberFormatOptions', ['notation', 'style', 'currency', 'unit']],
  ['PluralRulesOptions', ['type']],
  ['RelativeTimeFormatOptions', ['numeric']],
  ['SegmenterOptions', ['granularity']],
]);
for (const [sourceName, fields] of expectedFields) {
  const binding = bindings.bindings.find(
    (candidate) => candidate.sourceName === sourceName && candidate.space === 'type',
  );
  const actual = binding?.objectConstruction?.fields.map((field) => field.sourceField);
  if (JSON.stringify(actual) !== JSON.stringify(fields)) {
    throw new Error(
      `Intl binding ${sourceName}[type] fields were ${JSON.stringify(actual)}, expected ${JSON.stringify(fields)}`,
    );
  }
}

process.stdout.write(
  `Intl type bindings, including dotted namespace members, round-trip exactly (${compiler.commit.slice(0, 7)}).\n`,
);
