# A length array needs proof before it becomes a dense carrier

JavaScript `new Array(length)` creates holes. `flight::Array<T>` is dense, so flight-compiler correctly
refuses a bare length constructor instead of silently replacing holes with default values. Two P-3D2
sites, however, assign every index before the local can be observed. The backend has no full-initialization
proof for that canonical loop and refuses before reaching it.

## Minimal TypeScript

```ts
export function indices(length: number): number[] {
  const out = new Array<number>(length);
  for (let index = 0; index < length; index++) out[index] = index;
  return out;
}
```

At flight-compiler `2b687ea` this stops at construction:

```text
Array length construction is outside the dense flight-cpp array profile because its runtime contract
cannot represent holes; use an explicit fill value, build the array with push, or use Array.from when
every element should be present
```

## Controls and the required boundary

| source shape | result |
| --- | --- |
| `new Array<number>(length)` | correctly refused: holes can escape |
| `new Array<number>(0)` | emits: the array is vacuously dense |
| `new Array<number>(length).fill(0)` | emits a dense construction and fill |
| `new Array<number | undefined>(length).fill(undefined)` | emits dense, explicitly present `undefined` elements |
| `[undefined, 1]` | emits a dense optional-element array |
| `[, 1]` | correctly refused as a sparse literal |
| the complete indexed loop above | refused despite every slot being assigned before escape |

The last row is the request. Bare length construction and any path that can read, iterate, escape, or
return the array before all indices are initialized must continue to refuse. Explicit `undefined` is
not a hole: enumeration, `in`, and callback iteration distinguish them, so the compiler may not treat an
optional element type as permission to densify a sparse source.

## Expected lowering for a proven complete loop

Once a local analysis proves that the allocation is unobservable until every index in `0..length-1` is
assigned exactly as the loop advances, the existing dense constructor is a faithful temporary carrier:

```cpp
flight::Array<double> out(length);
for (double index = 0.0; index < length; index += 1.0) out.set(index, index);
return out;
```

If the proof does not close, keep the current refusal. This is deliberately narrower than adding a
sparse-array runtime or globally treating length construction as dense.

## P-3D2 source audit

There are three production length-array sites in the measured lane:

- `scene3d-formats/gltfParse.ts` fills `gltfNodeToDocNode[i]` for every glTF node before the array is
  read or returned.
- `skeleton3d/skeleton3d.ts` fills `joints[i]` for every source joint before hierarchy reconstruction
  or entity initialization observes it. A thrown `cloneJoint` aborts the function, so no partial array
  escapes.
- `scene3d-formats/colladaSceneShared.ts` intentionally writes only reachable node indices and returns
  `(Matrix4Like | undefined)[]`. This is not a full-initialization reduction and must remain refused
  unless the source explicitly chooses dense `undefined` elements or a sparse carrier is added.

None is a committed first-refusal row at the current inventory: `gltfParse.ts` and `skeleton3d.ts` stop
earlier on other evidence rules, and the COLLADA helper has no direct row. These are source-frontier
reductions, not an assertion that a loop proof alone makes any full module emit.

No Flight source or runtime workaround is carried.
