# Cube texture property reads and nullable face completeness

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, with the carried source patches, the emitted cube header
has two independent defects after its constructor call is made usable.

## A common property is read on the variant itself

`getCubeTextureFaceSize` retains a present `TextureSourceValue`, a variant of seven concrete source
references, then emits `face.value().width`. The property belongs to the selected reference's owner.
The correct operation visits that alternative and reads `source->width`, preserving the source owner,
the existing presence guard, traversal order, and first-present return.

The derived cube override uses that exact visit. Native regression coverage selects all seven source
alternatives, observes their widths and later width mutations, and verifies that the retained source
allocation does not change. No common-base copy, reinterpretation, or synthetic source owner is used.

## Indexed presence tests inspect bounds instead of the nullable face

The current `is_cube_texture_complete` tests `sources.get(i).has_value()` at indices zero through
five. That is the outer bounds optional. A null face occupies an in-range slot with an empty inner
optional, so the generated function reports a cube with six null faces as complete.

This is the named-tuple instance of
[nullable array element null tests](nullable-array-element-null-test.md). The current compiler patch
for indexed presence does not prevent this emitted tuple case.

The exact pinned TypeScript body is:

```ts
return getCubeSources(cube).every((face) => face !== null);
```

The temporary `texture-direct-cube-face-storage` source patch replaces it with six fixed-index tests
to bypass a separate callback carrier-order defect. Its old manifest claim that completeness remained
a refusal was stale: the function is emitted with the wrong bounds tests. The corrected manifest
records that limitation and the derived override that carries the original `every` behavior.

## Strict null and length behavior

The oracle extracts and runs the body directly from the pinned Git object, before source patches.
It verifies these results:

| Face storage | Pinned TypeScript result |
| --- | --- |
| Six null faces | false |
| Six populated faces | true |
| Any one of six faces null | false |
| Empty array | true |
| Short array containing only populated faces | true |
| Short array containing null | false |
| Six populated faces followed by a seventh null | false |
| An in-range undefined element | true |

`undefined !== null` is true. An out-of-range read is undefined, but `every` never reads beyond the
array's length. Thus a correction that requires six elements, tests only bounds, or equates an
out-of-range read with a null element changes the pinned behavior.

The native face element contract admits source owners and null, not an explicit in-range undefined
value. Its optional element represents null; `Array::get` keeps bounds absence in an independent
outer optional. The native regression checks those two layers without inventing an owner for
undefined. The JavaScript-only undefined case remains in the pinned oracle.

The override restores `sources.every` using a generic C++ predicate over the actual optional element:

```cpp
return sources.every([](const auto& face) { return face.has_value(); });
```

This avoids a separately synthesized callback variant, preserves actual array length and ordered
short-circuiting, and never performs an out-of-range read. The oracle records getter reads up to each
first-null position. The native test checks the same six results and the runtime `Array::every`
primitive's visited-index order and early return.

## Carried scope and expiry

The override is re-derived from the current repaired generated cube header. It changes only the
selected-owner width read and completeness body. That unit initially leaves `cloneCubeTexture`,
`copyCubeTexture`, `equalsCubeTexture`, and `setCubeTextureFace` refused and absent. The independent
[cube API follow-up](cube-face-index-write-domain.md) supplies clone/copy/equals and makes the original
all-surface Texture target pass. The unrestricted setter remains visibly absent and keeps the cube
supply partial; the target does not exercise that setter. The focused width/completeness test covers
the supplied constructor, face-size and completeness behavior.

Drop the override when the emitter lowers the common property through the retained variant and tests
the nullable tuple element's correct absence layer, or when the source callback emits with one
canonical variant carrier. Its `derivedFrom` hash detects any generated-header change before that.
