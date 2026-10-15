# `key in object` is emitted as `key in object`

**Family.** Contextual / operator lowering. **Sites at the pinned revisions:** 29 across 15 generated
headers. **Severity:** every one of them is a hard compile error, and the diagnostic is misleading.

## What the emitter writes

TypeScript's membership test is emitted literally:

```cpp
if (!(key_2 in entry)) {
```

`in` is not a C++ operator, so the compiler cannot parse the expression and reports the *key* as a type
name:

```
error: unknown type name 'key_2'
```

That diagnostic names the key rather than the construct, so the defect reads like a bad identifier
declaration rather than a missing lowering. It cost a chain of investigation here: the first sighting was
recorded as `unknown type name 'key_2'` in `@flighthq/effects`, and only the emitted text showed what had
actually happened.

## Where it appears

By generated header, at flight `a627849` / flight-compiler `2b687ea`:

| sites | header |
|------:|--------|
| 7 | `flight/render/render_registry_guards.hpp` |
| 5 | `flight/skeleton2d_formats/dragon_bones_parse_helpers.hpp` |
| 2 | `flight/interaction/interaction_manager.hpp` |
| 2 | `flight/lighting/light_analysis.hpp` |
| 2 | `flight/log/log.hpp` |
| 2 | `flight/scene3d_gl/gl_particle_emitter3_d.hpp` |
| 1 each | `effects/effect_defaults`, `physics3d_abi/physics3_dabi_command`, `render/explain_scene3_dpipeline_coverage`, `scene2d_formats/lottie_bezier_path`, `scene2d_formats/lottie_document`, `scene3d/scene_document`, `scene3d/scene_kind_usage`, `scene3d_gl/gl_shadow_map`, `scene3d_wgpu/wgpu_shadow_map` |

Two key shapes occur: a bare identifier (3 sites) and a string literal (26 sites), e.g.
`(key in obj)`, `(key_2 in entry)`, `(entity_runtime_key in source)`, and
`(flight::String("curve") in frame)`.

## Minimal reproduction

```ts
const obj: Record<string, unknown> = {};
for (const key of Object.keys(obj)) {
  if (!(key in obj)) continue;
}
```

emits `if (!(key in obj))`.

## What the lowering has to preserve

`key in object` asks whether the **property exists**. JavaScript answers `true` for a key whose value is
`undefined`, so a lowering through the value is wrong. The runtime already models the distinction exactly
and the emitter should name it:

- `flight::Record::has(key)` — presence, via the entry map or the attachment's `contains`.
- `flight::Record::get(key)` — an `std::optional<Any>` engaged iff the key exists, holding an `Any` that
  is undefined iff the *value* is.

Measured on the runtime (`include/flight/record.hpp`, `include/flight/any.hpp`):

| | `has` | `get` engaged | `Any` defined |
|---|---|---|---|
| absent | 0 | 0 | 0 |
| present, value `undefined` | **1** | 1 | 0 |

So the faithful lowering is `record.has(key)`, and any lowering that tests the value
(`record.get(key)->has_value()`, or `record.get(key).value_or(...)`) reports a present-undefined key as
absent.

## What we do meanwhile

`repairs/emission-repairs.json` carries one bounded `respell-membership-test` entry per site: it rewrites
`(<key> in <receiver>)` to `(<receiver>.has(<key>))`, only at receivers the entry declares and only for a
bare-identifier or string-literal key. The receiver cannot be inferred from text -- `in` appears over
Records, strings and maps alike -- so it is declared per file rather than matched, and there is deliberately
no global replacement. The entry expires when the emitter lowers a membership test to the presence
question; `sdk:check` fails if it matches no header.

The request is filed rather than patched because the fix belongs in the lowering: a repair has to be told
each receiver, and 29 sites across 15 packages is 15 declarations that should not exist.
