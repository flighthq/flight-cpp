# Named interface shapes through `Partial<T>`

The C++ emitter refuses `Partial<T>` when `T` is a named object interface, even when semantic type
information and the generated owner expose a closed, statically known row for every member.

## Minimal TypeScript

```ts
interface Material {
  baseColor: number;
  metallic: number;
  roughness: number;
}

declare function createMaterial(opts: Partial<Material>): Material;

export function createPreset(opts?: Readonly<Partial<Material>>): Material {
  return createMaterial({ baseColor: 0xffffffff, metallic: 1, roughness: 0.25, ...opts });
}
```

## Expected C++

The named interface should resolve to the same partial structural row already used by generated
functions that accept it directly:

```cpp
using MaterialOptions = flight::StructuralRef<flight::RowReadonly<
    flight::RowPartial<flight::RowOf<flight::Ref<Material>>>>>;
```

Object spread must preserve source order: preset defaults are installed first, then every field
present in the caller's partial row replaces its corresponding default.

## Affected modules

All eleven exported functions in `@flighthq/materials/materialPresets` are currently refused with
`cpp-partial-shape-unresolvable`. Ten use `Partial<StandardPbrMaterial>` directly; the glass preset
reaches the same refusal while constructing its named partial material and extension rows.

## Carrying workaround

`materials-upstream-preset-assemblies` supplies the refused module with explicit partial-row aliases
and preserves spread-last precedence. Its glass path also records the separate generated-interface
heritage limitation: until the Scene3D-GL-owned concrete extension value-arm patch is rebased, the
same transmission allocation is retained through the generated `PbrExtension` prefix view.

The workaround can be removed when named object interfaces resolve through `Partial<T>` and the
emitter preserves concrete owners through source interface heritage.
