# Upstream Flight example ports

The targets in this directory are audited native SDL ports of the pinned Flight TypeScript examples
under `.dependencies/flight/examples/packages`. They open a resizable window at the authority's
canvas size (800 by 600 except for spatial's 800 by 500) and render continuously. SDL replaces the
browser host and presentation surface; the example's camera,
collision, clock, motion-path, or spatial behavior still comes from the same
generated Flight APIs used by the TypeScript source. The source comment at the top of each port names
the TypeScript file used as its authority.

These targets require both `FLIGHT_CPP_BUILD_EXAMPLES=ON` and
`FLIGHT_CPP_BUILD_HOST_SDL=ON`; `scene3d` also requires the `Flight::HostSdlGl` target. A build
without the optional SDL host does not advertise headless substitutes as examples.

## Runnable ports

| Target suffix | Flight subsystem used for behavior | Behavior preserved from the TypeScript example |
| --- | --- | --- |
| `camera2d` | `flight::camera`, `flight::camera_controls` | Follow camera, bounded world, parallax layers, keyboard movement, pointer-centered wheel zoom |
| `clock` | `flight::clock` | Parent/child clock hierarchy, effective pause and scale controls, animated shapes and diagnostics |
| `collision` | `flight::collision` | Draggable circle/AABB/polygon colliders, pair testing, overlap color, and MTV arrows |
| `motionpath` | `flight::path`, `flight::motionpath` | Two-segment cubic path, moving oriented arrow, speed and clamp/loop/ping-pong controls |
| `scene3d` | `flight::scene3d_gl`, StandardPbr, effects GL | Procedural meshes, PBR materials, orbit camera, ambient/directional/point lights, shadow map, and HDR multisample presentation |
| `spatial` | `flight::spatial` | Moving indexed boxes, visible grid cells, pair/point/ray/region queries, and hit counts |

Every executable accepts `--smoke`. Smoke mode creates a hidden SDL window, renders three fixed-step
frames, and exits. The camera2d, collision, and spatial ports also assert generated package output,
so a locally simulated frame cannot satisfy their smoke checks. CTest uses SDL's dummy video driver
for painter examples and its offscreen GL driver with software rendering for `scene3d` on Linux.

Use `--capture output.ppm` to save the first rendered frame before presentation. Combine it with
`--smoke` to capture a frame while retaining all three verified frames. The lossless RGB PPM capture
can be converted to PNG for sharing.

```sh
cmake -S . -B build \
  -DBUILD_TESTING=ON \
  -DFLIGHT_CPP_BUILD_EXAMPLES=ON \
  -DFLIGHT_CPP_BUILD_HOST_SDL=ON
cmake --build build --target flight_cpp_upstream_examples
ctest --test-dir build --output-on-failure -R '^flight_cpp_upstream_.*_smoke$'
```

Run an interactive example without `--smoke`, for example:

```sh
./build/examples/upstream/flight_cpp_upstream_camera2d
```

The aggregate target builds every audited runnable port listed above. It deliberately does not claim
to build all 34 packages in the upstream tree. CMake does, however, require all 34 pinned packages to
appear exactly once in the runnable, awaiting-port, or renderer-blocked inventory, and it fails
configuration if a runnable source is missing.

## Non-runnable upstream examples

`UPSTREAM_EXAMPLES_AWAITING_FAITHFUL_SDL_PORT` in `CMakeLists.txt` names the 23 packages that do not
yet have an audited native port. Some of their prior C++ files locally recreated animation, flow,
interaction, movie-clip, shape, mixer, or text-input behavior. Those files were removed: passing a
smoke test is not enough if the example bypasses the Flight API it is meant to exercise. A future
port belongs in `UPSTREAM_EXAMPLES` only when its state and algorithms come from the same generated
Flight APIs as the pinned TypeScript source.

The five packages in `UPSTREAM_EXAMPLES_BLOCKED_SCENE3D_GL` require additional material, asset,
particle, instancing, or picking paths. The base Scene3D render chain is now runnable: the `scene3d`
port uses the generated mesh builders, StandardPbr shader/material renderer, light packing, shadow
map, render pass, and effects pipeline through the SDL GLES host. Its native default registry
contains the verified StandardPbr implementation; missing material kinds stay observable registry
misses rather than being mapped to a look-alike renderer. The remaining examples move to the
runnable list only as their actual upstream paths compile and run.

The upstream 2D examples normally select `scene2d-canvas`. Many headers for that package exist, but
there is no native Canvas2D contract or Cairo/Skia/Pango implementation in this repository. The SDL
presentation adapter rasterizes its own primitives with `SDL_Renderer` and uploads Flight's glyph
outlines as SDL textures; it is not a Canvas substitute. Canvas-dependent examples therefore stay
capability-gated unless a real `scene2d-canvas-cairo` or `scene2d-canvas-skia` backend implements the
upstream contract. The adapter is only used where the original example's Flight subsystem—not
Canvas—is the behavior under test.
