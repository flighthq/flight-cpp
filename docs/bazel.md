# Bazel build

`flight-cpp` is a Bazel module as well as a CMake project. The runtime and tests have no third-party dependencies; the build graph pins `rules_cc`, which supplies the C++ rules removed from Bazel's built-ins. Bazelisk reads [`.bazelversion`](../.bazelversion) to select Bazel 9.2.0.

## Local build and tests

The public targets corresponding to CMake's `Flight::Cpp` and `Flight::C` are `//:cpp` and `//:c`. Compatibility aliases `//:flight_cpp` and `//:flight_cpp_c` are also public.

Bazel C++ toolchains do not expose a portable target attribute for selecting a language dialect. The checked-in
`.bazelrc` therefore enables Bazel's host-platform configuration and selects C++20 for GCC/Clang on Linux, macOS,
FreeBSD, and OpenBSD and for Visual C++ on Windows. A clean local checkout can run the core graph directly:

```sh
cd flight-cpp
bazel build //:cpp //:c
bazel test //:tests
```

The optional `--config=local-posix` adds compile and link pthread flags for a local toolchain that still needs them.
The C++20 selection itself is automatic and does not require callers to remember a named configuration.

`//:tests` covers the runtime, compiler-generated source, C ABI, all public headers in isolation, and C and C++ public-surface consumers. The underlying labels are available for focused runs:

- `//examples:tween`
- `//tests:runtime_test`
- `//tests:runtime_conformance_test`
- `//tests:generated_runtime_test`
- `//tests:c_api_test`
- `//tests:c_api_thread_test`
- `//tests:public_header_tests`
- `//tests/consumer:cpp_consumer_test`
- `//tests/consumer:c_consumer_test`

The performance smoke test is intentionally excluded from wildcard and suite runs by its `manual` tag. Run it explicitly in an optimized configuration:

```sh
bazel test --config=local-posix --config=release //benchmarks:runtime_benchmark
```

## SDL host

The optional SDL host pins SDL 3.4.10 and builds it from source with `rules_foreign_cc`. Its targets are manual, so
the dependency-free runtime suite above does not download or build SDL. With CMake, Ninja, Make, M4, and pkg-config
available as build tools, run the host test and the offscreen GL example with:

```sh
bazel test --config=local-posix //tests:host_sdl_test
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  bazel run --config=local-posix //examples:tween_sdl_gl -- --smoke
```

Applications depend on `//:host_sdl`, `//:host_sdl_image`, `//:host_sdl_gl`, or `//:host_sdl_wgpu`. The image target
is linked transitively by both graphics paths. The Vulkan adapter remains CMake-only until its SDK is represented as
a pinned Bazel dependency.

## Registered toolchains and platforms

Production and cross builds should make the C++20 dialect, compiler, standard library, sysroot, linker, and target constraints part of a registered C++ toolchain. Select that toolchain through the normal Bazel resolution surface:

```sh
bazel test --noenable_platform_specific_config //:tests \
  --platforms=@company_platforms//cpp:linux_x86_64 \
  --extra_toolchains=@company_toolchains//cpp:clang_linux_x86_64
```

Disabling the automatic host configuration prevents a host compiler flag from leaking into a cross or remote action;
the selected toolchain must then declare C++20 itself. The target graph remains independent of the compiler and
platform labels. Put personal defaults in the ignored `.bazelrc.local`; checked-in cross/remote automation should
pass platform and toolchain labels explicitly.

`//:c` is a linked static C ABI library. The Bazel graph does not yet force a shared-library artifact: a correct portable shared target must give the adapter private export definitions and its consumers public import definitions, then be tested across ELF, Mach-O, and PE/COFF. An ad hoc `cc_binary(linkshared=True)` would not meet that contract on Visual C++. Until that split exists, use the CMake `BUILD_SHARED_LIBS` build when a loadable C ABI artifact is required.

## Reproducibility policy

The module pins its Bazel release and `rules_cc` version, contains an explicit source graph, disables build stamping, and gives tests a fixed UTC/C process environment. Strict action environments keep undeclared machine state out of compile and link actions. Release builds use `--config=release`; CI can add `--config=ci` to retain all failures from a test sweep.

`MODULE.bazel.lock` records the complete external build dependency graph, and `.bazelrc` makes drift an error. If another external build dependency is introduced, its version and integrity must be pinned in `MODULE.bazel` and the lock must be deliberately regenerated with Bazel 9.2.0. A consuming repository owns its own module lock and may substitute its registered platforms and toolchains without changing `flight-cpp`.

Bazel cache keys include the selected platform and toolchain. Reproducibility therefore means identical outputs for identical source, Bazel, flags, platform, and toolchain inputs; it does not assert byte equality between different C++ compilers or standard libraries.
