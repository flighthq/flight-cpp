import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
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
  process.stdout.write('flight-compiler is not rehydrated (Web string types oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (Web string types oracle skipped).\n`);
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
const bindings = JSON.parse(readFileSync(path.join(root, 'bindings', 'web-types.json'), 'utf8'));
const names = [
  'CanvasFillRule',
  'GlobalCompositeOperation',
  'GPUFeatureName',
  'GPUIndexFormat',
  'GPUPowerPreference',
  'GPUTextureFormat',
  'ImageSmoothingQuality',
  'PermissionName',
];
const source = api.parseTypeScriptSource(
  '/flight/packages/host-test/src/webTypes.ts',
  `export function values(${names.map((name, index) => `v${String(index)}: ${name}`).join(', ')}): string[] {
     return [${names.map((_name, index) => `v${String(index)}`).join(', ')}];
   }`,
);
const lowered = api.lowerTypeScriptSource(source, {
  packageName: '@flighthq/host-test',
  upstreamDirectory: '/flight',
});
if (lowered.diagnostics.length > 0) {
  process.stderr.write(`${JSON.stringify(lowered.diagnostics, undefined, 2)}\n`);
  process.exit(1);
}
const emitted = api.emitIrModuleCpp(lowered.module, {
  externalBindings: bindings,
  runtimeProfile: 'flight-cpp',
}).contents;
if (!emitted.includes(`flight::Array<flight::String> values(${names.map((_name, index) => `flight::String v${String(index)}`).join(', ')})`)) {
  process.stderr.write(`Web string type fixture did not emit exact flight::String parameters.\n${emitted}\n`);
  process.exit(1);
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-web-types-'));
try {
  writeFileSync(path.join(temporary, 'web_types.hpp'), emitted);
  const compilation = spawnSync(
    cppCompiler,
    [
      '-std=c++20',
      '-fsyntax-only',
      '-I',
      path.join(root, 'include'),
      '-x',
      'c++',
      path.join(temporary, 'web_types.hpp'),
    ],
    { cwd: root, encoding: 'utf8' },
  );
  if (compilation.status !== 0) {
    process.stderr.write(`${compilation.stdout}${compilation.stderr}\nEmitted Web string types header:\n${emitted}\n`);
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(`Web string aliases map exactly to flight::String (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`);
