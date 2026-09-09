import { execFileSync, spawnSync } from 'node:child_process';
import { existsSync, readdirSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { resolveDependency } from './dependencyLock.mjs';

// Does the pinned compiler's C++ still compile against this runtime?
//
// The compiler repository asks the same question from its side, against the runtime revision it
// pins. Both gates are wanted: each one owns the pin it can move, so a failure names which side
// changed. This is the lane that catches a runtime edit that quietly breaks emitted code, because
// here the runtime is the working tree and the compiler is the fixed point.
//
// The corpus is the pinned checkout's committed golden output rather than a live compile: those
// files are what that revision of the compiler emits, proven by its own `golden:check`, and reading
// them keeps this gate free of a Node build of the compiler.
//
// A missing checkout or absent toolchain is reported and skipped rather than failed, so the gate
// stays runnable on a machine that has neither.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const compiler = resolveDependency(root, 'flight-compiler');
const goldenDirectory = path.join(compiler.directory, 'golden');
const includeDirectory = path.join(root, 'include');
const failures = [];
let checked = 0;

if (!existsSync(goldenDirectory)) {
  process.stdout.write('flight-compiler is not rehydrated (skipped); run `npm run rehydrate`.\n');
  process.exit(0);
}

const cppCompiler = ['c++', 'g++', 'clang++'].find((command) => hasCommand(command, ['--version']));
if (!cppCompiler) {
  process.stdout.write('No C++ compiler installed (skipped).\n');
  process.exit(0);
}

const fixtures = readdirSync(goldenDirectory, { withFileTypes: true })
  .filter((entry) => entry.isDirectory() && existsSync(path.join(goldenDirectory, entry.name, 'cpp')))
  .map((entry) => entry.name)
  .sort();

for (const fixture of fixtures) {
  const emitted = path.join(goldenDirectory, fixture, 'cpp');
  for (const header of collectSourceFiles(emitted, '.hpp')) {
    checked += 1;
    // Emission is one module at a time by design, so a fixture's siblings come from the pinned
    // checkout's own support tree. Those stubs stand in for downstream implementations; the runtime
    // contract itself is this repository's `include/`, which is the whole point of the gate.
    const result = spawnSync(
      cppCompiler,
      [
        '-std=c++20',
        '-fsyntax-only',
        '-pthread',
        '-I',
        includeDirectory,
        '-I',
        path.join(goldenDirectory, 'support', 'cpp'),
        '-x',
        'c++-header',
        path.join(emitted, header),
      ],
      { cwd: emitted, encoding: 'utf8' },
    );
    if (result.status !== 0) {
      failures.push({ fixture: `${fixture}/${header}`, output: `${result.stdout ?? ''}${result.stderr ?? ''}`.trim() });
    }
  }
}

if (failures.length > 0) {
  for (const failure of failures) process.stderr.write(`\n### ${failure.fixture}\n${failure.output}\n`);
  process.stderr.write(
    `\n${String(failures.length)} emitted file(s) from flight-compiler ${compiler.commit.slice(0, 7)} do not compile against this runtime.\n`,
  );
  process.exit(1);
}

process.stdout.write(
  `Emitted C++ from flight-compiler ${compiler.commit.slice(0, 7)} compiles: ${String(checked)} file(s) across ${String(fixtures.length)} fixture(s) (${cppCompiler}).\n`,
);

function collectSourceFiles(directory, extension) {
  return readdirSync(directory, { recursive: true, withFileTypes: true })
    .filter((entry) => entry.isFile() && entry.name.endsWith(extension))
    .map((entry) => path.relative(directory, path.join(entry.parentPath, entry.name)))
    .sort();
}

function hasCommand(command, argumentList) {
  try {
    execFileSync(command, [...argumentList], { stdio: 'ignore' });
    return true;
  } catch {
    return false;
  }
}
