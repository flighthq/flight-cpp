# A named-property view and an empty record are the two arms of one ternary

## Reproduction

`flight/skeleton2d_formats/dragon_bones_parse_helpers.hpp` at `806d257`, line 61, from
`packages/skeleton2d-formats/src/dragonBonesParseHelpers.ts`:

```ts
const t = raw !== null && typeof raw === 'object' ? (raw as Record<string, unknown>) : {};
```

emitted as

```cpp
flight::Record<flight::String, flight::Any> t = ((raw is non-null && raw.type_of() == "object")
    ? flight::named_properties(raw)
    : flight::Record<flight::String, flight::Any>{});
```

The two arms have different types, so the conditional has no common type:

```
error: incompatible operand types ('NamedProperties' and 'flight::Record<flight::String, flight::Any>')
```

## Source semantics, and why neither arm may be converted

The true arm **aliases** `raw`: `raw as Record<string, unknown>` is an assertion, not a copy, and `t` is
only read afterwards (`'rotate' in t`, `t.rotate`). The false arm is a fresh empty object. So identity
matters in one arm and a fresh emptiness in the other.

`flight::NamedProperties` is already exactly the source's aliasing view: it holds the row owner, answers
`keys()`/`get()`/`has()` over the original cells and copies nothing. It is also `= default;`
default-constructible, and its default instance has no owner and reports no keys -- which is the `{}` arm's
own meaning.

## Required shape

Choose the view type for both arms. `NamedProperties` gives the true arm its identity and the false arm its
own empty state, so the conditional joins without any conversion. Materializing the view into a
`Record<String, Any>` would compile and would copy every property into new storage, breaking the alias the
source relies on; that is the one fix this request exists to rule out.

## The second cause in the same rows

These three headers then use the membership test (`'rotate' in t`), which is emitted literally as C++
`in`. That is already filed as `membership-test-lowered-literally.md`; the two causes combine, and neither
alone makes the headers compile.

## Affected

3 headers in the G1 sweep, all `skeleton2d-formats` (`dragon_bones_parse_helpers`, `_slots_handler`,
`_stub_handlers`). No runtime or override carry is possible without either the copy this request rules out
or hand-writing the parse.

## G2 the same split at a call argument: Spine JSON timeline handlers

At Flight `a627849` / compiler `806d257`, `spineJsonAnimationsHandler.ts` iterates
`Object.entries(raw as Record<string, unknown>)` and asserts each `animEntry as Record<string, unknown>`.
It then calls the registered `SpineJsonTimelineHandler`, whose third parameter is declared
`animEntry: Readonly<Record<string, unknown>>`.

The emitter lowers the assertion to the aliasing view, `flight::NamedProperties anim = flight::named_properties(anim_entry)`,
but lowers the handler type's parameter to the owning
`std::function<void(Ref<SpineJsonTimelineContext>, String, Record<String, Any>)>`. So
`handler.value()(timeline_context, name, anim)` at `skeleton2d_formats/spine_json_animations_handler.hpp:66`
has no matching call (GCC 15.2, one worker: `no match for call to '(std::function<...Record<String, Any>>) (..., flight::NamedProperties&)'`).

This is the ternary's mismatch at a function boundary. The required shape is the same: a readonly
`Record<string, unknown>` reached through an assertion is a view, and the parameter that receives it must
take that view. Materializing the record would copy every property and break the alias the handlers read
through. Nothing is carried: no materializing conversion, no handler adapter, and no narrowed animation domain.

`spine_json_section_handlers`, `spine_json_handlers` and `spine_json_full` reach this header through their
includes and were not recompiled. The next error in this header (an optional `Record` diagnostic detail at
`:76`) was noted but not analysed in this unit.
