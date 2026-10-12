# Floating-point contraction diverges from JavaScript on FMA hosts

JavaScript evaluates every arithmetic operation with its own IEEE-754 double rounding. It never fuses
`a * b - c * d` into a fused multiply-add. Clang does, by default (`-ffp-contract=on`), whenever the
target CPU has FMA. Nothing in this repository's CMake, Bazel, scripts or compiler wrapper turns that
off, so emitted code can round differently from the TypeScript it was generated from.

## Measured

`uy * fz - uz * fy` with `up == forward == (2/7, 3/7, 6/7)`:

| build | result |
| --- | --- |
| JavaScript | `0` |
| zig clang `-O1`, native CPU (FMA available) | `-4.5315225494904337e-18` |
| the same with `-ffp-contract=off` | `0` |
| the same with `-mcpu=baseline` (no FMA) | `0` |

## Observed effect

`setQuaternionLookRotation` tests that exact zero to choose its roll fallback when `up` is parallel to
`forward` (`generated/include/flight/geometry/quaternion.hpp`, the cross-product guard). On an FMA host
the residue skips the fallback. For forward `(2/7, 3/7, 6/7)` the native result maps +Z to
`(0.5116, 0.3016, 0.7977)` instead of `forward`.

Two upstream cases in `tests/generated_geometry_planar_test.cpp` observe this. They are recorded as known
defects rather than asserted, and they report when they start passing.

## Scope

The cause is the build profile, not the emitter or the geometry package. Every FMA-capable build of any
emitted arithmetic is exposed. Most results differ only in the last bits, but any source that branches on
an exact comparison of such a result can take a different branch, as above.

The fix that preserves TypeScript numeric semantics is to disable contraction for emitted code, for
example `-ffp-contract=off` on the `Flight::Cpp` interface and the matching Bazel `copts`. This document
records the gap. That repository-wide build change has not been made here.
