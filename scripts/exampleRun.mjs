import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Builds and RUNS the portable examples.
//
// The example headers are checked against compiler output by exampleHealth, and the CMake presets
// register the same binaries as CTest tests. Neither of those is reached by `npm run check`, so an
// example that compiles and then crashes at runtime could pass every gate here -- and one did:
// sdk_math dereferenced the null reference a finished entity used to cast to. This gate runs the
// binary, because "it builds" is not the claim an example makes.
//
// Portable means exactly that: dependency-free, one translation unit, the installed header tree
// plus the generated SDK inventory. The SDL examples need a host toolchain and stay with CTest.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const examples = [
  { name: 'tween', source: path.join('examples', 'tween', 'tween_example.cpp') },
  { name: 'sdk_math', source: path.join('examples', 'sdk_math_example.cpp') },
];

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (portable example run skipped).\n`);
  process.exit(0);
}

const generatedInclude = path.join(root, 'generated', 'include');
if (!existsSync(generatedInclude)) {
  process.stdout.write('The generated SDK inventory is absent (portable example run skipped).\n');
  process.exit(0);
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-example-run-'));
let failed = false;
try {
  for (const example of examples) {
    const executable = path.join(temporary, process.platform === 'win32' ? `${example.name}.exe` : example.name);
    const compilation = spawnSync(
      cppCompiler,
      [
        '-std=c++20',
        '-pthread',
        '-Wall',
        '-Wextra',
        '-Wpedantic',
        '-Werror',
        '-I',
        path.join(root, 'include'),
        '-I',
        generatedInclude,
        path.join(root, example.source),
        '-o',
        executable,
      ],
      { cwd: root, encoding: 'utf8' },
    );
    if (compilation.status !== 0) {
      process.stderr.write(`${compilation.stdout ?? ''}${compilation.stderr ?? ''}`);
      process.stderr.write(`The ${example.name} example does not compile.\n`);
      failed = true;
      continue;
    }
    const execution = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
    if (execution.status !== 0) {
      process.stderr.write(`${execution.stdout ?? ''}${execution.stderr ?? ''}`);
      const reason =
        execution.signal === null
          ? `exited with status ${String(execution.status)}`
          : `was killed by ${execution.signal}`;
      process.stderr.write(`The ${example.name} example ${reason}.\n`);
      failed = true;
    }
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

if (failed) process.exit(1);
process.stdout.write(`${String(examples.length)} portable examples build and run (${cppCompiler}).\n`);
