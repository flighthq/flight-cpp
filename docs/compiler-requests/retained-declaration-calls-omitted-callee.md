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

## G2 ASCII STL census retains the refused counter

At Flight `a627849` / compiler `806d257`, `scene3d_formats/stl_features.hpp` retains
`collect_stl_features` calling the refused `countAsciiStlFacets`. The caller decodes the complete
byte window with the existing UTF-8 decoder and passes its `flight::String` result, then stores
the counter result as `std::optional<double>`. The exact refusal marker, live same-header call
and absent definition support the existing `scene3d-formats-declare-refused-ascii-stl-count`
declaration repair:

```cpp
std::optional<double> count_ascii_stl_facets(flight::String text);
```

The same-header witness is sufficient; no `calledFrom` list or new repair mechanism is needed.
The repair must expire when the marker or retained call disappears. Both the counter and its
`readAsciiStlBodyTokens` helper remain refused at indexed string member presence. The two plain
indexed reads are recorded in
[indexed member calls](indexed-array-member-call-present-element.md), and the six optional
indexed reads in [optional index/member flow](optional-index-member-present-flow.md).

The source collector first checks exact binary payload bounds, then checks a short ASCII prefix
before decoding the whole file and applying the facet grammar. Declaration availability does
not supply that grammar, make the ASCII branch usable, or justify marking the partial module
complete. The required boundary is an unused real-header link that succeeds and a call through
the retained collector that compiles to an object but fails to link on this exact missing counter.

That boundary was checked against the actual repaired header with C++20: the unused-header program
compiled, linked and ran; the program invoking `collect_stl_features` compiled to an object, then
failed to link with exactly `flight::scene3d_formats::count_ascii_stl_facets(flight::String)` undefined.
Normal exact-pin single-module generation reproduced only the declaration addition and its repair
manifest record. No parser body or source rewrite was carried.

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

### G2 video URL-loader declaration boundary

At Flight `a627849` / compiler `806d257`, the exact `loadVideoResourceFromUrl` refusal marker,
both live same-header calls and absent definition support `video-declare-refused-url-loader`
through the existing declaration-only mechanism. The retained callers pass the readonly
`HostVideoCapability` row, `flight::String`, optional readonly `VideoResourceLoadOptions` row and
optional `AbortSignal`, and await or return `Task<Ref<VideoResource>>`. The declaration uses those
exact carriers and preserves the source optional-argument defaults:

```cpp
flight::Task<flight::Ref<flight::types::VideoResource>> load_video_resource_from_url(
  flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::HostVideoCapability>>>> host_video,
  flight::String url,
  std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::VideoResourceLoadOptions>>>>> options = std::nullopt,
  std::optional<flight::AbortSignal> signal = std::nullopt);
```

No cross-header witness or new repair mechanism is needed. The URL-loader body remains refused
as `cpp-union-runtime-domains-erased`; the existing
[image-source discriminator request](image-source-union-domain-discriminator.md) still covers it.
The actual generated `HostVideoCapability.load_url` already returns `Task<HostImageSource>`, and
the SDL handle already retains its provider kind. A declaration does not supply the missing
compiler-visible domain mapping, host video provider or URL decoder.

The source rejects an absent load callback with `No video element backend available`, passes
the original URL/options/signal to a present callback, and constructs a resource holding the
same returned element with `ownsElement` true. The blob caller creates its object URL once,
revokes it on rejection and records it on success; the list caller selects a source and uses
the empty-resource branch only when selection finds none. Those bodies, callback ordering,
owner identity, cancellation and task/error settlement are not substituted by this repair.
The boundary to verify is an unused actual-header link and an object from each retained caller
that fails to link on exactly the omitted URL loader.

Measured against the real repaired header in C++20: the unused-header program compiled, linked
and ran, including static signature/default-argument checks. Separate programs invoking the
retained blob and URL-list loaders each compiled to an object; each then failed to link with
exactly `flight::video::load_video_resource_from_url` undefined. No other missing symbol was
reported. Normal exact-pin single-module generation reproduced only the owned header's
declaration addition and the matching repair manifest record; the refusal block stayed intact.

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

### Measured after the untrack declaration repair (2026-10-17)

`midi-declare-refused-untrack-subscription` declares the refused `untrack_midi_subscription` from the
retained calls, so `midi_subscription.hpp:164` resolves and the caller compiles; the call itself stays
an unresolved link. Recompiled at `-j1`, all four headers now fail one callee further:
`use of undeclared identifier 'detach_midi_subscription'` at `midi_subscription.hpp:198`.
`detachMidiSubscription` is refused at an unrepresented `Task<MidiSubscriptionDetachOutcome>` domain
(see [`task-of-union-domain-unrepresented.md`](task-of-union-domain-unrepresented.md)), and its three
retained delegators pass `MidiAccessStateSubscription`, `MidiInputMessageSubscription` and
`MidiPortStateSubscription` where the declared parameter is `Entity`. Those source `extends Entity`
relationships are flattened in C++ (each emitted struct inherits only `ReferenceEnabled`), so a second
declaration repair would not compile the delegators. This is therefore the next callee in the same
class, not a separate defect.

## G2 site: `scene3d-wgpu` scene runtime

At compiler `806d257` / Flight `a627849`, the seven-header G1 group
`undeclared-identifier-7dc86326` first fails at `wgpu_scene_3d_runtime.hpp:194`.
`getWgpuScene3DRuntime` has the exact `NOT GENERATED` marker and
`cpp-structural-assertion-owner-unproven` refusal, but the same header retains
`get_wgpu_skinning_adapter(Ref<WgpuRenderState> state)` and its direct call
`get_wgpu_scene_3d_runtime(state)->skinning_adapter`. No definition or declaration of that callee
exists in the original header.

`scene3d-wgpu-declare-refused-scene-runtime` uses the existing guarded
`declare-refused-function` mechanism to insert only:

```cpp
flight::Ref<flight::types::WgpuScene3DRuntime> get_wgpu_scene_3d_runtime(
    flight::Ref<flight::types::WgpuRenderState> state);
```

The argument is the retained caller's actual carrier. The source return type and the emitted
`scene_runtimes` WeakMap value both name the same `Ref<WgpuScene3DRuntime>` result; no anonymous owner
or substitute result was invented. The exact refusal marker, same-header direct call and absence of a
definition are the three witnesses. Removing the marker when emission succeeds expires this repair.

The real repaired header compiles and links when unused. A separate object calling the real retained
`get_wgpu_skinning_adapter` compiles, then fails to link exactly on `get_wgpu_scene_3d_runtime`.
The header remains explicitly PARTIAL and its original refusal and source excerpt remain intact.
The [runtime-owner request](asserted-erased-runtime-owner.md#g2-site-flighthqscene3d-wgpu-scene-runtime)
records the body refusal and identity requirements; this declaration supplies no cache allocation,
skinning behavior, owner recovery or GPU backend implementation.

The scoped seven-header recheck is complete: **2 pass, 5 fail, 0 unattempted**, with one C++ worker.
The repaired runtime header and `enable_wgpu_scene_3d_forward_light_selection_guards.hpp` compile.
The custom-shader guards header and its two barrels now fail at line 116 on
`run_wgpu_custom_shader_guards`, whose body is also explicitly refused
(`cpp-contextual-union-inequivalent` at the `material.uniforms ?? {}` record fallback). The retained
enable function stores that missing function as a callback value; it does not call it directly.
Consequently this second site does not satisfy the existing direct-call repair witness, and no
declaration, replacement guard or repair-engine broadening is carried for it. This is the same
retained-reference consequence with a function-value use instead of a direct call.

The other two failures are the existing
[render-pass descriptor construction](external-descriptor-literal-construction.md) family in the
internal test helper, and a
[member read on the cube-image variant](reference-backed-outcome-variant-narrowing.md#g2-cube-image-value-variant).
All five stay failed and request-linked. The two compile passes do not make the refused scene
accessor callable or promote this partial module to a complete implementation.

## G2 Interaction graph-local-bounds calls cross the refused module

At compiler `806d257` / Flight `a627849`, five headers in `missing-member-5e9e8a2c` fail because
`display_hit_tests.hpp:28/32/36` and `sprite_hit_tests.hpp:16` retain qualified direct calls to
`flight::interaction::hit_test_graph_local_bounds(source, x, y)`, passing the actual `NodeAny`
carrier. The generic callee is explicitly refused in `hit_tests.hpp:88`, at its
[`Node<Traits>` to Node2D capability assertion](assertion-to-undeclared-row-member.md#g2-interaction-graph-bounds-need-the-original-nodes-2d-capability).

The exact refusal marker and retained calls are in different headers. `hit_tests.hpp` has **zero**
live calls to this symbol; the three display calls and one sprite call have no refusal marker in
their own headers. An in-memory probe through the existing `declare-refused-function` mechanism
matches **zero files** and leaves all three headers byte-identical. Thus its same-header witness
does not hold, even though this is a real omitted-callee family; no repair is registered, no body
is supplied and no negative-link check is applicable or claimed.

All five rows remain **0 pass / 5 fail**, retained from G1 and now request-linked with compiler
route. No C++ changed or unchanged native gate was rerun. Name recovery must account for the
cross-module refusal; the implementation still needs the same real node, world transform and local
bounds, rather than a false hit result or a copied/narrowed graph.

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

The completed eight-header G2 recheck is **1 pass, 7 fail, 0 unattempted** at `-j1`.
`swf_node.hpp` passes; five rows advance to the existing
[class-instance/interface request](class-instance-as-implemented-interface.md), and two to the
native `copy_shape_commands` readonly source-parameter mismatch. Those seven rows remain failures
in the [header worklist](../header-failures.md); none is promoted for merely losing its first diagnostic.

## G2 SWF legacy placement follow-on after adjustment heritage

At compiler `806d257` / Flight `a627849`, the bounded ColorScaleBiasAdjustment heritage repair
removes the Adjustment-array constructor failure from the four owned SWF headers. The completed
changed-input check is **1/4 pass, 3 fail, 0 unattempted**, one worker. One of the three failures is
`swf_place_object_handler.hpp:106`: `use of undeclared identifier 'read_legacy_place_object'`.
The other two reach the already filed effect-owner widening gap; they are not counted in this cause.

The same header has the exact `NOT GENERATED: function readLegacyPlaceObject -- source line 60`
marker and retains a call from the `swf_place_object_handler` parse callback. No definition exists in
the generated, runtime or override trees. The call passes the original
`Ref<SwfTagReader>` and the timeline's `Map<double, Ref<SwfTagPlacement>>`. The source helper reads
the character, depth, matrix and optional color transform, then updates that same placement map.
Its body is refused as `dual-sentinel optional property result requires one represented member value
domain` at the optional color-adjustment projection. A replacement must preserve the reader position,
validity, optional/null result and original array owners.

These witnesses classify the next diagnostic under this existing request. No helper declaration,
body or source patch was attempted in the bounded adjustment-heritage arc. A declaration-only candidate
would still require actual-header positive and missing-symbol link validation, including the retained
callback in the globally initialized handler; a syntax-only gate cannot certify that boundary.
`swf_place_object_handler.hpp` remains failed with its actual diagnostic, location and request link.
No SWF placement implementation is supplied, and no unchanged or global rerun is claimed.

## G2 Scene3D material texture listers are retained callback values

At compiler `806d257` / Flight `a627849`, four owned headers first fail at
`scene3d_resources/scene_material_texture_registry.hpp:162`, where the retained standard PBR
registration passes the undeclared `list_standard_pbr_material_textures` callback. The same module
retains the unlit registration passing `list_unlit_material_textures`. Both have exact NOT GENERATED
function markers and neither has a definition in the generated, runtime or override trees.

The emitted Scene3DMaterialTextureLister callback fixes both signatures without guessing:

```cpp
void list_standard_pbr_material_textures(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Material>>>> material,
    flight::Array<flight::types::Texture> out);
void list_unlit_material_textures(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Material>>>> material,
    flight::Array<flight::types::Texture> out);
```

The existing declare-refused-function mechanism now optionally accepts `retainedReference`, the exact
single code line that passes the refused symbol as a function argument. It is validated as a callback
argument and rejects comments, multiple lines and arbitrary symbol mentions. Matching still requires
the exact source-function refusal marker, absence of an existing definition/declaration, and that exact
registration line outside comments. It inserts only the declaration before the retained caller, and
reapplication changes nothing. Existing direct-call repairs retain their previous behavior.

The two entries expire when their source functions emit or their exact registrations disappear. No
body, empty texture list, material downcast, copied texture owner or closed PBR union is supplied.
Their underlying Material-to-concrete assertions and the separate extended-material registration
remain refused; see [the existing owner-recovery request](assertion-to-undeclared-row-member.md#g2-scene3d-material-texture-listers-retain-only-the-base-material-row).

Normal focused generation at the exact locks lowered the required 1519-module dependency closure and
wrote this partial header. Its difference from the committed inventory is exactly the two function
declarations; the manifest records each repair on that one header. The three original owner-recovery
refusal markers remain, and no package or refusal counter is promoted. Exact matching and reapplication
as a fixed point were checked. The emission-repair suite passes **26 tests, zero failures, one skipped**
generic C++ fixture because no default compiler is on PATH; that skip is separate from the actual-header
native checks below.

The actual repaired header compiles, links and runs in an unused-include program. A second program
constructs the real registry and retains both original registration functions; compile-time checks
confirm each function pointer fits Scene3DMaterialTextureLister, and object compilation succeeds.
Linking that object fails on exactly `list_standard_pbr_material_textures` and
`list_unlit_material_textures`, with no other undefined symbols. The declaration-only boundary is
therefore preserved: registration and texture discovery through those callbacks remain unavailable.

The completed changed-input four-header check is **1 pass, 3 fail, 0 unattempted**, one worker.
`scene_material_texture_registry.hpp` passes. `_internal_index.hpp`, `contract.hpp` and
`explain_scene_3d_resource_coverage.hpp` now fail at the last header's line 57, where a designated
initializer targets the SceneCoverageEntry variant. They join the existing outcome-construction
cause and [request](object-literal-into-outcome-variant.md#g2-scene3d-resource-coverage-returns-a-literal-into-the-outer-outcome-variant).
Those changed diagnostics remain failures. Only the four owned rows and affected causes changed;
the existing two unowned coverage rows and historical G1 totals are retained without recompiling them.
No global sweep or upstream test port was performed, and no refused lister body became available.

## G2 glTF transmission volume: a declaration also fails the unused-header link

At compiler `806d257` / Flight `a627849`, the one-header group
`undeclared-identifier-f6affb68` fails at `scene3d_formats/gltf_transmission_volume.hpp:166`.
The exact `NOT GENERATED: function resolveTransmissionVolume -- source line 72` marker remains;
three retained direct calls occur in the transmission, volume and IOR handlers' apply lambdas;
no declaration or definition supplies the resolver.

The existing guarded declaration engine matched this actual header once in a temporary overlay
and was idempotent. The retained readonly context parameter, numeric index, source return type
and concrete `target.value()->...` writes fix the exact signature:

```cpp
std::optional<flight::Ref<flight::types::TransmissionVolumePbrExtension>> resolve_transmission_volume(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GltfExtensionContext>>>> context,
    double index);
```

A real C++20 translation unit including that repaired header and returning zero from main
compiled to an object. **Its unused-header positive link failed**, with exactly one undefined
symbol: `flight::scene3d_formats::resolve_transmission_volume` at that readonly context/double
signature. The linker references all three original globally initialized handler callbacks.
Main never invokes them. This is a failed prerequisite, not a successful called-function
negative-link check or a usable declaration repair.

The candidate is rejected; no registry entry, generated header/manifest change or replacement
initializer is carried. Omission handling must account for the retained global callbacks, or
faithful owner-preserving body emission must supply the resolver. The existing
[narrower-row-owner request](assertion-to-undeclared-row-member.md#g2-gltf-transmission-volume-and-ior-need-the-same-retained-descriptor)
records its actual PbrExtension-to-concrete descriptor and material attachment/finder boundaries.
There is no fake volume, null substitute, copied owner or closed built-in material family.

The recorded production result remains **0/1 passing**, at the original undeclared-call
diagnostic. Only this owned row/cluster and existing requests change. No production C++ input
changed, so no normal generation or unchanged native/global rerun was performed. The measured
object/link check used only the scratch declaration candidate and the installed SDK/override
includes. No transmission, volume, IOR or material-import functionality is claimed.

## G2 glTF animations: a declaration fails the unused-header link prerequisite

At compiler `806d257` / Flight `a627849`, the four-header G1 group
`undeclared-identifier-862d162a` first fails at `scene3d_formats/gltf_animations.hpp:209`:
`use of undeclared identifier 'build_gltf_animations'`. The header has the exact
`NOT GENERATED: function buildGltfAnimations -- source line 34` marker, retains a direct call from
`gltf_animations_core_feature_handler.apply`, and has no declaration or definition of the builder.
No definition exists in the runtime or override trees either.

The builder is refused as `cpp-member-projection-without-present-storage`: a `length` read on optional
C++ storage lacks presence evidence. The refusal does not identify the receiver expression, so this
qualification does not attribute it to a particular source `length` read. The separate
`appendGltfWeightsChannels` refusal at source line 118 is the already reduced
[`ArrayLike.length` contextual-union evidence gap](compiler-contextual-imported-union-owner.md).
That request covers the weights helper, not the builder's different winning refusal.

The actual retained caller fixes the declaration candidate's carriers:

```cpp
flight::Array<flight::Ref<flight::types::Scene3DDocumentAnimation>> build_gltf_animations(
    flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::GltfCoreFeatureContext>>>> context);
```

The existing guarded repair engine matched exactly this header in a temporary overlay and inserted
only that declaration before the retained handler. Reapplication was a fixed point. With this actual
header, `#include <flight/scene3d_formats/gltf_animations.hpp>` followed by `int main() { return 0; }`
compiled to an object with Clang 22.1.8. **The required unused-header positive link failed**, with exactly
one undefined symbol: `flight::scene3d_formats::build_gltf_animations` at the declaration's readonly
context carrier. The link reference comes from the original handler's apply lambda, even though main
does not use the handler: the inline handler is globally initialized and retains the callback.
This is a failed prerequisite, not a successful called-function negative-link check.

The declaration candidate is therefore rejected. No repair entry, generated C++, manifest change,
replacement handler or builder body is carried. A placeholder signature alone cannot close this
globally initialized site. Omission propagation must account for the retained initializer, or faithful
body emission must make its callback available; a signature cannot make an unused include link here.

The source and emitted owners also rule out an empty-list substitute. GltfCoreFeatureContext owns the
original document and node/primitive index maps. Accessor results carry `SequenceView<double>` data;
both the pinned `initializeAnimationTrack` and its native override retain those times and values.
Each document animation holds `Array<Ref<Scene3DDocumentAnimationChannel>>`, and each channel retains
its `Ref<AnimationTrack>`. A faithful implementation must preserve source animation/channel order,
duration and name defaults, accessor/cardinality/path diagnostics, quaternion width, and weights
fan-out to each original morphable mesh node at that mesh's actual target count. Replacing channels,
tracks or buffers with copied owners would change these relationships.

Qualification remains **0/4 pass, 4 fail**, retained from G1 at the original undeclared-call diagnostic.
Only the four owned product rows and this cause are request-linked. No production C++ input changed,
so the unchanged four-header gate and global sweep were not rerun. There is no claim that animation
import can execute or that a new native header pass was obtained.

## G2 GL bitmap text retains a refused callback in an aggregate

`scene2d_gl/gl_bitmap_text.hpp` at compiler `806d257` / Flight `a627849` omits
`submitGlBitmapText` but retains `.submit = submit_gl_bitmap_text` inside the inline
`gl_bitmap_text_renderer` aggregate. Both original headers fail on that name at line163
(Clang22.1.8 jobs1:2 attempted,0 pass,2 fail,0 unattempted). The exact source signature is
`void submit_gl_bitmap_text(Ref<GlRenderState>, Ref<RenderProxy2D>);`.

This is not a direct call or a callback in a function argument: the existing guarded
`retainedReference` witness accepts the latter, and `calledFrom` requires named direct callers.
Extending that witness would not clear this header. One scratch copy containing only the exact
declaration still fails to compile the unused include at `.create_data`, whose NodeAny-taking
`noop_renderer_data` cannot construct the Sprite-taking callback. The later generic RenderState
versus GlRenderState submit-domain mismatch is visible by type inspection, not a native follow-on
result. Neither a positive unused-header link nor an exact missing-body negative link was reached,
and no declaration repair was carried.

The [heritage callback conversion](interface-heritage-widening.md#g2-gl-bitmap-text-renderer-create-data-callback)
and [typed runtime/backend owners](readonly-callback-derived-owner-recovery.md#g2-gl-bitmap-text-needs-its-own-runtime-owner)
need faithful carriers first. The body remains refused; declaration availability must not be reported
as glyph submission, texture/material binding or actual GL rendering behavior.

## G2 raster shape declaration witness is valid but callback carriers fail

At compiler `806d257` / Flight `a627849`, original cluster `undeclared-identifier-20fa691d`
contains `scene2d_gl/gl_raster_shape_renderer.hpp` and `scene2d_gl/gl_shape.hpp`. Both still fail on
`draw_gl_raster_shape` at the former's line173 (Clang22.1.8 jobs1:2 attempted,0 pass,2 fail,0 unattempted).
The source signature is `void draw_gl_raster_shape(Ref<GlRenderState>, Ref<RenderProxy2D>);`.

The exact `drawGlRasterShape` refusal marker is in the raster module; `gl_shape.hpp` includes it
and retains a qualified direct call. An in-memory use of existing `declare-refused-function` with
`calledFrom: ["flight/scene2d_gl/gl_shape.hpp"]` matches only the raster header and adds only that
declaration. This witness is valid even though the local renderer binding stores a function value.

A scratch unused include with only the exact declaration still fails compilation at `.create_data`:
`create_gl_shape_data` returns `optional<ErasedRef>` and accepts `Ref<GlRenderState>, Node2D`, while
`Scene2DRenderer.create_data` expects `optional<Ref<RendererData>>` from `Ref<RenderState>, Node2D`.
Thus the witness is not enough to satisfy the required positive unused-header link; no negative-link
boundary is reached and no declaration repair or repair-engine change is carried. The later destroy
and submit callback differences are type inspection, not additional native diagnostics.

The [body's null-presence refusal](nullable-optional-presence.md#g2-raster-shape-null-presence-refusal)
and [backend/data owner carriers](readonly-callback-derived-owner-recovery.md#g2-raster-shape-native-and-generic-renderer-callbacks)
remain open. Native raster lifecycle debt is explicit in the existing partial shape-data override;
a declaration must not be presented as full rasterization or actual texture/material/quad submission.

## G2 GL particle renderer after the attribute-location number carrier fix

At compiler `806d257` / Flight `a627849`, the changed two-header particle check clears the original
nonconstant int-to-double aggregate-initialization error. `WebGl2Context::get_attrib_location` now
returns the source number carrier, preserving every GLint result including -1. The 2D header then
fails at `scene2d_gl/gl_particle_emitter_2d.hpp:268` on undeclared
`draw_gl_particle_emitter_2d`.

The exact `NOT GENERATED: function drawGlParticleEmitter2D -- source line 144` marker remains.
Its renderer submit lambda retains the original flush followed by the missing draw call. The body
refusal is `cpp-union-runtime-domains-erased`, naming `GLsizei | primitive -> double`; the
[existing same-carrier union request](distinct-union-domains-one-carrier.md) covers that class.
This diagnostic does not identify the particular receiver expression, so no source expression is
attributed here. No declaration, draw body, empty draw or callback adapter is carried in this
numeric-binding change. Attribute lookup availability does not supply particle drawing.

The completed serial check is **0/2 pass, 2 fail, 0 unattempted**. Only this 2D row joins the
retained-callee cause. The 3D row reaches its existing literal-membership gap at line 113 and stays
failed under [that request](membership-test-lowered-literally.md#g2-particle-3d-reaches-the-previously-latent-membership-site).
Both later diagnostics remain failures; no rendering pass is claimed.

## G2 MD5 material family retains a refused binding

At compiler `806d257` / Flight `a627849`, the two original headers in
`undeclared-identifier-1048386b` are `scene3d_formats/md5_material_handler.hpp` and
`md5_section_registry.hpp`. The completed Clang 22.1.8 check at one worker is **0/2 pass,
2 fail, 0 unattempted**. Both report `use of undeclared identifier 'md5_material_handler'`
at the material header's line 69. This is a refused variable, not a missing function declaration:
`NOT GENERATED: variable (binding) -- source line 18` replaces `md5MaterialHandler`, but the emitted
`md5_material_family` still initializes its one element from that absent value. The registry includes
this header and spreads skeleton handlers followed by the material family into its full-support preset.

The binding refuses as `cpp-reference-assertion-without-heritage` at the concrete Blinn-Phong material
assertion; the [existing heritage request](interface-heritage-widening.md#g2-md5-material-handler-needs-the-original-concrete-material-owner)
records the source owner/field/resource semantics. The exact handler carrier is already emitted for the
sibling `md5_skeleton_handler`, whose source has the same `Readonly<Md5SectionHandler>` declaration:

```cpp
extern flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::Md5SectionHandler>>>> md5_material_handler;
```

A scratch use of the existing `insert-forward-declaration` engine, guarded by the exact refusal marker,
adds only this external declaration in the package namespace and reaches a fixed point. It leaves the
entire original global family initializer intact. The actual-header program containing only its include
and `int main() { return 0; }` compiled to an object, but its required unused-header positive link failed.
The linker reports exactly one undefined symbol, `flight::scene3d_formats::md5_material_handler`, referenced by the retained
family's `__cxx_global_var_init`. Main neither reads the family nor calls the handler. This is a failed
positive prerequisite, not a successful explicit-use negative-link test.

The declaration candidate is rejected. No repair entry, generated C++, manifest, source patch, override
or substitute handler is carried, and no generation change needs reproducing. The emitter must either
supply the faithful original handler or report its dependent family/preset unavailable coherently;
removing the material element from the full-support preset must not be reported as completed support.
Both original rows remain failed, with no header gain or native material behavior claim.

## G2 MD2 skin family retains its refused binding

At compiler `806d257` / Flight `a627849`, original cluster `undeclared-identifier-7ce7bd18`
contains `scene3d_formats/md2_section_registry.hpp` and `md2_skin_handler.hpp`. The completed
Clang 22.1.8 check at one worker is **0/2 pass, 2 fail, 0 unattempted**; both report
`use of undeclared identifier 'md2_skin_handler'` at the skin header's line 123.
`NOT GENERATED: variable (binding) -- source line 21` replaces the handler, refused as
`cpp-reference-assertion-without-heritage` at the actual `BlinnPhongMaterial` to `Material` assertion.
The [existing heritage request](interface-heritage-widening.md#g2-md2-skin-handler-reaches-the-same-material-owner-boundary)
records the independently inspected owner defect and the distinct MD2 record/material/diagnostic contract.
The byte-path reader itself emits; its availability does not supply the omitted collecting callback.

The retained inline `md2_skin_family` globally initializes its one element from the omitted handler.
The full-support section registry includes this header and spreads the skin family before the animation
family; that source ordering matters for mesh material binding. The actual sibling
`md2_animation_handler` has the same declared `Readonly<Md2SectionHandler>` source type and emits the
exact carrier used for this external declaration candidate:

```cpp
extern flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::Md2SectionHandler>>>> md2_skin_handler;
```

The existing `insert-forward-declaration` engine in scratch, guarded by the exact skin-handler refusal
marker, inserts only this declaration in the package namespace and reaches a fixed point. The original
path reader and global family initializer remain byte-for-byte intact. The actual-header program with
only its include and `int main() { return 0; }` compiled to an object, but its required unused-header positive link failed.
The linker reports exactly one undefined symbol, `flight::scene3d_formats::md2_skin_handler`, from
`__cxx_global_var_init` in the retained family initializer. Main neither accesses the family nor
invokes the collecting callback. This is a failed positive prerequisite, not a successful explicit-use
negative-link test; the independently measured MD2 boundary reaches the same disposition as MD5.

The declaration candidate is rejected. No repair entry, generated C++, manifest, source patch, override
or substitute skin/material/body is carried. Faithful handler emission or coherent refusal of its
dependent family/preset is required; silently removing the skin element from the full-support preset
would lose named materials, first binding, alternate skins and diagnostics. Both original rows remain
failed, with no header gain or native skin/material behavior claim.

## G2 Intl number declaration fails the unused-object prerequisite

At Flight `a627849` / compiler `806d257`, original cluster `undeclared-identifier-1a056bbc`
contains `intl/number.hpp` and the `contract.hpp` / `_internal_index.hpp` rows inherited from the earlier
date declaration repair. `number.hpp` retains five calls to `get_number_format`, but its exact
`NOT GENERATED: function getNumberFormat -- source line 67` marker replaces the helper and no definition
exists. Its [typed optional to erased-object refusal](compiler-intl-optional-and-union-carrier-evidence.md#g2-number-formatter-still-loses-the-typed-optional-options-boundary)
is still present; there is no omitted runtime method or formatter body to extend.

The three declaration witnesses match the existing `declare-refused-function` mechanism in scratch,
with `IntlNumberFormat(LocaleInput, optional<IntlNumberFormatOptions>)` taken from the actual plain-number
caller's carriers. However, the real unused-header objects fail for all three headers, before any link,
on the compact wrapper's bare initializer list passed to the optional parameter. The
[exact candidate and serial 0/3 result](compiler-union-and-literal-argument-construction.md#g2-number-declaration-candidate-stops-before-link-qualification)
reject the repair. No unused-header positive link or explicit-use negative-link result is claimed.
The four wrapper [option spreads are also silently dropped](object-spread-of-a-parameter-dropped.md#g2-intl-number-wrappers-drop-all-caller-option-spreads).

No declaration/body/registry/source change is carried, and the three production rows remain failed with
their original diagnostics and prior date provenance. Faithful body emission or a coherent partial boundary
still requires the caller construction and full options semantics; supplying a default formatter or a
non-optional overload would not close this request.

## G2 AWD2 parse state: declaration repaired, initializer still owed

`@flighthq/scene3d-formats` `awd2BlockDispatch.ts` at flight `a627849`: `initializeAwd2ParseState` is
refused because `Object.assign` has no lowering onto the parse-state entity (see
[object assign onto structural record](object-assign-onto-structural-record.md)), while retained
`createAwd2ParseState` calls it at `awd2_block_dispatch.hpp:86`. The three witnesses hold — exact marker,
same-header retained call, no definition — and carried repair
`scene3d-formats-declare-refused-awd2-parse-state` declares the five carriers the call constructs.

Measured at `-j1` with Clang 22.1.8: the unused header compiles, links and runs; a caller compiles and
fails to link on exactly
`flight::scene3d_formats::initialize_awd2_parse_state(...)`; `awd2_block_dispatch.hpp` passes and
`_internal_index.hpp` moves to the `create_texture` carrier family. No parse state, map, owner or field
assignment is supplied.

## G2 GL TextLabel generated Canvas path remains unavailable

The generated `scene2d_gl/gl_text_label.hpp` at Flight `a627849` / compiler `806d257` retains
`destroy_gl_text_label_data` calling `get_gl_text_label_data`, while that getter is refused as
`cpp-reference-assertion-without-heritage`. Its original retained call is at line100. The private
renderer-data allocation and later recovery must preserve the same owner; see
[flattened refinement](flattened-interface-refinement-identity.md#g2-gl-textlabel-private-renderer-data).
The source also needs its full TextLabel and typed runtime, not a base-shaped replacement.

The draw body is independently refused as `cpp-presence-test-without-absence-storage`. Its diagnostic
names no exact tested property; the source has several null checks, so no particular member is blamed
without further evidence. The final emitted global `gl_text_label_renderer` still references the
missing `draw_gl_text_label` as its submit value. Its callbacks declare generic RenderState,
Node2D/RendererData carriers while the local source callbacks take GL state and private renderer data;
the [typed callback/backend owner requirement](readonly-callback-derived-owner-recovery.md) still applies.
A getter declaration alone cannot supply those bodies or settle a globally retained function value.
This conclusion is type/source inspection, not a new declaration or native link probe.

The current effective header instead uses the preexisting partial native glyph factory and checked
ErasedRef binding. It has no call to the refused getter and no browser singleton. Original1 compiles
at jobs1, but CanvasHost/ImageHost overloads and browser renderer remain unavailable. No declaration
repair or shader/drawing substitute is proposed, so no positive-unused/negative-called link result or
new generation is claimed. The scoped result reconciles the stale G1 name error with the landed
native carry; it does not make the omitted generated path callable.

## G2 flex resolver binding is declared for explicit registration

At Flight `a627849` / compiler `806d257`, `flex_layout.hpp` retains
`register_flex_layout_resolver` constructing `std::optional<flight::types::LayoutResolver>` from
`flex_layout_resolver`. The actual source is `const flexLayoutResolver: LayoutResolver = ...`; its exact
line-21 variable-binding refusal marker remains and no definition exists. The binding refuses at the
[nullable style owner assertions](assertion-to-nullable-variant-alternative.md#g2-flex-header-boundary-nullable-style-owners-remain-refused).

The existing `insert-forward-declaration` repair `layout-declare-refused-flex-resolver` is scoped to
this exact header and refusal marker and adds only:

```cpp
namespace flight::layout { extern flight::types::LayoutResolver flex_layout_resolver; }
```

The alias is the actual callback carrier the retained call wraps, with Float32Array output,
readonly LayoutTree row, SequenceView<double> intrinsic sizes, two double indices and optional String
failure result. No callback, layout algorithm, registration side effect or successful result is supplied.
Normal exact-pin one-module generation applies the repair to exactly this header; the existing engine
reproduces the addition and reaches a fixed point.

The real unused-header program compiles, links and runs. A program calling the actual registration
function compiles to an object, then fails to link with exactly one undefined symbol,
`flight::layout::flex_layout_resolver`. The registered callback value is referenced only when explicit
registration is used; there is no missing global initializer in the unused program. After the separate
[basis guard source rewrite](mixed-value-union-literal-comparison.md), the original one-header serial
check is **1/1 pass**. This is header availability with a visible missing-body link boundary; the module
and callable layout resolver remain incomplete.

## G2 Lottie mask family requires the refused function during initialization

At compiler `806d257` / Flight `a627849`, original cluster `missing-member-703ef98e`
contains `scene2d_formats/lottie_mask_handlers.hpp`. Its completed Clang22.1.8 one-worker check is
**0/1 pass, 1 fail, 0 unattempted**: the registration at line18 names the absent
`flight::scene2d_formats::lottie_additive_mask_handler`. The same header's retained inline
`lottie_all_mask_handlers` initializes its single element from that function at line21.
The function itself is explicitly refused in `lottie_mask.hpp`, source line21, as
`cpp-contextual-union-inequivalent`; the [generic argument request](generic-call-argument-uses-unsubstituted-parameter-type.md#g2-lottie-mask-handler-refuses-before-emitting-the-call)
records the uninstantiated target `T` versus concrete shape-path alternatives. No generated header,
override, binding or runtime definition supplies this handler.

The exact signature comes from the actual source function and emitted `LottieMaskHandler` alias,
`std::function<void(Ref<LottieMaskContext>)>`. A scratch use of the existing
`insert-forward-declaration` mechanism, guarded by the exact function/refusal marker, adds only:

```cpp
namespace flight::scene2d_formats {
void lottie_additive_mask_handler(flight::Ref<flight::types::LottieMaskContext> context);
}
```

The repair loader accepts this candidate; exactly one header changes and the engine reaches a fixed
point. The original refused source excerpt, actual registration and entire global family initializer
remain byte-for-byte intact. A program including the actual family header with only
`int main() { return 0; }` compiles to an object, but its required unused-header positive link fails.
The only undefined symbol is `flight::scene2d_formats::lottie_additive_mask_handler` with the exact
shared `LottieMaskContext` parameter, referenced by line21's `__cxx_global_var_init`. Main neither
reads the family nor calls the handler. This is a failed positive prerequisite; it is not a successful
explicit-use negative-link check. No executable runs and no mask behavior passes.

The declaration candidate is rejected. No repair entry, source patch, generated C++, manifest,
override or substitute mask/body is carried, so no changed generation needs reproducing. Preserve the
family's original callback identity, additive kind and registry insertion/replacement behavior when
faithful emission becomes available. Supplying an empty family, dropping the additive member,
returning without masking, or moving initialization behind an accessor would change the source.
Either emit the actual handler or report the dependent family unavailable coherently. The original
row remains failed with no header gain.

## G2 GL velocity program declaration fails the unused-header compile

At Flight `a627849` / compiler `806d257`, `scene2d_gl/gl_velocity.hpp:155` retains a call to
`ensure_gl_velocity_program` after `ensureGlVelocityProgram` refuses its named-versus-anonymous cache
rebind owner. The exact emitted signature is `Ref<GlVelocityProgram>(Ref<GlRenderState>)`:
`GlVelocityProgram` is the declared program/buffer/location owner and the retained helper passes the
readonly context's existing state reference. The existing `declare-refused-function` loader and
exact marker/direct-call/absent-body matcher qualify that declaration once and idempotently in scratch.

The actual unused include with ONLY that declaration fails compilation before any link boundary:
`reference to non-static member function must be called`, candidate line410 (stock408), at
`gl.get_parameter(gl.viewport)`. The [external property-name request](external-property-method-name-collision.md)
records the method/constant distinction. This is a rejected declaration candidate, not an expected
called-function negative-link result. No repair or normal generation is carried. The production
original1 remains **0/1 pass**, Clang22.1.8, one worker, unchanged missing-name diagnostic at155.

The helper's [cache value-owner mismatch](contextual-union-map-value-carrier.md#g2-gl-velocity-program-lookup-and-rebind)
and [GlContext weak-key policy](bound-interface-weak-map-key-policy.md) are independent refusals.
`visitGlVelocity` also refuses a null test on an unnamed identifier, while all three kind-specific writer
bindings refuse erased-to-structural owner recovery. Declaring or supplying only the cache cannot
supply traversal, the registered writers or the complete velocity pass. Preserve generic transform
traits, same node/runtime owners, per-particle and per-instance vectors/atlas transforms, source-only
coarse fallback, RG signed/subpixel encoding and pixelRatio, original clear/covered alpha and GL-state
restoration. No dummy program, zero-velocity writer, copied owner, narrowed domain or drawing claim.
## G2 MTL material library retains the refused parser

At Flight `a627849` / compiler `806d257`, `scene3d_formats/mtl_parse.hpp` carries the NOT GENERATED
`parseObjMaterialLibrary` marker and no declaration, while `obj_material_model.hpp` retains
`collectObjMaterialModels` calling it with a `flight::String` source and `std::nullopt` diagnostics.
The caller is in another header, so `scene3d-formats-declare-refused-parse-obj-material-library`
names it in `calledFrom` and inserts only the emitter's own signature into the refused module's
header:

```cpp
flight::Ref<flight::types::ObjMaterialLibrary> parse_obj_material_library(
    flight::String source,
    std::optional<flight::Array<flight::Ref<flight::types::ImportDiagnostic>>> diagnostics = std::nullopt);
```

The refused module's header still carries the marker and defines nothing. An unused include of
`mtl_parse.hpp` links, and a direct call fails to link on exactly `parse_obj_material_library`. The
retained collector cannot compile yet for its own structural-reference follow-on, not for this
declaration, so it does not reach either boundary. No library, material, diagnostics or grammar
behavior is supplied; the underlying refusal is
[indexed array member calls](indexed-array-member-call-present-element.md).

## G2 grid track guard and resolver retain two distinct omissions

At Flight `a627849` / compiler `806d257`, `grid_layout.hpp` first calls the missing `is_track_list`
from its retained container validator. `NOT GENERATED: function isTrackList -- source line 183`,
two live same-header calls and no definition support `layout-declare-refused-track-list` through the
existing `declare-refused-function` engine:

```cpp
bool is_track_list(flight::Any value);
```

The source unknown parameter uses the same `Any` carrier as the header's emitted numeric guards.
The actual callers pass optional columns/rows arrays; the existing optional/array constructors retain
the array identity and absent-to-undefined channel. No typed-array-only overload or validator result
is substituted. The body remains refused at
[erased element runtime-type evidence](erased-array-element-reading.md#g2-grid-track-list-validator-retains-the-same-erased-element-boundary).

The same header also retains explicit registration of the separately omitted source binding
`const gridLayoutResolver: LayoutResolver = ...`. Its exact line-17 binding marker is
`cpp-type-assertion-unidentified`. `layout-declare-refused-grid-resolver` uses the existing exact-header,
marker-guarded `insert-forward-declaration` engine to add only:

```cpp
namespace flight::layout { extern flight::types::LayoutResolver grid_layout_resolver; }
```

The retained registration wraps this actual alias in `optional<LayoutResolver>`; its callback takes the
Float32Array output, readonly LayoutTree row, SequenceView<double> intrinsic sizes and two double indices,
and returns optional String failure. The [grid owner obligations](assertion-to-nullable-variant-alternative.md#g2-grid-resolver-keeps-its-own-item-and-container-contracts)
and [track fraction projection](reference-backed-outcome-variant-narrowing.md#g2-grid-track-fraction-still-requires-an-active-owner)
remain refused. Grid does not share flex's null-container acceptance or basis-validation problem.

An initial scratch declaration candidate stopped before any link because the numeric item guards called
`is_integer(Any)` while the runtime accepted only double. The runtime now supplies the noncoercive
`is_integer(const Any&)` overload: only a stored number reaches the existing scalar finite/integral test;
all other represented types return false without conversion or invocation. The existing runtime target
passes, covering numeric edge values, nonnumber categories and owner/value effects. No implicit
`Any -> double` conversion, source rewrite or generated guard body was added.

Normal exact-pin one-module generation reproduces only these two declarations and their repair records;
each existing engine matches exactly the one header and is idempotent. The actual unused header compiles,
links and runs. Two independent programs then qualify the retained uses: calling the real container
validator with a represented auto-track style compiles to an object but fails to link on exactly
`flight::layout::is_track_list(flight::Any)`; calling the actual registration function compiles to an
object but fails to link on exactly `flight::layout::grid_layout_resolver`. Neither refused body was run.
The partial marker, all six omissions, refusal ledger, module fingerprints and package counts remain.

The original grid header serial check completes **1 attempted / 1 pass / 0 fail / 0 unattempted**.
The single row is pass with `previousCluster`; no other header is measured or promoted. Header
availability and strict integer support do not make either refused grid operation callable.

## G2 GL Scale9 sprite retains a refused aggregate callback

`scene2d_gl/gl_scale9_sprite.hpp` at Flight `a627849` / compiler `806d257` omits
`drawGlScale9Sprite` under the nullable Material2D-to-Material heritage rule, while the retained
inline renderer stores `.submit = draw_gl_scale9_sprite`. Original1 remains0/1, Clang22.1.8 jobs1,
missing that name at168. The exact source declaration is
`void draw_gl_scale9_sprite(Ref<GlRenderState>, Ref<RenderProxy2D>);`.

As with bitmap text above, this is an aggregate-field reference. The existing repair has no direct
call to match, and its `retainedReference` loader correctly rejects the exact aggregate code line
because the callback is not in a function-argument position. No witness extension is proposed.
One scratch include with only that declaration fails compilation at `.create_data` (scratch170):
`create_sprite_renderer_data` returns `optional<ErasedRef>` from `(Ref<GlRenderState>, Node2D)`,
not the slot's `optional<Ref<RendererData>>` from `(Ref<RenderState>, Node2D)`.
No positive unused-header link/run or called-function negative-link qualification is reached.

The [material owner refusal](compiler-interface-heritage-owner-widening.md#g2-gl-scale9-sprite-material-owner)
and [backend/data/full Scale9 owner requirements](readonly-callback-derived-owner-recovery.md#g2-scale9-sprite-native-sprite-callbacks-are-not-generic-renderer-callbacks)
remain separate obligations. Dirty/submit and exact Sprite-recovery restrictions are inspected later
seams, not additional measured diagnostics. No repair, normal generation, source/runtime/override
change, fake draw or usable Scale9 rendering claim is carried.

## G2 GL Scale9 shape getter declaration stops at the cache carrier

At Flight `a627849` / compiler `806d257`, `gl_scale9_shape.hpp:114` retains the destroy callback's
call to `get_gl_scale9_shape_data` after the private nominal-owner assertion is refused. Existing
`declare-refused-function` loader, exact marker/direct-call/absent-body match, one declaration
insertion and idempotence pass in scratch for
`Ref<GlScale9ShapeData> get_gl_scale9_shape_data(Ref<RendererData>);`.

The actual unused include with only that declaration fails compilation at candidate121 (stock119):
`WeakMap<TextureSourceValue, Entry>` cannot initialize the local
`WeakMap<Ref<TextureSource>, Entry>`. This is the effective native context slot versus its emitted
consumer, as recorded in the [cache-local carrier request](native-texture-source-cache-local-carrier.md).
The candidate is rejected before link, not an expected called-function negative-link pass. No repair,
normal generation, positive unused-header link/run or callable getter is supplied. Production original1
remains0/1, Clang22.1.8 jobs1, at the unchanged missing-name diagnostic.

The [private raster-data owner request](flattened-interface-refinement-identity.md#g2-gl-scale9-shape-private-raster-data)
remains distinct from `drawGlScale9Shape`'s `cpp-presence-test-without-absence-storage` refusal, whose
message names no exact property. The retained mask wrapper calls that missing draw and the global
renderer stores it; generic-versus-GL callbacks and full ShapeRuntime remain unproven later seams.
No extra declaration or positive/negative link result for the draw was tested. Preserve complete
commands/grid, provider replay/dispatch/fallback, dirty size/scale/pixelRatio/revision checks and actual
surface/Image/GPU lifecycle, rather than substituting empty raster data or mesh-only support.

## G2 GL QuadBatch retains its refused submit callback

At Flight `a627849` / compiler `806d257`, `scene2d_gl/gl_quad_batch.hpp` omits `submitGlQuadBatch`
under the nullable Material2D-to-Material heritage rule but stores `.submit = submit_gl_quad_batch`
in its inline renderer aggregate. Original1 remains0/1, Clang22.1.8 jobs1, at missing submit191.
The exact source signature is `void submit_gl_quad_batch(Ref<GlRenderState>, Ref<RenderProxy2D>);`.

The existing declaration repair finds no direct call and correctly rejects the exact aggregate
`retainedReference`, which is not a callback in a function-argument position. No witness extension
is proposed. One scratch actual unused include with only that declaration fails compilation at
`.create_data` (candidate193): `noop_renderer_data(RenderState, NodeAny)` cannot initialize the
Sprite-taking slot. See the [existing callback owner request](interface-heritage-widening.md#g2-gl-quadbatch-create-data-callback).
No positive unused-header link/run or qualified called-function negative-link is reached. The later
submit callback's GlRenderState versus generic RenderState is inspected by type, not another actual
first-error result. No declaration/normal generation/body or callable renderer is carried.

The [material owner refusal](compiler-interface-heritage-owner-widening.md#g2-gl-quadbatch-material-owner)
remains separate. Preserve full QuadBatch/atlas/array/runtime owners, both transform modes and rotation
directions, invalid-instance skipping and compact valid packets, post-prepare material/color indices,
source-index per-quad fallback and final actual drawCount. An empty submit, fake packets, Sprite/base
copy or narrowed atlas/color domain would not implement that source.

## G2 Spine JSON slots require a complete result owner as well as the refused callee

At compiler `806d257` / Flight `a627849`, original `undeclared-identifier-34adf2ab` contains
`skeleton2d_formats/spine_json_slots_handler.hpp`. The Clang22.1.8 one-worker check completes
**0/1 pass, 1 fail, 0 unattempted** at the retained `parse_spine_slots` call on line81.
The exact source-line11 function marker remains; its body refuses
`cpp-presence-test-without-absence-storage` while reading the guarded erased array
([source/domain notes](erased-array-element-reading.md#g2-spine-json-slots-keep-aligned-outputs-and-original-bone-reads)).
No runtime, override or other generated header defines this function.

The caller destructures `attachmentNames` and `slots` through the returned reference. Its complete
anonymous result definition is omitted here along with the body. The actual sibling
`spine_binary_slots_handler.hpp` emits exactly the result both source annotations declare:

```cpp
struct attachment_names_slots_8d720886328896ca : public flight::ReferenceEnabled {
  flight::Array<std::optional<flight::String>> attachment_names;
  flight::Array<flight::Ref<flight::types::Slot2D>> slots;
};
```

Both pinned source functions explicitly return these same two array types in this field order. The
binary helper returns a Ref to that emitted owner in the same namespace. The current retained JSON
call passes Any from its document record and Array<Ref<Bone2D>> from its original bones, matching the
unknown/readonly-array source and the emitted sibling bone-lookup signature. The exact scratch
function declaration is therefore:

```cpp
flight::Ref<attachment_names_slots_8d720886328896ca> parse_spine_slots(
    flight::Any raw, flight::Array<flight::Ref<flight::types::Bone2D>> bones);
```

A result forward declaration alone cannot support the caller's member reads. A scratch candidate uses
the existing `insert-forward-declaration` loader/engine with the exact function/refusal marker,
this signature, and an include of that existing complete result owner's header. It matches exactly
the owned JSON header and reaches a fixed point; removing only the declaration/include recovers the
original header byte-for-byte. No result fields, owner instance, function body or parser are copied.

The real JSON-header program with only its include and an empty main fails its required unused-header
object compile. The existing result-owner dependency fails at `spine_binary_slots_handler.hpp:71`,
whose dark-color diagnostic passes a bare key/value list into optional<Record<...>>
([existing request](optional-record-literal-not-materialized.md)). This is a failed positive
prerequisite before link; no unused positive link/run or exact explicit-use negative link is reached
or claimed. Its emitted JSON reader alias still retains the section handler, and its linkage remains
an obligation rather than an assumed success after merely adding a signature.

The candidate is rejected. Do not invent an Any/empty result, borrow an incomplete pointer shape,
copy a new owner or silently omit slots to make the header compile. The emitter must keep a complete
source result contract visible to retained callers and preserve the missing helper's original semantics,
or coherently refuse the dependent caller. No repair, generated C++, source patch, override, manifest
or inventory change is carried, so no changed generation needs reproducing. The original row remains
failed, with no slot/parser/header gain or native behavior claim.
