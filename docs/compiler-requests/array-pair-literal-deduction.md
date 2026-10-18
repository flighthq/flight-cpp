# A pair literal is emitted as `flight::Array{...}`, so class-template deduction fails

## Reproduction

`flight/scene3d_formats/collada_feature_requirements.hpp` at `806d257`, line 24:

```cpp
flight::Map<flight::String, flight::Array<flight::Ref<flight::types::Requirement>>>(
  flight::Array<std::tuple<flight::String, flight::Array<flight::Ref<flight::types::Requirement>>>>{
    flight::Array{flight::String("Effect.Blinn"), flight::Array{flight::make_ref<...>(), ...}},
    // ...
  })
```

The outer array's declared element type is `std::tuple<flight::String, flight::Array<...>>`, but each inner
pair is emitted as `flight::Array{...}` — a two-element array-literal. Class template argument deduction then
has to deduce `Value` from `{String, Array<...>}`, whose `std::common_type_t` does not exist:

```
error: no viable constructor or deduction guide for deduction of template arguments of 'flight::Array'
```

The source builds an entries-style pair, and the emitter already names the tuple element type on the outer
array, so the inner literal's shape is inconsistent with the element type it is placed in.

## Required shape

A pair that the surrounding declared type spells as `std::tuple<A, B>` should be emitted as that tuple, not as
an `Array`. Adding a deduction guide `Array(First, Second) -> Array<std::tuple<First, Second>>` would make
the line compile but would produce the wrong element type — an array of tuples where the declared element is
itself the tuple — so the fix belongs in the lowering.

## Affected

13 headers in the G1 sweep, every `scene3d-formats` requirement table (`collada`, `awd2`, `gltf`, `md2`, `md5`,
`obj`, `3ds`). One cause, one request.

## What this repository carries meanwhile

`source-patches/compiler-contextual-tuple-element-array-literal.patch` carries the lowering through the
compiler patch stack. It applies clean at the end of the carried compiler patch stack; the section below
records what it changes and the evidence. No override is carried: the emitter choosing the tuple is the
faithful fix, and a hand-written copy would have to reproduce the same choice.

## Carried patch: `compiler-contextual-tuple-element-array-literal` (adopted)

**What it changes.** `emitArrayExpressionCpp` takes the tuple representation when the expected type
resolves to a tuple of the literal's own arity with no rest slot and the literal is dense: each element is
emitted against the tuple slot it fills, a present value for an optional slot is lifted with
`std::make_optional`, and construction goes through `emitCppTupleConstructionCpp` -- the same function an
IR tuple expression already uses. Any other expected type, or a literal of another arity, still takes the
array path, so an ordinary nested array literal is unchanged.

**Evidence.** Measured at flight-compiler `806d257` with the 84-patch stack plus this patch:

- Four focused fixtures in `cppTupleElementArrayLiteral.test.ts` pass: the heterogeneous pair in a typed
  entries map, the homogeneous pair keeping its `flight::Array<T>` representation, a present value lifted
  into an optional slot, and an ordinary nested array literal left to the array element type.
- The full `compiler-backend-cpp` and `compiler-semantic` suites fail the same 89 tests with and without
  the patch (3202 vs 3198 tests; the four added are the focused fixtures): 0 new failures, 0 fixed.
- A C++ probe of the emitted form compiles, links and runs (exit 0), reading both entries and their values.
- Scoped generation of `@flighthq/scene3d-formats` and the 13 cluster headers re-measured: **10 of 13
  compile** (before: 0/13, all thirteen at this diagnostic). The three that remain are past this defect:
  `gltf_requirements` fails at the `JSON.parse(...) as GltfDocument` static_cast, and
  `md5_mesh_requirements` / `obj_requirements` at `collect_md5_mesh_features` / `collect_obj_features`
  not being generated.
- The public requirement-table consumer `tests/generated_scene3d_formats_requirements_test.cpp` (registered
  in CMake and Bazel) builds and passes against the regenerated tables: all seven keep their source key order,
  each feature maps to its exact requirement, a requirement keeps its owner identity across lookups, and an
  absent feature stays absent.

**Not taken.** A deduction guide `Array(First, Second) -> Array<std::tuple<First, Second>>` compiles the
line but produces an `Array` where the declared element is the `std::tuple` itself, which is the wrong
element type. No patch to the pinned Flight source is carried: the pin has no simpler spelling for "the
declared element type of this entries array".
