# Naive C++ ports of upstream Flight TypeScript examples. Only examples that
# pass an independent syntax check belong in UPSTREAM_EXAMPLES; see README.md
# for the current blocker inventory and promotion procedure.

set(UPSTREAM_EXAMPLES
)

# Blocked by generated scene2d/display_object.hpp before the example body is
# checked. That partial header also omits createNode2D/createDisplayObject.
set(UPSTREAM_EXAMPLES_BLOCKED_SCENE2D
  adjustments
  benchmark
  bitmapfont_generate
  camera2d
  clock
  collision
  cross_backend_embed
  crossfade
  flowstates
  formatloading
  motionpath
  particleeditor
  particles
  pathboolean
  platformer
  shapes
  snapshot
  spatial
  spring
  spritesheet
  text
  textinput
  tilemap
  tween
  video
)

# Blocked by generated mesh/mesh_geometry_layout.hpp before the example body
# is checked (source->layout.stride is emitted for a Ref-owned layout).
set(UPSTREAM_EXAMPLES_BLOCKED_MESH_LAYOUT
  awd2loading
  instanced_mesh
  materialshowcase
  scene_fire
  scene_picking
  scene3d
)

# These fail earlier in another generated SDK header. They also depend on the
# blocked Scene2D stack after that first failure is repaired.
set(UPSTREAM_EXAMPLES_BLOCKED_OTHER
  interaction  # interaction/hit_tests.hpp calls omitted get_node_parent
  movieclip    # movieclip/movie_clip.hpp calls omitted add_timeline_frame_script
  sound        # media/audio_mixer.hpp calls omitted update_bus_gain_node
)

foreach(example IN LISTS UPSTREAM_EXAMPLES)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${example}.cpp")
    add_executable(flight_cpp_upstream_${example} ${example}.cpp)
    target_link_libraries(flight_cpp_upstream_${example} PRIVATE Flight::SdkPreview)
    flight_cpp_set_project_warnings(flight_cpp_upstream_${example})
  endif()
endforeach()
