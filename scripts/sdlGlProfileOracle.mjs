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
const profileNames = ['runtime', 'headless', 'sdl-gl'];
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
     attributes: WebGLContextAttributes;
     preference: WebGLPowerPreference;
     imageCache: WeakMap<CanvasImageSource, WebGLTexture>;
   }
   export function anisotropyEnums(extension: EXT_texture_filter_anisotropic): number {
     return extension.TEXTURE_MAX_ANISOTROPY_EXT + extension.MAX_TEXTURE_MAX_ANISOTROPY_EXT;
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
  'extension.texture_max_anisotropy_ext',
  'extension.max_texture_max_anisotropy_ext',
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
  `SDL/GL binding profile emits compilable native surface, handle, and weak-cache types (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
