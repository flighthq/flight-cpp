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
  'GPUCanvasAlphaMode',
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
   }
   export function encodeOptions(value: ImageEncodeOptions): ImageEncodeOptions {
     return value;
   }
   export function permission(value: PermissionDescriptor): PermissionDescriptor {
     return value;
   }
   export function position(value: PositionOptions): PositionOptions {
     return value;
   }
   export function metrics(value: TextMetrics): number[] {
     return [
       value.actualBoundingBoxAscent,
       value.actualBoundingBoxDescent,
       value.actualBoundingBoxLeft,
       value.actualBoundingBoxRight,
       value.alphabeticBaseline,
       value.emHeightAscent,
       value.emHeightDescent,
       value.fontBoundingBoxAscent,
       value.fontBoundingBoxDescent,
       value.hangingBaseline,
       value.ideographicBaseline,
       value.width,
     ];
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
  'flight::WebImageEncodeOptions encode_options(flight::WebImageEncodeOptions value)',
  'flight::WebPermissionDescriptor permission(flight::WebPermissionDescriptor value)',
  'flight::WebPositionOptions position(flight::WebPositionOptions value)',
  'flight::Array<double> metrics(flight::WebTextMetrics value)',
  'value.font_bounding_box_ascent',
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
  const auto encode_options = flighthq_host_test::encode_options(
      flight::WebImageEncodeOptions{.quality = 0.75, .type = flight::String("image/webp")});
  const auto permission = flighthq_host_test::permission(
      flight::WebPermissionDescriptor{.name = flight::String("camera")});
  const auto position = flighthq_host_test::position(
      flight::WebPositionOptions{.enable_high_accuracy = true, .timeout = 500.0});
  flight::WebTextMetrics metrics;
  metrics.actual_bounding_box_ascent = 1.0;
  metrics.actual_bounding_box_descent = 2.0;
  metrics.actual_bounding_box_left = 3.0;
  metrics.actual_bounding_box_right = 4.0;
  metrics.alphabetic_baseline = 5.0;
  metrics.em_height_ascent = 6.0;
  metrics.em_height_descent = 7.0;
  metrics.font_bounding_box_ascent = 8.0;
  metrics.font_bounding_box_descent = 9.0;
  metrics.hanging_baseline = 10.0;
  metrics.ideographic_baseline = 11.0;
  metrics.width = 12.0;
  const auto measured = flighthq_host_test::metrics(metrics);
  return point.w == 2.0 && point.x == 3.0 && !point.z.has_value() &&
                 settings.alpha == false && settings.color_space == flight::String("display-p3") &&
                 !settings.desynchronized.has_value() && settings.will_read_frequently == true &&
                 encode_options.quality == 0.75 &&
                 encode_options.type == flight::String("image/webp") &&
                 permission.name == flight::String("camera") &&
                 position.enable_high_accuracy == true && !position.maximum_age.has_value() &&
                 position.timeout == 500.0 && measured.size() == 12 && measured[0] == 1.0 &&
                 measured[7] == 8.0 && measured[11] == 12.0
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
