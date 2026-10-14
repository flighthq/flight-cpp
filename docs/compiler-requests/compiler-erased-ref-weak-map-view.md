# Checked typed view of an erased WeakMap

The compiler already proves exact Flight reference key and value carriers for a typed view of
`WeakMap<object, object>`, but stops before calling the runtime's checked erased-reference view.

## Minimal TypeScript

```ts
interface Key { id: number }
interface Value { version: number }
export function exercise(values: WeakMap<object, object>, key: Key, value: Value): Value | undefined {
  const typed = values as WeakMap<Key, Value>;
  typed.set(key, value);
  return typed.get(key);
}
```

## Expected C++

```cpp
auto typed = flight::checked_weak_map_view<flight::Ref<Key>, flight::Ref<Value>>(values);
typed.set(key, value);
return typed.get(key);
```

The view must share storage with the erased map and validate the recorded value owner on recovery.

## Affected modules

The context-owned mesh upload cache in `@flighthq/scene3d-gl`, which stores heterogeneous values at
the shared runtime boundary and recovers `MeshGeometry`/`GlMeshUpload` at typed consumers.

## Carrying workaround

[`compiler-erased-ref-weak-map-view.patch`](../../source-patches/compiler-erased-ref-weak-map-view.patch)
removes the terminal refusal after the existing key/value proof. It expires when the upstream emitter
uses `checked_weak_map_view` for proven reference domains.

## The source half at `a627849`

At the current pin, the scene3d-gl refusal persists because the source still reaches the map through a mutable
nullable local asserted out of the erased context slot, not through an immutable local view. The pending
source half is specified, re-derived and measured in
[the Scene3D GL upload cache request](scene3d-gl-erased-upload-cache.md).
