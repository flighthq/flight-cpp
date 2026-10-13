# A nullish fallback on a required read lowers to `value_or` on a non-optional

`@flighthq/host`'s `initializeHost` fills every capability group from a partial object:

```ts
out.audio = (capabilities.audio ?? {}) as Host['audio'];
```

The read is cast, so the `?? {}` is a belt-and-braces fallback on a value the type says is present. The emitter
lowers it as if the read were optional:

```cpp
flight::row_set<flight::RowKey<"audio">>(out, static_cast<flight::Ref<flight::types::HostAudioCapabilities>>(
    flight::row_get<flight::RowKey<"audio">>(capabilities).value_or(flight::make_ref<HostAudioCapabilities>())));
```

`row_get` on that schema yields a `Ref<HostAudioCapabilities>` -- a `std::shared_ptr` -- which has no `value_or`. The
result is a compile error, one per group:

```
error: no member named 'value_or' in 'std::shared_ptr<flight::types::HostAudioCapabilities>'
```

So `initializeHost` and `createHost` do not compile for ANY instantiation, and `@flighthq/host`'s six `host.test.ts`
cases have no callable surface to port to. Measured by instantiating `createHost<flight::types::Host>()` and
`createHost()`: the first fails on the `.value_or` errors above, the second cannot deduce its `Capabilities` template
parameter because every parameter is defaulted.

## Minimal TypeScript

```ts
interface Host { audio: Group }
export function initialize(out: Host, capabilities: Partial<Host>): void {
  out.audio = (capabilities.audio ?? {}) as Host['audio'];
}
```

## Expected

Either the fallback is dropped, because the read cannot be absent:

```cpp
flight::row_set<flight::RowKey<"audio">>(out, flight::row_get<flight::RowKey<"audio">>(capabilities));
```

or the read is made genuinely optional first (a partial row read) so `value_or` is the right operation. What is not
correct is a `value_or` on a value that cannot be nullish.

## Affected modules

`@flighthq/host` `host.ts`: `initializeHost` for every group, and `createHost` through it. The shape is not
host-specific -- any emitted `maybeValue ?? fallback` where the read turns out to be required has the same defect.

## Carrying workaround

None. This repository carries no patch for it, does not edit the emitted module, and does not weaken the signature.
`tests/generated_host_test.cpp` builds its hosts by hand -- group by group -- because the factory cannot be called,
and says so at the top.
