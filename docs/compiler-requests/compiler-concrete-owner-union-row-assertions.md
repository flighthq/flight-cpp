# Concrete owner assertion from an open union

A represented union may retain an open base owner and redundant concrete owners. An assertion to a
readonly concrete row currently refuses even when exactly one union slot retains that row's native
owner.

## Minimal TypeScript

```ts
interface Extension { readonly kind: string }
interface Anisotropy extends Extension { readonly kind: 'anisotropy'; strength: number }
interface Clearcoat extends Extension { readonly kind: 'clearcoat'; factor: number }
type Value = Readonly<Extension> | Readonly<Anisotropy> | Readonly<Clearcoat>;

export function strength(value: Value | null): number {
  const anisotropy = value as Readonly<Anisotropy> | null;
  return anisotropy === null ? 0 : anisotropy.strength;
}
```

## Expected C++

The emitted path must select the one exact `Anisotropy` slot, preserve absence, and project a row over
the same retained owner:

```cpp
if (!value.has_value()) return 0.0;
const auto anisotropy = std::get<flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<Anisotropy>>>>>(value.value());
return flight::row_get<flight::RowKey<"strength">>(anisotropy);
```

## Affected modules

Built-in PBR extension callbacks and Scene3D material renderers in `@flighthq/scene3d-gl`, including
nullable material callback paths.

## Carrying workaround

[`compiler-concrete-owner-union-row-assertions.patch`](../../source-patches/compiler-concrete-owner-union-row-assertions.patch)
routes the assertion through exact union-member selection and carries source absence. It expires when
flight-compiler performs that checked projection upstream.
