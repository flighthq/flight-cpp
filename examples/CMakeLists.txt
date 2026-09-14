add_executable(flight_cpp_tween_example tween/tween_example.cpp)
target_link_libraries(flight_cpp_tween_example PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_tween_example)

add_executable(flight_cpp_sdk_math_example sdk_math_example.cpp)
target_link_libraries(flight_cpp_sdk_math_example PRIVATE Flight::SdkPreview)
flight_cpp_set_project_warnings(flight_cpp_sdk_math_example)

if(TARGET Flight::HostSdlGl)
  add_executable(flight_cpp_tween_sdl_gl_example tween/tween_sdl_gl_example.cpp)
  target_link_libraries(flight_cpp_tween_sdl_gl_example PRIVATE Flight::HostSdlGl)
  flight_cpp_set_project_warnings(flight_cpp_tween_sdl_gl_example)
endif()

if(TARGET Flight::HostSdlSdkAudio)
  add_executable(flight_cpp_sound_sdl_example sound/sound_sdl_example.cpp)
  target_link_libraries(flight_cpp_sound_sdl_example PRIVATE Flight::HostSdlSdkAudio)
  flight_cpp_set_project_warnings(flight_cpp_sound_sdl_example)
endif()

if(BUILD_TESTING)
  add_test(NAME flight_cpp.example.tween COMMAND flight_cpp_tween_example)
  add_test(NAME flight_cpp.example.sdk_math COMMAND flight_cpp_sdk_math_example)
  if(TARGET flight_cpp_sound_sdl_example)
    add_test(NAME flight_cpp.example.sound_sdl COMMAND flight_cpp_sound_sdl_example --smoke)
    set_tests_properties(
      flight_cpp.example.sound_sdl
      PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy"
    )
  endif()
endif()
