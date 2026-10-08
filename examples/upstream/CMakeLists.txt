# Naive C++ ports of upstream Flight TypeScript examples. Only examples that
# pass an independent syntax check belong in UPSTREAM_EXAMPLES; see README.md
# for the current blocker inventory and promotion procedure.

set(UPSTREAM_EXAMPLES
  adjustments
  camera2d
  clock
  collision
  flowstates
  interaction
  motionpath
  movieclip
  shapes
  sound
  spatial
  text
  textinput
)

# Blocked by generated scene2d/display_object.hpp before the example body is
# checked. That partial header also omits createNode2D/createDisplayObject.
set(UPSTREAM_EXAMPLES_BLOCKED_SCENE2D
  benchmark
  bitmapfont_generate
  cross_backend_embed
  crossfade
  formatloading
  particleeditor
  particles
  pathboolean
  platformer
  snapshot
  spring
  spritesheet
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
)

foreach(example IN LISTS UPSTREAM_EXAMPLES)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${example}.cpp")
    add_executable(flight_cpp_upstream_${example} ${example}.cpp)
    target_link_libraries(flight_cpp_upstream_${example} PRIVATE Flight::SdkPreview)
    flight_cpp_set_project_warnings(flight_cpp_upstream_${example})
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      # Generated SDK dependencies still contain intentional no-op parameters, ignored
      # WeakMap erase results, and JavaScript-number indices converted to C++ index types.
      # Keep all other project warnings fatal for the ports.
      target_compile_options(flight_cpp_upstream_${example} PRIVATE
        -Wno-error=conversion
        -Wno-error=float-conversion
        -Wno-error=unused-parameter
        -Wno-error=unused-result
      )
    endif()
  endif()
endforeach()
