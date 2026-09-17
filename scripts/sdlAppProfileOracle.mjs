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
const profileNames = ['runtime', 'headless', 'web-types', 'sdl-image', 'sdl-gl', 'sdl-app'];
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
     if (document.hidden && document.hasFocus()) throw new Error('hidden document retained focus');
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
     const button: GamepadButton = gamepad.buttons[0]!;
     navigator.getGamepads();
     return keyboard.key + (input.data ?? '') + (composition.data ?? '') + gamepad.id +
       (button.touched ? String(button.value) : '');
   }
   export function inspectNativeRect(rect: DOMRect): number {
     return rect.left + rect.top + rect.right + rect.bottom + rect.x + rect.y + rect.width + rect.height;
   }
   export function inspectListenerOptions(options: AddEventListenerOptions): boolean {
     return (options.capture ?? false) || (options.once ?? false) || (options.passive ?? false);
   }
   export function buildNativeControls(): HTMLDivElement {
     const style = document.createElement('style');
     style.textContent = '.controls { color: white; }';
     document.head.appendChild(style);
     const controls = document.createElement('div');
     const label = document.createElement('span');
     label.textContent = 'Flight';
     controls.appendChild(label);
     document.body.appendChild(controls);
     return controls;
   }
   export function createNativeGlContext(): WebGL2RenderingContext {
     const canvas = document.createElement('canvas');
     canvas.width = 8;
     canvas.height = 4;
     return canvas.getContext('webgl2')!;
   }
   export function replaceNativeOverlay(div: HTMLDivElement): void {
     for (const element of div.querySelectorAll('[data-flight-overlay]')) element.remove();
     div.insertAdjacentHTML('beforeend', '<span data-flight-overlay>Flight</span>');
   }
   export function globalDocumentFocus(): boolean {
     const canvas = globalThis.document.createElement('canvas');
     canvas.width = 2;
     canvas.height = 2;
     return globalThis.document.hasFocus() === document.hasFocus();
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
  'flight::host_sdl::document.hidden',
  'flight::host_sdl::document.has_focus()',
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
  'flight::host_sdl::GamepadButtonSnapshot button',
  'button.touched',
  'flight::host_sdl::ClientRect rect',
  'rect.width',
  'flight::host_sdl::EventListenerOptions options',
  'options.once',
  'flight::host_sdl::HtmlDivElement build_native_controls()',
  'auto style = flight::host_sdl::document.create_element(flight::String("style"))',
  'style.text_content = flight::String(".controls { color: white; }")',
  'auto controls = flight::host_sdl::document.create_element(flight::String("div"))',
  'controls.append_child(label)',
  'flight::host_sdl::WebGl2Context create_native_gl_context()',
  // `globalThis` names the same host objects the bare bindings do, rather than a second set.
  'flight::host_sdl::global_this.document.create_element(flight::String("canvas"))',
  '(flight::host_sdl::global_this.document.has_focus() == flight::host_sdl::document.has_focus())',
  'canvas.get_context(flight::String("webgl2"))',
  'for (auto element : div.query_selector_all(flight::String("[data-flight-overlay]")))',
  'element.remove()',
  'div.insert_adjacent_html(flight::String("beforeend")',
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
