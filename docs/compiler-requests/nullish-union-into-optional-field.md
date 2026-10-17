# A nullish union stored into an optional field must use the optional carrier

An object literal passes a local typed `Light | null` into a field the destination declares
`optional<StructuralRef<...>>`. The emitter lowers the local to its three-state
`variant<StructuralRef<...>, Null, Undefined>` carrier and hands that to the row-field initializer,
which requires `constructible_from<optional<T>, variant<T, Null, Undefined>>` -- false, so the runtime's
row-construction assertion fires.

## Minimal TypeScript

```ts
interface Light { kind: string }
interface Lights { readonly ambient: Light | null; readonly others: Light[] }

declare function createLights(options?: Readonly<Lights>): Lights;

export function build(): Lights {
  let ambient: Light | null = null;
  ambient = acquireLight();
  return createLights({ ambient, others: [] });
}
```

## Observed C++

`@flighthq/scene3d` `sceneDocumentLights.ts` `createScene3DLightsFromDocument`:

```cpp
return flight::lighting::create_scene_3d_lights(
  flight::make_structural_ref<flight::RowReadonly<flight::RowPartial<
      flight::RowOf<flight::types::Scene3DLightsLike>>>>(
    flight::row_field<flight::RowKey<"ambient">>(
      ([&]() -> std::variant<flight::StructuralRef<...AmbientLight...>, flight::Null, flight::Undefined> {
        auto contextual_union_source = ambient;
        if (!contextual_union_source.has_value())
          return std::variant<...>{std::in_place_type<flight::Null>, flight::null};
        return std::variant<...>{std::in_place_type<flight::StructuralRef<...AmbientLight...>>, ...};
      }())),
    ...));
```

```
include/flight/structural_ref.hpp:1584: static assertion failed:
  structural row construction has an incompatible value type
include/flight/structural_ref.hpp:1586: no matching conversion from
  'std::variant<StructuralRef<RowReadonly<RowOf<shared_ptr<AmbientLight>>>>, flight::Null, flight::Undefined>'
  to 'std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<shared_ptr<AmbientLight>>>>>'
```

Instantiating `initialize_row_field` for `RowReadonly<RowPartial<RowOf<shared_ptr<Scene3DLights>>>>` at
`scene_document_lights.hpp:146`. The same failure repeats for `directional`; the sibling array fields
already emit the destination's own carrier (`std::optional<flight::Array<...>>{hemisphere}`).

## Required C++ shape

When the destination field is `optional<T>`, a `T | null` (or `T | null | undefined`) value lowers to
that optional: the sentinel arms become `std::nullopt` and the present arm is the value.

```cpp
flight::row_field<flight::RowKey<"ambient">>(
  ([&]() -> std::optional<T> {
    auto source = ambient;
    if (!source.has_value()) return std::nullopt;
    return std::optional<T>{structural_ref_cast<T>(source.value())};
  }()))
```

This is the mirror of [partial row reads must materialize three-state returns](partial-row-three-state-return.md),
which names the opposite conversion (an `optional<Value>` read emitted into a three-state return).

## Affected modules

`@flighthq/scene3d` `sceneDocumentLights.ts` (`createScene3DLightsFromDocument`) --
`scene3d/scene_document_lights.hpp`, one header, cluster `static-assertion-261f0db6`. Measured at
`-j1`; no other diagnostics in this header were observed.
