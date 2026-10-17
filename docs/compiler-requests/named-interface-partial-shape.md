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

`materials-upstream-preset-assemblies` supplies the preset bodies with explicit partial-row aliases
and preserves spread-last precedence. At compiler `806d257` / Flight `a627849`, ten standard
presets remain refused at structural-row construction, while the glass body is retained. The native
override still supplies these assemblies through its existing native entry points.

The G2 native assignment failure was a stale override boundary after the concrete-value-arm patch
landed: `ExtendedPbrMaterial.extensions` already stores `Array<PbrExtensionValue>`, but glass
constructed `Array<Ref<PbrExtension>>` using a `reinterpret_pointer_cast` prefix alias. Those array
carriers cannot be assigned. The override now places its existing `Ref<TransmissionVolumePbrExtension>`
directly in the declared variant alternative. The same allocation and shared control block are retained;
no base owner, copied descriptor, runtime covariance or unchecked recovery is needed. The source
patch and open extension arm are unchanged.

The current generated-source hash still matches the override manifest. Its obsolete prefix-alias debt
is removed; the named-Partial/spread compiler debt remains. The native glass function still takes
its existing optional nominal options reference, rather than the emitted readonly structural options
carrier. That separate entry adaptation is not supplied by this storage repair, so the manifest
records the override as partial instead of claiming that all source surface is provided. The materials semantic suite uses the
actual variant carrier and checks concrete descriptor fields, nested overrides, list order and identity,
alias mutation, default values and distinct owners across independent glass constructions.

The workaround can be removed only when the complete preset module emits with the declared extension
value carrier and preserves authored field presence and spread-last precedence. The original passing
three-field fixture remains insufficient to prove that expiry.
