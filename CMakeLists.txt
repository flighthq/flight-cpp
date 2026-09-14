cmake_minimum_required(VERSION 3.20)

project(
  FlightCpp
  VERSION 0.1.0
  DESCRIPTION "C++20 semantic runtime for Flight compiler output"
  LANGUAGES C CXX
)

include(CMakePackageConfigHelpers)
include(CTest)
include(GNUInstallDirs)

option(FLIGHT_CPP_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(FLIGHT_CPP_BUILD_BENCHMARKS "Build deterministic runtime performance smoke gates" OFF)
option(FLIGHT_CPP_BUILD_EXAMPLES "Build native Flight examples" OFF)
option(FLIGHT_CPP_BUILD_HOST_SDL "Build the optional SDL 3 native host package" OFF)
option(FLIGHT_CPP_BUILD_HOST_SDL_VULKAN "Build Vulkan support when the SDL host package is enabled" ON)
option(FLIGHT_CPP_WARNINGS_AS_ERRORS "Treat warnings in flight-cpp's own targets as errors" OFF)
set(FLIGHT_CPP_ABI_VERSION 1)

function(flight_cpp_set_project_warnings target)
  if(MSVC)
    target_compile_options("${target}" PRIVATE /W4 "$<$<COMPILE_LANGUAGE:CXX>:/permissive->")
    if(FLIGHT_CPP_WARNINGS_AS_ERRORS)
      target_compile_options("${target}" PRIVATE /WX)
    endif()
  else()
    target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    if(FLIGHT_CPP_WARNINGS_AS_ERRORS)
      target_compile_options("${target}" PRIVATE -Werror)
    endif()
  endif()
endfunction()

add_library(flight_cpp INTERFACE)
add_library(Flight::Cpp ALIAS flight_cpp)
set_target_properties(flight_cpp PROPERTIES EXPORT_NAME Cpp)
target_compile_features(flight_cpp INTERFACE cxx_std_20)
target_include_directories(
  flight_cpp
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)

include(generated/cmake/FlightSdkHeaders.cmake)
add_library(flight_sdk_preview INTERFACE)
add_library(Flight::SdkPreview ALIAS flight_sdk_preview)
target_sources(flight_sdk_preview INTERFACE ${FLIGHT_SDK_GENERATED_HEADERS})
target_include_directories(
  flight_sdk_preview
  INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/generated/include>"
)
target_link_libraries(flight_sdk_preview INTERFACE Flight::Cpp)

add_library(flight_cpp_c src/c_runtime.cpp)
add_library(Flight::C ALIAS flight_cpp_c)
set_target_properties(
  flight_cpp_c
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    EXPORT_NAME C
    POSITION_INDEPENDENT_CODE ON
    SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
    VERSION "${PROJECT_VERSION}"
    VISIBILITY_INLINES_HIDDEN YES
)
target_link_libraries(flight_cpp_c PUBLIC Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_c)
if(BUILD_SHARED_LIBS)
  target_compile_definitions(flight_cpp_c PRIVATE FLIGHT_CPP_C_EXPORTS PUBLIC FLIGHT_CPP_C_SHARED)
endif()

if(FLIGHT_CPP_ENABLE_SANITIZERS)
  if(MSVC)
    message(FATAL_ERROR "FLIGHT_CPP_ENABLE_SANITIZERS currently requires GCC, Clang, or AppleClang")
  endif()
  target_compile_options(flight_cpp INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(flight_cpp INTERFACE -fsanitize=address,undefined)
endif()

set(flight_cpp_install_targets flight_cpp flight_cpp_c)

if(FLIGHT_CPP_BUILD_HOST_SDL)
  find_package(SDL3 3.2 CONFIG REQUIRED)

  add_library(
    flight_host_sdl
    src/host_sdl/audio.cpp
    src/host_sdl/cursor.cpp
    src/host_sdl/host.cpp
    src/host_sdl/input.cpp
    src/host_sdl/window.cpp
  )
  add_library(Flight::HostSdl ALIAS flight_host_sdl)
  set_target_properties(
    flight_host_sdl
    PROPERTIES
      EXPORT_NAME HostSdl
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(flight_host_sdl PUBLIC Flight::Cpp SDL3::SDL3)
  flight_cpp_set_project_warnings(flight_host_sdl)

  add_library(flight_host_sdl_image INTERFACE)
  add_library(Flight::HostSdlImage ALIAS flight_host_sdl_image)
  set_target_properties(flight_host_sdl_image PROPERTIES EXPORT_NAME HostSdlImage)
  target_link_libraries(flight_host_sdl_image INTERFACE Flight::Cpp)

  add_library(flight_host_sdl_sdk_audio src/host_sdl/sdk_audio.cpp)
  add_library(Flight::HostSdlSdkAudio ALIAS flight_host_sdl_sdk_audio)
  set_target_properties(
    flight_host_sdl_sdk_audio
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(
    flight_host_sdl_sdk_audio
    PUBLIC Flight::HostSdl Flight::SdkPreview
  )
  flight_cpp_set_project_warnings(flight_host_sdl_sdk_audio)

  add_library(flight_host_sdl_sdk_clipboard src/host_sdl/sdk_clipboard.cpp)
  add_library(Flight::HostSdlSdkClipboard ALIAS flight_host_sdl_sdk_clipboard)
  set_target_properties(
    flight_host_sdl_sdk_clipboard
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(
    flight_host_sdl_sdk_clipboard
    PUBLIC Flight::HostSdl Flight::SdkPreview
  )
  flight_cpp_set_project_warnings(flight_host_sdl_sdk_clipboard)

  add_library(flight_host_sdl_sdk_cursor src/host_sdl/sdk_cursor.cpp)
  add_library(Flight::HostSdlSdkCursor ALIAS flight_host_sdl_sdk_cursor)
  set_target_properties(
    flight_host_sdl_sdk_cursor
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(
    flight_host_sdl_sdk_cursor
    PUBLIC Flight::HostSdl Flight::SdkPreview
  )
  flight_cpp_set_project_warnings(flight_host_sdl_sdk_cursor)

  add_library(flight_host_sdl_sdk_device src/host_sdl/sdk_device.cpp)
  add_library(Flight::HostSdlSdkDevice ALIAS flight_host_sdl_sdk_device)
  set_target_properties(
    flight_host_sdl_sdk_device
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(
    flight_host_sdl_sdk_device
    PUBLIC Flight::HostSdl Flight::SdkPreview
  )
  flight_cpp_set_project_warnings(flight_host_sdl_sdk_device)

  add_library(flight_host_sdl_sdk_haptics src/host_sdl/sdk_haptics.cpp)
  add_library(Flight::HostSdlSdkHaptics ALIAS flight_host_sdl_sdk_haptics)
  set_target_properties(
    flight_host_sdl_sdk_haptics
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(
    flight_host_sdl_sdk_haptics
    PUBLIC Flight::HostSdl Flight::SdkPreview
  )
  flight_cpp_set_project_warnings(flight_host_sdl_sdk_haptics)

  add_library(flight_host_sdl_sdk_keyboard src/host_sdl/sdk_keyboard.cpp)
  add_library(Flight::HostSdlSdkKeyboard ALIAS flight_host_sdl_sdk_keyboard)
  set_target_properties(
    flight_host_sdl_sdk_keyboard
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(
    flight_host_sdl_sdk_keyboard
    PUBLIC Flight::HostSdl Flight::SdkPreview
  )
  flight_cpp_set_project_warnings(flight_host_sdl_sdk_keyboard)

  add_library(flight_host_sdl_sdk_platform src/host_sdl/sdk_platform.cpp)
  add_library(Flight::HostSdlSdkPlatform ALIAS flight_host_sdl_sdk_platform)
  set_target_properties(
    flight_host_sdl_sdk_platform
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(
    flight_host_sdl_sdk_platform
    PUBLIC Flight::HostSdl Flight::SdkPreview
  )
  flight_cpp_set_project_warnings(flight_host_sdl_sdk_platform)

  add_library(flight_host_sdl_sdk_screen src/host_sdl/sdk_screen.cpp)
  add_library(Flight::HostSdlSdkScreen ALIAS flight_host_sdl_sdk_screen)
  set_target_properties(
    flight_host_sdl_sdk_screen
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(
    flight_host_sdl_sdk_screen
    PUBLIC Flight::HostSdl Flight::SdkPreview
  )
  flight_cpp_set_project_warnings(flight_host_sdl_sdk_screen)

  add_library(flight_host_sdl_sdk_window src/host_sdl/sdk_window.cpp)
  add_library(Flight::HostSdlSdkWindow ALIAS flight_host_sdl_sdk_window)
  set_target_properties(
    flight_host_sdl_sdk_window
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(
    flight_host_sdl_sdk_window
    PUBLIC Flight::HostSdl Flight::SdkPreview
  )
  flight_cpp_set_project_warnings(flight_host_sdl_sdk_window)

  add_library(
    flight_host_sdl_gl
    src/host_sdl/gl.cpp
    src/host_sdl/web_platform.cpp
    src/host_sdl/webgl.cpp
  )
  add_library(Flight::HostSdlGl ALIAS flight_host_sdl_gl)
  set_target_properties(
    flight_host_sdl_gl
    PROPERTIES
      EXPORT_NAME HostSdlGl
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(flight_host_sdl_gl PUBLIC Flight::HostSdl Flight::HostSdlImage)
  flight_cpp_set_project_warnings(flight_host_sdl_gl)

  add_library(flight_host_sdl_wgpu src/host_sdl/wgpu.cpp)
  add_library(Flight::HostSdlWgpu ALIAS flight_host_sdl_wgpu)
  set_target_properties(
    flight_host_sdl_wgpu
    PROPERTIES
      EXPORT_NAME HostSdlWgpu
      POSITION_INDEPENDENT_CODE ON
      SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
      VERSION "${PROJECT_VERSION}"
  )
  target_link_libraries(flight_host_sdl_wgpu PUBLIC Flight::HostSdl Flight::HostSdlImage)
  flight_cpp_set_project_warnings(flight_host_sdl_wgpu)

  list(
    APPEND flight_cpp_install_targets
    flight_host_sdl
    flight_host_sdl_gl
    flight_host_sdl_image
    flight_host_sdl_wgpu
  )

  if(FLIGHT_CPP_BUILD_HOST_SDL_VULKAN)
    find_package(Vulkan REQUIRED)
    add_library(flight_host_sdl_vulkan src/host_sdl/vulkan.cpp)
    add_library(Flight::HostSdlVulkan ALIAS flight_host_sdl_vulkan)
    set_target_properties(
      flight_host_sdl_vulkan
      PROPERTIES
        EXPORT_NAME HostSdlVulkan
        POSITION_INDEPENDENT_CODE ON
        SOVERSION "${FLIGHT_CPP_ABI_VERSION}"
        VERSION "${PROJECT_VERSION}"
    )
    target_link_libraries(flight_host_sdl_vulkan PUBLIC Flight::HostSdl Vulkan::Vulkan)
    flight_cpp_set_project_warnings(flight_host_sdl_vulkan)
    list(APPEND flight_cpp_install_targets flight_host_sdl_vulkan)
  endif()

  if(BUILD_SHARED_LIBS)
    target_compile_definitions(
      flight_host_sdl
      PRIVATE FLIGHT_HOST_SDL_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_SHARED
    )
    target_compile_definitions(
      flight_host_sdl_gl
      PRIVATE FLIGHT_HOST_SDL_GL_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_GL_SHARED
    )
    target_compile_definitions(
      flight_host_sdl_wgpu
      PRIVATE FLIGHT_HOST_SDL_WGPU_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_WGPU_SHARED
    )
    target_compile_definitions(
      flight_host_sdl_sdk_audio
      PRIVATE FLIGHT_HOST_SDL_SDK_AUDIO_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_SDK_AUDIO_SHARED
    )
    target_compile_definitions(
      flight_host_sdl_sdk_clipboard
      PRIVATE FLIGHT_HOST_SDL_SDK_CLIPBOARD_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_SDK_CLIPBOARD_SHARED
    )
    target_compile_definitions(
      flight_host_sdl_sdk_cursor
      PRIVATE FLIGHT_HOST_SDL_SDK_CURSOR_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_SDK_CURSOR_SHARED
    )
    target_compile_definitions(
      flight_host_sdl_sdk_device
      PRIVATE FLIGHT_HOST_SDL_SDK_DEVICE_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_SDK_DEVICE_SHARED
    )
    target_compile_definitions(
      flight_host_sdl_sdk_haptics
      PRIVATE FLIGHT_HOST_SDL_SDK_HAPTICS_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_SDK_HAPTICS_SHARED
    )
    target_compile_definitions(
      flight_host_sdl_sdk_keyboard
      PRIVATE FLIGHT_HOST_SDL_SDK_KEYBOARD_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_SDK_KEYBOARD_SHARED
    )
    target_compile_definitions(
      flight_host_sdl_sdk_platform
      PRIVATE FLIGHT_HOST_SDL_SDK_PLATFORM_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_SDK_PLATFORM_SHARED
    )
    target_compile_definitions(
      flight_host_sdl_sdk_screen
      PRIVATE FLIGHT_HOST_SDL_SDK_SCREEN_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_SDK_SCREEN_SHARED
    )
    target_compile_definitions(
      flight_host_sdl_sdk_window
      PRIVATE FLIGHT_HOST_SDL_SDK_WINDOW_EXPORTS
      PUBLIC FLIGHT_HOST_SDL_SDK_WINDOW_SHARED
    )
    if(TARGET flight_host_sdl_vulkan)
      target_compile_definitions(
        flight_host_sdl_vulkan
        PRIVATE FLIGHT_HOST_SDL_VULKAN_EXPORTS
        PUBLIC FLIGHT_HOST_SDL_VULKAN_SHARED
      )
    endif()
  endif()
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(FLIGHT_CPP_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if(FLIGHT_CPP_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

install(
  TARGETS ${flight_cpp_install_targets}
  EXPORT FlightCppTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(
  DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  PATTERN "flight/host_sdl/sdk_audio.hpp" EXCLUDE
  PATTERN "flight/host_sdl/sdk_clipboard.hpp" EXCLUDE
  PATTERN "flight/host_sdl/sdk_cursor.hpp" EXCLUDE
  PATTERN "flight/host_sdl/sdk_device.hpp" EXCLUDE
  PATTERN "flight/host_sdl/sdk_haptics.hpp" EXCLUDE
  PATTERN "flight/host_sdl/sdk_keyboard.hpp" EXCLUDE
  PATTERN "flight/host_sdl/sdk_platform.hpp" EXCLUDE
  PATTERN "flight/host_sdl/sdk_screen.hpp" EXCLUDE
  PATTERN "flight/host_sdl/sdk_window.hpp" EXCLUDE
)
install(
  EXPORT FlightCppTargets
  FILE FlightCppTargets.cmake
  NAMESPACE Flight::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlightCpp"
)

configure_package_config_file(
  cmake/FlightCppConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/FlightCppConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlightCpp"
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/FlightCppConfigVersion.cmake"
  COMPATIBILITY SameMinorVersion
)
install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/FlightCppConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/FlightCppConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FlightCpp"
)
