# `new Map(existingMap)` must copy, not alias

Silent miscompilation. In TypeScript, `new Map(map)` builds a new Map holding the same entries in the same
order. Writes to either map are invisible to the other.

The emitter lowers it to `flight::Map(map)`, or `flight::Map<K, V>(map)` with explicit arguments. When the
argument already has that exact `Map` type, C++ overload resolution prefers `Map`'s implicit copy
constructor to its `explicit Map(const Range&)` constructor. The copy constructor shares storage, because
a JavaScript Map passed by value IS the same object, and that is the representation this runtime relies on.
So the "copy" is the same Map, and every write through it lands in the caller's map.

## Minimal TypeScript

```ts
export function withEntry<T>(map: ReadonlyMap<string, T>, key: string, value: T): Map<string, T> {
  const entries = new Map(map);
  entries.set(key, value);
  return entries;
}

const before = new Map<string, string>();
const after = withEntry(before, 'a', 'v');
// before.size === 0, after !== before
```

Emitted, at flight `a627849`:

```cpp
auto entries = flight::Map(map);   // copy constructor: entries and map share one storage
entries.set(key, value);           // before.size() is now 1
```

## Required C++ shape

`new Map(iterable)` must construct a new Map. When the argument is itself a `flight::Map`, the runtime
already has the exact operation: `Map::clone()` copies every record in insertion order into fresh storage.

```cpp
auto entries = map.clone();
```

For any other iterable argument, the explicit range constructor is already selected and is correct.

The runtime cannot fix this on its own. A copy constructor that copied would break every ordinary
pass-by-value of a Map, which must keep reference identity.

## Affected modules

Confirmed by native tests:
- `@flighthq/registry` `registryTable.ts`: `concatKindMap`, `withKindMapEntry`, `withoutKindMapEntry`. The
  upstream assertions "mutates nothing", "does not mutate the table it was given" and "preserves base entries"
  fail without the repair below (`tests/generated_registry_test.cpp`).

Candidate sites with the same `flight::Map[<K, V>](x)` spelling, not yet examined. Each one aliases exactly
when `x` already has the constructed Map type:
- `scene2d_canvas/canvas_render_state.hpp` and `canvas_texture_resolver.hpp`;
- `scene2d_dom/dom_texture_resolver.hpp`;
- `scene2d_formats/rive_layout.hpp`;
- `scene2d_wgpu/wgpu_velocity.hpp`;
- `scene3d_gl/gl_shaded_prelude.hpp`;
- `swf/swf_timeline_parse.hpp`.

## Workaround carried

The `registry-new-map-copy-base` and `registry-new-map-copy-map` emission repairs respell the three registry
sites from `flight::Map(base)` / `flight::Map(map)` to `base.clone()` / `map.clone()`. `clone()` is the
runtime's copy of a Map: the same entries, in insertion order, in new storage. That is exactly the object
`new Map(map)` creates, so the repair restores the source's semantics rather than choosing new ones. It
expires when the emitter lowers `new Map(existingMap)` to a copy.
