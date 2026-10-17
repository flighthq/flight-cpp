# Reading the elements of an erased array must use the runtime's exact array recovery

JSON-shaped parsers take `raw: unknown`, narrow it with `Array.isArray(raw)`, iterate it, and test each
element against `null` before reading it as an object. TypeScript types the narrowed array as `any[]`, so
each element is `any`. The emitter refuses the null test on that element, and every faithful respelling
measured below fails one step later, because it never reads the erased value back as an array.

The runtime already has the carrier: `flight::Any` holds an erased array as `detail::AnyArray`, which keeps
the array's own identity and its exact type, and `Any::external_if<flight::Array<T>>()` recovers it only
when it was stored as exactly that type. The emitter's own refusal text says "flight::Any has no array
alternative or checked exact-array extraction", which is the part that is out of date.

Measured at flight-compiler `2b687ea` with all 42 of this repository's compiler source patches applied, one
module per case; the emitting cases were then compiled with clang 22.

## Minimal TypeScript (measured)

```ts
// a_isarray_iterate_null_test.ts
export function countObjects(raw: unknown): number {
  if (!Array.isArray(raw)) return 0;
  let n = 0;
  for (const entry of raw) {
    if (entry === null || typeof entry !== 'object') continue;
    n++;
  }
  return n;
}
```

```text
cpp emission failed for @local/source/a_isarray_iterate_null_test.ts: a presence test against null has no
absence channel in the emitted C++ storage for identifier
```

## Pinned boundary

| case | emission | C++ |
| --- | --- | --- |
| `Array.isArray(raw)`, iterate `raw`, `entry === null` | **refused** (no absence channel) | — |
| `raw: readonly unknown[]` parameter, same loop and test | emits | — |
| `entry: unknown` alone, `entry === null` | emits | — |
| `Array.isArray(raw)`, then `raw.length` | emits (`flight::is_array`, `named_properties(raw).get("length")`) | — |
| narrowed, then `const items: readonly unknown[] = raw` and iterate `items` | emits | **fails**: no viable conversion from `flight::Any` to `flight::Array<flight::Any>` |
| narrowed, then iterate `raw as readonly unknown[]` | **refused**: no identity-preserving reading as `flight::Array<flight::Any>` | — |
| `Array.isArray(owner.bone)`, then the typed local | emits | **fails**, as above |

## Required C++ shape

After `flight::is_array(raw)`, recover the array through the checked exact recovery and iterate it, with each
element an `flight::Any`, whose null test is already represented:

```cpp
if (!flight::is_array(raw)) return 0.0;
const auto* items = raw.external_if<flight::Array<flight::Any>>();
if (items == nullptr) /* stored as another element type: no honest erased reading */;
for (const flight::Any& entry : *items) {
  if (entry.is_null() || !entry.is_object()) continue;
  n += 1;
}
```

The recovery is only exact for an array stored as `Array<Any>`. Producers that store erased JSON arrays
must store that type (JSON parsing currently produces `flight::JsonValue`, not `Any`, and the parser
modules' own `JSON.parse` sites are not emitted either), so both halves need to agree on it.

## Affected Flight modules

`@flighthq/skeleton2d-formats` at flight `a627849`; every site has the measured shape.

`@flighthq/textureatlas-formats` at flight `a627849`, `firstFrame` in `textureAtlasDetect.ts`: after
`Array.isArray(frames)` the emitter narrows only the guard and writes
`frames[static_cast<size_t>(0.0)]` on a `flight::Any`, so `texture_atlas_detect.hpp:67` reports
`type 'flight::Any' does not provide a subscript operator` and the three format headers fail. This is
the index-zero form of the same recovery: the element must come from
`frames.external_if<flight::Array<flight::Any>>()`, not from a subscript on the erased value; as
above, the recovery is exact only for an array stored as `Array<Any>`.

Refused as "a presence test against null has no absence channel":
- DragonBones: `dragonBonesAnimationsHandler.ts`, `dragonBonesBoneTimelineHandler.ts`, `dragonBonesParse.ts`,
  `dragonBonesParseHelpers.ts`, `dragonBonesSectionCounts.ts`, `dragonBonesSlotTimelineHandler.ts`,
  `dragonBonesSlotsHandler.ts`;
- Spine JSON: `spineJsonBoneTimelineHandler.ts`, `spineJsonBonesHandler.ts`, `spineJsonSlotsHandler.ts`,
  `spineParse.ts`.

Refused as "no array alternative" (reading an erased value as an array):
- `dragonBonesBonesHandler.ts`, `dragonBonesSkinsHandler.ts` (`flight::Array<double>`), and
  `skeleton2dJsonAnalyzer.ts` (`flight::Array<flight::Any>`).

Together these refuse 14 of the package's 59 modules directly, and most of its 24 dependency cascades.

## Carrying workaround

None. No faithful source spelling reads an erased JSON array: each respelling measured above refuses or
fails to compile.


## G2: an erased array length reaches a numeric assignment without recovery

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` / compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, all seven rows in G1 cluster
`other-18dcce71` first fail at `flight/skeleton2d_formats/spine_parse_helpers.hpp:90`:
`assigning to 'double' from incompatible type 'Any'`. This is one shared erased-array reading cause,
not seven unrelated diagnostics.

The pinned `packages/skeleton2d-formats/src/spineParseHelpers.ts` source in
`skipCrumbSpineTimelineGroup` counts an array by its length and an ordinary object by its keys:

```ts
let count = 0;
if (Array.isArray(raw)) count = raw.length;
else if (raw !== null && typeof raw === 'object') count = Object.keys(raw as Record<string, unknown>).length;
```

The committed emission keeps `raw` as `flight::Any` and emits the array arm as:

```cpp
double count = 0.0;
if (flight::is_array(raw)) {
  (count = flight::named_properties(raw).get(flight::String("length")));
}
```

`NamedProperties::get` returns `Any`, not `double`, so the numeric assignment cannot compile.
There is also no array length behind this read: `named_properties(const Any&)` asks
`Any::erased_row_owner()`, which handles `AnyObject` only, whereas boxing an array selects `AnyArray`.
The resulting empty named view returns `Any(undefined)` for `"length"`. Adding `.as_number()` would
therefore only turn the compile failure into a failed read. Adding implicit conversion on all `Any`
values would lose the checked erased-read boundary and still supply no length.

The runtime can recover `Array<T>` through `external_if<Array<T>>()` at its exact stored element type,
sharing the original array storage. `Array.isArray` establishes array-ness, not that exact `T`; an array
boxed as `Array<double>` is still an array and cannot be recovered as `Array<Any>`. The required
compiler/runtime path must read the live original array without copying, either through a proved exact
carrier or an explicit erased-array length/index contract. A length snapshot captured when boxing would
be wrong after an alias mutates the array. The existing measured typed-local/assertion failures above
already show why simply respelling the unknown receiver as `readonly unknown[]` is not a working fix.
No fresh source-patch experiment or native rerun is claimed for this G2 disposition.

The array branch must continue to count `length`, rather than `Object.keys(raw).length`: array holes
and enumerable properties make those different questions in JavaScript. It must retain the original
object/array distinction and diagnostic count. No numeric coercion, copied array, fake property, or
replacement behavior is carried. The later `buildSpineSegmentEasings` body in this same emitted header
also reads guarded `curve.length` through the erased named view and indexes the erased `curve`; those
are visible source/emission boundaries, not freshly measured next-diagnostic or pass claims.

The owned headers, all under `flight/skeleton2d_formats/`, are
`spine_json_bone_timeline_handler.hpp`, `spine_json_draw_order_timeline_handler.hpp`,
`spine_json_skins_handler.hpp`, `spine_json_stub_handlers.hpp`, `spine_json_timeline_handlers.hpp`,
`spine_parse.hpp`, and `spine_parse_helpers.hpp`.
**0/7 pass, 7 fail** is preserved from G1. Every row remains failing and now links this request; the
old cluster ID is retained, its uncategorised cause is replaced with this actual cause, and its route
is compiler. No C++ input changed, so this disposition performs no unchanged compile rerun and makes
no native or behavioral improvement claim.


## G2: a refused DragonBones handler remains referenced as a callback value

At the same Flight `a627849` / compiler `806d257` pins, cluster
`undeclared-identifier-b90ed4db` has seven ANIM headers whose shared first error is
`dragon_bones_animations_handler.hpp:128`: undeclared `dragon_bones_animations_section_handler`.
This is a retained reference to a refused source function, rather than a missing runtime primitive or
an emitted definition placed after its use.

`packages/skeleton2d-formats/src/dragonBonesAnimationsHandler.ts:9` reads
`context.armature.animation`, checks `Array.isArray(raw)`, iterates it, and skips entries for which
`entry === null || typeof entry !== 'object'`. This is the exact module already listed in this
request's erased-array/null-test refusals. Its header has:

```cpp
// NOT GENERATED: function dragonBonesAnimationsSectionHandler -- source line 9
// refusal: cpp-presence-test-without-absence-storage [compiler-restriction]
```

The emitter then retains the source's exported reader alias as a value initialization:

```cpp
inline std::function<void(flight::Ref<flight::types::DragonBonesSectionContext>)>
    dragon_bones_animations_section_reader = dragon_bones_animations_section_handler;
```

That is the emitter's exact callback carrier. `DragonBonesSectionContext::armature` is emitted as
`Record<String, Any>` from the source's `Readonly<Record<string, unknown>>`; the body must recover the
original animation array and its entries, rather than copy an asserted array or invent entry owners.
The sibling `dragon_bones_section_handlers.hpp` also passes the same missing function to the registry
and stores it in `dragon_bones_all_section_handlers`. No declaration or definition of this handler
exists elsewhere in generated, override, or runtime headers. Reordering declarations cannot recover it.

The existing `declare-refused-function` repair requires all three witnesses, including a retained
**direct call** to the symbol. Here the exact refusal and missing definition agree, but the retained
use is a callback-value reference, with no direct call in the refused function's header. A probe using
its exact emitted signature through `applyEmissionRepairs` returned no matched files and left the
header byte-identical. The mechanism was neither broadened nor bypassed. No declaration repair was
shipped, so no negative-link result is claimed. A future repair for callback-value references would
also need to verify the retained inline initializer's linkage behavior and preserve the source alias;
compiling a declaration alone would not supply the animation handler.

The source body dispatches timeline handlers, builds channels/clips and durations, accumulates unresolved
bone and unsupported/unregistered timeline counts, and emits the corresponding diagnostics. An empty
`std::function`, a no-op body, or replacing the reader with another function would change behavior or
identity. The source explicitly aliases reader to handler; its existing tests assert that identity
and registry membership. Those tests were read as requirements only, not ported or run in this task.
No runtime method or external binding can substitute for this omitted TypeScript algorithm. The
source-spelling attempts already recorded above do not solve the erased-array recovery boundary;
no fresh patch experiment or new implementation is claimed here.

The owned headers, under `flight/skeleton2d_formats/`, are `_internal_index.hpp`, `contract.hpp`,
`dragon_bones_animations_handler.hpp`, `dragon_bones_full.hpp`, `dragon_bones_handlers.hpp`,
`dragon_bones_section_handlers.hpp`, and `skeleton_detect.hpp`.
**0/7 pass, 7 fail** is preserved from G1; all seven rows remain failed and now link this existing
request. The cluster route is compiler. No C++ input changed, so no unchanged native compile rerun was
performed. Closing the missing name must not be reported as restoring the refused animation behavior.

## G2 grid track-list validator retains the same erased-element boundary

At Flight `a627849` / compiler `806d257`, `layout/grid_layout.hpp` omits
`isTrackList(value: unknown): value is readonly GridLayoutTrack[]` as
`cpp-typeof-runtime-domain-unrepresented`. After `Array.isArray(value)` and the nonempty check,
its loop tests `typeof track !== 'object' || track === null`. The exact refusal says the iterated
value has no closed runtime type evidence. The input is still source `unknown`; it is not restricted
to an already-validated native `Array<GridLayoutTrack>`.

The retained container validator calls this missing helper on `columns` before `rows`, each stored as
`optional<Array<GridLayoutTrack>>`. Their existing `Any` optional/array construction preserves the array
identity and absent state. `layout-declare-refused-track-list` declares the exact `bool(Any)` boundary,
matching the source unknown carrier used by the same-header numeric guards, and supplies no validation.
The [retained-boundary notes](retained-declaration-calls-omitted-callee.md#g2-grid-track-guard-and-resolver-retain-two-distinct-omissions)
record the independent link checks.

Faithful emission must reject nonarrays, empty arrays, null/nonobject elements and unknown kinds. It must
accept `auto`, require finite nonnegative `size` for `fixed`, and finite positive `fraction` for
`fraction`. The source repeats kind/member reads and short circuits in a defined order; preserve getter
reads and exceptions instead of snapshotting, copying elements, coercing values or assuming every array
already contains valid track owners. A typed-only loop, default true/false guard or copied erased array
would not implement the source predicate. This G2 declaration leaves the entire body refused.

## G2 DragonBones skins: emitted display iteration and refused collection

At compiler `806d257` / Flight `a627849`, original cluster `other-5dfe27ec` contains
`skeleton2d_formats/dragon_bones_skins_handler.hpp`. Its completed Clang22.1.8 one-worker check is
**0/1 pass, 1 fail, 0 unattempted**, with the first error at line373:

```cpp
if (!flight::is_array(raw)) return displays;
for (auto raw_display : raw) { /* parse and append every display position */ }
// invalid range expression of type 'flight::Any'; no viable 'begin' function available
```

This is `parseDragonBonesDisplayList(raw: unknown, ...)`, not a declaration-order problem. The
source guard admits any array, and each cell goes to a parser taking unknown. Current `AnyArray`
retains the original array identity and an exact `Array<T>` handle in std::any;
`external_if<Array<T>>()` already recovers that exact type. There is no general element-read thunk,
array iterator or erased element view. Merely adding an Any iterator that handles only
`Array<Any>`, requiring `Array<double>`, converting the source to a replacement array, or dropping
cells that cannot be recovered would narrow the source's admitted arrays or replace identity.
The producer/erased-reader contract must agree without asserting that every boxed array has one
particular native element type. A guard is not that exact-type evidence.

Eight declarations remain explicitly omitted: `numberArray`, `parseDragonBonesRegionDisplay`,
`toFloat32Array`, `toUint16Array`, `parseDragonBonesWeightedMesh`,
`parseDragonBonesMeshDisplay`, `parseDragonBonesDisplay` and `parseDragonBonesSkins`.
The collector's exact `cpp-presence-test-without-absence-storage` diagnostic names only an
identifier, not a specific variable; its source has null/object checks in both raw-skin and raw-slot
loops, so neither is guessed as the uniquely failing test. Region/mesh/weighted construction also
refuses contextual Any conversion. `numberArray` reports erased exact-array recovery missing, even
though the current runtime can recover an already-known exact Array type. Array.isArray plus an
erased number-array assertion does not establish a homogeneous `Array<double>` or validate cells.
The [static typed-array from request](typed-array-static-from-binding.md) separately covers the
Float32Array/Uint16Array ambient member omissions; declaring that member would not supply the
unrepresented erased reads or other bodies.

### Full skin and display contract

- Keep skin and slot traversal in source order. Empty or missing skin names become `default`;
  nonarrays and null/nonobject entries follow their actual guards. Every valid skin-slot array yields
  its wardrobe entry, including alternate skins and empty attachment lists.
- Preserve every display position in the default skin's table, including a null for an unsupported
  display. The source updates that table before checking whether slotOrder resolves the slot. Unknown
  slots do not create named skin attachments, but must not erase or shift the default positional list.
  Named nonnull displays become attachments under the actual slot index and display name, retaining
  the same attachment object in both results. The handler appends skins and sets table entries on the
  original context arrays/Map; the reader remains its actual handler alias.
- A nonstring display type defaults to image. Image regions use the existing parsed transform.
  Unweighted meshes retain converted UVs, triangles, direct vertices and null skin. Unsupported kinds,
  shared meshes and legacy weights without bonePose report their source Skip diagnostics and occupy
  null display positions; they are not replaced with images or removed.
- Weighted meshes apply slotPose then inverse bonePose, remap raw bone indices positionally through
  rawIndexToOutput, and retain the authored influence weights. Duplicate bone names cannot collapse
  indices. Truncated streams, unresolved remaps/poses, singular matrices and influence overflow follow
  the existing recovery paths; counts cap at65535 while input pairs are still consumed. Each accepted
  influence is outputBone/x/y/weight, with Float32 conversion, and the recovered path emits its single
  Recover diagnostic. Do not normalize weights, use an epsilon determinant guard or silently drop the
  entire mesh to avoid this domain.

The nullable result still requires the original region/mesh owner to widen to Attachment2D; the
[heritage request](interface-heritage-widening.md#g2-dragonbones-skins-need-the-original-derived-attachment)
records that independent explicit refusal. Record/object recovery and missing-key reads also remain
under [the parsed JSON request](parsed-json-object-shape-reads.md): the retained section handler reads
`armature.get("skin").value()`, which cannot stand for JavaScript undefined on a missing key.
Those are inspected source/refusal boundaries, not extra winning header diagnostics or native tests.

No C++, runtime iterator, binding, source patch, declaration/repair, override or generated inventory
changes for this disposition, so no changed generation needs reproducing. A declaration cannot supply
the erased loop or eight omitted algorithms; no positive-unused/negative-link result is claimed.
No native skin, weighting, wardrobe, parser or functionality pass is claimed. The original row remains
failed with compiler route, and other DragonBones/Spine/particle/grid rows retain their own evidence.
