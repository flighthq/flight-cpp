# A WeakMap value needs a representable C++ type, and naming the literal is not enough

`videoChannel.ts` refuses on the value type of a module-level `WeakMap`, and the obvious source reading —
"the inline literal needs a name" — does **not** clear it. That negative result is the useful part of this
request.

Measured at `2b687ea`, flight `a627849`.

## Minimal TypeScript

`@flighthq/media`, `packages/media/src/videoChannel.ts`:

```ts
interface VideoChannelRuntime {
  loopsRemaining: number;
  onEnded: () => void;
}

const channelElements = new WeakMap<VideoChannel, HostImageSource>();
const videoChannelRuntimes = new WeakMap<HostImageSource, VideoChannelRuntime>();

// …
const onEnded = (): void => completeVideoChannel(hostVideo, channel);
videoChannelRuntimes.set(element, { loopsRemaining: channel.loops, onEnded });
```

## Observed

```
cpp emission failed for @flighthq/media/packages/media/src/videoChannel.ts: flight-cpp WeakMap value
requires a proven C++ representation
```

## What was tried, and what it rules out

A source patch that names the literal with the module's own declaration —

```ts
const runtime: VideoChannelRuntime = { loopsRemaining: channel.loops, onEnded };
videoChannelRuntimes.set(element, runtime);
```

— was written, applied and **did not clear the refusal**; the message was byte-identical. So the unproven
thing is not the literal's spelling but the *value type* itself (or the pairing of that value with this
key type). The key is a bound external carrier (`HostImageSource` → `flight::host_sdl::ImageSource`,
`ownership: shared`), and the value is a record holding a callable. Which of the two the emitter cannot
prove is not yet isolated, and the patch was withdrawn rather than carried as an ineffective workaround.

## Expected behavior

A `WeakMap<K, V>` whose value is a closed record with a `std::function` member and whose key is a
reference-backed external carrier is representable: the runtime's `WeakMap` already stores reference
keys, and a record of a number and a callable is an ordinary emitted struct. So the lowering should
either name the value record once and store it, or say which member of the pair lacks a representation —
the message as written does not distinguish "the value type is unrepresentable" from "the value type
needs one owner", and a reader cannot act on it without trying both, as this request records.

## Affected modules

`@flighthq/media` — `videoChannel.ts`, whose only refusal this is. Its sibling `audioMixer.ts` refuses
separately.

## A second package, and a sharper form of the same refusal

`@flighthq/lifecycle` refuses with the identical message, and its module's override records it as the
third of five defects there:

```ts
const _savedState = new WeakMap<AppLifecycle, Record<string, unknown>>();
```

Unlike media's, this value is a `Record` whose value domain is `unknown`. If the unproven part is the
*unknown* domain inside the value rather than the record wrapper, that is the same defect seen from the
other side, and a fix that represents this value would very likely fix media's too. I cannot separate the
two yet — lifecycle refuses earlier on other defects, so its WeakMap is not reachable for measurement —
but the pairing is worth knowing before either is worked on alone.

There is also [`nullable-reference-weak-map-value.md`](nullable-reference-weak-map-value.md), which is a
*different* WeakMap refusal (a nullable value type). Three documents in this family now, and they should
be read together before any one of them is fixed.

## Carrying workaround

None, deliberately: the patch above is the only shape a source edit can take, and it is ineffective.
