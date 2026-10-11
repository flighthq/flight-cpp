import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

import { resolveDependency } from './dependencyLock.mjs';
import { applyEmissionRepairs, loadEmissionRepairs } from './emissionRepairs.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const options = process.argv.slice(2);
const update = options.includes('--update');
const unknown = options.filter((option) => option !== '--update');
if (unknown.length > 0) {
  process.stderr.write(`Unknown native example health option(s): ${unknown.join(', ')}\n`);
  process.exit(1);
}
const compiler = resolveDependency(root, 'flight-compiler');
const compilerManifest = path.join(compiler.directory, 'package.json');
const compilerTypeScript = path.join(compiler.directory, 'node_modules', 'typescript');
const examples = [
  { name: 'sound', packageName: '@flighthq/examples-sound' },
  { name: 'tween', packageName: '@flighthq/examples-tween' },
];
const emissionRepairs = loadEmissionRepairs(root);

if (!existsSync(compilerManifest)) {
  process.stdout.write('flight-compiler is not rehydrated (skipped); run `npm run rehydrate`.\n');
  process.exit(0);
}
if (!existsSync(compilerTypeScript)) {
  process.stdout.write(
    'flight-compiler dependencies are not installed (skipped); run `npm ci --prefix .dependencies/flight-compiler`.\n',
  );
  process.exit(0);
}
for (const example of examples) {
  const generatedHeader = path.join(root, 'examples', example.name, 'generated', `${example.name}.hpp`);
  if (!existsSync(generatedHeader)) {
    process.stderr.write(
      `The checked-in native ${example.name} header is missing; regenerate it with the command in examples/README.md.\n`,
    );
    process.exit(1);
  }
}

const temporaryDirectory = mkdtempSync(path.join(tmpdir(), 'flight-cpp-examples-'));
try {
  const npm = process.platform === 'win32' ? 'npm.cmd' : 'npm';
  for (const example of examples) {
    const sourceDirectory = path.join(root, 'examples', example.name, 'source');
    const outputDirectory = path.join(temporaryDirectory, example.name);
    const result = spawnSync(
      npm,
      [
        '--prefix',
        compiler.directory,
        'run',
        'compile',
        '--',
        sourceDirectory,
        '--target',
        'cpp',
        '--out',
        outputDirectory,
        '--package',
        example.packageName,
      ],
      { cwd: root, encoding: 'utf8' },
    );
    if (result.status !== 0) {
      process.stderr.write(`${result.stdout ?? ''}${result.stderr ?? ''}`);
      process.stderr.write(
        `flight-compiler ${compiler.commit.slice(0, 7)} could not regenerate the ${example.name} example.\n`,
      );
      process.exit(1);
    }

    const generatedHeader = path.join(root, 'examples', example.name, 'generated', `${example.name}.hpp`);
    const regeneratedHeader = path.join(outputDirectory, `${example.name}.hpp`);
    if (!existsSync(regeneratedHeader)) {
      process.stderr.write(
        `flight-compiler ${compiler.commit.slice(0, 7)} did not emit ${example.name}.hpp.\n`,
      );
      process.exit(1);
    }
    // The compiler still emits a few spellings that the runtime ABI cannot expose. Apply the same
    // declared, expiring repairs as the SDK generator; otherwise the reproducibility check demands
    // raw output that cannot compile (notably flight::power versus the flight::power package namespace).
    const candidate = {
      contents: readFileSync(regeneratedHeader, 'utf8'),
      path: `flight/examples/${example.name}/${example.name}.hpp`,
    };
    applyEmissionRepairs(emissionRepairs, [candidate]);
    if (candidate.contents !== readFileSync(generatedHeader, 'utf8')) {
      if (update) {
        writeFileSync(generatedHeader, candidate.contents);
        continue;
      }
      process.stderr.write(
        `The native ${example.name} example differs from repaired flight-compiler ${compiler.commit.slice(0, 7)} output; run \`npm run examples:portable:generate\`.\n`,
      );
      process.exit(1);
    }
  }

  process.stdout.write(
    `${String(examples.length)} native examples ${update ? 'updated from' : 'match'} repaired flight-compiler ${compiler.commit.slice(0, 7)} output.\n`,
  );
} finally {
  rmSync(temporaryDirectory, { force: true, recursive: true });
}
