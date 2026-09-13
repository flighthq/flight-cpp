import { mkdirSync } from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const compiler = process.env.CXX || 'c++';
const probe = spawnSync(compiler, ['--version'], { encoding: 'utf8' });
if (probe.status !== 0) {
  process.stdout.write(`No C++ compiler available as ${compiler} (runtime oracle skipped).\n`);
  process.exit(0);
}

const outputDirectory = path.join(root, 'out', 'runtime-oracle');
const executable = path.join(outputDirectory, process.platform === 'win32' ? 'runtime-oracle.exe' : 'runtime-oracle');
mkdirSync(outputDirectory, { recursive: true });
const compilation = spawnSync(
  compiler,
  [
    '-std=c++20',
    '-pthread',
    '-I',
    path.join(root, 'include'),
    path.join(root, 'conformance', 'runtime-oracle.cpp'),
    '-o',
    executable,
  ],
  { cwd: root, encoding: 'utf8' },
);
if (compilation.status !== 0) {
  process.stderr.write(`${compilation.stdout}${compilation.stderr}`);
  process.exit(1);
}

const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
if (native.status !== 0) {
  process.stderr.write(`${native.stdout}${native.stderr}`);
  process.exit(1);
}

const bytes = new Uint8Array(8);
const dataView = new DataView(bytes.buffer);
dataView.setUint32(1, 0x12345678, true);
const values = [1, 4];
const removed = values.splice(1, 0, 2, 3);
const expression = /^flight$/gi;
const target = { first: 0, retained: 3 };
const source = { first: 1, second: 2 };
const recordSymbol = Symbol.for('runtime-oracle-record');
const record = {
  later: 1,
  10: 10,
  2: 2,
  '01': 1,
  [-0]: 0,
  4294967294: 4,
  4294967295: 5,
  [recordSymbol]: 7,
  after: 8,
};
record['2'] = 22;
const expected = JSON.stringify([
  dataView.getUint32(1, true),
  new TextDecoder().decode(new Uint8Array([0xe0, 0x80, 0x80])),
  String.fromCodePoint(0x41, 0x1f600),
  values,
  removed.length,
  expression.test('FLIGHT'),
  expression.test('flight'),
  expression.test('flight'),
  new URL('child', 'https://example.test/base').protocol,
  Number.parseInt('  -0x10tail'),
  Number('0b101'),
  Number('-0x10'),
  Object.entries(Object.assign(target, source)),
  Object.keys(record),
  Object.entries(record),
  record.missing === undefined && Object.keys(record).length === 8,
  JSON.stringify(JSON.parse('{"name":"Flight","values":[null,-1.5e2,"\\ud83d\\ude00"]}')),
  new Intl.ListFormat('en', { style: 'long', type: 'conjunction' }).format(['a', 'b', 'c']),
  new Intl.PluralRules('en').select(1),
  new Intl.RelativeTimeFormat('en', { numeric: 'always' }).format(-2, 'day'),
  Math.sign(new Intl.Collator('en').compare('a', 'b')),
]);
const actual = native.stdout.trim();
if (actual !== expected) {
  process.stderr.write(`Runtime source differential failed.\nExpected: ${expected}\nActual:   ${actual}\n`);
  process.exit(1);
}

process.stdout.write(`Runtime source differential matches ${JSON.parse(actual).length} observations (${compiler}).\n`);
