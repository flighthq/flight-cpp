import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { resolveDependency } from './dependencyLock.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const options = process.argv.slice(2);
const check = options.includes('--check');
const unknown = options.filter((option) => option !== '--check');
if (unknown.length > 0) {
  process.stderr.write(`Unknown upstream parity prototype option(s): ${unknown.join(', ')}\n`);
  process.exit(1);
}

const flight = resolveDependency(root, 'flight');
if (!existsSync(path.join(flight.directory, '.git'))) {
  process.stdout.write('flight is not rehydrated (upstream test parity prototype check skipped).\n');
  process.exit(0);
}
const head = spawnSync('git', ['rev-parse', 'HEAD'], { cwd: flight.directory, encoding: 'utf8' });
if (head.status !== 0 || head.stdout.trim() !== flight.commit) {
  process.stderr.write(`flight is not at pinned revision ${flight.commit.slice(0, 7)}.\n`);
  process.exit(1);
}

const generatedPath = path.join(root, 'tests', 'generated', 'upstream_math_geometry_parity.cpp');
const reportPath = path.join(root, 'artifacts', 'upstream-test-parity-prototype.json');
const packageNames = ['math', 'geometry'];
const pilot = [
  { packageName: 'math', source: 'clamp.test.ts' },
  {
    describe: 'reserveFloat32Array',
    packageName: 'geometry',
    source: 'typedarray.test.ts',
    title: 'leaves extended capacity zero-filled',
  },
];
const matcherMethods = new Map([
  ['toBe', 'to_be'],
  ['toBeCloseTo', 'to_be_close_to'],
  ['toBeGreaterThan', 'to_be_greater_than'],
  ['toBeGreaterThanOrEqual', 'to_be_greater_than_or_equal'],
  ['toBeLessThan', 'to_be_less_than'],
  ['toBeLessThanOrEqual', 'to_be_less_than_or_equal'],
  ['toBeNaN', 'to_be_nan'],
  ['toEqual', 'to_equal'],
]);

const files = [];
for (const packageName of packageNames) {
  const sourceDirectory = path.join(flight.directory, 'packages', packageName, 'src');
  for (const entry of readdirSync(sourceDirectory).sort()) {
    if (!entry.endsWith('.test.ts')) continue;
    const filename = path.join(sourceDirectory, entry);
    const source = readFileSync(filename, 'utf8');
    const tokens = tokenize(source);
    const descriptions = findCallbacks(tokens, 'describe');
    const imports = parseImports(source, packageName);
    const tests = findTests(tokens).map((test) => {
      const describePath = descriptions
        .filter((description) => test.start > description.bodyStart && test.start < description.bodyEnd)
        .sort((left, right) => left.bodyStart - right.bodyStart)
        .map((description) => description.title);
      const classification = classify(test, tokens);
      const expectationCount =
        test.bodyStart === undefined || test.bodyEnd === undefined
          ? 0
          : tokens
              .slice(test.bodyStart + 1, test.bodyEnd)
              .filter((token, index, body) => token.value === 'expect' && body[index + 1]?.value === '(').length;
      return { ...test, classification, describePath, expectationCount };
    });
    files.push({
      imports,
      packageName,
      path: `packages/${packageName}/src/${entry}`,
      sha256: sha256(source),
      source,
      sourceName: entry,
      tests,
      tokens,
    });
  }
}

const packageReports = {};
const rejectionReasons = {};
let eligibleTests = 0;
let eligibleExpectations = 0;
let totalTests = 0;
let totalExpectations = 0;
for (const packageName of packageNames) {
  const packageFiles = files.filter((file) => file.packageName === packageName);
  const tests = packageFiles.flatMap((file) => file.tests);
  const eligible = tests.filter((test) => test.classification.reason === undefined);
  const expectations = tests.reduce((sum, test) => sum + test.expectationCount, 0);
  const eligibleExpectationCount = eligible.reduce(
    (sum, test) => sum + test.classification.expectations.length,
    0,
  );
  packageReports[packageName] = {
    eligibleFraction: fraction(eligible.length, tests.length),
    eligibleExpectations: eligibleExpectationCount,
    eligibleTests: eligible.length,
    expectations,
    files: packageFiles.length,
    tests: tests.length,
  };
  eligibleTests += eligible.length;
  eligibleExpectations += eligibleExpectationCount;
  totalTests += tests.length;
  totalExpectations += expectations;
  for (const test of tests) {
    const reason = test.classification.reason;
    if (reason !== undefined) rejectionReasons[reason] = (rejectionReasons[reason] ?? 0) + 1;
  }
}

const pilotTests = [];
for (const selection of pilot) {
  const file = files.find(
    (candidate) => candidate.packageName === selection.packageName && candidate.sourceName === selection.source,
  );
  if (file === undefined) throw new Error(`Pilot source is missing: ${selection.packageName}/${selection.source}`);
  const selected = file.tests.filter(
    (test) =>
      (selection.describe === undefined || test.describePath.includes(selection.describe)) &&
      (selection.title === undefined || test.title === selection.title),
  );
  if (selected.length === 0) throw new Error(`Pilot selection matched no tests: ${JSON.stringify(selection)}`);
  for (const test of selected) {
    if (test.classification.reason !== undefined) {
      throw new Error(`${file.path}:${String(test.line)} pilot test is not eligible: ${test.classification.reason}`);
    }
    pilotTests.push({ file, test });
  }
}

const generated = renderCpp(pilotTests);
const report = {
  schema: 'flight-upstream-test-parity-prototype/1',
  source: {
    package: '@flighthq/flight',
    revision: flight.commit,
  },
  scope: {
    packages: packageNames,
    pattern: 'packages/{math,geometry}/src/*.test.ts',
  },
  eligibility: {
    definition:
      'An it callback contains only const declarations and non-negated expect calls, uses a native-harness matcher, and uses the conservative expression subset accepted by the prototype.',
    eligibleFraction: fraction(eligibleTests, totalTests),
    eligibleExpectations,
    eligibleTests,
    expectations: totalExpectations,
    files: files.length,
    packages: packageReports,
    rejectionReasons: Object.fromEntries(Object.entries(rejectionReasons).sort(([left], [right]) => left.localeCompare(right))),
    tests: totalTests,
  },
  pilot: {
    compiledTestCount: pilotTests.length,
    generatedFile: portable(path.relative(root, generatedPath)),
    selections: pilot.map((selection) => {
      const file = files.find(
        (candidate) => candidate.packageName === selection.packageName && candidate.sourceName === selection.source,
      );
      const tests = file.tests.filter(
        (test) =>
          (selection.describe === undefined || test.describePath.includes(selection.describe)) &&
          (selection.title === undefined || test.title === selection.title),
      );
      return {
        describe: selection.describe ?? '*',
        sha256: file.sha256,
        source: file.path,
        tests: tests.length,
        title: selection.title ?? '*',
      };
    }),
  },
};
const serializedReport = `${JSON.stringify(report, undefined, 2)}\n`;

if (check) {
  let failed = false;
  failed = compare(generatedPath, generated) || failed;
  failed = compare(reportPath, serializedReport) || failed;
  if (failed) {
    process.stderr.write('Run `npm run parity:generate` and commit the refreshed prototype artifacts.\n');
    process.exit(1);
  }
  process.stdout.write(
    `Upstream parity prototype matches Flight ${flight.commit.slice(0, 7)}: ${String(eligibleTests)}/${String(totalTests)} eligible, ${String(pilotTests.length)} compiled pilot tests.\n`,
  );
} else {
  mkdirSync(path.dirname(generatedPath), { recursive: true });
  mkdirSync(path.dirname(reportPath), { recursive: true });
  writeFileSync(generatedPath, generated);
  writeFileSync(reportPath, serializedReport);
  process.stdout.write(
    `Generated upstream parity prototype: ${String(eligibleTests)}/${String(totalTests)} eligible, ${String(pilotTests.length)} pilot tests.\n`,
  );
}

function classify(test, tokens) {
  if (test.parameterized) return { expectations: [], reason: 'parameterized-it-each' };
  if (test.bodyStart === undefined || test.bodyEnd === undefined) {
    return { expectations: [], reason: 'non-block-test-callback' };
  }
  const expectations = [];
  const statements = [];
  let cursor = test.bodyStart + 1;
  while (cursor < test.bodyEnd) {
    while (tokens[cursor]?.value === ';') cursor += 1;
    if (cursor >= test.bodyEnd) break;
    const parsed = parseExpectation(tokens, cursor, test.bodyEnd);
    if (parsed === undefined) {
      const declaration = parseConstDeclaration(tokens, cursor, test.bodyEnd);
      if (declaration === undefined) return { expectations, statements, reason: 'imperative-or-setup-body' };
      if (!expressionSupported(declaration.initializer)) {
        return { expectations, statements, reason: 'unsupported-expression' };
      }
      statements.push(declaration);
      cursor = declaration.end;
      continue;
    }
    if (parsed.negated) return { expectations, statements, reason: 'negated-expectation' };
    if (!matcherMethods.has(parsed.matcher)) {
      return { expectations, statements, reason: `unsupported-matcher:${parsed.matcher}` };
    }
    const expressions = [parsed.actual, ...parsed.arguments];
    if (!expressions.every((expression) => expressionSupported(expression))) {
      return { expectations, statements, reason: 'unsupported-expression' };
    }
    expectations.push(parsed);
    statements.push(parsed);
    cursor = parsed.end;
  }
  return expectations.length === 0
    ? { expectations, statements, reason: 'empty-test-body' }
    : { expectations, statements, reason: undefined };
}

function parseConstDeclaration(tokens, start, limit) {
  if (tokens[start]?.value !== 'const' || tokens[start + 1]?.kind !== 'identifier') return undefined;
  let end = start + 2;
  const stack = [];
  while (end < limit) {
    const value = tokens[end].value;
    if (['(', '[', '{'].includes(value)) stack.push(value);
    else if ([')', ']', '}'].includes(value)) stack.pop();
    if (value === ';' && stack.length === 0) break;
    end += 1;
  }
  if (end >= limit) return undefined;
  const declaration = tokens.slice(start + 2, end);
  const equals = topLevelIndex(declaration, '=');
  if (equals === -1) return undefined;
  return {
    end: end + 1,
    initializer: declaration.slice(equals + 1),
    kind: 'declaration',
    name: tokens[start + 1].value,
  };
}

function parseExpectation(tokens, start, limit) {
  if (tokens[start]?.value !== 'expect' || tokens[start + 1]?.value !== '(') return undefined;
  const actualClose = matchingToken(tokens, start + 1, '(', ')');
  if (actualClose === undefined || actualClose >= limit) return undefined;
  let cursor = actualClose + 1;
  if (tokens[cursor]?.value !== '.') return undefined;
  cursor += 1;
  let negated = false;
  if (tokens[cursor]?.value === 'not') {
    negated = true;
    cursor += 1;
    if (tokens[cursor]?.value !== '.') return undefined;
    cursor += 1;
  }
  const matcher = tokens[cursor]?.value;
  if (tokens[cursor]?.kind !== 'identifier' || tokens[cursor + 1]?.value !== '(') return undefined;
  const matcherClose = matchingToken(tokens, cursor + 1, '(', ')');
  if (matcherClose === undefined || matcherClose >= limit) return undefined;
  let end = matcherClose + 1;
  if (tokens[end]?.value === ';') end += 1;
  const arguments_ = splitArguments(tokens.slice(cursor + 2, matcherClose));
  return {
    actual: tokens.slice(start + 2, actualClose),
    arguments: arguments_,
    end,
    matcher,
    negated,
  };
}

function expressionSupported(tokens) {
  if (tokens.length === 0) return false;
  const blockedIdentifiers = new Set([
    'Object',
    'Symbol',
    'as',
    'async',
    'await',
    'function',
    'instanceof',
    'null',
    'typeof',
    'undefined',
  ]);
  const blockedTokens = new Set(['=>', '===', '!==', '?', '?.', '??']);
  for (let index = 0; index < tokens.length; index += 1) {
    const token = tokens[index];
    if (token.kind === 'string' || token.kind === 'template') return false;
    if (blockedIdentifiers.has(token.value) || blockedTokens.has(token.value)) return false;
    if (token.value === '=' || token.value.endsWith('=')) {
      if (!['<=', '>=', '==', '!='].includes(token.value)) return false;
    }
    if (token.value === '.') {
      const owner = tokens[index - 1]?.value;
      const member = tokens[index + 1]?.value;
      if (owner !== 'Math' && !(owner === 'Array' && member === 'from') && member !== 'length') return false;
      index += 1;
    }
    if (token.value === 'new') {
      if (!['Float32Array', 'Int16Array', 'Uint16Array'].includes(tokens[index + 1]?.value)) return false;
    }
  }
  return objectLiteralsSupported(tokens) && arrayLiteralsSupported(tokens);
}

function arrayLiteralsSupported(tokens) {
  for (let index = 0; index < tokens.length; index += 1) {
    if (tokens[index].value !== '[') continue;
    const close = matchingToken(tokens, index, '[', ']');
    if (close === undefined) return false;
    if (!splitArguments(tokens.slice(index + 1, close)).every((element) => expressionSupported(element))) return false;
    index = close;
  }
  return true;
}

function objectLiteralsSupported(tokens) {
  for (let index = 0; index < tokens.length; index += 1) {
    if (tokens[index].value !== '{') continue;
    const close = matchingToken(tokens, index, '{', '}');
    if (close === undefined) return false;
    const properties = splitArguments(tokens.slice(index + 1, close));
    const names = [];
    for (const property of properties) {
      const colon = topLevelIndex(property, ':');
      if (colon !== 1 || property[0]?.kind !== 'identifier') return false;
      if (!expressionSupported(property.slice(colon + 1))) return false;
      names.push(property[0].value);
    }
    if (names.join(',') !== 'x,y') return false;
    index = close;
  }
  return true;
}

function findTests(tokens) {
  const tests = [];
  for (let index = 0; index < tokens.length; index += 1) {
    if (tokens[index].value !== 'it' || tokens[index - 1]?.value === '.') continue;
    let open = index + 1;
    let parameterized = false;
    if (tokens[open]?.value === '.' && tokens[open + 1]?.value === 'each' && tokens[open + 2]?.value === '(') {
      parameterized = true;
      const tableClose = matchingToken(tokens, open + 2, '(', ')');
      if (tableClose === undefined || tokens[tableClose + 1]?.value !== '(') continue;
      open = tableClose + 1;
    }
    if (tokens[open]?.value !== '(') continue;
    const callback = parseCallback(tokens, index, open);
    if (callback !== undefined) tests.push({ ...callback, parameterized });
  }
  return tests;
}

function findCallbacks(tokens, name) {
  const callbacks = [];
  for (let index = 0; index < tokens.length; index += 1) {
    if (tokens[index].value !== name || tokens[index - 1]?.value === '.' || tokens[index + 1]?.value !== '(') continue;
    const callback = parseCallback(tokens, index, index + 1);
    if (callback !== undefined) callbacks.push(callback);
  }
  return callbacks;
}

function parseCallback(tokens, start, open) {
  const close = matchingToken(tokens, open, '(', ')');
  if (close === undefined) return undefined;
  const title = tokens[open + 1]?.kind === 'string' ? decodeString(tokens[open + 1].raw) : '<dynamic title>';
  let arrow;
  for (let index = open + 2; index < close; index += 1) {
    if (tokens[index].value === '=>') {
      arrow = index;
      break;
    }
  }
  if (arrow === undefined) return { bodyEnd: undefined, bodyStart: undefined, end: close, line: tokens[start].line, start, title };
  const bodyStart = arrow + 1;
  if (tokens[bodyStart]?.value !== '{') {
    return { bodyEnd: undefined, bodyStart: undefined, end: close, line: tokens[start].line, start, title };
  }
  const bodyEnd = matchingToken(tokens, bodyStart, '{', '}');
  if (bodyEnd === undefined || bodyEnd > close) return undefined;
  return { bodyEnd, bodyStart, end: close, line: tokens[start].line, start, title };
}

function renderCpp(selected) {
  const includes = [...new Set(selected.map(({ file }) => headerFor(file)))].sort();
  const groups = new Map();
  for (const entry of selected) {
    const name = `@flighthq/${entry.file.packageName} :: ${entry.test.describePath.join(' > ')}`;
    const group = groups.get(name) ?? [];
    group.push(entry);
    groups.set(name, group);
  }
  const lines = [
    `// Generated by scripts/upstreamTestParityPrototype.mjs from @flighthq/flight ${flight.commit}.`,
    '// Do not edit; run `npm run parity:generate`.',
    '#include "upstream_test_harness.hpp"',
    '',
    '#include <array>',
    '#include <limits>',
    '#include <numbers>',
    ...includes.map((include) => `#include <${include}>`),
    '',
    'int main() {',
    '  flight::upstream_test::Runner runner;',
  ];
  for (const [description, entries] of groups) {
    lines.push('', `  runner.describe(${cppString(description)}, [&runner]() {`);
    for (const { file, test } of entries) {
      lines.push(
        `    // ${file.path}:${String(test.line)} (sha256:${file.sha256})`,
        `    runner.it(${cppString(test.title)}, []() {`,
      );
      for (const statement of test.classification.statements) {
        if (statement.kind === 'declaration') {
          lines.push(`      const auto ${statement.name} = ${renderExpression(statement.initializer, file)};`);
        } else {
          const actual = renderExpression(statement.actual, file);
          const arguments_ = statement.arguments.map((argument) => renderExpression(argument, file));
          lines.push(
            `      flight::upstream_test::expect(${actual}).${matcherMethods.get(statement.matcher)}(${arguments_.join(', ')});`,
          );
        }
      }
      lines.push('    });');
    }
    lines.push('  });');
  }
  lines.push('', '  return runner.run();', '}', '');
  return lines.join('\n');
}

function renderExpression(tokens, file) {
  const parts = [];
  for (let index = 0; index < tokens.length; index += 1) {
    const token = tokens[index];
    if (token.value === 'new' && ['Float32Array', 'Int16Array', 'Uint16Array'].includes(tokens[index + 1]?.value)) {
      parts.push(`flight::${tokens[index + 1].value}`);
      index += 1;
      continue;
    }
    if (token.value === 'Array' && tokens[index + 1]?.value === '.' && tokens[index + 2]?.value === 'from') {
      const open = index + 3;
      const close = matchingToken(tokens, open, '(', ')');
      parts.push(renderExpression(tokens.slice(open + 1, close), file));
      index = close;
      continue;
    }
    if (token.value === '[') {
      const close = matchingToken(tokens, index, '[', ']');
      const values = splitArguments(tokens.slice(index + 1, close)).map((value) => renderExpression(value, file));
      parts.push(`std::array{${values.join(', ')}}`);
      index = close;
      continue;
    }
    if (token.value === '{') {
      const close = matchingToken(tokens, index, '{', '}');
      const properties = splitArguments(tokens.slice(index + 1, close));
      const values = properties.map((property) => {
        const colon = topLevelIndex(property, ':');
        return renderExpression(property.slice(colon + 1), file);
      });
      parts.push(`flight::geometry::create_vector2(${values.join(', ')})`);
      index = close;
      continue;
    }
    if (token.value === 'Math' && tokens[index + 1]?.value === '.') {
      const member = tokens[index + 2]?.value;
      const mathNames = new Map([
        ['PI', 'std::numbers::pi_v<double>'],
        ['abs', 'std::abs'],
        ['ceil', 'std::ceil'],
        ['floor', 'std::floor'],
        ['hypot', 'std::hypot'],
        ['max', 'std::max'],
        ['min', 'std::min'],
        ['pow', 'std::pow'],
        ['round', 'std::round'],
        ['sqrt', 'std::sqrt'],
      ]);
      parts.push(mathNames.get(member) ?? `std::${member}`);
      index += 2;
      continue;
    }
    if (token.value === 'NaN') {
      parts.push('std::numeric_limits<double>::quiet_NaN()');
    } else if (token.value === 'Infinity') {
      parts.push('std::numeric_limits<double>::infinity()');
    } else if (token.value === '.' && tokens[index + 1]?.value === 'length') {
      parts.push('.size()');
      index += 1;
    } else if (file.imports.has(token.value)) {
      parts.push(file.imports.get(token.value));
    } else {
      parts.push(token.raw);
    }
  }
  return parts.join('');
}

function parseImports(source, packageName) {
  const imports = new Map();
  const pattern = /import\s+(?:type\s+)?\{(?<names>[^}]+)\}\s+from\s+['"](?<module>[^'"]+)['"]\s*;/gu;
  for (const match of source.matchAll(pattern)) {
    let namespace = packageName.replaceAll('-', '_');
    if (match.groups.module.startsWith('@flighthq/')) {
      namespace = match.groups.module.split('/')[1].replaceAll('-', '_');
    }
    for (const part of match.groups.names.split(',')) {
      const [imported, alias] = part.trim().split(/\s+as\s+/u);
      if (!imported) continue;
      imports.set(alias ?? imported, `flight::${namespace}::${snake(imported)}`);
    }
  }
  return imports;
}

function headerFor(file) {
  const stem = file.sourceName.slice(0, -'.test.ts'.length);
  return `flight/${file.packageName.replaceAll('-', '_')}/${snake(stem)}.hpp`;
}

function splitArguments(tokens) {
  if (tokens.length === 0) return [];
  const result = [];
  let start = 0;
  const stack = [];
  const pairs = new Map([
    ['(', ')'],
    ['[', ']'],
    ['{', '}'],
  ]);
  for (let index = 0; index < tokens.length; index += 1) {
    const value = tokens[index].value;
    if (pairs.has(value)) stack.push(pairs.get(value));
    else if (stack.at(-1) === value) stack.pop();
    else if (value === ',' && stack.length === 0) {
      result.push(tokens.slice(start, index));
      start = index + 1;
    }
  }
  result.push(tokens.slice(start));
  return result;
}

function topLevelIndex(tokens, wanted) {
  const stack = [];
  const pairs = new Map([
    ['(', ')'],
    ['[', ']'],
    ['{', '}'],
  ]);
  for (let index = 0; index < tokens.length; index += 1) {
    const value = tokens[index].value;
    if (pairs.has(value)) stack.push(pairs.get(value));
    else if (stack.at(-1) === value) stack.pop();
    else if (value === wanted && stack.length === 0) return index;
  }
  return -1;
}

function matchingToken(tokens, start, open, close) {
  let depth = 0;
  for (let index = start; index < tokens.length; index += 1) {
    if (tokens[index].value === open) depth += 1;
    else if (tokens[index].value === close) {
      depth -= 1;
      if (depth === 0) return index;
    }
  }
  return undefined;
}

function tokenize(source) {
  const tokens = [];
  let index = 0;
  let line = 1;
  while (index < source.length) {
    const character = source[index];
    if (/\s/u.test(character)) {
      if (character === '\n') line += 1;
      index += 1;
      continue;
    }
    if (source.startsWith('//', index)) {
      const newline = source.indexOf('\n', index + 2);
      index = newline === -1 ? source.length : newline;
      continue;
    }
    if (source.startsWith('/*', index)) {
      const end = source.indexOf('*/', index + 2);
      const next = end === -1 ? source.length : end + 2;
      line += source.slice(index, next).split('\n').length - 1;
      index = next;
      continue;
    }
    if (character === "'" || character === '"' || character === '`') {
      const start = index;
      const startLine = line;
      const quote = character;
      index += 1;
      while (index < source.length) {
        if (source[index] === '\\') {
          index += 2;
          continue;
        }
        if (source[index] === '\n') line += 1;
        if (source[index] === quote) {
          index += 1;
          break;
        }
        index += 1;
      }
      const raw = source.slice(start, index);
      tokens.push({ kind: quote === '`' ? 'template' : 'string', line: startLine, raw, value: raw });
      continue;
    }
    const identifier = /^[A-Za-z_$][A-Za-z0-9_$]*/u.exec(source.slice(index));
    if (identifier !== null) {
      tokens.push({ kind: 'identifier', line, raw: identifier[0], value: identifier[0] });
      index += identifier[0].length;
      continue;
    }
    const number = /^(?:0[xob][0-9a-f]+|(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?)/iu.exec(source.slice(index));
    if (number !== null) {
      tokens.push({ kind: 'number', line, raw: number[0], value: number[0] });
      index += number[0].length;
      continue;
    }
    const operator = ['===', '!==', '>>>', '**=', '=>', '==', '!=', '<=', '>=', '++', '--', '&&', '||', '??', '?.', '**', '+=', '-=', '*=', '/=', '%=', '<<', '>>'].find(
      (candidate) => source.startsWith(candidate, index),
    );
    const raw = operator ?? character;
    tokens.push({ kind: 'punctuation', line, raw, value: raw });
    index += raw.length;
  }
  return tokens;
}

function decodeString(raw) {
  return raw
    .slice(1, -1)
    .replaceAll('\\n', '\n')
    .replaceAll('\\r', '\r')
    .replaceAll('\\t', '\t')
    .replaceAll(`\\${raw[0]}`, raw[0])
    .replaceAll('\\\\', '\\');
}

function snake(value) {
  return value
    .replace(/([a-z0-9])([A-Z])/gu, '$1_$2')
    .replace(/[^A-Za-z0-9]+/gu, '_')
    .replace(/^_+|_+$/gu, '')
    .toLowerCase();
}

function cppString(value) {
  return JSON.stringify(value).replaceAll('\\u2028', '\\u2028').replaceAll('\\u2029', '\\u2029');
}

function sha256(value) {
  return createHash('sha256').update(value).digest('hex');
}

function fraction(numerator, denominator) {
  return denominator === 0 ? '0/0 (0.0%)' : `${String(numerator)}/${String(denominator)} (${((100 * numerator) / denominator).toFixed(1)}%)`;
}

function portable(filename) {
  return filename.split(path.sep).join('/');
}

function compare(filename, expected) {
  if (!existsSync(filename) || readFileSync(filename, 'utf8') !== expected) {
    process.stderr.write(`${portable(path.relative(root, filename))} is stale or missing.\n`);
    return true;
  }
  return false;
}
