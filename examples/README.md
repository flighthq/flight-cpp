# Native examples

These examples follow the examples in the pinned [`flighthq/flight`](https://github.com/flighthq/flight) checkout while keeping the source computation separate from the native host. Code supported by `flight-portable-typescript/1` lives under `source/` and is transpiled into `generated/`; handwritten C++ supplies the executable entry point and platform integration.

The complete upstream example corpus also has a generated inventory at `upstream/generated/`.
`npm run examples:generate` compiles all example packages in one dependency-aware graph using the repository's
runtime, headless, and SDL/GL binding profiles. It commits every emitted example header and every refusal while
referring to the single SDK tree at top-level `generated/`; it does not make a private SDK copy for each example.
Run `npm run examples:generate:check` to check that inventory against the pinned Flight and compiler revisions.

## Generated SDK math

`sdk_math_example.cpp` links the full generated inventory through `Flight::SdkPreview` and executes interpolation
functions emitted from `@flighthq/math` plus Entity construction emitted from `@flighthq/lighting`. It is the small,
host-independent proof that consumers can compile and run SDK modules through the normal CMake graph:

```sh
cmake --preset development
cmake --build --preset development
./out/cmake/development/examples/flight_cpp_sdk_math_example
```

## Tween

The tween example preserves the fifteen easing tracks and curve equations from Flight's `examples/packages/tween` example. The browser example draws moving circles through the TypeScript scene and renderer packages. The native example draws the same tracks as a deterministic terminal frame because those SDK and renderer packages do not have C++ implementations yet.

Build and run it with the standard development preset:

```sh
cmake --preset development
cmake --build --preset development
./out/cmake/development/examples/flight_cpp_tween_example
```

The SDL/OpenGL ES entry point renders all fifteen generated easing tracks in a native window. It uses the handwritten
SDL host for the window, event loop, GL context, procedure loading, and presentation; the curve calculations still
come from the transpiled TypeScript above. Configure the optional host without requiring Vulkan, then run it:

```sh
cmake --preset development \
  -DFLIGHT_CPP_BUILD_HOST_SDL=ON \
  -DFLIGHT_CPP_BUILD_HOST_SDL_VULKAN=OFF
cmake --build --preset development
./out/cmake/development/examples/flight_cpp_tween_sdl_gl_example
```

Close the window or press Escape to exit. The executable also accepts `--smoke`, which creates a hidden GL window,
renders three frames, and exits; this is useful with an offscreen SDL video driver in automated environments.

To regenerate `tween/generated/tween.hpp` with the compiler revision pinned by this repository:

```sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm --prefix .dependencies/flight-compiler run compile -- \
  "$PWD/examples/tween/source" \
  --target cpp \
  --out "$PWD/examples/tween/generated" \
  --package @flighthq/examples-tween
```

The upstream browser entry point itself cannot currently be transpiled. It contains executable module-level statements
and browser host calls; the pinned compiler refuses those statements rather than emitting invalid C++. The portable
source also spells the elastic curve's phase shift as the equivalent `period / 4`, because the compiler currently
emits `Math.asin` with an invalid C++ qualifier. Keeping the curve calculation in a separate source module makes both
supported compiler boundaries visible and gives later compiler revisions a straightforward place to remove the
adaptations.

## Sound

The SDL sound example uses the procedural tone and frequency-sweep calculations from Flight's upstream sound
example. Those calculations are transpiled from `sound/source/sound.ts`; the native entry point narrows the emitted
number arrays once to Float32 PCM, wraps them in `flight::AudioBuffer`, and plays three concurrent sources through
`SdlAudioDeviceBackend`:

```sh
cmake --preset development \
  -DFLIGHT_CPP_BUILD_HOST_SDL=ON \
  -DFLIGHT_CPP_BUILD_HOST_SDL_VULKAN=OFF
cmake --build --preset development
./out/cmake/development/examples/flight_cpp_sound_sdl_example
```

Use `--smoke` to generate shorter samples. Automated builds run that form with SDL's dummy audio driver. The full
upstream application still waits on compiler package/source remapping and its renderer dependency closure; this
entry point exercises the already available generated PCM and native playback seam without duplicating the SDK. The
portable source uses `number[]` as a temporary because the current compiler emits intentional Float32 assignment
narrowing without an explicit C++ cast, which conflicts with this repository's warning-as-error development build.

Regenerate the checked-in sound header with its pinned compiler:

```sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm --prefix .dependencies/flight-compiler run compile -- \
  "$PWD/examples/sound/source" \
  --target cpp \
  --out "$PWD/examples/sound/generated" \
  --package @flighthq/examples-sound
```
