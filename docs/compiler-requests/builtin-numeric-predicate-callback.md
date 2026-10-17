# A numeric builtin used as a callback needs a callable value

At compiler `806d257d2f4100de745cc97340e61c2f301b0f24` / Flight
`a62784923f3be814463286c9fb28edfe6ab15789`, `colladaNumbers` in
`packages/scene3d-formats/src/colladaXml.ts` passes `Number.isFinite` to `Array.filter`.
The emitted `flight/scene3d_formats/collada_xml.hpp:44` passes `std::isfinite` directly.
That C++ name is an overload set, so the runtime cannot deduce the predicate type.

## Reproduction

```ts
export function finite(values: number[]): number[] {
  return values.filter(Number.isFinite);
}
```

The failing expression already present in the generated COLLADA owner is:

```cpp
optional_chain_receiver.value().filter(std::isfinite)
```

`Array::filter` takes a deduced predicate, invokes it with an element and, if supported,
its numeric index, and preserves the selected elements in order. An overloaded function
name alone has no type from which C++ can deduce that predicate. Adding a dummy or
number-specific overload to the general collection API would hide this value-lowering seam.

## Required shape

Materialize a callable at the source callback's actual element carrier, for example:

```cpp
values.filter([](double value) { return std::isfinite(value); })
```

For this numeric array the builtin ignores both callback index and source-array arguments;
the adapter must retain that behavior. This must also work for a retained readonly sequence
without copying or replacing its source owner. A nonnumeric or union input needs the actual
`Number.isFinite` rule: nonnumbers return false, without numeric coercion. The related
[union predicate request](closed-key-reads-and-union-number-isfinite.md) covers that separate
argument-lowering defect.

## Runtime work and remaining debt

G1 cluster `call-has-no-matching-47410fe2` originally stopped earlier, at
`map(flight::to_number)`, in seven SVG and nine COLLADA headers. The runtime now represents
`to_number` as a callable object, following `to_boolean`, with the same existing conversion
operations. This fixes `map(Number)` for Array and SequenceView without changing their
callback or owner contracts. Direct calls keep the same conversions.

The scoped `-j1` recheck completed all 16 headers: two SVG headers compile, nine COLLADA headers
fail at `filter(std::isfinite)`, and five SVG headers expose other compiler diagnostics.
The runtime build and `flight_cpp.runtime` semantic test pass (1/1).

The numeric predicate still names an overloaded function in `std`, which a runtime extension
cannot replace. This request remains open. No generated behavior, source rewrite, or override
is carried for it; independent parser refusals and other diagnostics exposed by the scoped
recheck remain failures in the header-status product.
