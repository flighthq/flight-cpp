# Reference recovery from an erased Map

`Map<string, object>` stores a tagged `ErasedRef`, but `Map.get` loses its declared
`object | undefined` evidence and the generic assertion lane runs before checked erased-reference
recovery.

## Minimal TypeScript

```ts
interface Program { handle: number }
interface Runtime { cache: Map<string, object> }
export function get<T extends Program>(runtime: Runtime, key: string): T | undefined {
  return runtime.cache.get(key) as T | undefined;
}
```

## Expected C++

```cpp
auto erased = runtime->cache.get(key);
if (!erased.has_value()) return std::nullopt;
return flight::erased_ref_as<typename T::element_type>(erased.value());
```

Concrete assertions use the same checked operation with their concrete owner type.

## Affected modules

The shared `Map<string, object>` program cache in `@flighthq/scene3d-gl`, including generic lookup
and concrete destruction loops.

## Carrying workaround

[`compiler-generic-erased-map-reference-recovery.patch`](../../source-patches/compiler-generic-erased-map-reference-recovery.patch)
preserves bare-object `Map.get` evidence and runs checked `ErasedRef` recovery before generic dynamic
conversion. It expires when upstream does both.
