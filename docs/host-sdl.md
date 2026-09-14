# SDL host package

`host_sdl` is the handwritten native boundary for Flight applications running in an SDL 3 process. It owns SDL
subsystem lifetime, windows, event ingress, and native graphics surface acquisition. It does not implement a scene
renderer. Generated `render-gl`, `scene2d-gl`, `render-wgpu`, and `scene2d-wgpu` code remains responsible for
rendering after an adapter supplies its native context or surface.

The package is independent of the generated SDK while the compiler contracts settle. Its public API lives under
`include/flight/host_sdl/`, its implementations live under `src/host_sdl/`, and none of its targets changes the
dependency-free `Flight::Cpp` target.

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
```

Bazel pins and builds SDL 3.4.10 from source for the SDL, GL, and type-erased WGPU targets. CMake, Ninja, Make, M4,
and pkg-config are the build tools used by `rules_foreign_cc`; SDL itself does not need to be installed. Build the
same host test and run the GL smoke with:

```sh
bazel test --config=local-posix //tests:host_sdl_test
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  bazel run --config=local-posix //examples:tween_sdl_gl -- --smoke
```

The public Bazel labels are `//:host_sdl`, `//:host_sdl_gl`, and `//:host_sdl_wgpu`. They and their tests are tagged
`manual`, so a core `bazel test //...` does not fetch or build SDL. CMake remains the complete package path for the
Vulkan surface adapter.

It animates the fifteen easing curves emitted from `examples/tween/source/tween.ts`. Rendering uses the copyable
`GlCanvas` and `WebGl2Context` host seam exposed by `Flight::HostSdlGl`. The example calls the context's reusable
texture upload, framebuffer clear/readback, shader/program, draw, viewport, and presentation operations; no
example-private OpenGL dispatch table or SDL renderer is involved.

Set `-DFLIGHT_CPP_BUILD_HOST_SDL_VULKAN=OFF` for an SDL and GL/WGPU build without Vulkan development files. Native
dependency discovery and target selection belong to CMake or Bazel, so an npm wrapper would only obscure their
options and is not provided.

The build exports four targets through the existing `FlightCpp` package:

- `Flight::HostSdl` initializes ref-counted SDL subsystems, polls or waits for `SDL_Event`, exposes the monotonic SDL
  clock, and owns plain, OpenGL, or Vulkan windows. `InputDispatcher` converts keyboard, text/IME, mouse, wheel, and
  standard-layout gamepad events into records matching Flight's input-ingress data shapes. `Window` controls SDL text
  input and relative-pointer mode for the eventual generated ingress adapter.
- `Flight::HostSdlGl` owns an `SDL_GLContext`, configures OpenGL or OpenGL ES attributes before window creation,
  resolves procedure addresses, controls the swap interval, and swaps the window. Its `GlCanvas` and
  `WebGl2Context` share that owner and supply the native types named by `bindings/sdl-gl.json`. The context already
  forwards the operations used by the tween and exposes the anisotropic-filter extension constants and live
  availability query required by Flight's GL runtime. Buffer, framebuffer, renderbuffer, texture, vertex-array,
  shader, program, and uniform-location handles share WebGL-style identity. Explicit deletion invalidates every
  alias, remaining live resources are reclaimed while their context is alive, and cross-context handle use is
  rejected. Its selected command surface includes 2D/3D and compressed texture upload, framebuffer clear/blit and
  readback, active-uniform metadata, and the render state, vertex, draw, and uniform calls used by Flight.
  `bindings/sdl-app.json` layers a browser-shaped application shell over this target for upstream examples. Its
  document attachment is intentionally lightweight, while its frame queue and SDL input bridge preserve ordered
  animation turns and route keyboard, pointer, and wheel events into registered listeners on `window` and
  `GlCanvas`.
- `Flight::HostSdlVulkan` copies the required instance extension names and owns the `VkSurfaceKHR` returned by SDL.
- `Flight::HostSdlWgpu` owns a type-erased native WebGPU surface through create/destroy callbacks supplied by a Dawn
  or wgpu-native adapter. It intentionally adds no WebGPU implementation dependency.

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
#include <flight/host_sdl/host.hpp>
#include <flight/host_sdl/window.hpp>

int main() {
  flight::host_sdl::Host host;
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

## Generated SDK wiring lane

The native mechanics are now present. Wiring them to generated Flight contracts remains a narrow adapter task:

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
3. Implement generated `WgpuHostBackend` and `WgpuRenderSurfaceProvider` with a selected Dawn or wgpu-native adapter.
   `WgpuSurfaceCallbacks` is the stable point where that dependency enters.
4. Adapt `InputDispatcher`'s normalized records into the generated Flight input types once `InputPointerData` and
   `InputIngressBackend` clear their current generated dependency refusals.
5. Clear the remaining SDK and example compiler refusals, then replace the handwritten tween loop with the generated
   application module. The repository now selects and compiles all upstream WebGL example sources through a recorded
   source remap and the SDL application-shell profile.

The corresponding compiler work is recorded in [the upstream request](upstream-flight-compiler-request.md). The host
package does not need to wait for those compiler changes: it does not yet include generated contracts, and the
eventual adapter can be replaced without changing SDL ownership.
