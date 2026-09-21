import { spawnSync } from 'node:child_process';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (number toFixed oracle skipped).\n`);
  process.exit(0);
}

const belowCutoff = adjacentDouble(1e21, -1);
const aboveCutoff = adjacentDouble(1e21, 1);
const cases = [
  { name: 'default digits', value: 12.75 },
  { digits: 0, name: 'zero digits', value: 1.25 },
  { digits: 2.9, name: 'fractional digits', value: 1.25 },
  { digits: -0.9, name: 'negative fractional digits normalize to negative zero', value: 1.25 },
  { digits: Number.NaN, name: 'NaN digits normalize to zero', value: 1.25 },
  { digits: 100, name: 'exact upper digit bound', value: 1.25 },
  { digits: 100.9, name: 'fractional upper digit bound', value: 1.25 },
  { digits: -1, name: 'negative digits reject', value: 1.25 },
  { digits: 101, name: 'digits above upper bound reject', value: 1.25 },
  { digits: Number.POSITIVE_INFINITY, name: 'positive infinite digits reject', value: 1.25 },
  { digits: Number.NEGATIVE_INFINITY, name: 'negative infinite digits reject', value: 1.25 },
  { digits: 2, name: 'NaN value', value: Number.NaN },
  { digits: 2, name: 'positive infinity', value: Number.POSITIVE_INFINITY },
  { digits: 2, name: 'negative infinity', value: Number.NEGATIVE_INFINITY },
  { digits: 2, name: 'below 1e21 cutoff', value: belowCutoff },
  { digits: 2, name: 'at 1e21 cutoff', value: 1e21 },
  { digits: 2, name: 'above 1e21 cutoff', value: aboveCutoff },
  { digits: 0, name: 'half away from zero', value: 2.5 },
  { digits: 2, name: 'inexact 1.005', value: 1.005 },
  { digits: 0, name: 'positive half', value: 0.5 },
  { digits: 1, name: 'inexact 1.45', value: 1.45 },
  { digits: 2, name: 'inexact 8.575', value: 8.575 },
  { digits: 2, name: 'negative zero', value: -0 },
  { digits: 2, name: 'negative value rounding to zero', value: -0.0001 },
  { digits: 0, name: 'large exact expansion below cutoff', value: 999999999999999900000 },
  { digits: 0, name: 'large binary boundary', value: 1000000000000000100 },
  { digits: 4, name: 'large fractional request below cutoff', value: 123456789012345680000 },
  { digits: 101, name: 'digit validation precedes infinity formatting', value: Number.POSITIVE_INFINITY },
];

const expected = cases.map(observeWithNode);
const nativeCases = cases.map(({ value, digits }) => {
  const hasDigits = digits === undefined ? 'false' : 'true';
  return `  {0x${doubleBits(value)}ULL, ${hasDigits}, 0x${doubleBits(digits ?? 0)}ULL},`;
}).join('\n');

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-number-to-fixed-'));
try {
  const source = path.join(temporary, 'oracle.cpp');
  const executable = path.join(temporary, process.platform === 'win32' ? 'oracle.exe' : 'oracle');
  writeFileSync(
    source,
    `#include <flight/number.hpp>

#include <bit>
#include <cstdint>
#include <iostream>
#include <stdexcept>

struct FixedCase final {
  std::uint64_t value_bits;
  bool has_digits;
  std::uint64_t digits_bits;
};

int main() {
  static constexpr FixedCase cases[]{
${nativeCases}
  };
  for (const auto& entry : cases) {
    const double value = std::bit_cast<double>(entry.value_bits);
    const double digits = std::bit_cast<double>(entry.digits_bits);
    try {
      const auto result = entry.has_digits
                              ? flight::number_to_fixed(value, digits)
                              : flight::number_to_fixed(value);
      std::cout << "ok:" << result.to_utf8() << '\\n';
    } catch (const std::range_error&) {
      std::cout << "range\\n";
    }
  }
}
`,
  );
  const compilation = spawnSync(
    cppCompiler,
    ['-std=c++20', '-I', path.join(root, 'include'), source, '-o', executable],
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
  const actual = native.stdout.replaceAll('\r\n', '\n').trimEnd().split('\n');
  const failures = [];
  for (let index = 0; index < cases.length; ++index) {
    if (actual[index] !== expected[index]) {
      failures.push(`${cases[index].name}: expected ${expected[index]}, actual ${actual[index] ?? '<missing>'}`);
    }
  }
  if (actual.length !== expected.length) {
    failures.push(`expected ${expected.length} observations, received ${actual.length}`);
  }
  if (failures.length > 0) {
    process.stderr.write(`Number.toFixed differential failed:\n${failures.join('\n')}\n`);
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `number_to_fixed matches Node across ${cases.length} normalization, cutoff, rounding, and sign cases (${cppCompiler}).\n`,
);

function observeWithNode({ value, digits }) {
  try {
    return `ok:${digits === undefined ? value.toFixed() : value.toFixed(digits)}`;
  } catch (error) {
    if (error instanceof RangeError) return 'range';
    throw error;
  }
}

function doubleBits(value) {
  const bytes = new ArrayBuffer(8);
  const view = new DataView(bytes);
  view.setFloat64(0, value);
  return view.getBigUint64(0).toString(16).padStart(16, '0');
}

function adjacentDouble(value, direction) {
  const bytes = new ArrayBuffer(8);
  const view = new DataView(bytes);
  view.setFloat64(0, value);
  const bits = view.getBigUint64(0);
  view.setBigUint64(0, direction < 0 ? bits - 1n : bits + 1n);
  return view.getFloat64(0);
}
