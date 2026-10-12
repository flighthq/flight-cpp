# An optional member must construct its declared Record value carrier

A `Record<string, Uint8Array>` is assignable to a readonly
`Record<string, ArrayLike<number>>` in TypeScript. The flight-cpp backend emits that conversion without
adapting it as a direct return and inside a required structural member, but refuses when the same Record
is used to fill an optional structural member.

## Minimal TypeScript

```ts
interface Options {
  externalBuffers?: Readonly<Record<string, ArrayLike<number>>>;
}

export function options(externalBuffers: Record<string, Uint8Array>): Readonly<Options> {
  return { externalBuffers };
}
```

With flight-compiler `2b687ea` and flight-cpp's pinned runtime bindings, this refuses:

```text
contextual union value type flight::Record<flight::String, flight::Uint8Array> is not a represented
runtime domain
```

The union is introduced by the optional outer member. The Record itself and both element carriers are
represented.

## Pinned boundary

| source and destination | result |
| --- | --- |
| optional member, `Record<string, Uint8Array>` to the exact same Record type | emits |
| optional member, source already `Record<string, ArrayLike<number>>` | emits |
| optional member, `Record<string, Uint8Array>` to `Record<string, ArrayLike<number>>` | refused |
| optional member, `Record<string, number[]>` to `Record<string, ArrayLike<number>>` | refused |
| required member with the same Uint8Array-to-ArrayLike widening | emits without adapting the Record |
| direct return with the same widening | emits `return v;`, which does not compile |

The array-valued control proves this is not specific to typed arrays. The exact-carrier controls prove
that optional structural members and Records are both otherwise represented. The required-member and
direct-return outputs are negative controls, not successful conversions: the direct fixture tries to
return `flight::Record<String, Uint8Array>` as `flight::Record<String, SequenceView<double>>`, for which
there is no C++ conversion.

## Required carrier behavior

The source Record already exists, so constructing a second Record would change JavaScript object
identity and split later writes. A correct lowering needs one of these two proofs:

- choose `Record<string, ArrayLike<number>>` when the Record is freshly allocated in that declared
  context, converting each inserted array value to the represented sequence view; or
- retain the existing Record identity through a readonly projected value view whose reads adapt
  `Uint8Array` to `ArrayLike<number>` without granting writes through the wider cell type.

The P-3D2 value is returned from a helper as an existing `Record<string, Uint8Array>`, so it needs the
second behavior unless the source contract is changed at the allocation boundary. The compiler must
not emit the currently invalid invariant C++ Record conversion, copy the entries, or erase values into
`Any`.

## P-3D2 reach and distinct requests

`packages/scene3d-resources/src/gltfLoad.ts` loads external `.bin` files into a
`Record<string, Uint8Array>` and supplies that existing Record to
`GltfImportOptions.externalBuffers?: Readonly<Record<string, ArrayLike<number>>>`. It is a committed
first refusal; the minimal optional-member fixture reproduces its exact diagnostic.

This is not [typed-array static `from`](typed-array-static-from-binding.md), which constructs a typed
array from a source sequence, and it is not [`Object.assign`](object-assign-onto-structural-record.md),
which mutates a target. It is a value-domain projection of one already allocated Record. The invalid
direct-return control should be fixed or truthfully refused with the same identity constraint.

No source rewrite or downstream Record copy is carried.
