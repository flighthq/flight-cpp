add_executable(flight_cpp_runtime_tests runtime_test.cpp)
target_link_libraries(flight_cpp_runtime_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_runtime_tests)

add_test(NAME flight_cpp.runtime COMMAND flight_cpp_runtime_tests)

add_executable(flight_cpp_intl_type_tests intl_type_test.cpp)
target_link_libraries(flight_cpp_intl_type_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_intl_type_tests)

add_test(NAME flight_cpp.intl_types COMMAND flight_cpp_intl_type_tests)

add_executable(flight_cpp_generated_runtime_tests generated_runtime_test.cpp)
target_link_libraries(flight_cpp_generated_runtime_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_generated_runtime_tests)

add_test(NAME flight_cpp.generated_runtime COMMAND flight_cpp_generated_runtime_tests)

# Needs the generated structural member table, so it links the SDK preview rather than the runtime
# alone; named row members do not resolve without it.
add_executable(flight_cpp_structural_row_tests structural_row_test.cpp)
target_link_libraries(flight_cpp_structural_row_tests PRIVATE Flight::Cpp Flight::SdkPreview)
flight_cpp_set_project_warnings(flight_cpp_structural_row_tests)
add_test(NAME flight_cpp.structural_row COMMAND flight_cpp_structural_row_tests)

add_executable(flight_cpp_generated_collision_tests generated_collision_test.cpp)
target_include_directories(
  flight_cpp_generated_collision_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_collision_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_collision COMMAND flight_cpp_generated_collision_tests)
set_tests_properties(flight_cpp.generated_collision PROPERTIES SKIP_RETURN_CODE 77)

add_executable(flight_cpp_generated_registry_tests generated_registry_test.cpp)
target_include_directories(
  flight_cpp_generated_registry_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_registry_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_registry COMMAND flight_cpp_generated_registry_tests)

add_executable(flight_cpp_generated_animation_tests generated_animation_test.cpp)
target_include_directories(
  flight_cpp_generated_animation_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_animation_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_animation COMMAND flight_cpp_generated_animation_tests)

add_executable(flight_cpp_generated_log_tests generated_log_test.cpp)
target_include_directories(
  flight_cpp_generated_log_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_log_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_log COMMAND flight_cpp_generated_log_tests)

add_executable(flight_cpp_generated_interaction_tests generated_interaction_test.cpp)
target_include_directories(
  flight_cpp_generated_interaction_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_interaction_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_interaction COMMAND flight_cpp_generated_interaction_tests)

add_executable(flight_cpp_generated_statusbar_tests generated_statusbar_test.cpp)
target_include_directories(
  flight_cpp_generated_statusbar_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_statusbar_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_statusbar COMMAND flight_cpp_generated_statusbar_tests)

add_executable(flight_cpp_generated_connectivity_tests generated_connectivity_test.cpp)
target_include_directories(
  flight_cpp_generated_connectivity_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_connectivity_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_connectivity COMMAND flight_cpp_generated_connectivity_tests)

add_executable(flight_cpp_generated_share_tests generated_share_test.cpp)
target_include_directories(
  flight_cpp_generated_share_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_share_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_share COMMAND flight_cpp_generated_share_tests)

add_executable(flight_cpp_generated_updater_tests generated_updater_test.cpp)
target_include_directories(
  flight_cpp_generated_updater_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_updater_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_updater COMMAND flight_cpp_generated_updater_tests)

add_executable(flight_cpp_generated_lifecycle_tests generated_lifecycle_test.cpp)
target_include_directories(
  flight_cpp_generated_lifecycle_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_lifecycle_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_lifecycle COMMAND flight_cpp_generated_lifecycle_tests)

add_executable(flight_cpp_generated_useragent_tests generated_useragent_test.cpp)
target_include_directories(
  flight_cpp_generated_useragent_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_useragent_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_useragent COMMAND flight_cpp_generated_useragent_tests)

add_executable(flight_cpp_generated_dialog_tests generated_dialog_test.cpp)
target_include_directories(
  flight_cpp_generated_dialog_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_dialog_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_dialog COMMAND flight_cpp_generated_dialog_tests)

add_executable(flight_cpp_generated_textshaper_cache_tests generated_textshaper_cache_test.cpp)
target_include_directories(
  flight_cpp_generated_textshaper_cache_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_textshaper_cache_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_textshaper_cache COMMAND flight_cpp_generated_textshaper_cache_tests)

add_executable(flight_cpp_generated_socket_statechart_tests generated_socket_statechart_test.cpp)
target_include_directories(
  flight_cpp_generated_socket_statechart_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_socket_statechart_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_socket_statechart COMMAND flight_cpp_generated_socket_statechart_tests)

add_executable(flight_cpp_generated_surface_tests generated_surface_test.cpp)
target_include_directories(
  flight_cpp_generated_surface_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_surface_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_surface COMMAND flight_cpp_generated_surface_tests)

add_executable(flight_cpp_c_api_tests c_api_test.c)
target_link_libraries(flight_cpp_c_api_tests PRIVATE Flight::C)
flight_cpp_set_project_warnings(flight_cpp_c_api_tests)
add_test(NAME flight_cpp.c_api COMMAND flight_cpp_c_api_tests)

find_package(Threads REQUIRED)

add_executable(flight_cpp_runtime_conformance_tests runtime_conformance_test.cpp)
target_link_libraries(flight_cpp_runtime_conformance_tests PRIVATE Flight::Cpp Threads::Threads)
flight_cpp_set_project_warnings(flight_cpp_runtime_conformance_tests)
add_test(NAME flight_cpp.runtime_conformance COMMAND flight_cpp_runtime_conformance_tests)

add_executable(flight_cpp_c_api_thread_tests c_api_thread_test.cpp)
target_link_libraries(flight_cpp_c_api_thread_tests PRIVATE Flight::C Threads::Threads)
flight_cpp_set_project_warnings(flight_cpp_c_api_thread_tests)
add_test(NAME flight_cpp.c_api_threads COMMAND flight_cpp_c_api_thread_tests)

set(
  flight_cpp_public_headers
  abort
  any
  array
  array_buffer
  array_buffer_view
  attachment
  audio_buffer
  audio_context
  base64
  blob
  boolean
  callable
  canvas_2d
  conditional_facet_ref
  contract
  data_view
  date
  dom_exception
  equality
  erased_ref
  error
  executor
  font_face
  host
  image_data
  intl
  iterable
  iterator
  json
  map
  math
  number
  object
  presence
  reference
  regexp
  record
  rejection
  set
  sequence_view
  string
  stream
  structural_ref
  structured_clone
  symbol
  runtime
  task
  template_argument
  text_decoder
  text_encoder
  typed_array
  uri
  url
  version
  weak_map
  weak_set
  web_types
)

foreach(header IN LISTS flight_cpp_public_headers)
  set(source "${CMAKE_CURRENT_BINARY_DIR}/header_${header}_test.cpp")
  file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/${header}.hpp>\n")
  add_library("flight_cpp_header_${header}_test" OBJECT "${source}")
  target_link_libraries("flight_cpp_header_${header}_test" PRIVATE Flight::Cpp)
  flight_cpp_set_project_warnings("flight_cpp_header_${header}_test")
endforeach()

if(TARGET Flight::HostSdlWgpu)
  add_executable(flight_cpp_host_sdl_tests host_sdl_test.cpp)
  target_link_libraries(
    flight_cpp_host_sdl_tests
    PRIVATE
      Flight::HostSdlGl
      Flight::HostSdlSdkPreview
      Flight::HostSdlWgpu
  )
  flight_cpp_set_project_warnings(flight_cpp_host_sdl_tests)
  add_test(NAME flight_cpp.host_sdl COMMAND flight_cpp_host_sdl_tests)
  set_tests_properties(
    flight_cpp.host_sdl
    PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
  )
endif()

if(TARGET Flight::HostSdlGl)
  set(source "${CMAKE_CURRENT_BINARY_DIR}/header_host_sdl_image_test.cpp")
  file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/host_sdl/image.hpp>\n")
  add_library(flight_cpp_header_host_sdl_image_test OBJECT "${source}")
  target_link_libraries(flight_cpp_header_host_sdl_image_test PRIVATE Flight::HostSdlImage)
  flight_cpp_set_project_warnings(flight_cpp_header_host_sdl_image_test)

  set(source "${CMAKE_CURRENT_BINARY_DIR}/header_host_sdl_webgl_test.cpp")
  file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/host_sdl/webgl.hpp>\n")
  add_library(flight_cpp_header_host_sdl_webgl_test OBJECT "${source}")
  target_link_libraries(flight_cpp_header_host_sdl_webgl_test PRIVATE Flight::HostSdlGl)
  flight_cpp_set_project_warnings(flight_cpp_header_host_sdl_webgl_test)

  set(source "${CMAKE_CURRENT_BINARY_DIR}/header_host_sdl_web_platform_test.cpp")
  file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/host_sdl/web_platform.hpp>\n")
  add_library(flight_cpp_header_host_sdl_web_platform_test OBJECT "${source}")
  target_link_libraries(flight_cpp_header_host_sdl_web_platform_test PRIVATE Flight::HostSdlGl)
  flight_cpp_set_project_warnings(flight_cpp_header_host_sdl_web_platform_test)
endif()

if(TARGET Flight::HostSdl)
  foreach(header IN ITEMS audio cursor input sdk_audio sdk_clipboard sdk_cursor sdk_device sdk_haptics sdk_keyboard sdk_platform sdk_screen sdk_window)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/header_host_sdl_${header}_test.cpp")
    file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/host_sdl/${header}.hpp>\n")
    add_library("flight_cpp_header_host_sdl_${header}_test" OBJECT "${source}")
    target_link_libraries("flight_cpp_header_host_sdl_${header}_test" PRIVATE Flight::HostSdl)
    flight_cpp_set_project_warnings("flight_cpp_header_host_sdl_${header}_test")
  endforeach()
endif()
