# Upstream Flight example ports

This directory contains 34 direct C++ ports of the upstream TypeScript examples. The sources retain
their SDK calls so they can act as integration probes. `CMakeLists.txt` adds a target only after its
translation unit compiles against the pinned SDK; blocked ports remain visible here without breaking
the repository build.

## Current status

No port is promotable at the current pin. Every source was compiled independently after correcting
stale include paths, package names, emitted snake-case fields, and the Camera2D call signatures.
The failures occur inside generated SDK headers before the compiler can validate the rest of each
example body.

| Blocker | Examples | First generated-header failure |
| --- | ---: | --- |
| Scene2D node ownership | 25 | `scene2d/display_object.hpp` passes a nominal `Ref<NodeAny>` to the structural `get_node_runtime` API; the same partial stack omits the node/display-object constructors and hierarchy operations used by the ports. |
| Mesh layout ownership | 6 | `mesh/mesh_geometry_layout.hpp` emits `source->layout.stride` even though `layout` is `Ref<VertexAttributeLayout>`. The affected 3D ports also need constructors currently marked `NOT GENERATED`. |
| Interaction hierarchy | 1 | `interaction/hit_tests.hpp` calls the omitted `get_node_parent` hierarchy function. |
| MovieClip timeline | 1 | `movieclip/movie_clip.hpp` calls the omitted `add_timeline_frame_script` function. |
| Audio mixer | 1 | `media/audio_mixer.hpp` calls the omitted `update_bus_gain_node` function. |

The exact membership of each group is kept next to the build list in `CMakeLists.txt`. These are SDK
emission/runtime gaps rather than missing local headers, so the ports are excluded instead of replacing
their Flight calls with unrelated stand-ins.

## Rechecking a port

From the repository root, syntax-check a source with the same include precedence as `Flight::SdkPreview`:

```sh
c++ -std=c++20 -fmax-errors=1 \
  -Ioverrides/include -Igenerated/include -Iinclude \
  -fsyntax-only examples/upstream/shapes.cpp
```

After the relevant SDK blocker is repaired, fix any newly exposed port-level diagnostics and move the
example name into `UPSTREAM_EXAMPLES`. Then verify the complete project with the standard configure,
build, and CTest commands from the repository `AGENTS.md`.
