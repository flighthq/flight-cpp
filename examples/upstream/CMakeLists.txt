# Windowed SDL ports of the upstream Flight TypeScript examples. A port only
# belongs in UPSTREAM_EXAMPLES once it preserves the original scene, animation,
# and controls. The SDL host is optional, so these examples are simply omitted
# from configurations that do not enable FLIGHT_CPP_BUILD_HOST_SDL.

set(UPSTREAM_EXAMPLES
  adjustments
  camera2d
  clock
  collision
  crossfade
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

if(TARGET Flight::HostSdl)
  add_library(flight_cpp_upstream_sdl_host STATIC sdl_host.cpp)
  target_link_libraries(flight_cpp_upstream_sdl_host PUBLIC Flight::HostSdl)
  target_include_directories(flight_cpp_upstream_sdl_host PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
  flight_cpp_set_project_warnings(flight_cpp_upstream_sdl_host)

  foreach(example IN LISTS UPSTREAM_EXAMPLES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${example}.cpp")
      add_executable(flight_cpp_upstream_${example} ${example}.cpp)
      target_link_libraries(flight_cpp_upstream_${example} PRIVATE
        flight_cpp_upstream_sdl_host
        Flight::SdkPreview
      )
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
endif()
