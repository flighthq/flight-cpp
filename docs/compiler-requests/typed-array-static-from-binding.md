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

The committed refusal ledger has five direct instances of the missing static member:

- `scene3d-formats/gltfSkins.ts` (`Float32Array.from`), the P-3D2 site above;
- `skeleton2d/slotDeform2D.ts` (`Float32Array.from`);
- `skeleton2d-formats/spineBinaryParse.ts` (`Float32Array.from`);
- `tilemap-formats/tiledLayerData.ts` (`Uint32Array.from`); and
- `tilemap-formats/tiledXmlParse.ts` (`Uint32Array.from`).

Additional modules use typed-array `from` behind earlier blockers, so this list is the directly measured
frontier rather than a claim that only five call sites exist.

## Acceptance

1. Bind static `from` for every concrete typed-array value without changing its constructor or type
   carrier.
2. Compile and run represented sequence inputs with and without a mapper against the runtime's existing
   conversion oracle.
3. Add a distinct length-only array-like fixture and prove source evaluation, `ToLength`, mapper order,
   `undefined` element arguments, and post-map typed conversion.
4. Re-run `gltfSkins.ts`; do not mark P-3D2 unblocked when only the represented-sequence fixture passes.

No Flight source rewrite or downstream duplicate binding is carried.
