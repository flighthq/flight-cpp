import { spawn, spawnSync } from 'node:child_process';
import { mkdirSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const compiler = process.env.CXX || 'c++';
const extraFlags = process.env.CXXFLAGS?.split(/\s+/u).filter(Boolean) ?? [];
const concurrency = Math.max(
  1,
  Number.parseInt(process.env.FLIGHT_CPP_COMPILE_JOBS ?? '', 10) || Math.min(6, os.availableParallelism()),
);
const generatedInclude = path.join(root, 'generated', 'include');
const runtimeInclude = path.join(root, 'include');
const reportFile = path.join(root, 'out', 'sdk-header-compilation.json');
const manifest = JSON.parse(readFileSync(path.join(root, 'generated', 'manifest.json'), 'utf8'));
const headers = filesUnder(generatedInclude)
  .filter((filename) => filename.endsWith('.hpp') && !filename.includes(`${path.sep}sdk${path.sep}`))
  .map((filename) => portable(path.relative(generatedInclude, filename)))
  .sort();

const version = spawnSync(compiler, ['--version'], { encoding: 'utf8' });
if (version.status !== 0) {
  process.stderr.write(`Unable to run C++ compiler ${compiler}. Set CXX to a C++20 compiler.\n`);
  process.exit(1);
}
const compilerVersion = `${version.stdout}${version.stderr}`;
const diagnosticLimit = compilerVersion.toLowerCase().includes('clang') ? '-ferror-limit=8' : '-fmax-errors=8';

let next = 0;
let completed = 0;
const results = [];
await Promise.all(Array.from({ length: Math.min(concurrency, headers.length) }, worker));
results.sort((left, right) => left.header.localeCompare(right.header));
const failures = results.filter((result) => !result.passed);
const report = {
  schema: 'flight-sdk-header-compilation/1',
  compiler: {
    command: compiler,
    version: compilerVersion.split(/\r?\n/u)[0],
  },
  generated: {
    compilerRevision: manifest.compiler.revision,
    sourceRevision: manifest.source.revision,
  },
  summary: {
    failedHeaders: failures.length,
    passedHeaders: headers.length - failures.length,
    totalHeaders: headers.length,
  },
  failures: failures.map(({ header, diagnostic }) => ({ diagnostic, header })),
};
mkdirSync(path.dirname(reportFile), { recursive: true });
writeFileSync(reportFile, `${JSON.stringify(report, undefined, 2)}\n`);

const summary = `${String(report.summary.passedHeaders)}/${String(headers.length)} generated Flight SDK headers compile independently with ${report.compiler.version}`;
if (failures.length === 0) {
  process.stdout.write(`${summary}.\n`);
} else {
  process.stderr.write(`${summary}; ${String(failures.length)} fail. Report: ${portable(path.relative(root, reportFile))}\n`);
  for (const failure of failures.slice(0, 20)) {
    process.stderr.write(`- ${failure.header}: ${failure.diagnostic}\n`);
  }
  if (failures.length > 20) process.stderr.write(`- … and ${String(failures.length - 20)} more\n`);
  process.exitCode = 1;
}

async function worker() {
  while (next < headers.length) {
    const index = next++;
    const header = headers[index];
    results.push(await compileHeader(header));
  }
}

function compileHeader(header) {
  return new Promise((resolve) => {
    const child = spawn(
      compiler,
      [
        ...extraFlags,
        '-std=c++20',
        diagnosticLimit,
        `-I${generatedInclude}`,
        `-I${runtimeInclude}`,
        '-x',
        'c++',
        '-fsyntax-only',
        '-',
      ],
      { stdio: ['pipe', 'ignore', 'pipe'] },
    );
    let stderr = '';
    child.stderr.setEncoding('utf8');
    child.stderr.on('data', (chunk) => {
      if (stderr.length < 64_000) stderr += chunk;
    });
    child.on('error', (error) => resolve({ diagnostic: error.message, header, passed: false }));
    child.on('close', (status) => {
      const diagnostic = stderr
        .split(/\r?\n/u)
        .find((line) => line.includes('error:'))
        ?.replace(/^.*?error:\s*/u, '') ?? `compiler exited with status ${String(status)}`;
      completed++;
      if (completed % 100 === 0 || completed === headers.length) {
        process.stdout.write(`Compiled ${String(completed)}/${String(headers.length)} generated headers.\n`);
      }
      resolve({ diagnostic, header, passed: status === 0 });
    });
    child.stdin.end(`#include <${header}>\n`);
  });
}

function filesUnder(directory) {
  return readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const filename = path.join(directory, entry.name);
    return entry.isDirectory() ? filesUnder(filename) : [filename];
  });
}

function portable(filename) {
  return filename.split(path.sep).join('/');
}
