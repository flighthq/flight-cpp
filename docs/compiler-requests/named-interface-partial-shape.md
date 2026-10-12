# Named interface shapes through `Partial<T>`

At the compiler pin where this request was filed, the C++ emitter refused `Partial<T>` when `T` was a
named object interface, even when semantic type information and the generated owner exposed a closed,
statically known row for every member. The original rule no longer reproduces in the minimal fixture at
flight-compiler `2b687ea`; the carried full module still refuses later under a different diagnostic.

**At `a627849` the shape is emitted and the spread over it is dropped.** `@flighthq/menu`'s
`createMenuItemTemplate(template?: Readonly<Partial<MenuItemTemplate>>)` now emits with a
`RowReadonly<RowPartial<…>>` parameter — the shape this request asked for — and then never reads it,
so the caller's fields are lost and the recursive default-fill never runs. That is filed as
[`object-spread-of-a-parameter-dropped.md`](object-spread-of-a-parameter-dropped.md): the shape half
of this request is done at this pin, and what remains is consuming it.

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

## Current pinned boundary

At `2b687ea`, the local fixture above emits. A simple cross-package version with `Material` and the
constructor declared in separate modules also emits, as do both spread orders for the three-field row.
The former `cpp-partial-shape-unresolvable` rule is therefore not an honest minimal explanation for the
current corpus refusal.

The committed `materialPresets.ts` row now stops at:

```text
structural-row construction requires explicit named properties
```

Its complete imported interface and inheritance graph has not been reduced to that diagnostic: smaller
named-`Partial` constructions emit. The P-3D2 3DS Record-spread site carries the same wording but a
different target and is recorded separately as a
[corpus-context non-reproduction](corpus-context-structural-row-record-spread.md). Neither diagnostic
should be reported upstream as the old mapped-shape failure without a fixture that preserves the
additional failing context.

## Originally affected modules

All eleven exported functions in `@flighthq/materials/materialPresets` were refused with
`cpp-partial-shape-unresolvable`. Ten use `Partial<StandardPbrMaterial>` directly; the glass preset
reached the same refusal while constructing its named partial material and extension rows. The full
module remains refused at the current pin, but under the generic structural-row diagnostic above.

`@flighthq/menu`'s `createMenuItemTemplate(template?: Readonly<Partial<MenuItemTemplate>>)` carried the
same original refusal on another named interface, and it was the reason the whole module was refused:
the emitted `MenuItemTemplate` already carries every member as `std::optional`, so the partial has a
closed shape the emitter declines to resolve. Supplied by the `menu-recursive-template-validation`
override.

## Carrying workaround

`materials-upstream-preset-assemblies` supplies the refused module with explicit partial-row aliases
and preserves spread-last precedence. Its glass path also records the separate generated-interface
heritage limitation: until the Scene3D-GL-owned concrete extension value-arm patch is rebased, the
same transmission allocation is retained through the generated `PbrExtension` prefix view.

The workaround can be removed only when the complete preset module emits and source interface heritage
preserves concrete owners through the `PbrExtension` view. The first condition must be tested against
the full imported owner graph; the now-passing three-field fixture is not an expiry proof.
