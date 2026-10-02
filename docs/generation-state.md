# State of generation

What the pinned compiler can and cannot turn into a usable SDK, measured rather than estimated. This
replaces the practice of writing findings up as letters to flight-compiler: findings are recorded here
and we keep patching. `docs/upstream-flight-compiler-request.md` is the historical correspondence and is
no longer the place new work goes.

Measured at flight `7e2fc7d` / flight-compiler `839d91e`, 2026-10-02.

## Read the profiled tree, never the unbound one

`npm run sdk:generate` passes **no** binding profiles. The committed `generated/` tree is therefore the
zero-bindings build, and its `manifest.json` says `bindingProfiles: []`. Anything measured there
overstates how much is blocked, because every external symbol is missing by construction.

| | `generated/` (unbound) | `out/sdk-sdl` (profiled) |
|---|---|---|
| modules emitted | 1153 / 2904 | **1506 / 2904** |
| root refusals | 938 | **603** |
| "binding plan incomplete" | 680 | **13** |

Use `npm run sdk:generate:sdl` and read `out/sdk-sdl/manifest.json`. External binding coverage is
roughly 98% complete; it is not the blocker it appears to be in the unbound ledger.

## The three rungs

A package is only usable at the third rung, and conflating the first with the third is the most common
way this repository has overstated itself.

1. **emits** — the compiler accepted every module. 1506 of 2904.
2. **compiles** — the emitted headers are valid C++. 1474 of 1495 attempted.
3. **behaves** — the oracles agree with TypeScript semantics.

`@flighthq/statusbar` emits all three of its modules and not one of its headers compiles. Emission is
not evidence.

`npm run sdk:tiers` reports rung 1 and rung 2 together, per package: **27 of 154 packages shippable**
(26 ready, 1 assisted), 78 partial, 48 blocked, 1 deferred.

## Where the 603 root refusals actually are

Cascade is 795 of the 1398 refusals, so a root fix is worth more than one module. By family:

| count | share | family |
|------:|------:|--------|
| 265 | 44% | **union / variant discrimination** |
| 85 | 14% | structural rows |
| 64 | 11% | other (one-offs) |
| 63 | 10% | contextual type evidence |
| 55 | 9% | external bindings — **ours** |
| 33 | 5% | null versus absence |
| 26 | 4% | WeakMap |
| 12 | 2% | array profile |

### Corrections to earlier claims from this repository

Two things previously reported from here were wrong, and the record should say so.

- We said external binding coverage was 72% of the root blockage. That came from the unbound ledger.
  Profiled, it is 9%.
- We said roughly 77 modules sat behind the single asserted-row rule, naming `@flighthq/node` as the
  keystone for `scene2d`, `scene3d` and `render`. **They are not cascade from node.** `scene2d` has four
  root refusals of its own and none of them involve node; `render` has seventeen, of which exactly one
  is blocked on `@flighthq/node/contract`. Fixing node would unblock node, and little else.

## Union / variant discrimination, the dominant family

Two distinct shapes share this family, and the second is the more interesting.

**Anonymous structural types get a per-package nominal identity.** A union of object shapes lowers to a
variant of generated structs named by their members and a structural hash, while the assertion target
keeps the named alias:

```
type assertion target must identify exactly one C++ variant alternative:
  target flight::Ref<flight::types::CollisionCapsule3D>
  against [flight::Ref<flight::types::x_y_z_radius_kind_b0bc53ca3f0e8006>,
           flight::Ref<flight::types::apex_x_..._kind_21c650ad310bf539>, …]
```

The alternatives are structurally the target, under a different name, so no alternative matches it.

The same mechanism produces header compile failures, and there it is unambiguous:

```
could not convert 'joint' from 'shared_ptr<flight::skeleton3d::w_x_y_z_5137f2b28145ccce>'
                           to 'shared_ptr<flight::mesh::w_x_y_z_5137f2b28145ccce>'
```

**The hash is identical and the namespace is not.** One structural type was emitted twice as two
unrelated C++ structs, once per package that mentions it. The compiler already has the conversion for
this case -- `emitCppContextualStructuralRecordConversionCpp`, which converts member-wise where names,
optionality and emitted member types agree exactly -- but it is not reached here, and a struct emitted
per package means even an identical shape needs it.

This is not something flight-cpp can close downstream. A converting constructor between arbitrary
structurally-identical generated structs would bridge types the source program may well have intended to
keep apart, and it would do so invisibly.

## Null versus absence: `flight::Presence` exists and is unused

A member declared `T | null` emits as `std::optional<Ref<T>>` with `nullopt` standing for null.
`RowPartial` then needs the same member absent-able and there is no second channel, because `optional`
is already spent. One root cause, three faces:

- 33 of 69 root refusals in the **examples**: "a presence test against null has no absence channel in
  the emitted C++ storage for identifier"
- the dual-sentinel refusals (`cppCompilerBackend.ts:18888`)
- most of the remaining header compile failures, which are **emitted, not refused** -- `node/has_clip.hpp`
  feeds `std::optional<shared_ptr<ClipRegion>>` into
  `std::variant<shared_ptr<ClipRegion>, flight::Null, flight::Undefined>`

`include/flight/presence.hpp` has shipped `Presence<V> = std::variant<Undefined, Null, V>` since before
the pin. It appears **0 times** in the generated headers and **0 times** in `cppCompilerBackend.ts`.

We deliberately do not patch around this. A conversion from `std::optional<Ref<T>>` to that variant would
have to invent the null-versus-absent distinction the representation already destroyed, and our own tests
would then certify the invention as correct.

## The examples, and why they are at zero

`frontierEmittedModules` is **0 of 103** across all 34 example packages. Every example has exactly the
same two root refusals, in `app.ts` and in the renderer module our own gate renames to `renderNative.ts`:

- `captured referent mutation of <binding> requires a shared C++ reference representation`
- `a presence test against null has no absence channel in the emitted C++ storage for identifier`

Neither is an example-authoring mistake. The first fires because
`hasSharedReferentRepresentationCpp` requires the binding's type to plan as `represented` with a
non-`inlineValue` representation, and the bindings in question are typed by `@flighthq/scene2d`
(`createDisplayObject` returns `DisplayObject`), which emits **0 of 10** modules. A type whose declaring
package was refused has no representation to plan, so every example's top-level scene binding fails the
test. The examples are blocked on `scene2d`, which is blocked on its own four root refusals -- two of them
in the dominant union family.

So the chain to a running example is: union/variant discrimination -> `scene2d` -> every example. Not
node.

## What we do about it

Ranked by what leaves no debt. The first two are ours and we exhaust them first.

1. **Runtime accommodation** -- extend `include/flight/` so emitted code compiles. Permanent, helps every
   future emission.
2. **Binding coverage** -- `bindings/*.json`. 55 root refusals remain in our half: 13 missing symbols, 10
   external call result types, 16 unions erased by a binding target, 26 WeakMap policies.
3. **Declared workarounds** -- `source-patches/`, `repairs/emission-repairs.json`,
   `deferred-packages.json`. Each carries its justification and an expiry check; see AGENTS.md.
4. **Record it here** and keep going.

What we will not do is change absence, reference identity, equality, ordering, exception shape or task
settlement to make something compile. Those are the semantics this runtime exists to preserve, and a
workaround that alters one is a claim our own tests would certify as true.
