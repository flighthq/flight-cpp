# A nullish fallback on a required read lowers to `value_or` on a non-optional

`@flighthq/host`'s `initializeHost` fills every capability group from a partial object:

```ts
out.audio = (capabilities.audio ?? {}) as Host['audio'];
```

The read is cast, so the `?? {}` is a belt-and-braces fallback on a value the type says is present. The emitter
lowers it as if the read were optional:

```cpp
flight::row_set<flight::RowKey<"audio">>(out, static_cast<flight::Ref<flight::types::HostAudioCapabilities>>(
    flight::row_get<flight::RowKey<"audio">>(capabilities).value_or(
        flight::make_ref<anonymous_bbb5ab6ffe89be03<Capabilities>>(anonymous_bbb5ab6ffe89be03<Capabilities>{}))));
```

There are **two** independent defects in that one expression, and the first masks the second.

**(1) `value_or` on a non-optional.** `row_get` on that schema yields a `Ref<HostAudioCapabilities>` -- a
`std::shared_ptr` -- which has no `value_or`. Measured: 19 identical errors before the compiler stops, one per group,
all of the shape

```
error: no member named 'value_or' in 'std::shared_ptr<flight::types::HostAudioCapabilities>'
```

**(2) The fallback arm's type.** The `?? {}` fallback mints an anonymous struct -- `anonymous_bbb5ab6ffe89be03<Capabilities>`,
declared by the emitter for this module as an EMPTY struct (`: public flight::ReferenceEnabled {}`) with no members at
all. The surrounding `as Host['audio']` becomes a `static_cast` to `Ref<HostAudioCapabilities>`, and that conversion
does not exist. Measured on its own, with (1) taken out of the way:

```
error: no matching conversion for static_cast from 'Ref<flight::host::anonymous_bbb5ab6ffe89be03<flight::types::Host>>'
       (aka 'std::shared_ptr<anonymous_bbb5ab6ffe89be03<flight::types::Host>>')
       to 'flight::Ref<flight::types::HostAudioCapabilities>' (aka 'std::shared_ptr<flight::types::HostAudioCapabilities>')
```

So fixing only the `value_or` is not enough to make the module compile; the fallback has to name the group's own
declared type.

So `initializeHost` and `createHost` do not compile for ANY instantiation, and `@flighthq/host`'s six `host.test.ts`
cases have no callable surface to port to. Measured by instantiating `createHost<flight::types::Host>()` and
`createHost()`: the first fails on the errors above, the second cannot deduce its `Capabilities` template parameter
because every parameter is defaulted.

## Minimal TypeScript

```ts
interface Host { audio: Group }
export function initialize(out: Host, capabilities: Partial<Host>): void {
  out.audio = (capabilities.audio ?? {}) as Host['audio'];
}
```

## Expected

Either lowering works, and the choice turns on whether the read can be absent:

**(a) A present group.** Install the caller's own group -- the same `Ref`, not a copy:

```cpp
flight::row_set<flight::RowKey<"audio">>(out, flight::row_get<flight::RowKey<"audio">>(capabilities));
```

**(b) An omitted group.** The source says `capabilities.audio ?? {}` on a `Partial<Host>`, so the read CAN be absent
and the `??` is not dead code: the omitted case must install a **fresh empty group of the group's declared
capability type** -- a `Ref<HostAudioCapabilities>` whose slots are all unset -- and must not install an absent group,
because the `as` cast asserts one exists.

That means, if the read is made a genuine partial read returning `std::optional<Ref<HostAudioCapabilities>>`, the
`value_or` is the right operation and only its argument is wrong:

```cpp
flight::row_set<flight::RowKey<"audio">>(out, flight::row_get<flight::RowKey<"audio">>(capabilities)
    .value_or(flight::make_ref<flight::types::HostAudioCapabilities>()));
```

What is not correct is a `value_or` on a value that cannot be nullish, or a fallback whose type is not the group's
declared capability type.

## Affected modules

`@flighthq/host` `host.ts`: `initializeHost` for every group, and `createHost` through it. The shape is not
host-specific -- any emitted `maybeValue ?? fallback` where the read turns out to be required has the same defect.

## Carrying workaround

None. This repository carries no patch for it, does not edit the emitted module, and does not weaken the signature.
`tests/generated_host_test.cpp` builds its hosts by hand -- group by group -- because the factory cannot be called,
and says so at the top.

## Runtime boundary

The separate merged-row runtime gap that prevented `EntityConstruction<Host & Capabilities>` from resolving and
storing its members is now fixed in `structural_ref.hpp`; see
[merged-row-not-constructible.md](merged-row-not-constructible.md). With that runtime change and the carried
fallback lowering, scratch-generated `createHost` instantiates and runs. The committed generated Host module remains
unchanged until its owning package is regenerated.

## G2 glTF skin component: distinguish an index miss from a present double

At Flight `a627849` / compiler `806d257`, the scoped `gltfSkins.ts` source patch removes the
missing typed-array `from` binding and emits the real builder. Both assigned headers then fail at
`flight/scene3d_formats/gltf_skins.hpp:118`:

```cpp
matrices->data[joint * 16.0 + component].value_or(0.0)
```

`GltfAccessorData.data` is `flight::SequenceView<double>`. Its `operator[]` returns a present
`double`, so Clang reports `member reference base type 'double' is not a structure or union`.
This has the same optional-operation-on-required-storage mismatch as the host site, but the
source index CAN miss: the required source expression is
`matrices.data[joint * 16 + component] ?? 0`. Removing the fallback, returning NaN, assuming a
complete matrix buffer, or copying the source sequence would change its behavior.

The runtime already declares the operation this expression needs:

```cpp
matrices->data.get(joint * 16.0 + component).value_or(0.0)
```

`SequenceView::get(double)` returns an optional after checking the index against the same retained
sequence owner; `operator[]` is a present-element projection. The emitter must choose the lookup's
index-miss carrier before applying nullish selection. It must preserve one source element read,
ascending component order, present NaN and signed zero, the missing-element zero, and the final
Float32 conversion. Adding a `value_or` member to double or bypassing source bounds is not a
runtime fix. No new compiler mechanism or unchecked indexing substitute is supplied here.

Normal exact-pin generation emits both `gltfSkins` and `registerGltfSkinHandlers` without a refusal.
Their native gate remains **2 attempted / 0 pass / 2 fail / 0 unattempted**, jobs **1**, at the
same expression. The real-handler behavior executable also failed to build, so no native run,
unused-header positive link or skin-import functionality is claimed. The independently measured
TypeScript equivalence checks qualify the source rewrite, not the incorrect C++ index lowering.
