# Animation track: number-carrier discrimination and optional-easing lowering

`@flighthq/animation`'s track module refuses to emit at `a627849`. The emitter writes a PARTIAL
header that omits five declarations, so a track cannot be cloned, trimmed, validated, or sampled
from portable C++ without a hand-written completion.

## Minimal TypeScript

```ts
// The concrete backing type is observable, and the result keeps it.
function cloneNumberBuffer(src: ArrayLike<number>): number[] | Float32Array {
  if (src instanceof Float32Array) return src.slice();
  const out = new Array<number>(src.length);
  for (let i = 0; i < src.length; i++) out[i] = src[i];
  return out;
}

// A nullable segment easing selects a callable or falls back to linear.
const easing = track.segmentEasings?.[segment] ?? linearEasing;

// An optional element of a parallel array is read only when present.
if (track.segmentEasings !== null && track.segmentEasings !== undefined && track.segmentEasings[segment] !== null) { … }
```

## Expected behavior

`cloneNumberBuffer` returns a `Float32Array` when its input was one and a plain array otherwise, so
the carrier survives the clone. A nullable segment easing falls back to the linear sampler. The
optional-indexed easing read is projected before the call.

Observed at `2b687ea` / `a627849`, from the module's own emission record:

```
code: unsupported-ir, stage: emission, line 203
cpp emission failed for @flighthq/animation/packages/animation/src/animationTrack.ts:
contextual union value type flight::SequenceView<double> is not a represented runtime domain
```

and the partial header the emitter still writes names what it dropped:

```
missing: function cloneNumberBuffer   -- source line 203
missing: function cloneAnimationTrack -- source line 12
missing: function trimAnimationTrack  -- source line 131
missing: function validateAnimationTrack -- source line 169
missing: function sampleAnimationTrack   -- source line 72
```

## Affected modules

`packages/animation/src/animationTrack.ts` directly. Eight modules are then refused only because they
depend on it: `animationBlendTree.ts`, `animationClip.ts`, `animationCrossfade.ts`,
`animationLayerStack.ts`, `animationRootMotion.ts`, `animationStateMachine.ts`, `contract.ts` and
`index.ts`.

## Carrying workaround

[`animation-track-complete-sampling-and-copy`](../../overrides/manifest.json) is a hand-written
override that shadows `flight/animation/animation_track.hpp` and supplies the module: it completes the
five omitted declarations with source-equivalent cloning, trimming, validation, step/linear/cubic
sampling, and quaternion normalization. The `Array`-versus-`Float32Array` discrimination is carried by
a narrow runtime query on the erased sequence, because the erased carrier cannot be recovered after
lowering and always settling on one backing would change a documented behavior.

It expires when the emitter represents `ArrayLike<number>` carrier discrimination, lowers nullable
segment-easing tests and the optional indexed fallback, and emits the whole module.
