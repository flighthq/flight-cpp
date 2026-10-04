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

## `callResultType` is overloaded between two emission lanes that want different values

Ten root refusals read `external call result type <T> is not one represented contextual runtime domain`
— eight of them `std::optional<flight::host_sdl::WebGlUniformLocation>` in `@flighthq/scene3d-gl`, two
`flight::host::TimerHandle` in `@flighthq/gui`.

`cppCompilerBackend.ts:11703` shows why. An external call result reaching a nullable context takes one
of two paths:

```ts
if (externalPresence?.immutable && plan.kind === 'optionalSingle' && absentMembers.length === 1 && …) {
  // "callResultType already names the complete carrier" -- emit the call directly
  return emitExpression(expression, context, undefined, false);
}
const targetSlots = plan.valueSlots.filter((slot) => slot.targetType === externalValueTarget);
if (targetSlots.length !== 1 && !unresolvedTargetSlot) { emissionError(…); }
```

`externalPresence.immutable` is only recorded for a call result bound to a non-mutable **variable**. The
`scene3d-gl` sites bind nothing — `gl.getUniformLocation(program, 'u_alphaCutoff')` flows straight into
an object-literal field — so there is no presence record, the carrier path is skipped, and the fallback
compares the declared `callResultType` against the union's value **slot** target.

Those two paths want different spellings of the same field:

| where the result goes | path | wants `callResultType` |
|---|---|---|
| `const loc = gl.getUniformLocation(…)` | carrier | `std::optional<WebGlUniformLocation>` |
| `{ locAlphaCutoff: gl.getUniformLocation(…) }` | slot | `WebGlUniformLocation` |

Measured, not reasoned: changing that one member to the present type took external-call-result refusals
from **10 to 2** and root refusals from 599 to 592 — and **regressed `sdl-gl:oracle` from one error to
two**, with the oracle's own `const`-bound site now failing

```
conversion from 'std::optional<WebGlHandle<WebGlUniformLocationTag>>' to non-scalar type
'flight::host_sdl::WebGlUniformLocation' requested
```

So one declaration cannot satisfy both lanes, and the change was **reverted**. Seven root refusals in a
package sitting at 3/68 are not worth regressing a semantic conformance gate, and trading a gate for a
refusal count in our own favour is exactly the move this repository should not make. The oracle is back
at one error.

The binding contract would need to carry the carrier type and the slot type separately — or the
`immutable` presence record would need to cover a call result consumed directly in a union context — for
both to work. Until then these ten refusals stay, and the declaration stays spelled for the carrier lane,
which is the one the conformance oracle exercises.

## `--best-effort` is the right mechanism and is currently unreachable

flight-compiler gained a best-effort mode. Read against this repository's problem it is close to exactly
what is needed, and `scripts/sdkGeneration.mjs` now accepts `--best-effort` and passes `bestEffort: true`
through to `compileTypeScriptPackageGraph`, ready for the first revision that can run it.

What the contract offers (`compilerPackageCompilationContract.ts`, schema `flight-compiler-best-effort/1`),
per module:

| status | meaning |
|---|---|
| `emitted` | real output |
| `dependency-incomplete` | **the module emitted its own real output**; only an inherited refusal keeps it out of a dependency-closed set |
| `refused-placeholder` | refused for its own reasons; the file at `path` is a replaceable stub |

Two further fields matter more than the statuses for a repository that intends to hand-finish the output:

- `consumers` — every module importing this one. The contract says it directly: "A hand-written
  replacement has to satisfy these callers."
- `declarationFingerprints` — present only while a module is a placeholder, and documented as "how an
  overlay notices that the module it replaces has moved underneath it" between pins.

That is drift detection for hand-written overlays across a pin move, which is the hardest part of the
freeze-and-patch plan, supplied by the compiler rather than invented here.

### The arithmetic, if it ran

At this pin, 1394 modules do not emit: **599 root refusals and 795 cascade**. The cascade set is precisely
what `dependency-incomplete` describes, so a best-effort run should turn those 795 into real output and
leave the 599 as stubs. Emission would go from 1510 of 2904 (52%) to roughly 2305 (79%), and the
hand-written burden from 1394 whole modules to about 599 stubs that arrive with their consumers and
fingerprints attached.

### Why it cannot be used yet

The feature post-dates the only revision that generates. `839d91e` contains **zero** occurrences of
`bestEffort`; it predates the mode entirely. And `d4287b4`, the current tip, **hangs like its three
predecessors** — terminated at 1500s with no output, the fourth consecutive tip to do so:

| revision | result |
|---|---|
| `839d91e` | completes; 5m56s unbound, ~12m profiled. No best-effort. |
| `ec8da2a` | no completion in 40 minutes, twice |
| `2ac16f7` | no completion, killed at `real 25m0.026s` |
| `7de41a3` | no completion, exit 124 at 900s |
| `d4287b4` | no completion, SIGTERM at 1500s. **Has best-effort.** |

The hang was profiled at `2ac16f7` to `lowerTypeScriptTypeNodeEvidence` recursing into itself
(`typeScriptSemanticLowering.js:4965`) with no progress output, over a type graph Flight makes mutually
recursive by design. Whether `d4287b4` hangs in the same place is unconfirmed.

So the single blocking issue for the freeze-and-patch plan is now the generation hang, not the emission
coverage. Nothing in this repository can raise coverage past 52% while the only usable compiler predates
the mode that would raise it.

### One experiment worth running next (answered below — see "The generation hang, bisected to one package")

The hang is observed on the full 2904-module graph. It is not known whether it is unbounded recursion or
merely superlinear, and the discriminator is cheap in principle: run `d4287b4` over a single small package
plus its closure instead of the whole SDK. If a small graph completes, the mode is usable package by
package today — which is the granularity this repository wants anyway — and the hang becomes a scaling
problem rather than a wall. `sdkGeneration.mjs` has no package filter, so this needs one.

## The foundation slice, package by package

Generated with `--package=@flighthq/entity --package=@flighthq/node --package=@flighthq/math
--package=@flighthq/geometry --best-effort` at compiler `2b687ea`. Closure: 10 packages, 1143 modules,
3m47s. Emission is near-total — the old pin's 52% is not the number to plan against:

| package | modules | headers compile |
|---|---|---|
| `@flighthq/entity` | 10 / 10 | **all** |
| `@flighthq/math` | 17 / 17 | **all** |
| `@flighthq/color` | 11 / 11 | **all** |
| `@flighthq/types` | 995 / 995 | 5 fail |
| `@flighthq/adjustments` | 22 / 22 | 3 fail |
| `@flighthq/geometry` | 30 / 30 | 3 fail |
| `@flighthq/signals` | 8 / 10 | 5 fail |
| `@flighthq/log` | 2 / 3 | 3 fail |
| `@flighthq/node` | 13 / 21 | 15 fail |
| `@flighthq/materials` | 16 / 24 | 20 fail |

Across the slice: 1107 `emitted`, 17 `dependency-incomplete` (real output), 19 `refused-placeholder`.
**17 of the 19 placeholders are classified `source-portability` by the compiler itself** — it is telling
us these are source shapes it cannot port, not holes in its own lowering, which is what makes source
patching the right tool rather than a workaround.

### An automated include repair that did not work, and why

The `types`, `signals`, `geometry` and `log` failures all read like a missing include: a symbol named but
not in scope, where the symbol is defined in a sibling header. A repair was built to insert the defining
header, driven by the compiler rather than by scanning names, and corrected once after the first version
inserted into the translation unit rather than the file the diagnostic pointed at.

**It fixed zero headers, twice, and was deleted.** The two shapes it targeted are not missing includes:

- **Missing namespace qualification.** `flight/log/log.hpp:59` says `LogSink` inside
  `namespace flight::log`, where the type is `flight::types::LogSink`. gcc names the fix in the
  diagnostic: *"did you mean 'flight::types::LogSink'?"* The include was already present and irrelevant;
  what is needed is a using-declaration or a qualified name.
- **Mutually dependent aliases.** `flight/types/dom_render_state.hpp:103` names
  `flight::types::DomTextureResolver`, defined in `dom_texture_resolver.hpp` — which includes
  `dom_render_state.hpp` back. With `#pragma once` one of the two always sees the other incomplete, and a
  `using` alias cannot be forward declared, so there is no include order that resolves it. This one is
  emitter-side: the alias has to be emitted before its use, or declarations split from definitions.

Worth recording because both shapes *look* like an include problem in the diagnostic text, and the
measurement is what distinguishes them.

### The two foundation wins actually available

- **`@flighthq/adjustments`** (22/22 emitted, 3 headers) fails on
  `Json::stringify(SequenceView<StructuralRef<RowReadonly<RowOf<…>>>>)`, and `include/flight/json.hpp`
  declares only `stringify(const Value&)`. Serializing a sequence of structural rows is a genuine runtime
  capability this SDK needs, it is permanent, and it owes nothing to the compiler.
- **`@flighthq/geometry`** (30/30 emitted, 3 headers) fails only inside `flight/log/log.hpp`, which is a
  **placeholder stub**. A stub is meant to be replaced, so hand-finishing that one file is the intended
  workflow rather than a workaround — and `log` is the package that blocks nineteen others.

## Foundation, package by package: what actually landed

### `@flighthq/adjustments` is complete

22/22 modules, and all three headers compile. Two runtime changes in `include/flight/json.hpp`:

**`JSON.stringify` accepts an absent `space`.** Emitted code writes
`Json::stringify(run, std::nullopt, std::nullopt)`, and the signature demanded `double`, so a call the
source was entitled to make did not compile. `space` is now resolved from absent, numeric, or optional.

**A structural row serializes its members.** This is the part that matters. `append_json_value` ended in
a fallback that appends `"{}"` for an unrecognized type, so a row stringified as an empty object — and
`colorLutRunSignature` builds a **cache key** from `JSON.stringify(run)`. Two different adjustment runs
therefore produced the same signature and the cache returned the wrong baked LUT. That is a silent
correctness bug, not a missing feature.

Rows are now serialized from the owner's own enumerable string keys in declaration order, which is
JavaScript's enumeration order. A property whose value is `undefined`, and one whose cell has no erased
representation at all, are both **omitted** — the second is almost always a function-valued member, and
JavaScript omits those too, so the agreement is real rather than convenient. The branch is reached by
duck typing on `shared_owner()`, so `json.hpp` gains no dependency on `structural_ref.hpp`.

Proven against the committed member table in `tests/structural_row_test.cpp`: a row serializes
`{"kind":"ambient",…}` rather than `{}`, the symbol-keyed entity runtime slot stays out, an absent space
matches omitting it, and — the property the cache depends on — two rows differing in one member, and a
run reordered, both produce different text. All 7 ctest targets pass.

### `insert-using-declaration`, a second repair kind

`flight/log/log.hpp` writes `LogSink`, `LogEntry`, `LogSpan` and `LogTransport` unqualified inside
`namespace flight::log`, where all four live in `flight::types`. gcc names the fix in its own diagnostic:
*"did you mean 'flight::types::LogSink'?"*. Neither existing tool reaches it — these are type aliases, so
no forward declaration exists, and the defining header is already included, so no include helps. Only
the qualification is missing, and a using-declaration introduces a name and nothing else.

Four declared repairs now apply to `flight/log/log.hpp` on every regeneration.

### Where `log` stops, and it is not a declaration

With the qualification fixed, `log` reaches a different error entirely, and the stub is not the problem:
best-effort emitted **1073 lines** of it, marking 22 declarations `NOT GENERATED` with their reasons and
source lines. What fails now is a representation disagreement inside the emitted code:

```
LogSink  = std::function<void(StructuralRef<RowReadonly<RowOf<Ref<LogEntry>>>>)>   // the alias: a ROW
create_fanout_log_sink's lambda takes flight::Ref<flight::types::LogEntry>          // the body: a REF
```

The same type appears as a row in the function-type alias and as a reference in the lambda the emitter
wrote to satisfy it, across roughly 20 sites. That is not fixable by adding a declaration, and rewriting
the lambda parameter types would be editing emitted *code* rather than adding text with no behavior —
the line this repository's repair mechanism deliberately does not cross. It is an emitter fix or a
hand-written replacement of log's sink constructors, and `log` blocks nineteen packages either way.

### Foundation state

| package | modules | headers |
|---|---|---|
| `entity` | 10 / 10 | all compile |
| `math` | 17 / 17 | all compile |
| `color` | 11 / 11 | all compile |
| **`adjustments`** | 22 / 22 | **all compile** |
| `types` | 995 / 995 | 5 fail (mutually dependent aliases) |
| `geometry` | 30 / 30 | 3 fail (all inside log's header) |
| `log` | 2 / 3 | 3 fail (row-versus-reference) |
| `signals` | 8 / 10 | 5 fail |
| `node` | 13 / 21 | 15 fail |
| `materials` | 16 / 24 | 20 fail |

## `log`: a source patch that failed, and a repair that needed fixing

Two attempts, both measured, one kept.

**The source patch failed.** The emitter lowers the same spelling two ways: `Readonly<LogEntry>` becomes
a readonly ROW in `type LogSink = (entry: Readonly<LogEntry>) => void` and a plain REFERENCE in an
annotated lambda parameter written with identical text. Both spellings are in Flight's source and both
are consistent there, so the hypothesis was that dropping the redundant annotation -- leaving the
parameter contextually typed from `LogSink` -- would leave one source of truth.

It did not change the lowering at all. The lambda still took `Ref<LogEntry>`. What it did change was a
side effect: removing the annotations removed the only references that pulled
`#include <flight/types/log.hpp>` into the emitted header, so the file then failed earlier with
`'flight::types' has not been declared`. Ineffective and harmful, and reverted.

**The repair needed fixing, and that was mine.** `insert-using-declaration` introduced four names into
`namespace flight::log` without ensuring they existed: this tip emits `log.hpp` referencing four
`flight::types` aliases and including none of them. A repair that introduces a name has to bring its
definition with it, so the declaration now carries an optional `include`, hoisted to file scope on the
ABI-assertion boundary -- not inside the namespace block, which is the mistake that cost three
iterations on the struct alias.

With that corrected, `log` is cleanly down to one class: roughly 200 diagnostics, all
`could not convert '<lambda closure object>' to 'flight::Ref<std::function<void(StructuralRef<RowReadonly<RowOf<Ref<LogEntry>>>>)>>'`.
Every one is the row-versus-reference disagreement. There is no declaration, include, or qualification
that reconciles it, and rewriting ~20 lambda parameter types would be editing emitted code rather than
adding text with no behavior. `log` is an emitter fix or a hand-written module, and it holds nineteen
packages plus `@flighthq/geometry`, which is 30/30 emitted and fails only through log's header.

## `node`: `source-portability` does not mean cheaply patchable

Eight placeholders, and the compiler classifies seven of them `source-portability`. That classification
is easy to read as "a source patch will do it". For `node` it does not.

Six of the eight fail one rule, `cpp-structural-assertion-writable-capability-unproven`, and all six
reach it through a single assertion in `node.ts`:

```ts
export function getNodeRuntime<Traits extends object = NodeTraits>(
  source: Readonly<Node<Traits>>,
): Readonly<NodeRuntime<Traits>> {
  return getEntityRuntime(source) as NodeRuntime<Traits>;   // asserts the WRITABLE row
}
```

The function declares a readonly return and asserts the writable row, which is precisely what the
diagnostic describes — and the diagnostic prescribes the fix: *"Spell the assertion as [the readonly
row] to remove the writable-capability claim."* Narrowing it to `Readonly<NodeRuntime<Traits>>` is a
one-line, plainly equivalent change, since the writable capability was discarded at the return boundary
and no caller could observe it.

**It changed nothing.** 13/21 modules and the same eight placeholders under the same eight rules. The
diagnostic had already said why, in the clause after the remedy: *"but that alone cannot recover the
derived owner … the retained row has no proven cells for those members."* Removing the spurious claim is
necessary and not sufficient; the owner proof is the real blocker, and the message's remaining remedies
— preserve the writable row through a named typed runner or carrier, or have an erased registry validate
and recover the owner — are a refactor of how Flight types node traversal, not a patch.

So the honest reading of `source-portability` is "the source shape is what the emitter cannot port",
which is a statement about where the problem lives, not about how cheap it is to move.

### The miss this exposed in our own tooling

`ineffectivePatches` did not flag the patch. It compared the declared refusal against refusals of the
patched **module**, and `node.ts` is refused under a different rule (`cpp-reference-assertion-without-heritage`),
so the targeted rule still firing in six *other* modules went unreported. A patch that edits one file to
clear a rule in several is the normal case, so the check now matches on the **package** as well as the
module. Found by a patch slipping through, fixed, and recorded.

### Three source patches now measured ineffective and reverted

`power-emit-void-signal-non-nullable`, `log-contextual-sink-parameter`, `node-runtime-readonly-assertion`.
All three were plausible, two were suggested almost verbatim by the compiler's own diagnostics, and none
changed the lowering. The pattern worth carrying forward: a prescriptive diagnostic describes a condition
the emitter needs met, not a guarantee that meeting it is sufficient — and where the message itself adds
a qualifying clause, that clause has been right every time.

## `log` under the override: 200 to 40, and the residual is a rewrite

The override mechanism works, and `log` is a poor first choice for hand-finishing. Both halves of that
are worth recording.

Mechanical edits alone took `flight/log/log.hpp` from roughly 200 diagnostics of one structural class to
**40**. In order of discovery, each found by compiling rather than by reading:

1. **The `LogSink` alias disagreed with every use of it.** Spelled as a readonly row in
   `flight/types/log.hpp`, spelled as a reference at all ten of its uses — the row spelling appears zero
   times in the module. Fixed in a separate, complete override of one line.
2. **Names left unqualified** inside `namespace flight::log` for types owned by `flight::types`.
3. **TypeScript's `in` operator emitted verbatim as C++**: `flight::String("__kind") in value`. Rewritten
   to `flight::object_has_own`, which is exactly equivalent here because this runtime has no prototype
   chain, so JavaScript's `in` would find only own properties. Two sites, both in this module; nothing
   else in the foundation tree leaks `in`.
4. **`LogLevel`'s representation, twice wrong.** It is a TypeScript `enum`, emitted correctly as
   `enum class LogLevel`, and then used as neither: wrapped in `Ref<>` in 21 places, which an enum can
   never need, and spelled in snake_case as though it were an object in 38 more — `log_level.debug` for
   `flight::types::LogLevel::Debug`.

### What the remaining 40 actually are

**Fifteen references to names that do not exist.** The eight functions best-effort marked
`NOT GENERATED`, plus `emit_signal` and `handle`. This half is honest work with a clear shape: write a
JSON formatter, a text formatter, two console writers, span-field merging, and sink-state teardown.

**About ten representation defects inside code the emitter *did* generate.** Designated initializers
applied to a `std::variant`; `.value` read off a `flight::Any` as though it were a struct; a `Record`
built with a key outside the PropertyKey domain; a ternary whose branches have different types. These are
not missing pieces — they are emitted expressions that are not valid C++, and repairing each one means
recovering its intended semantics from the TypeScript first.

That second group is what makes this module a **rewrite rather than a copy-and-tweak**. The copy-and-edit
workflow assumes the generated text is mostly right and locally wrong; here a tenth of what remains is
emitted code that has to be re-derived. Worth knowing before picking a package to finish by hand: prefer
one whose residual is group 1 only.

## The survey: no foundation package has a cheap residual

Having found that `log` is a rewrite rather than a copy-and-tweak, the next question was which package
*does* have a missing-functions-only residual — the case the copy-and-edit workflow is cheap for. The
answer, across the whole foundation slice, is **none of them**.

`@flighthq/signals` looked like the candidate: 8/10 modules, five failing headers, and the first
diagnostic was the familiar unqualified-name class — `Signal` written bare inside `namespace
flight::signals` where it is `flight::types::Signal`. Two declared `insert-using-declaration` repairs
later, carrying `flight/types/signal.hpp` and `flight/types/signal_connection.hpp`:

```
connection.hpp   33 errors -> 0        scope.hpp        35 -> 14
contract.hpp     49 -> 32              throttle.hpp     14 -> 18
_internal_index  49 -> 32
```

`connection.hpp` compiles. And everything left in `scope.hpp` and `throttle.hpp` is one class:

```
no match for 'operator!=' (operand types are 'const std::function<void(flight::Array<flight::Any>)>'
                                         and 'const std::function<void(flight::Array<flight::Any>)>')
```

Comparing two functions. TypeScript compares them by reference identity, which is how a listener is
removed from a signal; `std::function` has no `operator==` at all. This is the one case flagged from the
beginning as **must not be patched**: any equality invented here would be a semantics this repository
asserted and its own tests then certified. Closing it properly means function values carrying identity —
a `shared_ptr`-backed callable whose comparison is pointer comparison — which is the emitter's
representation choice for every `std::function` it writes, not something the runtime can impose from
below.

So the foundation's remaining blockers are, without exception, representation or emitter issues:

| package | blocked on |
|---|---|
| `signals` | function reference identity — must not be patched |
| `log` | 8 un-generated functions **and** ~10 emitted-code representation defects |
| `node` | derived-owner proofs; a Flight typing refactor |
| `types` | mutually dependent aliases; no include order resolves it |
| `materials` | the anonymous-struct variant family, the corpus's hardest class |

The useful conclusion for sequencing: there is no cheap first package here. `log` remains the most
valuable by a wide margin — nineteen packages plus `geometry` — and its cost is now known precisely
rather than guessed, which is a better position to choose from than the one we were in an hour ago.

## Stubbing log, and the one blocker that is left

`log` is now stubbed rather than implemented, because the goal is a working SDK and not a finished log
module. The override does three things in order of increasing bluntness:

1. **Corrects representation** — the `LogSink` alias, unqualified names, the `in` operator, and
   `LogLevel`, which is a TS enum the emitter both wrapped in `Ref<>` (21 sites) and spelled snake_case
   as an object (38 sites).
2. **Stubs nine invalid bodies.** Each of these functions had an emitted body that is not valid C++ —
   designated initializers on a `std::variant`, `.value` read off a `flight::Any`, a `Record` key outside
   the PropertyKey domain, and in `create_file_log_sink` a read of a `handle` that is never declared in
   its scope. Each now throws, naming itself and the reason. Throwing is deliberate: a logging function
   that silently does nothing looks fine forever, and this names the gap at the one moment it matters.
3. **Declares three functions the emitter never generated** — `merge_span_fields`,
   `initialize_log_entity`, `create_json_log_formatter`. Their real return types differ per call site, so
   there is no single signature to write; they return a `NotImplementedInThisProfile` value that converts
   to whatever the call site wanted and throws at the conversion.

That took the module from roughly 200 diagnostics to **10**, and all ten are one thing:

```
include/flight/equality.hpp:10: no match for 'operator==' (operand types are
  'const std::function<void(std::shared_ptr<flight::types::LogEntry>)>' and the same)
```

`emit_to_sinks` calls `emit_signal`; emitting a signal compares its slots; `SameValueZero` needs
`operator==` on the slot type. TypeScript compares functions by **reference identity** — that is how a
listener is removed — and `std::function` has none.

This is the blocker refused on purpose throughout, and it is worth being precise about why no local fix
works. `std::function::target()` looks like an answer and is not: copies of one `std::function` hold
distinct targets, so it would report *different* exactly where JavaScript reports *same*, breaking
listener removal in the quiet direction. Any equality invented here would be a semantics this repository
asserted and its own tests then certified as true. Closing it needs function values that carry identity —
a `shared_ptr`-backed callable compared by pointer — which is the emitter's representation choice for
every `std::function` it writes, and the same blocker that stops `@flighthq/signals`.

So `log` and `signals` are one blocker, not two, and it is the single highest-value thing left in the
corpus: it holds nineteen packages plus `geometry` through log, and `signals` on its own account.

## The generation hang, bisected to one package

The experiment proposed above was run, and then run to a conclusion. The answer is not scaling.

`sdkGeneration.mjs` now takes `--package=` (repeatable, resolving the transitive closure itself, because
`analyzeFlightWorkspace` takes its target list literally and resolves no closure). With that filter the
hang can be bisected, and it was:

| slice | packages | modules | result |
|---|---|---|---|
| `types` alone | 1 | 995 | 1m40s ✓ |
| foundation-era closure | 38 | — | 6m59s ✓ |
| `scene2d` closure | 13 | 1175 | 3m59s ✓ 1144 emitted |
| `render` closure | 14 | 1208 | ✓ 1165 emitted |
| **`render-gl` closure** | **18** | — | **hangs; killed at 600s** |
| `scene2d`+`scene2d-gl`+`render`+`render-gl` | — | — | hangs; killed at 1500s |
| full graph | 154 | 2904 | >22m, ignored SIGTERM |
| **`render-gl` closure *minus `render-gl` itself*** | **17** | **1240** | **6m6s ✓ 1185 emitted** |

The last row is the result. `render-gl`'s closure is `render`'s plus `animation`, `scene2d`, `texture`,
and `render-gl`'s own 33 modules. Generate all seventeen of those packages — 1240 modules, more than the
`scene2d` and `render` slices that both completed, and all four of the packages `render-gl` adds — and it
finishes in 6m6s. Add `render-gl`'s own source and it does not finish in 600s.

So the trigger is **`@flighthq/render-gl`'s own modules**, not graph size, not module count, and not any
package it depends on. `--package=@flighthq/render-gl` is a minimal reproduction, and it is minimal in the
sense upstream said it needed: the difference between completing and hanging is 33 files, against a
17-package closure that is proven to complete.

That also explains why the full-graph hang looked like a scaling wall for four consecutive compiler tips.
It never was. The full graph contains `render-gl`.

Two consequences for this repository:

1. **Per-package generation is unblocked today.** Every slice that excludes `render-gl` and `scene2d-gl`
   (which pulls it in) completes, which covers the foundation and the whole `scene2d`/`render` target.
2. **It is one package's shape, so it is probably one construct.** A 33-module reproduction is small
   enough to bisect further by module if the trigger needs naming rather than avoiding.

### What the seventeen-package slice actually emits

The slice that completes is also the widest honest coverage measurement we have — 1185/1240 modules,
17 packages, 55 refusals:

| package | modules | placeholders |
|---|---|---|
| `types` | 995/995 | — |
| `geometry` | 30/30 | — |
| `adjustments` | 22/22 | — |
| `camera` | 21/21 | — |
| `math` | 17/17 | — |
| `animation` | 12/13 | 1 |
| `color` | 11/11 | — |
| `entity` | 10/10 | — |
| `signals` | 8/10 | 2 |
| `materials` | 16/24 | 8 |
| `node` | 13/21 | 8 |
| `mesh` | 8/16 | 8 |
| `texture` | 5/9 | 4 |
| `render` | 10/25 | 15 |
| `scene2d` | 3/10 | 7 |
| `log` | 2/3 | 1 |
| `registry` | 2/3 | 1 |

Six packages are already whole at the emitter: `types`, `geometry`, `adjustments`, `camera`, `math`,
`color`, `entity`. The 55 placeholders concentrate in the leaves — `render` 15, `materials`/`node`/`mesh`
8 each, `scene2d` 7 — and they cluster under a short list of rules:

```
12  cpp-reference-assertion-without-heritage
10  cpp-structural-assertion-writable-capability-unproven
 7  cpp-structural-assertion-owner-unproven
 5  cpp-contextual-union-value-type-unrepresented
 4  cpp-member-projection-multiple-present-domains
 3  cpp-type-assertion-unidentified
```

Those top three — 29 of 55 — are the asserted-row family already documented above, the one whose repairs
have been measured at zero every time they were tried from this side. The next two are the anonymous
variant family in `materials`. Nothing in the residual list is new; the value of this run is that the
distribution is now measured across seventeen packages rather than inferred from four.

## The largest single cause: `flight::Ref` is a non-deduced context

This one is worth stating plainly because it had been mis-attributed for a long time, including by me.

`flight::Ref<Value>` is not a transparent alias. It resolves through

```cpp
template <typename Value>
using Ref = typename detail::reference_shape<Value,
    decltype(detail::complete_probe<Value>(0))::value>::type;
```

— a computation. A template parameter that appears *only* inside that alias is therefore in a
**non-deduced context**, and every generated signal and node helper is declared exactly that way:

```cpp
template <typename T, typename... ArgsPack>
  requires flight::callable_signature_v1<T>::template accepts<ArgsPack...>
inline void emit_signal(flight::Ref<flight::types::Signal<T>> signal, ArgsPack&&... args);
```

The declaration is well-formed, the file compiles alone, and **every call fails**:

```
error: no matching function for call to 'emit_signal(...)'
note:   couldn't deduce template parameter 'T'
```

That is why packages which are 100% emitted still have failing headers. `@flighthq/camera` is 21/21
emitted and three of its 21 headers fail for no reason of its own — they reach `flight/log/log.hpp`,
whose two `emit_signal` calls cannot deduce. Measured over the seventeen-package slice, the shape
appears in 13 files and 33 parameter positions, concentrated in `node`, `signals` and `registry`, which
are three of the stuck packages and the dependencies of most of the rest.

### What it is not

Two earlier readings were wrong, and both cost real time:

- **It is not the row-versus-reference seam.** The runtime already admits that deliberately:
  `callable_argument_v1` accepts a `std::shared_ptr<T>` argument for a structural-row parameter over
  `T`, with a comment explaining why. Probed directly, both
  `accepts<Ref<LogEntry>&>` over a `Ref<LogEntry>` parameter and over a
  `StructuralRef<RowReadonly<RowOf<Ref<LogEntry>>>>` parameter are **true**. An override rewriting
  `LogSignals`' handler spelling to match the emit site was written, measured, and **deleted as
  unnecessary** — the repair alone closes it.
- **It is not function-reference identity.** The entry for the `log` override claimed the remaining ten
  diagnostics came from `emit_signal` comparing a signal's slots. `emit_signal` compares nothing; it
  forwards to `signal->emit`. That misreading is what made `log` look unfinishable.

### The repair, and why its equivalence is compiled rather than argued

`reference_shape` has exactly two answers: `std::shared_ptr<Value>` for a `Value` deriving from
`flight::ReferenceEnabled`, and `Value` itself otherwise — and `make_ref` already `static_assert`s that
correspondence. So writing the alias's own result in place of the alias is a **spelling** change, not a
behavior change, and that is what keeps it inside the rule repairs live by. The only difference is that
the result is deducible and the alias is not.

A new repair kind, `respell-reference-alias`, does that. Twelve are declared, each naming the template
and which of the two expansions it takes:

| expansion | symbols |
|---|---|
| `shared-pointer` | `Signal`, `SignalData`, `SignalConnection`, `Node`, `NodeRuntime`, `NodeOrderList`, `OrdinalTable`, `KeyedTable`, `SlotTable` |
| `value` | `Transform2DNode`, `Transform3DNode`, `RegistryTable` |

The split is not cosmetic and getting it backwards is a type error: the first group are structs deriving
from `ReferenceEnabled`, the second are aliases to a `StructuralRef` or a `std::variant`, for which
`Ref<X>` collapses to `X`. So the repair does not *argue* its equivalence — generation emits
`flight/repairs/reference_alias_identity.hpp`, one line per repair:

```cpp
static_assert(std::same_as<flight::Ref<flight::types::Signal<std::function<void()>>>,
                           std::shared_ptr<flight::types::Signal<std::function<void()>>>>,
              "respell-reference-alias respell-reference-alias-signal claims the wrong expansion for Signal");
```

compiled by the same gate that compiles the headers. A wrong expansion fails the build instead of
quietly changing a signature. All twelve assertions compile.

Applied to the seventeen-package tree the twelve repairs touch **205 files**, and every one of them
matches something, so none is already obsolete.

## `log` compiles

With the deduction defect repaired, `flight/log/log.hpp` goes to **zero diagnostics**. Two further
corrections were needed, both found only once the module got far enough to show them:

1. **`Record<LogLevel, String>` rekeyed to `Record<double, String>`.** TypeScript's
   `Record<LogLevel, string>` is keyed by a *number* at runtime — `LogLevel` is a numeric enum and
   `_levelNames[level]` is a numeric index. The emitter kept the enum as the C++ key type, and
   `flight::Record` admits only the PropertyKey domain, so the spelling failed its own `static_assert`:
   *Flight Record key is not a PropertyKey domain*. Twelve enum keys and both lookups now convert at the
   boundary. Same class of correction as the `LogLevel` work already in that override.
2. **`remove_log_sink` stubbed to throw.**

That second one is where the refusal is load-bearing, and the honest accounting is this: function
reference identity was reachable from exactly **two lines** in `@flighthq/log`.

- `add_log_sink`'s `sinks.includes(sink)` — a dedupe. Dropped, with the divergence named: registering
  the same sink twice now appends twice. No caller in the SDK does.
- `remove_log_sink`'s `sinks.index_of(sink)` — finding the sink by identity **is** the function. It
  cannot be dropped and it cannot be faked, so it throws and says why.

`@flighthq/signals` keeps the genuine blocker in `flight/signals/slot.hpp`: `disconnect_signal` and
`is_slot_connected` compare a stored slot against a passed one, which is listener removal. Those are
templates, so they fail only where instantiated — which is a materially better position than "log and
signals are one blocker holding nineteen packages", the claim this section replaces.

### A correction to the claim above, and what is actually established

The section above says "per-package generation is unblocked today. Every slice that excludes
`render-gl` and `scene2d-gl` completes." That was written from slices of up to 38 packages and it
over-reached. Measured afterwards: a run over **all 145 applicable packages** with `render-gl`'s whole
family and the five `web` packages excluded **did not complete in 45 minutes** (killed with `-s KILL`,
no output).

So two claims have to be separated, because only one of them is established.

**Established**, by a controlled comparison: `render-gl`'s own source triggers a hang. Its 18-package
closure does not finish in 600s; the same closure minus its 33 modules finishes in 6m6s with 1185 of
1240 emitted. The two runs differ by one package's source and nothing else.

**Not established**: that `render-gl` is the *only* trigger. The 145-package result is consistent with a
second trigger somewhere in the remaining 138 packages, and equally consistent with 145 packages simply
costing more than 45 minutes of wall clock. Nothing measured so far separates those, and the timings do
not extrapolate: 995 modules of `types` alone take 1m40s, 1240 modules across 17 packages take 6m6s, and
a 38-package closure takes 6m59s, which is superlinear in packages and roughly flat in modules. A 145
package run being four to six times the 38-package cost would land outside the 45-minute window on its
own.

The discriminator is a halving, and it is running: 73 packages, then 72. If both halves complete, the
cost is scale and the answer is batched generation. If one half hangs, it contains a second trigger and
halving again names it — the same ladder that found `render-gl`, which took five runs.

What this does not change: the per-package and per-slice workaround is still real for every slice
actually measured, and `log`, `camera`, `geometry`, `node`, `registry` and `signals` were all repaired
against a tree that generated in 6m6s. The open question is the size of the batch the committed
inventory can be produced in, not whether the packages can be generated at all.

## The unqualified-name class, swept to the bottom

The report that drives `sdk:header-compile` keeps the FIRST diagnostic per header, which makes an
iterative repair loop look like it has converged when it has only moved. Three rounds of
repair-then-recompile over `node` and `registry` left the count at 6 of 24 compiling, because each round
cleared one name and revealed the next. Compiling each header directly and collecting *every*
`was not declared in this scope` in one pass names the whole class at once:

```
node      Matrix4  NodeAny  NodeOrderList  NodeOrderListEntryVisitor  NodeTraits  Rectangle
          Transform3DLike  Transform3DNode  Vector3Like  ViewportAlign
node      allocate_entity  finish_entity  create_signal  create_rectangle
node      point  source  target
registry  get_registry_table_entry_state  registry_entry_state
```

Three different things, and only the first two are repairable:

1. **Ten types owned by `flight::types`**, used unqualified inside `namespace flight::node`. The existing
   `insert-using-declaration` kind covers these exactly.
2. **Four FUNCTIONS from other packages** — `allocate_entity` and `finish_entity` from
   `flight::entity`, `create_signal` from `flight::signals`, `create_rectangle` from
   `flight::geometry` — written unqualified. The same defect on a function rather than a type, and a
   using-declaration introduces the name and nothing else, so it stays inside the rule.
3. **`point`, `source` and `target` are not defects at all.** They are parameter names, and they failed
   to resolve only because their own types (`Vector3Like`, `Transform3DNode`) were undeclared. They
   disappear with group 1 and were never worth a repair. Worth saying because a sweep like this produces
   exactly this kind of false positive, and counting them would have inflated the class by three.

Fourteen repairs added; applied to the seventeen-package tree the full set now touches **256 files**.

### Why `registry` is not finished by any of this

`registry`'s two names look like the same class and are not. `registry_entry_state` is a TypeScript
`const` object, not an enum, and the emitter lowered it correctly as a `flight::Ref<...>` in
`flight::types` — so its uses need qualification *and* `->` instead of `.`, which is editing code.
`get_registry_table_entry_state` is worse: it is one of **eleven refused functions** in that module,
listed in the file's own `NOT GENERATED` header alongside `concatRegistryTable`, `createKeyedTable`,
`withRegistryTableEntry` and eight more. Finishing `registry` means hand-writing most of a module, which
is an override, not a repair, and it is not the cheapest thing left. Recorded rather than attempted.

## `node`, taken from one cause to two, and why the last one is refused

`node` went from 6 of 21 headers compiling to **12 of 21** in this pass, and the value is less in the
number than in what the residual turned into: five distinct causes became two.

The sequence, each step measured:

| step | node | what it closed |
|---|---|---|
| after the `respell-reference-alias` repairs | 6/21 | `couldn't deduce template parameter 'T'` |
| after fourteen unqualified-name repairs | 6/21 | every `was not declared in this scope` |
| after `name-defaulted-template-argument` | 6/21 | bare `Node` in template-argument position |
| after `deduce-call-argument-from-assignment` | **12/21** | `create_signal()` |

The three middle rows moving nothing is the point worth recording: each closed a whole error class and
revealed the next one behind it in the same headers. A count is a bad progress measure under a
first-error-per-file report; the class list is the real one.

### Two repairs worth describing, because both could have been done wrong

**`create_signal()`.** `createSignal<T>()` takes its type argument from TypeScript's *contextual
typing* — `out.onChildAdded = createSignal()` reads `T` off the declared type of the target — and C++ has
no equivalent, so the emitted call has nothing to deduce from. It was the sole remaining cause in twelve
of node's 21 headers.

The fix follows the order `AGENTS.md` sets: extend the runtime first. `include/flight/template_argument.hpp`
adds one trait that names the first template argument of an instantiation, generic over any class
template and naming nothing the compiler emits, so the runtime stays uncoupled from generated types. The
repair then applies TypeScript's own rule mechanically — it reads `T` off the assignment target, which is
sitting in the text:

```cpp
(out->on_child_added = create_signal<flight::template_argument_t<
     typename std::remove_cvref_t<decltype(out->on_child_added)>::element_type>>());
```

The argument is not *chosen* here, it is *recovered*: TypeScript says `T` is the target's parameter, so
there is one answer. And unlike the other repairs this one is self-checking — a wrong type would not
compile, so a wrong rewrite cannot reach a built header. Only the assignment form is matched; the single
non-assignment call in the tree (`flight/render/render_cache.hpp`, inside a nullish-assignment lambda) has
no target to read and is left alone rather than guessed at. Five tests pin the trait, including the
`element_type` path the repair relies on and the first-of-many-arguments case.

**Bare `Node`, and the wrong answer I wrote first.** TypeScript declares
`interface Node<Traits extends object = NodeTraits>`, so a bare `Node` means `Node<NodeTraits>`; the
emitter drops the default and writes the bare name in template-argument position, which is not a type.
Two substitutions are available and both are wrong:

- `flight::types::NodeAny` is `Node<any>` — a *different* instantiation, which the tree uses 29 times for
  genuinely untyped nodes.
- `flight::types::NodeTraits`, which is what I wrote first. It compiles in isolation and then fails to
  convert at `set_reparent_node_guard`, because everywhere the emitter *does* supply this argument it
  spells it `Node<flight::Ref<flight::types::NodeTraits>>` — eight occurrences, and zero of the bare
  form. That is the authority for the spelling, and it is also the semantically right answer:
  `NodeTraits` derives from `flight::ReferenceEnabled` and TypeScript object types lower through `Ref`.

The repair fires only where the bare name sits in template-argument position — after a `<` or `,` and
before a `,` or `>` — because a class-template name with no arguments is *never* valid C++ there. So
there is no reading of the text under which the rewrite could be altering a correct program, and the
definition site, the parameterised uses and every ordinary mention are all outside it.

### Repairs are now applied to a fixed point

Adding the above exposed an ordering bug in the mechanism itself. The bare-`Node` repair writes a type
argument; the using-declaration repair that would have introduced that name had already run and found
nothing, so the result depended on the order entries happen to sit in the JSON — not a property anyone
would think to preserve while editing the file. `applyEmissionRepairs` now iterates until nothing
changes, with a bound of eight rounds and a hard error if it is hit, since two repairs rewriting each
other is a declaration bug rather than something to tolerate. It immediately paid: round one writes
`flight::Ref<Node<...>>` and round two's respell repair turns it into `std::shared_ptr<Node<...>>`.

### The absence channel: recorded, deliberately not repaired

`has_clip`, `has_material` and `has_blend_mode` fail on one shape:

```
could not convert row_get<RowKey<"clip">, RowReadonly<RowPartial<RowOf<shared_ptr<HasClip>>>>>(...)
  from 'std::optional<std::shared_ptr<ClipRegion>>'
  to   'std::variant<std::shared_ptr<ClipRegion>, flight::Null, flight::Undefined>'
```

The emitter is right to want three states: `initClipTrait` reads `obj?.clip ?? null` where
`clip: ClipRegion | null`, so the value can be absent, null, or a region. The runtime's `row_get` over a
`RowPartial` returns a single `optional<V>`, which has already collapsed null into absent, so **the
information is not recoverable at the call site**.

It would be easy to "fix" by mapping `nullopt` to `Undefined`, and at these three sites it is even
observationally identical — the surrounding code only asks whether a region is present and treats null
and undefined alike. That is exactly the reasoning to refuse. A repair applies everywhere its pattern
matches, not only where someone checked the collapse is harmless, and `AGENTS.md` names absence as one of
the semantics no workaround may change. So this is not repair work: it is a runtime capability —
`row_get` over a `RowPartial` preserving three states — plus the emitter agreeing on the spelling, and
`flight::Presence<V>` already exists for it in `include/flight/presence.hpp`, though with its alternatives
in the other order (`variant<Undefined, Null, V>` against the emitter's `variant<V, Null, Undefined>`),
which is likely why it has never been used.

## Half A terminates: the hang is `render-gl`, and the rest is wall clock

The halving ran, and the first half settled the question the 145-package timeout had left open.

**73 packages generate in 27 minutes.** The run then failed at the very end, for a reason that is its own
lesson: `loadEmissionRepairs` was called *after* the compile, and the declaration file was edited while
the run was in flight, so a 27-minute compile finished and then threw on a repair kind the
already-loaded module did not recognise. Generation itself had completed. `loadEmissionRepairs` now runs
before the compile so that mistake costs a second instead of half an hour.

With 73 packages at 27 minutes, a 150-package run at more than 45 minutes is ordinary superlinear growth,
not evidence of a second trigger. The ladder in full:

| slice | packages | result |
|---|---|---|
| `types` alone | 1 | 1m40s |
| `scene2d` closure | 13 | 3m59s |
| `render` closure | 14 | completes |
| `render-gl` closure **minus its own source** | 17 | 6m6s |
| foundation-era closure | 38 | 6m59s |
| **half A** | **73** | **27m7s** |
| `render-gl` closure | 18 | hangs at 600s |
| all applicable | 145 | no completion in 45m |

Every row that excludes `render-gl` completes. Every row that includes it does not. Nothing in the
timings needs a second explanation.

## The committed inventory is now reproducible

Several things had to be true before `generated/` could hold a best-effort tree, and none of them were:

1. **Best-effort is now the default**, not a flag. The committed inventory has to be reproducible by
   `npm run sdk:generate` with no arguments, and a flag would mean three call sites — that script,
   `sdk:check`, and `scripts/check.mjs`'s own argument list — each remembering to pass it. `--best-effort`
   is still accepted as a no-op so recorded command lines keep working; `--no-best-effort` asks for the
   old strict behaviour.

2. **The target set excludes what cannot be generated, by declaration.** A deferral normally changes no
   generated output — headers are still emitted, still compiled, always reported — and that property is
   why deferral is safe to have. The `render-gl` family is the one exception, and it is marked as such:
   `generation: "does-not-terminate"` on the entry, refused by the loader on anything but a `kind` of
   `defect`, because a package the generator cannot finish is debt we owe and never a capability this
   profile lacks.

   The *consequences* are computed, not listed: any package whose transitive closure reaches a
   non-terminating one is excluded too, so a package that newly starts depending on `render-gl` is
   excluded without anyone remembering to add it. That computation also corrected my own list — I had
   been excluding seven packages, and only **four** are in `@flighthq/sdk`'s closure at all
   (`render-gl`, `effects-gl`, `scene2d-gl`, `scene3d-gl`); `tool-capture` and `host-web` are workspace
   packages outside the barrel, and `sdk` is not its own dependency. **150 of 154 packages are generable.**

   I had also been excluding the five `web` packages from generation, which is wrong on the same
   principle: applicability governs the compile gate and the shippable denominator, not whether a header
   gets emitted.

3. **Repair expiry is judged only on a full run.** A `--package=@flighthq/math` run reported ten repairs
   as obsolete — five node ones, the geometry one, and four tree-wide ones whose constructs math does not
   contain — which reads as an instruction to delete live repairs because of what the run did not ask
   for. Scoping by `appliesTo` fixes the first group and provably cannot fix the last, since a repair
   declared over `flight/` is in scope for every run and still only fires where its construct appears.
   A full run is the only run where "matched nothing" and "no longer needed" are the same statement.

## Where the foundation stands

Measured against the repaired seventeen-package tree, 125 of 148 headers in the foundation slice compile,
and the complete packages are no longer only the trivially independent ones:

| package | headers | |
|---|---|---|
| `adjustments` | 22/22 | complete |
| `geometry` | 30/30 | **complete** |
| `math` | 17/17 | complete |
| `color` | 11/11 | complete |
| `entity` | 10/10 | complete |
| `log` | 3/3 | **complete** |
| `camera` | 18/21 | one file |
| `node` | 12/21 | two causes |
| `signals` | 6/10 | |
| `registry` | 0/3 | eleven refused functions |

`log` and `geometry` are the new ones, and they were the two that mattered: `log` is the second-largest
cascade root in the SDK at nineteen packages, and `geometry` was 30/30 emitted and failing only inside
`log`'s header and two `log_once` calls.

`camera`'s remaining three failures are all the same single file, `frustum_corners.hpp`, which writes
`out.element(i).x = ...` where the element is a `shared_ptr` — TypeScript's `out[i].x = v` through a
reference, needing `->`. That is an operator emitted as source text, which `AGENTS.md` names as override
territory rather than repair territory, and an override needs a `derivedFrom` digest from a `generated/`
tree that actually contains the file. So `camera` completes once the inventory is promoted, not before.

`signals` has one cheap item and two that are not. The cheap one is a single `binding_value %= period` on
two `double`s, which C++ has no `%` for and which `std::fmod` answers exactly — JavaScript's `%` *is*
`fmod`. It is deliberately left alone: it is the only such site in the tree, and behind it `signals` is
blocked by `disconnect_signal`'s function-reference identity and by a slot-storage type mismatch
(`Array<optional<function<void(Array<Any>)>>>` against a signal whose `T` is `function<void(double)>`),
so repairing one line would buy no package and add a maintenance obligation.

## `camera` completes, and why its one-line fix is an override rather than a repair

`camera` is now **21/21**. The whole modification is `.` to `->`, three times, in
`get_camera3_dfrustum_corners`:

```cpp
(out.element(i_2)->x = results.element(i_2).element(0.0));
```

`out` is `Array<Vector3Like>` and `Vector3Like` is `flight::Ref<Vector3>`, a `shared_ptr`. The
TypeScript is `out[i].x = results[i][0]` — a member assignment *through* a reference — and the emitter
wrote a direct member access on the element, so the diagnostic is
`'std::shared_ptr<flight::types::Vector3>' has no member named 'x'`. `Array::element` returns a
reference to the stored pointer, so `->x = v` assigns through to the pointee: the same object the caller
passed in, mutated in place, which is what the TypeScript does to the array it was given.

It looks mechanical enough to be a repair, and it must not be one. Choosing between `.` and `->`
requires knowing the element type is a pointer, which is type information no text rewrite has. There are
eleven `.element(...).member` sites in the tree and `@flighthq/adjustments` **compiles** with its own,
because there the element is a value — so a textual rule would break a complete package to fix this one.
That is the test for whether something belongs in `repairs/`: not "is the edit small", but "is the
condition under which it is correct visible in the text".

Three overrides now, and all three are the same category — an operator or a name the emitter wrote as
source text, which is the one thing the other three mechanisms cannot express.

## A real defect that repairing buys nothing, and so is not repaired

`SignalData<T>` is declared in TypeScript as

```ts
export interface SignalData<T extends (...args: any[]) => void> {
  slots: (T | null)[];
```

and emitted as

```cpp
template <typename T>
struct SignalData : public flight::ReferenceEnabled {
  flight::Array<std::optional<std::function<void(flight::Array<flight::Any>)>>> slots;
```

The emitter substituted `T`'s **constraint** for `T`. `slots` should be `Array<optional<T>>` — `T | null`
is `optional<T>` by the emitter's own convention for a nullable — and the consequence is that
`SignalData<T>::slots` holds the same type for every `T`, so a signal instantiated with a concrete slot
type cannot store its own slots: `Array<optional<function<void(Array<Any>)>>>::splice(..., function<void(double)>&)`.

That reads like a clean, high-value repair. **It was measured and it is not one.**

Patched into a scratch tree, `Array<optional<T>>` is accepted and every use site agrees with it — which
confirms the diagnosis — and then:

| | before | after |
|---|---|---|
| `signals` headers | 6/10 | 6/10 |
| `animation` + `scene2d` + `texture` headers | 8/32 | 8/32 |

Nothing. `signals` does not move because the identity blocker sits behind it: with `slots` restored,
`disconnect_signal` still needs `operator!=` between `optional<T>` and `T`, which is `operator==` on a
`std::function`, which is the refusal this repository holds on purpose. And the consumers do not move
because all 24 of their failures are an earlier and entirely different class — unqualified names
(`AnimationInterpolation`, `NodeAny`, `TextureLike`).

So the defect is real, the fix is right, and declaring it today would add a maintenance obligation and a
line in the expiry ledger for zero shipped headers. It is recorded here instead, to be declared when
something downstream of it can actually move. This is the same conclusion the deleted
`headerIncludeRepair` reached, and the same rule: a repair earns its place by removing a failure, not by
being correct.

For the record, the full `signals` residual once `slots` is restored and the single `%=` is rewritten is
two causes: `std::optional<std::function<...>>::optional(<brace-enclosed initializer list>)` at
`flight/signals/slot.hpp:155`, and the `operator!=` identity blocker. The second cannot be closed here,
so `signals` cannot be a complete package at this pin whatever else is done to it.

## The unqualified-name class is almost never the blocker

This is the third time in this session that a correct fix turned out to buy nothing, and the three
together make a pattern worth stating as a rule rather than rediscovering a fourth time.

`animation`, `scene2d` and `texture` are 8 of 32 headers. Every one of the 24 failures reported an
unqualified name, so the class looked like the gate. Resolved against the tree — each name's namespace
taken from where it is actually declared, not guessed — that is 56 names across seven (package, namespace)
pairs. Hand-applied to a scratch tree and iterated to convergence:

| round | names found | resolvable |
|---|---|---|
| 1 | 71 | 56 |
| 2 | 15 | 3 |
| 3 | 12 | **0** |
| 4, 5 | 12 | 0 |

**8 of 32 before. 8 of 32 after. Nothing newly passes.**

The chain terminates on twelve names that have *no declaration anywhere in the generated tree* —
`initialize_animation_track`, `get_node_runtime`, `initialize_sprite_renderer_data`, `clone_texture`,
`get_first_texture_source`, `equals_texture_content` and six more. Those are not qualification problems;
they are functions the compiler refused to generate. Behind the names is hand-written code.

The final residual for these three packages is two causes and neither is reachable by any repair:

- **18 headers**: a never-generated function. Override territory — writing the function.
- **6 headers** (all of `texture`): `base operand of '->' has non-pointer type
  'flight::Ref<std::variant<std::shared_ptr<...>>>'`. `Ref` of a variant collapses to the variant, and
  the emitter wrote `->` on it. TypeScript permits `.member` on a union where every member has it; C++
  needs `std::visit`. That is a runtime capability or an override, not a text rewrite.

### The rule

In this corpus, "was not declared in this scope" is what gcc reports *first*, almost never what is
actually blocking. The report keeps one diagnostic per header, so a class that appears in every failure
row looks like the gate and usually is not. Three measurements this session:

| fix | correct? | headers gained |
|---|---|---|
| `respell-reference-alias` (12 repairs) | yes | **many** — closed `log`, enabled `geometry`, `camera` |
| `deduce-call-argument-from-assignment` | yes | **6** — node 6/21 to 12/21 |
| `SignalData<T>` constraint substitution | yes | 0 |
| 56 unqualified names in three packages | yes | 0 |

The two that paid were the ones that answered a *structural* defect — a non-deduced context, a missing
contextual type. The two that did not were cosmetic in effect even though real in cause. Measure the
delta, never the plausibility.

### What was declared anyway, and why that is not a contradiction

The seven grouped name repairs **are** declared, despite gaining zero headers, and the reasoning is
narrow enough to be worth writing down because it reverses a test I set myself one step earlier:

- They are not speculative. These packages cannot compile without them *and* without the overrides; the
  names are necessary, just not sufficient.
- They cannot mask anything. A using-declaration either resolves a name or does not; unlike a type or
  operator rewrite there is no wrong-but-compiling outcome.
- They match headers, so the expiry rule is satisfied honestly — `sdk:check` is not being told a
  falsehood to keep them alive.
- Without them, whoever writes the `texture` and `scene2d` overrides rediscovers all 56 names first.

`SignalData` stays undeclared by the same reasoning applied honestly: it is a *type* change, it could be
wrong-but-compiling, and nothing downstream of it can move at this pin.

### One entry per defect, not one per name

56 rows for one emitter defect would make the expiry ledger unreadable and its signal worthless, so
`insert-using-declaration` now accepts a `symbols` list with a shared `namespace` — seven entries. Each
name is still enumerated, and each is introduced only into the files that use it unqualified, so the
emitted text is what 56 single-symbol entries would have produced.

Each name carries **its own** defining header, and that is not a detail. The first version of this used
one `include` per entry taken from the first name's header; `scene2d` got
`flight/types/animation_interpolation.hpp` for a group that also contained `EntityConstruction`, and a
header that had been **passing** started failing with `'EntityConstruction' has not been declared in
'flight::types'` — 8/32 down to 7/32. Measuring on a scratch tree before declaring is the only reason
that never reached `repairs/`.

It is deliberately not `using namespace flight::types;`, which would be shorter still: a directive also
pulls in every name the package did not ask for, so a future collision between a package's own name and
a types-owned one becomes an ambiguity error in generated code nobody edited.

## The committed inventory is the SDL-profiled tree, and the first whole-SDK failure map

### What was wrong with the first promotion

`generated/` was promoted once from a run with **no binding profiles**, and that was a mistake worth
recording because the symptom is so quiet. Without the SDL profiles the compiler has no binding for
`console`, `performance`, `setInterval` or `clearInterval`, so it refuses whole modules that need them:
`generated/include/flight/log/log.hpp` came out as a 149-line placeholder reading *"no part of it was
generated"*, where the profiled tree has 1081 lines of real output.

| | unbound | SDL-profiled |
|---|---|---|
| 150 packages | 1907/2709, 802 refusals | **2172/2709, 537 refusals** |
| `log`'s 4-package closure | log refused entirely | 1015/1018, 3 refusals |
| `math` + `types` | — | 1012/1012, **0 refusals** |

The tell was mechanical, not a judgement call: the committed tree **did not match its own overrides**.
`overrides:check` reported `flight/log/log.hpp is no longer generated`, and the expiry check named four
`log-log*-using-declaration` repairs as matching nothing — correct, because a placeholder contains no
unqualified names. Both mechanisms did exactly what they exist for.

So the SDL profile set is now the script's **default**, for the same reason best-effort is: three call
sites have to agree and a flag lets them drift. An explicit `--binding-profile` list still wins, so the
narrower `sdk:generate:headless` and friends are unaffected. After regeneration all three overrides match
their `derivedFrom` digests and all 58 repairs match at least one header.

Two defaults followed from it: `overridesCheck` and `sdkPackageTiers` both defaulted to `out/sdk-sdl`,
which was the SDL side output back when `generated/` was unbound. Those trees now have identical content
and `out/` is gitignored, so the default failed on a fresh clone for no reason. Both read `generated/`.

### The name class, measured across the whole SDK

Half the tree compiled (1356 of 2710 headers, 553 passing) gives the first corpus-wide cause breakdown:

| cause | headers | share of failures |
|---|---|---|
| unqualified or undeclared name | 576 | **71.7%** |
| type conversion | 72 | 9.0% |
| missing operator | 55 | 6.8% |
| no matching function | 50 | 6.2% |
| member on wrong shape | 25 | 3.1% |
| arrow on a non-pointer | 6 | 0.7% |
| other | 19 | 2.4% |

Resolving every one of those names against the whole committed tree — asking whether the name is
declared *somewhere* — gives **550 of 576 resolvable by qualification (95.5%)**, with only 26 genuinely
declared nowhere.

That is a **correction to the conclusion recorded above**, which generalised from three packages where
the name chain terminated in never-generated functions. Across 150 packages it overwhelmingly does not:
the names almost all resolve.

The first attempt at this measurement said 489 of 576 and named `registry_entry_state` as undeclared 61
times. That was my own indexing bug, not a finding: the declaration index matched functions with
`^inline … name(` and so missed `inline` **variables**, which have no parenthesis —
`inline flight::Ref<bound_tombstoned_…> registry_entry_state = …` is right there in
`flight/types/registry_table.hpp`. One missing alternation inflated the unresolvable share by a factor of
three. Worth stating because the corrected number changes which work is worth doing.

### What is still not established

Resolvable is not the same as *gained*, and the three-package experiment is still the only direct
measurement of the delta: 56 names resolved, iterated to a fixed point, **zero headers gained**. So the
open question is not whether these names can be qualified but whether anything is behind them, and the
answer differs per package.

The packages worth testing first are the **45 that emitted completely**, because a package with no
refusals cannot have a never-generated function behind its names — there is nothing else for the chain to
terminate on. None of the 576 sampled name failures fall in one, which is either because those packages
pass or because they sit in the half not yet compiled. The full report settles it.

## 33 of 146 packages are shippable

The full tree compiled against the committed inventory: **1686 of 2710 headers**, and with that report the
tier gate can finally rank packages.

**33 of 146 applicable SDK packages are shippable — 28 ready, 5 assisted.** None are "emitted but never
compiled" any more; 112 are partial, 1 deferred, 4 not applicable.

Ready, needing nothing: `abc`, `accessibility`, `adjustments`, `binpack`, `camera-controls`, `clipboard`,
`color`, `compression`, `device`, `encoding`, `entity`, `geolocation`, `haptics`, `image-codec`,
`importdiagnostics`, `ipc`, `keyboard`, `math`, `mediasession`, `motionpath`, `permissions`, `platform`,
`protocol`, `sensors`, `shell`, `spring`, `webcam`, `xml`.

Assisted, each naming what carries it: `camera` (flattened-union respell), `clock` (reference-alias
respell), `geometry` (release-function alias), `screen` (a source patch), `timeline` (a source patch plus
a reference-alias respell).

Two things in that list are worth drawing out.

`log` is **not** shippable, and should not be. Its three headers compile, but they compile because an
override stubs nine functions to throw and declares three the emitter never wrote. A gate that counted
"compiles" as "works" would have called it ready; this one counts refusals, so a module whose functions
throw stays partial. That is the taxonomy doing its job.

`webcam` is ready. It was named earlier as the reason applicability must be read from
`flight.environment` and never inferred from a package name — an inference would have parked it as
web-only. It compiles on a native SDL host, and now it ships.

### The sharpest remaining target: fully emitted and still failing

A package with **no refusals** cannot have a never-generated function behind its diagnostics, so whatever
it reports is the real blocker. 45 packages are fully emitted; 31 of them compile completely. The other
twelve are therefore the highest-value work in the corpus, and they are small:

| package | headers | blocker |
|---|---|---|
| `types` | 407/411 | `DomTextureResolver` is not a member of `flight::types` |
| `bitmap` | 41/44 | `optional<Array<double>>::optional(<brace-enclosed initializer list>)` |
| `path` | 23/31 | `Array::copy_within` missing from the runtime |
| `host` | 2/6 | `Any::has_value` missing from the runtime |
| `net` | 2/5 | `log_once` designated initializer — **fixed** this pass |
| `registry-catalog` | 1/4 | `SequenceView::map` missing from the runtime |
| `socket` | 1/5 | calling a `std::function` with the wrong argument shape |
| `statechart` | 1/5 | `ErasedRef` will not convert to `shared_ptr<void>` |
| `flow` | 0/4 | **fixed** this pass — now 4/4 |
| `path-formats` | 0/3 | same braced-initializer shape as `bitmap` |
| `registry-codegen` | 0/3 | `SequenceView::map` |
| `requirements` | 0/4 | `RequirementFacet` alias to `String` misused |

### Missing runtime members are the preferred fix, and there are only five

`AGENTS.md` ranks extending the runtime above every patching mechanism, and the failure set names exactly
what is missing:

| member | headers |
|---|---|
| `SequenceView::map` | 6 |
| `Any::has_value` | 7 |
| `Array::copy_within` | 3 |
| `String::search` | 2 |
| `StructuralRef::has_value` | 1 |

Three have exact JavaScript semantics and an existing house pattern to follow: `Array::copy_within` is
`Array.prototype.copyWithin` and `normalize_boundary` is already the runtime's relative-index clamp;
`SequenceView::map` mirrors `Array::map`; and `String::search` needs care, because JS `search` must not
disturb `lastIndex` while `RegExp::exec` updates it for a global pattern — so it goes through
`std::regex_search` directly and computes the index the way `exec` does, in UTF-16 code units rather than
bytes.

`Any::has_value` is the largest and is deliberately last: `flight::Any` carries `unknown`, so what
`has_value` means on it is an absence question, and absence is the one thing this repository will not
decide casually.

## Three runtime members, and what they unblocked

`AGENTS.md` ranks extending the runtime above every patching mechanism, and the failure set named exactly
what was missing. Three were added, each with the semantics pinned by tests in `tests/runtime_test.cpp`:

- **`Array::copy_within`** — `Array.prototype.copyWithin`, over the existing `normalize_boundary` clamp
  this class already uses for `fill` and `slice`. The **overlap direction is load-bearing**:
  `copyWithin(0, 3)` shifts down and reads ahead of the write cursor, while `copyWithin(2, 0)` shifts up
  and must read behind it. A forward `std::copy` yields `[1,2,1,2,1]` where the specification says
  `[1,2,1,2,3]`, so that exact case is a test.
- **`SequenceView::map`** — mirrors `Array::map`, index argument included, so emitted code written against
  an Array works unchanged over a read-only view.
- **`String::search`** — deliberately **not** implemented through `RegExp::exec`. JS `search` ignores
  `lastIndex` and leaves it as it found it, while `exec` on a global pattern both reads and writes it, so
  this goes through `std::regex_search` directly. The index is in UTF-16 code units, converted the way
  `exec` converts its own; a test with a two-byte character pins it, because a byte offset would report 3
  where JavaScript reports 2.

`Any::has_value` is the largest remaining gap at 7 headers and is deliberately untouched: `flight::Any`
carries `unknown`, so what `has_value` means on it is an absence question, and absence is the one thing
this repository will not decide casually.

### A fourth defect found behind them

With `copy_within` and `SequenceView::map` in place, `path`'s diagnostics moved on to this:

```cpp
return std::optional<flight::Array<double>>{std::make_tuple(a, b)};
```

A TypeScript tuple is an array at runtime, and the emitter agrees when it writes the **type** — then
builds the value with a `std::tuple`, which has no conversion to `flight::Array`. The repair reads the
declared value type out of the text and constructs that, so it recovers the emitter's own stated intent
rather than choosing a representation.

Reading the declared type is also what makes it safe, and that is the whole design. Three of the tree's
`std::make_tuple` sites are declared `std::optional<std::tuple<double, double, bool>>`, where
`make_tuple` is correct and a rewrite would break working code. A rule shaped like "every `std::make_tuple`
inside a brace" would have hit them; keying on the value type skips them for a reason that is *checked*
rather than remembered — it does not begin with `flight::Array<`.

Result: `path_formats` **0/3 to 3/3**, `path` **23/31 to 26/31**.

### Two green results that measured nothing

Both worth recording, because each looked like success:

`ctest` reported **7/7 passing on a stale binary** after the build had failed on `get_index` (that is the
typed-array accessor; `Array` uses `element`). The fix is trivial; the lesson is that a passing test run
after a failed build is not evidence. So one assertion was then deliberately broken to confirm the new
tests execute at all — `FAIL: copyWithin shifting up handles the overlap backwards`, exit 1 — and
restored.

A verification loop reported **all 34 `path` headers failing with one error each**, while a single direct
compile of one of them succeeded. The harness was wrong, not the code: the header list already held
`flight/path/...` and the loop's `printf '#include <flight/%s>'` added a second prefix, so every
compilation failed with `flight/flight/path/clean_path.hpp: No such file or directory`. It was caught only
because "all 34 fail identically" contradicted "one compiles fine", and the contradiction was worth
chasing instead of explaining away.

### `path`'s last blocker is a module-private interface crossing a module boundary

`stroke_path.hpp` declares `flight::Ref<issue_issue_subpath_pieces_704ed4447de90a30>` and assigns
`build_stroke_path_geometry(...)`, which returns `Ref<StrokePathGeometry>`.

`StrokePathGeometry` is **not exported** from `strokePathGeometry.ts` — it is module-private. When
`strokePath.ts` imports a function returning it, the emitter cannot name the private interface across the
module boundary, so it synthesises an anonymous structural struct for the same shape. The two are
structurally equivalent but **not byte-identical**: the named one spells its fields
`StrokePathTessellationIssue` and `Array<Ref<StrokePathPieceGeometry>>`, the anonymous twin spells them
`double` and `Array<Ref<closed_end_cap_left_right_start_cap_a41f9e2ea90d2c0a>>`.

That is why the derived duplicate-struct aliasing does not fire: it requires byte-identical bodies, on
purpose, because that is what makes it safe without type analysis. Closing this one needs structural
equivalence across differently-spelled-but-equivalent field types, which is a transitive alias problem and
genuinely harder than anything else in this file. Recorded, not attempted.

## The refusals are the frontier, and they are better distributed than expected

Two measurements over the full-tree report settle where the remaining work is.

**The name class is not the frontier.** It is 73.5% of all failures (753 of 1024 headers), and **748 of
those 753 sit in packages that already have refusals**. Only 5 are in a fully emitted package. That
reconciles the two earlier results that looked contradictory: names resolve 95.5% of the time, and
resolving them gains nothing, because they are almost always standing in front of a refusal that needs
hand-written code anyway.

**The refusals are concentrated in ones and twos:**

| refused modules | packages |
|---|---|
| 0 | 45 |
| **1** | **42** |
| 2–3 | 28 |
| 4–9 | 21 |
| 10–24 | 10 |
| 25+ | 4 |

Forty-two packages are a single module away from full emission, and three of those already have **every
header compiling** — `easing` 23/23, `particles` 13/13, `filesystem` 3/3 — held back only by the refusal
count.

## `easing` ships: one function, hand-written

`easing`'s single refusal is one declaration, `createEasingSamples`, and the refusal is about the
**signature** rather than the body: `out?: Float32Array` returning `Float32Array` gives two union domains
— the supplied array and a freshly allocated one — landing on one carrier with no discriminator. Refusing
that in general is right, because the compiler cannot know the two are meant to be the same object.

Here they are, and the TypeScript says so: *"Returns the output array (always the same object as `out`
when one is supplied)."* `flight::Float32Array` is a handle onto shared storage, so returning the supplied
array returns that object rather than a copy — which is what makes the guarantee hold rather than merely
appear to.

The three behaviours the source comments single out are the three worth checking, and all three are
preserved: `count=1` samples the **midpoint** `ease(0.5)` and not `ease(0)`; the endpoints are reassigned
*after* the loop to clamp away floating-point drift in `i * step`; and `t` is read into a local before the
write, which the source marks as alias safety because `out` may be a view over memory the easing function
also reads. `Number.isFinite` lowers to `std::isfinite`, which is how the emitter spells it everywhere
else in the tree — checked rather than assumed.

Result: **23/23 modules, 23/23 headers**.

## The tier gate could not see overrides at all

Landing that exposed a hole in the gate. `complete` demanded `refusedModules === 0`, and overrides were
never consulted — so a package whose only gap was supplied by hand stayed `partial` for ever, and the
override mechanism, one of the four declared mechanisms, could never show progress. That defeats the point
of having it.

The fix is an explicit claim rather than an inference. An override may declare `supplies`, naming the
refused module it faithfully implements by its source path, and the gate then counts that package's
refusal as answered and ranks it `assisted` — never `ready`, because it carries debt.

`supplies` is deliberately **a stronger claim than `status`**, and separate from it, because
`status: complete` only means the override compiles — which a file full of functions that throw also does.
`flight/log/log.hpp` is exactly that file: it compiles, and nine of its functions throw. It must never
carry `supplies`, and the loader refuses the claim on any override that is not `complete`. Measured:

```
@flighthq/easing     assisted   supplies: easing-create-easing-samples
@flighthq/log        partial    supplies: (none)
```

`log` staying partial is the gate working. **34 of 146 applicable packages are now shippable.**

### Why `particles` and `filesystem` are not the next `easing`

They have the same *shape* — one refused module, every other header compiling — and a very different cost,
which is worth writing down so nobody starts them expecting `easing`.

`easing` was **one function, twenty lines**, and the refusal was about a signature the TypeScript itself
explained. These are not:

| package | refused module | TS lines | what is missing |
|---|---|---|---|
| `particles` | `validateParticleEmitterConfig.ts` | 242 | **five** functions, including the module's main export |
| `filesystem` | `filesystem.ts` | 433 | the **whole module** — "no part of it was generated" |

`particles` refuses under two distinct rules at once — `cpp-erased-record-assertion-unrepresented` for the
module and `cpp-intersection-member-shapeless` for `reportCurve` — with the stated cause that
`Partial<T>` needs a statically resolvable C++ object shape. `filesystem` refuses under
`cpp-indexed-access-unlowered` and emitted nothing at all, so its placeholder is a surface declaration
with no bodies behind it.

Each is a few hundred lines of careful transcription where a subtle divergence would be invisible, which
is a different kind of task from `easing` and should be costed as one. The `easing` play — read the refused
function out of the comment, transcribe it, declare `supplies` — repeats cleanly only where the refusal is
*one* declaration whose invariant the source states. Of the 42 single-refusal packages, how many are that
shape is not yet measured, and measuring it is cheaper than attempting any of them: the count of
`missing: function` lines in each placeholder header is the whole estimate.

### Measured: the `easing` play was almost unique

The previous paragraph said the count of `easing`-shaped packages was not yet measured and that measuring
it was cheaper than attempting any. It was, and the answer is small enough to change the plan.

Across the whole tree, **187 refused modules are missing exactly one function** — which looks like the
`easing` play repeating 187 times. It does not, because the package around the module also has to compile.
Intersecting "exactly one refused module" with "every header already compiling" leaves **three** packages:

| package | modules | headers | missing functions |
|---|---|---|---|
| `easing` | 23 | 23/23 | 1 — **done** |
| `particles` | 13 | 13/13 | 5 |
| `log` | 3 | 3/3 | 22 — already stubbed, deliberately |

So `easing` was not the first of a series; it was very nearly the only one of its kind at this pin. The
other 41 single-refusal packages have failing headers *as well*, which means an override alone does not
finish them — they need the header failures closed first, and those are the name class and the conversion
family, which is where the earlier measurements said the leverage is not.

That is a more useful thing to know than another finished package would have been. The next real increment
is not more hand-written modules; it is whatever closes header failures across the 111 partial packages,
and the honest answer from the cause map is that no single repair does — 748 of 753 name-class failures sit
in front of a refusal.

## `types` goes clean, and `particles` ships

### `types`: one defect, two shapes

`@flighthq/types` is the largest package in the SDK — 995 modules, 411 headers — and everything depends on
it. It was 406/411, and both causes were the same emitter defect: **aliases written into the
forward-declaration prologue, which sits before the include block.** That placement is fine for a struct,
whose name can be forward-declared, and wrong for an alias, whose definition names types the includes have
not brought in yet.

`FlightDocumentValue` needed only a forward declaration — and that a forward declaration *suffices* is
measured, not assumed: the alias `using FlightDocumentLayoutNode = Ref<LayoutNode<Record<String,
FlightDocumentValue>, ...>>` never instantiates `Record` or `LayoutNode`, so the incomplete type is never
examined. Tested before the repair was declared, seven diagnostics to zero.

The three texture resolvers close a real cycle, and no forward declaration can reach them:

```
dom_texture_resolver.hpp   defines  using DomTextureResolver = std::function<...>;
                           includes dom_render_state.hpp
dom_render_state.hpp       uses     KeyedTable<flight::types::DomTextureResolver>
```

Whichever is parsed first needs the other, and **an alias has no forward declaration**. A new kind,
`repeat-alias-declaration`, repeats the identical alias in the file that needs it. That is legal C++ and
introduces no new type, name or behaviour — and crucially, **if the two declarations ever disagreed the
compiler would reject the file, so the language enforces the equivalence rather than the entry asserting
it.** That is a stronger guarantee than any other repair in this file has.

Placement is load-bearing, which is why this is not a variation of `insert-using-declaration`: the alias
goes at the **end** of the prologue block, after the struct forward declarations, because it references
them. Inserting at the start — where the using-declaration repair inserts — would put the alias above its
own dependencies. `GlTextureResolver` additionally needs `struct GlTextureRealization;` alongside it, so
the declaration carries both.

All five previously failing headers now compile, and the eight files the forward declaration incidentally
touched still compile.

### `particles`: delegated, and verified rather than accepted

`builder` implemented the five refused functions in `validateParticleEmitterConfig` (490 lines). It was
reviewed independently, because `supplies` is the claim that decides shippability and a wrong one ships a
lie. Three checks were worth the time:

- **`derivedFrom` matches byte-for-byte**, so both clones worked from the same regenerated inventory.
- **`regionIdMax`** is the line a careful transcription still gets wrong. The TypeScript clamps from
  `out.regionIdMin` — the *pre-clamp* field — not from the normalized one it has just written. The
  override reads from `out`. The two values coincide at this pin, so getting it backwards would have been
  invisible.
- **Numeric stringification matches JavaScript, not C++.** Verified directly: `flight::to_string` gives
  `NaN`, `Infinity`, and `3` for `3.0` — not `nan`, `inf`, `3.000000` — so the interpolated
  `(got ${value})` message text is identical. And `curve.length % stride !== 0` as
  `std::fmod(...) != 0.0` is right, since JS `%` on numbers *is* `fmod` and `-0.0 != 0.0` is false in
  both languages.

**35 of 146 applicable packages are shippable — 27 ready, 8 assisted.**

### A known imprecision in the tier gate

Landing those moved `@flighthq/permissions` from `ready` to `assisted`, and that is an artifact rather
than a change in the package. `assisted` currently means *a repair touched this package*, not *this
package needed it*: the new `conditional-absent-branch-optional` repair rewrote a ternary in `permissions`
— plausibly one inside a template that was never instantiated, since the package compiled before the
repair existed.

Distinguishing the two would mean compiling each package with and without each repair that touches it,
which is expensive and has not been done. So the count understates `ready` by at least one, and
`assisted` should be read as "carries declared debt, which it may or may not depend on". `easing` and
`particles` are genuinely assisted — remove their overrides and the packages do not build.

## Measured: what the five repairs and five runtime members were worth

Full compile over the regenerated tree, against the pre-repair baseline of 1686/2710:

**1714 of 2710 headers, +28.** And **39 of 146 applicable packages shippable (29 ready, 10 assisted)**,
up from 35 (27 ready, 8 assisted).

| package | before | after | |
|---|---|---|---|
| `path-boolean` | 0/10 | 7/10 | +7 |
| `flow` | 0/4 | **4/4** | +4 |
| `bitmap` | 41/44 | **44/44** | +3 |
| `path-formats` | 0/3 | **3/3** | +3 |
| `registry-catalog` | 1/4 | **4/4** | +3 |
| `path` | 23/31 | 26/31 | +3 |
| `collision`, `font`, `physics3d`, `shape`, `textshaper` | | | +1 each |

`path-boolean` at +7 was not predicted — it was 0/10 and never appeared in any of the targeted analysis.
It uses the same `std::make_tuple`-for-an-array construction as `path`, so the tuple repair reached it for
free. Worth noting because the targeted work was aimed at `path` and `bitmap`; a third of the gain came
from a package nobody looked at.

Four packages went from partial to complete on headers: `flow`, `bitmap`, `path-formats`,
`registry-catalog`.

Not in this measurement: the five `types` headers fixed by `types-flight-document-value-forward-declaration`
and the three `repeat-alias-declaration` entries. Those were declared after the regeneration, so the
committed tree does not carry them yet — another +5 and `@flighthq/types` going ready are pending the next
`npm run sdk:generate`.

## A repair built, measured, and reverted: pairing structs by shape

The conversion family is 85 headers. Classified in detail:

| cause | headers |
|---|---|
| structural row seam | 37 |
| distinct struct identities | 25 |
| other | 19 |
| `Record<String, Any>` from a brace list | 3 |

That breakdown redirected the work twice.

**It killed a runtime addition before it was built.** `@flighthq/host` fails because a `.map(...)` result
— an `Array<String>` — will not become a `flight::Any`, and `Any` can only hold an object through a
`shared_ptr`. Adding array support to `Any` looked worthwhile until the count said **three headers**.

**And the detailed grouping produced artifacts of my own tooling.** It first reported
`Texture2D → Texture2D`, `AmbientLight`, `ClipRegion` and `Material` as distinct-identity pairs. They are
not: the compiler abbreviates `flight::types::StandardMaterial` to `types::StandardMaterial` in the same
diagnostic, and my regex read the two spellings as two types. Those rows were row-seam cases misfiled by
the classifier, not findings.

### The real cluster, and the generalisation that failed

`flight::types::WgpuRenderStats` and `flight::scene2d_wgpu::draw_call_count_..._9fe60b6367cb7daf` have
**byte-identical bodies differing only in the struct name** — a module-private interface the emitter
cannot name across a module boundary, synthesised as an anonymous twin. The derived aliasing pairs structs
by NAME, so it never sees them.

Pairing by BODY was implemented. The argument for it felt solid: TypeScript is structurally typed, so two
interfaces with identical members are mutually assignable by definition, and merging them is what the
source language already says they are.

It worked, and then what it chose was the problem:

```
{a, b, c, d, tx, ty}  ->  SwfTagMatrix     (the twin is used by render_wgpu, swf, shape_formats, scene2d_wgpu)
{r, g, b, a}          ->  UnityColor
```

`{a,b,c,d,tx,ty}` is the shape of every 2D affine matrix in the corpus, and the only *named* type in
`flight::types` holding it is `SwfTagMatrix`. So a WGPU shader's transform became an SWF tag matrix.

The argument was **true and insufficient**. Structural assignability makes such an alias type-correct; it
does not make the chosen NAME correct, and the name is what every diagnostic, debugger and future reader
sees. Which name wins is also an accident of the corpus — whichever named type happens to be the unique
holder of that shape. Reverted.

Three self-inflicted failures on the way there, each reporting a confident `0 aliased`: the scratch tree
held `render_wgpu` when the twin lives in `scene2d_wgpu`; `body` in the existing code means the whole
struct text *including the name*, while the new index held member text only, so they could never compare
equal; and the early return tested the same-name index and bailed before the new path ran.

### What replaced it, and what it was actually worth

A declared kind, `alias-anonymous-struct-to-named`, one verified pair at a time. `WgpuRenderStats`
qualifies on evidence that is read rather than computed: byte-identical members, the same four names in
the same order, and a consuming package (`scene2d_wgpu`) in the same WGPU render-statistics domain as the
named type.

**It gained one header** — `scene2d_wgpu` 9/33 to 10/33. The conversion appears 15 times in the
diagnostics, which is occurrences and not headers; the rest of that package fails on unrelated causes
behind it. The repair is kept because it removes a real failure and the pair is verified, but the 15 is
not its value, and the overstated claim has been corrected in the repair's own `defect` field.

That is the fourth time this session a correct fix gained nothing or nearly nothing. The rejected
body-matching generalisation is recorded inside the repair so nobody rebuilds it.

## `filesystem`: 40 of 44, and the four that were refused on purpose

`builder` supplied the module. 501 lines, 40 of the 44 declarations, and **`supplies` deliberately
omitted** — the manifest records `status: incomplete` with the gap named: `read_dialog_handle_binary_file`
and three siblings need `getFileDialogHandleOperations`, which lives in a *separately refused* module, and
`FileDialogHandle`'s generated carrier exposes no checked projection to it. Returning null or false would
change behaviour whenever operations are present.

That is the right call, and the mechanism is built to record it: the four omissions have **no callers
outside the package**, so nothing breaks today, and the package stays `partial` rather than claiming a
tier it has not earned.

The abort boundary is faithful in the order that matters — rejected with `signal.reason.snapshot()`
*before* any host callback runs, resolved with the source's default for a missing host member:

```cpp
if (signal.has_value() && signal->aborted) return detail::reject_aborted<bool>(*signal);
const auto append = detail::member(host_file_system, &HostFileSystemCapability::append_text_file, "appendTextFile");
if (!append.has_value()) return flight::Task<bool>::resolve(false);
```

One correction to the brief I wrote: I said the `signal === undefined ? f(a,b) : f(a,b,signal)` ternary
"selects a different host arity" and had to be preserved. In TypeScript it does. In the C++ lowering the
host callback is a single `std::function` taking `std::optional<AbortSignal>`, so there is no arity to
select and both branches are identical — the ternary is harmlessly redundant either way.

## A correction that inverts the central inference: the name class is a SYMPTOM of the refusal

`builder` supplied one refused declaration in each of two packages. I predicted no movement, told builder
so, and cited these exact numbers as the reason. Both went complete:

| package | before | after |
|---|---|---|
| `@flighthq/font` | 5/9 | **9/9** |
| `@flighthq/texture-formats` | 7/11 | **11/11** |

The prediction rested on an inference recorded earlier in this file, and the inference was wrong.

**What was measured** (and still holds): 748 of 753 name-class failures sit in packages that already have
refusals.

**What I concluded from it**: that those packages therefore need *both* halves — an override for the
refusal AND repairs for the name failures — so an override alone would not move them.

**What the two packages actually show**: the name failures were *downstream of the refusal*. Look at what
the four failing `font` headers were reporting:

```
'infer_font_format_from_url' is not a member of 'flight::font'
```

That is the refused function itself. The barrels and the two real consumers all failed because the
declaration did not exist — not because of anything a repair could address. Supplying the module fixed
all four. `texture-formats` is the same shape one step removed: its four headers reported
`'TextureContainerParseFailureReason' was not declared in this scope`, a name the refused module declares,
so writing the module brought the name with it.

So the correlation was real and the causation ran the other way. In a package whose refusal withholds a
declaration that its own barrels and siblings reference, **the override is not half the fix, it is the
whole fix**, and the name-class diagnostics are the refusal's shadow rather than an independent problem.

This reprices both queues. Hand-written modules are worth more than this file previously claimed, and
repairing unqualified names in a package that has a refusal is worth less — frequently nothing, because
supplying the module removes the diagnostic anyway. The earlier result that resolving 56 names across
three packages gained zero headers now reads differently too: those names were waiting on refusals, and
qualifying them was never going to help.

What survives unchanged: "was not declared in this scope" is what gcc reports first and almost never what
is really wrong. The lesson is the same; the remedy is the opposite of the one I inferred.

## Hand-written modules are outperforming repairs, measured

Six modules supplied by `builder`, each verified here independently against the same regenerated
inventory (digests matched byte-for-byte in every case):

| package | headers before | after | |
|---|---|---|---|
| `particles` | 13/13 | 13/13 | the refusal was the only gap |
| `font` | 5/9 | **9/9** | complete |
| `texture-formats` | 7/11 | **11/11** | complete |
| `textbidi` | 4/8 | **8/8** | complete |
| `filesystem` | 3/3 | 3/3 | 40 of 44 declarations; gap declared, `supplies` omitted |
| `font-formats` | 13/17 | 13/17 | no gain, and reported as none |

Against that, four of my repairs this session gained zero or one header. The asymmetry is consistent with
the correction recorded above: name-class diagnostics are mostly the refusal's shadow, so supplying the
module is often the whole fix while repairing names around it is often nothing.

Two things `builder` did that are worth keeping as the standard for this work. It reported `font-formats`
as **no gain** after measuring both ways, naming the real blocker rather than claiming the headers its
override touched. And on `filesystem` it stopped at 40 of 44 declarations and left `supplies` off, because
the remaining four need a function from a *separately refused* module and returning a default would change
behaviour when operations are present. An honest gap beats a stub that claims completion, and the manifest
is built to record exactly that difference.

## A repair that reported success and did nothing, and the regression fixing it caused

`font-formats` depends on `data_tag_aef43e71dd1e9a6d`, which `woff_font.hpp` constructs and casts to
`data_tag_3ad9a8f109659685` — and which is **defined nowhere in the tree**. The emitter hashed one declared
TypeScript type under two names and emitted a definition for only one. The source settles it:

```ts
interface WoffTable { data: Uint8Array; tag: number; }
```

exactly `data_tag_3ad9a8f109659685`'s `flight::Uint8Array data; double tag;`. Since the cast requires the
two to BE one type, a second definition cannot work and an alias is the fix. Unlike the reverted
shape-matching, this introduces no misleading name — both spellings are anonymous hashes of the same shape
in the same module — and the correspondence is read from the source, not inferred from matching bodies.

Then the placement went wrong twice, in opposite directions, because the emitter writes two file shapes:

|  | `flight/types/dom_render_state.hpp` | `flight/font_formats/woff_font.hpp` |
|---|---|---|
| shape | prologue block of forward declarations, then aliases, then a second block with definitions | one block, code from the first line |
| end-of-block | correct | **useless** — alias at line 133, used at line 59 |
| start-of-block | **wrong** — alias above `struct DomRenderState;`, which this file defines | correct |

End-of-block was the original. It matched `woff_font.hpp`, reported `touched: 1 file`, satisfied the expiry
check, and changed nothing — **the one failure mode here that looks like success**. Every other mistake
this session surfaced as a wrong number or a compile error.

Start-of-block was the fix, and it regressed `font-formats` from 13/17 to **11/17** by breaking the `types`
headers it includes: `'DomRenderState' is not a member of 'flight::types'`, plus a conflicting declaration.
That was caught only because the measurement covered both shapes rather than the one being fixed — the
`types` resolvers were already banked, so the loss would have been silent.

The anchor is now the end of the leading run of forward declarations inside the block: line 20 in
`woff_font.hpp`, line 48 in `dom_render_state.hpp` directly after `struct DomRenderState;`. All four
`types` headers at zero errors, `font-formats` back to 13/17.

**The alias still gains nothing.** `woff_font.hpp` now fails on a `std::variant<bool, double, String>`
construction inside `report_import_diagnostic`, which is a third variation of the brace-initialised-union
family and not reachable by either existing repair — `in_place_type` followed by a brace list no longer
appears anywhere in the tree, because the earlier repair rewrites all of those at generation time. Recorded
rather than chased.
