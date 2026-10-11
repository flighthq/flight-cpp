# Imported owner recovery from readonly views

Recovering a nominal imported union from nullable readonly views compares unqualified local owner
spellings with qualified imported slots, so an otherwise unique owner match is refused.

## Minimal TypeScript

```ts
// types.ts
export interface Texture2D { readonly dimension: '2d'; version: number }
export interface TextureCube { readonly dimension: 'cube'; version: number }
export type Texture = Texture2D | TextureCube;
export type TextureView = Readonly<Texture2D> | Readonly<TextureCube>;

// consumer.ts
import type { Texture, TextureView } from './types.js';
export function recover(value: TextureView | null): Texture | null { return value; }
```

## Expected C++

```cpp
return std::visit([](const auto& view) -> types::Texture {
  using View = std::decay_t<decltype(view)>;
  if constexpr (/* Texture2D view */) {
    return types::Texture{std::in_place_type<flight::Ref<types::Texture2D>>,
        flight::structural_ref_cast<flight::Ref<types::Texture2D>>(view)};
  } else {
    return types::Texture{std::in_place_type<flight::Ref<types::TextureCube>>,
        flight::structural_ref_cast<flight::Ref<types::TextureCube>>(view)};
  }
}, value.value());
```

Source absence remains absence.

## Affected modules

The PBR extension registry in `@flighthq/scene3d-gl`, where imported nullable readonly texture views
are passed to the canonical texture readiness predicate.

## Carrying workaround

[`compiler-imported-readonly-view-owner-recovery.patch`](../../source-patches/compiler-imported-readonly-view-owner-recovery.patch)
canonicalizes both sides through the declaring namespace before the existing unique-owner proof. It
expires when upstream uses one canonical spelling for imported view owners and nominal slots.
