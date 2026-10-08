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
| `awd2loading` | Synthetic AWD2 scene, import summary, rim-lit cube, and orbit/zoom controls |
| `benchmark` | Bouncing quad batch and hold-to-add benchmark interaction |
| `bitmapfont_generate` | Runtime glyph atlas text, recoloring, metrics, and atlas preview |
| `camera2d` | Follow camera, bounded world, parallax layers, keyboard movement, pointer-centered wheel zoom |
| `clock` | Parent/child clock hierarchy, effective pause and scale controls, animated shapes and diagnostics |
| `collision` | Draggable circle/AABB/polygon colliders, pair testing, overlap color, and MTV arrows |
| `cross_backend_embed` | Animated producer shown through copied and live paths with live-view click events |
| `crossfade` | Click-driven idle/locomotion crossfade, blended walk/run pose, additive body motion, progress bar |
| `flowstates` | Boot/menu/play/pause/game-over stack with the original keyboard transitions and overlay behavior |
| `formatloading` | In-memory glTF, TexturePacker, and Tiled fixture presentation |
| `instanced_mesh` | Twenty-five box instances, ground plane, and orbit/zoom controls |
| `interaction` | Precise hover, topmost picking, captured dragging, live overlap reporting, and event HUD |
| `materialshowcase` | Twenty material variants, selection, live controls, and orbit/zoom interaction |
| `motionpath` | Two-segment cubic path, moving oriented arrow, speed and clamp/loop/ping-pong controls |
| `movieclip` | 24-frame labeled timeline, frame scripts, play/stop, stepping, and label navigation |
| `particleeditor` | Spring-follow emitter, editable parameters, blend toggle, and live particle count |
| `particles` | Pointer-following fire plus drifting snow with distinct forces and curves |
| `pathboolean` | Union/intersection/difference/XOR cells with draggable source shapes |
| `platformer` | Title/game-over flow, keyboard movement, jumping, collision, and follow camera |
| `shapes` | All five drawing rows, gradients, stroke variants, and the live interactive star |
| `scene_fire` | Three layered fire emitters, braziers, lighting, post-effect presentation, and orbit camera |
| `scene_picking` | Nine pickable primitives with hover, selection, and automatic orbit |
| `scene3d` | Lit box, metallic sphere, cone, ground plane, shadows, and orbit/zoom controls |
| `snapshot` | Five snapshot slots, restore, interpolation, orbiting player, items, and score |
| `sound` | Procedural tones through the SDL audio backend plus button, master, bus-gain, and pan controls |
| `spatial` | Moving indexed boxes, visible grid cells, pair/point/ray/region queries, and hit counts |
| `spring` | Second-order spring versus fixed-duration tween, click target, and five presets |
| `spritesheet` | Three independent players using loop, speed, and ping-pong playback |
| `text` | The complete text-label, alignment, wrapping, style, background, border, and code gallery |
| `textinput` | Normal, restricted numeric, and password fields with focus, selection, editing, undo, and redo |
| `tilemap` | Procedural 72×54 terrain, camera pan/drag/zoom, and live tile picking |
| `tween` | Fifteen easing tracks with staggered, repeating animation |
| `video` | Three independently transformed and advancing generated video views |

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

All 34 packages in the pinned upstream example tree have a corresponding executable and smoke test.
