#pragma once

#if defined(_WIN32) && defined(FLIGHT_HOST_SDL_SHARED)
#if defined(FLIGHT_HOST_SDL_EXPORTS)
#define FLIGHT_HOST_SDL_API __declspec(dllexport)
#else
#define FLIGHT_HOST_SDL_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define FLIGHT_HOST_SDL_API __attribute__((visibility("default")))
#else
#define FLIGHT_HOST_SDL_API
#endif

#if defined(_WIN32) && defined(FLIGHT_HOST_SDL_GL_SHARED)
#if defined(FLIGHT_HOST_SDL_GL_EXPORTS)
#define FLIGHT_HOST_SDL_GL_API __declspec(dllexport)
#else
#define FLIGHT_HOST_SDL_GL_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define FLIGHT_HOST_SDL_GL_API __attribute__((visibility("default")))
#else
#define FLIGHT_HOST_SDL_GL_API
#endif

#if defined(_WIN32) && defined(FLIGHT_HOST_SDL_VULKAN_SHARED)
#if defined(FLIGHT_HOST_SDL_VULKAN_EXPORTS)
#define FLIGHT_HOST_SDL_VULKAN_API __declspec(dllexport)
#else
#define FLIGHT_HOST_SDL_VULKAN_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define FLIGHT_HOST_SDL_VULKAN_API __attribute__((visibility("default")))
#else
#define FLIGHT_HOST_SDL_VULKAN_API
#endif

#if defined(_WIN32) && defined(FLIGHT_HOST_SDL_WGPU_SHARED)
#if defined(FLIGHT_HOST_SDL_WGPU_EXPORTS)
#define FLIGHT_HOST_SDL_WGPU_API __declspec(dllexport)
#else
#define FLIGHT_HOST_SDL_WGPU_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define FLIGHT_HOST_SDL_WGPU_API __attribute__((visibility("default")))
#else
#define FLIGHT_HOST_SDL_WGPU_API
#endif

#if defined(_WIN32) && defined(FLIGHT_HOST_SDL_SDK_AUDIO_SHARED)
#if defined(FLIGHT_HOST_SDL_SDK_AUDIO_EXPORTS)
#define FLIGHT_HOST_SDL_SDK_AUDIO_API __declspec(dllexport)
#else
#define FLIGHT_HOST_SDL_SDK_AUDIO_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define FLIGHT_HOST_SDL_SDK_AUDIO_API __attribute__((visibility("default")))
#else
#define FLIGHT_HOST_SDL_SDK_AUDIO_API
#endif

#if defined(_WIN32) && defined(FLIGHT_HOST_SDL_SDK_CURSOR_SHARED)
#if defined(FLIGHT_HOST_SDL_SDK_CURSOR_EXPORTS)
#define FLIGHT_HOST_SDL_SDK_CURSOR_API __declspec(dllexport)
#else
#define FLIGHT_HOST_SDL_SDK_CURSOR_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define FLIGHT_HOST_SDL_SDK_CURSOR_API __attribute__((visibility("default")))
#else
#define FLIGHT_HOST_SDL_SDK_CURSOR_API
#endif

#if defined(_WIN32) && defined(FLIGHT_HOST_SDL_SDK_WINDOW_SHARED)
#if defined(FLIGHT_HOST_SDL_SDK_WINDOW_EXPORTS)
#define FLIGHT_HOST_SDL_SDK_WINDOW_API __declspec(dllexport)
#else
#define FLIGHT_HOST_SDL_SDK_WINDOW_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define FLIGHT_HOST_SDL_SDK_WINDOW_API __attribute__((visibility("default")))
#else
#define FLIGHT_HOST_SDL_SDK_WINDOW_API
#endif

#if defined(_WIN32) && defined(FLIGHT_HOST_SDL_SDK_CLIPBOARD_SHARED)
#if defined(FLIGHT_HOST_SDL_SDK_CLIPBOARD_EXPORTS)
#define FLIGHT_HOST_SDL_SDK_CLIPBOARD_API __declspec(dllexport)
#else
#define FLIGHT_HOST_SDL_SDK_CLIPBOARD_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define FLIGHT_HOST_SDL_SDK_CLIPBOARD_API __attribute__((visibility("default")))
#else
#define FLIGHT_HOST_SDL_SDK_CLIPBOARD_API
#endif

#if defined(_WIN32) && defined(FLIGHT_HOST_SDL_SDK_PLATFORM_SHARED)
#if defined(FLIGHT_HOST_SDL_SDK_PLATFORM_EXPORTS)
#define FLIGHT_HOST_SDL_SDK_PLATFORM_API __declspec(dllexport)
#else
#define FLIGHT_HOST_SDL_SDK_PLATFORM_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define FLIGHT_HOST_SDL_SDK_PLATFORM_API __attribute__((visibility("default")))
#else
#define FLIGHT_HOST_SDL_SDK_PLATFORM_API
#endif

#if defined(_WIN32) && defined(FLIGHT_HOST_SDL_SDK_DEVICE_SHARED)
#if defined(FLIGHT_HOST_SDL_SDK_DEVICE_EXPORTS)
#define FLIGHT_HOST_SDL_SDK_DEVICE_API __declspec(dllexport)
#else
#define FLIGHT_HOST_SDL_SDK_DEVICE_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define FLIGHT_HOST_SDL_SDK_DEVICE_API __attribute__((visibility("default")))
#else
#define FLIGHT_HOST_SDL_SDK_DEVICE_API
#endif
