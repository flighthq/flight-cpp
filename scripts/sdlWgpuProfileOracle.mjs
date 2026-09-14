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
  process.stdout.write('flight-compiler is not rehydrated (SDL/WGPU profile oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (SDL/WGPU profile oracle skipped).\n`);
  process.exit(0);
}
const sdlFlags = spawnSync('pkg-config', ['--cflags', 'sdl3'], { encoding: 'utf8' });
if (sdlFlags.status !== 0) {
  process.stdout.write('SDL 3 development files are unavailable (SDL/WGPU profile oracle skipped).\n');
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
const bindingProfiles = ['web-types', 'sdl-wgpu'].map((name) =>
  JSON.parse(readFileSync(path.join(root, 'bindings', `${name}.json`), 'utf8')),
);
const bindings = {
  schema: 'flight-cpp-external-bindings/1',
  bindings: bindingProfiles.flatMap((profile) => profile.bindings),
};
const handleTypes = [
  ['GPU', 'WgpuApi'],
  ['GPUAdapter', 'WgpuAdapter'],
  ['GPUBindGroup', 'WgpuBindGroup'],
  ['GPUBindGroupLayout', 'WgpuBindGroupLayout'],
  ['GPUBuffer', 'WgpuBuffer'],
  ['GPUCanvasContext', 'WgpuCanvasContext'],
  ['GPUCommandEncoder', 'WgpuCommandEncoder'],
  ['GPUDevice', 'WgpuDevice'],
  ['GPUCopyExternalImageSource', 'WgpuExternalImageSource'],
  ['GPUPipelineLayout', 'WgpuPipelineLayout'],
  ['GPUQueue', 'WgpuQueue'],
  ['GPURenderPassEncoder', 'WgpuRenderPassEncoder'],
  ['GPURenderPipeline', 'WgpuRenderPipeline'],
  ['GPUSampler', 'WgpuSampler'],
  ['GPUShaderModule', 'WgpuShaderModule'],
  ['GPUTexture', 'WgpuTexture'],
  ['GPUTextureView', 'WgpuTextureView'],
];
const source = api.parseTypeScriptSource(
  '/flight/packages/host-test/src/wgpu.ts',
  `${handleTypes.map(([sourceName], index) => `export function keep${String(index)}(value: ${sourceName}): ${sourceName} { return value; }`).join('\n')}
   export function lostMessage(value: GPUDeviceLostInfo): string { return value.message; }
   export function red(value: GPUColor): number { return value.r; }
   export function deviceDescriptor(value: GPUDeviceDescriptor): GPUDeviceDescriptor { return value; }
   export function sourceInfo(value: GPUCopyExternalImageSourceInfo): GPUCopyExternalImageSourceInfo { return value; }
   export function destinationInfo(value: GPUCopyExternalImageDestInfo): GPUCopyExternalImageDestInfo { return value; }
   export function makeSourceInfo(source: GPUCopyExternalImageSource): GPUCopyExternalImageSourceInfo { return { source }; }
   export function makeDestinationInfo(texture: GPUTexture, origin: GPUOrigin3D): GPUCopyExternalImageDestInfo {
     return { texture, origin };
   }
   export function makeDeviceDescriptor(requiredFeatures: GPUFeatureName[]): GPUDeviceDescriptor {
     return { requiredFeatures };
   }
   export function origin(value: GPUOrigin3D): GPUOrigin3D { return value; }
   export function vertexLayout(value: GPUVertexBufferLayout): GPUVertexBufferLayout { return value; }
   export function maxTextureSize(value: GPUAdapter): number { return value.limits.maxTextureDimension2D ?? 8192; }
   export function supportsTimestamp(value: GPUAdapter): boolean { return value.features.has('timestamp-query'); }
   export function usageFlags(): number {
     return GPUBufferUsage.VERTEX | GPUTextureUsage.RENDER_ATTACHMENT | GPUShaderStage.FRAGMENT;
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
for (const expected of [
  '#include <flight/host_sdl/wgpu.hpp>',
  ...handleTypes.map(([_sourceName, targetName]) => `flight::host_sdl::${targetName}`),
  'flight::host_sdl::WgpuDeviceLostInfo',
  'flight::host_sdl::WgpuColor',
  'flight::host_sdl::WgpuDeviceDescriptor',
  'flight::host_sdl::WgpuExternalImageSourceInfo',
  'flight::host_sdl::WgpuExternalImageDestinationInfo',
  'flight::host_sdl::WgpuOrigin3D',
  'flight::host_sdl::WgpuVertexBufferLayout',
  'flight::host_sdl::wgpu_buffer_usage_vertex',
  'flight::host_sdl::wgpu_texture_usage_render_attachment',
  'flight::host_sdl::wgpu_shader_stage_fragment',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`SDL/WGPU binding fixture did not emit ${expected}.\n${emitted}\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-sdl-wgpu-profile-'));
try {
  const header = path.join(temporary, 'sdl_wgpu.hpp');
  writeFileSync(header, emitted);
  const compilation = spawnSync(
    cppCompiler,
    [
      '-std=c++20',
      '-fsyntax-only',
      '-I',
      path.join(root, 'include'),
      ...sdlFlags.stdout.trim().split(/\s+/u).filter(Boolean),
      '-x',
      'c++',
      header,
    ],
    { cwd: root, encoding: 'utf8' },
  );
  if (compilation.status !== 0) {
    process.stderr.write(`${compilation.stdout}${compilation.stderr}\nEmitted SDL/WGPU header:\n${emitted}\n`);
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(`SDL/WGPU handle profile matches flight-compiler ${compiler.commit.slice(0, 7)} output.\n`);
