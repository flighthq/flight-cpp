# A value of a named interface must keep that row carrier, not be recast as an anonymous struct

A map typed `Map<string, ObjMaterial>` yields `Ref<ObjMaterial>` values. When one is passed to a
parameter declared `Readonly<ObjMaterial>`, the emitter re-derives the value's shape as an anonymous
struct named by its members and constructs a structural row over **that** schema from the
`shared_ptr<ObjMaterial>`. The runtime refuses: the anonymous struct is an unrelated C++ owner, so
`flatten_ref<ObjMaterial>` hits its incompatible-object assertion. The declared named carrier would
have converted exactly.

## Minimal TypeScript

```ts
interface Material { ambient: number[]; name: string }
declare function hasPbr(material: Readonly<Material>): boolean;
declare function materials(): ReadonlyMap<string, Material>;

export function anyPbr(): boolean {
  for (const material of materials().values()) {
    if (hasPbr(material)) return true;
  }
  return false;
}
```

## Observed C++

`@flighthq/scene3d-formats` `objMaterialModel.ts` `collectObjMaterialModels`:

```cpp
auto material = for_of_value;   // Ref<flight::types::ObjMaterial>
found.add(has_obj_pbr_directives(
    flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::ObjMaterial>>>>>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<
          ambient_anisotropy_..._c26dc3f9c14be1fd>>>>(material))));
```

The anonymous struct's members are byte-identical to `flight::types::ObjMaterial` (same names, order,
types and optionality), but it is a separate owner. The failure is:

```
include/flight/structural_ref.hpp:448: static assertion failed due to requirement
  'dependent_false<flight::types::ObjMaterial>': structural reference source has an incompatible object type
  at StructuralRef<RowWritable<RowOf<Ref<anonymous>>>>(material)
```

while the callee (`obj_material_model.hpp:22`) already declares the named carrier:

```cpp
inline bool has_obj_pbr_directives(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ObjMaterial>>>> material)
```

## Required C++ shape

Use the declared named carrier the value already has:

```cpp
flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::ObjMaterial>>>>>(material)
```

or construct the writable named row (`StructuralRef<RowWritable<RowOf<Ref<ObjMaterial>>>>(material)`)
and project it. No pointer cast, copied owner, or narrowed domain is wanted, and no anonymous struct
needs to be emitted at all.

## Affected modules

`@flighthq/scene3d-formats` `objMaterialModel.ts` (`collectObjMaterialModels`) --
`scene3d_formats/obj_material_model.hpp`, cluster `static-assertion-ecc68c2b`. Measured at `-j1`.

Interim workaround carried: emission repair
`scene3d-formats-alias-obj-material-structural-shape` aliases the anonymous shape to
`flight::types::ObjMaterial` (members verified identical) so the second owner never exists. It expires
when this request lands and the emitter stops re-deriving the row.
