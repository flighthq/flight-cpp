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

## Deferred compile probes

The source names in the three `UPSTREAM_EXAMPLES_BLOCKED_*` lists are not example targets. They are
earlier compile probes retained to expose independent generated-SDK gaps in the Scene2D and mesh
stacks. They must not be promoted merely because a translation unit compiles: promotion requires a
windowed port with the same visible behavior and controls as its TypeScript source.
