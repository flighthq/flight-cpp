# Native examples

These examples follow the examples in the pinned [`flighthq/flight`](https://github.com/flighthq/flight) checkout while keeping the source computation separate from the native host. Code supported by `flight-portable-typescript/1` lives under `source/` and is transpiled into `generated/`; handwritten C++ supplies the executable entry point and platform integration.

## Tween

The tween example preserves the fifteen easing tracks and curve equations from Flight's `examples/packages/tween` example. The browser example draws moving circles through the TypeScript scene and renderer packages. The native example draws the same tracks as a deterministic terminal frame because those SDK and renderer packages do not have C++ implementations yet.

Build and run it with the standard development preset:

```sh
cmake --preset development
cmake --build --preset development
./out/cmake/development/examples/flight_cpp_tween_example
```

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

The upstream browser entry point itself cannot currently be transpiled. It contains executable module-level statements and browser host calls; the pinned compiler refuses those statements rather than emitting invalid C++. The portable source also spells the elastic curve's phase shift as the equivalent `period / 4`, because the compiler currently emits `Math.asin` with an invalid C++ qualifier. Keeping the curve calculation in a separate source module makes both supported compiler boundaries visible and gives later compiler revisions a straightforward place to remove the adaptations.
