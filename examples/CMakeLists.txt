add_executable(flight_cpp_tween_example tween/tween_example.cpp)
target_link_libraries(flight_cpp_tween_example PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_tween_example)

if(BUILD_TESTING)
  add_test(NAME flight_cpp.example.tween COMMAND flight_cpp_tween_example)
endif()
