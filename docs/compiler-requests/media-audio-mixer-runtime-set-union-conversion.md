# Equivalent structural payloads must share one generic owner

Structurally equivalent payloads can acquire different emitted names before they are nested in a generic
container. The resulting `Set<...>` or `Signal<...>` specializations are unrelated C++ owners even
though TypeScript proved the payloads equivalent. `audioMixer.ts` supplies the anonymous-shape case: the
destination names the shape after the members it read, while the source names it after the runtime
record the call returned.

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

The initial measured module was `@flighthq/media` — `audioMixer.ts`. The P-3D2 audit below adds the
same carrier-identity rule through a different generic container.

## Same carrier family: a Signal over equivalent readonly rows

The P-3D2 resource loader supplies a second exact reduction. Two progress interfaces declare the same
four cells, but their readonly projections keep the nominal declaration name inside a callback, and
that callback keeps the differing name inside its `Signal` owner:

```ts
interface Signal<T extends (...args: any[]) => void> { slots: T[] }
interface LoadProgress { loaded: number; phase: 'download' | 'upload'; total: number; url: string }
interface SceneProgress { loaded: number; phase: 'download' | 'upload'; total: number; url: string }
interface LoadOptions { progress?: Signal<(value: Readonly<LoadProgress>) => void> }
interface SceneOptions { progress?: Signal<(value: Readonly<SceneProgress>) => void> }

declare function load(options?: Readonly<LoadOptions>): void;
export function forward(options?: Readonly<SceneOptions>): void {
  load(options ? { progress: options.progress } : undefined);
}
```

At flight-compiler `2b687eaf6b631e0fa4d7d6e18dad93b845f99633` this refuses with two optional
carriers whose only difference is `RowOf<Ref<LoadProgress>>` versus `RowOf<Ref<SceneProgress>>` inside
the callback and `Signal`. Changing `SceneOptions` to use `LoadProgress` emits, proving that optional
construction and `Signal` are both represented. The same-member readonly rows need one canonical
structural runtime identity before the generic owner is selected; converting two already-distinct
`Signal<...>` structs afterward would not preserve identity.

The corpus site is `scene3d-resources/sceneDocumentSource.ts:16`, where
`Scene3DDocumentLoadOptions.progress` is forwarded to `LoadOptions.progress` for `loadBytes` and
`loadText`. It is the same representation rule as the Set case above — equal structural payloads must
not mint different generic-container owners — rather than a new imported-union request.

The affected modules are therefore `@flighthq/media`'s `audioMixer.ts` and
`@flighthq/scene3d-resources`'s `sceneDocumentSource.ts`.

## A second package, same rule, different pair of names

`@flighthq/app`'s `appLoop.ts` refuses identically — `contextual C++ union conversion requires
equivalent source union evidence` — with the destination naming an anonymous shape after its members
(`fixed_accumulator_fixed_time_step_fps_buffer_…_67b4093dbdac7169`) and the source naming a `LoopState`
record. Two packages now, and in both the disagreement is the emitter's name for one shape rather than a
member or representation difference. `compiler-intl-optional-and-union-carrier-evidence.md` covers the
adjacent carrier-evidence question.

## Carrying workaround

None. Nothing narrower is available: the source expression is `Array.from(runtime.activeChannels)`, and
the disagreement is between two emitter-chosen names for one shape, which a source edit cannot reach.
For the progress site, reusing `LoadProgress` in the public Scene3D callback would remove the duplicate
declaration, but that is a Flight API decision rather than a compiler workaround and is not carried.

## What was tried, and what it rules out

Two source-level repairs were written, applied through the patch manifest and measured at flight
`a627849`, flight-compiler `2b687ea`. **Both failed with a byte-identical refusal**, and the patch
manifest's own gate reported them: *"did not remove the refusal they name ... do not carry it
unexamined."* Neither is carried.

1. **Naming the element type where the collection is created.** The mixer runtime record is stored as a
   literal with two empty Sets; the patch named their elements and stored the record through a local
   annotated with the module's own `AudioMixerRuntime`:

   ```ts
   const runtime: AudioMixerRuntime = {
     activeChannels: new Set<AudioChannel>(),
     channelsPausedByMixer: new Set<AudioChannel>(),
     …
   };
   mixerRuntimes.set(mixer, runtime);
   ```

2. **Naming the element on the rebind itself.** With the patch above reverted, the refusal localizes to
   `registerBusInReverseMap` (source line 278) — `let runtimes = busToMixerRuntimes.get(bus); … runtimes
   = new Set();` — which is exactly the shape this request's own message prescribes fixing ("For a Map
   or WeakMap lookup followed by a rebind, ensure every rebound member already has the declared value
   owner member's carrier"). The patch did precisely that:

   ```ts
   runtimes = new Set<AudioMixerRuntime>();
   ```

   Refused identically.

So the disagreement is **not** reachable from the source: annotating the collection with the declared
element type changes nothing, including at the very site the refusal points to, and including the fix
the message itself names. That is a stronger negative than "no workaround was found" and it means the
remaining repair is on the emitter side, as the request above concludes.

One further measurement, from the same session, that a reader should have before attempting the
regeneration path: regenerating `@flighthq/media` package-scoped while the SDK is mid-binding-profile
transition produces a tree inconsistent with media's own dependencies (`@flighthq/entity`,
`@flighthq/log`, `@flighthq/math`), and it moves `audio_mixer.hpp` from *17 declarations missing* to
*1 missing* — while making it stop compiling, because the single remaining omission
(`registerBusInReverseMap`) is called from emitted code. The compile count goes 4/7 to 2/7. So the
regeneration is not a package-scoped operation today; it has to be done with the dependency closure.
