# Null literals in an array of nullable variants must use the element's absence carrier

Measured against Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, with the carried source-patch stack.

`createCubeTexture` defaults its six face slots with:

```ts
sources: opts?.sources ?? [null, null, null, null, null, null],
```

The patched `TextureSourceCubeFaces` tuple retains `TextureSourceValue | null` in every slot.
`TextureSourceValue` is an open union retaining the seven exact source owners, so the generated
storage is `flight::Array<std::optional<flight::types::TextureSourceValue>>`, whose payload is a
variant of those seven reference types.

The emitter chooses that element storage but spells the six null literals as pointer nulls:

```cpp
flight::Array<std::optional<flight::types::TextureSourceValue>>{
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}
```

Every reference alternative could accept a pointer null. No unique present variant arm exists, so
`std::optional<TextureSourceValue>` cannot be constructed from `nullptr`, and the array initializer
fails. The runtime does not need a conversion that chooses an owner for an absent face.

## Required lowering

Use the null state of the declared element carrier:

```cpp
flight::Array<std::optional<flight::types::TextureSourceValue>>{
    std::nullopt, std::nullopt, std::nullopt,
    std::nullopt, std::nullopt, std::nullopt}
```

The array is dense and has exactly six in-range slots. Each element is absent in its optional
carrier; the slots themselves are present. `Array::get` keeps the independent out-of-range absence
in its outer optional. This request does not equate a hole, an out-of-range access, a present null
reference alternative, and an empty element.

When the declared carrier distinguishes both JavaScript null and undefined, a general compiler
fix must still select the correct sentinel; this exact site has only nullable source elements.

## Scope and evidence

`flight/texture/cube_texture.hpp:97` is the direct site; the texture contract and index headers
inherit it. After the guarded readonly sampler repair, the package remains 6/9 independent headers.
An exact shadow substitution of only these six literals removes the array-constructor diagnostic.
The separate readonly options-row constructor boundary and variant `.width` read still fail, so
this repair alone does not improve that 6/9 count or supply the refused cube APIs.

The source already means six null face slots. The exact-owner cube constructor in the texture
override uses the same six `std::nullopt` elements. This is separate from
[fresh null-array contextual element election](fresh-null-array-into-nullable-element-union.md):
here the emitter has already elected the correct array and element types.

A standalone C++20 probe at the same include roots compiled and ran successfully. It proves:

- `Element = std::optional<TextureSourceValue>` is not constructible from `std::nullptr_t`, and is
  constructible from `std::nullopt_t`.
- The replacement produces six in-range empty elements, while `get(6)` has no outer value.
- Populating one slot with the Bitmap alternative retains the identical Bitmap allocation and
  leaves other slots empty.
- Copying the Array retains the same shared slot storage.

The probe's central checks are:

```cpp
using Element = std::optional<flight::types::TextureSourceValue>;
static_assert(!std::is_constructible_v<Element, std::nullptr_t>);
static_assert(std::is_constructible_v<Element, std::nullopt_t>);
flight::Array<Element> faces{
    std::nullopt, std::nullopt, std::nullopt,
    std::nullopt, std::nullopt, std::nullopt};
assert(faces.size() == 6);
for (double index = 0; index < 6; ++index) {
  assert(faces.get(index).has_value());
  assert(!faces.element(index).has_value());
}
assert(!faces.get(6).has_value());
auto bitmap = flight::make_ref<flight::types::Bitmap>();
faces.element(2) = flight::types::TextureSourceValue{
    std::in_place_type<flight::Ref<flight::types::Bitmap>>, bitmap};
assert(std::get<flight::Ref<flight::types::Bitmap>>(*faces.element(2)) == bitmap);
assert(!faces.element(0).has_value());
assert(!faces.element(5).has_value());
auto alias = faces;
assert(&faces.element(0) == &alias.element(0));
```

## Carrying repair

`texture-cube-null-face-array-optional-storage` is an exact
`respell-reference-member-access` entry scoped to `flight/texture/cube_texture.hpp`. It replaces
only the complete six-element initializer above. It changes no branch, owner, array allocation,
slot order, helper call, or return value.

The entry expires when the emitter lowers these null literals using the declared optional element
carrier. A full `sdk:check` detects its zero-match expiry; scoped generation is used to verify the
Texture slice during the ongoing pin transition.
