# Nullable structural-row widening

A nullable readonly structural reference cannot widen to a nullable readonly view of a proven base
row, even though the runtime converting constructor preserves the object and row owner.

## Minimal TypeScript

```ts
interface Surface { alpha: number }
interface Extended extends Surface { extension: number }
function readSurface(surface: Readonly<Surface> | null): number {
  return surface?.alpha ?? 0;
}
export function readExtended(value: Readonly<Extended> | null): number {
  return readSurface(value);
}
```

## Expected C++

```cpp
return read_surface(std::optional<flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<Surface>>>>>(value));
```

The conversion is allowed only after the generated row proof establishes `Extended` widens to
`Surface`; readonly authority may be preserved or strengthened, never weakened.

## Affected modules

Standard, extended, and specular-glossiness PBR callbacks passed through the upstream
`SurfaceMaterial` binder contract.

## Carrying workaround

[`compiler-nullable-structural-row-widening.patch`](../../source-patches/compiler-nullable-structural-row-widening.patch)
recognizes optional-single structural carriers with the same sentinel and delegates to the proven
runtime conversion. It expires when upstream supports this checked nullable widening.
