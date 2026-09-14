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
  process.stdout.write('flight-compiler is not rehydrated (SDL/GL profile oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (SDL/GL profile oracle skipped).\n`);
  process.exit(0);
}
const sdlFlags = spawnSync('pkg-config', ['--cflags', 'sdl3'], { encoding: 'utf8' });
if (sdlFlags.status !== 0) {
  process.stdout.write('SDL 3 development files are unavailable (SDL/GL profile oracle skipped).\n');
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
const profileNames = ['runtime', 'headless', 'web-types', 'sdl-gl'];
const profiles = profileNames.map((name) =>
  JSON.parse(readFileSync(path.join(root, 'bindings', `${name}.json`), 'utf8')),
);
const bindings = {
  schema: 'flight-cpp-external-bindings/1',
  bindings: profiles.flatMap((profile) => profile.bindings),
};
const source = api.parseTypeScriptSource(
  '/flight/packages/host-test/src/sdlGl.ts',
  `export interface NativeGlBindings {
     canvas: HTMLCanvasElement;
     context: WebGL2RenderingContext;
     buffer: WebGLBuffer;
     framebuffer: WebGLFramebuffer;
     program: WebGLProgram;
     renderbuffer: WebGLRenderbuffer;
     shader: WebGLShader;
     texture: WebGLTexture;
     uniformLocation: WebGLUniformLocation;
     vertexArray: WebGLVertexArrayObject;
     anisotropy: EXT_texture_filter_anisotropic;
     activeInfo: WebGLActiveInfo;
     attributes: WebGLContextAttributes;
     preference: WebGLPowerPreference;
     imageCache: WeakMap<CanvasImageSource, WebGLTexture>;
   }
   export interface NativeGlParameters {
     maxSamples: number;
     depthWrite: boolean;
     texture: WebGLTexture | null;
     framebuffer: WebGLFramebuffer | null;
     program: WebGLProgram | null;
     vertexArray: WebGLVertexArrayObject | null;
     colorMask: boolean[];
     viewport: number[];
   }
   export function anisotropyEnums(extension: EXT_texture_filter_anisotropic): number {
     return extension.TEXTURE_MAX_ANISOTROPY_EXT + extension.MAX_TEXTURE_MAX_ANISOTROPY_EXT;
   }
   export function nativeGlExtensions(context: WebGL2RenderingContext): number {
     const anisotropy = context.getExtension('EXT_texture_filter_anisotropic');
     if (anisotropy === null) return 0;
     const colorBufferFloat = context.getExtension('EXT_color_buffer_float') !== null;
     return anisotropy.MAX_TEXTURE_MAX_ANISOTROPY_EXT + (colorBufferFloat ? 1 : 0);
   }
   export function nativeGlSmoke(context: WebGL2RenderingContext, data: Float32Array): void {
     const buffer = context.createBuffer();
     context.bindBuffer(context.ARRAY_BUFFER, buffer);
     context.bufferData(context.ARRAY_BUFFER, data, context.STATIC_DRAW);
     context.bufferSubData(context.ARRAY_BUFFER, 0, data, 0, data.length);
     context.clearColor(0, 0, 0, 1);
     context.clear(context.COLOR_BUFFER_BIT);
     const vertexArray = context.createVertexArray();
     context.bindVertexArray(vertexArray);
     context.drawArrays(context.TRIANGLES, 0, 3);
     context.bindVertexArray(null);
     context.deleteVertexArray(vertexArray);
     context.deleteBuffer(buffer);
   }
   export function nativeGlProgram(context: WebGL2RenderingContext): boolean {
     const vertex = context.createShader(context.VERTEX_SHADER);
     if (vertex === null) return false;
     context.shaderSource(vertex, '#version 300 es\\nvoid main() { gl_Position = vec4(0.0); }');
     context.compileShader(vertex);
     if (!context.getShaderParameter(vertex, context.COMPILE_STATUS)) return false;
     const program = context.createProgram();
     context.attachShader(program, vertex);
     context.linkProgram(program);
     const linked = context.getProgramParameter(program, context.LINK_STATUS) !== 0;
     if (linked) context.useProgram(program);
     else context.useProgram(null);
     const color = context.getUniformLocation(program, 'u_color');
     context.uniform4f(color, 1, 1, 1, 1);
     context.getActiveUniform(program, 0);
     context.deleteProgram(program);
     context.deleteShader(vertex);
     return linked;
   }
   export function nativeGlTextureAndFramebuffer(
     context: WebGL2RenderingContext,
     bytes: Uint8Array,
     floats: Float32Array,
     image: CanvasImageSource,
   ): void {
     context.texImage2D(
       context.TEXTURE_2D, 0, context.RGBA8, 1, 1, 0, context.RGBA, context.UNSIGNED_BYTE, bytes,
     );
     context.texImage2D(
       context.TEXTURE_2D, 0, context.RGBA8, 1, 1, 0, context.RGBA, context.UNSIGNED_BYTE, null,
     );
     context.texImage2D(context.TEXTURE_2D, 0, context.RGBA, context.RGBA, context.UNSIGNED_BYTE, image);
     context.texSubImage2D(
       context.TEXTURE_2D, 0, 0, 0, 1, 1, context.RGBA, context.UNSIGNED_BYTE, bytes,
     );
     context.texImage3D(
       context.TEXTURE_3D, 0, context.RGBA8, 1, 1, 1, 0, context.RGBA, context.UNSIGNED_BYTE, bytes,
     );
     context.texStorage3D(context.TEXTURE_2D_ARRAY, 1, context.RGBA8, 1, 1, 1);
     context.compressedTexImage2D(context.TEXTURE_2D, 0, context.RGBA8, 1, 1, 0, bytes);
     context.compressedTexSubImage3D(
       context.TEXTURE_2D_ARRAY, 0, 0, 0, 0, 1, 1, 1, context.RGBA8, bytes,
     );
     context.clearBufferfi(context.DEPTH_STENCIL, 0, 1, 0);
     context.clearBufferfv(context.COLOR, 0, [0, 0, 0, 1]);
     context.drawBuffers([context.COLOR_ATTACHMENT0]);
     context.blitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, context.COLOR_BUFFER_BIT, context.NEAREST);
     context.readPixels(0, 0, 1, 1, context.RGBA, context.UNSIGNED_BYTE, bytes);
     context.readPixels(0, 0, 1, 1, context.RGBA, context.FLOAT, floats);
   }
   export function nativeGlParameters(context: WebGL2RenderingContext): NativeGlParameters {
     return {
       maxSamples: context.getParameter(context.MAX_SAMPLES) as number,
       depthWrite: context.getParameter(context.DEPTH_WRITEMASK) as boolean,
       texture: context.getParameter(context.TEXTURE_BINDING_2D) as WebGLTexture | null,
       framebuffer: context.getParameter(context.FRAMEBUFFER_BINDING) as WebGLFramebuffer | null,
       program: context.getParameter(context.CURRENT_PROGRAM) as WebGLProgram | null,
       vertexArray: context.getParameter(context.VERTEX_ARRAY_BINDING) as WebGLVertexArrayObject | null,
       colorMask: context.getParameter(context.COLOR_WRITEMASK) as boolean[],
       viewport: context.getParameter(context.SCISSOR_BOX) as number[],
     };
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
  '#include <flight/host_sdl/webgl.hpp>',
  'flight::host_sdl::GlCanvas canvas;',
  'flight::host_sdl::WebGl2Context context;',
  'flight::host_sdl::WebGlProgram program;',
  'flight::host_sdl::GlAnisotropyExtension anisotropy;',
  'flight::host_sdl::WebGlActiveInfo active_info;',
  'extension.texture_max_anisotropy_ext',
  'extension.max_texture_max_anisotropy_ext',
  'context.get_extension(flight::String("EXT_texture_filter_anisotropic"))',
  'anisotropy.max_texture_max_anisotropy_ext',
  'context.create_buffer()',
  'context.bind_buffer(context.array_buffer, buffer)',
  'context.buffer_data(context.array_buffer, data, context.static_draw)',
  'context.buffer_sub_data(context.array_buffer, 0.0, data, 0.0, static_cast<double>(data.size()))',
  'context.clear_color(0.0, 0.0, 0.0, 1.0)',
  'context.clear(context.color_buffer_bit)',
  'context.create_vertex_array()',
  'context.bind_vertex_array(vertex_array)',
  'context.draw_arrays(context.triangles, 0.0, 3.0)',
  'context.delete_vertex_array(vertex_array)',
  'context.delete_buffer(buffer)',
  'context.create_shader(context.vertex_shader)',
  'context.shader_source(',
  'context.compile_shader(',
  'context.get_shader_parameter(',
  'context.create_program()',
  'context.attach_shader(',
  'context.link_program(program)',
  'context.get_program_parameter(program, context.link_status)',
  'context.get_uniform_location(program, flight::String("u_color"))',
  'context.uniform4f(color, 1.0, 1.0, 1.0, 1.0)',
  'context.get_active_uniform(program, 0.0)',
  'context.delete_program(program)',
  'context.delete_shader(',
  'context.tex_image2_d(context.texture_2_d, 0.0, context.rgba8, 1.0, 1.0, 0.0, context.rgba, context.unsigned_byte, bytes)',
  'context.tex_image2_d(context.texture_2_d, 0.0, context.rgba, context.rgba, context.unsigned_byte, image)',
  'context.tex_sub_image2_d(',
  'context.tex_image3_d(',
  'context.tex_storage3_d(',
  'context.compressed_tex_image2_d(',
  'context.compressed_tex_sub_image3_d(',
  'context.clear_bufferfi(',
  'context.clear_bufferfv(',
  'context.draw_buffers(',
  'context.blit_framebuffer(',
  'context.read_pixels(',
  'static_cast<double>(context.get_parameter(context.max_samples))',
  'static_cast<bool>(context.get_parameter(context.depth_writemask))',
  'static_cast<std::optional<flight::host_sdl::WebGlTexture>>(context.get_parameter(context.texture_binding_2_d))',
  'static_cast<flight::Array<bool>>(context.get_parameter(context.color_writemask))',
  'static_cast<flight::Array<double>>(context.get_parameter(context.scissor_box))',
  'flight::host_sdl::WebGlContextAttributes attributes;',
  'flight::host_sdl::GlImageSourceWeakPolicy',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`SDL/GL binding fixture did not emit ${expected}.\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-sdl-gl-profile-'));
try {
  const header = path.join(temporary, 'sdl_gl.hpp');
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
    process.stderr.write(`${compilation.stdout}${compilation.stderr}`);
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `SDL/GL profile emits compilable surface, handle, extension, weak-cache, and GL command bindings (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
