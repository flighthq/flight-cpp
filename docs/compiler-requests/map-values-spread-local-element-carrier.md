# An inferred Map-values spread local must retain its typed elements

At Flight `a627849` / compiler `806d257`, the retained `createSwfScale9ShapeNode` body in
`packages/swf/src/swfDefineShapeHandler.ts` creates a fresh array of known placement owners:

```ts
if (sprite.frames.length !== 1 || sprite.actions.size > 0) return null;
const placements = [...sprite.frames[0].values()];
if (placements.length !== 1) return null;
const inner = placements[0];
```

`SwfTimeline.frames` is declared as `ReadonlyMap<number, SwfTagPlacement>[]` in
`packages/types/src/SwfTagParseState.ts:83`. Its emitted field keeps that evidence as
`Array<Map<double, Ref<SwfTagPlacement>>>`. The spread's emitted lambda also selects that exact
element carrier, but its inferred destination local loses it:

```cpp
flight::Array<flight::Any> placements = ([&]() {
  flight::Array<flight::Ref<flight::types::SwfTagPlacement>> array_spread_result;
  {
    auto&& array_spread_collection =
        flight::row_get<flight::RowKey<"frames">>(sprite).element(0.0);
    for (const auto& [array_spread_key, array_spread_mapped_value] : array_spread_collection) {
      static_cast<void>(array_spread_key);
      array_spread_result.push(array_spread_mapped_value);
    }
  }
  return array_spread_result;
}());
```

Clang 22.1.8 rejects the initialization at `flight/swf/swf_define_shape_handler.hpp:115`:
`no viable conversion from 'Array<flight::Ref<flight::types::SwfTagPlacement>>' to 'Array<flight::Any>'`.
This is observed on the committed SDK after the native `copy_shape_commands` readonly source
binding was repaired. The original Shape binding diagnostic is gone; the header still fails.

The required local carrier is the lambda's existing
`flight::Array<flight::Ref<flight::types::SwfTagPlacement>>`, or an equivalent inferred C++ type.
The following indexed read must retain the same placement reference. The source spread creates
fresh array storage in Map insertion order while retaining each placement owner. The one-frame,
one-placement and identity-matrix guards must keep their existing behavior.

No unknown input needs recovery here. This differs from
[erased-array-element-reading.md](erased-array-element-reading.md), which starts with an actual
`unknown` array. It also differs from
[new-set-from-collection-iterator.md](new-set-from-collection-iterator.md), whose carried fix
handles an optional constructor argument rather than a typed spread result assigned to an erased
local. Adding general implicit Array covariance, copying placement objects, or recovering arbitrary
`Any` values as placements would hide the lost element evidence instead of preserving it.

No compiler fix or source patch is carried for this follow-on. An explicit TypeScript local type
is a possible source-spelling candidate, but has not been generated or validated at these pins;
this request makes no claim that it clears the retained body or its other masked diagnostics.
The existing authored-bounds declaration remains declaration-only and does not supply that function.

The completed scoped G2 check recompiles only `flight/swf/swf_define_shape_handler.hpp` and
`flight/swf/swf_shape_tag_family.hpp` at `-j1`: **0/2 pass, 2 fail, 0 unattempted**, both at this
same local initialization. The readonly Shape copy semantic/owner suite passes (1/1), and override
integrity passes. The two product rows remain failing and link this request; G1 sweep totals and
all other header rows are unchanged. No whole-SDK sweep or unchanged authored-bounds link rerun
was performed.
