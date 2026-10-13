# Typed-array construction from a typed array must allocate and copy

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, `@flighthq/bitmap` emits all 44 modules with no refusal
and all 44 headers compile independently. Native clone coverage nevertheless exposes a reference-
identity mismatch in `packages/bitmap/src/bitmap.ts`:

```ts
out.data = new Uint8ClampedArray(source.data);
```

JavaScript's typed-array constructor allocates a new backing buffer and copies the source elements.
Mutating `clone.data` therefore cannot change `source.data`. The emitted expression is:

```cpp
flight::Uint8ClampedArray(flight::row_get<flight::RowKey<"data">>(source))
```

That selects the C++ wrapper copy constructor. Wrapper copies deliberately share the same typed-array
backing identity so ordinary JavaScript assignments and pass-by-value parameters remain references to
one mutable buffer. Consequently `cloneBitmap` returns a distinct Bitmap whose `data.identity()` is
the source identity, and a write through the clone changes the source pixel bytes.

The compiler must distinguish JavaScript `new TypedArray(existingTypedArray)` from an ordinary carrier
copy and lower the former to a fresh element copy. `TypedArray::slice()` already implements the required
allocation and copy while `subarray()` and wrapper copy retain shared view identity.

For the carrying package row, the target-neutral source-equivalent spelling is:

```ts
out.data = source.data.slice();
```

It has the same element values and allocation semantics as the constructor expression for this full-
array input, introduces no new absence or exception behavior, and should expire when constructor
lowering preserves the required fresh backing identity. Native coverage must assert both distinct
storage identities and mutation isolation; value equality alone cannot detect this defect.
