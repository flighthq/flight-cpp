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
  ['GPUCommandBuffer', 'WgpuCommandBuffer'],
  ['GPUCommandEncoder', 'WgpuCommandEncoder'],
  ['GPUDevice', 'WgpuDevice'],
  ['GPUCopyExternalImageSource', 'WgpuExternalImageSource'],
  ['GPUExternalTexture', 'WgpuExternalTexture'],
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
   export function bindingResource(value: GPUBindingResource): GPUBindingResource { return value; }
   export function bindGroupDescriptor(value: GPUBindGroupDescriptor): GPUBindGroupDescriptor { return value; }
   export function bindGroupEntry(value: GPUBindGroupEntry): GPUBindGroupEntry { return value; }
   export function bindGroupLayoutDescriptor(value: GPUBindGroupLayoutDescriptor): GPUBindGroupLayoutDescriptor { return value; }
   export function bindGroupLayoutEntry(value: GPUBindGroupLayoutEntry): GPUBindGroupLayoutEntry { return value; }
   export function bufferBinding(value: GPUBufferBinding): GPUBufferBinding { return value; }
   export function bufferBindingLayout(value: GPUBufferBindingLayout): GPUBufferBindingLayout { return value; }
   export function samplerBindingLayout(value: GPUSamplerBindingLayout): GPUSamplerBindingLayout { return value; }
   export function textureBindingLayout(value: GPUTextureBindingLayout): GPUTextureBindingLayout { return value; }
   export function storageTextureBindingLayout(value: GPUStorageTextureBindingLayout): GPUStorageTextureBindingLayout { return value; }
   export function externalTextureBindingLayout(value: GPUExternalTextureBindingLayout): GPUExternalTextureBindingLayout { return value; }
   export function bufferSource(value: GPUAllowSharedBufferSource): GPUAllowSharedBufferSource { return value; }
   export function bufferDescriptor(value: GPUBufferDescriptor): GPUBufferDescriptor { return value; }
   export function deviceDescriptor(value: GPUDeviceDescriptor): GPUDeviceDescriptor { return value; }
   export function extent(value: GPUExtent3D): GPUExtent3D { return value; }
   export function extentDictionary(value: GPUExtent3DDict): GPUExtent3DDict { return value; }
   export function strictExtent(value: GPUExtent3DStrict): GPUExtent3DStrict { return value; }
   export function origin2d(value: GPUOrigin2D): GPUOrigin2D { return value; }
   export function origin2dDictionary(value: GPUOrigin2DDict): GPUOrigin2DDict { return value; }
   export function origin3dDictionary(value: GPUOrigin3DDict): GPUOrigin3DDict { return value; }
   export function texelLayout(value: GPUTexelCopyBufferLayout): GPUTexelCopyBufferLayout { return value; }
   export function texelTexture(value: GPUTexelCopyTextureInfo): GPUTexelCopyTextureInfo { return value; }
   export function textureDescriptor(value: GPUTextureDescriptor): GPUTextureDescriptor { return value; }
   export function sourceInfo(value: GPUCopyExternalImageSourceInfo): GPUCopyExternalImageSourceInfo { return value; }
   export function destinationInfo(value: GPUCopyExternalImageDestInfo): GPUCopyExternalImageDestInfo { return value; }
   export function makeSourceInfo(source: GPUCopyExternalImageSource): GPUCopyExternalImageSourceInfo { return { source }; }
   export function makeDestinationInfo(texture: GPUTexture, origin: GPUOrigin3D): GPUCopyExternalImageDestInfo {
     return { texture, origin };
   }
   export function makeDeviceDescriptor(requiredFeatures: GPUFeatureName[]): GPUDeviceDescriptor {
     return { requiredFeatures };
   }
   export function makeBufferDescriptor(size: number, usage: number): GPUBufferDescriptor {
     return { size, usage, mappedAtCreation: false, label: 'vertices' };
   }
   export function makeBufferBinding(buffer: GPUBuffer): GPUBufferBinding {
     return { buffer, offset: 16, size: 64 };
   }
   export function makeBindGroupEntry(binding: number, resource: GPUBindingResource): GPUBindGroupEntry {
     return { binding, resource };
   }
   export function makeBindGroupDescriptor(
     layout: GPUBindGroupLayout, entries: GPUBindGroupEntry[],
   ): GPUBindGroupDescriptor {
     return { layout, entries, label: 'material' };
   }
   export function makeBufferBindingLayout(): GPUBufferBindingLayout {
     return { type: 'uniform', hasDynamicOffset: true, minBindingSize: 64 };
   }
   export function makeBindGroupLayoutEntry(
     binding: number, visibility: number, buffer: GPUBufferBindingLayout,
   ): GPUBindGroupLayoutEntry {
     return { binding, visibility, buffer };
   }
   export function makeBindGroupLayoutDescriptor(
     entries: GPUBindGroupLayoutEntry[],
   ): GPUBindGroupLayoutDescriptor {
     return { entries, label: 'material-layout' };
   }
   export function makeExtent(width: number, height: number): GPUExtent3DDict {
     return { width, height, depthOrArrayLayers: 1 };
   }
   export function makeTexelLayout(bytesPerRow: number): GPUTexelCopyBufferLayout {
     return { offset: 0, bytesPerRow, rowsPerImage: 1 };
   }
   export function makeTexelTexture(texture: GPUTexture, origin: GPUOrigin3D): GPUTexelCopyTextureInfo {
     return { texture, mipLevel: 0, origin, aspect: 'all' };
   }
   export function origin(value: GPUOrigin3D): GPUOrigin3D { return value; }
   export function vertexLayout(value: GPUVertexBufferLayout): GPUVertexBufferLayout { return value; }
   export function maxTextureSize(value: GPUAdapter): number { return value.limits.maxTextureDimension2D ?? 8192; }
   export function supportsTimestamp(value: GPUAdapter): boolean { return value.features.has('timestamp-query'); }
   export function blend(srcFactor: GPUBlendFactor, dstFactor: GPUBlendFactor): GPUBlendState {
     const component: GPUBlendComponent = { srcFactor, dstFactor, operation: 'add' };
     return { color: component, alpha: component };
   }
   export function stencil(): GPUStencilFaceState {
     return { compare: 'always', passOp: 'replace', failOp: 'keep', depthFailOp: 'keep' };
   }
   export function sampler(compare?: GPUCompareFunction): GPUSamplerDescriptor {
     const descriptor: GPUSamplerDescriptor = {
       addressModeU: 'repeat', addressModeV: 'mirror-repeat', addressModeW: 'clamp-to-edge',
       magFilter: 'linear', minFilter: 'nearest', mipmapFilter: 'linear',
       lodMinClamp: 1, lodMaxClamp: 4, maxAnisotropy: 8, label: 'material',
     };
     if (compare !== undefined) descriptor.compare = compare;
     return descriptor;
   }
   export function usageFlags(): number {
     return GPUBufferUsage.VERTEX | GPUTextureUsage.RENDER_ATTACHMENT | GPUShaderStage.FRAGMENT |
       GPUColorWrite.ALL | GPUMapMode.READ;
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
  'flight::host_sdl::WgpuBlendComponent',
  'flight::host_sdl::WgpuBlendState',
  'flight::host_sdl::WgpuBindingResource',
  'flight::host_sdl::WgpuBindGroupDescriptor',
  'flight::host_sdl::WgpuBindGroupEntry',
  'flight::host_sdl::WgpuBindGroupLayoutDescriptor',
  'flight::host_sdl::WgpuBindGroupLayoutEntry',
  'flight::host_sdl::WgpuBufferBinding',
  'flight::host_sdl::WgpuBufferBindingLayout',
  'flight::host_sdl::WgpuSamplerBindingLayout',
  'flight::host_sdl::WgpuTextureBindingLayout',
  'flight::host_sdl::WgpuStorageTextureBindingLayout',
  'flight::host_sdl::WgpuExternalTextureBindingLayout',
  'flight::host_sdl::WgpuAllowSharedBufferSource',
  'flight::host_sdl::WgpuBufferDescriptor',
  'flight::host_sdl::WgpuDeviceDescriptor',
  'flight::host_sdl::WgpuExtent3D',
  'flight::host_sdl::WgpuExtent3DDictionary',
  'flight::host_sdl::WgpuTexelCopyBufferLayout',
  'flight::host_sdl::WgpuTexelCopyTextureInfo',
  'flight::host_sdl::WgpuTextureDescriptor',
  'flight::host_sdl::WgpuExternalImageSourceInfo',
  'flight::host_sdl::WgpuExternalImageDestinationInfo',
  'flight::host_sdl::WgpuOrigin3D',
  'flight::host_sdl::WgpuVertexBufferLayout',
  'flight::host_sdl::WgpuStencilFaceState',
  'flight::host_sdl::WgpuSamplerDescriptor',
  '.src_factor = src_factor, .dst_factor = dst_factor, .operation = flight::String("add")',
  '.buffer = buffer, .offset = 16.0, .size = 64.0',
  '.binding = binding, .resource = resource',
  '.layout = layout, .entries = entries, .label = flight::String("material")',
  '.type = flight::String("uniform"), .has_dynamic_offset = true, .min_binding_size = 64.0',
  '.binding = binding, .visibility = visibility, .buffer = buffer',
  '.entries = entries, .label = flight::String("material-layout")',
  '.size = size, .usage = usage, .mapped_at_creation = false, .label = flight::String("vertices")',
  '.width = width, .height = height, .depth_or_array_layers = 1.0',
  '.offset = 0.0, .bytes_per_row = bytes_per_row, .rows_per_image = 1.0',
  '.texture = texture, .mip_level = 0.0, .origin = origin, .aspect = flight::String("all")',
  '.compare = flight::String("always"), .pass_op = flight::String("replace")',
  '.address_mode_u = flight::String("repeat"), .address_mode_v = flight::String("mirror-repeat")',
  'descriptor.compare = compare.value()',
  'flight::host_sdl::wgpu_buffer_usage_vertex',
  'flight::host_sdl::wgpu_texture_usage_render_attachment',
  'flight::host_sdl::wgpu_shader_stage_fragment',
  'flight::host_sdl::wgpu_color_write_all',
  'flight::host_sdl::wgpu_map_mode_read',
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
