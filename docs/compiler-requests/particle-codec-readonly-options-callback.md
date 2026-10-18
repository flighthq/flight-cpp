# Particle codec readonly options callback

Pinned Flight `a62784923f3be814463286c9fb28edfe6ab15789` and compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`.

The complete original Unity parser now compiles and runs. All three particle barrel headers
next fail at `register_built_in_particle_formats.hpp:171`, where the original
`starlingPexParticleFormat.codec.parseToConfig` stores `parseStarlingPex` directly.

The source is structurally valid:

```ts
interface ParseParticleConfigOptions extends ParticleDesignerParseOptions,
  UnityParseOptions, LibgdxParseOptions, StarlingPexParseOptions {}

// ParticleFormatCodec
parseToConfig(text: string, options?: Readonly<ParseParticleConfigOptions>): ParticleEmitterConfig;

// Original exported Starling entrypoint
function parseStarlingPex(xml: string, options?: StarlingPexParseOptions): ParticleEmitterConfig;
```

The generated slot takes
`optional<StructuralRef<RowReadonly<RowOf<Ref<ParseParticleConfigOptions>>>>>`,
but the retained public function takes `optional<Ref<StarlingPexParseOptions>>`.
The function pointer cannot construct that `std::function` slot. The document callback has
the same source boundary. Both original Starling entrypoints only read
`options?.textureSize ?? 1`; neither stores, mutates, compares or returns the options owner.

An arbitrary structural options literal is a source-valid codec argument. It need not own a
native `StarlingPexParseOptions` allocation. Recovering only native Starling allocations would
narrow this domain; constructing a Starling owner would copy fields and change identity and
live reads. Structural similarity alone cannot justify either operation or generic callable
variance. The established canonical callable-declaration fix does not adapt distinct parameter
owners, and GL shared-cell declarations do not supply this options bridge.

Required proof includes absent and present options, absent versus present fields, a live field
updated through another view, the same source owner and callback identity, single argument
evaluation, exact source defaults and errors, native public `Ref` compatibility, and exclusion of
mutating, escaping, identity-observing and opaque callees. A private body specialization or
another existing read-view mechanism must earn those properties. A new public parameter or
callback carrier needs a bounded architecture decision with its exact type and site changes.
No adapter, copied options owner, unchecked cast or public ABI change is supplied by the Unity unit.

The measured Unity unit is one passing header and three failed barrels, all four attempted.
The particle package remains partial at 17/18 emitted modules. Later retained refusals in
registration and the nullable JSON Record helper are separate boundaries; this first error
does not establish that clearing it will qualify the barrels or registry behavior.

## Exact decision boundary and independent controls

The strict type fixture `scripts/fixtures/particleCodecOptions.ts` admits the original assignment,
an ordinary structural options literal, direct function identity comparisons and a result assertion
that reads the original extra document. `scripts/particleCodecOptionsOracle.mjs` executes the
untouched pinned Starling, factory, constants and diagnostics bodies: twelve checks pass for the
two callback identities, structural getter/live reads, one argument evaluation, absent options and
fields, present undefined, failure before options access, the exact thrown owner and retained
document surface. A field-only result copy and a forwarding function are explicit counterexamples,
not proposed implementations.

The installed callback slots are still:

```cpp
// Existing public parser ABI; must remain callable with this exact native function-pointer type.
Config parse_starling_pex(String, optional<Ref<StarlingPexParseOptions>>);
// Existing codec slot; source callers may provide a row with no native Starling allocation.
std::function<Config(String, optional<ReadonlyParseOptionsRow>)> parse_to_config;
```

The document slot has an additional owner boundary. Its current result is an owning
`Ref<config_diagnostics_...>`, whereas the public parser returns `Ref<StarlingPexParseResult>`
with the real `document` property. A copy of only the statically declared fields would replace
the original result owner and lose a property the source actually returns. Clearing the options
error alone does not establish a sound document callback.

The existing contextual callable entrypoint deliberately does nothing when both signatures have
the same arity. Its lower-level parameter conversion can project native owners into rows; it
cannot manufacture the nominal owner this opposite direction requires. Extending that arity
guard alone therefore supplies no proof. Existing `NominalOrStructuralView` needs a native base
reference and exposes native escape; it does not represent every schema-only options row.

A possible implementation shape is one private original-body implementation over a readonly
view plus the unchanged native public wrapper, with contextual calls entering the private body.
That would still require preserving the original named function identity through an adapted
`std::function`, and preserving the original document result owner through the declared result
surface. Current `Function` shares identity only within its signature; an arbitrary forwarding
lambda in `std::function` correctly reports that identity is unavailable. That was the architecture boundary presented to Manager. Manager subsequently approved the
bounded implementation below; no function-pointer cast or field copy is used.

The registered native boundary fixture keeps the real public parser function-pointer type and
checks that a structural row is not implicitly convertible to nominal options. Its independent
one-member row protocol uses the installed `size` key: the normal SDK has no `textureSize` key
entry until a real lowered row read asks for one. No generated member metadata is fabricated, and
these protocol checks do not claim an installed particle-options view or codec adapter. Mutating,
escaping, identity-observing and opaque parameter uses remain required exclusions for any future
private specialization proof. These controls alone establish no installed adapter or header promotion.


## Approved implementation and preparation evidence

Manager approved the two Starling assignments and Foreman approved the four exact-shape Unity and
ParticleDesigner config/document assignments. The declared
`compiler-readonly-named-callable-views.patch` adds a second entrypoint emitted from the original
function IR, beside the unchanged public native function. Its options parameter uses the existing
readonly structural row. Only synchronous nongeneric two-parameter functions with a required string
and optional nominal options containing one optional numeric property qualify. Every use of the
options binding must be a direct optional property read outside mutation or nested capture.
Aliases, opaque calls, owner comparisons, truthiness, typeof, indexed/asserted access, wider options,
defaulted options and already-structural signatures retain their previous path. No parser algorithm
or generated body is authored by this patch.

The contextual store uses `flight::NamedFunction<Signature>`, an opt-in `std::function` target holding
the existing `Function` invocation state and an original-declaration token. The token is the address
of an inline object specialization keyed by the native function, without a function-pointer cast.
Original aliases and translation units share it; distinct functions with identical bodies do not.
The ordinary `Function` layout and `std::function` ABI remain unchanged. Compatible raw native
function-pointer targets compare against the retained original pointer; unknown erased closures
still report unavailable identity explicitly.

The companion `particle-codec-document-result-owned-view.patch` wraps the existing two-field result
in `Required<{config: ParticleEmitterConfig; diagnostics: ImportDiagnostic[]}>`. Both fields were
already required and remain writable: the strict TypeScript fixture checks assignment both ways and
writes through both spellings. The existing Required row carrier can retain the full native result
owner. The callback projection requires each target field to have the same optionality, writable
source cell, exact emitted storage, and identical original member-type declaration resolved through
its own imports/barrels. Distinct declarations with the same fields fail this proof. Extra `document`
storage stays on the original owner and base-view writes reach that owner.

Preparation on the current 110-patch preceding compiler stack, including the latest nullable JSON
root guard and symbol own-presence changes, has these results:

- All declared preceding patches and this candidate apply cleanly to raw compiler
  `806d257d2f4100de745cc97340e61c2f301b0f24`; the strict production build passes.
- Sixteen new source assertions pass, including imports/barrels and distinct-declaration exclusions.
  Nine unsafe parameter-use cases are unchanged against that exact preceding stack: seven headers
  byte-identical and two identical structured refusals.
- Existing callable ABI, arity, identity and private JSON graph source controls pass 40 assertions.
  One existing native case is explicitly unselected by the local `node:assert` adapter; this is not
  a Vitest run. The separate emitted native control supplies the semantic checks for this change.
- Registered runtime, named-origin and public native particle-boundary CTest targets pass 3/3.
  The origin target checks signatures, compatible raw pointers, effects/errors and translation units.
- `scripts/particleCodecOptionsNativeOracle.mjs` compiles and runs a synthetic fixture emitted by
  the production compiler. It checks the unchanged native ABI, schema-only and native shared options,
  live getter/default/error behavior, original function identity and writable original result/document.
- `scripts/particleCodecOptionsOracle.mjs` passes 48 checks over untouched pinned Starling, Unity
  and ParticleDesigner bodies, including each original callback, defaults, getter effects and errors.
  Unity config validates before reading options; Unity document reads options before validation.
  Those different source timings are preserved, not normalized.

An isolated emission from the retained normal original graph now selects all six approved stores;
registration remains partial because of other retained refusals. This is source preparation, not a
normal SDK run or an installed registration claim. No generated headers, refusal ledger rows,
capability declaration or package/header completion count changes in this preparation. The prior
installed result remains one passing Unity header, three failing barrels and 17/18 particle modules.
The coordinated builder2 Types generation must supply the normal equivalent result declaration;
only then can the one bounded particles run qualify actual installed callbacks and report the next
concrete boundary. A-prime was not applied or needed for these current-owner controls.
