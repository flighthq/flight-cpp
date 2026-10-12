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

## Carrying workaround

None, deliberately: the patch above is the only shape a source edit can take, and it is ineffective.
