# A call result and its destination must be the same anonymous shape, not two with equal members

`audioMixer.ts` refuses with a union-conversion error whose two carriers differ only in the *name* of an
anonymous structural shape: the destination names the shape after the members it read, the source names
it after the runtime record the call returned.

Measured at `2b687ea`, flight `a627849`, after `compiler-array-from-runtime-member` cleared the module's
first refusal (`Array.from` on the ambient `Array`).

## Minimal shape

`@flighthq/media`, `packages/media/src/audioMixer.ts`:

```ts
return Array.from(runtime.activeChannels);
```

where the function's declared result is `readonly AudioChannel[]` and the runtime record's field is a
`Set` of channel references.

## Observed

```
cpp emission failed for @flighthq/media/packages/media/src/audioMixer.ts: contextual C++ union conversion
requires equivalent source union evidence: target optionalSingle carriers
[flight::Set<flight::Ref<active_channels_channels_paused_by_mixer_buses_bus_nodes_channel_to_bus_graph_6738b3976aa8c03b>>]
from call source optionalSingle carriers [flight::Set<flight::Ref<AudioMixerRuntime>>]. Narrow or …
```

The two `flight::Set<…>` carriers are the same at the member level: the target's element name is the
emitter's hash of the anonymous object shape the destination declares, the source's is the module's own
`AudioMixerRuntime`. So the conversion is refused for a naming difference between two anonymous
structural shapes, not for a member or representation difference.

## Expected behavior

Either the two shapes are recognized as one structural row (they have the same members by construction —
the destination reads the fields the source object carries), or the emitter names the anonymous shape
once and reuses it, which is the same rule the derived duplicate-structural-struct alias already applies
across packages.

## Affected modules

`@flighthq/media` — `audioMixer.ts`. No other package is known to hit it today; the shape (a call result
stored into a row whose fields were read) is common enough that the request is worth filing on its own.

## Carrying workaround

None. Nothing narrower is available: the source expression is `Array.from(runtime.activeChannels)`, and
the disagreement is between two emitter-chosen names for one shape, which a source edit cannot reach.
