# Partial row reads must materialize three-state returns

A nullable member read from an optional `Readonly<Partial<T>>` receiver can emit an
`optional<Value>` directly into a lambda declared to return `variant<Value, Null, Undefined>`. C++
has no implicit conversion between those carriers, and the generated header fails to compile.

## Minimal TypeScript

```ts
interface Texture2D { readonly kind: '2d' }
interface TextureCube { readonly kind: 'cube' }
type Texture = Texture2D | TextureCube;

interface Material {
  textures: Record<string, Texture> | null;
}

export function textures(opts?: Readonly<Partial<Material>>): Record<string, Texture> | null {
  return opts?.textures ?? null;
}
```

## Required C++ shape

The receiver-absent branch already returns `Undefined`. The present-receiver row read must preserve
the other two states explicitly:

```cpp
auto projected = flight::row_get<flight::RowKey<"textures">>(receiver);
if (!projected.has_value()) {
  return Result{std::in_place_type<flight::Null>, flight::null};
}
return Result{std::in_place_type<Record>, projected.value()};
```

Immediate `?? null` lowering can then project either absence sentinel to the nullable destination
and retain the same record object when present.

## Affected module

`@flighthq/materials/packages/materials/src/customShaderMaterial.ts` fails in
`createCustomShaderMaterial` while initializing the nullable `textures` and `uniforms` record bags.
Its direct header and the package contract/index headers that include it fail together. Repairing the
first site exposes the same malformed return at the second, so both declared keys belong to one defect.

## Carrying workaround

The `materials-custom-shader-records-partial-row-absence` emission repair is anchored to
`flight/materials/custom_shader_material.hpp` and the `textures` and `uniforms` keys. It expands only
the malformed short returns into the emitter's established long-form projection and expires when that
exact output no longer occurs.
