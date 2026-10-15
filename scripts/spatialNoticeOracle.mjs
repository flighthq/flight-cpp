import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// `npm run spatial-notice:oracle` checks the formatSpatialIndexingNotice override against the pinned
// TypeScript source it supplies, not against a copy of its text.
//
// Node runs the function from `.dependencies/flight` after three exact, counted rewrites that erase only
// what JavaScript cannot parse: the type-only import, the MAX_INDEXED_CELLS_PER_OBJECT import (replaced by
// the constant read from uniformGrid.ts), and the signature's type annotations. Each rewrite must match
// exactly once, so a source change the rewrite does not understand fails here rather than testing
// something else. The native side is compiled against overrides/include ahead of generated/include, the
// same order every build uses. Like the other gates, it reports and skips when the pinned checkout or a
// compiler is absent.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const spatialSource = path.join(root, '.dependencies', 'flight', 'packages', 'spatial', 'src');
if (!existsSync(path.join(spatialSource, 'formatSpatialIndexingNotice.ts'))) {
  process.stdout.write('spatial notice oracle skipped: the pinned flight checkout is absent. Run `npm run rehydrate`.\n');
  process.exit(0);
}
const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (spatial notice oracle skipped).\n`);
  process.exit(0);
}

const cases = [];
for (const operation of ['insert', 'update', 'remove']) {
  for (const reason of [null, 'invalid-cell-size', 'inverted-bounds', 'missing-id', 'unrecognized-reason']) {
    for (const mode of ['cells', 'overflow', 'declined', 'absent']) {
      cases.push({ cellSize: 128, id: 7, mode, operation, reason, wouldOccupyBucketCount: 2048 });
    }
  }
}
for (const value of [Number.NaN, Infinity, -Infinity, -0, 0.1 + 0.2, 1e21, 1e-7, 123456789.125, -2.5]) {
  cases.push({ cellSize: value, id: 7, mode: 'overflow', operation: 'insert', reason: 'invalid-cell-size', wouldOccupyBucketCount: 0 });
  cases.push({ cellSize: 1, id: value, mode: 'cells', operation: 'update', reason: null, wouldOccupyBucketCount: 0 });
  cases.push({ cellSize: 1, id: 3, mode: 'overflow', operation: 'remove', reason: null, wouldOccupyBucketCount: value });
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-spatial-notice-'));
try {
  const formatSpatialIndexingNotice = await loadPinnedFormatter(temporary);
  const expected = cases.map((notice) => formatSpatialIndexingNotice(notice));

  const source = path.join(temporary, 'oracle.cpp');
  const executable = path.join(temporary, process.platform === 'win32' ? 'oracle.exe' : 'oracle');
  const nativeCases = cases
    .map(
      (notice) =>
        `  {0x${doubleBits(notice.cellSize)}ULL, 0x${doubleBits(notice.id)}ULL, "${notice.mode}", "${notice.operation}", ` +
        `${notice.reason === null ? 'nullptr' : `"${notice.reason}"`}, 0x${doubleBits(notice.wouldOccupyBucketCount)}ULL},`,
    )
    .join('\n');
  writeFileSync(
    source,
    `#include <flight/spatial/format_spatial_indexing_notice.hpp>

#include <bit>
#include <cstdint>
#include <iostream>
#include <optional>

struct NoticeCase final {
  std::uint64_t cell_size;
  std::uint64_t id;
  const char* mode;
  const char* operation;
  const char* reason;
  std::uint64_t would_occupy_bucket_count;
};

int main() {
  static constexpr NoticeCase cases[]{
${nativeCases}
  };
  for (const auto& entry : cases) {
    auto notice = flight::make_ref<flight::types::SpatialIndexingNotice>();
    notice->cell_size = std::bit_cast<double>(entry.cell_size);
    notice->id = std::bit_cast<double>(entry.id);
    notice->mode = flight::String(entry.mode);
    notice->operation = flight::String(entry.operation);
    notice->reason = entry.reason ? std::optional<flight::String>(flight::String(entry.reason)) : std::nullopt;
    notice->would_occupy_bucket_count = std::bit_cast<double>(entry.would_occupy_bucket_count);
    std::cout << flight::spatial::format_spatial_indexing_notice(notice).to_utf8() << '\\n';
  }
}
`,
  );
  const compilation = spawnSync(
    cppCompiler,
    [
      '-std=c++20',
      '-I', path.join(root, 'overrides', 'include'),
      '-I', path.join(root, 'generated', 'include'),
      '-I', path.join(root, 'include'),
      source,
      '-o', executable,
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
  const actual = native.stdout.replaceAll('\r\n', '\n').trimEnd().split('\n');
  const failures = [];
  for (let index = 0; index < cases.length; ++index) {
    if (actual[index] !== expected[index]) {
      failures.push(`${JSON.stringify(cases[index])}:\n  expected ${expected[index]}\n  actual   ${actual[index] ?? '<missing>'}`);
    }
  }
  if (actual.length !== expected.length) failures.push(`expected ${expected.length} lines, received ${actual.length}`);
  if (failures.length > 0) {
    process.stderr.write(`formatSpatialIndexingNotice differential failed:\n${failures.join('\n')}\n`);
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `format_spatial_indexing_notice matches the pinned TypeScript across ${cases.length} reason, mode, operation, and number-rendering cases (${cppCompiler}).\n`,
);

async function loadPinnedFormatter(directory) {
  const grid = readFileSync(path.join(spatialSource, 'uniformGrid.ts'), 'utf8');
  const constant = grid.match(/^export const MAX_INDEXED_CELLS_PER_OBJECT = (\d+);$/m);
  if (!constant) throw new Error('uniformGrid.ts no longer declares MAX_INDEXED_CELLS_PER_OBJECT as an integer literal');
  let text = readFileSync(path.join(spatialSource, 'formatSpatialIndexingNotice.ts'), 'utf8');
  text = rewriteOnce(text, "import type { SpatialIndexingNotice } from '@flighthq/types/contract';\n", '');
  text = rewriteOnce(
    text,
    "import { MAX_INDEXED_CELLS_PER_OBJECT } from './uniformGrid.ts';\n",
    `const MAX_INDEXED_CELLS_PER_OBJECT = ${constant[1]};\n`,
  );
  text = rewriteOnce(
    text,
    'formatSpatialIndexingNotice(notice: Readonly<SpatialIndexingNotice>): string {',
    'formatSpatialIndexingNotice(notice) {',
  );
  const module = path.join(directory, 'formatSpatialIndexingNotice.mjs');
  writeFileSync(module, text);
  return (await import(pathToFileURL(module).href)).formatSpatialIndexingNotice;
}

function rewriteOnce(text, from, to) {
  const count = text.split(from).length - 1;
  if (count !== 1) throw new Error(`expected exactly one occurrence of ${JSON.stringify(from)} in the pinned source, found ${count}`);
  return text.replace(from, to);
}

function doubleBits(value) {
  const view = new DataView(new ArrayBuffer(8));
  view.setFloat64(0, value);
  return view.getBigUint64(0).toString(16).padStart(16, '0');
}
