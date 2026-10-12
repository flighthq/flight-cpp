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
