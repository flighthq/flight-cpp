# Present projection of a dual-sentinel multivariant

After control flow excludes both `null` and `undefined`, present-binding projection still demands one
value slot. It rejects a heterogeneous union even though several concrete owners legitimately remain.

## Minimal TypeScript

```ts
type Source =
  | { readonly kind: 'image'; readonly source: string; readonly width: number }
  | { readonly kind: 'voxel'; readonly depth: number; readonly width: number };

export function width(source: Source | null | undefined): number {
  if (source === null || source === undefined) return -1;
  return source.width;
}
```

## Expected C++

The emitter must rebuild the same present alternatives without either sentinel, then visit them:

```cpp
PresentSource present = std::visit([](const auto& value) -> PresentSource {
  // Construct the matching present arm; absent arms are unreachable after the guards.
  return PresentSource{std::in_place_type<std::decay_t<decltype(value)>>, value};
}, source);
return std::visit([](const auto& value) { return value->width; }, present);
```

## Affected modules

Nullable `TextureLike` and `TextureSourceValue` flows that read fields common to several concrete
native owners after nullish checks.

## Carrying workaround

[`compiler-present-dual-sentinel-multivariant.patch`](../../source-patches/compiler-present-dual-sentinel-multivariant.patch)
maps every remaining exact slot into a smaller present-only variant. It expires when upstream
presence projection supports multiple represented value domains.
