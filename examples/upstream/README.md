# Upstream Flight example ports

The targets in this directory are audited native SDL ports of the pinned Flight TypeScript examples
under `.dependencies/flight/examples/packages`. They open a resizable window at the authority's
canvas size (800 by 600 except for spatial's 800 by 500) and render continuously. SDL replaces the
browser host and presentation surface; the example's camera,
collision, clock, motion-path, or spatial behavior still comes from the same
generated Flight APIs used by the TypeScript source. The source comment at the top of each port names
the TypeScript file used as its authority.

These targets require both `FLIGHT_CPP_BUILD_EXAMPLES=ON` and
`FLIGHT_CPP_BUILD_HOST_SDL=ON`; `camera2d` and `scene3d` also require the `Flight::HostSdlGl` target. A build
without the optional SDL host does not advertise headless substitutes as examples.

The GL ports use the installed `flight::host_sdl::GlApplication` window and frame-loop API from
`Flight::HostSdlGl`. Downstream applications can use the same API without linking this directory;
see `docs/host-sdl.md` for timing, resize, event, presentation, and shutdown behavior.

## Runnable ports

| Target suffix | Flight subsystem used for behavior | Behavior preserved from the TypeScript example |
| --- | --- | --- |
| `camera2d` | `flight::camera`, `flight::camera_controls`, `flight::scene2d_gl` | Follow camera, bounded world, parallax layers, keyboard movement, pointer-centered wheel zoom; genuine GL scene graph, tessellated shapes, and textured Sprite batches |
| `clock` | `flight::clock` | Parent/child clock hierarchy, effective pause and scale controls, animated shapes and diagnostics |
| `collision` | `flight::collision` | Draggable circle/AABB/polygon colliders, pair testing, overlap color, and MTV arrows |
| `motionpath` | `flight::path`, `flight::motionpath` | Two-segment cubic path, moving oriented arrow, speed and clamp/loop/ping-pong controls |
| `scene3d` | `flight::scene3d_gl`, StandardPbr, effects GL | Procedural meshes, PBR materials, orbit camera, ambient/directional/point lights, shadow map, and HDR multisample presentation |
| `spatial` | `flight::spatial` | Moving indexed boxes, visible grid cells, pair/point/ray/region queries, and hit counts |

Every executable accepts `--smoke`. Smoke mode creates a hidden SDL window, renders three fixed-step
frames, and exits. The camera2d, collision, and spatial ports also assert generated package output,
so a locally simulated frame cannot satisfy their smoke checks. CTest uses SDL's dummy video driver
for the four painter examples and its offscreen GL driver with software rendering for `camera2d`
and `scene3d` on Linux. Camera2D's CTest also captures its first actual GL frame as
`examples/upstream/camera2d-smoke.ppm` inside the build directory.

Camera2D follows the pinned `camera2d/src/app.ts` and `render.webgl.ts` authorities. Its stars,
mountains, clouds, landmarks, player, world border, and visible-bounds outline are real Flight Shapes
under the authored parallax/world containers. The grid uses solid one-pixel Bitmap textures stretched
by Flight Sprites, submitted through the standard material/texture resolver and instanced quad-batch
writer. Flight prepares the proxies, traverses the graph, tessellates fill/stroke paths, uploads
textures, issues GL draws, and presents the frame. SDL rasterizes only the separate HUD asset, which
is itself uploaded and drawn as a HUD Sprite; it never paints world or parallax geometry.

The Camera2D smoke retains the camera/follow/zoom/bounds/parallax assertions and additionally checks
layer order and transforms, all authored Shape meshes, concrete player ownership, Sprite upload/batch
resources, and the yellow player's pixels in every real framebuffer. Native owner tests cover cache
identity, invalidation, reparenting, teardown, and affine instance values. The installed native GL
binding is still partial: custom runtime factories, arbitrary base-typed callbacks, configured clip
hooks, and canvas/raster Shape replay are not claimed as supported by this example.

`camera2d` and `scene3d` exercise their actual Flight GL renderers. `clock`, `collision`, `motionpath`,
and `spatial` continue to use the SDL painter for presentation while testing their listed Flight
behavior APIs; they do not provide Scene2D GL coverage.

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
upstream contract. The four remaining painter ports use that adapter where the original example's
Flight behavior subsystem is under test. Camera2D uses the native Scene2D GL path described above.
