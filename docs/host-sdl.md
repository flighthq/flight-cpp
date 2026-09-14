# SDL host package

`host_sdl` is the handwritten native boundary for Flight applications running in an SDL 3 process. It owns SDL
subsystem lifetime, windows, event ingress, decoded-PCM playback, and native graphics surface acquisition. It does
not implement a scene renderer. Generated `render-gl`, `scene2d-gl`, `render-wgpu`, and `scene2d-wgpu` code remains
responsible for rendering after an adapter supplies its native context or surface.

The installed package is independent of the generated SDK while the compiler contracts settle. Its public API lives
under `include/flight/host_sdl/`, its implementations live under `src/host_sdl/`, and none of its targets changes the
dependency-free `Flight::Cpp` target. The build tree additionally exposes SDK audio and cursor adapters against the
committed preview headers so the exact generated interfaces are continuously compiled and executed before they
become installable.

## Build

SDL 3.2 or newer is required. Vulkan headers and a loader are required unless Vulkan support is disabled. On a clean
checkout with SDL and Vulkan development packages installed, build and test the complete package with:

```sh
cmake -S . -B out/cmake/host-sdl -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DFLIGHT_CPP_BUILD_HOST_SDL=ON
cmake --build out/cmake/host-sdl
ctest --test-dir out/cmake/host-sdl --output-on-failure
```

The same option can be layered onto the repository preset:

```sh
cmake --preset development -DFLIGHT_CPP_BUILD_HOST_SDL=ON
cmake --build --preset development
ctest --preset development
```

With examples enabled by the preset, the native tween example exercises the GL path end to end:

```sh
./out/cmake/development/examples/flight_cpp_tween_sdl_gl_example
./out/cmake/development/examples/flight_cpp_sound_sdl_example
```

Bazel pins and builds SDL 3.4.10 from source for the SDL, GL, and type-erased WGPU targets. CMake, Ninja, Make, M4,
and pkg-config are the build tools used by `rules_foreign_cc`; SDL itself does not need to be installed. Build the
same host test and run the GL smoke with:

```sh
bazel test --config=local-posix //tests:host_sdl_test
SDL_AUDIODRIVER=dummy \
  bazel run --config=local-posix //examples:sound_sdl -- --smoke
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  bazel run --config=local-posix //examples:tween_sdl_gl -- --smoke
```

The public Bazel labels are `//:host_sdl`, `//:host_sdl_image`, `//:host_sdl_gl`, `//:host_sdl_sdk_audio`,
`//:host_sdl_sdk_clipboard`, `//:host_sdl_sdk_cursor`, `//:host_sdl_sdk_device`, `//:host_sdl_sdk_keyboard`, `//:host_sdl_sdk_platform`, `//:host_sdl_sdk_screen`, `//:host_sdl_sdk_window`, and `//:host_sdl_wgpu`. They and their tests are tagged `manual`, so a core
`bazel test //...` does not fetch or build SDL. CMake remains the complete package path for the Vulkan surface
adapter.

It animates the fifteen easing curves emitted from `examples/tween/source/tween.ts`. Rendering uses the copyable
`GlCanvas` and `WebGl2Context` host seam exposed by `Flight::HostSdlGl`. The example calls the context's reusable
texture upload, framebuffer clear/readback, shader/program, draw, viewport, and presentation operations; no
example-private OpenGL dispatch table or SDL renderer is involved.

The sound example transpiles the upstream procedural tone and sweep calculations, narrows their results to Float32
PCM once, and plays three concurrent sources through the generated `AudioDeviceBackend` record returned by
`SdkAudioDeviceBackend`. Its `--smoke` mode runs with SDL's dummy audio driver and verifies application-thread
completion delivery.

Set `-DFLIGHT_CPP_BUILD_HOST_SDL_VULKAN=OFF` for an SDL and GL/WGPU build without Vulkan development files. Native
dependency discovery and target selection belong to CMake or Bazel, so an npm wrapper would only obscure their
options and is not provided.

The installed build exports five primary host targets through the existing `FlightCpp` package:

- `Flight::HostSdl` initializes ref-counted SDL subsystems, polls or waits for `SDL_Event`, exposes the monotonic SDL
  clock, and owns plain, OpenGL, or Vulkan windows. `InputDispatcher` converts keyboard, text/IME, mouse, wheel, and
  standard-layout gamepad events into records matching Flight's input-ingress data shapes. `Window` controls SDL text
  input and relative-pointer mode for the eventual generated ingress adapter. `SdlAudioDeviceBackend` implements
  Flight's decoded-PCM device, buffer, and source lifecycle with live gain, equal-power pan, playback rate, bounded
  regions, and completion notification. It mixes mono or stereo Float32 sources in SDL's device callback and queues
  completions for serialized application-thread delivery. `SdlCursorBackend` maps Flight's CSS cursor identifiers to
  SDL system cursors, implements `none` with SDL visibility, and retains the requested value when a headless driver
  has no native cursor. Unknown or unsupported CSS cursor images use the SDL default.
- `Flight::HostSdlImage` owns no graphics context. It is the shared decoded-RGBA image carrier used by GL and
  WebGPU, with source-kind metadata and weak identity for texture caches. Native image decoders and video providers
  populate it; neither graphics target owns decoding or frame acquisition.
- `Flight::HostSdlGl` owns an `SDL_GLContext`, configures OpenGL or OpenGL ES attributes before window creation,
  resolves procedure addresses, controls the swap interval, and swaps the window. Its `GlCanvas` and
  `WebGl2Context` share that owner and supply the native types named by `bindings/sdl-gl.json`. Image uploads consume
  the shared carrier selected by `bindings/sdl-image.json`. The context already
  forwards the operations used by the tween and exposes the anisotropic-filter extension constants and live
  availability query required by Flight's GL runtime. Buffer, framebuffer, renderbuffer, texture, vertex-array,
  shader, program, and uniform-location handles share WebGL-style identity. Explicit deletion invalidates every
  alias, remaining live resources are reclaimed while their context is alive, and cross-context handle use is
  rejected. Its selected command surface includes 2D/3D and compressed texture upload, framebuffer clear/blit and
  readback, active-uniform metadata, and the render state, vertex, draw, and uniform calls used by Flight.
  `bindings/sdl-app.json` layers a browser-shaped application shell over this target for upstream examples. Its
  document attachment is intentionally lightweight, while its request/cancel frame queue and SDL input bridge
  preserve ordered animation turns and route keyboard, pointer, and wheel events into registered listeners on
  `window` and `GlCanvas`. SDL window focus, visibility, minimize, and restore events update the document facade and
  dispatch its focus/page lifecycle callbacks. The input surface also exposes value-owned standard-layout gamepad
  snapshots through `navigator.getGamepads()`, a common event carrier for Flight's base-event narrowing, and an
  identity-preserving typed `CustomEvent<T>` detail carrier.

  The document facade also covers the concrete button, details, div, heading, input, label, option, paragraph,
  select, span, and style element types used by the upstream examples. `document.createElement()` returns one C++
  facade because the compiler retains `auto` for its tag-dependent TypeScript result; that facade exposes the DOM
  value surface directly and lazily creates an SDL GL canvas when a canvas context is requested. It does not
  implement a Canvas2D renderer.
- `Flight::HostSdlVulkan` copies the required instance extension names and owns the `VkSurfaceKHR` returned by SDL.
- `Flight::HostSdlWgpu` owns a type-erased native WebGPU surface and typed shared WebGPU object handles through
  callbacks supplied by a Dawn or wgpu-native adapter. Handle copies and weak cache keys preserve identity, provider
  releases run exactly once, adapter capabilities remain value-owned metadata, and external image copies use the
  same decoded source as GL. It intentionally adds no WebGPU implementation dependency.

The composed SDK sweeps also include `bindings/web-types.json`. That profile maps standard Canvas, WebGPU,
image-smoothing, and permission string-literal domains to `flight::String`. It also supplies portable
`DOMPointInit` and `CanvasRenderingContext2DSettings` value dictionaries, preserving optional-member presence. It
selects no renderer or platform provider and keeps object-handle names available until a concrete backend profile
supplies them.

`bindings/sdl-image.json` maps the DOM image, video, bitmap, offscreen-canvas, SVG image, video-frame, GL image-source,
and WebGPU external-copy type domains to one shared source and weak-key policy. Browser constructor values remain
unbound until flight-compiler can lower external runtime type tests against the retained source kind.

`bindings/sdl-wgpu.json` supplies the WebGPU object handles, adapter features/limits,
device/origin/vertex/external-image/sampler descriptors, bind-group resources and layouts, buffer and texture
transfer descriptors, iterable extents,
shared buffer/view sources, blend and stencil pipeline values, and standard buffer, texture, shader, color-write,
and map-mode flags. Optional descriptor fields retain their source presence for the provider, and copied buffers and
views retain their backing identity.
It is a provider ABI rather than a renderer: device methods remain the responsibility of the selected Dawn or
wgpu-native adapter. Run `npm run sdk:generate:sdl-wgpu` for that profile alone, or `npm run sdk:generate:sdl` for
the complete GL + WebGPU + application-shell inventory.

An installed consumer selects only the backend it uses:

```cmake
find_package(FlightCpp 0.1 CONFIG REQUIRED)
target_link_libraries(my_gl_app PRIVATE Flight::HostSdlGl)
# or: Flight::HostSdlVulkan / Flight::HostSdlWgpu
```

## Ownership and event loop

SDL video operations must run on the main thread. Construct `Host` before `Window`; destroy GL contexts and Vulkan or
WebGPU surfaces before their window; destroy windows before `Host`. The types are move-only so ownership transfers are
explicit.

```cpp
#include <flight/host_sdl/audio.hpp>
#include <flight/host_sdl/host.hpp>
#include <flight/host_sdl/window.hpp>

int main() {
  flight::host_sdl::Host host;
  flight::host_sdl::SdlAudioDeviceBackend audio;
  flight::host_sdl::Window window({
      .title = "Flight",
      .width = 1280,
      .height = 720,
      .graphics_api = flight::host_sdl::GraphicsApi::vulkan,
  });
  flight::host_sdl::InputDispatcher input(window.id(), {
      .key_down = [](const flight::host_sdl::InputKeyboardData& data) {
        // Forward synchronously to the generated Flight InputIngressSink adapter.
      },
  });

  SDL_Event event{};
  bool running = true;
  while (running) {
    while (host.poll_event(event)) {
      if (event.type == SDL_EVENT_QUIT) running = false;
      input.dispatch(event);
    }
    host.pump_timers();
    static_cast<void>(audio.pump());
    // Update Flight, render through render-wgpu or render-gl, then present.
  }
}
```

`Window::id()` lets a future Flight input adapter associate an SDL event with the correct application window.
`Window::size()` reports logical units and `Window::pixel_size()` reports the renderable pixel extent for high-density
displays. `Host::pump_timers()` executes the headless binding profile's due timeout and interval callbacks on the SDL
thread; no background timer thread can race Flight state.

`WebPlatformInput` composes the persistent `InputDispatcher` with a `GlCanvas`, and
`pump_animation_frame(timestamp_ms)` runs the callbacks that were pending when that frame began. Callbacks scheduled
by another frame callback remain queued for the next turn, matching the browser ordering used by the examples.
Its SDL window-event path also keeps `document.hidden` and `document.hasFocus()` current and emits
`visibilitychange`, `pagehide`, `pageshow`, `focus`, and `blur`. The event stores accept
`flight::Function<Signature>`, deduplicate its copy-stable identity with the event type and capture flag, and expose
matching removal operations. The binding remains gated on flight-compiler electing that carrier for JavaScript
function values; plain `std::function` copies cannot identify the callback captured by Flight's returned unsubscribe
closure.

`SdlAudioDeviceBackend::pump()` delivers each completed source callback on the pumping thread. Call it once per host
turn, just like `Host::pump_timers()`. Destroying a source suppresses its pending completion, invalid handles follow
Flight's sentinel/no-op contract, and destroying a buffer does not invalidate sources that already acquired it.

`Flight::HostSdlSdkCursor` and Bazel `//:host_sdl_sdk_cursor` populate the committed generated
`flight::types::CursorBackend` record. Copies of the adapter and emitted record share the selected cursor state, and
the native operations stay on the SDL application thread.

`Flight::HostSdlSdkClipboard` and Bazel `//:host_sdl_sdk_clipboard` populate the committed generated
`ClipboardTextBackend` record. It exposes clear, nonempty-text detection, UTF-8 reading, and UTF-8 writing with the
same resolved-task sentinels as the Web host. SDL clipboard calls must run on the application thread. Rich formats,
images, bookmarks, and change notifications are not advertised by this adapter.

`Flight::HostSdlSdkDevice` and Bazel `//:host_sdl_sdk_device` populate the committed generated `DeviceBackend`.
The snapshot reports SDL's attached keyboard and mouse presence, the desktop display containing its window, safe
area, native architecture, logical CPU count, configured RAM, and canonical platform name. It leaves install id,
available memory, physical DPI, OS version, and hardware/product details at Flight's documented sentinels because
SDL does not expose them. Display and safe-area reads resolve the SDL window id each time and return sentinels after
the native window is destroyed.

`Flight::HostSdlSdkPlatform` and Bazel `//:host_sdl_sdk_platform` populate the committed generated
`PlatformBackend`. It writes into Flight's caller-owned `PlatformInfo`, using SDL for the OS name, preferred locale,
and touch-device presence and the native compiler target for architecture, byte order, and pointer width. Version,
OS-build, and distribution values stay empty where SDL has no source for them.

`Flight::HostSdlSdkScreen` and Bazel `//:host_sdl_sdk_screen` populate the committed generated
`ScreenQueryBackend` and `ScreenDetailsBackend`. Query calls enumerate SDL displays into the caller-owned array and
report virtual-desktop bounds, work-area size, density, physical dimensions, refresh and pixel depth, orientation,
HDR state, labels, the primary display, and global pointer position. Native enumeration has no browser-style
permission prompt, so the details record resolves `granted` and `true`. SDL does not expose physical DPI, gamut,
luminance, or portable internal/touch classification; screen-change subscriptions remain a separate event adapter.

`Flight::HostSdlSdkKeyboard` and Bazel `//:host_sdl_sdk_keyboard` populate the committed generated
`SoftKeyboardInfoBackend`, `SoftKeyboardVisibilityBackend`, and `SoftKeyboardChangeBackend`. SDL reports whether the
active driver has an on-screen keyboard, whether it is shown for the bound window, starts and stops native text
input, and emits global shown/hidden events. Pass each polled event to `SdkSoftKeyboardBackend::dispatch()`. Drivers
without on-screen keyboard support return Flight's acquisition/operation failure values. SDL does not expose the
keyboard rectangle, so geometry remains zero; platform-specific style and layout controls are separate capabilities.

`Flight::HostSdlSdkWindow` and Bazel `//:host_sdl_sdk_window` bind an SDL window id to the committed generated
`ApplicationVisibilityBackend` and `FullscreenBackend` records. Generated fullscreen target handles are registered
weakly, unknown handles return `false`, and records retained after native window destruction fail closed. The
fullscreen event callbacks remain absent from the optional fields because the current emitted `std::function`
carrier cannot satisfy callback-identity removal; the same issue gates the non-optional `ApplicationExitBackend`.
The adapter also creates weakly registered generated `InputTargetHandle` values and populates
`InputTargetBackend`, `InputFocusBackend`, `InputDropFileBackend`, and `InputPointerLockBackend`. Pass each polled
event to `SdkWindowBackend::dispatch()` as well as other host dispatchers to deliver focus and file paths. The
returned release closures remove the exact subscription, and pointer lock maps to SDL relative mouse mode.

## Generated SDK wiring lane

The native mechanics are now present. Wiring them to generated Flight contracts remains a narrow integration task:

1. Finish compiler emission of the narrowed Flight `GlContext` interface. The current compiler resolves the SDL/GL
   ambient bindings but leaves its inherited `viewport` member as an unresolved C++ type.
2. Populate that generated callable interface from `WebGl2Context` and connect compressed-extension enum lookup once
   the compiler selects the downstream ordered `Record<String, double>` carrier. The SDL/GL
   binding profile, shared object handle identities and lifetime, image-source weak-key policy, anisotropy carrier,
   context ownership, buffer and texture upload, compressed texture upload, framebuffer clear/blit/readback,
   shader/program compilation, closed state queries, extension availability, state commands, vertex attributes, draw
   calls, uniform uploads, procedure lookup, and presentation path are present. A live compiler fixture emits and compiles exact calls through this native
   context, and the offscreen SDL smoke executes its ordinary texture/readback path. With the
   anisotropy ambient refusal removed, `GlContextRuntime` now reaches the compiler's closed-value proof for one of
   its `WeakMap` fields.
3. `Flight::HostSdlSdkAudio` and Bazel `//:host_sdl_sdk_audio` already populate the emitted
   `flight::types::AudioDeviceBackend` record and execute it against SDL's dummy driver in the host test. The CMake
   target and `sdk_audio.hpp` are build-tree preview surfaces until `Flight::Sdk` is installable. A compiler module
   remap must replace the sound example's `webAudioDeviceBackend` provider. Encoded sound bytes still need a native
   decoder/provider; decoded PCM now uses the portable `flight::AudioBuffer` carrier.
4. `Flight::HostSdlSdkCursor` and Bazel `//:host_sdl_sdk_cursor` populate and execute the emitted
   `flight::types::CursorBackend` record. The compiler source-remap lane must select that native provider in place of
   `createWebCursorBackend` for the interaction and sound examples.
5. `Flight::HostSdlSdkWindow` and Bazel `//:host_sdl_sdk_window` populate the generated visibility, fullscreen
   command, target preparation, focus, file-drop, and pointer-lock records. Compiler adoption of `flight::Function`
   will unlock exact fullscreen event subscriptions and `ApplicationExitBackend`; no identity approximation is
   installed in the meantime.
6. Implement generated `WgpuHostBackend` and `WgpuRenderSurfaceProvider` with a selected Dawn or wgpu-native adapter.
   `WgpuSurfaceCallbacks` is the stable point where that dependency enters.
7. Adapt `InputDispatcher`'s normalized records into the generated Flight input types once `InputPointerData` and
   `InputIngressBackend` clear their current generated dependency refusals.
8. Clear the remaining SDK and example compiler refusals, then replace the handwritten tween loop with the generated
   application module. The repository now selects and compiles all upstream WebGL example sources through a recorded
   source remap and the SDL application-shell profile.

The corresponding compiler work is recorded in [the upstream request](upstream-flight-compiler-request.md). The
installed host package does not wait for those compiler changes; the preview adapter is isolated so compiler-driven
type changes do not alter SDL ownership or the native playback implementation.

Run `npm run sdk:generate:sdl` to audit the full maintained host profile. It composes the runtime, headless, SDL/GL,
and SDL application-shell manifests under `out/sdk-sdl`; `npm run sdk:compile:sdl` writes its independent-header
report. The narrower `sdk:generate:sdl-gl` command remains useful when application-shell ambient bindings are not
wanted.
