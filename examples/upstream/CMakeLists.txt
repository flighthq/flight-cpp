# Windowed SDL ports of the upstream Flight TypeScript examples. A port only
# belongs in UPSTREAM_EXAMPLES once it preserves the original scene, animation,
# and controls. The SDL host is optional, so these examples are simply omitted
# from configurations that do not enable FLIGHT_CPP_BUILD_HOST_SDL.

set(UPSTREAM_EXAMPLES
  camera2d
  clock
  collision
  motionpath
  spatial
)

set(UPSTREAM_GL_EXAMPLES
  scene3d
)

# These packages do not yet have faithful SDL ports. Most core APIs are present,
# but a port must keep the upstream Flight subsystem as the source of truth and
# use SDL only for host input and presentation. Keep them out of
# UPSTREAM_EXAMPLES until that is true; a local simulation of the subsystem is
# not the upstream example.
set(UPSTREAM_EXAMPLES_AWAITING_FAITHFUL_SDL_PORT
  adjustments
  benchmark
  bitmapfont_generate
  crossfade
  cross_backend_embed
  flowstates
  formatloading
  interaction
  movieclip
  particleeditor
  particles
  pathboolean
  platformer
  shapes
  snapshot
  sound
  spring
  spritesheet
  text
  textinput
  tilemap
  tween
  video
)

# These TypeScript examples select renderer features that do not yet have a
# complete native GL path. Their real materials, lighting, and picking cannot
# be represented by a custom SDL painter.
set(UPSTREAM_EXAMPLES_BLOCKED_SCENE3D_GL
  awd2loading
  instanced_mesh
  materialshowcase
  scene_fire
  scene_picking
)

set(_flight_cpp_upstream_inventory
  ${UPSTREAM_EXAMPLES}
  ${UPSTREAM_GL_EXAMPLES}
  ${UPSTREAM_EXAMPLES_AWAITING_FAITHFUL_SDL_PORT}
  ${UPSTREAM_EXAMPLES_BLOCKED_SCENE3D_GL}
)
list(LENGTH _flight_cpp_upstream_inventory _flight_cpp_upstream_inventory_count)
set(_flight_cpp_upstream_unique_inventory ${_flight_cpp_upstream_inventory})
list(REMOVE_DUPLICATES _flight_cpp_upstream_unique_inventory)
list(LENGTH _flight_cpp_upstream_unique_inventory _flight_cpp_upstream_unique_inventory_count)
if(NOT _flight_cpp_upstream_inventory_count EQUAL 34 OR
   NOT _flight_cpp_upstream_unique_inventory_count EQUAL 34)
  message(FATAL_ERROR "Every one of the 34 pinned upstream examples must have exactly one status")
endif()

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
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${example}.cpp")
      message(FATAL_ERROR "Audited upstream example ${example} has no source file")
    endif()
    add_executable(flight_cpp_upstream_${example} ${example}.cpp)
    list(APPEND flight_cpp_upstream_targets flight_cpp_upstream_${example})
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
  endforeach()

  if(TARGET Flight::HostSdlGl)
    foreach(example IN LISTS UPSTREAM_GL_EXAMPLES)
      if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${example}.cpp")
        message(FATAL_ERROR "Audited upstream GL example ${example} has no source file")
      endif()
      add_executable(flight_cpp_upstream_${example} ${example}.cpp)
      list(APPEND flight_cpp_upstream_targets flight_cpp_upstream_${example})
      target_link_libraries(flight_cpp_upstream_${example} PRIVATE
        Flight::HostSdlGl
        Flight::SdkPreview
      )
      flight_cpp_set_project_warnings(flight_cpp_upstream_${example})
      if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # The Scene3D include closure instantiates the full generated GL runtime. Suppress the
        # generator's known JavaScript-number and aggregate warnings instead of printing tens of
        # thousands of diagnostics for every example rebuild; warnings from the port remain fatal.
        target_compile_options(flight_cpp_upstream_${example} PRIVATE
          -Wno-conversion
          -Wno-missing-field-initializers
          -Wno-unused-parameter
          -Wno-unused-result
        )
      endif()
      if(BUILD_TESTING)
        add_test(
          NAME flight_cpp_upstream_${example}_smoke
          COMMAND flight_cpp_upstream_${example} --smoke
        )
        if(UNIX AND NOT APPLE)
          set_tests_properties(
            flight_cpp_upstream_${example}_smoke
            PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1"
          )
        endif()
      endif()
    endforeach()
  endif()
  add_custom_target(flight_cpp_upstream_examples DEPENDS ${flight_cpp_upstream_targets})
endif()
