# flight-compiler package graph review

The maintained downstream checklist now lives in [flight-compiler adoption status](flight-compiler-adoption.md).

The current checkout pins Flight `7e2fc7d` and flight-compiler `ef60fb6`. Both revisions are recorded in
[`dependencies.lock.json`](../dependencies.lock.json), and the maintained status document records the active counts
and remaining ownership. The section immediately below is the current round; everything after it is the historical
record of the earlier `903f328`/`fbfcc11`, `993c280` and `9f6ce1c` handoffs. There were two rounds on
2026-09-21; the second one is first.

## Round of 2026-09-21 (second): computed cells, GlExtension, and the conformance artifact

Three needs arrived in priority order. All three are answered. The second one is answered by
leaving a gate red on purpose, which is explained below rather than buried.

### 1. Computed cells were invisible to the widening proof

Reproduced before anything was changed. Four subjects that agree on every string key at the same
type and differ only behind `Symbol.for('EntityRuntime')`:

```cpp
struct Base   { double value; std::optional<flight::Ref<RuntimeA>> entity_runtime_key; };
struct Peer   { double value; std::optional<flight::Ref<RuntimeA>> entity_runtime_key; };
struct Widest { double value; };
struct Deriv  { double value; std::optional<flight::Ref<RuntimeB>> entity_runtime_key; };
```

Every pair widened onto every other pair, and the consequence was observable rather than
theoretical: with the cell engaged on the object, `row_has(widened, EntityRuntime)` answered
**false**. Same object, same owner, opposite answer from the two rows.

Two things were wrong, and fixing either alone leaves a hole.

**The proof could not see the cell.** A computed cell is not a row key. No `RowKey` ever names it,
so nothing in the generated key table compares it, and the proof's verdict was decided entirely by
members that happened to agree. The fix is a second, separate question asked alongside the proof:

- `flight::detail::RowComputedCells<Object>` reports the cell's type, or `void` when the subject
  does not declare it.
- `flight::detail::computed_cells_agree<Base, Derived>` is conjoined onto the widening branch of
  `row_objects_convertible`. It is the **runtime's own** list, deliberately not derived from the
  generated table: a table older than this contract contributes no widening and is safe, but no
  table of any age may approve a pair the runtime itself can tell apart.
- The generated table adds `FLIGHT_SDK_ROW_COMPUTED(member)` for every computed cell the emitted
  SDK declares — including ones the runtime cannot reach. A disagreement there is fatal to the
  proof rather than merely uncounted.

The rule is narrower than "the cells must match", and the corpus is what narrowed it. Refusing a
cell that only ONE subject declares costs five headers and is wrong: a computed cell is declared
`EntityRuntime | undefined`, so `binpack::FreeRectangle` — a plain `{x, y, width, height}` — really
is a `Readonly<types::Rectangle>` in TypeScript, and `types::Adjustment` really is readable as its
own anonymous `{kind}` bag. Both now read honestly in both directions, because of the owner-bound
cell below. What is refused is two subjects that **both** declare the cell at **different** types:
the owner holds one cell of one type, so the other row's read misses the typed lookup and is handed
a default instead of the value sitting on the object. That one is silent, which is why it is a
compile error.

**Even a permitted widening could not reach the cell.** `shared_object()` is type-checked against
the row's own subject, so over a derived object a base-typed row answered null and the symbol read
fell through to the attachment. Computed cells are now bound on the `RowOwner` — the one thing
every row over an object shares — through `bind_computed`, and `row_get`, `row_set` and `row_has`
over a `Symbol` consult the owner first. A widened row now reads and writes the subject's real
member.

The two halves are complementary rather than redundant. The owner serves the read; the comparison
catches what the owner cannot serve — a read asking for the base's type when the object holds the
derived one, and any cell the runtime's `Symbol` overloads cannot reach at all.

#### How computed cells are found, and the one the runtime cannot reach

The generator finds them from the only evidence the emitted output carries: the compiler declares a
computed key as an `inline const flight::Symbol` constant and names the member it produces after
that constant. A symbol constant whose spelling also appears as a member declaration is a computed
cell; anything spelled like a string row key is excluded, because the two key spaces are separate
and `altKey` is an ordinary string property.

On the SDL corpus that finds exactly two:

| cell | symbol | reachable by the runtime's `Symbol` overloads |
| --- | --- | --- |
| `entity_runtime_key` | `Symbol.for('EntityRuntime')` | yes |
| `scene3_dresource_resolver_runtime_key` | `Symbol(String("Scene3DResourceResolverRuntime"))` | **no** |

The second one is a live read-side hole and is worth stating plainly. The emitted SDK contains

```cpp
flight::row_get<flight::Ref<flight::types::Scene3DResourceResolverRuntime>>(
    row, flight::types::scene3_dresource_resolver_runtime_key)
```

and the runtime cannot serve it. `Symbol::for_key` interns by description, but
`Symbol(String("X"))` is a *unique* symbol whose identity is the specific constant, so the runtime
has no way to recognise it: the read falls through to the attachment and hands back a default. The
widening proof fails closed across a disagreement about this cell, so nothing unsound is approved,
but the read itself is wrong today.

Closing it needs the compiler, not the runtime. The generated table knows both the symbol constant
and the member name and could bind the pair the way `bind_generated_row_members` binds string keys
— but it would have to name the constant, which lives in a generated module header the table cannot
include. **The ask**: emit the computed-key bindings into `flight/sdk/structural_members.hpp`
itself, or emit the symbol constants somewhere the table can reach, and the runtime will consume
them through the same conservative-primary-plus-generated-specialization shape the widening trait
already uses.

### 2. GlExtension, and the two compiler gaps it exposes

`GlExtension` reduced every WebGL extension object to a bool with two static anisotropy enums
hanging off it, so `EXT_color_buffer_float` answered `TEXTURE_MAX_ANISOTROPY_EXT`, `getExtension`
never returned null, and no extension could be told from another. The shape is now determined by
what the real Flight sources actually do with these objects
(`render-gl/src/glCompressedTexture.ts`): test the result against `null`, then read numeric enums
off it by name with a `typeof … === 'number'` guard.

- `WebGl2Context::get_extension` returns `std::optional<GlExtension>`; the nine `sdl-gl` bindings
  are now `nullable`.
- `GlExtension` carries the queried `name()` and exposes `get(String) -> std::optional<double>`,
  `has`, declaration-order `keys()`, and `to_record()`. `get` returning an empty optional **is**
  the `typeof` check: a property the extension does not define answers nothing, never `0` and never
  `-1`.
- Per-extension enum tables for S3TC, S3TC sRGB, RGTC, BPTC, ETC/EAC, ASTC, PVRTC and anisotropy,
  cross-checked against the Khronos registry and the installed GL headers.
  `EXT_color_buffer_float` and `OES_texture_float_linear` expose deliberately empty key sets, which
  is the case that proves identity is real.

**`npm run sdl-gl:oracle` is red, on purpose.** Its TypeScript reads
`anisotropy.MAX_TEXTURE_MAX_ANISOTROPY_EXT` after a `!== null` guard. Against the corrected header
the pinned compiler `ef60fb6` emits, verbatim:

```cpp
return (anisotropy.max_texture_max_anisotropy_ext + (color_buffer_float ? 1.0 : 0.0));
```

```
error: 'class std::optional<flight::host_sdl::GlExtension>' has no member named
       'max_texture_max_anisotropy_ext'
```

That single line is two separate gaps and both are yours:

1. **Narrowing.** `anisotropy` is an `std::optional` and the `!== null` guard is not being used to
   unwrap it. This is independent of extensions and would bite any nullable external binding.
2. **Lowering.** Even unwrapped, the member does not exist, because the enums are per-extension
   data now rather than static members on a type shared by every extension. A property read on a
   bound external value type needs to lower onto `GlExtension::get(String)`.

The gate was briefly made green by deleting the property read from its source. That was reverted:
the case is the only coverage anywhere of an extension-object property read, and a suite that got
quiet by asking less is worth less than one red case with a named owner. It stays red until the
compiler lowers it, and it is neither skipped nor allowlisted — the failure prints its own ownership
and points back to this section, so the next person to run `npm run check` reads "known gap, owned
upstream" rather than "something I just broke".

### 3. The native-conformance artifact was stale

Regenerated `tests/generated/semantic_runtime.hpp` from the vendored
`tests/generated/semantic_runtime.ts` through the pinned compiler; the drift was two lines, which
matches the recurring backend mismatch that was reported. `scripts/conformanceGeneration.mjs` now
regenerates into a temporary directory and diffs against the checked-in header, and
`conformance:check` is wired into `npm run check`, so the artifact cannot rot silently again. It
skips cleanly when `.dependencies/` is absent, like every other gate that needs a checkout.

### Not asked for, found on the way: a row keeps its subject alive forever

Any object that acquires a `RowOwner` is never collected. The generated member binding captures the
object **strongly** in its getter; the owner registry holds the owner strongly and the object
weakly; the cycle keeps both alive and the weak entry never expires, so it is never swept.

```
alive while a row names it:                       yes
alive after both the ref and the row are gone:    YES (leaked)
```

Confirmed pre-existing — it reproduces at `HEAD` with this tranche's changes stashed, and a subject
that binds no member at all is collected correctly. The computed-cell binding added here follows
the same capture pattern deliberately, so it adds no new class of leak, but the underlying defect
wants its own change: either a weak capture that locks per read, or binding by member pointer
against the owner's existing weak object. Flagged rather than fixed, because it is a lifetime
decision that belongs in its own commit with its own test.

### Answering the review of this release

Three things came back. One is yours and already tracked; the other two need something from you
before we can move, and both are stated here rather than left implicit.

**The conformance artifact is not behind — the pin is.** It was regenerated with `ef60fb6` because
that is what [`dependencies.lock.json`](../dependencies.lock.json) names, and the whole point of
`npm run conformance:check` is that the artifact tracks the pin exactly. Regenerating it against a
compiler this checkout does not pin would produce a file matching no revision either repository can
name, and our own new gate would fail it immediately — correctly.

So this is a pin move, not an artifact refresh, and it needs two things we do not have:

1. **The commit.** Name the flight-compiler SHA to pin. "Our folded compiler" is not resolvable
   from here; `.dependencies/` is materialised from the lock and nothing else.
2. **A re-measurement budget.** Moving the pin means regenerating the committed SDK inventory and
   re-running the emission and header sweep, because every number in this document is stated
   against `ef60fb6`. Per `AGENTS.md` that lands as its own commit carrying the gate result that
   motivated it.

Send the SHA and it is a contained piece of work. Until then the artifact is correct for the pin,
and `conformance:check` will flag it the moment the pin moves — which is the behaviour that was
asked for.

**The WGPU device/queue/texture callable surface was never requested of this repository.** It has
not appeared in any of the handoffs relayed here — the 2026-09-19 tranche, the 2026-09-20 primitives
and integration addendum, or either 2026-09-21 round. That is worth saying plainly so it is
diagnosed as a lost relay rather than as work that was received and skipped.

What exists today, so the gap is concrete rather than a shrug. `bindings/sdl-wgpu.json` carries 66
entries across 18 WebGPU object domains — 61 type bindings and 5 value bindings — and `GPUDevice`,
`GPUQueue`, `GPUTexture` and `GPUCommandEncoder` are among them. They are bound as **opaque identity
handles**: `WgpuObject` gives provider-owned lifetime, release callbacks that run exactly once,
stable identity, and weak policies for generated caches. What none of them has is **operations**.
There is nothing for `device.createBuffer` to lower onto, which is the same shape of gap
`GlExtension` had before this release and was fixed by giving the handle real operations.

Measured against the pinned Flight sources, the surface is 34 distinct methods and 505 call sites.
The dense end of it:

| calls | method | | calls | method |
| ---: | --- | --- | ---: | --- |
| 60 | `pass.setBindGroup` | | 25 | `device.createRenderPipeline` |
| 50 | `device.createBindGroup` | | 21 | `pass.setPipeline` |
| 39 | `device.createBindGroupLayout` | | 19 | `pass.draw` |
| 37 | `device.createShaderModule` | | 17 | `texture.createView` |
| 37 | `queue.writeBuffer` | | 13 | `texture.destroy` |
| 36 | `device.createBuffer` | | 12 | `pass.end` |
| 29 | `device.createTexture` | | 10 | `queue.writeTexture` |
| 25 | `device.createPipelineLayout` | | 10 | `device.createSampler` |

Note that a third of the weight is the **render-pass encoder**, not device/queue/texture — a
device/queue/texture-only surface would leave `pass.*` with nothing to lower onto and the modules
still refused.

The reason this has not simply been built is a decision that is not ours and not yours. Every one of
those methods has to reach a real WebGPU implementation, and this repository has deliberately not
selected one: the adoption record states that Dawn or wgpu-native supplies device operations and
that the package does not choose between them. Implementing the callable surface means taking that
dependency, in a runtime whose contract is to be dependency-free outside the optional SDL host.
That is the repository owner's call. Say which implementation you are targeting and whether the
render-pass encoder is in scope, and it becomes a scoped piece of work rather than an open one.

**Guarded-optional narrowing and `GlExtension::get` lowering** are yours, unchanged, and
`scripts/sdlGlProfileOracle.mjs` stays red until they land. See the section above for the verbatim
emission and diagnostic.

## Round of 2026-09-21: the four integration follow-ups

Two of the four were already satisfied here and needed verifying rather than changing; two were
real. Taken in the order they were sent.

### 1. The native semantic conformance oracle — already correct, verified

`tests/generated_runtime_test.cpp` already reads exactly what was asked for:

```cpp
const auto for_in_values = flight::make_ref<flighthq_cpp_conformance::ForInValues>();
for_in_values->value = 1.0;
check(flighthq_cpp_conformance::select_first_key(for_in_values) == flight::String("value"), …);
```

and `tests/generated/semantic_runtime.hpp` already declares `struct ForInValues : public
flight::ReferenceEnabled` with `select_first_key(flight::Ref<ForInValues>)`. No brace-initialisation
of `ForInValues` survives anywhere in the repository. It was fixed on 2026-09-14 in `7f5474d` and
the header refreshed on 2026-09-17 in `c4cb9ec`, both before the pin the compiler is holding.

Verified rather than assumed: a strict compile with `-Wall -Wextra -Wpedantic -Wconversion -Werror`
succeeds and the executable oracle exits 0. `Ref` semantics are untouched. The header cannot be
regenerated from this checkout because `@flighthq/cpp-conformance` is not part of the pinned
compiler tree — and it does not need to be.

### 2. The structural-row widening hook — the diagnosis was right, the contract is now implemented

The symbol did exist here, but the failure mode reported was real and the proposed contract is the
correct fix. The old arrangement defined `generated_row_widening_proven` **inside the generated
table** and gave the runtime a fallback only in the no-table branch, so any table predating the
predicate left the name undeclared — a compile error in a consumer that did nothing wrong. That is
exactly what integration hit.

Implemented as specified:

- `flight::detail::GeneratedRowWidening<Base, Derived>` has a conservative `std::false_type`
  primary, declared **before** `StructuralRef` uses it and before the generated table is included,
  with `generated_row_widening_proven_v` as the value alias.
- The generated table adds exactly one **constrained partial specialization** —
  `requires(generated_row_widening_matches<Base, Derived>())` — answering true only for pairs it can
  prove. Nothing else specializes it.
- Include order is runtime declarations, then the generated structural table, then the module that
  instantiates a conversion.
- **Exact-owner readonly views work through the default-false path.** Same-subject conversions are
  answered by the same-subject rule before the trait is consulted, so a readonly view of the owner's
  own type never needs a proof. This is covered by a test whose subject declares only a member that
  is not an SDK row key, so it proves nothing even against itself and must still convert.

The regression test for the original failure is that `hostCapabilityOracle`'s hand-written member
table now emits **no widening code at all** and still compiles — which is the property that makes a
table older than this contract safe.

Measured: `generated/` regenerated at 1138/2904 and the seven-profile SDL lane at 1435/2904, both
unchanged, and the folded-tree sweep is **1368/1435 with 67 failures — identical to before the
change**. Changing how every widening is decided should not ship unmeasured, so it did not.

### 3. The missing WebGL constant — added

`WebGl2Context::max_texture_size = 3379` is in the alphabetized GLenum block, with native SDL
coverage and compiler-emission coverage in the SDL/GL profile oracle. The rest of the `GL_MAX_*`
family the current SDL corpus reaches was audited at the same time: `MAX_SAMPLES` and the extension
`MAX_TEXTURE_MAX_ANISOTROPY_EXT` were already present, and `MAX_TEXTURE_SIZE` was the only reached
standard constant missing. Nothing unreached was bulk-added.

### 4. `Number.prototype.toFixed` — already implemented, and now proven; **fa18c9da can be unparked**

`flight::number_to_fixed(double value, double digits = 0.0)` has been in `flight/number.hpp` since
an earlier tranche. It was not changed, because it already satisfies the contract; what it lacked
was proof, which it now has. A live Node/native differential passes identical double bit patterns
through 28 cases: default, zero, fractional, negative, NaN, 100, 100.9, out-of-range and infinite
digits; NaN and both infinities as values; both neighbours of the 1e21 cutoff and the cutoff itself;
the tie-sensitive cases that separate ECMAScript from `printf` — `(2.5).toFixed(0)`,
`(1.005).toFixed(2)`, `(0.5).toFixed(0)`, `(1.45).toFixed(1)`, `(8.575).toFixed(2)`; negative zero
and `-0.0001`; large values below the cutoff; and validation precedence. It is wired into
`npm run check` as `numberToFixedOracle`.

The rounding is done on the double's exact decimal expansion rather than by asking `printf` for the
final precision, because the two disagree on ties — `printf` rounds to even, ECMAScript rounds away
from zero — and the sign comes from the comparison rather than the sign bit, so negative zero has no
sign while a negative value that rounds to zero keeps one.

### On the larger follow-ups

Non-copying native-reference-to-structural-row views, computed-symbol row metadata and owner-bound
access, WeakMap platform key policy, and existential structural owners are not in this round. The
constraint attached to them — no unchecked casts, no copied or materialized rows — is already how
the adjacent machinery is built, and is worth stating so the follow-ups start from it: a widened row
keeps the source object and the source's single row owner rather than copying anything,
`structural_ref_cast`'s materializing path is gated to a source whose object type is EMPTY so it can
never replace an object that holds state, and `ErasedRef::as<T>()` answers only for the type the
reference was erased from.

## Integration addendum of 2026-09-20: `538fe1d` is not a separate lineage

The addendum asked flight-cpp to "merge the newer portable-service commits onto the complete runtime lineage before
advancing the compiler's flight-cpp pin", on the evidence that a locally available `538fe1d` tree has 27 runtime
headers and that compiling against it failed 739 of 1107 headers because `structural_ref.hpp`, `any.hpp`,
`boolean.hpp` and `record.hpp` were absent.

**There is nothing to merge: `538fe1d` is an ancestor of the pin, not a branch beside it.** It is 2026-09-11, the
compiler's pin `7107eee` is 2026-09-18, and `538fe1d` is 118 commits behind that pin and 136 behind this branch.
Every commit the addendum calls a "portable-service commit" is already in this history:

| Commit | Date | In the current branch |
| --- | --- | --- |
| `70656d4` feat(runtime): add generated reference semantics | 2026-09-10 | yes |
| `0390ed6` feat(runtime): add compiler portable services | 2026-09-11 | yes |
| `538fe1d` fix(runtime): compile emitted portable services | 2026-09-11 | yes |
| `533b480` chore(example): regenerate tween header | 2026-09-11 | yes |
| `911c336` chore: pin latest flight-compiler | 2026-09-11 | yes |

The four "absent" headers are absent from `538fe1d` because they had not been written yet, and each was added
after it on this same line:

| Header | Added | Commit |
| --- | --- | --- |
| `record.hpp` | 2026-09-13 | `9186298` feat(runtime): add ordered record storage |
| `structural_ref.hpp` | 2026-09-13 | `98f93dd` feat(sdk): add native preview runtime |
| `boolean.hpp` | 2026-09-14 | `75267cd` feat(runtime): implement JavaScript truthiness |
| `any.hpp` | 2026-09-17 | `1c9a1f8` feat(runtime): add the erased dynamic value |

So the 739/1107 failures are fully explained by compiling against an eight-day-old checkout, and merging `538fe1d`
forward would land a no-op at best. **The action is the opposite of the one requested: advance the compiler's
flight-cpp pin from `7107eee` to this branch's tip once it is merged.** The pin is already newer than `538fe1d`;
nothing needs to be rescued onto it.

If a tree with 27 runtime headers is what a compiler builder actually has checked out, the checkout is stale rather
than a different lineage — this repository has one line of development and `origin/main` is it.

### The SDL generation lanes are present and were never removed

The addendum also asks to restore the seven profiles. They are all in this branch and the lanes that compose them
are too: `bindings/runtime.json`, `headless.json`, `web-types.json`, `sdl-image.json`, `sdl-gl.json`,
`sdl-wgpu.json` and `sdl-app.json`, driven by `npm run sdk:generate:sdl` and `npm run sdk:compile:sdl`, with
`sdk:generate:headless`, `sdk:generate:runtime`, `sdk:generate:sdl-gl` and `sdk:generate:sdl-wgpu` beside them for
narrower lanes. The agreement that the portable generator alone gives a misleading corpus score stands: it emits
1138 of 2904 modules where the seven-profile SDL lane emits 1435.

### `sdk:compile` is now resumable

`scripts/sdkHeaderCompile.mjs` checkpoints and resumes, which is what makes a long sweep survivable:

- The report is rewritten atomically — written to a sibling temporary and renamed — every `--checkpoint-seconds`
  (default 10) and again on the way out, so a reader never sees a half-written report and an interrupted write
  cannot destroy the one already there.
- It always distinguishes **attempted and passed**, **attempted and failed**, and **never attempted**. A run
  stopped by `--deadline-seconds` or by SIGINT/SIGTERM finishes its in-flight headers, records the remainder as
  unattempted, and exits 2 rather than pretending to be a result.
- `--resume` continues from an existing report instead of starting over. A report from before this change lists
  only failures, so it is rejected with a message rather than being read as "everything unnamed passed" — that
  absence means "passed" and "never tried" alike, and assuming the friendlier one would report a pass nobody
  observed.
- `--headers=a.hpp,b.hpp` (or `--headers=@file`) compiles just those. **This is the loop a compiler builder should
  use**; the full sweep stays the folded-tree gate that runs once at the end.
- Progress lines carry elapsed time, rate and ETA.

The report schema moves to `flight-sdk-header-compilation/2`, which adds `run` (complete, elapsedSeconds,
headerSelection, jobs), `summary.attemptedHeaders` and `summary.unattemptedHeaders`, and the `passed` and
`unattempted` header lists that make resume possible. The `summary.totalHeaders`, `passedHeaders` and
`failedHeaders` keys are unchanged.

## Round of 2026-09-20: three runtime primitives, and the two bindings they need

The compiler tranche asked for three small primitives with direct C++ tests and said explicitly that no SDK
regeneration was needed, because the combined corpus measurement runs on the compiler side after the work is
folded. All three are in this checkout. Two of them need a compiler-side binding before emitted code can reach
them, and those are the asks below.

### 1. A read-only dynamic named-property view — `flight::NamedProperties`

`flight/structural_ref.hpp` now carries the view that `Object.keys`, `Object.entries` and a computed `value[name]`
read need over a generated object or a structural row. `explainHost` is the shape it was built against: it
enumerates a host, keeps the members that are objects, and then enumerates each capability group it finds, knowing
neither type.

```cpp
flight::NamedProperties view = flight::named_properties(ref);   // or (structural_row)
std::vector<flight::String> keys = view.keys();                 // source declaration order
bool present = view.has(key);                                   // own string keys only
flight::Any value = view.get(key);                              // absent reads as undefined
bool answerable = view.is_represented(key);                     // does get() raise?
```

Four things are worth stating because they are decisions rather than mechanics.

**Keys come back in source declaration order.** They are recovered from member addresses: [class.mem] ties address
order to declaration order for members sharing access control, and every generated struct declares all of its
members public. A key written into the row after construction has no member to order against and follows the
declared ones, which is where JavaScript puts a property added later. This needs nothing new from the emitter.

**Named string properties and symbol attachments stay separate.** A symbol-keyed property is not an own enumerable
string key and `Object.keys` does not report one. The view reports only names; `flight::AttachedProperties` reports
only symbols; a name and a symbol of the same spelling are two properties and neither view sees the other. This
holds today only because the emitter never reaches a symbol-keyed slot through a `RowKey` — the entity runtime slot
is written through its symbol — so nothing binds it as a named cell. A regression test asserts it on a real
generated entity, because the invariant is the emitter's to keep.

**A read is a sound `Any` or an honest refusal.** `flight::detail::any_from` maps a stored value when it is one of
the ECMAScript language types: a primitive, an object reference, a callable, or an optional wrapping one of those,
where an empty optional reads as `undefined`. `flight::Array`, a structural row and a variant are objects in the
source language but cannot be handed to `Any` without inventing an identity or reinterpreting storage, so `get`
raises `flight::UnrepresentedProperty` naming the key and the C++ type rather than fabricating a value.
`is_represented` asks the same question in advance.

**The view never writes.** It is the read side only, so handing one to a caller cannot become a way to mutate an
object through a name that was never declared.

**The ask:** bind `Object.keys`, `Object.entries` and a computed string-key read over a structural object to these
operations. They are compiler built-ins rather than external profile symbols, so this is not a downstream binding
profile change.

### 2. A checked erased reference — `flight::ErasedRef`

**`flight::Ref<void>` cannot carry a checked binding, and no runtime operation can rescue it.** A
`std::shared_ptr<void>` has already forgotten what it pointed at by the time it is stored, so the only conversion
out of it is `std::static_pointer_cast`: it always "succeeds", and a binding read at the wrong type hands the caller
a pointer to an object of another type. The type has to be captured where it is still known — at the call that
erases it.

`flight/erased_ref.hpp` does exactly that:

```cpp
flight::ErasedRef erased = typedRef;              // implicit; records typeid(T)
flight::Ref<T> recovered = erased.as<T>();        // the stored type only; null otherwise
bool matches = erased.holds<T>();                 // the same question, no reference produced
flight::Ref<T> bound = flight::erased_ref_as<T>(slot);   // over std::optional<ErasedRef>
```

Nothing here is a side table: the type travels inside the value, so there is no registry keyed on object identity,
nothing keeping entries alive, and nothing to go stale. An erasure of a null reference is empty rather than a typed
null, and two erasures of one object compare equal.

**The ask:** emit `flight::ErasedRef` where `flight::Ref<void>` is emitted today for TypeScript `object`.
`ErasedRef` is implicitly constructible from any `Ref<T>`, so `attachEntityBinding(entity, typedRef)` captures the
type at the call site with no other change; `EntityRuntime.binding` becomes `std::optional<flight::ErasedRef>` and
`getEntityBindingAs<Type>` becomes `flight::erased_ref_as<Type>(binding)`. The refusal this clears, at the current
pins, is on `packages/entity/src/binding.ts`: *type assertion target must identify exactly one C++ variant
alternative: target `std::optional<Type>` against `[flight::Ref<void>]`*.

### 3. Host capability seams — audited, and already aligned

No SDL capability is minted through `allocate_entity`/`finish_entity`, given an `entity_runtime_key`, or registered
by capability identity; compile-time assertions now hold that for every capability record the SDL adapters return.
Capabilities are default-constructed generated records populated through callable members, and stateful callables
capture their adapter's explicit shared state; window adapters take their `Window` in the constructor and callers
pass `SDL_Event` explicitly to each `dispatch()` seam.

The only `flight::WeakMap` uses in the SDL host are `SdkWindowBackend`'s `fullscreen_targets` and `input_targets`.
They are provider-owned handle registries, not hidden capability state: the adapter mints opaque target handles and
resolves their identity back to a stable `SDL_WindowID`, weak keys do not retain the handles, and the registry
belongs to one explicitly constructed backend state — separately constructed backends reject each other's handles,
which is now covered by a test. The runtime's structural-row owner registry and attached-symbol registry are
untouched: they are the documented identity-resolution mechanism for rows and symbol properties, not host state.


## Round of 2026-09-17: the four downstream asks are implemented

`agents/flight-cpp-adoption.md` at `fbfcc11` named four items that could only land here. All four are in this
checkout, with native tests and, where the compiler already spells them, a measured effect on the ledger.

### Landed and already elected through a binding profile

- **`CanvasRenderingContext2D`, `CanvasGradient`, `CanvasPattern`, `DOMMatrix`.** `flight/canvas_2d.hpp` implements
  the half of the Canvas 2D contract that is device-independent -- the drawing-state stack, the current
  transformation matrix, path construction, dash lists, gradient stops, pattern parameters, and the settings a
  context was created with -- and delegates pixels to a `Canvas2DRasterizer` a host supplies. There is no rasterizer
  in flight-cpp and no default no-op one: a context created without a rasterizer reports `isContextLost()` and
  throws from every operation that would have produced or read pixels, while continuing to maintain the state the
  runtime owns. Path segments retain the transform in force when each was added, so a transformed arc stays an exact
  arc rather than being flattened at a tolerance the runtime cannot choose. `bindings/web-types.json` elects all four
  types. **Measured: the `CanvasRenderingContext2D` direct refusal goes from 58 modules to 0.**
- **`structuredClone`.** `flight/structured_clone.hpp` deep-copies the runtime's own value domain, preserving shared
  references and cycles, and refusing symbols and callables with `DataCloneError` as JavaScript does. A type with no
  clone definition is a compile-time refusal naming `structured_clone_traits`, not a shallow copy: C++20 cannot
  enumerate an aggregate's members, so a permissive default would be a shallow copy wearing a deep copy's name.
  Elected in `bindings/runtime.json`. **Measured: all three `@flighthq/snapshot` refusals clear.**
- **Per-arm settled results.** `flight::TaskFulfillment<T>` and `flight::TaskRejectionResult` are the two arms
  separately, with `status` spelled as the string the source narrows on and `as_fulfilled`/`as_rejected` to move from
  the union without inventing the member the other arm has. Elected in `bindings/runtime.json` as
  `PromiseFulfilledResult` and `PromiseRejectedResult`. **Measured: both modules clear.**

The whole `runtime external symbol binding plan is incomplete` family falls from **96 modules to 36** on the complete
SDL profile. Sixteen modules emit outright; the rest advance to their next blocker, which is the lower-bound property
the coverage plan describes rather than a disappointment. The profile moves from 1,145 to **1,161 of 2,900** emitted
modules, with refusals from 1,755 to 1,739 and direct refusals from 979 to 960. No module newly refuses.

### Landed and waiting on compiler election

- **The erased dynamic value is `flight::Any`** (`flight/any.hpp`). It is a closed variant over every ECMAScript
  language type this runtime has -- `undefined`, `null`, boolean, number, string, symbol, object reference, callable,
  and an opaque host value -- so a position written `unknown` that holds a number stays a number instead of being
  misstated as `flight::Ref<void>`. An object reference retains its concrete type and comes back only at that type.
  `typeof` reports the language's own answers, `===` never coerces, `SameValueZero` matches NaN with NaN so an erased
  value can key a `Map`, and `Object.is` separates the zeroes. The relational comparison is implemented exactly over
  the primitive domain and throws `TypeError` for a symbol, object, callable, or host operand rather than fabricating
  a `ToPrimitive` result the runtime has no prototype chain to obtain. The one gap is stated rather than faked: there
  is no `bigint` alternative, because the runtime has no arbitrary-precision integer.

  **Missing and present-but-absent stay distinct**, which was the explicit requirement. `Any` holds `undefined` as a
  present value; `flight::AnySlot` (`std::optional<Any>`) is the absence of an entry. `Record<K, Any>::get` returns
  the latter, so a key written with `undefined` and a key never written are different results from the same call, and
  `has` agrees. A native test asserts exactly that pair.

  **This cannot be elected from a binding profile, so it needs a compiler change.** `emitTypeCpp` maps
  `IrType { kind: 'unknown' }` to the literal `'auto'` for every `source` other than `this` and `object`
  (`packages/compiler-backend-cpp/src/cppCompilerBackend.ts`, the `case 'unknown':` arm), and
  `assertCppOutputHasNoUnresolvedTypePlaceholder` then refuses the module. There is no `sourceName` for `any` or
  `unknown`, so `flight-cpp-external-bindings/1` has no way to reach that arm.

  **The change is four lines and it works.** Verified here, not assumed: a local, never-committed patch to that arm --
  `if (type.source === 'any' || type.source === 'unknown') { context.includes.add('flight/any.hpp'); return 'flight::Any'; }`,
  left after the `this` and `object` cases so unresolved-alias residue still yields `auto` and stays refused --
  turns a fixture carrying the corpus's three named blockers into an emitted header that compiles and runs against
  this runtime:

  ```cpp
  struct AnimationChannel : public flight::ReferenceEnabled {
    flight::Any target_ref;
    flight::Array<double> values;
  };

  using NativeWindowHandle = flight::Any;

  struct NodeInteractiveStateBinding : public flight::ReferenceEnabled {
    flight::Any data;
    NativeWindowHandle handle;
  };
  ```

  The compiled fixture reads `42` back out of `targetRef` as a number with `typeof` `"number"`, reports a host window
  handle as `"object"`, and reports an unwritten `any` member as `undefined`. The patched compiler was reverted and
  rebuilt before this checkout's gates were run, so nothing here depends on it; what is recorded is that the
  downstream half is ready and the upstream half is small.

- **The erased-object symbol-keyed property view is `flight::AttachedProperties`**, with
  `flight::attached_properties(ref)` over a `Ref<Object>`, a `Ref<void>`, or a `StructuralRef`. It satisfies each
  part of the ask, and two of them were previously broken rather than merely absent:
  - *One owner per native object identity.* `detail::owner_for` kept a `static` registry **per object type**, so a
    typed projection and an erased `Ref<void>` projection of one object resolved two different owners and therefore
    two different sets of symbol properties. The registry is now one table keyed on the erased address.
  - *Attachments live for the object's lifetime.* The registry held owners weakly, so attachments died with the last
    transient row rather than with the object. It now holds the owner strongly and the object weakly; `NativeRowOwner`
    holds its object weakly in turn, and `StructuralRef` retains the object itself, so lifetimes are unchanged for
    existing callers while attachments now outlive the view that wrote them. An address reused by a later object
    never inherits the earlier object's entries, because expiry is checked on every lookup.
  - *A missing entry differs from a present `undefined`.* `get` returns `AnySlot`; `has` answers without reading.
  - *No copying.* Entries live in the owner, not in the object's storage, and nothing copies the object or its
    properties.

  **The spelling this compiler already emits works as-is.** `particle_emitter_signals.hpp` casts an erased
  `flight::Ref<void>` to `flight::Record<flight::Symbol, std::optional<flight::Ref<ParticleEmitterSignals>>>` and then
  uses presence-bearing `get`/`set`. That construction is now supported: it produces a view onto the object's
  attached properties rather than a copy, `get` returns `std::optional<Value>` so a missing entry and a present
  `std::optional` holding nothing are different answers, and the view shares one store with `AttachedProperties` and
  with a row's computed-symbol accessors because all three resolve the same attachment by object identity. **That
  header compiles at this pin**, so the compiler need not change anything unless it prefers a different spelling.

  Independent compilation of the complete SDL inventory is **1,131 of 1,161 headers** with SDL 3.4.2 present and its
  include flags supplied through `CXXFLAGS`. All 30 failures are genuine generated-code defects and none is in the
  contracts added this round: three each in clipboard, connectivity, geolocation, keyboard, sensors, and shell, two
  each in bitmap and image, and one each in effects-canvas, lighting, media, scene2d-canvas, swf, textbidi, types,
  and video.

### Checked against flight-cpp and deliberately not bound

- **`Function[value]`** (`@flighthq/effects-gl/glShaderTestHelper.ts`) is `new Function(source)()` -- evaluating
  JavaScript source text at runtime. flight-cpp has no evaluator and will not grow one to satisfy a test helper, so
  there is no target to name. This is a deliberate absence, not an oversight; the coverage plan's "check each against
  flight-cpp before adding a table entry" is answered "no target exists".
- **`globalThis[value]` is now bound**, in `bindings/sdl-app.json`, where it belongs: its members are host values the
  profile already binds, and the portable runtime has no global scope to expose. `flight::host_sdl::global_this` is a
  view onto the globals this host already has rather than a second set of them -- `global_this.document` *is* the
  `Document` the `document` binding names, asserted by address in the host test, so a listener registered through one
  is seen through the other. Source that writes `globalThis as Record<string, unknown>` gets `named_globals()`, one
  process-wide store of erased named values shared by every projection. Two of the five modules clear on this alone;
  the other three also need the erased value or the browser-image constructor values.

### Answering the two items handed back on 2026-09-18

**The mutually recursive `Ref<NodeRuntime<...>>` failures are fixed downstream.** The diagnosis was
right and the conclusion that no ordering, include, or forward declaration helps was right; the part
worth adding is *why*, because it points at the fix. The cycle was not between the records. It was in
`flight::Ref` itself: the completeness question lived in a partial specialization of a class template
keyed on the type, deciding `Ref<Node<T>>` instantiated `Node<T>`, whose member asked for
`Ref<NodeRuntime<T>>`, which instantiated `NodeRuntime<T>`, whose member asked for `Ref<Node<T>>`
again -- and a class template specialization cannot be used while it is still being selected, so the
inner question failed outright.

A function template has no such state. `flight/reference.hpp` now asks completeness through overload
resolution, and dispatches on the answer to a specialization keyed on `(type, bool)` rather than on
the type alone, so the inner query re-runs cleanly, finds the type incomplete because it is
mid-definition, and takes the fallback. The answer it lands on is the correct one rather than a lucky
one: a record is owned through a shared pointer whether complete or not, so the inner and outer
queries agree and the type has one meaning throughout the program.

Verified rather than asserted: a fixture matching the reported shape exactly -- `Node<T>` holding
`std::optional<Ref<NodeRuntime<T>>>` and `NodeRuntime<T>` holding
`std::function<bool(Ref<Node<T>>, double)>` -- compiles, runs, and keeps record identity across the
recursive reference, and the whole emitted corpus re-compiles to exactly the same result as before:
1,131 of 1,161 headers, the same 30 failures, none of them new. `Ref` answers identically for
records, forward-declared records, value shapes, `void`, and an already-reference-backed
representation; all of that is now pinned by static assertions rather than left implicit.

One thing the fix exposed that is worth stating, because it was latent before and silent: a type that
names itself through `Ref` and is *not* marked `ReferenceEnabled` had two meanings for one spelling --
a shared pointer inside its own definition and a value outside it -- and the old form hid that behind
class-template instantiation caching. `make_ref` now refuses such a type with a message naming the
fix. Nothing in the emitted corpus hits it; the one type in this repository that did was a test
fixture, and it was wrong.

**On the recurring defect shape: the audit is worth doing, and here is downstream evidence for it.**
Three of the thirty remaining generated-code failures have exactly the signature described -- a lookup
that needed "the declaration this reference names", got nothing, and emitted something anyway with no
diagnostic. They are independent of each other and reproducible from the committed inventory:

- `flight/clipboard/{clipboard,contract,_internal_index}.hpp` emit `Pick<...>` **verbatim** as a C++
  name: `flight::Ref<Pick<flight::Ref<flight::types::HostClipboardChangeProvider>, flight::String>>`.
  The ambient utility resolved to nothing and was printed rather than refused, and its key list
  `'subscribe' | 'unsubscribe'` was lowered as `flight::String` -- a value type -- rather than as a key
  selection. By this compiler's own `Omit` argument, a key projection over a reference-preserving
  subject keeps the subject's row, so the spelling here should be
  `flight::Ref<flight::types::HostClipboardChangeProvider>`.
- `flight/lighting/light_probe.hpp` names `flight::types::max_min_aaa6a2ccb661fee8` in a
  `make_ref` and an aggregate initializer, and that type is declared nowhere: the name appears in
  exactly one file, only as a use. The anonymous-object record was named but never emitted.
- `flight/geolocation/{geolocation_access,contract,_internal_index}.hpp` emit
  `co_return co_await {.reason = flight::String("runtime-unavailable")};` -- a braced initializer with
  no type in front of it, because the target type it should have been qualified with resolved to
  nothing.

All three fail loudly at the C++ compiler rather than at the point the lookup returned nothing, which
is the property that makes the audit worth more than the individual fixes.

### Answering the clipboard hand-over of 2026-09-18: `Required` now has a representation

The diagnosis was exactly right -- the row machinery could make members optional and had no way to
make them required, so `Required<Pick<…>>` unwrapped to the subject and `change->subscribe(callback)`
ended up invoking the `std::optional` rather than the callable inside it.

`flight/structural_ref.hpp` now supplies **`flight::RowRequired<Row>`**, the dual of `RowPartial`.
The spelling to emit for the clipboard case is

```cpp
flight::StructuralRef<flight::RowRequired<flight::RowOf<flight::Ref<flight::types::HostClipboardChangeProvider>>>>
```

with the `Pick<…>` layer collapsing into the subject's row by this compiler's own argument for
`Omit`: a key projection over a reference-preserving subject keeps the subject's row.

Three properties are worth knowing before electing it:

- **The subject's storage does not change.** A member the subject declared optional stays an
  `std::optional` in the object. What the row states is how it is *read*: `row_get` yields the value,
  not the optional, and a write through the row lands back inside the optional the subject declared,
  so a required projection and the object never disagree about where the value lives.
- **A required member that holds nothing is reported, not returned.** `row_get` throws
  `std::out_of_range`, and `row_has` answers false. That is the whole point of the marker: the
  failure the original defect produced -- calling an empty optional -- is now the case that gets a
  diagnostic.
- **`Required` overrides an inner `RowPartial` rather than inheriting from it**, because it is the
  inverse and not the absence of partial, so `Required<Partial<T>>` names every member present.
  `RowReadonly` composes with it in both directions.

Covered by `tests/structural_row_test.cpp`, which runs against the committed generated member table
rather than a hand-made stand-in, so it resolves named members exactly the way emitted code does. One
incidental finding from writing it: `"unsubscribe"` is not a key in that table, while `"subscribe"`
is -- the table carries only the keys the emitted SDK actually projects, so a `Pick` naming both will
need `unsubscribe` to become a projected key before the second member resolves.

**A latent bug fixed on the way.** `RowOwner::named_value` throws `std::bad_cast` when a cell exists
under a different type, which made the second step of the existing `RowPartial` lookup unreachable:
it tries the exact optional-typed cell and then the bare-typed one, but the first call threw rather
than reporting absence whenever storage held the other shape. There is now a non-throwing
`named_value_if` for readers that genuinely accept more than one storage shape, and the partial path
uses it. `named_value` still throws, because asking for one shape and finding another is an error for
a reader with nothing else to try.

The complete SDL inventory recompiles unchanged after both this and the `Ref` fix: 1,131 of 1,161
headers, the same 30 failures, none new.

I have left the braced-initializer question alone, as intended.

### Compiler-side defects this round surfaced

- **A local array literal over native-binding property reads loses its element type.**
  `const restored = [ctx.globalAlpha, ctx.lineWidth];` emits `flight::Array<auto` and refuses with
  `cpp-unresolved-type-placeholder`, although TypeScript types both properties `number` and the same expression
  emits correctly when returned directly. Reproduced against the Canvas 2D binding at `fbfcc11`; the Canvas 2D
  oracle works around it by returning the literal instead of binding it.
- **Three ambient members have no C++ binding row.** `Math.fround` and `Math.SQRT2` stop five modules, and
  `flight::fround` has existed in `flight/math.hpp` since the numeric-conversion contract landed, so those are two
  missing rows in `cppFlightRuntimeAmbientMemberBindings` rather than missing runtime work. `Number.prototype.toFixed`
  was the third, and it did need runtime work: `flight/effects_canvas/canvas_source_mode_compositing.hpp` emits
  `a.to_fixed(3)` on a `double`, which cannot compile. **`flight::number_to_fixed` now exists** and matches
  `Number.prototype.toFixed` including the tie cases, where printf and the language disagree -- printf rounds a tie
  to even and ECMAScript rounds it away from zero, so `(2.5).toFixed(0)` is `"3"` here as it is in JavaScript, while
  `(1.005).toFixed(2)` is still `"1.00"` because 1.005 is really 1.00499999999999989. Every expectation in its test
  was read out of Node. A `'number.toFixed': { kind: 'method', targetName: 'flight::number_to_fixed' }` row is all
  that remains; three SDK modules call it.
- **`captured referent mutation of ctx requires a shared C++ reference representation`** now blocks three
  Canvas 2D modules. A bound context is emitted as a by-value parameter, so a closure that captures it and writes an
  attribute cannot be represented. The runtime side is already shared -- copies of a context share one state stack,
  one rasterizer, and one identity -- so what is needed is the compiler's reference representation for a captured
  binding, not a different runtime carrier.
- **Upstream examples import `@flighthq/host-web/contract` without declaring it.** At Flight `903f328` the example
  package graph no longer validates: `examples/packages/awd2loading/src/render.webgl.ts` and its siblings import the
  Web host package while their manifests declare only `@flighthq/sdk`, and the package graph rejects a module edge to
  a package the owning package does not declare. `scripts/upstreamExampleGeneration.mjs` now records that one package
  name as an explicit addition, closes over its declared dependencies, and adds it to the dependency list of every
  example whose sources name it -- the same class of recorded remap as the existing `renderNative.ts` selector. The
  example inventory grows from 33 examples and 100 selected modules to 34 and 103. This is a Flight-side manifest
  gap rather than a compiler one; the downstream workaround is recorded so it can be removed when the manifests
  declare the dependency.


## Regression still present at 9f6ce1c

`npm run facets:oracle` still emits an invalid declaration pair for the existing conditional-facet fixture:

```cpp
struct TrayWithImage;
using TrayWithImage = flight::FacetRef<TrayIcon, tray_with_image_facet>;
```

The second declaration conflicts with the first, leaving the alias incomplete at every conversion and call site.
The runtime ABI and fixture passed at `993c280`; the failure appears in the compiler's module-reference
forward-declaration path. Alias targets must not receive record-style `struct` forward declarations. The focused
oracle reproduces the same failure at `9f6ce1c`; keep it red until the compiler emits a legal alias dependency order.

The same pin adds a second compiler-owned regression to the downstream aggregate gate. The `arrayBinding` golden
emits the nested tuple default `[1]` as:

```cpp
std::get<0>(array_pattern_value).value_or(std::make_tuple(1.0));
```

The optional contains `flight::Array<double>`, so GCC correctly rejects the tuple argument. This must emit the
represented array value instead. Adding a general tuple-to-Array conversion in flight-cpp would accept unrelated
target-shape errors and is not a valid runtime fix. `npm run compile:check` reproduces this as its sole failing
compiler fixture.

## Complete report sweep

The full graph processes all 154 SDK packages and 2,851 source modules, emits 950 dependency-closed portable
headers, and records 1,901 refused modules: 1,105 direct emission refusals and 796 propagated dependency refusals. The exact headers,
initialization order, package totals, and refusal diagnostics are committed under [`generated/`](../generated/).
`npm run sdk:check` reproduces the tree from the two pins.

The emitted inventory is exposed in the build tree as `Flight::SdkPreview` and as Bazel `//:sdk_preview`. The preview
contains every emitted header but deliberately is not installed as `Flight::Sdk`: independent native compilation is
still red. Run `npm run sdk:compile` to compile every compiler-emitted header and write the full toolchain-specific
diagnostic report to `out/sdk-header-compilation.json`.

## Runtime work adopted downstream

flight-cpp now supplies all runtime headers referenced by the emitted inventory:

- stable shared `ArrayBuffer`, typed-array views, complete numeric `DataView` access, UTF-8 `TextDecoder`, and
  `String::from_code_point`;
- a common `ArrayBufferLike` carrier for ordinary, shared, and host-owned storage, now selected by the runtime binding
  profile and accepted by typed-array/DataView zero-copy constructors, plus the concrete `SharedArrayBuffer` type and
  constructor mapping;
- shared live `MapIterator<T>` and `ArrayIterator<T>` cursors for `keys`, `values`, and `entries`, including
  insertion-order, deletion, post-creation insertion, copy identity, and sticky-exhaustion behavior;
- shared `AbortController`/`AbortSignal` state with first-reason retention, listener dispatch/removal, and
  `throw_if_aborted`, covered by a live compiler-versus-Node oracle;
- immutable shared `Blob` bytes with typed-array construction, slicing, MIME normalization, text decoding, and
  `ArrayBuffer` conversion, plus exact `BlobPart` and `BlobPropertyBag` aliases selected through the runtime profile
  and covered by a live compiler-versus-Node oracle;
- URI component and browser base64 globals with UTF-8, binary-string, malformed-input, padding, and whitespace
  semantics covered by live compiler-versus-Node oracles;
- shared readable/writable stream and async-iterable carriers, including compiler-emitted writer operations and
  native task callbacks; these admit three more independently compiling SDK headers;
- shared decoded-PCM `AudioBuffer` storage with live channel views and bounded channel copies, plus its named
  `AudioBufferOptions` type, selected through the runtime profile and exercised by compiler-emitted native code;
- CPU-backed `ImageData` with exact live `Uint8ClampedArray` reuse, Web IDL dimensions, color-space values, named
  DOM exceptions, and shared identity, plus complete provider-neutral `TextMetrics`, `ImageEncodeOptions`,
  `PermissionDescriptor`, and `PositionOptions` values exercised by live compiler fixtures;
- `TextEncoder` scalar UTF-8 and unpaired-surrogate replacement, whose newly admitted SWF helper compiles after
  shared runtime containers gained JavaScript-compatible logical constness;
- numeric conversion and prefix parsing, ECMAScript radix number formatting, `String.padEnd`, safe-integer checks,
  object keys/values, global and non-global symbols, URL protocol parsing, regular expressions, and a deterministic
  Intl baseline;
- idempotent `Ref<T>` projection, generated structural-row member access, writable entity construction, and
  structural reference casts;
- weak identity maps and sets for Flight references, closed reference variants, and weakly recoverable Flight arrays,
  including erased values and checked typed map views;
- the versioned callable signature/binding ABI used by Signals;
- an explicit JSON value model plus parsing and stringification for every JSON value domain;
- conditional capability facet references with required nested-member checks;
- source-compatible array `length`, resize, insertion splice, and iterable `Array.from` operations, plus variadic
  `Math.min`/`Math.max`;
- typed-array `from` with JavaScript integer conversion after mapping, and `ArrayBuffer.isView` for represented
  buffers and views;
- `RangeError` and `TypeError` classes that accept `flight::String`, preserve the JavaScript name/message surface,
  and share the existing `flight::Error` exception base.

The native suite covers the new ownership and observable behavior. A generated SDK executable links
`Flight::SdkPreview`, calls the emitted interpolation and Entity construction functions, runs under CMake's
development preset, and is declared in the Bazel graph.

## Remaining compiler-owned native failures

The emitted set is dependency-closed in the TypeScript package graph, but dependency closure is not yet the same as
C++ well-formedness. With GCC 15.2, 921 of the 950 portable headers compile independently and 29 fail. The composed
SDL profile emits 1,093 modules, of which 1,049 compile and 44 fail. The new downstream `number_to_string`,
`String::pad_end`, and `Symbol(String)` contracts remove every missing-runtime-symbol diagnostic. The current native
report is entirely emission defects that cannot be repaired by adding another runtime symbol:

- 16 incompatible structural assertions across adjustments, image-codec, and spatial;
- 21 nominal, structural, optional, and `Record` conversions across binpack, font-formats, materials, media, mesh,
  particles, physics3d, scene2d-formats, and skeleton2d;
- six recursive-alias declaration failures for `TiledLayer` and `FlightDocumentValue`; and
- one incorrectly optionalized XML replacement-callback parameter.

The stricter compiler newly refuses 16 direct roots that the previous portable sweep emitted. Five need equivalent
source-union evidence, five expose generic typed-array backing domains that are not represented by the concrete C++
aliases, three contain unresolved callable result types, one needs named structural-row construction, and two retain
an unresolved alternative in a scene-light-selection union. Their dependency refusals account for the rest of the
35 previously emitted modules absent from the full SDL profile. Returning `MapIterator<T>.next().value` as
`T | undefined` also reaches `contextual optionalSingle construction requires expression type evidence`; the exact
iterator binding and its `done` path compile in the live runtime oracle.

The compiler-emitted RegExp fixture now verifies global `String.match`, substitution tokens, empty global matches,
and callback offsets against Node. One contextual callback edge remains: an unannotated second parameter for a
zero-capture `String.replace` callback currently emits as `flight::String`, although JavaScript supplies the numeric
match offset at that position. An explicit `offset: number` annotation emits the correct `double` and passes the
native oracle. Capture-aware callback typing must also preserve the runtime `undefined` value for unmatched groups;
the TypeScript library's broad callback signature does not expose that absence by itself.

The current compiler can now contextualize Flight's nested `GPUBlendComponent` literals, so flight-cpp binds
`GPUBlendComponent`, `GPUBlendState`, and `GPUStencilFaceState` to provider-neutral value carriers. Seven affected
modules advance to their next union-evidence, optional-callable, anonymous-property, or `Record`-spread failures.
The profile now also supplies buffer/texture descriptors, iterable extents, shared buffer/view sources, texel-copy
layouts, the complete bind-group resource and layout value family, and the render-pipeline descriptor family. Their
pass-through signatures and
declaration-ordered compiler fixtures compile. Three production scene3d-wgpu modules advance past their ambient
refusals: one reaches `typeOf` computation, one reaches dense-array length construction, and the inline `{ buffer }`
resource in `wgpuMeshPipeline.ts` reaches `anonymous object property buffer requires concrete C++ type evidence`.
`wgpuTestHelper.ts` no longer requests `GPURenderPipelineDescriptor`; it remains refused on its browser-image and
test-host names, so the full SDL profile still has 120 direct ambient-refusal records and emits 1,098 modules.
The shared SDL image-source profile now maps every image/video/bitmap/offscreen/SVG/frame constituent type to one
decoded RGBA owner with retained source kind and a cohesive weak policy. Direct ambient-refusal records fall again,
from 120 to 112, while the emitted closure remains 1,098. `glDraw.ts` now reaches contextual source-union evidence;
`wgpuDraw.ts` retains only its browser constructor values. Those values should select a compiler-lowered external
runtime-type test against the retained kind rather than require a browser DOM implementation.
Direct external descriptor literals also preserve source property order in C++ designators; arbitrary valid
TypeScript property order can therefore violate the target aggregate's declaration order. Contextual target types
and ordered-designator emission remain required for inline resource construction and property-order-independent
construction.

These diagnostics arise after the runtime includes resolve, and many occur in a header before later errors in that
header can be observed. The JSON report from `npm run sdk:compile` is the compact handoff surface for fixing them in
flight-compiler. Downstream source rewriting would obscure compiler provenance and produce a second, unversioned
transpiler, so the checked-in SDK remains the compiler's exact output plus its generated build/member inventories.

The SDL lifecycle profile now supplies the exact `CustomEvent<T>`, navigation timing, document visibility, and focus
carriers. The headless profile also maps `PerformanceEntry` and `PerformanceEntryList` to the standard value and
base-entry array while its native navigation query returns the narrower navigation-timing array used by Flight. Two
compiler contracts remain before that module can use them faithfully. Generated object-valued
`event.detail` access must dereference `Ref<T>` rather than emit `event.detail.member`, and copied callable values must
retain source identity so `removeEventListener(type, listener)` can remove the registration added before the listener
is captured by the returned unsubscribe closure. Downstream now provides `flight::Function<Signature>`, whose copies
share a weakly recoverable identity, and the SDL element/document/window stores deduplicate and remove it using the
DOM type/callback/capture key. flight-compiler must emit that carrier for JavaScript function values; the maintained
profile cannot bind removal while it still emits `std::function`. A diagnostic removal mapping moves `lifecycle.ts`
to the separate "WeakMap value requires a proven C++ representation" refusal, confirming the next boundary.

Several direct ambient-member refusals now have exact downstream targets and need only compiler election:

- `Number.parseInt` → `flight::parse_int`, `Number.parseFloat` → `flight::parse_float`, and
  `Number.isSafeInteger` → `flight::is_safe_integer` from `flight/number.hpp`;
- `Object.values` → `flight::object_values` from `flight/object.hpp`;
- iterable `Array.from` → `flight::array_from` from `flight/array.hpp`;
- typed-array `from` → the selected concrete alias's static `from`, such as `flight::Uint32Array::from`, from
  `flight/typed_array.hpp`;
- `ArrayBuffer.isView` → `flight::is_array_buffer_view` from `flight/array_buffer_view.hpp` when the argument has a
  represented closed buffer/view domain.
- `Math.fround` → `flight::fround` from `flight/math.hpp`, and each typed-array constructor's
  `BYTES_PER_ELEMENT` → its `bytes_per_element` constant from `flight/typed_array.hpp`.

Length-only `Array.from({ length }, mapper)` still requires dedicated compiler lowering for its implicit
`undefined` elements. Open `object` arguments to `ArrayBuffer.isView`, `Object.prototype.hasOwnProperty.call`,
generic structured cloning, and `Promise.allSettled` results still need represented compiler contracts; the runtime
does not provide a permissive erasure for them.

The runtime profile now binds `Iterable<T>` to an owner-preserving `flight::Iterable<T>` carrier. It removes this
ambient type from 14 full-SDK roots and from the snapshot and spritesheet example roots. Ten of the full-SDK roots
advance immediately to named compiler failures: anonymous-object evidence, target-name candidate identity,
source-union evidence, `typeOf` lowering, optional construction, captured reference representation, structural-union
representation, or optional-variant construction. In the selected examples, spritesheet advances to its Canvas
types and snapshot reaches `ambient value Array member from has no C++ binding`. `Array[value]` is already reserved
by the compiler runtime plan, so an external member mapping for `from` is rejected as a duplicate; the compiler must
elect the existing `flight::array_from` helper in that intrinsic plan.

The latest compiler still records direct refusals for all seven member families above. Newly emitted generic typed
arrays also expose a distinct representation issue: TypeScript's current library models backing storage as
`Uint8Array<ArrayBufferLike>` and peers, but C++ emission applies the concrete aliases as templates. The runtime must
not redefine each concrete element alias as a generic backing template; the compiler must either erase a proven
backing parameter to the existing `ArrayBufferLike` member or emit a compatible generic carrier deliberately.

The runtime profile maps both spaces for `RangeError` and `TypeError` to the semantic runtime classes in
`flight/error.hpp`. Current generated headers no longer fail by trying to construct `std::range_error` from
`flight::String`; the remaining failures are later optional-unwrapping or missing-symbol emission defects.

The Web-types profile maps both `ImageData` spaces, `ImageDataArray`, and `ImageDataSettings` to the downstream
CPU-backed RGBA carrier. This removes `ImageData` from every direct refusal and moves bitmap/effects modules to
their `CanvasRenderingContext2D` provider boundary and image-codec modules to their bitmap/canvas provider boundary.
The composed SDL direct ambient frontier falls from 124 to 123 modules. The downstream `DOMException` carrier has
exact message, name, and legacy-code values, but its ambient constructor remains deliberately unbound: the current
`typeof DOMException !== 'undefined' && error instanceof DOMException` path emits C++ `instanceof` syntax and member
access on the erased catch value. flight-compiler needs a represented external runtime-type-test operation before
that global can be selected soundly.

The same profile removes `TextMetrics`, `ImageEncodeOptions`, `PermissionDescriptor`, and `PositionOptions` from
every direct refusal. Each affected root advances to its remaining Canvas, offscreen-canvas, geolocation,
permissions, media, or wake-lock provider contract. The emitted closure remains 1,098 because no root was otherwise
dependency-complete; these portable values are ready for those host adapters without choosing their implementation.

The runtime now also provides `flight::all_settled_tasks(Array<Task<T>>)` and
`Task<T>::all_settled(std::vector<Task<T>>)`, returning ordered `TaskSettlement<T>` values without rejecting the
aggregate. The compiler can bind `Promise.allSettled` to the free function after it maps
`PromiseSettledResult<T>` to that carrier and lowers the source union's `status`, `value`, and `reason` access. A
disposable exact-pin member mapping already removes the only direct all-settled refusal; the scene-resource module
then reaches its existing `resolveScene3DResources` dependency refusal, so this change does not alter the current
emitted-header total by itself.

## Complete example sweep

`npm run examples:generate:check` now gives flight-compiler a stable downstream example gate. It discovers all 33
packages under Flight's `examples/packages`, inventories all 181 source modules, and selects the 100-module native
SDL/GL lane: each `render.ts` selector and chosen `render.webgl.ts` implementation receive an explicit recorded
`renderNative` source remap, with the DOM-only cross-backend example recording its fallback. Example output has its
own `examples/upstream/generated/include/flight/examples` tree and references the single canonical SDK tree rather
than copying SDK headers per example.

At the current pins, dependency closure emits 0 of those 100 modules. The linked ledger contains 46 dependency and
54 emission refusal entries. Forty example modules are held by the refused `@flighthq/sdk` barrel and
20 renderer modules are held by the refused `@flighthq/host-web/contract` path through `webGraphicsHost`. The exact
per-module evidence is committed in `examples/upstream/generated/refusals.json`.

The separate frontier ledger deliberately compiles each selected example without its package dependencies, so its
29 missing package-evaluation entries are boundary markers rather than claims that the full graph omitted those
packages. It records 33 dependency and 38 direct emission boundaries, led by contextual optional construction,
captured referent mutation, contextual typing for empty arrays, and package evaluation. Seven application roots now
report `runtime external symbol binding plan is incomplete (missing: R[type])`. No `R` external type exists in the
pinned Flight sources, so this is synthesized generic evidence escaping into the external-binding plan and cannot be
made sound by adding a downstream binding. The SDL
application profile resolves every direct keyboard, pointer, wheel, gamepad-button, rectangle, DOM attachment,
window, animation-frame, generic iterable, and concrete HTML control ambient in the chosen lane. Those HTML bindings
expose intersection, `typeOf`, contextual union, multi-variant, optional construction, and empty-array compiler
failures in eight UI-heavy examples. Remaining direct names describe Canvas 2D, media, and the sound example's
`AudioContext`; each stays explicit until
its compiler or host contract exists.

The remaining integration request is a first-class source/package remap in the programmatic graph API. Flight uses
build-time renderer aliases and its examples import Web host providers. `flight-cpp` can name selected native
renderer and host modules, but it should not edit upstream import strings or maintain a second TypeScript transform
to do so. A remap must be importer-specific, recorded in provenance, participate in both type and module-evaluation
resolution, and continue to verify that the replacement exports every requested name. With that contract, the
handwritten SDL package can replace Web lifecycle/input/context providers while generated Flight GL modules retain
renderer ownership.

The SDL cursor side of that replacement is now concrete. `Flight::HostSdlSdkCursor` and Bazel
`//:host_sdl_sdk_cursor` populate the compiler-emitted `flight::types::CursorBackend` record and execute it against
SDL's main-thread system-cursor API. The interaction and sound examples still import `createWebCursorBackend`; the
remap must select a native provider with the same requested export before those entry points can consume this
adapter without source rewriting.

The SDL window side now also populates emitted `ApplicationVisibilityBackend` and the request/exit portion of
`FullscreenBackend` through `Flight::HostSdlSdkWindow`. It also populates the dependency-closed input-target,
focus, file-drop, and pointer-lock records; their returned release closures do not require callback comparison. The optional fullscreen listener fields and the required
`ApplicationExitBackend` listener pair are deliberately left unwired: the provider must remove the same JavaScript
function object passed at subscription time, which cannot be recovered from separately copied `std::function`
values. Emitting the prepared `flight::Function` carrier unlocks those exact adapters.

The dependency-closed `ClipboardTextBackend` is also populated by `Flight::HostSdlSdkClipboard`; SDL covers its
clear, presence, UTF-8 read, and UTF-8 write methods without a new compiler contract. Rich clipboard records remain
absent until a native provider implements those separate capabilities.

`Flight::HostSdlSdkPlatform` now populates the dependency-closed `PlatformBackend` and writes SDL/native facts into
the exact caller-owned `PlatformInfo`. It needs no additional compiler behavior and leaves unavailable version,
build, and distribution fields at their specified sentinels.

`Flight::HostSdlSdkDevice` also populates the dependency-closed `DeviceBackend`. SDL supplies input-device presence,
desktop-display dimensions and pixel ratio, per-window safe-area geometry, CPU count, configured memory, and native
platform identity; unavailable identity and environment fields retain the upstream sentinel contract. This backend
needs no further compiler work and resolves its SDL window id on each dynamic read.

`Flight::HostSdlSdkHaptics` populates the emitted `HapticsBackend` without compiler changes. It uses SDL gamepad
rumble for the continuous and named feedback operations, accepts the emitted optional impact intensity, dynamically
tracks a capable connected device, and leaves
multi-step pattern/waveform capability absent because SDL has no corresponding timed primitive.

`Flight::HostSdlSdkScreen` populates the dependency-closed `ScreenQueryBackend`, `ScreenDetailsBackend`, and
`ScreenChangeBackend` without compiler changes. SDL provides native multi-display enumeration, permission-free
access, and display events. The adapter caches the prior display snapshots, constructs the generated readonly
structural change-event view, and returns an idempotent release closure for each subscription.

`Flight::HostSdlSdkKeyboard` populates the emitted soft-keyboard info, visibility, and change records without
compiler changes. SDL's shown/hidden events and returned release closure satisfy the subscription contract directly;
unsupported desktop drivers return the existing failure-domain values. Platform-only style and layout setters remain
absent capabilities.

## Host boundary

The manifest-free generation remains the portable floor. Browser, media, Node, and graphics handles require explicit
binding profiles. SDL owns lifecycle and GL, Vulkan, or WebGPU surface acquisition; generated Flight renderer
packages own rendering behavior. These host bindings increase the emitted module set from 950 to 1,093, while the
portable compile gate remains useful and independent of platform SDKs.

The SDL host now also implements the complete emitted `AudioDeviceBackend` operation record over SDL's device
callback, including PCM buffer acquisition, concurrent source playback, live gain/pan/rate, bounded regions,
teardown semantics, and application-thread completion delivery. Its generated-record adapter compiles and runs at
this pin. A native example also executes the upstream sound example's compiler-generated procedural PCM through the
SDL backend under CMake and Bazel. The compiler still needs importer-specific module remapping to replace
`webAudioDeviceBackend` in the full sound example. The runtime now represents the `AudioBuffer` stored by
`AudioResource` and created by
`createAudioResourceFromSamples`; encoded-byte decoding still requires an explicit native codec/provider contract.
The host playback seam is no longer part of that blocker.

The downstream `flighthq/flight-cpp/sdl-gl/1` profile now names concrete SDL-owned canvas/context types, shared GL
object handles, context attributes, a weakly recoverable image-source carrier, and the standard anisotropy,
float-buffer/filtering, ASTC, BPTC, ETC, PVRTC, RGTC, S3TC, and S3TC-sRGB extension domains. Composed with the runtime
and exact Web string-alias profiles it emits 1,081
modules, 32 beyond the runtime/headless inventory; 23 of those additions compile independently. The extension binding removes its direct
ambient refusal; `GlContextRuntime` then
reaches `flight-cpp WeakMap value requires a proven C++ representation`. The next renderer-wide compiler blocker is exact:
`packages/types/src/GlContext.ts` retains
`auto viewport;` after its closed `Pick<WebGL2RenderingContext, GlContextMember>` is materialized. The fail-closed
placeholder gate correctly refuses it. The materialized surface also has six C++ target-name collisions because the
current snake-case transform maps `ACTIVE_TEXTURE`/`activeTexture`, `CULL_FACE`/`cullFace`,
`DEPTH_FUNC`/`depthFunc`, `FRONT_FACE`/`frontFace`, `STENCIL_FUNC`/`stencilFunc`, and `VIEWPORT`/`viewport` to the
same six names. The mapped-interface fix must assign distinct stable names before the record can be well formed.
Once all picked constants and methods receive their concrete callable types,
flight-cpp can populate the generated `flight::types::GlContext` record from its working `WebGl2Context`; the host
adapter already forwards shared GL object lifetime and every non-polymorphic command selected by Flight: buffer and
texture upload, compressed texture upload, framebuffer clear/blit/readback, shader/program compilation, render
state, vertex attributes, draw calls, and uniforms. A compiler fixture emits representative calls for that complete
surface directly through the external `WebGL2RenderingContext` binding and compiles them. Its closed `getParameter`
result carrier converts to every numeric, boolean, array, and identity-preserving GL handle domain used by Flight;
`getExtension` supplies feature presence and anisotropy enums. The SDL tween also executes the ordinary texture,
framebuffer, readback, state-query, shader, draw, and presentation paths against an offscreen GLES 3 context. Dynamic
compressed-extension enum lookup still depends on the compiler selecting flight-cpp's ordered
`Record<String, double>` representation and lowering optional indexed access. This proceeds into `render-gl` without
adding an SDL renderer.

The added extension domains remove every direct refusal for those interfaces. `glCompressedTexture.ts` advances to
dual-sentinel optional-chain lowering, `glRenderTarget.ts` advances to nullish-coalescing presence lowering, and
`glEnvironmentIblBake.ts` advances to an unrelated contextual `flight::Map` union conversion. The dependency-closed
header count remains 1,098 until those compiler-owned boundaries move.

Composing `bindings/sdl-app.json` with the maintained renderer profiles admits the represented window, document,
`HTMLElement`, keyboard, mouse, pointer, wheel, and animation-frame cancellation contracts; these now reach their
next compiler or package dependency failures in the full SDK graph. The runtime profile maps `PromiseLike<T>` to the
existing `flight::Task<T>` carrier; the dialog module consequently advances to the compiler's async-closure
coroutine-lowering refusal.

The same profile maps `DOMRect` to the host's complete eight-field `ClientRect`, which is populated from SDL logical
canvas dimensions. This removes the name from all eleven selected example roots that referenced it. Four of those
roots now stop directly at compiler-owned union, captured-reference, intersection, or SDK-dependency boundaries;
the others retain separate Canvas, listener-option, or HTML-control requirements. In the full SDK sweep, direct
ambient-binding refusals are now 111 modules, down from the original 242-module SDL/GL frontier, while the
dependency-closed total stays at 1,098 modules.

The runtime profile also binds the global `Boolean` value to a callable object with exact represented truthiness.
This supports both direct conversion and `filter(Boolean)` without overload erasure. `svgDocument.ts` now reaches
the existing `Number.parseFloat` intrinsic gap, whose downstream `flight::parse_float` operation is already present.
Closed `std::variant` unions use the same operation for logical negation. That removes the invalid `operator!`
diagnostic from all 22 affected SDL headers: `texture/sampler.ts` now compiles, while the other 21 expose the
existing optional-number unwrapping defect.

The runtime profile now maps `ArrayIterator<T>` to shared live cursors over `Array.keys`, `Array.values`, and
`Array.entries`. `tiledXmlParse.ts` moves immediately to the existing concrete typed-array backing error:
`flight::Uint32Array<flight::ArrayBuffer>` is emitted as though the concrete alias were a template.

The SDL profile now also supplies `Event`, `CompositionEvent`, `InputEvent`, `Gamepad`, `GamepadButton`,
`GamepadEvent`, `navigator.getGamepads()`, and `AddEventListenerOptions` with compiler-checked native carriers. SDL's
gamepad carrier exposes the standard `pressed`, `touched`, and `value` button fields and preserves them in each
polled snapshot. The listener option carrier represents optional capture, once, passive, and abort signal fields;
the shell removes once registrations before reentrant dispatch and removes signal-bound registrations on abort.
Filesystem and scene-resource roots now advance to awaited-union and source-union compiler failures, and four of the
five affected example apps advance to contextual-union or `typeOf` failures; tilemap retains only its Canvas 2D gap.
`packages/input/src/inputManager.ts` consequently reports only `EventTarget[type]`. A focused compiler probe with an
otherwise valid `EventTarget` binding fails with `flight-cpp type position retains unresolved auto placeholder:
std::function<auto` for `addEventListener` and `removeEventListener`. The compiler needs to materialize the listener's
event parameter and elect the downstream `flight::Function<Signature>` carrier. Its copies preserve identity, and
the SDL host already accepts it for add/remove operations with the DOM capture-key rule.

The provider-neutral `flighthq/flight-cpp/sdl-wgpu/1` profile now supplies typed shared identity for 18 WebGPU
object domains, exact adapter capability metadata, standard usage flags, and weak-key policies. The composed
`sdl-image` profile supplies its external-copy source. On its own the WebGPU profile adds
18 dependency-closed headers over runtime/headless and all 18 compile. Composed with SDL/GL and the application
shell, it raises the inventory from 1,081 to 1,098 headers; all 17 additions compile and direct ambient-refused
modules fall from the SDL/GL profile's 242 to 163. The device, origin, vertex, external-image, buffer, texture,
extent, shared-buffer-source, texel-copy, sampler, bind-group, and render-pipeline descriptors now have
compiler-checked native representations.
Optional source fields preserve their presence; binding the sampler advances
`wgpuRenderState.ts` to `dual-sentinel optional chaining requires presence projection lowering`. `wgpuHost.ts`
reaches contextual optional construction, while `wgpuExternalImageSource.ts`
retains only browser constructor values (`DOMException`, image/video/canvas/bitmap/frame); both are compiler or
browser-adapter boundaries rather than missing native WGPU data contracts.

Regenerate and validate from the repository root:

```sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm run sdk:generate
npm run sdk:check
npm run sdk:compile
cmake --preset development
cmake --build --preset development
ctest --preset development
```
