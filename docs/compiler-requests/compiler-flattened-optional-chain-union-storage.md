# Optional-chain payload union storage

An optional chain over a nullable multi-arm member emits `optional<variant<...>>`, but immediate
nullish coalescing plans a flat sentinel variant and applies `std::get_if` to the optional wrapper.

## Minimal TypeScript

```ts
interface Texture2D { readonly width: number }
interface TextureCube { readonly size: number }
type Texture = Texture2D | TextureCube;
interface Material { map: Texture | null }

export function apply(target: Material, opts?: Readonly<Partial<Material>>): void {
  target.map = opts?.map ?? null;
}
```

## Expected C++

The coalesce must use the carrier actually emitted by the optional chain:

```cpp
std::optional<Texture> left = /* optional receiver/member read */;
target->map = left.has_value()
    ? std::optional<Texture>{left.value()}
    : std::nullopt;
```

It must not emit `std::get_if<T>(&left)` where `left` is an `std::optional`.

## Affected modules

Classic, specular-glossiness, standard, and extended PBR constructors whose `Partial<Material>`
options carry nullable texture unions.

## Carrying workaround

[`compiler-flattened-optional-chain-union-storage.patch`](../../source-patches/compiler-flattened-optional-chain-union-storage.patch)
records the optional payload-variant plan and reuses the existing equivalent optional-union path. It
expires when contextual coalescing is planned from the emitted optional-chain carrier upstream.
