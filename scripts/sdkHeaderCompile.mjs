import { spawn, spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, readdirSync, readFileSync, renameSync, writeFileSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Compiles each generated SDK header on its own and records what happened.
//
// The full sweep is the FOLDED-TREE gate: it is what you run once, at the end, over a complete
// generated tree. It is not the loop to develop against — a compiler builder wants a focused check
// over the handful of headers a change touches, which is what `--headers` is for.
//
// A sweep over a large corpus takes long enough that it will sometimes be interrupted, and an
// interrupted run that reports nothing wastes all of it. So the run checkpoints: the report is
// rewritten atomically every few seconds and again on the way out, and it always distinguishes
// headers that were ATTEMPTED and passed, attempted and failed, and never attempted at all. A run
// stopped by a deadline or a signal leaves a report that says exactly that, and `--resume` picks up
// the unattempted remainder rather than starting over.
//
//   --generated=DIR            the generated tree to compile (default: generated/)
//   --report=FILE              where the report is written
//   --headers=a.hpp,b.hpp      compile only these, or --headers=@FILE to read them one per line
//   --resume                   skip what the existing report already attempted
//   --deadline-seconds=N       stop starting new work after N seconds and report the remainder
//   --checkpoint-seconds=N     how often the report is flushed while running (default 10)
//
//   CXX, CXXFLAGS, FLIGHT_CPP_COMPILE_JOBS

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const options = process.argv.slice(2);
const valued = ['--generated=', '--report=', '--headers=', '--deadline-seconds=', '--checkpoint-seconds='];
const flags = ['--resume'];
const unknown = options.filter(
  (option) => !valued.some((name) => option.startsWith(name)) && !flags.includes(option),
);
if (unknown.length > 0) {
  process.stderr.write(`Unknown SDK header compile option(s): ${unknown.join(', ')}\n`);
  process.exit(1);
}
const valueOf = (name) => {
  const hit = options.find((option) => option.startsWith(name));
  return hit === undefined ? undefined : hit.slice(name.length);
};
const numberOf = (name, fallback) => {
  const raw = valueOf(name);
  if (raw === undefined) return fallback;
  const parsed = Number.parseFloat(raw);
  if (!Number.isFinite(parsed) || parsed <= 0) {
    process.stderr.write(`${name} needs a positive number.\n`);
    process.exit(1);
  }
  return parsed;
};

const compiler = process.env.CXX || 'c++';
const extraFlags = process.env.CXXFLAGS?.split(/\s+/u).filter(Boolean) ?? [];
const concurrency = Math.max(
  1,
  Number.parseInt(process.env.FLIGHT_CPP_COMPILE_JOBS ?? '', 10) || Math.min(6, os.availableParallelism()),
);
const generatedRoot = path.resolve(root, valueOf('--generated=') ?? 'generated');
const generatedInclude = path.join(generatedRoot, 'include');
const runtimeInclude = path.join(root, 'include');
const reportFile = path.resolve(root, valueOf('--report=') ?? path.join('out', 'sdk-header-compilation.json'));
const resume = options.includes('--resume');
const deadlineSeconds = numberOf('--deadline-seconds=', undefined);
const checkpointSeconds = numberOf('--checkpoint-seconds=', 10);
const manifest = JSON.parse(readFileSync(path.join(generatedRoot, 'manifest.json'), 'utf8'));

const everyHeader = filesUnder(generatedInclude)
  .filter((filename) => filename.endsWith('.hpp') && !filename.includes(`${path.sep}sdk${path.sep}`))
  .map((filename) => portable(path.relative(generatedInclude, filename)))
  .sort();

const selected = selectHeaders();
const previous = resume ? readPreviousReport() : new Map();
const done = new Map();
for (const header of selected) {
  const earlier = previous.get(header);
  if (earlier) done.set(header, earlier);
}
const pending = selected.filter((header) => !done.has(header));

const version = spawnSync(compiler, ['--version'], { encoding: 'utf8' });
if (version.status !== 0) {
  process.stderr.write(`Unable to run C++ compiler ${compiler}. Set CXX to a C++20 compiler.\n`);
  process.exit(1);
}
const compilerVersion = `${version.stdout}${version.stderr}`;
const diagnosticLimit = compilerVersion.toLowerCase().includes('clang') ? '-ferror-limit=8' : '-fmax-errors=8';

if (resume && done.size > 0) {
  process.stdout.write(
    `Resuming: ${String(done.size)} of ${String(selected.length)} headers already attempted, ${String(pending.length)} to go.\n`,
  );
}

const startedAt = Date.now();
const deadlineAt = deadlineSeconds === undefined ? undefined : startedAt + deadlineSeconds * 1000;
let next = 0;
let attemptedHere = 0;
let stopping = false;
let lastCheckpoint = startedAt;
let lastProgress = 0;

for (const signal of ['SIGINT', 'SIGTERM']) {
  process.on(signal, () => {
    stopping = true;
  });
}

await Promise.all(Array.from({ length: Math.min(concurrency, Math.max(pending.length, 1)) }, worker));
const complete = writeReport();

const passed = [...done.values()].filter((result) => result.passed).length;
const failures = [...done.values()].filter((result) => !result.passed);
const unattempted = selected.length - done.size;
const summary =
  `${String(passed)}/${String(selected.length)} generated Flight SDK headers compile independently with ${compilerVersion.split(/\r?\n/u)[0]}`;
if (!complete) {
  process.stderr.write(
    `${summary}; ${String(failures.length)} fail and ${String(unattempted)} were not attempted. ` +
      `Run again with --resume to finish. Report: ${portable(path.relative(root, reportFile))}\n`,
  );
  process.exitCode = 2;
} else if (failures.length === 0) {
  process.stdout.write(`${summary}.\n`);
} else {
  process.stderr.write(
    `${summary}; ${String(failures.length)} fail. Report: ${portable(path.relative(root, reportFile))}\n`,
  );
  for (const failure of failures.slice(0, 20)) {
    process.stderr.write(`- ${failure.header}: ${failure.diagnostic}\n`);
  }
  if (failures.length > 20) process.stderr.write(`- … and ${String(failures.length - 20)} more\n`);
  process.exitCode = 1;
}

function selectHeaders() {
  const raw = valueOf('--headers=');
  if (raw === undefined) return everyHeader;
  const names = (raw.startsWith('@') ? readFileSync(path.resolve(root, raw.slice(1)), 'utf8').split(/\r?\n/u) : raw.split(','))
    .map((name) => portable(name.trim()))
    .filter(Boolean);
  const known = new Set(everyHeader);
  const missing = names.filter((name) => !known.has(name));
  if (missing.length > 0) {
    process.stderr.write(`Not headers of ${portable(path.relative(root, generatedRoot))}: ${missing.join(', ')}\n`);
    process.exit(1);
  }
  return [...new Set(names)].sort();
}

function readPreviousReport() {
  const recorded = new Map();
  if (!existsSync(reportFile)) return recorded;
  let report;
  try {
    report = JSON.parse(readFileSync(reportFile, 'utf8'));
  } catch {
    process.stderr.write(`Existing report ${portable(path.relative(root, reportFile))} is not readable JSON; starting over.\n`);
    return recorded;
  }
  // A report from an older run records only its failures, so every header it does not name has to
  // be re-attempted: "passed" and "never tried" are the same absence there, and assuming the
  // friendlier one would silently report a pass nobody observed.
  if (!Array.isArray(report.passed)) {
    process.stderr.write('Existing report predates resumable runs and lists no passes; starting over.\n');
    return recorded;
  }
  for (const header of report.passed) recorded.set(header, { diagnostic: '', header, passed: true });
  for (const failure of report.failures ?? []) {
    recorded.set(failure.header, { diagnostic: failure.diagnostic, header: failure.header, passed: false });
  }
  return recorded;
}

function writeReport() {
  const attempted = [...done.values()].sort((left, right) => left.header.localeCompare(right.header));
  const failures = attempted.filter((result) => !result.passed);
  const passedHeaders = attempted.filter((result) => result.passed).map((result) => result.header);
  const unattemptedHeaders = selected.filter((header) => !done.has(header));
  const isComplete = unattemptedHeaders.length === 0;
  const report = {
    schema: 'flight-sdk-header-compilation/2',
    compiler: { command: compiler, version: compilerVersion.split(/\r?\n/u)[0] },
    generated: {
      compilerRevision: manifest.compiler.revision,
      sourceRevision: manifest.source.revision,
    },
    run: {
      complete: isComplete,
      elapsedSeconds: Math.round((Date.now() - startedAt) / 100) / 10,
      headerSelection: valueOf('--headers=') === undefined ? 'all' : 'explicit',
      jobs: concurrency,
    },
    summary: {
      attemptedHeaders: attempted.length,
      failedHeaders: failures.length,
      passedHeaders: passedHeaders.length,
      totalHeaders: selected.length,
      unattemptedHeaders: unattemptedHeaders.length,
    },
    failures: failures.map(({ header, diagnostic }) => ({ diagnostic, header })),
    passed: passedHeaders,
    unattempted: unattemptedHeaders,
  };
  mkdirSync(path.dirname(reportFile), { recursive: true });
  // Rewritten through a temporary in the same directory so a reader never sees a half-written
  // report, and so an interrupted write cannot destroy the one already there.
  const temporary = `${reportFile}.partial`;
  writeFileSync(temporary, `${JSON.stringify(report, undefined, 2)}\n`);
  renameSync(temporary, reportFile);
  return isComplete;
}

async function worker() {
  while (next < pending.length && !stopping) {
    if (deadlineAt !== undefined && Date.now() >= deadlineAt) {
      stopping = true;
      break;
    }
    const header = pending[next++];
    done.set(header, await compileHeader(header));
    attemptedHere++;
    reportProgress();
  }
}

function reportProgress() {
  const now = Date.now();
  if (now - lastCheckpoint >= checkpointSeconds * 1000) {
    lastCheckpoint = now;
    writeReport();
  }
  if (attemptedHere - lastProgress < 100 && attemptedHere !== pending.length) return;
  lastProgress = attemptedHere;
  const elapsed = (now - startedAt) / 1000;
  const rate = elapsed > 0 ? attemptedHere / elapsed : 0;
  const remaining = pending.length - attemptedHere;
  const eta = rate > 0 ? remaining / rate : 0;
  process.stdout.write(
    `Compiled ${String(attemptedHere)}/${String(pending.length)} headers — ${duration(elapsed)} elapsed, ` +
      `${rate.toFixed(1)}/s, ETA ${duration(eta)}.\n`,
  );
}

function duration(seconds) {
  const whole = Math.max(0, Math.round(seconds));
  const minutes = Math.floor(whole / 60);
  return minutes > 0 ? `${String(minutes)}m${String(whole % 60).padStart(2, '0')}s` : `${String(whole)}s`;
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
