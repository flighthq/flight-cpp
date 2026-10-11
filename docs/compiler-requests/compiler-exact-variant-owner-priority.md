# Exact variant owners must outrank shape matches

Variant narrowing treats exact owner identity, a shared literal, and equivalent object layout as
equal evidence. That makes a concrete assertion ambiguous and can select the wrong texture arm.

## Minimal TypeScript

```ts
interface Source { kind: string }
interface Target extends Source { kind: 'target'; target: number }
interface T2 { readonly dimension: '2d'; source: Source | Target | null }
interface T3 { readonly dimension: '3d'; source: Source | Target | null }
interface RenderTexture extends T2 { source: Target }
type Value = Readonly<T2> | Readonly<T3> | Readonly<RenderTexture>;

export function target(value: Value): number {
  return (value as Readonly<RenderTexture>).source.target;
}
```

## Expected C++

The exact `RenderTexture` owner must win before literal or structural matches:

```cpp
if (value.index() == render_texture_index) {
  const auto render = std::get<flight::StructuralRef</* RenderTexture row */>>(value);
  return flight::row_get<flight::RowKey<"target">>(
      flight::row_get<flight::RowKey<"source">>(render));
}
```

Heterogeneous common-property results may widen only when every visitor arm has an exact checked
destination.

## Affected modules

The GL texture resolver and its `Texture2D`, `Texture3D`, `TextureCube`, and `RenderTexture` flows in
`@flighthq/scene3d-gl`.

## Carrying workaround

[`compiler-exact-variant-owner-priority.patch`](../../source-patches/compiler-exact-variant-owner-priority.patch)
ranks exact carrier evidence before discriminants and shape, then applies the ranking consistently to
tests, reads, and view conversion. It expires when upstream variant selection uses the same ordering.
