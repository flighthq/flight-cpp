# An initialized optional typed-array member needs a present indexed write

At compiler `806d257` / Flight `a627849`, `scene3d/instanced_mesh.hpp:233` writes
`target->instance_colors.element(index)` on `optional<Uint32Array>`. The source's
`setInstancedMeshInstanceColor` returns for an index outside the active instance count, initializes
and white-fills the nullable color array when needed, then assigns one color and invalidates the
same InstancedMesh. The final source member is present by that initialization.

The emitter must retain that lvalue presence proof and the typed-array assignment domain. A simple
unwrap followed by `element(index) = color` is insufficient: `TypedArray::element` is a native
reference access, while `set_index` implements ignored invalid integer-indexed properties and the
array's Uint32 conversion. Fractional and NaN indices pass the source's outer range guard, still
perform any lazy allocation and version invalidation, and must not become successful element writes
or native bounds exceptions. Non-finite/negative/overflowing colors must use the ordinary Uint32 rule.

The carried source patch adds an erased non-null assertion only at the final indexed write:

```ts
target.instanceColors![index] = color;
```

Whole original/patched module transpilation yields byte-identical JavaScript, preserving property
read/setter order, allocation/fill, target and array identity, index/color evaluation and version changes.
The patch is applied after the existing allocation-owner correction; no new cache or local reference
changes the source reads. Exact-pin normal one-module generation emits the payload unwrap plus
`set_index` and only renumbers a later generated temporary. No generated C++ is hand-written.

The source's material array, geometry, matrix storage and runtime owner remain on that same target;
no material/instance result, optional wrapper API, Any, copied owner or restricted numeric domain is
substituted. Remove the patch when post-initialization or explicit non-null evidence reaches indexed
lvalue lowering with the correct typed-array write protocol.

## Verification at the instanced-mesh site

Exact-pin normal generation emits this whole module with no refusal, reproducing the checked write.
The changed one-header check completes at one worker: **0/1 pass, 1 fail, 0 unattempted**. The original
optional-member diagnostic clears; the header now fails at line353 on `Ref<Aabb>` to writable
`Ref<AabbLike>`, under the [existing output-owner request](writable-structural-out-of-a-wider-owner.md#g2-instanced-mesh-bounds-after-the-color-write-fix).

An actual native color test using the real InstancedMesh factory and setter failed to build at those
bounds calls (lines353 and363), so it was not linked or run. Its source/logs are retained as scratch
evidence; the unbuildable test and registration are not carried. No native color, instance or material
behavior pass is claimed. Source JavaScript identity and generation pass; the SDK header remains failed.
