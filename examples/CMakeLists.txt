add_executable(flight_cpp_tween_example tween/tween_example.cpp)
target_link_libraries(flight_cpp_tween_example PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_tween_example)

if(TARGET Flight::HostSdlGl)
  add_executable(flight_cpp_tween_sdl_gl_example tween/tween_sdl_gl_example.cpp)
  target_link_libraries(flight_cpp_tween_sdl_gl_example PRIVATE Flight::HostSdlGl)
  flight_cpp_set_project_warnings(flight_cpp_tween_sdl_gl_example)
endif()

if(BUILD_TESTING)
  add_test(NAME flight_cpp.example.tween COMMAND flight_cpp_tween_example)
endif()
