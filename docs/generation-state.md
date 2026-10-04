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
| `types` | 990/995 [^1] | `DomTextureResolver` is not a member of `flight::types` |
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

`@flighthq/types` is the largest package in the SDK — 995 modules and 995 headers — and everything depends
on it. It was 990/995, and both causes were the same emitter defect: **aliases written into the
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

## The synthesis: which half is the whole fix depends on what the refusal withheld

`builder` audited the five modules whose refusal reports **zero** `missing: function` lines, before writing
anything, and the result corrects an assumption recorded earlier in this file.

I had written that `refused-placeholder` means the file is a replaceable stub. For these five it does not.
The placeholder retains the **complete source surface**; the refusal is recorded at module level only and
nothing is withheld. The decisive observation is `builder`'s: `PARTIAL` / `NOT GENERATED` markers appear
only where a declaration is *actually* absent, so a zero marker count is **positive evidence that there is
nothing to supply** — not an anomaly to investigate. Four of the five would have been overrides nobody
needed.

Those four fail first on ordinary qualification. Measured, all four:

| package | before | after | |
|---|---|---|---|
| `spatial` | 8/11 | **11/11** | complete, on one using-declaration for `SpatialIndexingNotice` |
| `textshaper` | 7/10 | 7/10 | no gain |
| `bitmapfont` | 3/8 | 3/8 | no gain |
| `glyphatlas` | 5/10 | 5/10 | no gain |

One of four paid, and `builder` called all four correctly in advance: `spatial` was described as
"using/include repair territory, not an override", and `textshaper`, `bitmapfont` and `glyphSource` each
as structural with "move on". Its predictions about which repairs would NOT help were as accurate as the
one about which would — the three that gained nothing are blocked behind exactly the structural defects it
named, one layer down.

So the two halves are not interchangeable, and which one is the *whole* fix is predictable from what the
refusal withheld:

- **The refusal withheld declarations** → the override is the whole fix, and repairing names around it
  gains nothing. `font` 5/9→9/9, `texture-formats` 7/11→11/11, `textbidi` 4/8→8/8.
- **The refusal withheld nothing** → the qualification repair is the whole fix, and there is nothing for
  an override to supply. `spatial` 8/11→11/11.

That resolves the tension between two earlier records in this file — "hand-written modules are
outperforming repairs" and "the name class is the refusal's shadow". Both were measured correctly; neither
was the general rule. The marker count tells you which case you are in, and it is free to read.

`builder`'s structural findings behind the other three are recorded for the queue rather than worked
around: `textshaper` stores `Ref<TextShaperCacheRuntime>` through an `EntityRuntime` cell whose flattened
owners have no heritage; `bitmapfont` needs an owner-preserving `Bitmap → TextureSource` conversion;
`glyphSource` produces `optional<Ref<Bitmap>>` where `GlyphSource` requires
`optional<Ref<TextureSource>>`. All three are the asserted-row and interface-heritage families.

One reporting detail worth imitating: `builder` noted that its sweep deliberately excluded existing
overrides, which is why it measured `textshaper` 6/10 and `bitmapfont` 2/8 against this clone's 7/10 and
3/8. A discrepancy explained is worth more than a number that happens to agree.

## `path-boolean` completes, and two corrections to how this file counts

`builder` supplied `martinezKernel.ts` — 849 lines — and the package went **7/10 to 10/10**.

### The costing column was mislabelled

Every costing table above, including the one sent to `builder`, called the marker count
"missing **function** lines". It is not: the marker names whatever kind of declaration was withheld, and
across the tree that is

```
1519  missing: function
 205  missing: variable
   2  missing: interface
   1  missing: type
   1  missing: class
```

`martinezKernel` was costed as "1 function" and the withheld declaration was `class DirectedGraph`, which
is a materially different job — a class with state and methods rather than a free function. The estimate
survived only because `builder` read the marker rather than trusting the summary. The 205 `variable`
markers matter too: a withheld `const` table is cheap, a withheld class is not, and the current tables do
not distinguish them.

### An override copies the REPAIRED text, not the emitter's text

`builder` noted that the copied body "needed its existing Array tuple repair" — that is,
`array-from-tuple-construction` had already rewritten the generated file, and the override, being a copy
of that file, had to carry the rewrite with it.

The mechanism handles this correctly and it is worth stating why, because it looks like duplication. Repairs
run **before** the tree is written, so `derivedFrom` is the digest of the *repaired* file. An override is
therefore derived from the repaired text by construction, and if the repair later changes or is deleted the
digest moves and `overrides:check` reports drift — which is the signal to re-derive. The duplication is
real but it is tracked, which is the whole point of recording `derivedFrom` rather than trusting that a copy
stays current.

What this does mean in practice: a repair and an override that touch the same file are coupled, and the
repair's expiry no longer removes its effect from the tree — the override still carries it. Anyone deleting
a repair as obsolete should check whether an override is shadowing a file that repair used to rewrite.

## The synthesis, corrected: the marker count predicts the OVERRIDE, never the repair

The section above claimed the marker count predicts which half is the whole fix — declarations withheld
means the override, nothing withheld means the qualification repair. The first half holds. **The second
half is wrong**, and `@flighthq/glyphatlas` is the counterexample.

`glyphatlas` is a zero-marker package: `builder`'s audit established its refused modules withhold nothing.
By the stated synthesis, qualification should therefore have been the whole fix. So every unqualified name
in all ten headers was swept in one pass — 13 found, 11 resolvable, declared as three grouped repairs
touching **24 files** — and the result was **5/10 before, 5/10 after**.

What is actually behind the names:

```
conversion from 'flight::IteratorResult<double>' to 'flight::Ref<flight::glyphatlas::done_…>'
'flight::Ref<flight::types::Bitmap>' has no member named …
```

An iterator-result against the emitter's anonymous `{done, value}` shape, and a member access through a
`shared_ptr`. Neither is reachable by qualification.

Tallying all four zero-marker packages honestly:

| package | zero markers | qualification fixed it? |
|---|---|---|
| `spatial` | yes | **yes** — 8/11 to 11/11 |
| `textshaper` | yes | no — structural behind it |
| `bitmapfont` | yes | no — structural behind it |
| `glyphatlas` | yes | no — structural behind it |

One of four. So the defensible rule is narrower than what was written, and it is the half that was actually
established by evidence:

> **A zero marker count means an override has nothing to supply.** That is positive evidence and it saved
> four overrides nobody needed. It says nothing about whether a *repair* will help; only measuring does.

This is the second generalisation in this file walked back after over-reaching from a small sample — the
first was inferring from "748 of 753 name failures sit behind refusals" that both halves were always
needed. Both times the measurement that contradicted it was cheap and available. The pattern worth
internalising is not either specific rule but that a mechanism-level explanation derived from two or three
packages is a hypothesis, and this corpus has 150.

`glyphatlas`'s three grouped name repairs are kept: they match 24 files, remove real undefined-name
errors, and are prerequisites for whatever closes the structural defects behind them. But they gain no
header today and are recorded as such.

## The structural-row family, localised to one line

This is the largest remaining cluster and four independent paths converge on it, so it is worth stating
exactly where it lives.

**Size.** 46 headers whose *first* diagnostic names a structural row, 37 of them "will not widen or
convert":

| package | headers | | package | headers |
|---|---|---|---|---|
| `physics2d` | 15 | | `requirements` | 4 |
| `scene2d-resources` | 7 | | `loader` | 3 |
| `node` | 5 | | `registry-codegen` | 3 |
| `lighting` | 4 | | six others | 1 each |

Plus the packages whose *remaining* failures terminate here once their names are qualified:
`textshaper`, `bitmapfont`, `glyphatlas`, and `textlayout`'s last three — the last identified by
`builder`, which is what made the convergence visible.

**Where it lives.** The runtime already has a widening path. `row_objects_convertible` admits
`generated_row_widening_proven_v<To, From>`, a trait the generated structural member table specialises
for every pair it can prove, over the key set collected from every `flight::RowKey<"…">` spelling in the
tree. The proof itself is one macro, `FLIGHT_SDK_ROW_WIDENS`, and the refusal is one clause of it:

```cpp
else if constexpr (!std::same_as<std::remove_cvref_t<decltype(std::declval<Base&>().member)>,
                                 std::remove_cvref_t<decltype(std::declval<Derived&>().member)>>)
  return false;
```

**The concrete failure.** `@flighthq/textlayout`'s `rich_text_metrics.hpp` cannot pass a
`Readonly<RichTextData>` row where `compute_text_bounds_width` wants
`Readonly<auto_size_height_width_word_wrap_75a4ff02b472c2de>`. The two subjects agree on three of four
members and differ on one:

```cpp
auto_size_height_width_word_wrap_…:  std::optional<bool> word_wrap;
RichTextData:                        bool                word_wrap;
```

All four keys are present in the generated table, so the proof has everything it needs. It refuses on
exact-type equality alone.

**Why that refusal is stricter than the source language.** In TypeScript the target is
`{ wordWrap?: boolean }` and `RichTextData` has `wordWrap: boolean`; a required property **is** assignable
to an optional one. Reading a `bool` cell through an `optional<bool>` view is sound — the value is simply
always present — and the target here is `RowReadonly`, so nothing can write `nullopt` back into a required
field. The unsound direction is the reverse, and it stays refused.

So the candidate capability is narrow and checkable: admit a base member of type `std::optional<T>`
against a derived member of type `T` **when the target schema is readonly**. That is one clause, it
matches the source language, and it fails closed for a writable target.

It is recorded rather than implemented because a widening rule is exactly the kind of change this file
has twice had to walk back for over-reaching: the soundness argument above needs a semantic test per
direction — readonly target admits, writable target refuses, absence still distinguishable through the
widened view — before it earns the 37 headers it looks worth. The one-line localisation is the durable
part; the clause is cheap once someone writes those tests.

## The two families that remain, both sized

With `builder`'s queue worked through, the residue has resolved into two named families rather than a long
tail. Both are runtime-capability questions, both are localised, and neither should be attempted without
the tests named below.

### 1. Structural row widening — 46 headers

Sized and localised above: one `std::same_as` clause in `FLIGHT_SDK_ROW_WIDENS`, refusing a widening
TypeScript permits (`{wordWrap?: boolean}` accepts `wordWrap: boolean`). Candidate fix is one clause,
admitting an `optional<T>` base member against a `T` derived member **when the target schema is readonly**.

Needs: a semantic test per direction — readonly target admits, writable target refuses, absence still
distinguishable through the widened view.

### 2. The absence channel — 10 headers

| package | headers |
|---|---|
| `node` | 5 |
| `lighting` | 4 |
| `materials` | 1 |

`node`'s `has_clip` / `has_material` / `has_blend_mode`, and — identified by `builder` — `lighting`'s
`scene_lights.hpp`, which "tries to return optional partial-row reads as `variant<T, Null, Undefined>`".
That is the same defect from a second direction, which is what confirms it as a family rather than three
odd sites.

The shape, unchanged from where it was first refused: `row_get` over a `RowPartial` returns a single
`optional<V>`, having already collapsed *null* into *absent*, while the emitter wants three states. The
information is **not recoverable at the call site**, so no repair can close it — and mapping `nullopt` to
`Undefined` would compile and be observationally identical at every current site, which is exactly why it
must not be done. `AGENTS.md` names absence as a semantics no workaround may change.

The real fix is upstream of `row_get`: a partial row's cell has to carry three states. `flight::Presence<V>`
already exists for it in `include/flight/presence.hpp` — unused by the emitter, and with its alternatives
in the opposite order (`variant<Undefined, Null, V>` against the emitter's `variant<V, Null, Undefined>`),
which is probably why it has never been wired up. Closing this is a runtime/emitter contract change, not a
repair, and it is the smaller of the two families.

### What that leaves

Every other remaining failure is either a one-off or sits behind one of these two. The useful consequence:
there is no longer a long tail to triage — there are two capabilities, 46 headers and 10 headers, each
localised to a specific mechanism, each with its required tests written down.

## Correcting the `types` figures: right conclusion, wrong numbers, three times

`@flighthq/types` **is** complete — measured 995/995 with every declared repair applied, from a baseline
of 990/995 taken from the full-tree compile report. The five headers gained are the three
`*_texture_resolver.hpp` (by `repeat-alias-declaration`) and the two `flight_document_*` (by the
`FlightDocumentValue` forward declaration), exactly as predicted.

Every number this file previously attached to that conclusion was wrong, and the sequence is worth keeping
because each error had a different cause.

**"407 of 411."** `411` was never the package size. It came off a *mid-run checkpoint* while the full
compile was at 1356 of 2710 headers: of the `flight/types/` entries attempted *so far*, 407 passed. The
package has 995 headers. A progress snapshot read as a final figure.

**"411/411, complete."** An inference, not a measurement. Thirteen headers were compiled — the five that
had been failing plus the eight the forward declaration incidentally touched — all returned zero, and the
package was declared complete without being swept.

**"990/995 → 993/995, not complete."** The correction was also wrong. The scratch tree used for it had
only four repair IDs applied by hand, and `types-flight-document-value-forward-declaration` was not among
them, so the two headers that repair fixes were failing for want of the repair rather than for want of a
fix.

The instructive part is not the arithmetic. It is that the *correction* was published before the
correction was measured: a result arrived that contradicted the earlier claim, and the contradiction itself
felt like evidence. It was not — it was a second incomplete measurement. A partial measurement that
disagrees deserves exactly as much scepticism as one that agrees, and the rule recorded earlier in this
file ("measure the delta, never the plausibility") was written about the confirming case and then not
applied to the disconfirming one.

[^1]: corrected; the table above was built from a mid-run checkpoint and read 407/411.

## Correcting "46 headers, one clause": the family was a regex bucket, not a family

The section above claims the structural-row family is 46 headers and that one `std::same_as` clause in
`FLIGHT_SDK_ROW_WIDENS` addresses it. **The 46 is a coarse regex bucket and the claim is wrong.** It was
produced by matching any diagnostic mentioning `StructuralRef|RowReadonly|RowOf|RowPartial|RowMerge`,
which catches every failure that happens to involve a row anywhere in a long type, not the ones a widening
rule could fix.

Reclassified by what the conversion actually is:

| | headers | packages |
|---|---|---|
| concrete type → variant alternative | 22 | `physics2d` 15, `node` 5, `lighting` 1, `materials` 1 |
| other conversion involving a row | 15 | `scene2d-resources` 7, `lighting` 3, `loader` 3, two more |
| row WRITE incompatible value type | 7 | `requirements` 4, `registry-codegen` 3 |
| no overload accepts the row | 1 | `shading` |
| other | 1 | `statechart` |
| **row → row widening** | **1** | `lighting/scene_lights.hpp` |

**One.** The widening clause I localised addresses the `Readonly<RichTextData>` →
`Readonly<auto_size_height_width_word_wrap_…>` shape, and across the whole corpus that shape appears in
one header in this report plus `textlayout`'s three remaining failures, which post-date it. So the clause
is worth roughly **four headers**, not 46.

The three genuinely separate families the bucket was hiding:

- **`physics2d`'s 15** are a concrete struct converting into a *variant of structs* —
  `shared_ptr<x_y_radius_kind_…>` into `variant<shared_ptr<min_x_min_y_…>, …>`. A discriminated union of
  collider shapes where the emitter passes one alternative's type where the union is wanted. Nothing to do
  with rows or widening; it is its own defect and the largest single cluster left.
- **The absence channel, 10** — `node` 5, `lighting` 4, `materials` 1 — unchanged and still correctly
  refused.
- **Row writes, 7** — `requirements` 4, `registry-codegen` 3 — the `static_assert` on an incompatible
  cell value type, which is a different mechanism from the read-side widening proof.

This is the third over-reach of this session and the mechanism was the same every time: a plausible
mechanism-level story attached to a number that came from pattern-matching rather than from reading the
cases. The earlier two were inferring that both halves are always needed, and that a zero marker count
predicts a repair will help. The cheap check that would have caught all three is the same — group by what
the diagnostic actually *says*, then read three of them.

What survives: the localisation itself is still correct and still valuable. The clause is real, the
soundness argument holds, and `physics2d`'s 15 is now identified as the biggest target. Only the sizing was
fiction.

## `physics2d`'s 15 headers are compiler-side, and that is now established rather than guessed

The largest single cluster left is not fixable from this repository. Worth recording in detail, because the
number is the biggest on the board and would otherwise attract effort indefinitely.

The source is one function with a switch:

```ts
function createPhysics2DColliderWorldShape(
  local: Readonly<CollisionBuiltInShape2D>,
): CollisionBuiltInShape2D & Entity {
  case 'circle': {
    const out = allocateEntity<(CollisionCircle2D & { kind: 'circle' }) & Entity>();
    initializeCollisionCircle2D(out, 'circle', local.radius, local.x, local.y);
    return finishEntity(out);
  }
```

The emitter lowers the same intersection **two different ways in the same file**:

```cpp
// the branch's allocation — intersection as C++ INHERITANCE
struct x_y_radius_kind_entity_runtime_key_3b411ce3c09d63df : public flight::types::CollisionCircle2D {
  flight::String kind;
  std::optional<flight::Ref<flight::types::EntityRuntime>> entity_runtime_key;
};

// the return union's alternative — intersection FLATTENED
struct x_y_radius_kind_entity_runtime_key_9d232207bd12a07b : public flight::ReferenceEnabled {
  double x; double y; double radius;
  flight::String kind;
  std::optional<flight::Ref<flight::types::EntityRuntime>> entity_runtime_key;
};
```

Same logical member set, different structural hash, because the hash is over *declared* members and one
inherits `x`/`y`/`radius` while the other declares them. So `finish_entity` returns the inheriting type
where the variant holds the flattened one, and the two are unrelated C++ types.

**Why none of the four mechanisms can close it.** An alias is wrong: these are not two names for one type,
they are two different layouts, one with a base subobject. A conversion is worse: constructing the
flattened struct from the inheriting one copies members and yields a **new object**, where the TypeScript
returns the same one — that is a reference-identity change, which `AGENTS.md` names as a semantics no
workaround may change. A repair cannot reach it because the defect is a type-representation choice, not
text. And an override would have to rewrite the function around the inconsistency rather than supply
anything missing.

The fix is for the emitter to lower an intersection type consistently — either always by inheritance or
always flattened — within a single module. Until then these 15 headers stay refused, and that is the
correct outcome rather than a gap to paper over.

**Consequence for the queue.** The remaining work that IS actionable from here is smaller than the headline
numbers suggested:

| target | headers | nature |
|---|---|---|
| row WRITE incompatible value type | 7 | `requirements` 4, `registry-codegen` 3 |
| `scene2d-resources` row conversions | 7 | other conversion involving a row |
| the row-widening clause | ~4 | one `std::same_as` clause, tests required first |
| `loader`, `lighting` row conversions | 6 | |
| the absence channel | 10 | refused on purpose; needs a runtime/emitter contract change |

That is the honest board: roughly 24 addressable headers plus two refused families, not the 46 + 10 the
earlier sizing implied.

## A union where a row is wanted, and the two branches it has to dispatch between

The `scene2d-resources` cluster's blocker was narrower than "a row conversion". An audio reference is
embedded or external, so the SDK hands a `std::variant` of two unrelated `Ref` types where a row over the
union's common shape — the anonymous `{state}` struct — is wanted. Everything else in the chain already
worked: `SequenceView` projects an `Array` without copying, and the widening proof already passed, both
alternatives declaring `state` at the type the row wants with `FLIGHT_SDK_ROW_WIDENS(state)` present. The
only missing piece was that `StructuralRef` had no constructor from a variant at all.

Two things about closing it were not obvious, and both were found by a test rather than by reading.

**`std::constructible_from` cannot express the constraint.** The natural guard is a fold asserting every
alternative is constructible. It does not work: the `shared_ptr` constructor is viable for *any* pointee
and validates inside `flatten_ref`, which ends in `static_assert(dependent_false<Type>)` — a hard error,
not a substitution failure. So the fold answers yes for alternatives that then fail to compile inside
`structural_ref.hpp`, with the diagnostic landing in the runtime instead of at the call site. The
assertion that caught this was the NEGATIVE one — that an unrelated alternative is *not* convertible.
`detail::variant_alternative_rows_as` is the real predicate, mirroring `row_objects_convertible`'s
disjunction over the alternative's pointee.

**The two qualifying relationships reach the class through different constructors.** This is the part a
single delegation gets silently wrong. When the pointee *inherits* from the subject the pointer itself
converts, so the `shared_ptr` constructor applies and the row keeps a typed object pointer. A
**structural** widening is the case the SDK actually has — `GlTextureRenderTarget extends GlRenderTarget`
flattens to two unrelated C++ structs — and there `flatten_ref` cannot cast the pointer and hard-errors.
That conversion exists only *between rows*, where the subject stays erased behind its owner. So the
alternative is first given a row over its own subject, and that row is then widened.

Writing it as one `StructuralRef(held)` call compiles for the inheritance case and fails for the
structural one, which is the case the constructor was added for. The first version did exactly that, and
the reason it looked finished is worth recording: the earlier error in the real header moved on to an
unqualified name, so the conversion never got instantiated and the defect never surfaced. It surfaced
only against the test fixtures that carry a real widening proof. Collapsing the dispatch back to one
branch reproduces it as `structural_ref.hpp:399: structural reference source has an incompatible object
type` — that mutation is how the test was confirmed to cover it.

A consequence worth knowing before it reads as a bug: a structurally widened row has a **null**
`shared_object()`. That is pre-existing row-to-row behaviour, not something the variant path introduced,
and the tests now assert it on both paths so the two branches cannot be collapsed unnoticed.

**Correction to how that was first written here.** It said the widened row "carries its subject through
the owner", which implied nothing in the row owns the subject — and since `NativeRowOwner` holds its
object only weakly, that would mean a widened row built over a temporary dangles. It does not, and the
distinction is worth stating exactly because the wrong reading would have blocked the row-write repair
below. The row's own erased `object_` is a strong `shared_ptr<void>` and the row-to-row conversion carries
it across. `shared_object()` returns null only because it first tests
`owner_->native_type() != typeid(object_type)`, which fails when the subject is structurally rather than
nominally related — so the handle is populated and simply not retrievable AS `object_type`.
`shared_native_object()` still returns it. A widened row keeps its subject alive, and
`structural_row_test.cpp` now proves it by observing a `weak_ptr` after every other reference is gone.

Two deliberate asymmetries with `row_objects_convertible` are documented at the concept. Its
readonly-partial clause is **absent** because it cannot apply: that clause governs row-to-row conversion
where the object is already erased, while this path must produce a `shared_ptr<object_type>` from the
alternative's own pointer, and a partial target does not change what its subject is. Conversely
`flatten_ref` accepts one shape this refuses — a nested reference, via its `*value` branch — and the
emitter does not put nested references in a union, so recursing for it would be untested generality.

What this does **not** yet establish is that the seven `scene2d-resources` headers compile. The
conversion is fixed and tested; the next error in that cluster is an unqualified name
(`LottieDocumentImportResult`, `AudioResourceFetch`), which is a separate repair and is still owed.

## The row-write array repair is withdrawn: it would change array identity

`project-array-at-row-write` was queued as the next repair — the seven row-write headers
(`requirements`, `registry-codegen`) fail because the emitted write passes an `Array` whose element type
is not the one the row's value type declares, and `flight::array_of<Target>(source)` produces exactly the
array the write wants. It compiles, and the four sites were verified. **It is not a legal repair, and it
is withdrawn.**

The reason is in `array_of`'s own test, which pinned it before there was a use for it. `array_of` keeps
element identity — the elements are the same objects, not clones — but the array it returns is a
**separate handle**: a `push` through the result is not visible through the source. That is the limitation
the test exists to pin, and it is why `array_of` is a free function rather than a converting constructor
on `Array`.

At a row **write**, that separateness is a semantics change. TypeScript's `row.items = arr` stores *that*
array: afterwards `row.items === arr`, and a later `arr.push(...)` is visible through the row. Through
`array_of` the row holds a different array, so the identity comparison is false and the later push is
invisible. `AGENTS.md` lists reference identity among the semantics no workaround may change, and names
why: a workaround that alters one is a claim our own tests would then certify as true. These seven
headers would have compiled, measured as a gain, and been wrong.

The distinction is the SITE, not the function, so this does not retire `array_of`:

- where TypeScript itself produces a fresh array — a `.map`, a spread, a `from` — a fresh array is the
  faithful lowering, and `array_of` is correct;
- where TypeScript assigns or stores an existing array, only an identity-preserving conversion will do,
  and `array_of` is not one.

Worth noting against the repair contract as practised. "May only add text with no behavior" reads like
forward declarations only, but the declared kinds are broader than that: `wrap-conditional-absent-branch`
puts `std::optional{...}` around a present branch, and `name-array-from-tuple-construction` replaces a
`std::make_tuple` call. Both change expression text. What makes them legal is that each only **names the
type the surrounding declaration already requires** — the emitter wrote the type and then built a value
that did not match it. Neither introduces an operation. `array_of` does: it allocates. That is the line,
and it is a sharper test than "is it a declaration".

**What the seven headers need instead.** Identity-preserving is the whole requirement, so the candidates
are narrow. If the two element types are duplicate structural structs for one TypeScript shape, the
answer is an alias and costs nothing — but the derived alias repair only fires on byte-identical bodies
in different packages, so it has to be checked at these sites rather than assumed. If they are genuinely
different types, then TypeScript would not have permitted the assignment either, and the defect is a
type-representation choice in the emitter — the same shape as the `physics2d` intersection family, and
correctly refused rather than papered over. Which of the two it is cannot be read off the heuristic; it
needs the generated sites, so it is deferred to the regeneration now in flight and is NOT counted as
addressable work until then.

The board line `row WRITE incompatible value type | 7` therefore moves out of "addressable" and into
"mechanism unknown pending measurement". That drops the honest addressable count from roughly 24 headers
to roughly 17.

## The absence channel was never a runtime gap — it is three cases, and five of six files compile

The earlier entry refused this family on two premises. Both are wrong, and the correction is worth as much
as the fix.

**Premise 1: "`row_get` over a `RowPartial` has already collapsed null into absent, so the information is
not recoverable at the call site."** False whenever the member can represent null itself. A partial read
of `std::optional<flight::Ref<ClipRegion>>` has three distinguishable states — `nullopt` is the absent
property, a held NULL pointer is TypeScript `null`, and a held non-null pointer is the value. The collapse
is real only for a member with no null of its own, such as `optional<double>`. This is now pinned in
`structural_row_test.cpp` against the committed member table.

**Premise 2: "this is a runtime capability — `row_get` preserving three states — plus the emitter agreeing
on the spelling."** No runtime change is needed and the emitter already has the spelling. It emits a
correct long-form lowering for this exact construct elsewhere in the same tree
(`scene2d_formats/svg_document.hpp`): project the read into a local, map `!has_value()` to `flight::Null`,
and return the value alternative otherwise. At every long-form site in the tree, `nullopt` maps to
`flight::Null` — unanimously, never `Undefined`. So a repair here copies the emitter's own decision rather
than inventing an absence semantics, which is the strongest justification available and a checkable one.
`flight::Presence` is irrelevant to this and stays unused.

**The textual pattern over-matches, which is the trap.** A scan for a bare `return flight::row_get<...>`
inside a lambda declared to return `variant<V, Null, Undefined>` finds 8 sites in 6 files. Applying one
transform to all 8 fixes four files, breaks one that **already compiled**, and fails on one more. The
sites are not one defect but three, separated by facts no text match can see:

| case | subject member | receiver row | `row_get` yields | fix |
|---|---|---|---|---|
| 1 | `optional<Ref<T>>` | `RowPartial` | `optional<Ref<T>>` | the emitter's long form: `nullopt` → `Null`, else the value |
| 2 | `variant<V, Null, Undefined>` | `RowPartial` | `optional<variant<V, Null, Undefined>>` | unwrap: `nullopt` → `Undefined`, else return the held variant |
| 3 | any | NOT partial | the member type itself | nothing — it already compiles |

Case 3 is `scene2d_resources/scene2_ddocument_source.hpp`, which has no `RowPartial` anywhere in it: the
read returns the member type, which is already the three-state variant, so it converts. It was flagged
only by textual resemblance, and the transform broke working code. That is the whole argument against
declaring this repair on a text pattern.

Case 2 is `materials/standard_material.hpp`, whose `name` member is *declared* as
`variant<String, Null, Undefined>`. A partial read therefore yields `optional<variant<...>>`, and the
case-1 transform builds `in_place_type<String>` from a variant and fails. Its fix needs no choice from us
and loses nothing: the member already distinguishes null from a value, so `nullopt` can only mean the
property was absent, which is `Undefined`. That is a forced mapping, not a judgement call.

Measured against the committed tree, compiling each header standalone:

| header | baseline errors | after |
|---|---|---|
| `node/has_clip.hpp` | 1 | compiles |
| `node/has_material.hpp` | 2 | compiles |
| `node/has_blend_mode.hpp` | 1 | compiles |
| `lighting/scene_lights.hpp` | 2 | compiles |
| `materials/standard_material.hpp` | 1 | compiles (case 2) |
| `scene2d_resources/scene2_ddocument_source.hpp` | 0 | untouched; needed nothing |

So the family is two declared repairs anchored to their sites — six sites in four files for case 1, one
site in one file for case 2 — not one pattern and not a runtime capability. The reason this was recorded
as refused for so long is that the refusal reasoning was done against the diagnostic rather than against
the subject's member declaration, and the diagnostic is identical in all three cases.

Both repairs are drafted and verified but NOT yet declared: a regeneration is in flight, and editing
`repairs/` mid-run already cost one 27-minute compile. They land when it does.

### The seven sites, and the exact text each needs

Recorded because the site list is the part that took measurement rather than reasoning, and because the
textual pattern must NOT be used to rediscover it — it finds an eighth site that needs nothing.

Case 1 — six sites in four files. Each is a bare
`return flight::row_get<flight::RowKey<"K">>(optional_chain_receiver.value());` inside a lambda declared
to return `std::variant<V, flight::Null, flight::Undefined>`, where `V` is a `flight::Ref<...>`:

| file | keys |
|---|---|
| `node/has_clip.hpp` | `clip` |
| `node/has_material.hpp` | `material`, `materialData` |
| `node/has_blend_mode.hpp` | `blendMode` |
| `lighting/scene_lights.hpp` | `ambient`, `directional` |

Each becomes the emitter's own long form, with `VARIANT` standing for the lambda's declared return type
and `V` for its first alternative:

```cpp
auto optional_chain_projected = flight::row_get<flight::RowKey<"K">>(optional_chain_receiver.value());
if (!optional_chain_projected.has_value()) return VARIANT{std::in_place_type<flight::Null>, flight::null};
return VARIANT{std::in_place_type<V>, optional_chain_projected.value()};
```

Case 2 — one site, `materials/standard_material.hpp`, key `name`, whose subject member is declared
`std::variant<flight::String, flight::Null, flight::Undefined>`:

```cpp
auto optional_chain_projected = flight::row_get<flight::RowKey<"name">>(optional_chain_receiver.value());
if (!optional_chain_projected.has_value()) return VARIANT{std::in_place_type<flight::Undefined>, flight::undefined};
return optional_chain_projected.value();
```

The discriminator between the two cases is the subject's member declaration, not anything at the call
site: `optional<Ref<T>>` is case 1, an already-three-state `variant<...>` is case 2, and a receiver row
with no `RowPartial` is case 3 and needs nothing. A repair implementation must test the member
declaration, or anchor to these sites explicitly.

### The gain is five headers, not ten — and `contract.hpp` is why that was misjudged

The family was recorded as 10 headers (`node` 5, `lighting` 4, `materials` 1). Measured, the repair fixes
**five**. The difference is entirely in how a cascade was counted.

`contract.hpp` and `_internal_index.hpp` are per-package aggregates: they include the whole package, so
they accumulate every defect in it. Measured standalone against the committed tree:

| aggregate | baseline errors | with the absence-channel repair |
|---|---|---|
| `node/contract.hpp` | 585 | 581 |
| `lighting/contract.hpp` | 48 | 46 |

The repair is real and the error count drops by exactly the number of sites in each package, but neither
aggregate comes close to compiling, because neither was ever within four errors of it. Attributing them
to this family counted two headers that no single repair can unlock.

The rule this gives: **never count a `contract.hpp` or `_internal_index.hpp` in a family's gain without
reading its own error count first.** A cascade claim is only as good as the aggregate's distance from
zero, and these two are the headers most likely to appear in any family's failure list precisely because
they include everything.

This is the same bias as the collision package's overcount, where the heuristic's 20 attributable
failures measured as 17. Two independent overcounts in the same direction is enough to treat every
unmeasured per-family estimate as an upper bound.

## The unqualified-name family is 75% of all failures — and the first diagnostic hides a second defect

Sizing the whole report rather than one package: of 996 header failures, **745 fail on `'X' was not
declared in this scope`**, across 129 distinct names. Of those names, 124 are declared in
`flight::types` and simply used unqualified from another package's namespace — which is exactly what the
established `insert-using-declaration` kind repairs, one entry per (package, symbol) with the defining
header as its `include`. That accounts for 656 of the header failures and is by far the largest single
shape in the tree.

Five names first looked like they were declared nowhere in the tree, accounting for 89 header failures.
Checked individually, only three of them are, and the correction matters because the two big ones are the
ordinary case:

| occurrences | name | actually |
|---|---|---|
| 69 | `registry_entry_state` | declared in `flight::types` — `types/registry_table.hpp` has it as an `inline flight::Ref<...>` **variable**, used unqualified as `registry_entry_state.bound` |
| 10 | `get_node_runtime` | declared in `flight::node` — `node/revision.hpp`, a **template** function with a defaulted `Traits` argument |
| 4 | `data_tag_aef43e71dd1e9a6d` | genuinely absent: 2 references, no definition |
| 3 | `test_image_dimension_resolver` | genuinely absent: 1 reference, no definition |
| 3 | `warn_on_unsupported_snapshot_source` | genuinely absent: 1 reference, no definition |

So roughly 735 of the 745 are qualification problems and about 10 are referenced-but-never-emitted
symbols — a much smaller and different family, and the only one of the two that an override could be the
answer for.

The cause of the misreading is worth recording because it will recur: the declaration index matched only
`struct`, `class`, `enum class` and `using`. An emitted constant object is an `inline` variable and an
emitted generic function is a template, so neither was indexed, and both then read as "declared nowhere".
`get_node_runtime` also lives in `flight::node` rather than `flight::types`, so a sweep that assumes
`flight::types` is the only home for an unqualified name will miss it and mis-declare its repair.

**But 656 is an upper bound on headers unlocked, not a prediction, and the first measurement already
shows why.** The report records only each header's FIRST diagnostic. Resolving `Light` in
`lighting/light_analysis.hpp` — adding `using flight::types::Light;` and its include, which works — moves
the header to `expected ')' before 'in'`, a `for...in` lowering the emitter should never have produced.
That is a separate family, and no declaration repair reaches it.

So the sweep has to be measured per package by iterating to a fixed point: resolve every name the compiler
names, re-compile, and record whether the header actually reaches zero errors or lands on a different
family. Projecting from the 745 would repeat the mistake this document has now made three times.

One practical note for implementing it: gcc writes its diagnostics with U+2018/U+2019 curly quotes, while
the stored report has them normalized to ASCII. A scraper written against the report's spelling silently
matches nothing against live compiler output, which cost a round here.

### Measuring a repair with a single-file overlay reports false failures

The cheap way to measure a candidate repair without touching the committed tree is to write the modified
header into a scratch directory and put that directory first on the include path. It works, but it has a
failure mode that looks exactly like a defect in the repair.

Generated headers include their own package siblings with a QUOTED relative include —
`flight/app/app_render_view.hpp` contains `#include "app_window.hpp"`. A quoted include resolves relative
to the including file's own directory first, so when the overlay holds only `app_render_view.hpp`, that
sibling is not beside it and the compile fails with `app_window.hpp: No such file or directory`. The tree
is complete — 2711 headers on disk against 2710 the report attempted, and both `types/app_window.hpp` and
`app/app_window.hpp` exist — and the repair under test is irrelevant to the error.

The fix is to copy `generated/include` once into a scratch tree and patch headers **in place** there, so
siblings stay beside each other. Restore each header afterwards so one measurement cannot contaminate the
next.

The direction of the bias is worth keeping in mind when reading an earlier measurement taken this way: the
artifact produces false FAILURES, never false passes. So a header measured as compiling under a
single-file overlay is still sound evidence, and the absence-channel results above are unaffected — all
five of those compiled. Only a measured failure needs re-running in a full tree.

### `loader` is four families, not one — and a grep over emitted code must strip comments

The report lists `loader` as 1/4 with all three failures naming one `request_options(...)` conversion in
`load.hpp`. Compiled directly, `load.hpp` has **ten** errors in four distinct families:

- the `request_options` conversion, at two sites;
- `expected ')' before 'instanceof'` — a raw JavaScript operator emitted into C++;
- `std::optional<flight::String>` built from a brace-enclosed initializer, which is the
  `wrap-conditional-absent-branch` / designated-initializer family already declared;
- three errors inside `signals/slot.hpp`, a dependency that is itself broken, so `load.hpp` cannot
  compile until that does.

The `request_options` conversion is NOT fixed by the new variant constructor, and it was a reasonable
guess that it would be: the diagnostic is
`optional<variant<Ref<signal_progress_…>, Ref<signal_progress_…>>>` to
`optional<StructuralRef<RowReadonly<RowOf<Ref<NetRequestOptions>>>>>`, and `std::optional`'s converting
constructor would carry a variant-to-row conversion through. It does not apply because the two
`signal_progress_*` alternatives are progress-callback shapes that do not declare `NetRequestOptions`'
keys, so neither widens onto it and the constraint correctly refuses. That refusal is the right answer —
the emitter is passing a progress shape where a request-options row is declared — so this is an emitter
type error, not a conversion to supply.

**The comment trap.** Sizing the raw-operator family by grep gave `instanceof` in 14 headers and `typeof`
in 57, which read as a large family. Stripping `//` comments first leaves **3 headers** with a raw
operator in actual code, and the single `typeof` in code is in a header that PASSES. Emitted headers carry
the original TypeScript in comments — `// if (typeof data === 'string') return { msg: data, ...fields };`
in `log/log.hpp` is what produced most of the count — so any grep for a JS construct over this tree
measures the comments unless it strips them. `new` is worse than useless as a probe, since it is valid
C++ and matched 161 headers.

That is the fourth over-count in this session, after the collision estimate (20 vs 17), the
absence-channel attribution (10 vs 5), and the unqualified-name projection. Every one was high, and every
one came from counting a proxy instead of compiling the thing. The standing rule: a family's size is what
a compiler says after the fix, and anything else is a hypothesis.

### A header can pass the gate and still break every consumer

`signals/slot.hpp` **passes** the per-header compile gate. It also contributes three of the ten errors in
`loader/load.hpp`. Both are true, and the reason is structural rather than a flaw in either measurement.

The failing code is inside `template <typename T> inline T make_dispatch(...)`, in a generic lambda in its
body. A template's body is only type-checked when it is instantiated with concrete arguments, so
compiling `slot.hpp` on its own checks that it parses and nothing more. `loader/load.hpp` instantiates
`make_dispatch` with its concrete signal type, and the body fails then — `slot.value()(args...)` calls an
`optional<function>`, because the value is doubly wrapped.

This is why `loader` cannot be fixed by fixing `loader`: one of its four families lives in a dependency
that the gate reports as healthy. Twelve headers include `signals/slot.hpp` directly.

The blind spot is real and it is **bounded**: of the 1714 passing headers, **83 (5%)** contain a template
definition at all, so at most that many could be passing without their bodies checked — and only some of
those will have defects. The headline fraction is not badly wrong; it has a known 5% ceiling of
uninstantiated code, and `slot.hpp` is one confirmed instance.

Two consequences for how work is chosen. A package's failures may be rooted in a dependency that itself
reports as passing, so a per-package diagnosis has to follow the error's FILE and not the header under
test — the ten errors in `load.hpp` are in two different files. And a template-bearing header should not
be counted as verified on the strength of the gate alone; something must instantiate it. That is an
argument for the runtime's own tests over header counting, which is where semantics get pinned anyway.

## The whole board, by family

Every one of the 996 failures in the last full report, classified by its diagnostic. **Read the column as
"headers whose FIRST error is in this family", not "headers this family would fix"** — that distinction is
the one this document has now got wrong five times, and the families below are not independent.

| family | headers | share |
|---|---|---|
| undeclared name | 745 | 74.8% |
| no match for operator or call | 62 | 6.2% |
| no matching call | 40 | 4.0% |
| row / structural conversion | 36 | 3.6% |
| other conversion | 35 | 3.5% |
| not a member | 14 | 1.4% |
| missing member | 12 | 1.2% |
| `->` used on a non-pointer `flight::Ref` | 12 | 1.2% |
| parse / unlowered syntax | 8 | 0.8% |
| structural row WRITE incompatible type | 7 | 0.7% |
| everything else (13 shapes) | 25 | 2.5% |

Three things in this table are worth acting on.

**212 of the 996 — 21% — are per-package aggregates**, `contract.hpp` and `_internal_index.hpp`. Those
cannot be addressed directly at all: each clears only when its entire package clears, which is why
`node/contract.hpp` sits at 585 errors. The real addressable surface is **784 headers**, and any plan that
counts the aggregates as work items is counting the same work twice.

**The `->` on a non-pointer `Ref` family is 12 headers**, and one of them is already closed — it is the
family `raycast_collision_shape3_d.hpp` belonged to, and it was fixed with a SOURCE PATCH rather than the
override that `AGENTS.md` nominally points to for an operator emitted as source text. That is the cheaper
mechanism and it carries no copy, so the remaining eleven should be attempted the same way first.

**The row WRITE family is exactly 7**, matching the earlier count, and its repair is withdrawn (see
above) — so those seven are currently unaddressed and their mechanism is unknown pending measurement of
whether the element types are duplicate structural structs.

The 745 undeclared names remain the only family large enough to change the headline fraction, and the
measured rate at which resolving a name actually reaches zero errors is still unknown. That measurement —
iterate each header to a fixed point, record whether it compiles or lands on a different family — is the
next thing worth running, and it needs CPU that the in-flight regeneration currently owns.

### Retraction: the `for...in` behind `light_analysis.hpp` was in a shadowed file

Recorded above as a finding: resolving the unqualified `Light` in `lighting/light_analysis.hpp` exposes
`expected ')' before 'in'`, a `for...in` lowering. **Retracted.** That header has a hand-written override,
so the generated file I measured is shadowed and never compiles. Whatever is wrong with it does not ship.

The cause was an include order I got wrong, and it is worth stating because it is silent. The compile gate
orders its include paths `overrides/include`, then `generated/include`, then the runtime — overrides
**first**, which is the whole mechanism, as `scripts/overrides.mjs` says in its header comment. My ad-hoc
measurement command used `-I include -I generated/include -I overrides/include`, with overrides **last**.
For a header with no override the two orders give identical results; for an overridden one, the generated
file wins and you measure a file that is not part of the build.

Checked rather than assumed: none of the six absence-channel headers has an override, so those
measurements stand. `light_analysis.hpp` and `scene_forward_lights.hpp` both do.

The general form of the mistake: an override is invisible in the generated tree by design, so a
measurement that reads the generated tree directly cannot see that it is testing dead code. Any one-off
compile has to copy the gate's include order, not approximate it.

## The regeneration, and what it settled

124 minutes, committed at `98b53e16`. **2172/2709 modules emitted across 150 packages**, 537 refusals,
3 source patches applied, 637 headers repaired after emission, 42 duplicated structural structs aliased.
Four packages were excluded as non-terminating: `effects-gl`, `render-gl`, `scene2d-gl`, `scene3d-gl`.
The pinned Flight checkout came back with **0 dirty files**, so every patch reverted.

The important result is how little moved. 38 files changed — 37 headers and the manifest — and the only
manifest field that differs is `emissionRepairs: 61 -> 74`. Module counts are identical. So the 13 repairs
added since the previous run touched 37 headers and changed nothing else, which is what a regeneration
should look like when the repairs are declaration-shaped.

`npm run overrides:check` reported exactly the one drift predicted: `glyphatlas-explain-entry-optional-map-read`.

### Re-deriving a drifted override is a judgement, not a re-hash

The temptation is to re-record the digest and move on. What the drift actually asks is whether the
override is still correct for the file it now shadows, and that needs the diff read.

The generated file changed by exactly two added lines — `#include <flight/types/glyph_source.hpp>` and
`using flight::types::GlyphAtlas;` — one of the new `insert-using-declaration` repairs. Three things then
had to hold before re-pinning was the right move, and each was checked rather than assumed:

- the function is still refused — `NOT GENERATED: function explainGlyphAtlasEntry` is still in the
  generated file, so the override still has something to supply and cannot simply be dropped;
- the override does not depend on the added lines — it spells `flight::types::GlyphAtlas` fully qualified,
  so the new using-declaration is irrelevant to it;
- the override still compiles against the new tree, in the gate's include order.

Only then is the digest re-recorded, in both the manifest's `derivedFrom` and the override's own header
comment. Had the generated file's refusal been lifted instead, the correct action would have been to
**delete** the override, and re-hashing would have silently kept a copy of code the emitter can now
produce — which is the exact failure mode `derivedFrom` exists to catch.

A side effect worth knowing: that using-declaration repair is now **inert for this header**, because the
override shadows the whole file. It still matches a generated header, so `sdk:check` is satisfied, but its
effect is invisible in the shipped tree. A repair and an override targeting the same header are not an
error, but only one of them is doing anything.

### Measuring the unqualified-name repair one header at a time understates it

The first attempt at measuring the 745-header family patched the header under test: insert
`using flight::types::X;` for each name the compiler reported, re-compile, iterate. On two headers it got
nowhere, and the reason is a granularity error rather than a property of the family.

`allocate_entity` is used unqualified in **nine** `flight/animation/*.hpp` headers, and the header under
test includes several of them. Patching that one header leaves the same name unqualified in everything it
pulls in, so the name is reported again and the measurement records a failure that the real repair would
not have.

The declared repairs already have this right. `appliesTo` is a path PREFIX — `"flight/log/"`, not a file
— so one declaration inserts the using-declaration into every header of the package. The measurement has
to do the same: patch the whole package, iterate to a fixed point over all of its headers, and only then
count how many went from failing to passing. A per-header harness answers a question nobody is asking.

Two smaller things the same investigation turned up. `allocate_entity` lives in `flight::entity` and
`get_node_runtime` in `flight::node`, so `flight::types` is not the only home for an unqualified name and
a sweep that assumes it will generate wrong declarations. And the index must skip one-line forward
declarations — `namespace flight::types { struct AnimationBlendTree; }` appears at the top of many headers
and is not the package's namespace opener, so the insertion point has to be a namespace opener that ends
its line.

### The absence-channel repairs, implemented and measured — including the control that breaks

Both kinds are now implemented in `scripts/emissionRepairs.mjs` as `project-partial-row-absence` and
`unwrap-partial-row-three-state-member`, and run against the real generated files through
`applyEmissionRepairs`:

| header | baseline errors | after the repair |
|---|---|---|
| `node/has_clip.hpp` | 1 | **0** |
| `node/has_material.hpp` | 2 | **0** |
| `node/has_blend_mode.hpp` | 1 | **0** |
| `lighting/scene_lights.hpp` | 2 | **0** |
| `materials/standard_material.hpp` | 1 | **0** |
| `scene2d_resources/scene2_ddocument_source.hpp` *(control)* | 0 | **3** |

The last row is the point. It is the same construct, textually indistinguishable — its enclosing lambda is
declared to return the same three-state variant — but its receiver row is not partial, so the read already
yields the member type and the header compiles untouched. Declared deliberately as a control, the repair
fires and **breaks it**, 0 errors to 3.

So the transform is correct where it is declared and destructive where it is not, and nothing available at
the call site distinguishes the two. That is why both kinds anchor `appliesTo` to an exact header path and
name their keys, instead of matching the construct the way the other repair kinds do. It is the one place
in this file where a narrower `appliesTo` than a package prefix is the right choice, and the control is the
evidence rather than the argument.

Two implementation notes. The enclosing lambda's declared return type cannot be found by scanning
backwards, because the emitter writes these as one deeply nested line; `enclosingLambdaReturnType` walks
forward keeping a stack of open lambdas with the brace depth each body opened at. And the statement end is
taken from the call's matching parenthesis rather than the first semicolon — an argument could contain
one, and a mis-sliced call could be rewritten into something that still compiles.

While adding them, the dispatch in `applyOneRound` was replaced with a `HANDLERS` table. The ternary chain
had reached eleven branches, which is how `repeat-alias-declaration` got the wrong insertion point twice;
the table was checked to cover exactly what the chain did, with only `insert-forward-declaration` falling
to the default as before.

## Partly un-withdrawing the row-write repair: the rule was right, the sites are safe

The row-write repair was withdrawn above because `flight::array_of` returns a separate array handle, so
using it where TypeScript stores an existing array changes array identity. That reasoning stands. What was
wrong was applying it to these seven sites without reading them, and the correction matters because it
turns a refused family back into work.

The exact types, from the compiler rather than the heuristic: the write passes
`Array<Ref<types::Requirement>>` where `types::Requirement::requirements` is declared
`Array<StructuralRef<RowReadonly<RowOf<Ref<Requirement>>>>>`. So the element conversion is a row
projection over the **same subject** — not two types for one shape, and nothing to do with duplicate
structural structs, which is what the earlier note guessed it would be.

**The safety condition is what the written expression IS**, and it is visible at the site:

| site | written expression | identity observable? |
|---|---|---|
| `requirements/requirement_set.hpp` `requirements` | `distinct_sorted_requirements(...)` | no — a temporary |
| `requirements/requirement_set.hpp` `covers` | `distinct_sorted(covers)` | no — a temporary |
| `registry_codegen/registry_codegen.hpp` `entries` | a fresh local, built up then written once | no — the local dies with the call |
| `registry_codegen/registry_codegen.hpp` `unresolved` | a fresh local, already the row's element type | n/a — no conversion needed |

A temporary has no identity anyone can hold, and `distinct_sorted_requirements` mints new objects with
`make_ref` anyway, exactly as the TypeScript builds fresh object literals — so there is no pre-existing
array or element identity to preserve. A local written once into a row and never otherwise retained is the
same case: its identity is unobservable after the call returns.

So `array_of` is sound here, and measured: inserting it at the `requirements` write takes
`requirements/requirement_set.hpp` from failing to **0 errors**.

The general rule and these sites are both correct, and the distinction is the repair's precondition: the
written expression must be a temporary, or a local that is written once and not otherwise retained. Where
the written array is a parameter, or a value the caller still holds, `array_of` is still forbidden and the
withdrawal stands. That precondition has to be stated in the declaration and checked per site, because it
is not visible in the diagnostic — the same shape of discipline the absence-channel repairs needed, for
the same reason.

## Measured: the unqualified-name family is a coupled graph, and bulk application REGRESSES

The 745-header family is 75% of all failures and matches an established repair kind, which made it look
like the one lever big enough to move the headline fraction. Measured at the granularity a real repair
uses — insert the using-declaration and its include into every header of a package, iterate to a fixed
point, then count headers that went from failing to passing:

| package | before | after | names resolved |
|---|---|---|---|
| `clip` | 0/4 | 0/4 | 12 |
| `command` | 1/8 | **0/8** | 21 |
| total | 1/12 | **0/12** | 33 |

Thirty-three names resolved, **net minus one header**. The family is not a mechanical win, and the
regression is the useful part.

`command/command_history_signals.hpp` passes at baseline and fails after the sweep, and the error is not
in that file:

```
flight/registry/registry_table.hpp:372:54: error: 'registry_entry_state' was not declared in this
  scope; did you mean 'flight::types::registry_entry_state'?  [-Wtemplate-body]
```

The insertion added an `#include` to reach a name, that include pulled in another package's header, and
THAT header has its own unqualified name inside a **template body**. GCC checks template bodies ahead of
instantiation (`-Wtemplate-body`), so the defect surfaces as soon as the header is included at all. A
passing header was turned into a failing one by a repair that is individually correct.

So three things are true of this family at once:

- **it is coupled through includes.** Repairing a package needs the closure of packages its headers
  transitively include, not the package alone. `registry_entry_state` — the single most frequent name at
  69 occurrences — is unqualified in `flight::registry`, so anything that reaches that header inherits the
  failure;
- **adding an include is not a free action.** Every `insert-using-declaration` declaration carries an
  `include`, and each one widens the set of template bodies the compiler will check. That is how a repair
  can have negative value;
- **the 656 figure is not a plan.** It counts headers whose first diagnostic is a name. It does not count
  the second family behind each one, and it does not net off the headers a careless application breaks.

This also retrospectively explains why the 47 `insert-using-declaration` entries already declared were
added package by package with a measurement each time, rather than generated in a batch. That was the
right method and this is the evidence for it: a batch would have shipped a regression.

The family remains worth working, but as a per-package, measured, include-closure-aware sequence — and
`flight::registry`'s own unqualified names should be repaired before anything that includes it. It is not
the quick lever the headline share implied, and nothing in the earlier sizing should be read as a forecast.

**One refinement, measured afterwards: the family is not inherently net-negative — careless bulk
application is.** Applying only the targeted `flight::registry` using-declaration, and nothing to the
`command` package at all, leaves `command/command_history_signals.hpp` at **0 errors**. It was the bulk
insertion's new includes that broke it, not the existence of the repair. So the cost sits specifically in
adding an include to reach a name, and a declaration that is individually justified and measured does not
carry it. That is the difference between the 47 declared entries, which are safe, and a generated batch,
which is not.

### The row-write family closes completely: 7 headers from two declarations

Measured against the regenerated tree, each header compiled standalone in the gate's include order:

| header | baseline errors | with `array_of` |
|---|---|---|
| `requirements/requirement_set.hpp` | 2 | **0** |
| `requirements/requirement_collector.hpp` | 2 | **0** |
| `requirements/contract.hpp` | 2 | **0** |
| `requirements/_internal_index.hpp` | 2 | **0** |
| `registry_codegen/registry_codegen.hpp` | 2 | **0** |
| `registry_codegen/contract.hpp` | 2 | **0** |
| `registry_codegen/_internal_index.hpp` | 2 | **0** |

**+7 headers from two repair sites** — one `array_of` at `requirements`' write, one at
`registry_codegen`'s `entries`. That is the whole family the board listed as 7, closed.

Both aggregates cleared here, which qualifies the aggregate rule stated earlier rather than contradicting
it. The rule was to read a `contract.hpp`'s own error count before crediting it to a family: `node/contract.hpp`
has 585 and cannot be cleared by one repair, while `requirements/contract.hpp` has **2** — the package's
only defect was this one, so fixing it clears the aggregate and the index with it. The error count is the
test, not the filename.

Implemented as `project-array-at-row-write`, which carries an `identityArgument` field alongside its
`keys` and `element`, because the precondition that makes `array_of` legal — the written expression is a
temporary, or a local written once and not otherwise retained — is invisible in the diagnostic and has to
be asserted per site. The handler was checked to reproduce the hand-verified edits byte for byte and to be
idempotent across repair rounds, since `applyEmissionRepairs` iterates to a fixed point and a second wrap
would nest `array_of` inside itself.

### `flight::registry` is three stacked families, and an unchanged error count hid the progress

The previous entry concluded that `flight::registry` should be repaired before anything that includes it,
since its unqualified `registry_entry_state` is checked inside a template body and poisons every includer.
Tested, by inserting `using flight::types::registry_entry_state;` and its include into
`flight/registry/registry_table.hpp`:

**4 errors before, 4 errors after** — and the repair worked. The errors are not the same ones:

| before | after |
|---|---|
| `'registry_entry_state' was not declared` (line 372, `-Wtemplate-body`) | `flight::Ref<types::bound_tombstoned_…> {aka shared_ptr<…>}` — member access on a reference |
| `'get_registry_table_entry_state' was not declared` | unchanged |
| variant alternative error | unchanged |
| `'registry_entry_state' was not declared` (line 395) | the same member-access error at 396 |

Both name errors resolved and both were replaced by the **`.` instead of `->` on a `flight::Ref`** family —
`registry_entry_state` is declared `inline flight::Ref<bound_tombstoned_…>`, so the emitted
`registry_entry_state.bound` needs `->bound`. That is the family `raycast_collision_shape3_d.hpp` belonged
to, already closed once with a source patch rather than an override, so the mechanism is known.
`get_registry_table_entry_state` is in neither tree and is one of the three genuinely-absent symbols.

So `registry` needs three repairs stacked — the using-declaration, the arrow family, and a refused
function — and it is not the single unlock the include-coupling argument suggested. The coupling claim
still holds; the hoped-for leverage does not.

**The method lesson is the error count.** 4 to 4 reads as "the repair did nothing" and would have been
recorded that way on the strength of the number. The repair in fact resolved both names it targeted. An
error count is a measure of the FRONT of a queue, not of progress through it, and the only honest readings
are zero or a diff of the diagnostics themselves. This is the same first-diagnostic trap as the report,
one level down, and it is the sixth over-or-under-count in this session traceable to counting a proxy.

### Why `respell-reference-alias` is not the answer for `registry_entry_state.bound`

Worth writing down because it is the obvious cheap fix and it is wrong.

`registry_entry_state` is a TypeScript const object — `{ bound: 'bound', tombstoned: 'tombstoned' }` — and
the emitter gives it a heap identity:

```cpp
inline flight::Ref<bound_tombstoned_54a1df218909e3d5> registry_entry_state =
    flight::make_ref<bound_tombstoned_54a1df218909e3d5>({.bound = …, .tombstoned = …});
```

then reads it as `registry_entry_state.bound`, which needs `->bound` on a `shared_ptr`. The tempting
repair is `respell-reference-alias` with `expansion: "value"`, which exists precisely to say that a
`flight::Ref<...>` of some named template should expand to a value rather than a shared pointer — and a
value would make `.bound` correct.

It does not apply. That kind's own rule is `shared-pointer` for a struct deriving from
`flight::ReferenceEnabled` and `value` for anything else, and
`struct bound_tombstoned_54a1df218909e3d5 : public flight::ReferenceEnabled` derives from it. So
`shared-pointer` is the CORRECT expansion here, and declaring `value` would contradict the rule that
`referenceAliasIdentityProof` turns into a compiled assertion — the repair would either fail its own proof
or change the object's identity from shared to copied, which is a reference-identity change.

So these two sites are genuinely the `.` instead of `->` family, which is an operator emitted as source
text. `AGENTS.md` nominates an override for that, and the one instance already closed used a SOURCE PATCH
instead, which is cheaper and carries no copy. An override here would be larger than it looks, because the
same module also refuses `get_registry_table_entry_state` — one of the three genuinely-absent symbols — so
an override would have to supply that too, not merely respell an operator.

Left for whoever takes the arrow family, with the cheap option already eliminated.

## `npm run build:check` caught an unregistered public header — and a larger pre-existing drift

`build:check` reported `template_argument.hpp` missing from the CMake public-header list, from Bazel's
`hdrs`, and from the Bazel per-header self-containment suite. That header was added here earlier and is
included by `flight/runtime.hpp`, so it ships on the public boundary while being absent from the build
metadata — exactly the gap that gate exists to find. Now registered in all four places (the three the gate
names, plus a case in `header_self_containment_test.cpp`, which the Bazel entry's selector indexes into).

Investigating where to put it surfaced a **pre-existing and much larger inconsistency in the Bazel header
tests**, which `build:check` does not detect because it only checks that each name appears as text.

`tests/header_tests.bzl` pairs each header with a selector, and `flight_cpp_public_header_tests` passes it
as `FLIGHT_CPP_HEADER_SELECTOR`, which `header_self_containment_test.cpp` resolves through an `#elif`
chain. The selector is therefore an index into that chain, and the two have drifted:

- **50 of 54 bzl entries name the wrong header.** `("presence", 12)` builds `header_presence_test`, but
  selector 12 in the chain includes `runtime.hpp`. The numbering follows each list's own order, and the
  two orders are unrelated — the bzl is roughly alphabetical, the chain is append-ordered.
- **3 bzl entries resolve to no header at all** — `structured_clone`, `attachment` and `audio_context` are
  given 54, 55 and 56, and the chain stops at 53, so those three targets hit
  `#error "FLIGHT_CPP_HEADER_SELECTOR does not name a public header"` and fail to build under Bazel.
- **3 headers have no chain case**: `audio_context`, `erased_ref`, `font_face`.
- **2 headers are absent from the bzl entirely**: `base64`, `canvas_2d`.

Because the selectors are close to a permutation, most headers are still compiled by *some* target — just
not the one named after them — so the suite provides weaker assurance than its target names claim rather
than none. CMake is unaffected: it generates `#include <flight/${header}.hpp>` from the name directly and
needs no selector, which is why only the Bazel side drifted.

**Not fixed here.** Re-pointing 50 test targets and adding five chain cases is a visible change to the
build graph that is outside making the generated tree compile, so it is reported rather than taken. The
one-line fix that would prevent a recurrence is for `build:check` to assert the mapping instead of the
text — that it can read each bzl selector, resolve it through the chain, and require the result to equal
the entry's own name.

One other `build:check` failure is open and also pre-existing, unchanged by this regeneration: `generated
SDK contains 2710 module headers, expected 2172`. `summary.emittedModules` is 2172 in both the previous
and the current manifest, so the gate is comparing a header count against a module count and the two
legitimately differ — every package also emits `contract.hpp` and `_internal_index.hpp`, which are not
modules. Either the gate's expectation or the manifest field it reads is wrong.

## Every per-family estimate has come in high, and one came in at zero

Five independent attempts to size a family before repairing it, against what the compiler then said:

| family | estimated | measured | by |
|---|---|---|---|
| collision attributable failures | 20 | 17 | builder |
| absence channel | 10 | 5 | here |
| raw JS operators in emitted code | 53 | 3 | here |
| `swfText` attributable failures | 10 | **0** | builder |
| unqualified names (bulk, per package) | 656 | **net −1** | here |

Five for five in the same direction, two of them at or below zero. This is no longer a run of bad luck; it
is a property of how the estimates are produced. Each one counts headers whose FIRST diagnostic matches a
family, and a header's first diagnostic is the front of a queue — behind it sit families the count never
saw, dependencies that report as healthy, and in the bulk case a cost the count cannot express at all.

`swfText` is the sharpest case and the method is worth keeping. The cheapest source rewrite was tested
exhaustively rather than argued about: an explicit-fallback form removed the dual-sentinel refusal and
exposed a null-presence test over an inferred variant; an explicit common result shape removed that and
exposed a missing `GlyphOutlineMetrics` binding; spelling the four-field structural shape inline finally
emitted the function; a safe-integer guard corrected the emitted nullable-array lookup. Four successive
refusals cleared — and the header still did not compile, because the emitted function then produced
independent diagnostics for Record construction, Path structural conversion, nested optional Path member
access and metrics structural conversion, on top of inherited shape-package failures. Identical-denominator
census unchanged at 9/19, and `swf_text.hpp` still reports `CapsStyle` from `shape_bounds.hpp` first.

**The patch was then removed** (`408cd3e`) rather than carried. That is the right call and the reason is
worth stating: a rewrite that converts one refusal into several C++ diagnostics has negative value even
though it looks like progress at the refusal ledger. It makes the pinned TypeScript less direct, it carries
a maintenance claim, and it buys nothing a compiler will confirm. A source patch has to be judged on
whether a header compiles, not on whether a refusal disappeared.

The standing rule this all supports: **a family's size is what a compiler says after the fix.** Publish
estimates as hypotheses, name the measurement that would settle them, and expect to revise downward.

## Rank defects by BLAST RADIUS, not by first-diagnostic count — one header blocks 178

Every family count in this document shares a flaw: it attributes a header to whatever its first
diagnostic happens to be, which says nothing about where the defect physically lives. Computing the
reverse-transitive include closure over the generated tree and intersecting it with the compile report
gives a different and far more actionable ranking.

| defect site | headers downstream | of those, pass | fail | shadowed by an override? |
|---|---|---|---|---|
| `registry/registry_table.hpp` | **178** | **0** | **178** | no |
| `signals/slot.hpp` | 47 | 14 | 33 | no |
| `log/log.hpp` | 118 | — | — | **yes — shadowed, so inert** |
| `tween/_internal_internal.hpp` | 7 | 0 | 7 | no |
| `scene3d/scene_document.hpp` | 2 | — | — | no |

**`registry/registry_table.hpp` is the single highest-leverage defect in the tree.** 178 headers reach it
and **not one of them compiles** — a perfect correlation, and 19% of all 944 failures. Its three stacked
defects are already diagnosed above: the unqualified `registry_entry_state` (measured: the
using-declaration resolves both occurrences), the `.` instead of `->` on a `flight::Ref` that sits behind
it, and the refused `get_registry_table_entry_state`. All three must land together; fixing one leaves the
header failing and the 178 unchanged.

`tween/_internal_internal.hpp` validates the method independently: 7 downstream, 0 passing, which is
exactly the measurement builder reported for tween from the other direction — all eight failing headers
stopping at that file's emitted `key in tween->property_map`. Two different approaches, the same answer.

`log/log.hpp` is the cautionary row. It has the largest raw fan-out after registry at 118 headers and
contains a raw JS `in` in code — and none of it matters, because a hand-written override shadows the whole
file. A blast-radius ranking computed without checking `overrides/include` first would have put it near the
top of the queue. The override set is invisible in the generated tree by design, so it has to be consulted
explicitly, exactly as it does for any measurement in the gate's include order.

### The raw `in` operator, sized properly

The earlier raw-operator entry measured `instanceof` and `typeof` and reported 3 headers. It never tested
the **`in` operator**, which is the one builder's tween audit hit, because `in` is too common a word to
grep for naively. Anchoring on the emitted shape — an identifier followed by ` in ` inside parentheses,
comments stripped — finds it in exactly **3 headers of code**: `tween/_internal_internal.hpp`,
`scene3d/scene_document.hpp`, and `log/log.hpp` (shadowed). So the raw-operator family really is tiny by
site count; what makes `in` matter is that one of its three sites is a package's internal header with
every other header in the package downstream of it.

Which is the general lesson. **Site count and headers blocked are different numbers, and the second is the
one worth queueing on.** Three defect sites block 187 headers between them.

## The fresh numbers, against the regenerated tree

`npm run sdk:compile:sdl` over the tree at `98b53e16`, 89 minutes, then `sdk:tiers` against that report:

| figure | previous | now |
|---|---|---|
| headers compiling | 1714/2710 (63.2%) | **1752/2710 (64.6%)** |
| shippable packages | 39/146 | **44/146** (29 ready, 15 assisted) |
| partial | — | 101 |
| deferred defects | — | 1 (`statusbar`, 3 headers) |
| not applicable | — | 4, all declaring `flight.environment: "web"` |

`@flighthq/types` is at **995/995**, fully compiling. The remaining four non-applicable packages —
`effects-canvas`, `scene2d-canvas`, `scene2d-dom`, `textshaper-canvas` — are derived from their declared
environment, not inferred, as `AGENTS.md` requires.

## Correction, and it is mine: blast radius is leverage's CEILING, not leverage

The entry above ranked `registry/registry_table.hpp` as the highest-leverage defect in the tree on the
strength of 178 downstream headers, none of which compiled. The ranking was right about the site. The
implied leverage was wrong, and builder measured it rather than accepting it.

Builder landed the unit — a source patch rewriting the two retained `RegistryEntryState.Bound`
comparisons to the literal `'bound'`, plus an incomplete override supplying the refused
`get_registry_table_entry_state` — and measured the exact 179-header closure (the site plus its 178
downstream):

**baseline 0/179 → 3/179.** The three are `registry_table.hpp`, `registry/_internal_index.hpp` and
`registry/contract.hpp`. The other 176 reach later, independent diagnostics.

So the correct statement is: *178-header blast radius, 3 independently compiling headers gained.* Being
downstream of a defect means a header **cannot** compile until that defect is fixed; it does not mean the
defect is the only thing standing in its way. Blast radius is a necessary-condition count and therefore a
ceiling on leverage, exactly as a first-diagnostic count is. I had six prior examples of a count coming in
high and still presented this one as though it were a forecast; the discipline has to apply to my own
metrics, not only to the ones I was auditing.

That makes **six for six**. The pattern now includes a count I invented specifically to escape the pattern.

It does not retire the metric. A necessary condition is still worth knowing — those 176 headers will not
compile until registry is fixed, so the work was correctly prioritised and had to happen either way. What
changes is only the claim attached to it: rank queue ORDER by blast radius, and report GAIN only after
compiling the closure.

### Two consequences for the pending declarations

Builder's literal rewrite removes both occurrences of the unqualified `registry_entry_state`, so the
`registry-entry-state-using-declaration` entry I had drafted is **subsumed after regeneration** and must
be dropped rather than declared — a repair matching no header is exactly what `sdk:check` is built to
fail. Dropped; the pending set is 7, not 8.

And builder's override deliberately carries **no `supplies` claim**, with `status: incomplete`, because 11
declarations in that module remain refused — `concat`, `keys`, all three create/initialize pairs, and
without/with/tombstone. That is the right call: `supplies` is a stronger claim than `status: complete`,
and claiming it for a module where most of the surface is still refused would make the override's own
ledger lie.

### A `pgrep -f` watcher for a long job matches itself and never fires

Operational, but it cost ninety minutes of unnecessary polling and a burned CPU core, so it is worth
writing down.

Waiting for the compile was armed as `until ! pgrep -f sdkHeaderCompile >/dev/null; do sleep 30; done`.
That never fires. `pgrep -f` matches against the full command line of every process, and the watcher's own
shell carries the string `sdkHeaderCompile` in its command line — so the pattern matches the watcher, the
condition stays true after the real job exits, and no notification ever arrives. The compile finished and
nothing said so; it was found only by reading the report file.

Worse, an earlier watcher from a previous session was written as
`while pgrep -f sdkHeaderCompile >/dev/null; do :; done` — self-matching **and** with no `sleep`, so it
spun a core at 100% for as long as it lived. Load sat around 7 with only a 6-worker compile running;
killing three stale watchers dropped it to 0.97. Those watchers had been slowing the very job they were
waiting for.

Wait on the **pid** instead — `until ! kill -0 <pid> 2>/dev/null; do sleep 30; done` — which cannot match
itself. If a name must be used, exclude the watcher (`pgrep -f 'name' | grep -v $$`) and never omit the
sleep.

## A quoted relative include BYPASSES an override, and nothing currently checks for it

`AGENTS.md` says of overrides: "The whole mechanism is include order: the override directory goes first".
That is true for an angle include and **false for a quoted one**, which is a real hole in the mechanism
rather than a detail of it.

`#include "node_interaction_state.hpp"` resolves against the including file's own directory *before* any
`-I` path is consulted. The including file is the generated sibling, so the generated copy wins and the
override in `overrides/include` is never seen. `#include <flight/interaction/node_interaction_state.hpp>`
goes through the include path and does get the override. Both forms appear in the same package:

```
interaction/interaction_spatial_index.hpp:  #include "node_interaction_state.hpp"                      <- bypasses
interaction/hit_tests.hpp:                  #include "node_interaction_state.hpp"                      <- bypasses
interaction/enable_interaction_guards.hpp:  #include <flight/interaction/node_interaction_state.hpp>   <- honoured
```

This is what capped builder's interaction result at **0/19 → 1/19** despite a complete, faithful override
with a valid `supplies` claim and a 16-header closure. Fifteen of those siblings reach the generated
partial through a quoted include, so the override cannot affect them. Builder measured it and said
explicitly not to report 16 gained — the blast radius was real and the override was correct, and the
mechanism still could not deliver it.

**Scale, measured rather than assumed.** The tree has **836 quoted relative includes** against 15,985
angle includes. Of the 17 override files in this clone, **2** are reached by at least one quoted include
(`lighting/light_analysis.hpp`, `textlayout/text_format.hpp`), and both are also reached by angle includes,
so each is partially effective today. So this is not currently widespread damage — it is a latent hazard
that bites exactly when an override's own package siblings are its consumers, which is the common shape for
a package-internal header and therefore the shape most worth overriding.

It also explains the earlier measurement artifact from the opposite direction: a single-file overlay breaks
a quoted sibling include because the sibling is not beside the overlay copy. Same resolution rule, two
different symptoms.

**What should change.** `overrides:check` currently verifies only that each override still hashes to the
generated file it was derived from. It should also verify that the override can actually be *reached*: for
each override, scan the generated tree for any header that includes it with a quoted relative path, and
report those as sites the override does not cover. An override whose consumers all bypass it is a copy
carrying a maintenance claim and delivering nothing, and `derivedFrom` cannot detect that. Not implemented
here — it is a gate change, and the measurement above is the specification for it.

### The zero-pass packages, classified by whether an override can even reach them

28 packages have zero passing headers, 193 headers between them. The useful second column is not the
header count but the **include form their own siblings use**, because that decides whether an override can
reach the consumers at all:

| intra-package quoted includes | packages | verdict |
|---|---|---|
| 0 | **20** | an override reaches every consumer |
| 1–4 | 6 | partial; check the specific consumer |
| 13 | 1 (`interaction`) | the 16-to-1 case |
| 171 | 1 (`effects_wgpu`, 60 headers) | worst in the tree |

**Twenty**, corrected from 19 — builder reproduced the table independently against its own tree, matched
every per-package edge count, and caught the arithmetic: the eight partial-delivery packages are
`effects_wgpu`, `interaction`, `particleemitter`, `bitmapfont_formats`, `textureatlas`, `app`, `textinput`
and `shape_formats`, so 28 − 8 = 20. Worth keeping as a reminder that 19 is a real number here but a
different one: it is how many packages have a single internal header whose reverse closure covers every
header in the package. Two plausible figures for two different graph properties is exactly how a wrong one
survives.

Twenty of 28 are clean, and eleven of those are three-header packages — one module plus `contract.hpp`
and `_internal_index.hpp` — where a single root defect clears the package. Those are the cheapest wins on
the board.

This also shows that sending builder at `interaction` was a bad call on my part: of the 28, it was among
the four worst choices by reachability, and the check that would have revealed it costs minutes. Compute
reachability BEFORE choosing an override target, not after measuring a disappointing gain.

`requirements` (4) and `registry_codegen` (3) are on this list and are covered by the two
`project-array-at-row-write` declarations now in flight, which measured all seven of their headers to zero
errors. `statusbar` (3) is the single declared deferred defect in the fresh tier report, so it sits in the
shippable denominator by design and closing it is real progress.

## The doubly-optional call: one truthiness test in TypeScript, two optional levels in C++

`signals/slot.hpp` is the header that passes the gate while breaking its consumers, and its defect is
precise and small:

```cpp
std::optional<std::optional<std::function<void(flight::Array<flight::Any>)>>> slot = data->slots.get(i);
if (!slot.has_value()) { i++; continue; }
slot.value()(std::forward<ArgsPack>(args)...);     // calling an optional<function>
```

Both the type and the first check are RIGHT. `data->slots` is an `Array<optional<function>>` because a
slot can be cleared, and `Array::get` returns an `optional` because an index can be out of range — so
`optional<optional<function>>` faithfully models TypeScript's `Fn | null | undefined`. The emitter then
checks only the OUTER level and calls `.value()` once, which yields an `optional<function>` and is not
callable.

The cause is that **one JavaScript truthiness test covers both levels and C++ separates them.** The source
is `if (!slot) { i++; continue; } slot(...args)`, and `!slot` rejects `undefined` *and* `null` in one
expression. Lowered faithfully that is two tests and two unwraps:

```cpp
if (!slot.has_value() || !slot.value().has_value()) { i++; continue; }
slot.value().value()(std::forward<ArgsPack>(args)...);
```

Sized, with comments stripped: there are **128** doubly-optional local declarations tree-wide and only
**4** headers use one through a single `.value()` — `signals/slot.hpp`, `signals/safe.hpp`,
`preferences/storage.hpp`, and `scene3d_wgpu/wgpu_mesh_pipeline.hpp`. So the emitter normally gets this
right and these four are exceptions, which is a point in favour of a narrow declared fix rather than a
general one.

**Mechanism.** This is not declaration-only — it adds a condition and an unwrap — so by the standard used
for the other repairs it is not an emission repair, and `AGENTS.md` points at an override. An override is
a poor fit here: `slot.hpp` is template-heavy, and it would mean carrying a copy of a header the gate
reports as PASSING, which is a confusing thing to leave behind. The better first attempt is a **source
patch** that makes the two levels explicit in the TypeScript — `if (slot === undefined || slot === null)`
rather than `if (!slot)` — so the emitter has no truthiness to collapse. That cannot be tested while a
regeneration holds the pinned checkout, so it is the next thing to try when the tree is free, with an
override as the fallback only if no equivalent rewrite lowers correctly.

Note the runtime cannot help here, which is worth stating because it is the first option `AGENTS.md`
prefers: making `slot.value()(...)` compile would require `operator()` on `std::optional`, and that is not
ours to add.
