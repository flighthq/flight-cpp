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

## G2: the join and the membership lowerings now depend on each other (2026-10-18)

The membership request landed as a carried compiler patch (`compiler-membership-test-presence`), and it
interacts with this one in the same three rows. `dragonBonesParseHelpers.ts` reads:

```ts
const t = raw !== null && typeof raw === 'object' ? (raw as Record<string, unknown>) : {};
...
if ('rotate' in t) ...
```

If this request is implemented as it specifies -- both arms emitted as `flight::NamedProperties`, preserving
the alias -- then `t` is a named-property view, and the landed membership lowering **refuses** `'rotate' in t`
by name (`cpp-membership-test-unrepresented`): `NamedProperties::has` answers own keys only, and the view does
not know the viewed object's prototype. The join alone therefore moves these rows from one compile error to a
named refusal, not to a compiling header.

The refusal is conservative rather than wrong: `rotate` is not an `Object.prototype` name, and a parsed JSON
object's prototype *is* `Object.prototype`, so own-key presence and `in` agree for this key. But the emitter
cannot prove that from the declared type: `raw` is `unknown` at this boundary, and nothing records that every
caller passes a JSON object. Joining on `Record` instead would make membership compile, at the cost of copying
every property into new storage and breaking the alias this request exists to protect.

So the two requests are one decision, not two independent fixes. The faithful resolution needs one of:
- a prototype-known evidence chain for the viewed object (`raw` proven to be a parsed JSON object), on which
  the membership lowering may answer `has(key)` for a key that is not an `Object.prototype` name; or
- a view that answers the full prototype-chain question for a known-plain owner, so `in` over it is exact with
  no per-site proof.

Neither is a per-site lowering. Recorded here so the next attempt does not implement the join alone and
report the rows as advanced when they are not.
