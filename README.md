# flight-cpp

`flight-cpp` is the incubating C++20 runtime for TypeScript compiled by Flight Compiler. It was incubated inside the compiler repository while the generated-code boundary settled and now stands on its own, carrying that history with it. The runtime builds with CMake or Bazel and needs no Node.js.

This is a working foundation, not yet a production-support claim. Version 0.1.0 provides tested representations for shared arrays, insertion-ordered maps, sets, and JavaScript-ordered records, typed-array views over owned or external buffers, explicit `undefined`/`null` presence, SameValueZero equality, UTF-16 strings, source-style errors and number formatting, UTC date instants, shared coroutine tasks, and closed multi-member unions with distinct C++ alternatives. Tasks use an explicit non-reentrant executor and implement first-settlement-wins construction, exact rejection values, queued continuation, recovery, cleanup, assimilation, and ordered aggregation. Full Unicode case conversion is supplied through a host service. Compiler-generated conformance exercises collections, strings, classes, typed arrays, optional access, coroutines, and checker-proven union narrowing; cancellation, time zones, captured mutation, duplicate union representations, and optional variants remain open.

## Build

The core runtime and its tests have no third-party dependencies. The presets require CMake 3.20 or newer, a C++20 compiler, and Ninja:

```sh
cmake --preset development
cmake --build --preset development
ctest --preset development
```

The presets use Ninja and leave compiler selection to CMake. Pass `-DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake` at configure time for a cross or pinned toolchain; `CMakeUserPresets.json` is ignored for machine-local settings.

The independent Bazel 9 module builds and runs the same public runtime, C ABI, generated-source, header, and consumer surfaces. Bazelisk selects the checked-in version:

```sh
bazel test --config=local-posix //:tests
```

Use `--config=local-msvc` for the default Visual C++ toolchain, or select an arbitrary registered platform and C++ toolchain without changing the Flight graph. The [Bazel build contract](docs/bazel.md) covers local, cross, remote-execution, and reproducibility policy.

GCC and Clang development builds can add `-DFLIGHT_CPP_ENABLE_SANITIZERS=ON` to run the same runtime and generated-program tests under AddressSanitizer and UndefinedBehaviorSanitizer.

The handwritten SDL 3 host is optional. CMake can use an installed SDL, while Bazel builds its pinned SDL source;
both keep SDL out of `Flight::Cpp`:

```sh
cmake --preset development -DFLIGHT_CPP_BUILD_HOST_SDL=ON
cmake --build --preset development
ctest --preset development

bazel test --config=local-posix //tests:host_sdl_test
SDL_VIDEODRIVER=offscreen bazel run --config=local-posix //examples:tween_sdl_gl -- --smoke
```

The SDL package also implements Flight's decoded-PCM audio-device contract; its build-tree preview adapter populates
the exact generated `AudioDeviceBackend` record. See the [SDL host package guide](docs/host-sdl.md) for dependencies,
exported targets, ownership, callback pumping, and the remaining generated SDK wiring lane.

Release builds can add `-DFLIGHT_CPP_BUILD_BENCHMARKS=ON`. The resulting `flight_cpp.performance` CTest emits JSON-lines measurements and applies deliberately broad throughput floors for collection, ordered-map, and settled-task regressions. These are smoke gates, not cross-machine comparisons; release-candidate history should tighten them only after a stable runner baseline exists.

## Examples

The development and release presets build the native examples. Run the compiler-generated tween example after a development build:

```sh
./out/cmake/development/examples/flight_cpp_tween_example
```

The example preserves the fifteen easing tracks from Flight's TypeScript tween example and renders one deterministic frame in a terminal. Its portable calculation is TypeScript transpiled by the pinned `flight-compiler`; a small handwritten C++ host owns terminal output. See [`examples/README.md`](examples/README.md) for the source, generated output, regeneration command, and the current boundary around browser-backed examples.

The same build produces an example backed directly by the committed SDK inventory. It calls generated math functions
and constructs a generated lighting Entity through the native structural-row ABI:

```sh
./out/cmake/development/examples/flight_cpp_sdk_math_example
```

The same generated curves also have an interactive SDL/OpenGL ES host. Enable the optional SDL package and disable the
unused Vulkan adapter, then run the native window:

```sh
cmake --preset development \
  -DFLIGHT_CPP_BUILD_HOST_SDL=ON \
  -DFLIGHT_CPP_BUILD_HOST_SDL_VULKAN=OFF
cmake --build --preset development
./out/cmake/development/examples/flight_cpp_tween_sdl_gl_example
```

Consumers can build it in-tree with `add_subdirectory`, or install it and use:

```cmake
find_package(FlightCpp 0.1 CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE Flight::Cpp)
```

Foreign-language consumers can link `Flight::C` and include `<flight/c/runtime.h>`. The initial ABI exposes version negotiation and reference-counted UTF-8 string handles without leaking C++ layout or exceptions. It is intentionally smaller than the C++ surface; bindings add functions only after their ownership, error, and threading rules are fixed. The [C ABI contract](docs/c-api.md) defines those rules, and the [ABI v1 contract snapshot](abi/c-api-v1.txt) makes function-signature and status-value drift explicit in repository checks.

The C++ semantic runtime remains header-only during incubation; `Flight::C` is its separately linked ABI adapter. Include the complete C++ compatibility surface with:

```cpp
#include <flight/runtime.hpp>
```

The umbrella header provides `FlightTask<T>` and `FlightDate` compatibility names. The compiler's `flight-cpp` runtime profile emits the namespaced semantic APIs directly.

Native hosts configure executor and Unicode policy together with `flight::HostScope`. Services are thread-scoped and nest safely, which gives an embedder an explicit boundary instead of process-global callbacks.

## Boundary

The installed `flight/` headers and `Flight::Cpp` target are the extraction boundary. Nothing in this directory imports the compiler, assumes its repository layout, participates in the npm workspace, or relies on generated source checked in elsewhere.

The compiler emits semantic runtime types such as `flight::Array<T>` and `flight::Map<K, V>` when `runtimeProfile: "flight-cpp"` is elected. The separate `standard-library` profile preserves generic provisional output without claiming TypeScript-equivalent collection behavior. See [compiler integration](docs/compiler-integration.md) and [runtime semantics](docs/runtime-semantics.md).

The full-SDK inventory is committed under [`generated/`](generated/README.md). It contains every header the pinned
compiler can currently emit from the package closure declared by `@flighthq/sdk`, plus complete refusal and
initialization ledgers. `Flight::SdkPreview` and Bazel `//:sdk_preview` expose that exact inventory while the
independent-header gate is still red; `npm run sdk:compile` writes its detailed report under `out/`. The generated SDK
math/Entity example proves working emitted package paths. The [SDL host bring-up](docs/host-sdl.md) defines the handwritten
native lane that injects GL or WebGPU handles without duplicating upstream renderers.
The [flight-compiler adoption status](docs/flight-compiler-adoption.md) tracks each downstream runtime, host, and
release obligation without treating a present header as a completed semantic contract.

All 33 upstream example packages have a pinned SDL/GL compilation inventory under
[`examples/upstream/generated/`](examples/upstream/generated/README.md). It keeps example headers and refusal
ledgers separate while resolving SDK dependencies against the one shared top-level generated tree.

The supported input boundary is versioned as [`flight-portable-typescript/1`](conformance/portable-typescript-v1.json). [`known-exceptions.json`](conformance/known-exceptions.json) owns every checked-in C++ refusal. The compiler repository verifies it against its own fixture corpus, because a refusal changes when the compiler changes; this repository owns the file, and that gate reads it from a pinned checkout of this repository.

## Pinned siblings

The runtime and the compiler are separate repositories that must keep agreeing, so each pins the other rather than sharing a tree. [`dependencies.lock.json`](dependencies.lock.json) names the exact commit of `flight` and `flight-compiler` this checkout is verified against:

```sh
npm run rehydrate          # materialize the pinned checkouts under .dependencies/
npm run rehydrate:check    # fail if a checkout is missing or off its pin
npm run rehydrate:update   # re-pin each dependency to its tracking branch head
```

The checkouts are gitignored, disposable build inputs. Nothing in `.dependencies/` is committed, and the lock is the only thing that decides which revision a gate reads.

## Repository gates

The native build is the runtime's own gate and is run directly with CMake or Bazel. `npm run check` covers what building cannot show:

| Gate | Question |
| --- | --- |
| `npm run abort:oracle` | Do compiler-emitted cancellation bindings match native `AbortController` behavior? |
| `npm run abi:check` | Do the C header, its implementation, and the committed ABI snapshot name the same symbols? |
| `npm run array-like:oracle` | Do compiler-emitted portable bindings for array-like views, weak sets, and numeric globals compile and preserve native behavior? |
| `npm run base64:oracle` | Do compiler-emitted browser-compatible base64 operations match Node? |
| `npm run blob:oracle` | Do compiler-emitted Blob construction, slicing, text, and binary operations match Node? |
| `npm run build:check` | Do the CMake and Bazel graphs describe the same headers, sources, tests, and benchmarks? |
| `npm run examples:check` | Does the pinned compiler reproduce the checked-in native example output? |
| `npm run examples:generate:check` | Does the SDL/GL graph reproduce the committed inventory for all upstream example packages? |
| `npm run facets:oracle` | Do compiler-emitted conditional facets preserve capable and incapable host types? |
| `npm run headless:oracle` | Do the headless binding manifest's emitted console, timer, and performance calls compile and run? |
| `npm run sdk:check` | Does the pinned compiler reproduce the committed SDK headers and refusal inventory? |
| `npm run sdk:compile` | Which dependency-closed SDK headers compile independently with the selected `CXX` toolchain? |
| `npm run sdk:compile:headless` | Which headers compile after applying the runtime-carrier and headless binding profiles? |
| `npm run sdk:generate:sdl-gl` | Generate the full SDK inventory with runtime, headless, and SDL/OpenGL host bindings? |
| `npm run sdk:compile:sdl-gl` | Which headers compile under the SDL/OpenGL host profile? |
| `npm run release:check` | Do the version, ABI, C++ standard, and conformance profile agree across every file that states them? |
| `npm run runtime:oracle` | Do the native runtime services match the same TypeScript-valid operations under Node? |
| `npm run sdl-app:oracle` | Do compiler-emitted window, document, frame, and input calls match the SDL application shell? |
| `npm run sdl-gl:oracle` | Does the compiler emit the SDL/OpenGL surface, handle, and weak-cache types from the maintained profile? |
| `npm run structural:oracle` | Does the pinned compiler's generic Entity proxy compile and preserve native write interception? |
| `npm run stream:oracle` | Do compiler-emitted readable, writable, and async-iterable carriers compile and preserve stream behavior? |
| `npm run text-encoder:oracle` | Does compiler-emitted `TextEncoder` produce the same UTF-8 bytes as Node? |
| `npm run uri:oracle` | Do compiler-emitted URI component operations match Node? |
| `npm run compile:check` | Does the pinned compiler's emitted C++ still compile against this runtime? |

`compile:check` reports and skips when the checkout is absent or no C++ compiler is installed, so a fresh clone stays runnable. The compiler repository asks the same question from its side against the runtime revision it pins; both are wanted, because each side owns the pin it can move and a failure names which one changed.

## License

MIT. See [LICENSE.md](LICENSE.md).
