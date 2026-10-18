# A null test on an element of a nullable-element array tests the wrong optional layer

This is a silent miscompilation: the output compiles and returns wrong answers.

`(T | null)[]` lowers to `flight::Array<std::optional<T>>`, and an index read lowers to `Array::get(i)`,
which returns `std::optional<std::optional<T>>`. The outer layer answers "is the index in range" (JavaScript
`undefined`) and the inner layer answers "is the element null". Every comparison of an indexed element
against `null` or `undefined` is emitted as `get(i).has_value()`, the outer layer only. So a `null` element
compares as present, and an out-of-range read compares as null.

## Minimal TypeScript

```ts
export function live(slots: (number | null)[], i: number): boolean {
  return slots[i] !== null;
}
```

At flight-compiler `2b687ea` with all 38 of this repository's compiler source patches applied:

```cpp
inline bool live(flight::Array<std::optional<double>> slots, double i) {
  return slots.get(i).has_value();
}
```

Run against `[null, 1]`:

| expression | JavaScript | native |
| --- | --- | --- |
| `slots[0] !== null` | `false` | `true` |
| `slots[1] !== null` | `true` | `true` |
| `slots[5] !== null` | `true` | `false` |

## Pinned boundary

All of these emit the same `slots.get(i).has_value()`, or its negation:

| source | correct C++ meaning |
| --- | --- |
| `slots[i] !== null` | out of range, or in range and the element present |
| `slots[i] === null` | in range and the element null |
| `slots[i] != null` | in range and the element present |
| `slots[i] !== undefined` | in range (the only one the current emission gets right) |
| `const slot = slots[i]; slot !== null` | same as `slots[i] !== null`; the bound local is typed `std::optional<std::optional<double>>` and tested with one `has_value()` |

The element type does not matter: `((() => void) | null)[]` emits identically.

## Required C++ shape

Test both layers as the source comparison requires, for example for `slots[i] !== null`:

```cpp
const auto element = slots.get(i);
return !element.has_value() || element->has_value();
```

## Affected Flight modules

`@flighthq/signals` at flight `a627849` uses a null slot as a tombstone while a dispatch is running:
- `compactSignalData` skips tombstones with `data.slots[read] === null`
  (`generated/include/flight/signals/slot.hpp:71`, and the copy in `safe.hpp:50`);
- `countLiveSlots`, behind `hasSignalSlots`, counts `data.slots[i] !== null` (`slot.hpp:167`).

Natively a tombstone therefore counts as a live slot and is never compacted. After a slot disconnects
during a dispatch:
- `hasSignalSlots` keeps reporting it;
- the arrays keep the dead cell;
- a signal whose last slot disconnected does not return to `data === null`.

Three upstream cases observe this. They are ported in `tests/generated_signals_test.cpp` and reported as
known defects rather than asserted. With the two tests corrected to the shape above in a scratch copy of
`slot.hpp`, all three pass and nothing else regresses.

Other SDK modules that null-test indexed elements of nullable-element arrays are expected to be affected
the same way. They were not enumerated here.

## Workaround carried

[`compiler-indexed-element-presence-test.patch`](../../source-patches/compiler-indexed-element-presence-test.patch)
adds the two-layer lane to `emitCppPresenceTestCpp`. It covers an element read through a runtime array,
written directly or retained by an immutable array-element binding. The outer layer is tested as the index
miss (`undefined`) and, when the element type stores a sentinel as optional absence, the inner layer as that
sentinel. An element type with no absence of its own has only the outer layer, so strict `null` is
type-decided. RegExpExecArray captures and typed arrays keep their own lanes.

A binding's declaration owns its storage, and two emission repairs flatten a declared single-absence binding
to one layer after emission (`signals-flatten-generic-slot-read`,
`animation-state-machine-flatten-channel-index-read`). So a binding's test dispatches at compile time on the
carrier it actually has, inside a generic lambda so that the branch not taken is never instantiated.

Verified three ways:
- against node on 90 cells: every operator above, crossed with direct reads, bound locals and null,
  undefined, callable and plain element arrays, crossed with in-range null, in-range present and
  out-of-range indices;
- by `cppIndexedElementPresence.test.ts` (11 cases, all failing without the patch);
- by the unchanged set of pre-existing backend failures.

`@flighthq/signals` is regenerated with the patch, and its three tombstone cases are asserted in
`tests/generated_signals_test.cpp`. Reverting either regenerated comparison fails them.

## G2 Rive clipping conditional binding has one optional layer

At Flight a627849 / compiler806d257, applyRiveClipping stores
`const target = owner >= 0 ? nodes[owner] : null`, where nodes is readonly (DisplayObject|null)[].
It then skips target === null || target === undefined. The current header emits:

```cpp
std::optional<flight::types::DisplayObject> target =
    ((owner >= 0.0) ? std::optional{nodes.element(owner)} : std::nullopt);
if ((!target.has_value() || !target.value().has_value())) { /* skip */ }
```

The declaration/conditional has **one** optional, and its payload DisplayObject is the existing
StructuralRef graph/traits view. It has no has_value member. The indexed presence logic has retained
a second layer that the stored binding no longer has. Actual scratch declaration-only unused-header
compilation fails at this test (candidate226, original224), before any link or native clipping call.
See the [retained-callee audit](retained-declaration-calls-omitted-callee.md#g2-rive-clipping-declaration-does-not-pass-the-unused-header).

The existing compile-time carrier dispatch must apply through this conditional/binding shape too;
adding has_value to StructuralRef would invent an absence channel on a present payload. Preserve null
versus out-of-range undefined where strict source tests distinguish them; here the disjunction skips
both and present targets must retain their original node identity. The emitted element read is also
not proof that out-of-range behavior matches the source; restoring the full indexed-read contract is
required before claiming runtime success. No shared runtime, compiler patch or text repair is carried,
and removing only one condition is not presented as a complete clipping fix. The following optional
Record error and missing transform/registration bodies remain separate obligations.

## G2 Scene3D material calls must receive the guarded inner element

At Flight `a627849` / compiler `806d257`, no-viable-conversion-eabaf455 contains one
header, scene3d/scene_material.hpp:94, retaining **0 pass / 1 fail**. The existing
scene3d-material-value-enumeration source patch already propagates Material3DValue through
collection output, helper results and named lookup. Its seventeen arms include sixteen concrete
built-ins plus the open Material3D vendor arm; the old base MaterialValue name is not this error.
The retained array read and call are, with Value standing for that exact union:

```cpp
// materials is optional<Array<optional<Value>>>:
std::optional<std::optional<Value>> material = materials.value().get(i);
if (/* source material !== null */ && !out.includes(material.value())) {
  out.push(material.value());
}
```

material.value() is still optional<Value>, while includes/push require Value. The same carrier
reaches the name visitor at line118. These are no-viable-conversion/member visitation barriers,
not missing material declarations or an unrepresented concrete union. The already-carried
indexed-element-presence patch emits the two-layer strict-null predicate correctly; its fix
must remain. This follow-up needs the predicate's proven element state to survive into argument
and member projection, with the original active material owner and union arm preserved.

The outer layer is an indexed undefined/miss and the inner layer is source null. Strict
material!==null is true for a miss, unlike a loose nullish test; it does not itself prove both
layers present. Valid present material elements must project the inner Value, while indexed
undefined/error behavior must remain faithful to the source. Do not rewrite the guard to skip
all absence, discard null slots before indexing, assume an unchecked double-value() is always
safe, coerce optional to a default material, or add permissive Array overloads that hide the
wrong carrier. Source-level non-null spelling alone does not establish that full contract.

collectNodeMaterials reads the original optional materials capability of the same readonly
node, returning on a null/absent array. It scans the live array in source index order, skips
source null slots, and appends to the caller's EXISTING out without clearing it. Duplicate
suppression is strict object/reference identity against that seeded output; equal fields,
kind or name do not merge distinct objects, and a shared material across meshes appears once.
Preserve the actual material array, each concrete/vendor material and caller array, not copied
records, closed built-in-only families, sorted output, a detached set or a new result array.
A bare group needs absent-material behavior; no geometry/kind/visibility/enabled gate is added.

getScene3DMaterials collects the root first and then every descendant in depth-first pre-order.
findScene3DMaterialByName checks the root before walking descendants, scans each node's slots
in order, compares names strictly/case-sensitively and returns the SAME first matching object,
including a named root material. Null/no-match returns null, without continuing after a match.
Preserve live graph/runtime/child arrays, traversal callback/error order and later material-name
mutation visibility. No fake material, copied node/runtime, empty collection or narrowed graph,
name or material domain is faithful. The [material-value request](material3d-concrete-value-path.md)
explains why the vendor arm and concrete ownership remain mandatory.

Later issues are inspected, not new measured first errors. Root/visitor helpers mix Node3D /
NodeOf<Node3DTraits> with explicitly defaulted NodeTraits traversal callbacks; the existing
[heritage-aware deduction request](heritage-argument-generic-deduction.md) records the needed
trait substitution, and a global alias or copied node is not a fix. Named lookup's callback
rebinds found_capture, yet its final emitted return evaluates read_binding only for void and
returns literal null. The callback-updated value must be returned; otherwise descendant lookup
would lose a found object even after compilation. Neither these issues nor the original optional
materials owner view are proved resolved by projecting the first guarded element.

Existing source tests specify root inclusion, nested first-match lookup, null/anonymous slots,
shared-material deduplication and empty mesh-free collection. They were inspected as requirements,
not executed as native discovery evidence. This arc changes only the sole failed row/root,
worklist142 and this existing request appendix. C++, runtime, source/patches, repairs, overrides,
manifests, counts/refusals/init/policy and pins remain unchanged; no native or generation rerun.
No material enumeration/name lookup, ownership, traversal or native success claim is supplied.

## The narrowing after the presence test (G2 cluster `missing-member-00139043`)

The presence test is only half of the two-layer read. At Flight `a627849` / compiler `806d257`,
`spineBinaryBoneTimelineHandler.ts` binds `const points = timeline.curves[i]` where `curves` is
`(number[] | null)[]`, tests `points === null`, and then reads `points.length` and `points[v * 4]`.
The emitter now tests the *inner* optional correctly, but the bound local keeps both layers
(`std::optional<std::optional<flight::Array<double>>>`) and the guarded length/index reads unwrap
one layer, leaving `std::optional<flight::Array<double>>` where `size()`/`element()` are called.

The guard excludes the inner null element, while an unchecked indexed undefined remains reachable.
The faithful member reads select the same live array when present and throw TypeError at the original
operation for that remaining undefined, without copying or flattening the local's carrier. A computed
key must run before the missing-receiver error; method arguments must run after the receiver check.
This is the narrowing counterpart of the presence lowering above, and the same rule applies to
every guarded read of a doubly optional element. Measured in the scoped `-j2` survey after the
nullable-element lift landed: 8 Spine binary handlers fail on this first diagnostic, one bound
local each, all pointing at `spine_binary_bone_timeline_handler.hpp:88`. Recorded as
`missing-member-00139043` in [`generated/header-status.json`](../../generated/header-status.json).

The bounded `compiler-guarded-indexed-nullable-member` correction applies at member operations on immutable indexed locals with a single optional element representation and the existing present-payload proof. The declaration and null/undefined comparisons retain both layers. A generic checked receiver adapts to the actual stored carrier; it preserves the original array storage, object owner and live cells. Computed indexing caches its base and key before checking the receiver, including a key's exact thrown owner. Method lookup checks the receiver before arguments. The ordinary binding and defaulted-parameter projections remain on their established paths.

Scratch qualification reproduces the old native optional<Array>.size failure and passes 15 emitted native checkpoint groups and 71 comparisons with the unchanged TypeScript fixture. Controls include null/undefined states, negative/out-of-range/fractional/NaN/Infinity indices, both guards, nullable undefined elements, ordinary single-layer reads, String/array/object payloads, shared ownership, live mutation and delayed captured reads. Two focused length-proven test failures are identical on the saved baseline; the final required-member predicate and broader prototype both have identical outcomes to the saved baseline on 328 existing backend tests (305 pass, 23 fail, 1236 unselected each). Original optional-chain operations are excluded, with five optional/defaulted emissions byte-identical to baseline. The final declared 96-patch stack applies and builds, reproduces the owned helper, dispatch and full fixture exactly, and passes the two native fixture tests. The fixture IR is byte-identical across the 92-to-96 frontend advance. The prepared public consumer includes the complete original header and calls buildSpineBinarySegmentEasings, using 11 untouched-source cases, 42 numeric samples and the original diagnostic callee. Its normal scoped output, actual execution and eight affected header checks remain pending.
