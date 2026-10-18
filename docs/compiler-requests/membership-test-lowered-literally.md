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

## The measurement behind the table

Compiled and run against the runtime at this pin, so the presence semantics are a result rather than a
reading of the headers:

```cpp
#include <flight/record.hpp>
#include <flight/runtime.hpp>
int main() {
  flight::Record<flight::String, flight::Any> obj;
  obj.set(flight::String("value"), flight::Any(1.0));
  obj.set(flight::String("undefinedValue"), flight::Any());  // present, value undefined
  const flight::String absent("absent");
  const flight::String present_undefined("undefinedValue");
  // has() reports existence; the Any's own test reports definedness.
  return (!obj.has(absent) && obj.has(present_undefined) &&
          !(obj.get(absent).has_value() && obj.get(absent)->has_value()) &&
          !(obj.get(present_undefined).has_value() && obj.get(present_undefined)->has_value()))
             ? 0
             : 1;
}
```

```
absent            : has=0 get-engaged=0 any-defined=0
present-undefined : has=1 get-engaged=1 any-defined=0
```

## At flight-compiler `806d257`: four receivers, three lowerings (G2)

The G1 sweep at compiler `806d257` / Flight `a627849` groups 31 failing headers as
`in-operator-not-lowered-b1eae4af`, all `expected ')'`. Only four headers contain the expression that fails there. The other 27 fail
because they include one of them; an include walk over the 31 reaches `interaction_manager` from 21,
`render_registry_guards` from 8, `explain_scene_3d_pipeline_coverage` from 1, and
`wgpu_render_state` from `render_wgpu/wgpu_shader.hpp`.

| header | source | receiver as emitted | probed key |
|---|---|---|---|
| `interaction/interaction_manager.hpp:510` | `interactionManager.ts` `hasTransform2DRuntime` | `StructuralRef<RowReadonly<RowOf<shared_ptr<NodeRuntime<…>>>>>` | `worldMatrix` |
| `render/explain_scene_3d_pipeline_coverage.hpp:44` | `explainScene3DPipelineCoverage.ts:78` | `StructuralRef<RowReadonly<RowOf<NodeAny>>>` | `materials` |
| `render/render_registry_guards.hpp:48-81` (7 sites) | `renderRegistryGuards.ts:54-80` | `flight::Ref<flight::types::RenderState>` (nominal) | `device`, `element`, `gl` |
| `render_wgpu/wgpu_render_state.hpp:259` | `wgpuRenderState.ts:245` | `std::variant<Ref<WgpuRenderStateRuntime>, StructuralRef<…WgpuDeviceState>>` | `EntityRuntimeKey` (Symbol) |

None of these receivers is a Record. The Record lowering above (`record.has(key)`) does not apply, and
neither does the bounded `respell-membership-test` repair, whose spelling is `.has()` on the receiver.
`StructuralRef` has no `.has`. `NamedProperties::has` answers own keys only, which would narrow inherited
presence; see [Interaction runtime membership](interaction-runtime-property-membership.md).

**Every probed string key is a required member on every type that declares it.** `HasTransform2D.worldMatrix`
(`Matrix | null`), `Mesh`/`InstancedMesh`/`Billboard.materials`, `WgpuRenderState.device`,
`DomRenderState.element` and `GlRenderState.gl` are all non-optional. None is an `Object.prototype` name.
So present-but-undefined cannot arise at the string-keyed sites, and a getter is never on the path.

**The Symbol key is the opposite case.** `Entity` declares `[EntityRuntimeKey]: EntityRuntime | undefined`
as a required member, and entities are routinely written as `{ [EntityRuntimeKey]: undefined, … }`. So
present-but-undefined is the normal state there. `row_has(row, Symbol)` reports the computed cell's
engagement (`symbol_present`, `structural_ref.hpp:1556`), so it would answer false for a fresh device state
that JavaScript reports as present. It must not be used for this site.

The receiver is a two-arm variant. `WgpuDeviceState extends Entity` declares the key as required, while
`WgpuRenderStateRuntime extends RenderStateRuntime extends EntityRuntime` does not declare it. The faithful
lowering is therefore the arm test `std::holds_alternative<StructuralRef<…WgpuDeviceState>>(x)`. It is
chosen because exactly one arm declares the key and declares it as required, not because of the arm's name.
If two arms declared it, or one declared it as optional, the emitter must refuse.

**Structural receivers (2 sites).** `row_has<RowKey<"k">>(row)` (`structural_ref.hpp:1564`) can be faithful
here. For a key outside the row's static schema it asks the shared owner `has_named_cell(k)`. That reports
whether the owner bound the cell, regardless of its value, and it invokes no getter. Widened rows share the
owner, so a base-typed row sees the derived object's cells. The lowering is exact under one condition: the
key is required on every declaring type, which is true at both sites.

The lowering is NOT exact for a key declared optional (`k?: T`). Absent and present-undefined are then one
disengaged `std::optional`, and `row_has` reports engagement. Where the key is inside the static schema,
`row_has` reports engagement for an optional member and true for a required one. The emitter must refuse
the optional case rather than pick either answer.

**Nominal receiver (7 sites, one header).** No membership test can be faithful here. The emitted native
`WgpuRenderState`, `GlRenderState`, `DomRenderState` and `CanvasRenderState` each derive only from
`flight::ReferenceEnabled`, not from `RenderState`, and `ReferenceEnabled` carries no member table. So a
`Ref<RenderState>` can never hold the backend state the source probes for. Any lowering of `in` that
compiles there answers false at every site, and `getRenderRegistriesMissMessage` would always return the
Canvas/GL fallback text. The source parameter is the structural interface `RenderState`. It must be
carried as a structural row over the caller's actual state owner, as the other two sites already are,
before `row_has` can answer.

**Requested lowering.** For `'k' in x`:
- A Record receiver: `x.has(k)` plus the `Object.prototype` names, as above.
- A variant receiver where exactly one arm declares `k` as required and no arm declares it optionally: test that arm.
- A structural-row receiver where `k` is required on every type declaring it: `flight::row_has<flight::RowKey<"k">>(x)`.
- An optional `k`, or a receiver carried nominally: refuse with a named reason.

Never use an own-keys predicate, a value read, a constant, or a nominal downcast chosen by name.

**Latent sites.** Eight more headers contain the same expression but fail first on an earlier diagnostic,
so G1 grouped them in other clusters. They will surface here when those clusters clear:
- `particles_formats/register_built_in_particle_formats.hpp:217` (`start`)
- `render/render_cache.hpp:131` (`cache`)
- `scene2d_formats/lottie_document.hpp:906` (`x`)
- `scene3d/scene_document.hpp:144` (`EntityRuntimeKey`)
- `scene3d_gl/gl_particle_emitter_3d.hpp:113` (`blendMode`)
- `scene3d_wgpu/wgpu_shadow_map.hpp:127` (`instanceMatrices`)
- `skeleton2d_formats/dragon_bones_parse_helpers.hpp:64-146` (`rotate`, `skew`, `curve`, `tweenEasing`)

The DragonBones keys are optional JSON members, where absent and present-undefined differ. These sites
are not classified here.

No source patch is carried. A TypeScript rewrite that erases to the same JavaScript cannot express
presence on these receivers more simply than `in` already does, and the nominal site needs a carrier
change in the emitter, not a source change.

## G2 particle 3D reaches the previously latent membership site

The runtime correction to the attribute-location number carrier clears the original list-narrowing
error from `scene3d_gl/gl_particle_emitter_3d.hpp`. Its scoped one-worker check now first reports
`expected ')'` at line 113, in `is_scene_particle_emitter_3d`. This is the particle site already listed
above, now measured as the winning diagnostic rather than inferred from a later expression.

The existing `scene3d-gl-particle-capability-guard.patch` supplies the TypeScript predicate over the
original readonly NodeAny owner: matching kind plus `'blendMode' in node` and `'data' in node`.
Emission preserves the readonly row but writes both `in` operators verbatim. The required particle
fields are not repaired by a value read, nominal cast, copied node or always-false guard. This numeric
binding change supplies no membership lowering and carries no change to that existing source patch.

Only the owned particle 3D row joins this cause; its first diagnostic and line 113 are recorded in
the product. Historical G1 totals and all unowned rows remain unchanged. The two-header particle
check is 0 pass, 2 fail; its other row reaches the separately refused 2D draw callee.

## Interim compiler patch: `compiler-membership-test-presence` (code only, not yet generated)

`source-patches/compiler-membership-test-presence.patch` carries the requested lowering through the compiler patch
stack. It has been measured on the compiler alone; no SDK package has been regenerated with it yet.

**Evidence.** The semantic lowering attaches `IrMembershipTestEvidence` to every `in` expression, computed by the
checker. It records:
- the key, either a string literal or a dynamic string;
- how the receiver's own type declares the key: required, optional, or undeclared;
- whether the key resolves only through the apparent prototype, as `toString` does on an object literal;
- whether any analyzed object type assignable to the receiver declares the key optional. A generic container
  counts as assignable, so it can only make the evidence more conservative.

**Lowering, by the carrier the receiver is held in:**

| receiver | lowering |
|---|---|
| `flight::Record`, literal key | `r.has(k)`; `true` for an `Object.prototype` name |
| `flight::Record`, dynamic string key | `r.has(k) \|\| flight::is_object_prototype_property(k)` (new runtime helper in `record.hpp`) |
| any recognized carrier, key declared **required** by the receiver | `(receiver, true)`; the value is never read |
| structural row, key undeclared, every assignable declarer required | `flight::row_has<flight::RowKey<"k">>(row)`: the owner's bound cell, no value read, no getter |
| key declared optional, or an assignable optional declarer | refused (`cpp-membership-test-unrepresented`) |
| nominal `Ref<T>` that does not declare the key | refused: the carrier cannot hold the probed owner (render_registry_guards) |
| `flight::NamedProperties` view | refused: `has` is own-keys only and the view does not know the prototype |
| closed-union arm, erased `Any`, type parameter, symbol key | unchanged: the existing lowering and its own refusal stand |

**Measured** at flight-compiler `806d257` with the 69-patch stack plus this patch:
- All 70 patches apply in sequence with plain `git apply`.
- The new `cppMembershipTest.test.ts` passes 6/6.
- The full `compiler-backend-cpp` and `compiler-semantic` suites fail on the same 78 tests with and without the
  patch: 0 new failures and 0 fixed.
- Emitted Record and row headers were compiled with g++ 15.2 against this runtime and the SDK structural member
  table, then run. A Mesh viewed through a Node row reports `materials` as present and a plain Node does not.
  For a Record, an own key is present, an absent key is not, and `toString` is.

**Activated (preparation; not yet qualified).** Commit `cc41e83a` adds the manifest entry and retires
`effects-membership-test-defaults`. It re-derives `gl_shadow_map`'s override: `isShadowInstancedMesh` is now the
generated `row_has` instead of a kind comparison. It also adds a guard: a key declared required by an
**asserted** receiver (`value as T`) is refused rather than answered `true`, because the checked recovery would
throw where the source asks the object (`render_cache` `isRenderCacheAdapter`). Scoped generation ran for
effects, interaction, render and scene3d-gl, followed by a render-only re-run for that guard. Only the
membership hunks are kept; closure-dependent foreign drift is restored. GCC 15.2 `-fsyntax-only` at `-j2` on the
current base:

| set | result |
|---|---|
| `effect_defaults.hpp`, `gl_shadow_map.hpp` | **2 pass** |
| the 32 `in-operator-not-lowered-b1eae4af` headers | **0/32 pass**; 31/32 now stop past the membership site |
| 21 interaction/gui | `interaction_manager.hpp:722` `'Traits' was not declared`, already present behind `:510` |
| 8 render-registry includers | `render_registry_guards.hpp:127` `to_string(RenderRegistryTable)`, after the nominal `device in state` refusal |
| `explain_scene_3d_pipeline_coverage` | `row_get<"materials">` on the unnarrowed NodeAny row (`static-assertion-00a0e75d`) |
| `gl_particle_emitter_3d` | `:121`, past the membership site at `:113` |
| `render_wgpu/wgpu_shader` | still `in`: the `EntityRuntimeKey` symbol key on a variant, outside this patch |

The compiler suites fail on the identical set with and without the patch on the 69-, 75- and 84-patch stacks (0
new). **Not claimed:** the `normalizeEffect` consumer passes its membership cases but throws `bad_optional_access`
for an unknown kind, at `defaults.get(kind).value()`. That is a typed **non-erased** Record missing-key read, a
separate cause; a type-only `| undefined` annotation does not change the emission. The header-status rows move
when that consumer qualifies.

**Owed at the regeneration that adopts it:**
- `repairs/emission-repairs.json` `effects-membership-test-defaults` will match nothing. The emitter then writes
  the prototype-aware Record form, which is more faithful than the repair's own-key `.has`. The repair must be
  removed in that commit.
- Overrides that copy membership sites must be re-derived (`overrides:check`).
- Each of the 32 `in-operator-not-lowered-b1eae4af` headers, and the latent sites listed above, must be
  re-measured. `interaction_manager`, `explain_scene_3d_pipeline_coverage` and `gl_particle_emitter_3d` are
  expected to lower. `render_registry_guards` is expected to become a named refusal. `wgpu_render_state` (symbol
  key on a variant) is unchanged. No header result is claimed until measured.
