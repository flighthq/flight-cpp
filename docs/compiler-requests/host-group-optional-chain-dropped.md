# An optional chain through a reference declared present must still test it
Run-time null dereference. A `?.` whose receiver the checker considers non-nullable is lowered as a plain
member read. That is sound for a receiver that cannot be absent. A `flight::Ref` can be null, though, and
upstream relies on the run-time test.
This file was written from builder's independent reproduction. builder7 filed the same defect at this path in
unmerged Host work (60ae8043), and Integration reconciles the two texts.
## Upstream shape
`@flighthq/types` declares every Host capability group as required:
```ts
interface Host { readonly audio: HostAudioCapabilities; readonly clipboard: HostClipboardCapabilities; /* ... */ }
```
`@flighthq/host` `hostQuery.ts` reads each slot through `?.` anyway, on purpose:
```ts
// A hand-built stand-in with a group missing must produce `null` rather than a TypeError. The `?.` is at
// exactly one level -- the slot below it is optional by declaration and the `??` handles it.
export function getHostAudioDevice(host: Readonly<Host>): HostAudioDeviceCapability | null {
  return host.audio?.device ?? null;
}
```
## Emitted (flight-compiler 2b687ea with the carried patches, flight a627849)
```cpp
return flight::row_get<flight::RowKey<"audio">>(host)->device;
```
The semantic layer marks the chain's receiver `receiverNullish: 'excluded'` from the declared type, and the
emitter then drops the `?.`. `Host::audio` is a `flight::Ref<HostAudioCapabilities>`, so a Host built without
that group holds a null pointer, and the read dereferences it.
Measured natively against the generated `host_query.hpp`:
- `get_host_audio_device` on a `make_ref<Host>()` with no groups set segfaults;
- a Host whose `audio` group is present returns the very capability object;
- a present group without the `device` slot returns null.
## Minimal TypeScript
```ts
interface Device { id: number }
interface Group { device?: Device; count: number }
interface Host { readonly audio: Group }
export function f(host: Host): Device | null {
  return host.audio?.device ?? null;   // emitted: return host->audio->device;
}
```
## Required behaviour
When the excluded receiver is a nominal reference, the chain tests it at run time. Where its member's storage
already admits absence (a single `std::optional`), a null receiver yields that absent state, so the chain's
result type is unchanged:
```cpp
return ([&]() -> std::optional<flight::Ref<Device>> {
  auto optional_chain_receiver = host->audio;
  if (!optional_chain_receiver) return std::nullopt;
  return optional_chain_receiver->device;
}());
```
## Boundaries
- **A required member under a missing group** (`host.audio?.count` with `count: number`): TypeScript types the
  result as `number`, so there is no absent state to produce without widening the result type. This is left as
  the plain read.
- **Value receivers** (`text?.length` on a `string`): these cannot be null and are not tested.
- **Structural row reads, variant projections and other spellings:** these are not changed. The guard applies
  only where the plain read is exactly `receiver->member`.
## Carried compiler patch
[`compiler-declared-present-reference-chain.patch`](../../source-patches/compiler-declared-present-reference-chain.patch)
implements the behaviour above, with fixtures in `cppDeclaredPresentReferenceChain.test.ts`.
A scratch regeneration of `@flighthq/host` was run with and without the patch. Across Host's closure the only
change is `host_query.hpp`: its 28 `row_get<key>(host)->member` slot reads each become the guarded form. Natively:
- a missing group answers null for both slots tested natively (`get_host_audio_device` and `get_host_clipboard_formats`), where before it segfaulted;
- a present group still returns the very capability object.
The Host row is not regenerated here; it changes when its owner regenerates.
