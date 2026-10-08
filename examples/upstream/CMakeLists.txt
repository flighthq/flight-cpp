# Naive C++ ports of upstream Flight TypeScript examples.
# These are not expected to compile yet — they call SDK functions that
# the compiler may have refused. Builder iterates on getting them to build.

set(UPSTREAM_EXAMPLES
  adjustments
  awd2loading
  benchmark
  bitmapfont_generate
  camera2d
  clock
  collision
  cross_backend_embed
  crossfade
  flowstates
  formatloading
  instanced_mesh
  interaction
  materialshowcase
  motionpath
  movieclip
  particleeditor
  particles
  pathboolean
  platformer
  scene_fire
  scene_picking
  scene3d
  shapes
  snapshot
  sound
  spatial
  spring
  spritesheet
  text
  textinput
  tilemap
  tween
  video
)

foreach(example IN LISTS UPSTREAM_EXAMPLES)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${example}.cpp")
    add_executable(flight_cpp_upstream_${example} ${example}.cpp)
    target_link_libraries(flight_cpp_upstream_${example} PRIVATE Flight::SdkPreview)
    flight_cpp_set_project_warnings(flight_cpp_upstream_${example})
  endif()
endforeach()
