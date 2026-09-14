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
  process.stdout.write('flight-compiler is not rehydrated (ImageData oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (ImageData oracle skipped).\n`);
  process.exit(0);
}

if (!existsSync(compilerEntry)) {
  const build = spawnSync('npm', ['run', 'build', '--silent'], {
    cwd: compiler.directory,
    encoding: 'utf8',
  });
  if (build.status !== 0) {
    process.stderr.write(`${build.stdout}${build.stderr}`);
    process.exit(1);
  }
}

const api = await import(pathToFileURL(compilerEntry));
const profiles = ['runtime', 'web-types'].map((name) =>
  JSON.parse(readFileSync(path.join(root, 'bindings', `${name}.json`), 'utf8')),
);
const bindings = {
  schema: 'flight-cpp-external-bindings/1',
  bindings: profiles.flatMap((profile) => profile.bindings),
};
const source = api.parseTypeScriptSource(
  '/flight/packages/runtime-test/src/imageData.ts',
  `export function blank(width: number, height: number): ImageData {
     return new ImageData(width, height, { colorSpace: 'display-p3' });
   }
   export function pixels(data: ImageDataArray, width: number, height?: number): ImageData {
     return new ImageData(data, width, height);
   }
   export function observe(image: ImageData): number[] {
     image.data[0] = 7;
     return [image.width, image.height, image.data[0], image.data.length];
   }
   export function colorSpace(image: ImageData): PredefinedColorSpace {
     return image.colorSpace;
   }`,
);
const lowered = api.lowerTypeScriptSource(source, {
  packageName: '@flighthq/runtime-test',
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
for (const expected of [
  '#include <flight/image_data.hpp>',
  'flight::ImageData blank(double width, double height)',
  'flight::ImageData(width, height, {.color_space = flight::String("display-p3")})',
  'flight::ImageData pixels(flight::Uint8ClampedArray data, double width, std::optional<double> height',
  'flight::ImageData(data, width, height)',
  'image.data[static_cast<size_t>(0.0)] = 7.0',
  'image.color_space',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`ImageData binding fixture did not emit ${expected}.\n${emitted}\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-image-data-'));
try {
  writeFileSync(path.join(temporary, 'image_data_fixture.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "image_data_fixture.hpp"

#include <cstdint>

int main() {
  const auto blank = flighthq_runtime_test::blank(2.0, 3.0);
  if (blank.width != 2.0 || blank.height != 3.0 || blank.data.size() != 24 ||
      blank.color_space != flight::String("display-p3")) {
    return 1;
  }

  const flight::Uint8ClampedArray pixels{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
  const auto image = flighthq_runtime_test::pixels(pixels, 2.0);
  const auto observed = flighthq_runtime_test::observe(image);
  return observed.size() == 4 && observed[0] == 2.0 && observed[1] == 1.0 &&
                 observed[2] == 7.0 && observed[3] == 8.0 &&
                 static_cast<std::uint8_t>(pixels[0]) == 7 &&
                 flighthq_runtime_test::color_space(image) == flight::String("srgb")
             ? 0
             : 2;
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
    process.stderr.write(`${compilation.stdout}${compilation.stderr}\nEmitted ImageData header:\n${emitted}\n`);
    process.exit(1);
  }
  const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
  if (native.status !== 0) {
    process.stderr.write(
      `${native.stdout}${native.stderr}Compiler-emitted ImageData fixture failed with status ${String(native.status)}.\n`,
    );
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Compiler-emitted ImageData preserves live RGBA storage (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
