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

Nothing: the cluster is open, and no override or patch would reproduce the declared element type more
faithfully than the emitter choosing the tuple.
