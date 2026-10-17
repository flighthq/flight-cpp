# Typed-array static `from` needs a flight-cpp binding

The runtime implements JavaScript element conversion for every concrete typed array's static `from`,
including the optional mapper. The compiler binds the constructor and type spaces for those arrays but
does not bind the ambient value member `from`, so no source form reaches the runtime operation.

## Minimal TypeScript

```ts
export function copy(values: readonly number[]): Float32Array {
  return Float32Array.from(values);
}
```

At flight-compiler `2b687ea` this refuses before argument or result planning:

```text
ambient value Float32Array member from has no C++ binding
```

`Uint32Array.from(values)` reaches the corresponding `Uint32Array` refusal. The defect belongs to the
compiler's built-in flight-cpp ambient-member table: downstream external binding profiles are appended
after the built-in `Array`, typed-array, `ArrayBuffer`, `Object`, and `Promise` symbols and cannot add
members to them.

## Expected represented-sequence lowering

For an already represented iterable/range and an optional represented mapper, select the exact typed
array owner and its existing runtime operation:

```cpp
return flight::Float32Array::from(values);
```

The mapper overload must receive `(value, index)` in ascending order and apply the target typed array's
JavaScript element conversion after the mapper returns. `include/flight/typed_array.hpp` and the native
runtime tests already cover unsigned modulo conversion, signed wrapping, clamping, empty input, and the
two-argument mapper.

## The P-3D2 source needs one more lowering than the binding

`packages/scene3d-formats/src/gltfSkins.ts` uses the array-like overload:

```ts
Float32Array.from(
  { length: 16 },
  (_, component) => matrices.data[joint * 16 + component] ?? 0,
);
```

With the repository's `runtime-carriers` binding profile applied so `ArrayLike<T>` is represented, the
minimal array-like fixture stops at the same missing ambient binding. Without that profile it correctly
stops one stage earlier at the missing `ArrayLike[type]` carrier; that is not evidence about `from`. The
runtime's current `TypedArray::from` overload accepts C++ ranges, and a structural `{ length }` value is
not one. Adding only the ambient member mapping would therefore close the iterable fixture but is not
sufficient evidence that `gltfSkins.ts` is fixed.

The compiler also needs its length-only `Array.from`-style lowering: evaluate the source and mapper once,
apply `ToLength`, invoke the mapper for indices `0..length-1` with `undefined` as each source element,
then perform the destination typed-array conversion. The glTF mapper ignores that first argument, but a
general lowering may not omit it.

## Current measured reach

The committed refusal ledger has six direct instances of the missing static member:

- `scene3d-formats/gltfSkins.ts` (`Float32Array.from`), the P-3D2 site above;
- `skeleton2d/slotDeform2D.ts` (`Float32Array.from`), now carried by the
  [`skeleton2d-slot-deform-offsets-slice`](../../source-patches/skeleton2d-slot-deform-offsets-slice.patch)
  source patch, which copies with the typed array's own `slice()`;
- `skeleton2d-formats/spineBinaryParse.ts` (`Float32Array.from`); at `a627849` the site is
  `spineBinarySkinsHandler.ts`, carried by the
  [`skeleton2d-formats-binary-influences-constructor`](../../source-patches/skeleton2d-formats-binary-influences-constructor.patch)
  source patch (`new Float32Array(influences)` over a dense local `number[]`); the module then stops on
  [interface heritage widening](compiler-interface-heritage-owner-widening.md); and `spineJsonSkinsHandler.ts`, whose
  argument is an erased JSON array and is blocked first by [erased-array-element-reading](erased-array-element-reading.md);
- `tilemap-formats/tiledLayerData.ts` (`Uint32Array.from`); and
- `tilemap-formats/tiledXmlParse.ts` (`Uint32Array.from`); and
- `scene3d-formats/awd2GeometryHandler.ts` (`Uint32Array.from`), where the faithful
  `new Uint32Array(indices)` rewrite exposes the separate
  [typed-array/SequenceView union gap](typed-array-into-sequence-view-union.md) instead of emitting.

Additional modules use typed-array `from` behind earlier blockers, so this list is the directly measured
frontier rather than a claim that only five call sites exist.

### P-3D2 OBJ has a second boundary after the binding

`scene3d-formats/objParse.ts` calls `Uint32Array.from(indices)` twice while building this field:

```ts
indices?: Readonly<Uint16Array<ArrayBuffer>> | Readonly<Uint32Array<ArrayBuffer>> | null;
```

Its partial generated header currently drops both functions at the missing ambient `from` binding. The
refusal ledger also retains the later contextual boundary for the topology helper:

```text
contextual union value type flight::Uint32Array is not a represented runtime domain
```

That later record is useful acceptance evidence, not a separate request: binding `from` must produce the
concrete `Uint32Array<ArrayBuffer>` carrier and contextual construction must then select the exact
readonly Uint32 arm of `MeshGeometryOptions.indices`. It must not erase the buffer type, choose the
Uint16 arm, or copy the freshly constructed typed array into a replacement owner.

The existing pin without a typed-array member mapping stops at the ambient binding, so the contextual
record is a corpus site to rerun after item 1 below. It is kept here with the operation that exposes it
instead of creating a duplicate request from the second diagnostic.

## Acceptance

1. Bind static `from` for every concrete typed-array value without changing its constructor or type
   carrier.
2. Compile and run represented sequence inputs with and without a mapper against the runtime's existing
   conversion oracle.
3. Add a distinct length-only array-like fixture and prove source evaluation, `ToLength`, mapper order,
   `undefined` element arguments, and post-map typed conversion.
4. Re-run `gltfSkins.ts` and both OBJ geometry builders; do not mark P-3D2 unblocked when only the
   represented-sequence fixture or ambient binding passes.

No downstream duplicate ambient binding is carried.

## G2 fixed-length skin source rewrite and its remaining native failure

`scene3d-formats-gltf-skin-matrix-loop` spells the glTF site's exact length-16 mapper as a
fresh Float32Array and ascending component writes. An existing Matrix4Like annotation names
the fresh inverse-bind array's destination carrier; a typed diagnostic Record local stays
inside its original argument evaluation, retaining callback lookup before detail reads.
These are source-equivalent spellings, not a general `from` binding or a copied input owner.

Nine comparisons of the actual pinned original/patched TypeScript handler passed: valid Float32
values (including NaN, signed zero and absent elements), absent accessor, accessor fault,
count mismatch, read/report/data exceptions, absent skins, and a diagnostic getter that changes
the count before argument evaluation. They also check fresh independent matrices, joint mapping,
same document/mesh owners and report/read order.

Normal exact-pin generation of the two assigned modules emits both with no refusal, with the
normal structural member table gaining only the two newly used accessor callback keys. Native
compilation remains **0/2 passing**, at `.value_or(0)` on a required SequenceView index result;
the [existing nullish-storage request](host-initialize-optional-fallback-on-a-required-read.md#g2-gltf-skin-component-distinguish-an-index-miss-from-a-present-double)
records the required checked lookup. The native behavior executable did not build and was not run.
Both worklist rows remain failed. This source recovery does not certify callable skin import or
close the general typed-array/array-like binding request.
