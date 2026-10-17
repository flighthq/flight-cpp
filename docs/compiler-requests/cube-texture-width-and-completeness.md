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

## G2 GL environment face revisions use the same selected-owner read

At compiler `806d257` / Flight `a627849`, `missing-member-95ead583` covers
`scene3d_gl/gl_environment_cube.hpp`, `gl_environment_ibl_bake.hpp` and
`gl_environment_skybox.hpp`. The retained first diagnostic is at
`flight/scene3d_gl/gl_environment_cube.hpp:56`: no member named `version` on the seven-arm
`TextureSourceValue` variant.

Every stored arm already declares `double version`: `VoxelGrid`, `TextureSource`, `ExternalTexture`,
`Bitmap`, `CompressedImageResource`, `RenderTarget` and `ImageResource`. This is a read of the active
source owner's existing revision, not a missing host binding or a request for a synthetic version on
an unsupported source. The original open `TextureSource` arm must remain available for vendor kinds.

The pinned source maps `source?.version ?? -1` over the cube faces. The existing
`scene3d-gl-explicit-environment-carriers` patch expands that into an ordered loop with an explicit
null test and one append of either `-1` or `source.version`. The generated code retains presence:

```cpp
for (auto source : cube->sources) {
  versions.push((!source.has_value() ? -1.0 : source.value().version));
}
```

`source.value()` is still the complete value variant. The same module's current-face comparison
emits `source.value().version` after rejecting a null face, and its pixel-kind guard similarly emits
`image.value().kind`. The required operation is a visit through the active reference, for example
`std::visit([](const auto& owner) { return owner->version; }, source.value())`, retaining each existing
presence check, fallback, comparison order and source owner. Reading a common member does not prove
that the active arm is Bitmap or ImageResource.

A runtime helper cannot change a direct member access emitted on `std::variant`; an additional field
on the concrete structs would duplicate storage they already have. A copied TextureSource base, an
unconditional alternative selection, or a closed replacement union would change the source contract.
No runtime or binding conversion is added for this classification.

A bounded experiment changed only the existing revision-loop ternary into explicit null/else branches.
Each face still appends exactly once, in the same order, using `-1` for null and the original owner's
version otherwise. Normal focused SDK generation at the exact locks applied the full declared patch
stack, lowered the required 1643-module dependency closure and emitted only `glEnvironmentCube.ts`:
**1/1 emitted, zero refusals**. Both sibling checkouts were restored cleanly by normal generation.
The emitted header changes only that control flow and still reads `source.value().version` directly
in the else branch, at candidate line 60. Generation acceptance therefore does not fix the native
member projection.

The actual generated candidate was used as a one-header include overlay against the canonical SDK.
Only the three owned headers were compiled, with Clang 22.1.8 at one worker: **0/3 pass, 3 fail,
0 unattempted**. Every first error is the same missing `version` member at candidate line 60. The
source edit was rejected and withdrawn; its original patch is restored byte-for-byte. No generated
header, source patch, runtime, binding, emission repair or override change is carried.

The shipped rows therefore retain their **0/3 pass, 3 fail** G1 qualification and original diagnostics
at line 56, with all three now linked here and the cause routed to the compiler. Candidate diagnostics
are not substituted for the shipped tree's diagnostics. The historical sweep totals and unowned rows
are unchanged. No unchanged-stock or global recheck, upstream test port, or duplicate request was made.
