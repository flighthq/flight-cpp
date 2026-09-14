add_executable(flight_cpp_runtime_tests runtime_test.cpp)
target_link_libraries(flight_cpp_runtime_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_runtime_tests)

add_test(NAME flight_cpp.runtime COMMAND flight_cpp_runtime_tests)

add_executable(flight_cpp_generated_runtime_tests generated_runtime_test.cpp)
target_link_libraries(flight_cpp_generated_runtime_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_generated_runtime_tests)

add_test(NAME flight_cpp.generated_runtime COMMAND flight_cpp_generated_runtime_tests)

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
  array
  array_buffer
  array_buffer_view
  audio_buffer
  base64
  blob
  boolean
  callable
  conditional_facet_ref
  contract
  data_view
  date
  dom_exception
  equality
  error
  executor
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
  symbol
  runtime
  task
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
      Flight::HostSdlSdkAudio
      Flight::HostSdlSdkClipboard
      Flight::HostSdlSdkCursor
      Flight::HostSdlSdkDevice
      Flight::HostSdlSdkPlatform
      Flight::HostSdlSdkWindow
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
  foreach(header IN ITEMS audio cursor input sdk_audio sdk_clipboard sdk_cursor sdk_device sdk_platform sdk_window)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/header_host_sdl_${header}_test.cpp")
    file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/host_sdl/${header}.hpp>\n")
    add_library("flight_cpp_header_host_sdl_${header}_test" OBJECT "${source}")
    target_link_libraries("flight_cpp_header_host_sdl_${header}_test" PRIVATE Flight::HostSdl)
    flight_cpp_set_project_warnings("flight_cpp_header_host_sdl_${header}_test")
  endforeach()
endif()
