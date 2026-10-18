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

## G2 Spine JSON slots keep aligned outputs and original bone reads

At compiler `806d257` / Flight `a627849`, `spineJsonSlotsHandler.ts:11` refuses
`parseSpineSlots(raw: unknown, bones: readonly Bone2D[])` as
`cpp-presence-test-without-absence-storage`. Its source rejects a nonarray with two fresh empty result
arrays, then iterates unknown entries and skips null/nonobject entries. The exact diagnostic names an
identifier without its source name; the loop's source shape is recorded without inventing an exact
emitter-identified variable. A guard does not prove that a boxed array is only Array<Any>, nor that
object entries are specific native Slot2D or Record owners. Recover original elements without copying
an asserted array or narrowing the full JSON/unknown input domain.

Each accepted entry creates one Slot2D and one corresponding attachment-name entry, in input order.
String names (including empty strings) remain strings, other names become null. A string bone name
searches the original readonly bone array and selects the first strict matching name; unknown or
nonstring bone names retain -1 and do not cause the slot to be dropped. Preserve repeated slot.bone
reads, short circuit and first-match order instead of sorting bones, caching those member reads or
collapsing duplicate names into a different map policy.

Attachment names are strings or null at the same output index. Each new slot initially has null
attachment, its actual bone index/name and `parseSpineColor(slot.color)`. The source color helper
uses white for nonstrings or strings whose length is not8; otherwise it uses parseInt base16, white
for NaN and the unsigned32 result. It is not an added full-hex validator. Neither universal white nor
filtering unresolved slots/attachment names preserves the source.

The section handler completes parsing before mutating the original context. It appends all slots
first, then the aligned attachment names, retaining existing context arrays and original bone owners.
Its exported reader aliases the actual handler. A missing doc.slots property must reach the helper
as undefined/nonarray, whereas the retained emitted `doc.get("slots").value()` also exposes the
[existing missing-key/JSON read problem](parsed-json-object-shape-reads.md). No new schema validation
or exact anonymous-object assertion can turn valid dynamic inputs into a narrower native-only domain.

The [retained-callee/result-owner request](retained-declaration-calls-omitted-callee.md#g2-spine-json-slots-require-a-complete-result-owner-as-well-as-the-refused-callee)
records original1 **0/1 pass** at the missing helper81. Its exact existing declaration/result-owner
candidate fails the real unused-header compile at the result dependency's optional Record diagnostic,
before either link gate. This does not supply erased iteration or the parser algorithm. No C++ body,
source rewrite, copied owner, fake slots/parser, empty-success substitute or narrowed input is carried;
source tests are requirements only, with no native parser behavior or link pass claimed.

## G2 DragonBones bones preserve topological order and positional remaps

At compiler `806d257` / Flight `a627849`, `dragonBonesBonesHandler.ts:40` refuses
`parseDragonBonesBones(raw: unknown, diagnostics?: ImportDiagnostic[])` as
`cpp-erased-array-predicate-runtime-required`. Its emitted refusal says Any lacks an array carrier
and exact extraction. Current AnyArray already preserves shared-array identity, its exact Array<T>
type and checked `external_if<Array<T>>()`; it has no generic erased element-read view. Array.isArray
does not prove Array<Any>, Array<double> or a native Bone2D/Record element owner. A faithful boundary
must admit the full guarded unknown-array domain without copying it or silently skipping a boxed
element type. JSON.parse's JsonValue and dynamic object/member reads remain under the
[existing JSON request](parsed-json-object-shape-reads.md).

The source contract requires all of the following:

- Nonarrays use a fresh empty raw array. For an array, allocate a raw-length remap filled with -1;
  skip null/nonobject entries without compacting their raw positions. Arrays themselves pass the
  JavaScript object guard; no extra plain-object schema restriction is present. Each accepted entry
  creates one real Bone2D, a pending parent name and its original raw index.
- Names and parent names are strings, including empty strings, or null. Length uses numberOr(0),
  which tests typeof number rather than coercing or imposing finiteness. ParentIndex starts at -1,
  shearX at0. The original transform helper uses rotate/skew when either property exists; otherwise
  rotation=skY and shearY=skX-rotation. Numeric defaults are0 for x/y/rotation/skew and1 for scX/scY.
  Preserve property-presence branching, repeated reads, defaults and the source numeric domain.
- TransformInherit2D has four independent booleans. Only actual booleans override the true defaults,
  including inheritTranslation. Arbitrary combinations remain supported, including rotation=false
  with scale/reflection=true; reducing these to five presets or adding a Skip diagnostic is wrong.
  The emitted transform helper already constructs Ref<types::TransformInherit2D>, matching Bone2D;
  there is no inspected anonymous-preset owner mismatch to borrow from the Spine task.
- Repeatedly scan pending entries in their current order. Accept roots and children whose parent
  name is already mapped, assign the current output parent index, set a string name to the new index,
  write the positional raw remap, append the same bone object and splice that pending entry. A child
  skipped before its parent is visited on a later pass. Duplicate names use Map.set's latest resolved
  index at each step; a different topological sort or name-only remap can change observable results.
- If a pass makes no progress, emit exactly the source Drop diagnostic
  dragonbones.unresolved-bone-parent / parseDragonBonesSkeleton with count=pending.length. Append
  every remaining entry in pending order as parentIndex=-1, including orphans/cycles, and fill its
  remap. Do not drop those bones, change severity to Skip or resolve later fallback entries anew.
  Weighted skin indices consume the positional remap; duplicate names must not collapse them.

Parsing finishes before the section handler mutates its original context. It appends the returned
bone references, writes the remap prefix by index without clearing a preexisting tail, then rebuilds
the name map from the local output array (last duplicate wins) and sets those entries on the original
context Map. Do not replace arrays/Map, offset local indices by preexisting context bones, or clear
unmentioned keys. The exported reader remains the actual handler alias.

Two inspected obligations survive a future parser declaration. A missing armature.bone and missing
inheritance keys must reach their helpers as undefined/defaults, whereas retained Record.get(...).value()
throws. Also parseDragonBonesSkeletonWithRegistry initializes context.rawIndexToOutput to []; the
handler's first remap write must grow that same array. The emitted assignment through
`context->raw_index_to_output.element(i)` requires an existing index and throws at0 on that empty array. The
[existing dense indexed-growth request](array-index-write-must-grow.md) covers this source shape.
These are source/emission inspections, not native execution or additional measured first errors.

The [retained result/callee request](retained-declaration-calls-omitted-callee.md#g2-dragonbones-bones-omit-both-the-parser-and-its-complete-result-contract)
records the scoped original1 **0/1 pass** at missing parser141 and absence of an exact complete emitted
result owner. No lawful declaration candidate or either link gate is reached. Source tests for parent
ordering, transform defaults, independent inheritance, orphan Drop severity, duplicate-name weighted
remapping and wrong-typed JSON values establish requirements; they are not native behavior results.
No fake parser/bones, copied owner, empty-success substitute, narrowed JSON domain or unmeasured
source respelling is carried. No native parser, remapping or header gain is claimed.


## G2 Shape JSON finite-number validation retains an erased array

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` / compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, the three recorded failures in cluster
`no-viable-conversion-b5b698fe` share `flight/shape_formats/shape_json.hpp:54`:
`no viable conversion from 'flight::Any' to 'flight::Array<flight::Any>'`.
This is the existing erased-element-reading boundary, not a missing Record conversion or a
missing parser declaration. Source `packages/shape-formats/src/shapeJson.ts:181-186` is:

```ts
function isFiniteNumberArray(value: unknown): boolean {
  if (!Array.isArray(value)) return false;
  for (const entry of value) {
    if (typeof entry !== 'number' || !Number.isFinite(entry)) return false;
  }
  return true;
}
```

The retained C++ performs the guard, then constructs an unproved native array:

```cpp
inline bool is_finite_number_array(flight::Any value) {
  if (!flight::is_array(value)) return false;
  flight::Array<flight::Any> entries = value; // recorded first failure, line54
  for (auto entry : entries) {
    if (entry.type_of() != flight::String("number") || !std::isfinite(entry.as_number()))
      return false;
  }
  return true;
}
```

The current runtime's `detail::AnyArray` retains the original shared-array identity, exact
`Array<T>` handle and type. `external_if<Array<T>>()` recovers that handle only at its stored
exact type; there is no general erased element-read view. `Array.isArray` proves neither
`Array<Any>` nor `Array<double>`. Both a boxed numeric array and a heterogeneous array of
numeric Any cells can satisfy this predicate; invalid cells must be read and rejected. An empty
array is valid, holes read as undefined and fail, nonnumbers/NaN/infinities fail, and finite negative
values and -0 are accepted. Nonarrays, including typed-array views, remain false. A faithful
compiler/runtime boundary must read each original cell in order, with the source short circuit,
without copying or accepting only one native element type. The exact-type recovery above is usable
when independently proved; returning false or throwing merely because another valid array carrier
was boxed would narrow the admitted domain. No implicit Any-to-Array conversion, replacement array,
unchecked cast, permissive iterator or constant guard is carried. The already measured typed-local
and assertion attempts earlier in this request cover this representation gap; none is remeasured here.

### Record, schema and owner boundaries inspected

- `isPlainObject` at source215 is the source's non-null, non-array object predicate with a
  `Record<string, unknown>` type predicate; it is not a prototype/schema validator. Dynamic reads
  of root.shapeFormat/commands, entry.key/args, texture.index and matrix fields require the original
  object's named members and missing-key semantics. The retained `is_matrix_like` uses named reads,
  but `is_matrix_value` at native71-72 asks still-erased Any for direct .a/.b/.c/.d/.tx/.ty fields
  after the user-defined predicate. These are inspected lowering gaps under the
  [existing parsed-JSON/member-read request](parsed-json-object-shape-reads.md), not newly measured
  next diagnostics. An exact anonymous Matrix owner is not established by a structural guard.
- The shared `defaultShapeCommandSchemas` is a ReadonlyMap built from
  `SHAPE_COMMAND_SCHEMA_ARGUMENTS satisfies Readonly<Record<ShapeCommandKey, ShapeCommandSchemaDefinition>>`.
  Its emitted home is `shape_command_schemas.hpp`, with the existing ShapeCommandSchema and argument
  owners. The distinct appender table is a readonly Record of the same 17 built-in keys. It is emitted
  as `Record<String,ShapeCommandAppender>`, with an erased native callback alias
  `std::function<void(Ref<Shape>,Array<void>)>`; source callbacks have heterogeneous positional
  arities and default arguments. The source's `never[]` erasure is not proof that native appenders
  take Array<void>, nor authority to replace or copy the table or any command/argument owner.
  No new callable adapter, table or duplicate schema owner is supplied here.
- `isValidShapeCommandArgs` reads the shared Map on each call, rejects a missing schema, checks
  required count through maximum argument count, then checks each supplied positional type in order.
  Preserve number finiteness, numeric arrays, numbersOrNull, string, boolean, matrixOrNull and texture
  cases. Strings use typeof string, not an added enum validator. Texture validation after reconstruction
  accepts the resolver's non-null object; no source Texture-subtype test is added. Optional trailing
  arguments and null matrix/triangle-array positions remain valid. Do not hard-code only drawPath,
  remove texture commands, tighten empty arrays or manufacture a schema when a lookup is missing.

### Full codec behavior remains required

Formatting walks the original ShapeData.commands flat key/count/argument stream, advancing by
argCount+2, and validates before serializing each entry. It throws the source TypeError for an
unrepresentable command, rather than emitting corrupt geometry or returning empty JSON. Preserve null,
finite scalar/array values, all six finite matrix fields and the supplied JSON.stringify space option.
Live texture internals are not serialized: each texture position becomes a zero-based ordinal reference
in command order. Arrays and existing command/matrix/texture owners are not cloned as a workaround for
the erased-read failure; ordinary JSON serialization still has its specified value-copy behavior.

Parsing catches JSON syntax failure to null, requires a non-array object with shapeFormat exactly3 and
an array commands field, and rejects malformed entries, unknown keys and malformed arguments. It
allocates one real Shape through createShape and replays accepted commands through their original
appendShape builders into that same result's command buffer, preserving order, builder defaults and
content invalidation. An empty command list yields a valid empty Shape; malformed input yields null.
These outcomes cannot be collapsed into an empty-success result or a null-only parser.

Reconstruction keeps null, scalar values and arrays, creates a real Matrix from six finite fields, and
passes a texture reference's numeric index to the caller's resolver. The source checks typeof number
for that index, without an extra integer/range restriction. An omitted resolver or nullish result yields
DROP_COMMAND: stop that command's remaining reconstruction and skip its validation/replay, retaining
other commands. MALFORMED_ARG instead aborts the whole parse. Do not move arity/type validation ahead
of this drop branch or replace resolver results: the exact returned Texture object must enter the
command stream. Preserve repeated reads, resolver/builder exceptions and their order; only JSON.parse
is inside the source catch.

The original module still explicitly omits three functions: reconstructShapeCommandArg122
(`cpp-optional-chain-multi-alternative-receiver-unrepresented`), formatShapeJson36
(`cpp-collection-argument-multiple-present-domains`) and parseShapeJson80
(`cpp-contextual-union-inequivalent`). The format refusal names optional command-token present
carriers that its Any collection cannot receive; the parse refusal compares the optional resolver
function's expanded Texture variant with its declared Texture alias. Native ShapeJsonParseOptions
retains the optional callback over the existing readonly ShapeTextureReference owner, and Texture
retains all four existing 2D/array/cube/3D alternatives. Removing an alternative or fabricating a callback,
parser body, schema or Shape is not a repair. These refusal texts and retained declarations were inspected,
not promoted to newly measured next errors, link results or separate duplicate compiler requests.

The existing shapeJson.test.ts specifies full non-texture round trips, same-object texture resolution,
omitted/null resolver drops, all six non-finite matrix-field rejections, positional/arity errors,
non-finite scalars/arrays, optional tails and schema coverage. Those tests were read as source requirements
only. No native codec, round-trip, parser, rendering, header-availability or link success is claimed.

**Recorded 0/3 pass, 3 fail is retained.** The owned headers are
`flight/shape_formats/{_internal_index,contract,shape_json}.hpp`. This docs/status-only disposition
links their rows and cluster to this existing request and the related JSON-read request. C++ inputs,
repairs, source patches, generation inventory and pins are unchanged; unchanged C++ is not rerun.
Foreign compact product lines, worklist entries and historical/G1 totals are preserved.


## G2 DragonBones section counts read erased length into a numeric Map

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` / compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, recorded cluster
`no-viable-conversion-303c42f1` contains two failing headers. Their first diagnostic is
`flight/skeleton2d_formats/dragon_bones_section_counts.hpp:33`,
`no viable conversion from 'Any' to 'double'`. This is the existing erased-array-length boundary
above, now at a numeric Map value rather than an assignment. Source
`packages/skeleton2d-formats/src/dragonBonesSectionCounts.ts:37-39` reads:

```ts
const value = record[jsonKey];
if (!Array.isArray(value) || value.length === 0) return;
counts.set(kind, value.length);
```

The retained `count_section_...` helper takes `Map<String,double>` and `Record<String,Any>`.
It emits the lookup as `record.get(json_key).value()`, keeps that result as Any, then emits:

```cpp
if (!flight::is_array(value) ||
    flight::named_properties(value).get(flight::String("length")) == 0.0) return;
counts.set(kind, flight::named_properties(value).get(flight::String("length")));
```

Map::set requires its declared double value; NamedProperties::get returns Any. More than a numeric
extraction is missing: named_properties(Any) asks erased_row_owner(), which answers AnyObject only,
not AnyArray. For a boxed array the named view is empty and get("length") returns undefined.
Adding .as_number() would turn this compile failure into a bad read; an implicit Any-to-double
conversion, zero fallback or special Map overload would conceal the absent length rather than count.
The retained tallyTimeline helper at native36-40 has the same erased length for its zero test and
`(counts.get(kind) ?? 0) + raw.length` accumulation; these additional sites were inspected, not
reported as newly measured next diagnostics.

AnyArray already retains the shared array identity, exact Array<T> handle and checked
external_if<Array<T>>() recovery. Array.isArray does not establish that T. A faithful boundary must
read the original array's live length through independently proved exact recovery or a general erased
array length contract. It must admit every source array element type; counting requires no element
schema. A copied Array<Any>, an Array<double>-only path, a boxing-time length snapshot or
Object.keys(value).length would change identity, reject valid input or count a different property set.
No bounded source spelling, existing declaration repair or external binding supplies that emitted
numeric read. No fresh compiler/runtime prototype or native rerun is claimed for this disposition.

### Full counting and requirements contract

- JSON syntax errors return null. Null/nonobject documents and a nonarray/missing armature field are
  unreadable. A real empty armature array returns a fresh empty Map; so does a null/nonobject first
  armature. Only the first armature is inspected, rather than aggregating all armatures. First-armature
  and animation-entry guards are non-null object checks, not extra plain-object/schema validators.
  Preserve the actual guard/read order and missing-key-as-undefined semantics. The current retained
  Record lookups use .value(), which also exposes the
  [existing parsed-JSON/member-read request](parsed-json-object-shape-reads.md). Requiring a nominal
  armature struct, parser version, bone name or validated skeleton would narrow this census.
- The first armature's bone, slot, skin and ik fields contribute their array lengths to bones, slots,
  skins and ikConstraints, in that source order. Missing, nonarray or empty fields add no key, rather
  than a stored zero. Every array position counts, without validating element contents or filtering
  null/unsupported entries. Array length and object-key enumeration are different operations.
- A nonempty animation array contributes its entire raw length to animations before entry filtering.
  Then visit every entry in order, skip null/nonobjects, and tally bone, slot, ffd, ik and zOrder arrays
  under bone, slot, deform, ik and zOrder respectively. Add each nonempty array's full length to that
  family's existing count with missing-count default0; do not overwrite prior animation totals,
  count keyframes inside the family entries, merge section/timeline spellings, or count only valid
  animation objects for the animations total. Empty/nonarray families do not insert keys.
- All helpers mutate the same counts Map and preserve first-insertion order while updating totals.
  The native Map already has shared reference storage; copying the counts into a second result or
  replacing source arrays/Record owners is not a solution. The section/timeline kind constants in
  DragonBonesRegistry.ts supply these exact strings; this function is a presence/length census,
  not handler dispatch or a skeleton parser. No bone/slot/skin schema or registry lookup is added.
- dragonBonesRequirements.ts9 uses non-null counts for isReadableDragonBones, so an empty readable
  document must remain true. parseDragonBonesRequirements iterates present Map keys and constructs
  DocumentFormat requirements named dragonbones.<kind>, then uses the real createRequirementSet
  with DocumentFormat coverage even for unreadable/no-content input. That factory sorts/deduplicates
  requirement identities and coverage; preserve it and the existing Requirement/RequirementSet owners.
  Counts must not become a canned empty requirement set, nor must null and empty Map be conflated.

The module still explicitly omits collectDragonBonesSectionCounts3 as
`cpp-erased-array-assertion-runtime-required`, and countAnimationTimelines42 as
`cpp-presence-test-without-absence-storage`. The collector's refusal says Any has no array
alternative/extraction; that wording is stale for exact recovery, while the general erased-read gap
remains real. The timeline diagnostic names only an identifier, so no uniquely failing variable is
inferred from that text. Its source has the raw-array iteration and animEntry null/object guard.
The requirements header retains two direct calls to the missing collector, with the result stored as
optional<Map<String,double>>. A declaration would supply neither omitted algorithm nor the broken
retained length reads, and cannot qualify unused-header availability while those reads fail. No
candidate declaration, positive unused compile/link/run or qualified negative-link result was tested
or claimed here; these are inspected omissions/calls, not fresh native evidence.

The existing section-count and requirements tests specify invalid JSON/primitive/missing-armature
rejections, readable empty counts, each section family, timeline counts, omitted keys, readability and
namespaced DocumentFormat requirements. They were read as source requirements only. No native counts,
parser, requirements, skeleton or animation functionality pass is claimed.

**Recorded 0/2 pass, 2 fail is retained.** Only
`flight/skeleton2d_formats/{dragon_bones_requirements,dragon_bones_section_counts}.hpp` rows and their
cluster gain this existing request and the related JSON-read link. The worklist slice and this
append-only request note accompany those three compact product lines. C++, runtime, source patches,
repairs, generation inventory and pins are unchanged; unchanged C++ is not rerun. Foreign compact
products, worklist entries and historical/G1 totals are preserved.

## Runtime follow-up: checked live size and indexed reads

The emitted-header consumability follow-up adds two reusable runtime operations in `flight/any.hpp`:

```cpp
std::size_t Any::array_size() const;
std::optional<Any> Any::array_get(double index) const;

if (flight::is_array(raw)) {
  const double count = static_cast<double>(raw.array_size());
  const flight::Any first = raw.array_get(0.0).value_or(flight::Any(flight::undefined));
  // A caller still performs its original typeof/null checks on first.
}
```

Each boxed array records size/get function pointers at the point its exact element type is known.
They read the existing typed handle in its existing std::any holder; no element-type guessing,
replacement Array<Any>, copied vector, boxing-time length snapshot or named-property fiction is
introduced. Array<double>, String, bool, Any, reference, optional and represented active variant
cells all keep their original storage. Reads see subsequent mutations through the same array's
aliases. Rebinding the original variable leaves the already-boxed array attached to its own storage.
The `external_if<Array<T>>()` exact-type contract and shared array identity are unchanged.

Index validation delegates to Array<T>::get: negative/fractional/nonfinite/out-of-range indices
return an empty optional, and negative zero denotes zero. A present undefined or null is an engaged
Any, distinct from an absent index. Element boxing uses the existing any_from rules, preserving
object/nested-array reference identity; a present unrepresented cell throws BadAnyAccess rather
than becoming a missing/undefined value. Size still works for that array, since counting requires
no element schema. Non-array receivers throw BadAnyAccess; buffer-backed TypedArray values remain
non-Array under is_array. Neither API adds Any numeric coercion or a blanket subscript operation.

Commit-first scoped runtime build/link passed, followed by **1/1 runtime CTest pass, 0.01 s**
(0.04 s total). New tests cover exact typed recovery, fractional/signed-zero values, alias mutations,
length changes/rebinding, invalid bounds, typed heterogeneous variants and live object references,
string/bool/optional cells, nested-array identity, undefined/null/absence, rejected receivers and
unrepresented cells, and retention/release after original handles expire.

This supplies the reusable runtime boundary only. Builder2 owns the concrete Spine source/header
lowering, and builder7 owns its public consumer regression; neither is replaced by this runtime
suite. No source patch, generated SDK file, header-status row, compiler pin or previous scoped result
is changed here. Existing recorded failures above remain historical evidence; no parser, easing,
clampUnit public-header or general emitter/iterator pass follows from this runtime result alone.
No generation, full sweep or unchanged native header compile was run.

### Emitted free guard follow-up

The original runtime tests above exercised the Any member guard but missed the emitted free
`flight::is_array(Any)` spelling. Builder2's actual caller audit found that it selected array.hpp's
generic false fallback, skipping the newly available reads. A dedicated free Any overload now
forwards to the existing member tag check. The exact emitted spelling is tested for boxed numeric,
Any and String arrays, const/mutable/temporary receivers, engaged/absent/null optionals, and rejected
undefined/null/number/object/TypedArray receivers; direct Array and generic scalar behavior remain.
Committed changed runtime build/link passed, then **1/1 runtime CTest pass, 0.01 s** (0.02 s total).
No Spine source/header, array storage, coercion policy or caller qualification claim is changed here.

## G2 resolution for `spineParseHelpers.ts`: runtime API plus a derived override

The runtime half now exists (builder4): `Any::array_size()`, `Any::array_get(double)` and the free
`flight::is_array(const Any&)`. All three read the live original typed storage. The override
`skeleton2d-formats-spine-parse-helpers-erased-array` lowers this module's three length reads and four indexed
`curve` reads onto them, which is the required C++ shape this request asks the emitter to produce. Its expiry is
the emitter producing that shape itself.

The consumer proof is recorded in `docs/header-failures.md` cluster 21. The JSON-producer half named above is
still open: `spine_parse.hpp:152` passes a `flight::JsonValue` to `named_properties` (G2 cluster
`no-matching-function-b37ddf91`).

## G2 for-of over an erased array: the loop and the element storage

At Flight `a627849` / compiler `806d257`, a `for (const entry of raw)` where `raw: unknown` has been
narrowed by `Array.isArray` emitted `for (auto entry : raw)` with `raw` as `flight::Any`. `Any` has no
`begin()`, so the loop could not compile; the loop binding also had no emitted storage or type
evidence, so `entry === null` refused with `cpp-presence-test-without-absence-storage` (and
`typeof entry` refused for the same missing evidence). This is the root behind the erased-array sites
in the retained-call population: the `skeleton2d-formats` Spine JSON handlers and the DragonBones
display loops, measured at generation. `scene2d-gl` `glVelocity`'s nearby presence-test refusal is a
typed-union root, not this erased-array one.

Source patch `compiler-erased-array-iteration` lowers exactly that shape through the runtime reads
above: evaluate the iterable once, read `Any::array_size()` each step
(`%ArrayIteratorPrototype%` re-reads length on `next()`), and read each element with
`Any::array_get(index).value_or(Any(undefined))` so a hole is `undefined`. The loop binding is
recorded with the erased storage it was given and keeps the erased type evidence, so presence and
`typeof` tests answer from the value's own tag. `unknown[]`, typed arrays, Map/Set views, explicitly
typed loop variables and non-erased operands keep their existing lowering.

Measured: new `cppErasedArrayIteration.test.ts` 2 pass; `compiler-backend-cpp` typecheck exit 0;
focused `cppLengthProvenNullableArrayRead` + `cppBindingTypeInference` 8 pass; scratch CLI runs of
the two refusing shapes emit 3/3 and 2/2 modules with 0 refusals (before: 1 refused each), and the
emitted headers compile in a consumer TU against the flight-cpp runtime.

Generation was then granted and run: `npm run pkg:generate -- skeleton2d-formats -j2` moved the
package from 43/59 to 50/59 modules emitted (16 → 9 refused) and changed 13 headers. The previously
omitted `parse_spine_bones`, `parse_spine_slots`, `parse_spine_skins`, `add_spine_bone_channel` and
the DragonBones display loops now emit as the runtime array iteration, and the inventory rows of
`dragon_bones_skins_handler` and `dragon_bones_animations_handler`/`skeleton_detect` record their
progression. The scoped compile at `-j2` then passed 0/13: the erased-array first error is gone from
every changed header, and each now stops on a distinct next root — `NamedProperties`/`Record`
(DragonBones animations, skeleton_detect, parse helpers, slots), `optional<Record>` (DragonBones
parse, spine_parse), `Any`→double (section counts), the `transform_mode_2d` heritage conversion
(bones), a new `variant == Any` comparison (slots), and the next omitted callees
(`parse_dragon_bones_display`, `parse_spine_attachment`). The public-consumer compile/link/run is
therefore **not yet met on a public header**; the lowering itself is verified by the compiler tests
and the scratch generated consumer. No 96 claim: this subcause covers the erased-array for-of sites
only.

## G2 erased-array length and index reads after an Array.isArray guard

Same root as the for-of section above, one read family further: after
`if (!Array.isArray(raw)) return`, a `raw.length` emitted
`flight::named_properties(raw).get("length")` (an `Any` reaching a numeric assignment) and `raw[i]`
emitted a C++ subscript on `flight::Any`, which has none. The guard proves the receiver is an array,
and the runtime already provides the checked reads (`array_size`, `array_get`).

Source patch `compiler-erased-array-length-index` records the guard proof for the statements it
dominates (both `if (!Array.isArray(x)) <exit>` and `if (Array.isArray(x)) { ... } else <exit>`, with
return/throw/continue/break treated as exits), withdraws it when the binding is assigned, and under
it lowers `length` to `static_cast<double>(x.array_size())` and `x[i]` to
`x.array_get(i).value_or(Any(undefined))`. The proof is an implication over the condition: a
surviving disjunction is false on every operand and a surviving conjunction true on every operand, so
the DragonBones shape `if (!Array.isArray(x) || x.length === 0) return` proves the array, and the
short-circuited right operand of `||` is emitted under the same proof -- its own `length` test is the
runtime size, so an empty array takes the source's early return rather than setting a zero. Reads without the proof keep the dynamic named lookup,
which is the source's own operation on a possibly non-array object.

Measured: new `cppErasedArrayLengthIndex.test.ts` 3 pass; package typecheck exit 0; focused
`cppErasedArrayIteration`/`cppArrayLengthAssignment`/`cppLengthProvenNullableArrayRead` 7 pass
together; the emitted scratch header compiles, links and runs (live length 2, present element,
absent index → undefined). The patch is written at its own stack position: all 69 compiler patches
apply in order with this one last and the stacked compiler builds and passes its focused tests. No
committed regeneration yet: the generation slot is queued after builder2's descriptor run.
