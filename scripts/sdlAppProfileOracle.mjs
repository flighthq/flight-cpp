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
  process.stdout.write('flight-compiler is not rehydrated (SDL app profile oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (SDL app profile oracle skipped).\n`);
  process.exit(0);
}
const sdlFlags = spawnSync('pkg-config', ['--cflags', 'sdl3'], { encoding: 'utf8' });
if (sdlFlags.status !== 0) {
  process.stdout.write('SDL 3 development files are unavailable (SDL app profile oracle skipped).\n');
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
const profileNames = ['runtime', 'headless', 'web-types', 'sdl-gl', 'sdl-app'];
const profiles = profileNames.map((name) =>
  JSON.parse(readFileSync(path.join(root, 'bindings', `${name}.json`), 'utf8')),
);
const bindings = {
  schema: 'flight-cpp-external-bindings/1',
  bindings: profiles.flatMap((profile) => profile.bindings),
};
const source = api.parseTypeScriptSource(
  '/flight/packages/host-test/src/sdlApp.ts',
  `export function wireNativeApplication(canvas: HTMLCanvasElement): number {
     document.body.appendChild(canvas);
     window.addEventListener('keydown', (event: KeyboardEvent) => event.preventDefault());
     canvas.addEventListener('pointermove', (event: PointerEvent) => event.preventDefault());
     canvas.addEventListener(
       'wheel',
       (event: WheelEvent) => event.preventDefault(),
       { passive: false },
     );
     if (KeyboardEvent.DOM_KEY_LOCATION_RIGHT !== 2) throw new Error('invalid key location');
     if (WheelEvent.DOM_DELTA_PIXEL !== 0) throw new Error('invalid wheel mode');
     if (window.devicePixelRatio <= 0) throw new Error('invalid pixel ratio');
     const discarded = requestAnimationFrame((_timestamp) => {});
     cancelAnimationFrame(discarded);
     return requestAnimationFrame((_timestamp) => {});
   }
   export function inspectNativeEvents(
     event: Event,
     input: InputEvent,
     composition: CompositionEvent,
     gamepadEvent: GamepadEvent,
   ): string {
     event.preventDefault();
     const keyboard = event as KeyboardEvent;
     const gamepad: Gamepad = gamepadEvent.gamepad;
     navigator.getGamepads();
     return keyboard.key + (input.data ?? '') + (composition.data ?? '') + gamepad.id;
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
  '#include <flight/host_sdl/web_platform.hpp>',
  'flight::host_sdl::document.body.append_child(canvas)',
  'flight::host_sdl::window.add_event_listener(',
  'flight::host_sdl::window.device_pixel_ratio',
  'canvas.add_event_listener(flight::String("pointermove")',
  'canvas.add_event_listener(flight::String("wheel")',
  'flight::host_sdl::keyboard_location_right',
  'flight::host_sdl::wheel_delta_pixel',
  'flight::host_sdl::cancel_animation_frame(discarded)',
  'flight::host_sdl::request_animation_frame(',
  'flight::host_sdl::InputKeyboardData event',
  'flight::host_sdl::InputPointerData event',
  'flight::host_sdl::DomEvent event',
  'static_cast<flight::host_sdl::InputKeyboardData>(event)',
  'flight::host_sdl::navigator.get_gamepads()',
  'flight::host_sdl::GamepadSnapshot gamepad',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`SDL app binding fixture did not emit ${expected}.\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-sdl-app-profile-'));
try {
  const header = path.join(temporary, 'sdl_app.hpp');
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

process.stdout.write(`SDL application-shell profile matches flight-compiler ${compiler.commit.slice(0, 7)} output.\n`);
