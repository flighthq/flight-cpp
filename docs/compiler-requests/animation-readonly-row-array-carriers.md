# Owning arrays, readonly-row arrays and number buffers need conversions where they meet

Three emission defects in `@flighthq/animation` share one cause: the emitter keeps a value in one array
carrier and hands it to a slot declared with a different one, without the conversion C++ needs. Each is
emitted, not refused, and each header fails at C++ compile. They are carried today by two hand-written
overrides and had no request until now.

Observed at flight-compiler `2b687ea` (with this repository's compiler patches) against emitted
`@flighthq/animation` at flight `a627849`.

## 1. An owning array written into a field declared as an array of readonly rows

```ts
// animationBlendTree.ts initializeAnimationBlendTree; animationCrossfade.ts initializeAnimationCrossfade
const channels: AnimationBlendTreeChannel[] = [];
// ... channels.push(...) ...
out.channels = channels; // field: readonly Readonly<AnimationBlendTreeChannel>[]
```

The local is `flight::Array<flight::Ref<AnimationBlendTreeChannel>>`; the field is
`flight::Array<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<AnimationBlendTreeChannel>>>>>`.
`row_set` is handed the owning array, and `flight::Array` has no cross-element-type constructor (a static
assertion fires). TypeScript stores the same array object. The required shape is a field carrier that can
hold the owning array itself, or a view array over the same storage; an element-wise copy is only faithful
where, as at both sites here, the local is complete and unused after the store.

## 2. An assertion back to the owning element type before `push`

```ts
(existing.sources as AnimationBlendTreeChannelSource[]).push({ channelIndex, inputIndex });
```

`existing.sources` is a readonly-row array; the assertion is emitted as
`static_cast<flight::Array<flight::Ref<AnimationBlendTreeChannelSource>>>(existing->sources).push(...)`, a value
conversion that does not exist (and would push into a copy if it did). See also
[assertion-lowered-to-unchecked-static-cast](assertion-lowered-to-unchecked-static-cast.md). The push must
reach the field's own storage.

## 3. A writable number buffer passed where a readonly number sequence is expected

```ts
// addAnimationSample(out: number[] | Float32Array, base: ArrayLike<number>, ...)
addAnimationSample(out, out, tree.sampleScratch, input.weight, channel.track.quaternion);
```

`out` is `std::variant<flight::Array<double>, flight::Float32Array>`; `base` is `flight::SequenceView<double>`.
No conversion is emitted from the variant to the view, though each alternative converts on its own. The
required shape visits the variant and views the held buffer (the same storage, read-only).

## Carrying workarounds

- [`animation-blend-tree-array-structural-ref-mismatch`](../../overrides/manifest.json) (all three, in
  `animation_blend_tree.hpp`);
- [`animation-crossfade-channels-array-structural-ref-mismatch`](../../overrides/manifest.json) (the first, in
  `animation_crossfade.hpp`).
