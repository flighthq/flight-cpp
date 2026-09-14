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
  'CanvasLineCap',
  'CanvasLineJoin',
  'GlobalCompositeOperation',
  'GPUAddressMode',
  'GPUBlendFactor',
  'GPUBlendOperation',
  'GPUCompareFunction',
  'GPUFeatureName',
  'GPUFilterMode',
  'GPUIndexFormat',
  'GPULoadOp',
  'GPUMipmapFilterMode',
  'GPUPowerPreference',
  'GPUPrimitiveTopology',
  'GPUStencilOperation',
  'GPUTextureFormat',
  'ImageSmoothingQuality',
  'LDMLPluralRule',
  'PermissionName',
  'PredefinedColorSpace',
  'WakeLockType',
];
const source = api.parseTypeScriptSource(
  '/flight/packages/host-test/src/webTypes.ts',
  `export function values(${names.map((name, index) => `v${String(index)}: ${name}`).join(', ')}): string[] {
     return [${names.map((_name, index) => `v${String(index)}`).join(', ')}];
   }
   export function time(value: DOMHighResTimeStamp): number {
     return value;
   }
   export function point(value: DOMPointInit): DOMPointInit {
     return value;
   }
   export function settings(value: CanvasRenderingContext2DSettings): CanvasRenderingContext2DSettings {
     return value;
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
if (!emitted.includes('double time(double value)')) {
  process.stderr.write(`DOMHighResTimeStamp fixture did not emit an exact double parameter.\n${emitted}\n`);
  process.exit(1);
}
for (const expected of [
  '#include <flight/web_types.hpp>',
  'flight::DomPointInit point(flight::DomPointInit value)',
  'flight::CanvasRenderingContext2DSettings settings(flight::CanvasRenderingContext2DSettings value)',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`Web dictionary fixture did not emit ${expected}.\n${emitted}\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-web-types-'));
try {
  writeFileSync(path.join(temporary, 'web_types.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "web_types.hpp"

int main() {
  const auto point = flighthq_host_test::point(
      flight::DomPointInit{.w = 2.0, .x = 3.0, .y = 4.0});
  const auto settings = flighthq_host_test::settings(
      flight::CanvasRenderingContext2DSettings{
          .alpha = false,
          .color_space = flight::String("display-p3"),
          .will_read_frequently = true,
      });
  return point.w == 2.0 && point.x == 3.0 && !point.z.has_value() &&
                 settings.alpha == false && settings.color_space == flight::String("display-p3") &&
                 !settings.desynchronized.has_value() && settings.will_read_frequently == true
             ? 0
             : 1;
}
`,
  );
  const executable = path.join(temporary, process.platform === 'win32' ? 'oracle.exe' : 'oracle');
  const compilation = spawnSync(
    cppCompiler,
    [
      '-std=c++20',
      '-I',
      path.join(root, 'include'),
      '-I',
      temporary,
      path.join(temporary, 'consumer.cpp'),
      '-o',
      executable,
    ],
    { cwd: root, encoding: 'utf8' },
  );
  if (compilation.status !== 0) {
    process.stderr.write(`${compilation.stdout}${compilation.stderr}\nEmitted Web string types header:\n${emitted}\n`);
    process.exit(1);
  }
  const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
  if (native.status !== 0) {
    process.stderr.write(
      `${native.stdout}${native.stderr}Compiler-emitted Web value dictionary fixture failed with status ${String(native.status)}.\n`,
    );
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Web scalar aliases and value dictionaries map exactly (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
