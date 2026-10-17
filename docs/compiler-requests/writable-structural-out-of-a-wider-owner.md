# A wider owner passed as a writable structural out parameter must keep its identity

`Aabb` is assignable to `AabbLike`: it declares the same `min`/`max` members, plus its entity key. A
function that writes through an `out: AabbLike` parameter is therefore callable in TypeScript with an
`Aabb`, and the caller then observes the writes on its own object. The emitter lowers that writable
parameter as the nominal owner `flight::Ref<AabbLike>` and passes the argument through unconverted, so
no `Aabb` (and nothing else that is not literally an `AabbLike`) can be passed. The call does not compile.

A readonly parameter of the same type already lowers correctly, as an identity-preserving structural row.
This asks for the same treatment of a writable one.

## Minimal TypeScript

```ts
export interface Vec { x: number }
export interface Box { kind: string; max: Vec; min: Vec }
export type BoxLike = { max: Vec; min: Vec };

export function createBox(): Box {
  return { kind: 'box', max: { x: 0 }, min: { x: 0 } };
}

export function setBox(out: BoxLike, v: number): void {
  out.min.x = v;
  out.max.x = v;
}

export function use(): Box {
  const b = createBox();
  setBox(b, 1);
  return b;
}
```

At flight-compiler `2b687ea` (with this repository's compiler source patches applied) this emits:

```cpp
inline void set_box(flight::Ref<BoxLike> out, double v) {
  (out->min->x = v);
  (out->max->x = v);
}

inline flight::Ref<Box> use() {
  auto b = create_box();
  set_box(b, 1.0);  // error: no matching function for call to 'set_box'
  return b;
}
```

## Required C++ shape

The writable counterpart of what a readonly parameter already gets:

```cpp
inline void set_box(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<BoxLike>>>> out, double v) {
  (flight::row_get<flight::RowKey<"min">>(out)->x = v);
  (flight::row_get<flight::RowKey<"max">>(out)->x = v);
}

// call site
set_box(flight::structural_ref_cast<flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<BoxLike>>>>>(
            flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Box>>>>(b)),
        1.0);
```

The runtime already supports this. A hand-written build of exactly this shape against the current runtime:
- compiles;
- the writes land on the caller's `Box`, and its `min` owner is the same object afterwards;
- a direct `BoxLike` owner still passes through the same parameter.

No copy, write-back or side storage is involved, which is the point: a copied `BoxLike` written and
discarded would silently drop the caller's result.

## Pinned boundary

| parameter and argument | result |
| --- | --- |
| `out: BoxLike` (type alias), argument `Box` | `Ref<BoxLike>` parameter, unconverted argument: does not compile |
| `out: BoxLike` (interface), argument `Box` | same |
| `source: Readonly<BoxLike>`, argument `Box` | `StructuralRef<RowReadonly<RowOf<Ref<BoxLike>>>>` via `structural_ref_cast`: compiles |
| `out: Box`, argument `Box` | `Ref<Box>`: compiles |
| `out: BoxLike`, argument already a `BoxLike` | `Ref<BoxLike>`: compiles |

The controls isolate the defect to a writable parameter whose type is a different, structurally
compatible owner from the argument's. Readonly parameters and exact-owner arguments already work.

## Distinct from

- [readonly-adapter-stored-as-writable](readonly-adapter-stored-as-writable.md) is a source-level assertion
  from a readonly row to a writable one, correctly refused. Here nothing is readonly and nothing is asserted:
  a writable value is passed to a writable parameter, which TypeScript accepts by plain assignability.
- [assertion-to-undeclared-row-member](assertion-to-undeclared-row-member.md) is a narrowing downcast that
  needs a checked owner cast. Here the conversion widens: every member the parameter declares exists on the
  argument, so a structural row over the retained owner is sufficient and nothing needs checking.

## Affected Flight modules

`@flighthq/geometry` `aabb.ts` at flight `a627849`: every writer takes `out: AabbLike`, while `createAabb`
returns `Aabb`. The writers are:
- `copyAabb`, `expandAabbByPoint`, `expandAabbBySphere`, `intersectAabb`;
- `setAabb`, `setAabbFromPoints`, `transformAabbByMatrix4`, `unionAabb`.

No `createAabb` result can be passed to any of them; the compiler diagnostic is
`no viable conversion from 'Ref<flight::types::Aabb>' to 'Ref<flight::types::AabbLike>'`. The geometry
header itself compiles, because no geometry function passes an `Aabb` to a writer, so the package's tier
does not show this. The native geometry tests exercise the writers through an `AabbLike` out with the same
expected values.

## Masked consumer: scene3d

`@flighthq/scene3d` creates its scratch boxes with `createAabb()` and passes them as the `out` of
`transformAabbByMatrix4`:
- `sceneNodeCulling.ts` (`generated/include/flight/scene3d/scene_node_culling.hpp:70`);
- `sceneNodeBounds.ts` (`scene_node_bounds.hpp:57`).

These calls will not compile until this lowering exists. The scene3d row is at `7e2fc7d` and none of its
21 headers compile for earlier reasons, so this dependency is currently masked. It is recorded here so the
scene3d migration does not rediscover it.

## Workaround carried

None. A copy-in/copy-out shim or an override converting through a temporary `AabbLike` would change
identity: the caller's `min`/`max` vectors would not be the ones written. A source patch retyping
`createAabb` or the writers would change the public API.

## G2 instanced-mesh bounds after the color-write fix

At compiler `806d257` / Flight `a627849`, `scene3d/instanced_mesh.hpp` reaches this previously
masked boundary after its [optional color-write fix](initialized-optional-typed-array-write.md).
The serial changed-header check is **0/1 pass, 1 fail, 0 unattempted**. Its first diagnostic is
`no viable conversion from 'Ref<flight::types::Aabb>' to 'Ref<flight::types::AabbLike>'` at line353:
`transform_aabb_by_matrix4` receives the original `scratch_instance_bounds` as its writable out.
Line363 also passes the original `out` Aabb owner to `union_aabb`; the readonly input projections
already retain their source owners. Both failures occur while compiling the real header, before any
instance-color test can link or execute.

The source `Aabb` is an Entity with `min/max: Vector3`; `AabbLike` is the compatible structural
`min/max: Vector3Like` type alias, not a declared C++ base class. Preserve the same output object and
nested vector owners as each transformed instance is unioned. No copy-in/out adapter, native pointer
cast, source retyping, fake bounds or material/instance result is supplied. The failed actual native
color-test build is not a behavior pass; its temporary registration was withdrawn. The source-only
color correction is carried independently, and the production row remains failed under this request.
