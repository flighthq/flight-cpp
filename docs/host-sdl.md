# SDL host bring-up

`host_sdl` is the handwritten native boundary for Flight applications running in an SDL process. It owns platform
lifecycle and graphics-handle acquisition. It does not implement a scene renderer: transpiled upstream
`@flighthq/render-gl`, `@flighthq/scene2d-gl`, `@flighthq/render-wgpu`, and `@flighthq/scene2d-wgpu` retain that work.

## Intended source and target layout

```text
include/flight/host_sdl/
  host.hpp
  window.hpp
  gl_backend.hpp
  wgpu_backend.hpp
src/host_sdl/
  host.cpp
  window.cpp
  gl_backend.cpp
  wgpu_backend.cpp
```

The eventual CMake targets are deliberately separable:

- `Flight::HostSdl` owns SDL initialization, windows, events, timing, and input ingress.
- `Flight::HostSdlGl` requests an OpenGL ES 3 context and adapts the narrowed upstream `GlContext` contract to native
  GLES handles and calls.
- `Flight::HostSdlWgpu` creates a WebGPU surface for an SDL window through Dawn or `wgpu-native`, then implements the
  upstream `WgpuHostBackend` and `WgpuRenderSurfaceProvider` seams.

Keeping the graphics adapters separate lets an application select one backend and avoids making SDL or a native
WebGPU implementation a dependency of `Flight::Cpp` or `Flight::Sdk`.

## Bring-up order

1. Make the generated `@flighthq/types` modules containing `GlContext`, `WgpuHostBackend`,
   `WgpuPresentationSurface`, and `WgpuRenderSurfaceProvider` compile. These are the contracts the host implements.
2. Add the optional `Flight::HostSdl` target and a lifecycle smoke executable that opens a window, polls events, and
   exits cleanly. No renderer is involved.
3. Add one graphics adapter. WGPU has explicit native acquisition and presentation seams upstream; GL has a narrow
   interface but still requires a WebGL2-to-GLES object adapter.
4. Transpile the matching render and scene packages and render a solid shape. Text, Canvas raster fallback, asset
   loading, and device-loss recovery are later vertical slices.
5. Add the other graphics adapter after the first path proves the generated SDK/host boundary.

## Upstream compiler lane

`generated/refusals.json` is the source of work for compiler bring-up. The first priorities for this host are
multi-module type closure, explicit C++ namespace remapping from `@flighthq/*` to `flight`, WebGPU ambient type
bindings, and executable module initialization. Regenerating the inventory after each compiler pin update shows
exactly which host and renderer modules became available.
