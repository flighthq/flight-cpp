# An optional chain on a host capability group is lowered as an unconditional dereference

`@flighthq/host`'s accessors read a capability through its GROUP: `host.audio?.device ?? null`. The group access is
optional-chained on purpose, and the source says so:

> The group access is optional-chained even though `Host` declares every group non-optional: these are the diagnostic
> tier, reached exactly when a host is suspected of being malformed, and a structurally built stand-in with a group
> missing must produce `null` rather than a TypeError.

The emitted C++ drops that chain. `Host`'s groups are plain `Ref`s, so "a group missing" is a null `Ref`, and the
lowering dereferences it:

```cpp
inline std::optional<flight::Ref<flight::types::HostAudioDeviceCapability>> get_host_audio_device(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Host>>>> host) {
  return flight::row_get<flight::RowKey<"audio">>(host)->device;   // -> on a possibly-null Ref
}
```

Measured: a host row that carries no `audio` group PANICS with

```
thread panic: member access within null pointer of type 'element_type' (aka 'flight::types::HostAudioCapabilities')
```

where the source returns `null`. This is the one case the function exists for -- the malformed host -- so the
diagnostic tier aborts on exactly the input it is meant to diagnose.

## Minimal TypeScript

```ts
interface Group { device?: Device }
interface Host { audio: Group }
export function getDevice(host: Readonly<Host>): Device | null {
  return host.audio?.device ?? null;
}
```

## Expected

The absent group must produce the absent read, at the one level the source chains:

```cpp
const auto group = flight::row_get<flight::RowKey<"audio">>(host);   // Ref, possibly null
return group ? group->device : std::nullopt;
```

The `?? null` on the SLOT is already correct -- an empty group's absent slot reads as an absent optional -- so only
the group level is wrong.

## Affected modules

`@flighthq/host` `hostQuery.ts`: every accessor in the file takes this shape, and `hostExplain.ts`'s
`explainHost*` family calls them. The pattern is not host-specific: any emitted `obj?.member ?? fallback` over a
reference that can be null has the same defect.

## Carrying workaround

None. This repository carries no patch for it, does not broaden `structuralMemberTable`, and does not edit the
emitted module. `tests/generated_host_test.cpp` covers the present-group cases around it and records this one as a
divergence at the point of use, since a panic cannot be asserted in-process.
