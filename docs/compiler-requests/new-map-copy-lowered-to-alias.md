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

## Workaround carried before the patch (retired)

The `registry-new-map-copy-base` and `registry-new-map-copy-map` emission repairs respelled the three registry
sites from `flight::Map(base)` / `flight::Map(map)` to `base.clone()` / `map.clone()`. `clone()` is the
runtime's copy of a Map: the same entries, in insertion order, in new storage. That is exactly the object
`new Map(map)` creates, so the repair restored the source's semantics rather than choosing new ones. They
were retired when the compiler patch below produced the copy itself.

## Carried compiler patch (supersedes the registry repairs)

[`compiler-new-map-copy.patch`](../../source-patches/compiler-new-map-copy.patch) spells a flight-cpp
`new Map(argument)` whose single argument is itself a `Map` or `ReadonlyMap` as `flight::Map<K, V>(argument).clone()`.
Entry arrays, iterators, literals and empty construction are emitted as before. Its fixtures are in
`cppNewMapCopy.test.ts`. The `registry-new-map-copy-*` emission repairs match nothing after a registry
regeneration, so they are retired. The compiler's spelling is `flight::Map(map).clone()` where the repair wrote
`map.clone()`: the text differs, the behavior does not, and `tests/generated_registry_test.cpp` covers it.

Live iteration is unaffected. A Map's own iterators still walk its storage as it changes (a key added during
`for...of` is visited), and the copy is a snapshot taken at construction, which is exactly what `new Map(map)`
is in JavaScript.

## Inventory for owners: upstream sites this patch changes on their next regeneration

At flight a627849, `new Map(<expression>)` appears 45 times outside tests. Only those whose argument is itself
a Map change; array-built maps (`debug.ts`, `lottieDocument.ts`) do not. None was regenerated or edited here.

| Package | Source | Sites |
| --- | --- | --- |
| render-gl | `glRenderState.ts` (`buildGlRenderRegistries`, currently refused) | 12 |
| scene2d-canvas | `canvasRenderState.ts` | 10 |
| render-wgpu | `wgpuRenderState.ts` | 9 |
| scene2d-dom | `domRenderState.ts`, `domTextureResolver.ts` | 4 |
| swf | `swfTimelineParse.ts`, `swfDocument.ts` | 2 |
| scene2d-formats | `riveLayout.ts` | 1 |
| scene2d-wgpu | `wgpuVelocity.ts` | 1 |
| registry | `registryTable.ts` (regenerated with this patch) | 3 |

Whether each one aliases today depends on whether its argument's C++ type equals the constructed type. Every one
is a silent mutation candidate until its owner regenerates.

`new Set(existingSet)` may have the same shape, because `flight::Set` copies are likewise reference copies. It is
not examined here.
