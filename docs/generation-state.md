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

## Which refused packages block the most others

Cascade is 795 of 1398 refusals, so the question that matters is not "how many modules does this package
have" but "how many other packages wait on it". Counting distinct packages blocked by each refused
cross-package dependency:

| packages blocked | refused dependency |
|-----------------:|--------------------|
| 27 | `@flighthq/types/contract` |
| **19** | **`@flighthq/log/contract`** |
| 9 | `@flighthq/registry/contract` |
| 9 | `@flighthq/node/contract` |
| 5 | `@flighthq/path/contract` |
| 5 | `@flighthq/importdiagnostics/contract` |
| 3 | `@flighthq/render/contract` |

`@flighthq/log` is the striking one: it blocks nineteen packages and has exactly **one** root refusal.

### What blocks `log`, and why we have not fixed it yet

`packages/log/src/log.ts:568`, in `serializeLogError`:

```ts
if (value.stack !== undefined) result.stack = value.stack;
if (value.cause !== undefined) result.cause = serializeLogError(value.cause);
```

`flight::Error` has **neither member**. `include/flight/error.hpp` declares only `message()` and a
static `name()`. So this is a genuine runtime gap on our side, not a compiler limitation — which makes
it the most valuable thing in our own lane that is still open.

Two things stand in the way of simply adding them, and both are real:

**`cause` needs `Any`, and `any.hpp` already includes `error.hpp`.** The include is not incidental:
`any.hpp` constructs and throws `TypeError` in three inline bodies, which needs the complete type. So
adding `std::optional<Any> cause` to `Error` closes a cycle. Resolving it means a layering decision —
type-erasing the cause behind a handle `error.hpp` can declare, or moving the throwing helpers out of
`any.hpp` — not a local edit.

**We do not know the member shape the emitter expects.** `flight::Error` appears in the emitted tree
only as `throw flight::Error(flight::String("…"))`; no emitted code ever reads a member of one. So
there is no precedent showing whether a read lowers to `stack`, `stack()`, or something else, and
guessing costs a twelve-minute generation run per guess.

A source patch that drops the two reads was considered and rejected. It would be truthful about *this*
target — flight-cpp captures no stack and has no `cause` — but `serializeLogError` exists to carry
exactly that information, and silently returning less of it is a behavior change in a diagnostics path
rather than a lowering workaround.

### A rejected hypothesis, recorded so it is not retried

`@flighthq/power` has one root refusal, `missing required call argument at position 2` in
`attachPower`. `emitSignal` is declared

```ts
export function emitSignal<T extends (...args: any[]) => void>(signal: Signal<T>, ...args: Parameters<T>): void
```

and the failing calls pass no rest arguments. The obvious comparison is `@flighthq/keyboard`, which is
already shippable and calls `emitSignal(keyboard.onHide)` with no rest arguments at all. The difference
looked decisive: `keyboard.onHide` is `Signal<() => void>`, while power's signals are
`Signal<() => void> | null` and every call site passes a local narrowed by a null check. So the
hypothesis was that `T` does not resolve through the narrowing, leaving the variadic pack planned as one
required argument.

That predicted a fix: route the checked value through a **non-optional parameter**, the one narrowing
form the emitter does resolve.

```ts
function emitVoidSignal(signal: Signal<() => void>): void { emitSignal(signal); }
```

**It did not work.** With the helper in place the refusal is unchanged, at the same site. The patch was
removed rather than carried — `ineffectivePatches` reported it on the very run that produced it, which
is what that check exists for. Whatever defeats the pack resolution here, passing through a
non-nullable parameter does not restore it, and `@flighthq/signals`' own root refusal -- "dependent
callable parameter pack args may only be used as a terminal …" -- is the same limitation seen from the
declaring side. Closing this needs the declaration handled, not the call site.

## The single-root list, and why it stops where it does

Forty packages have exactly one root refusal, with the rest of their refusals pure cascade on it. That
makes each one a candidate to complete off a single fix, and `@flighthq/screen` proved the pattern: one
source patch took it from 0/3 blocked to 3/3 shippable. The rest of the list was worked through, and the
remainder divides cleanly into three kinds, none of which is a call-site rewrite.

**Needs a compiler change.** `@flighthq/adjustments` (19/22) refuses on
`colorLutRunSignature(run, size)` where caller and callee declare the *identical* parameter type,
`ReadonlyArray<Readonly<{ kind: string }>>`, and the callee's whole body is `JSON.stringify(run)`. There
is no mismatch to align: the refusal is about representing a readonly array of anonymous structural
objects as an owning array of nominal references. `@flighthq/geometry` (27/30) and `@flighthq/easing`
(19/23) are the same shape, and both additionally cascade on `@flighthq/log`.

**Needs runtime API we would be designing speculatively.** `@flighthq/input` refuses on
`export type InputIngressSource = object` — a bare opaque handle with no shape, so no representation can
be chosen. A binding would close it, but only by pointing at a flight-cpp opaque type that does not
exist. `include/flight/host_sdl/sdk_window.hpp` declares `InputTargetHandle` inside
`namespace flight::types`, which means it is generated from Flight's own type rather than a handle we
own. `@flighthq/log`'s `Error.stack`/`Error.cause` is the same category, and the most valuable instance.

**Needs a product decision, not an engineering one.** `@flighthq/textshaper-canvas` refuses on missing
`OffscreenCanvas[value]` and `OffscreenCanvasRenderingContext2D[type]`. The *type* is already bound, to
`flight::host_sdl::ImageSource`; what is missing is the constructor and the 2D context, and the SDL host
has neither a canvas nor text measurement. There is nothing honest to bind them to. This package is not
broken, it is **not applicable to this profile** — and the 154-package denominator currently counts
web-only packages that will never ship on an SDL host. Separating "blocked by a defect" from "not
applicable here" would make the shippable fraction mean something, but which packages are out of scope
for a profile is a decision for the project, not something to infer from a refusal.

## Two experiments in our own lane, one won and one lost

### Won: external object field contracts for the WGPU profile

`bindings/sdl-wgpu.json` bound 61 types but declared no `objectConstruction` for any of them, so every
object literal the WGPU surface builds was refused:

```
external object GPUCopyExternalImageSourceInfo construction requires an exact field contract
rule: cpp-external-object-field-contract-missing
```

The binding format carries the contract as `objectConstruction: { kind: 'field-assignment', fields: [
{ sourceField, targetName } ] }`, where `sourceField` is the TypeScript dictionary key and `targetName`
the C++ member it assigns. Contracts are now declared for all **37** bound types that resolve to a plain
`struct … final` in `include/flight/host_sdl/wgpu.hpp`, with the field lists derived from those structs
rather than hand-written, and the camelCase-to-snake_case mapping applied per field.

The result: the fixture emits, and **the emitted header compiles clean** (`g++ -std=c++20
-fsyntax-only`), where before the compiler refused outright. That is the capability gain, and it is
verified rather than assumed.

`npm run sdl-wgpu:oracle` is still red, for a reason worth stating precisely rather than papering over.
The oracle asserts on emitted text and expects **designated-initializer** construction:

```
.src_factor = src_factor, .dst_factor = dst_factor, .operation = flight::String("add")
```

The compiler instead emits default-construct-then-assign inside an immediately-invoked lambda. Both are
valid C++ and reach the same final field values — but `field-assignment` is the *only* kind the contract
type admits (`kind: 'field-assignment'` is the whole union), so no `objectConstruction` declaration can
ever produce designated initializers. The oracle's thirteen designated-initializer expectations describe
these dictionaries being emitted through the **record** path, which does emit designated initializers,
not through the external-object path.

That is a modelling question — whether these WGPU dictionaries should be `ownership: value` externals at
all, or plain records — and it is not resolved by editing the oracle to accept what the compiler
currently does. The oracle stays red and says what it wants.

### Lost: `Error.stack` is not reachable from the runtime side

`@flighthq/log` blocks nineteen packages on one refusal, `value.stack !== undefined`. Since emitted code
reads an object property as a plain C++ data member — every `.message` read in the generated tree is a
member on a record struct, never an accessor call — the hypothesis was that `flight::Error` simply needs
a readable `stack` member.

A `std::optional<String> stack` member was added and the SDK regenerated. **The refusal is byte-for-byte
unchanged**, still `a presence test against undefined has no absence channel in the emitted C++ storage
for property` at `log.ts:568`. The compiler plans the property from the TypeScript declaration and never
consults what the bound C++ type offers, so no member we add changes the outcome. The probe member was
removed rather than left in place as decoration, which is what its own comment promised.

`log` is therefore **not** reachable from the runtime side, and the `cause`/`any.hpp` include cycle is
moot. This is the single highest-leverage blocker in the corpus and it is closed to us.

### The field contracts cleared every refusal of their kind, and uncovered two more defects

After declaring `objectConstruction` on 37 WGPU bindings and on `AudioBufferOptions`, field-contract
refusals across the whole profiled SDK went **10 to 0**, root refusals 603 to 599, and emitted modules
1509 to 1510. The affected packages (`effects-wgpu`, `render-wgpu`, `scene2d-wgpu`, `scene3d-wgpu`,
`audio`) each have other refusals, so no package reached the shippable tier from this — real root-cause
progress with no tier movement, which is worth stating plainly rather than rounding up.

Unblocking `@flighthq/audio`'s `audioResourceFrom` module then revealed two defects the refusal had been
masking, neither related to field assignment:

```cpp
// blob.type || undefined, lowered as a String-returning lambda that returns nullopt
([&]() -> flight::String { auto logical_or_value = blob.type;
  if (flight::to_boolean(logical_or_value)) return logical_or_value; return std::nullopt; }())

// and TypeScript's operator emitted verbatim into C++
if (!(response->body instanceof flight::ArrayBuffer)) {
```

The second is the notable one: `instanceof` reaches the output as source text, so the header can never
compile. It appears in exactly two files at this pin — `flight/audio/audio_resource_from.hpp` and
`flight/loader/load.hpp` — which is also why `@flighthq/loader` sits at 1/4.

This is the expected shape of progress at this stage: closing a refusal does not produce a working
package, it produces the next honest error. The header count moved 21 to 22 failures for exactly this
reason, while three timeline failures were fixed, so the net is 24 to 22 against the earlier baseline.

## A note on editing the binding profiles

`bindings/*.json` keeps one binding per line in a compact object form. Rewriting a profile with
`JSON.stringify(value, null, 2)` preserves the content and destroys the formatting: the first attempt at
the WGPU contracts produced a 1514-insertion diff for 37 real changes, which is unreviewable and hides
whether a key was dropped. Insert into the existing line instead and the same change is 37 lines. Verify
both the top-level key set and the binding count after any programmatic edit — a previous incident in
this repository silently deleted `identity` and `profile` from a profile, and a formatting-only
comparison could not see it.

## Profile applicability: four packages are not work this host owes

The 154-package denominator counted packages that can never ship on an SDL host, which made the
shippable fraction unreachable by construction. Four are now declared `kind: "not-applicable"` in
`deferred-packages.json` and excluded from it:

| package | its own description |
|---|---|
| `@flighthq/scene2d-canvas` | Canvas 2D renderer implementation |
| `@flighthq/scene2d-dom` | DOM renderer implementation |
| `@flighthq/effects-canvas` | Canvas 2D recipes for render effects |
| `@flighthq/textshaper-canvas` | Canvas 2D text-shaper backend: advances-only shaping via measureText |

Each reaches for `document.createElement`, `HTMLCanvasElement` and `CanvasRenderingContext2D` directly,
and for each the profile already uses a different sibling backend: `scene2d-gl` / `scene2d-wgpu`,
`effects-gl` / `effects-wgpu`, and `textshaper`. There is nothing on an SDL host for these to bind to.

**The set is derived, not listed.** Flight declares each package's host environment as
`flight.environment` in its own `package.json`; generation records it per package in the manifest, and a
package whose declared environment is not in `applicableEnvironments` (empty for this native profile) is
excluded. Exactly four of the 154 inventory packages declare `environment: "web"`, and they are the four
above. `host-web`, `host-electron`, `host-tauri`, `host-capacitor`, `tool-capture` and `tool-registry`
declare non-native environments too but are not in `@flighthq/sdk`'s dependency closure, so they never
appear in the inventory.

Deriving it matters because the first version of this was a hand-written list, and a list has to be
remembered. A package that gains or loses the declaration upstream is now reclassified with no edit here.

The two directions of error are both real, which is why the declaration is the only acceptable source:

- `@flighthq/webcam` matches a name filter for web packages and is **already shippable**. Deferring by
  name would have parked working code.
- `@flighthq/render-wgpu` has web-only symbols in its refusals (`HTMLImageElement[value]`,
  `ImageBitmap[value]`) but is the WGPU renderer this profile wants; those refusals are about
  `copyExternalImageToTexture` sources, which on SDL come from `ImageSource`. Deferring on the presence
  of a web symbol would have parked a renderer we need.

`@flighthq/statusbar` keeps `kind: "defect"`: it should work here and does not, so it stays in the
denominator as debt.

**28 of 150 applicable packages are shippable**, with 4 of 154 not applicable. The `refusedDeferrals`
check was run over all 1511 emitted headers and reports zero: no required header includes any deferred
package, so nothing is being hidden by this.

## WeakMap: the weak-key policy is only consulted for ambient types

26 root refusals are WeakMap-related — 20 on the key, 6 on the value — and the key half concentrates
hard. Counting key types across the refused modules:

```
16  GlContext            2  CollisionTriangleMesh3D   1  GlRenderState
 4  Node2D               2  GlLitProgram              1  Physics3DWorld
 2  CollisionHeightfield3D   2  TextureAtlas          1  GlFullscreenProgram  …
```

`GlContext` is the dominant one, and it looked like a one-line binding fix. `flight::host_sdl::WebGl2Context`
already has a declared `weakKeyPolicyTargetName` (`WebGl2ContextWeakPolicy`), the profiled tree emits
`using GlContext = flight::host_sdl::WebGl2Context;`, and the policy is looked up by source name — so
adding a `GlContext` type binding carrying the same policy should have closed sixteen refusals.

**It changed nothing.** Emitted modules, root refusals and WeakMap key refusals were identical to three
figures before and after (1510 / 599 / 20), and the `GlContext` alias still emits with the entry removed,
so nothing depended on it. The entry was deleted rather than carried.

The reason is `getWeakMapKeyRepresentationCpp` at `cppCompilerBackend.ts:8526`. The external weak-key
policy is consulted on exactly one branch:

```ts
if (type.kind === 'named' && type.reference.kind === 'ambient' && type.typeArguments.length === 0) {
  const weakKeyPolicyTargetName = getCompilerExternalBindingWeakKeyPolicyTargetCpp(…);
```

`reference.kind === 'ambient'` is the gate. `GlContext` is declared in Flight's own source —
`export interface GlContext extends Pick<WebGL2RenderingContext, GlContextMember> {}` — so its reference
kind is `binding`, not `ambient`, and the policy branch is never reached however the binding is written.
The fallbacks do not rescue it either: the representation plan does not report `flightReference` for a
type a binding maps to an external shared value, and `resolveAlias` returns nothing because an interface
is not an alias.

So a Flight-owned interface that a binding maps to an external C++ type with a declared weak-key policy
**cannot be a WeakMap key**, and no binding we write changes that. The remaining key types are Flight's
own generated types, where `Node2D` is a structural row — a row view has no stable referent identity to
weaken — and the 6 value-side refusals are a separate gap: `WeakMap<AppLifecycle, Record<string, unknown>>`
needs an erased record as a weak value.

This matters more than its count, because the project's position is that the SDK will always contain
WeakMaps. The whole family is closed to the binding lane.
