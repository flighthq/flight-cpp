# Floating-point contraction is off, matching JavaScript

JavaScript evaluates every arithmetic operation with its own IEEE-754 double rounding. It never fuses
`a * b - c * d` into a fused multiply-add. C++ compilers do unless told not to:
- Clang contracts within an expression by default (`-ffp-contract=on`) whenever the target CPU has FMA.
- GCC's GNU dialects (`-std=gnu++20`, which CMake selects by default) contract across statements
  (`-ffp-contract=fast`).

Either way, emitted code can round differently from the TypeScript it was generated from.

## Profile

Every compile path turns contraction off:

| path | flag |
| --- | --- |
| `Flight::Cpp` CMake interface, GCC/Clang/AppleClang | `-ffp-contract=off` |
| `Flight::Cpp` CMake interface, clang-cl | `/clang:-ffp-contract=off` |
| `Flight::Cpp` CMake interface, MSVC | `/fp:precise` (no contraction since Visual Studio 2022 17.0) |
| `.bazelrc` linux, macos, freebsd, openbsd | `--copt=-ffp-contract=off` |
| `.bazelrc` windows | `--copt=/fp:precise` |
| `pkg:compile` header gate | `-ffp-contract=off` |

The CMake option is an `INTERFACE` option, so it also reaches installed consumers of `Flight::Cpp`, which
is where emitted code is compiled. Bazel cannot propagate a copt from a header-only library to its
dependents, so a Bazel consumer outside this repository must set the same copt in its own `.bazelrc`.

`scripts/buildHealth.mjs` fails if any of these entries is missing. `tests/floating_point_contraction_test.cpp`
takes the profile only from its target, so it fails on an FMA host if the profile is dropped.

## Measured

`uy * fz - uz * fy` with `up == forward == (2/7, 3/7, 6/7)`:

| build | result |
| --- | --- |
| JavaScript | `0` |
| zig clang `-O1`, native CPU (FMA available) | `-4.5315225494904337e-18` |
| the same with `-ffp-contract=off` | `0` |
| the same with `-mcpu=baseline` (no FMA) | `0` |

## Observed effect without the profile

`setQuaternionLookRotation` tests that exact zero to choose its roll fallback when `up` is parallel to
`forward` (`generated/include/flight/geometry/quaternion.hpp`, the cross-product guard). Contracted, the
residue skips the fallback. For forward `(2/7, 3/7, 6/7)` the native result maps +Z to
`(0.5116, 0.3016, 0.7977)` instead of `forward`. The two upstream cases that observe this are asserted in
`tests/generated_geometry_planar_test.cpp` and pass only with the profile.
