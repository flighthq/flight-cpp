# Upstream Flight example ports

The targets in this directory are native SDL ports of the pinned Flight TypeScript examples under
`.dependencies/flight/examples/packages`. They open an 800 by 600 resizable window, render
continuously, and retain the original scene, animation, and controls. The source comment at the top
of each port names the TypeScript file used as its authority.

These targets require both `FLIGHT_CPP_BUILD_EXAMPLES=ON` and
`FLIGHT_CPP_BUILD_HOST_SDL=ON`. A build without the optional SDL host does not advertise headless
substitutes as examples.

## Runnable ports

| Target suffix | Behavior preserved from the TypeScript example |
| --- | --- |
| `adjustments` | Interactive brightness, contrast, hue, and saturation sliders; fused matrix and swatches |
| `camera2d` | Follow camera, bounded world, parallax layers, keyboard movement, pointer-centered wheel zoom |
| `clock` | Parent/child clock hierarchy, effective pause and scale controls, animated shapes and diagnostics |
| `collision` | Draggable circle/AABB/polygon colliders, pair testing, overlap color, and MTV arrows |
| `crossfade` | Click-driven idle/locomotion crossfade, blended walk/run pose, additive body motion, progress bar |
| `flowstates` | Boot/menu/play/pause/game-over stack with the original keyboard transitions and overlay behavior |
| `interaction` | Precise hover, topmost picking, captured dragging, live overlap reporting, and event HUD |
| `motionpath` | Two-segment cubic path, moving oriented arrow, speed and clamp/loop/ping-pong controls |
| `movieclip` | 24-frame labeled timeline, frame scripts, play/stop, stepping, and label navigation |
| `shapes` | All five drawing rows, gradients, stroke variants, and the live interactive star |
| `sound` | Procedural tones through the SDL audio backend plus button, master, bus-gain, and pan controls |
| `spatial` | Moving indexed boxes, visible grid cells, pair/point/ray/region queries, and hit counts |
| `text` | The complete text-label, alignment, wrapping, style, background, border, and code gallery |
| `textinput` | Normal, restricted numeric, and password fields with focus, selection, editing, undo, and redo |

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

The aggregate target builds every runnable port listed above. It deliberately does not claim to
build every package in the upstream tree.

## Blocked upstream examples

The remaining upstream packages are listed in `UPSTREAM_EXAMPLES_BLOCKED_SCENE2D_GL` and
`UPSTREAM_EXAMPLES_BLOCKED_SCENE3D_GL` in `CMakeLists.txt`; they are not executable targets. Their
TypeScript render adapters use `@flighthq/render-gl` plus `@flighthq/scene2d-gl` or
`@flighthq/scene3d-gl`. The corresponding C++ headers are absent because generation over
`@flighthq/render-gl` currently does not terminate, as recorded in the repository's
`deferred-packages.json`.

This is a compiler/runtime integration gap, not a DOM dependency. In particular, the 3D core scene,
mesh, camera, lighting, and material data types exist in C++, but the GL scene renderer does not.
These examples must be ported through that renderer once it is generated; a custom 2D projection or
hand-painted material is not an acceptable substitute.
