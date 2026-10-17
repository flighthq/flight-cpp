# A retained declaration must not call a declaration the module omitted

When one declaration in a module is refused, the emitter still emits the module's other declarations —
and at least one of them calls the refused one. The retained caller is written out in full, with no
placeholder, no declaration of its callee, and no marker on itself, so the PARTIAL header fails to
compile at a name that never existed. The refusal costs the module far more than the refused
declaration: the whole header is unusable, and so is every header that re-exports it.

The emitter already has the rule for the *module* boundary — `contract.ts` and `index.ts` are refused
with `dependency-refused` because `./videoResourceFrom.ts` was — but not inside a module, which is where
this costs the header.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/video`.

## Observed

`generated/include/flight/video/video_resource_from.hpp` omits `load_video_resource_from_url` (source
line 32) and retains the module's other two functions. `load_video_resource_from_blob` (line 11) is
emitted in full and contains:

```cpp
    (resource = co_await load_video_resource_from_url(host_video, url, options, signal));
```

`load_video_resource_from_urls` (line 37) retains the same call. Neither name is declared anywhere in
the header, and `video_resource_from.hpp` has no `NOT GENERATED` marker over either caller.

```
flight/video/video_resource_from.hpp: use of undeclared identifier 'load_video_resource_from_url'
flight/video/contract.hpp:            use of undeclared identifier 'load_video_resource_from_url'
flight/video/_internal_index.hpp:     use of undeclared identifier 'load_video_resource_from_url'
```

Three of the package's five headers fail for this one reason. The refused function is filed separately as
[`image-source-union-domain-discriminator.md`](image-source-union-domain-discriminator.md) — that request
is about *why* `loadVideoResourceFromUrl` cannot be lowered; this one is about what happens to its
callers, and it would outlive that refusal: any partial module whose retained functions call its refused
ones has the same outcome.

## Second measured site: `@flighthq/midi`

The same failure, from the same increment, at `generated/include/flight/midi/midi_subscription.hpp`: the
module omits `untrackMidiSubscription` (source line 279) and its retained declarations call it.

```
flight/midi/midi_subscription.hpp: use of undeclared identifier 'untrack_midi_subscription'
flight/midi/contract.hpp:          use of undeclared identifier 'untrack_midi_subscription'
flight/midi/_internal_index.hpp:   use of undeclared identifier 'untrack_midi_subscription'
flight/midi/midi_access.hpp:       use of undeclared identifier 'untrack_midi_subscription'
```

Four of that package's seven headers fail, every one of them at `midi_subscription.hpp:164`. The omitted
function is filed separately as
[`optional-property-call-result-evidence.md`](optional-property-call-result-evidence.md); the module's
three other omissions are filed too and none of them is what breaks the header today. `@flighthq/midi` is
`partial` with those four failures reported.

## Reduction

The reduction is the emission itself: the refusal is in `loadVideoResourceFromUrl`, and the two retained
functions in the same module both call it. Removing the refusal removes the failure — the two retained
bodies are otherwise faithful, and `videoResourceFrom.test.ts` is their specification.

## What would close this

When a declaration is dropped, propagate the omission through the module's call graph as the `dependency`
stage already does across modules: either omit its retained callers too and mark each with its own
`NOT GENERATED` line, or emit the dropped declaration as a placeholder signature so the retained callers
resolve. The second is the shape
[`partial-module-placeholder-qualification.md`](partial-module-placeholder-qualification.md) already
establishes for a *name*; this asks for it one step further out, for a *callee*. Either is acceptable as
long as the retained header compiles: a partial module that cannot compile is not partial, it is refused,
and the ledger should say so.

## Distinguish from the requests already filed

[`partial-module-placeholder-qualification.md`](partial-module-placeholder-qualification.md) is about
qualification and includes for a retained declaration whose dependencies *were* emitted — names in scope.
[`compiler-imported-alias-call-declaration-recovery.md`](compiler-imported-alias-call-declaration-recovery.md)
is about recovering an imported alias's declaration across a boundary. This one is about a callee that
does not exist anywhere in the emitted tree because the same module refused it.

## Carrying workaround

None. The module is 51 lines and a hand supply would be easy, but the missing piece is the *refused*
function itself, whose refusal is the profile-mapping defect in
[`image-source-union-domain-discriminator.md`](image-source-union-domain-discriminator.md) — supplying
the callers would change nothing while that stands. `@flighthq/video` is `partial` with the three failing
headers reported.

## Third measured site: `@flighthq/updater`

The same outcome from a refused *binding* rather than a refused function. `packages/updater/src/updater.ts`
refuses its three frozen outcome constants and `checkForAppUpdate`, and retains `install_downloaded_update` and
`destroy_updater`. The retained install body names a constant the module omitted:

```cpp
    if ((outcome->reason != flight::String("ok"))) { co_return operation_failed; }
    download_owners.erase(update);
    co_return install_ok;
```

`install_ok` (source line 11) is declared nowhere in the header -- its own `NOT GENERATED` marker sits above it
-- and `operation_failed` is declared as the *check* union's carrier, which this function's declared result
(`flight::Ref<AppUpdateInstallOutcome>`) cannot hold, so the body fails twice: at a name that never existed
(this request's subject) and at a carrier of the wrong union
([`shared-frozen-constant-across-two-outcome-unions.md`](shared-frozen-constant-across-two-outcome-unions.md)).

Measured on the regenerated slice, emission alone, no override: `flight/updater/updater.hpp` gives 5 errors --
`138: no matching member function for call to 'return_value'`, `141: use of undeclared identifier 'install_ok'`,
`144:` the same `return_value` failure -- and all three of the package's headers report the same 5, because
`contract.hpp` and `_internal_index.hpp` both re-export the module.

## Fourth measured site: `@flighthq/useragent` -- the caller is in another module

The same outcome again, and this one shows the omission reaching *across* a module boundary.
`packages/useragent/src/userAgentParse.ts` refuses `parseUserAgentOsVersion` (source line 86) and emits its
other two parsers; `packages/useragent/src/userAgent.ts` is emitted in full and ends with:

```cpp
  return flight::useragent::parse_user_agent_os_version(ua);
```

```
generated/include/flight/useragent/user_agent.hpp:192:29: error: no member named
  'parse_user_agent_os_version' in namespace 'flight::useragent'
```

The caller's own module carries no marker either, so here the omission costs **two** modules rather than one:
the refused module, and a module the emitter considers complete whose last function cannot compile.
Measured on the emission alone, three of the package's four headers fail -- `user_agent.hpp` and both
aggregators that re-export it, five errors each -- while the refused module's own header compiles cleanly,
because it does not claim the missing function exists. This is the case the "What would close this" note above already describes -- the omission has to
propagate to declarations that call it -- with the caller on the far side of a module boundary rather than
inside the module that refused.

## Fifth measured site: `@flighthq/tilemap`

The a627849 tilemap emission refuses `getTilemapSignals` but retains all three mutators that call it:
`clearTilemap`, `setTilemapTile`, and `setTilemapTiles`. Each emitted body names
`get_tilemap_signals`, for which the header contains only a `NOT GENERATED` comment and no declaration.

```
generated/include/flight/tilemap/tilemap.hpp:445:18: error: use of undeclared identifier 'get_tilemap_signals'
generated/include/flight/tilemap/tilemap.hpp:460:18: error: use of undeclared identifier 'get_tilemap_signals'
generated/include/flight/tilemap/tilemap.hpp:498:18: error: use of undeclared identifier 'get_tilemap_signals'
```

The direct header and both aggregators fail together. The refusal itself is the flattened-owner
identity problem already carried by the package override; this measured site is specifically the
additional compile defect in the three declarations the emitter claims it retained.

## Sixth measured site: `@flighthq/selection`

Current-pin selection emission omits each module's private runtime accessor but retains every public
operation that calls it. Focused `-j2` compilation with clang 22 therefore reports only undeclared
callees, not the underlying direct refusal:

```
flight/selection/lasso_selection.hpp:112: use of undeclared identifier 'get_lasso_selection_runtime'
flight/selection/marquee_selection.hpp:98: use of undeclared identifier 'get_marquee_selection_runtime'
flight/selection/contract.hpp:            the lasso failure through re-export
flight/selection/_internal_index.hpp:     the lasso failure through re-export
```

The header gate reports 2/6 passing even though `selection_state.hpp` is not usable: its retained callers
are templates, so their missing `get_selection_state_runtime` name is deferred until instantiation. The
completion supplies the refused constructors and accessors together; this request remains the record that
a raw partial emission must either omit its dependent callers or declare their callee.

## Seventh measured site: `@flighthq/quadbatch`

At Flight `a627849` / flight-compiler `2b687ea`, `getQuadBatchSignals` is omitted after its readonly
`QuadBatch` row cannot be refined to the writable computed-symbol attachment owner
`QuadBatchWithSignals`. Three otherwise retained declarations call the absent function:

```text
generated/include/flight/quadbatch/quad_batch.hpp:689: use of undeclared identifier 'get_quad_batch_signals'
generated/include/flight/quadbatch/quad_batch.hpp:726: use of undeclared identifier 'get_quad_batch_signals'
generated/include/flight/quadbatch/quad_batch.hpp:840: use of undeclared identifier 'get_quad_batch_signals'
```

The callers are `clearQuadBatch`, `removeQuadBatchInstance`, and `appendQuadBatchInstance`. Directly
including the generated module, while retaining dependency overrides, produces exactly those three
name failures in addition to the separate partial-row carrier failures. The quadbatch override supplies
the omitted signal accessor and keeps the three retained callers, so this site expires when omission
propagation or an emitted placeholder makes the unoverridden partial module compile.

## Eighth measured site: `@flighthq/media` -- and it costs the same module twice over

`packages/media/src/videoChannel.ts` refuses its module-local `getElement` (source line 170, marked
`NOT GENERATED` in `generated/include/flight/media/video_channel.hpp`) and retains `playVideoResource`, whose
emitted body calls it. `get_element` is declared nowhere in the emitted tree.

```text
flight/media/video_channel.hpp: use of undeclared identifier 'get_element'
flight/media/_internal_index.hpp: use of undeclared identifier 'get_element'
flight/media/contract.hpp:       use of undeclared identifier 'get_element'
```

Three of the package's seven headers fail, so `@flighthq/media` is partial with 4/7 compiling and the whole
`videoChannel` fixture -- 29 upstream cases -- is unreachable at this pin. Measured on the emission alone,
with no override and no patch. The omitted function is `getElement`, whose own refusal reason is separate;
this request is only about its retained callers.

## Ninth measured site: `@flighthq/log` -- the omission is in a dependency, and it makes logging throw

`packages/log/src/log.ts` refuses its module-local `_mergeSpanFields` while retaining `logInfo`, `logError`,
and `logDebug`, whose emitted bodies pass their resolved data through it. `merge_span_fields` is defined
nowhere in the emitted tree. Unlike the earlier sites, this header compiles and fails at runtime:

```text
libc++abi: terminating due to uncaught exception of type flight::Error:
  flight::log::merge_span_fields was not generated for this profile
```

Every retained logger therefore throws instead of logging, and packages whose behavior is observed through
the log become unobservable. This site strengthens the propagation requirement: compile-time failures are
loud, while a retained call to an omitted callee that still compiles remains silent until execution.

## Tenth measured site: `@flighthq/bitmapfont` -- the refusal names its own missing carrier

`packages/bitmapfont/src/bitmapFontGlyphSource.ts` refuses `initializeGlyphSourceFromBitmapFont` (source line
16) and emits its caller `createGlyphSourceFromBitmapFont` in full. The retained caller constructs the entity
and then calls the function the module omitted:

```cpp
inline flight::Ref<flight::types::GlyphSource> create_glyph_source_from_bitmap_font(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::BitmapFont>>>> font) {
  flight::types::EntityConstruction<flight::Ref<flight::types::GlyphSource>> out = flight::entity::allocate_entity<flight::Ref<flight::types::GlyphSource>>();
  initialize_glyph_source_from_bitmap_font(out, font);
  return flight::entity::finish_entity<flight::Ref<flight::types::GlyphSource>>(...);
}
```

Measured, emission alone, no override:

```text
flight/bitmapfont/bitmap_font_glyph_source.hpp:103:3: use of undeclared identifier 'initialize_glyph_source_from_bitmap_font'
flight/bitmapfont/_internal_index.hpp:                the same, through the include
flight/bitmapfont/contract.hpp:                       the same, through the include
```

`initialize_glyph_source_from_bitmap_font` is defined nowhere in the emitted tree -- its `NOT GENERATED`
marker sits at `bitmap_font_glyph_source.hpp:57`, above the empty space where the definition would be. Both
callers' witnesses hold: the marker is present, emitted code calls the symbol, and nothing defines it.

**Why this site is worth naming separately: the refusal states the fix.** Its class is
`cpp-contextual-union-interface-heritage-carrier-unrepresented [target-runtime]`, and the diagnostic reads:

> contextual nullable interface conversion from VoxelGrid to TextureSource follows TypeScript interface
> heritage, but flight-cpp stores those declarations in independent `flight::Ref<flight::types::VoxelGrid>`
> and `flight::Ref<flight::types::TextureSource>` owner types: each interface struct is emitted flattened, so
> the heritage is not a C++ base relation and no pointer cast exists to follow. A native pointer cast would
> claim C++ inheritance the emitted interface structs do not have, materializing the target would replace
> object identity, and a structural row would change the declared result carrier. ... **add an
> owner-preserving interface-heritage carrier to the runtime contract.**

So this is the family's clearest case: the refused function is not refused for an arbitrary reason but
because a *carrier* is absent, and the missing carrier is runtime-wide rather than TEXT-local. The same
decision -- heritage as a real C++ base relation, or a checked recovery of the concrete owner -- is the open
identity-preserving owner-recovery question that also blocks physics2d's joint factories,
`connectSignalTracked`, the loader refinement and `bitmapfont-formats`. No package-scoped override or patch
can close it; it can only make the header compile.

**Planned closeout, deliberately split in two.** The TEXT-lane closeout is the fail-loud
`declare-refused-function` repair (`repairs/emission-repairs.json`, two precedents in `@flighthq/particleemitter`):
it declares the symbol with the emitter's own carriers and no body, so the three headers compile and any
call fails at link, keeping the function visibly unusable rather than substituted. The runtime carrier is a
separate future architecture decision and is explicitly **not** part of that closeout.

The upstream case for this function is thin and worth stating so nobody mistakes a green suite for coverage:
`bitmapFontGlyphSource.test.ts` asserts only `expect(typeof initializeGlyphSourceFromBitmapFont).toBe('function')`.
A declaration-only repair satisfies exactly that, which is why the fail-loud closeout is verified by a
header compile plus a *negative* link check rather than by a behavioural assertion.

## G2 SWF authored bounds: declaration visibility and data ownership are separate gaps

At compiler `806d257` / Flight `a627849`, `swfNode.ts` refuses `applySwfAuthoredBounds` at source
line 29 as `cpp-type-assertion-unidentified`, but retains `createSwfDisplayObject` calling it.
G1 cluster `undeclared-identifier-294e632e` contains eight headers whose first error is that
undeclared call in `flight/swf/swf_node.hpp`.

The three declaration-repair witnesses agree:

- The header contains the exact `NOT GENERATED: function applySwfAuthoredBounds` marker.
- The retained caller passes its `Node2D` alias and an explicitly constructed optional readonly
  `SwfTagRectangle` view. Imported shape and text callers use the same carriers.
- No runtime, generated header or override defines the function.

The existing `declare-refused-function` mechanism now supplies only this signature, immediately
before the retained caller:

```cpp
void apply_swf_authored_bounds(
    flight::types::Node2D node,
    std::optional<flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::SwfTagRectangle>>>>> bounds);
```

`swf-declare-refused-authored-bounds` in `repairs/emission-repairs.json` expires when that exact
refusal marker disappears. The generated manifest records its single affected header. The repair
adds no body and does not change bounds, install a hook, allocate replacement node data, or perform
unchecked owner recovery.

The underlying function is still unavailable. Its TypeScript body crosses `node.data as unknown as
SwfAuthoredBoundsData | null`, copies the supplied rectangle, updates the existing node data (or
installs data when absent), and installs `computeSwfLocalBoundsRectangle` on the same node runtime.
That reader is separately refused because the declared `Node2DData` owner does not carry
`authoredBounds`; see [the existing owner-recovery request](assertion-to-undeclared-row-member.md).
A common-field-only implementation, a copied data owner or a bounds cache beside the node would
substitute behavior rather than repair declaration visibility.

Verification uses the actual repaired `swf_node.hpp`: an unused include must compile and link,
while a call to `apply_swf_authored_bounds(Node2D{}, std::nullopt)` must compile to an object and
then fail to link on that exact symbol. Both checks passed with Clang 22.1.8: the unused program
also ran successfully, and the called program stopped at the expected undefined symbol during linking.
Header compilation is not evidence that authored bounds can execute. Only the eight assigned headers
are rechecked; any later diagnostic stays a failure.
