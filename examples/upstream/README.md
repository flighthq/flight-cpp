# Upstream Flight example ports

The targets in this directory are audited native SDL ports of the pinned Flight TypeScript examples
under `.dependencies/flight/examples/packages`. They open an 800 by 600 resizable window and render
continuously. SDL replaces the browser host and presentation surface; the example's camera,
collision, clock, motion-path, or spatial behavior still comes from the same
generated Flight APIs used by the TypeScript source. The source comment at the top of each port names
the TypeScript file used as its authority.

These targets require both `FLIGHT_CPP_BUILD_EXAMPLES=ON` and
`FLIGHT_CPP_BUILD_HOST_SDL=ON`. A build without the optional SDL host does not advertise headless
substitutes as examples.

## Runnable ports

| Target suffix | Flight subsystem used for behavior | Behavior preserved from the TypeScript example |
| --- | --- | --- |
| `camera2d` | `flight::camera`, `flight::camera_controls` | Follow camera, bounded world, parallax layers, keyboard movement, pointer-centered wheel zoom |
| `clock` | `flight::clock` | Parent/child clock hierarchy, effective pause and scale controls, animated shapes and diagnostics |
| `collision` | `flight::collision` | Draggable circle/AABB/polygon colliders, pair testing, overlap color, and MTV arrows |
| `motionpath` | `flight::path`, `flight::motionpath` | Two-segment cubic path, moving oriented arrow, speed and clamp/loop/ping-pong controls |
| `spatial` | `flight::spatial` | Moving indexed boxes, visible grid cells, pair/point/ray/region queries, and hit counts |

Every executable accepts `--smoke`. Smoke mode creates a hidden SDL window, renders three fixed-step
frames, and exits; CTest runs this path with SDL's dummy video driver.

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

The six packages in `UPSTREAM_EXAMPLES_BLOCKED_SCENE3D_GL` are a separate renderer-generation
blocker. Their TypeScript sources select `@flighthq/render-gl` and `@flighthq/scene3d-gl`; the
corresponding C++ headers are absent because generation over `@flighthq/render-gl` currently does not
terminate, as recorded in `deferred-packages.json`. This is not a DOM dependency. The 3D scene, mesh,
camera, lighting, and material data types exist in C++, but the GL scene renderer does not. These
examples must use that renderer once it is generated; a custom 2D projection or hand-painted
material is not an acceptable substitute.

The upstream 2D examples normally select `scene2d-canvas`. Many headers for that package exist, but
critical render-state and shape/text renderer modules are recorded as refused in
`generated/refusals.json`. The audited ports therefore use a small SDL presentation adapter while
retaining the upstream Flight subsystem calls. That adapter is not permission to replace a Flight
algorithm with a lookalike implementation.
