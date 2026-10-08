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

# These packages are not executable ports. Their TypeScript examples require
# @flighthq/render-gl and @flighthq/scene2d-gl or @flighthq/scene3d-gl. Those
# packages have no generated C++ headers because generation currently does not
# terminate in @flighthq/render-gl (see deferred-packages.json). Keep them out
# of UPSTREAM_EXAMPLES: a hand-painted lookalike is not the upstream example.
set(UPSTREAM_EXAMPLES_BLOCKED_SCENE2D_GL
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

set(UPSTREAM_EXAMPLES_BLOCKED_SCENE3D_GL
  awd2loading
  instanced_mesh
  materialshowcase
  scene_fire
  scene_picking
  scene3d
)

if(TARGET Flight::HostSdl)
  add_library(flight_cpp_upstream_sdl_host STATIC
    sdl_host.cpp
  )
  target_link_libraries(flight_cpp_upstream_sdl_host PUBLIC Flight::HostSdl Flight::SdkPreview)
  target_include_directories(flight_cpp_upstream_sdl_host PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
  flight_cpp_set_project_warnings(flight_cpp_upstream_sdl_host)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(flight_cpp_upstream_sdl_host PRIVATE
      -Wno-error=missing-field-initializers
    )
  endif()

  set(flight_cpp_upstream_targets)
  foreach(example IN LISTS UPSTREAM_EXAMPLES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${example}.cpp")
      add_executable(flight_cpp_upstream_${example} ${example}.cpp)
      list(APPEND flight_cpp_upstream_targets flight_cpp_upstream_${example})
      target_link_libraries(flight_cpp_upstream_${example} PRIVATE
        flight_cpp_upstream_sdl_host
        Flight::SdkPreview
      )
      if(example STREQUAL "sound")
        target_link_libraries(flight_cpp_upstream_${example} PRIVATE Flight::HostSdlSdkAudio)
      endif()
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
      if(BUILD_TESTING)
        add_test(
          NAME flight_cpp_upstream_${example}_smoke
          COMMAND flight_cpp_upstream_${example} --smoke
        )
        set_tests_properties(
          flight_cpp_upstream_${example}_smoke
          PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy"
        )
      endif()
    endif()
  endforeach()
  add_custom_target(flight_cpp_upstream_examples DEPENDS ${flight_cpp_upstream_targets})
endif()
