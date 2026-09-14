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
  process.stdout.write('flight-compiler is not rehydrated (AudioBuffer oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (AudioBuffer oracle skipped).\n`);
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
const bindings = JSON.parse(readFileSync(path.join(root, 'bindings', 'runtime.json'), 'utf8'));
const source = api.parseTypeScriptSource(
  '/flight/packages/runtime-test/src/audioBuffer.ts',
  `export function makeAudioBuffer(samples: Float32Array): AudioBuffer {
     const buffer = new AudioBuffer({
       length: samples.length + 2,
       numberOfChannels: 2,
       sampleRate: 48000,
     });
     buffer.copyToChannel(samples, 1, 1);
     return buffer;
   }
   export function makeAudioBufferWithOptions(options: AudioBufferOptions): AudioBuffer {
     return new AudioBuffer(options);
   }
   export function observeAudioBuffer(buffer: AudioBuffer): number[] {
     const channel = buffer.getChannelData(1);
     channel[0] = 0.5;
     return [
       buffer.numberOfChannels,
       buffer.length,
       buffer.sampleRate,
       buffer.duration,
       channel[0],
       buffer.getChannelData(1)[1],
     ];
   }
   export function copyAudioSamples(buffer: AudioBuffer, destination: Float32Array): void {
     buffer.copyFromChannel(destination, 1, 2);
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
  '#include <flight/audio_buffer.hpp>',
  'flight::AudioBufferOptions options',
  'flight::AudioBuffer({',
  '.number_of_channels = 2.0',
  '.sample_rate = 48000.0',
  'buffer.copy_to_channel(samples, 1.0, 1.0)',
  'buffer.get_channel_data(1.0)',
  'buffer.number_of_channels',
  'buffer.sample_rate',
  'buffer.copy_from_channel(destination, 1.0, 2.0)',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`AudioBuffer binding fixture did not emit ${expected}.\n${emitted}\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-audio-buffer-'));
try {
  writeFileSync(path.join(temporary, 'audio_buffer.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "audio_buffer.hpp"

#include <cmath>

int main() {
  const auto buffer = flighthq_runtime_test::make_audio_buffer(
      flight::Float32Array{0.25F, -0.5F, 0.75F});
  const auto named_options_buffer = flighthq_runtime_test::make_audio_buffer_with_options(
      flight::AudioBufferOptions{.length = 2.0, .sample_rate = 8000.0});
  const auto observed = flighthq_runtime_test::observe_audio_buffer(buffer);
  if (observed.size() != 6 || observed[0] != 2.0 || observed[1] != 5.0 ||
      observed[2] != 48000.0 || std::abs(observed[3] - 5.0 / 48000.0) > 1.0e-12 ||
      observed[4] != 0.5 || observed[5] != 0.25) {
    return 1;
  }
  flight::Float32Array destination{9.0F, 9.0F, 9.0F};
  flighthq_runtime_test::copy_audio_samples(buffer, destination);
  return destination[0] == -0.5F && destination[1] == 0.75F && destination[2] == 0.0F &&
                 named_options_buffer.length == 2.0 && named_options_buffer.sample_rate == 8000.0
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
      '-pthread',
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
    process.stderr.write(`${compilation.stdout}${compilation.stderr}\nEmitted AudioBuffer header:\n${emitted}\n`);
    process.exit(1);
  }
  const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
  if (native.status !== 0) {
    process.stderr.write(`${native.stdout}${native.stderr}Compiler-emitted AudioBuffer fixture failed with status ${String(native.status)}.\n`);
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Compiler-emitted AudioBuffer constructs and shares decoded PCM (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
