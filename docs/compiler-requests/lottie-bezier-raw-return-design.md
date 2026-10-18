# Bezier raw values must survive the return and storage boundary

This is the first design deliverable requested by Foreman for the erased Bezier capability. It introduces probes and a proposal, with no product compiler, runtime, public ABI, source patch, or SDK change. The [evidence](../../artifacts/lottie-bezier-raw-return-design.json) distinguishes builder5's existing 20 source cases and eight owner/API observations from the eight additional original-source cases and ten existing-runtime component checks measured here. Native Bezier execution remains unqualified.

## Concrete counterexample

At Flight pin `a62784923f3be814463286c9fb28edfe6ab15789`, the complete original `lottieBezierPath.ts` has SHA256 `da3f167f812fc52463da4e6de9c9ae346373312f18c7c6f50f5f40c713115ce2`. Its presence guard returns the original object for `{v: [['x', 2]], i: [], o: []}` without reading `v`. Its original flatten function returns `['x', 2]`, despite declaring `number[]`. Neither an exact native-object recovery nor `Array<double>` can carry that result.

The return escapes further than the helper:

| Original operation | Measured result | Existing native boundary |
| --- | --- | --- |
| `flattenLottieShapePath` | `['x', 2]` | `Array<double>` return and local output |
| Original `createLottieTrack`, with a probe callback invoking the original selector and flatten | `track.values` contains `['x', 2]` | Callback result, `samples`, `samples.flat()`, and `AnimationTrack.values: SequenceView<double>` |
| Original `sampleAnimationTrack`, one retained keyframe | Sample output contains `['x', 2]` | Numeric output-array storage |
| Original `createLottieBezierPath` and path append functions | Returned `Path.data` contains `['x', 2]` | Numeric parameters and `Path.data: Array<double>` |
| Original `unflattenLottieShapePath` | Returned vertices contain `[['x', 2]]` | Native nested numeric-array fields |

An object coordinate also retains its exact referent through the original track callback, stored values, and sampler; a later mutation is visible. This is a physical storage and callable-result issue, not only an assertion issue. Calling `JsonScalarCell::as_native_number()` at any merely annotated numeric argument, return, array insertion, or field assignment would reject a result the measured JavaScript preserves.

## Required ordering and domain

Selection keeps the source's `Array.isArray`/first-element choice, null/type check, and inherited-aware `'v' in path`. It performs zero `v` reads or writes, including for `{v: 7}`, present undefined, and inherited/throwing getters. Builder5's 20-case original-module evidence remains the baseline; its eight native checks establish raw parsed-owner retention, live mutation, lifetime, and the existing typed-row failures. Those checks are not a native implementation of the source module.

The added probes establish constraints on the next implementation:

- `[path.v, path.i, path.o]` reads its members before inner iteration. With `v` returning `7` and `i` throwing a particular `RangeError`, that same error wins; an early numeric-array check on `v` would change behavior.
- Strings are source-reachable iterables. `v: '🙂'` yields one string iterator value, whose subsequent `[0]` and `[1]` reads are UTF-16 code units. An array-only view rejects this accepted path.
- A point getter can append another point before the array iterator finishes. The original flatten includes that point. A C++ range-for over the current `SequenceView` snapshots its end; the existing `ArrayIterator` protocol demonstrates the needed live behavior.
- Only nullish coordinates default to zero. `false` remains `false`, and strings and object references remain their original values.

`NamedProperties::has` is an own-key API. It cannot replace source `in`, and own-only scalar reads cannot be assumed to implement arbitrary inherited getters. Parsed JSON reads, represented native accessors, and unrepresented foreign prototype/iterator behavior must remain distinct capability questions at the actual operation. No such gap may become a stronger selection guard or a fabricated empty array.

## Minimal proposed representation

1. Track raw provenance through the existing JSON/erased row analysis using canonical declaration and call identities. Preserve the original complete IR bodies. Do not make every `number`, `number[]`, readonly row, or function result globally raw.
2. Reuse the same `RowOwner`/`Any` at selection. Represented member reads must retain their raw `JsonScalarCell` values, presence, original getter calls, and error timing. Defer iterable operations until the source starts iteration; obtaining a lazy view must not validate its declared element schema.
3. Use the existing `SequenceView` for lazy indexed access and the existing live `ArrayIterator` protocol, or an equivalent emitted live index loop, for array `for-of`. A raw string branch additionally needs JavaScript code-point iteration followed by existing UTF-16 property reads. Do not select the array-only factory for all source-reachable iterables. A general foreign `Symbol.iterator` protocol remains a separate unrepresented boundary, not a false guard result.
4. Use existing `Array<JsonScalarCell>`/`SequenceView<JsonScalarCell>` storage in an additive raw specialization. Allocate the output arrays the source actually creates, retaining raw referents inside them. Do not materialize input trees or manufacture a native `LottieShapePath`. Preserve the original named callable identity while routing raw arguments and results through the same original IR.
5. Propagate this result representation through both actual shape-channel callback forms, `createLottieTrack`'s sample storage/flattening, original sampler writes, and the Path builder/returned data. Ordinary native signatures remain unchanged. No raw sidecar may shadow an `Array<double>` field, and no implicit conversion may pretend it can store a string or object there. Source-authored numeric operations use the existing scalar semantics at their original sites.

The smallest honest raw entry therefore needs raw physical storage for the returned Path/track and sample/current arrays as well as a raw flatten result. The existing native `Path` and `AnimationTrack` layouts cannot hold this domain. Their source factories allocate fresh entities; any additive raw specialization must preserve those allocation, identity, EntityRuntimeKey, and lifetime facts with one set of cells. This storage/entry ABI needs a scope decision before product changes. Changing the global scalar/array ABI, replacing source bodies, narrowing inputs, and converting existing native owners into copied raw objects are excluded alternatives.

## Existing-family read and iteration components

The [additional component receipt](../../artifacts/lottie-bezier-read-iterator-components.json) records 24 passing native checks using the existing runtime APIs. An erased native object keeps its original owner, and inherited presence performs no getter read. The own-key JSON reader returns absent for that inherited getter; the existing inherited-accessor `typeof` operation explicitly refuses, without invoking it. A direct getter call is only an instrumentation control. This does not establish inherited getter value dispatch.

Existing deferred own accessor cells preserve left-to-right member capture and the exact exception before iteration starts. A private probe adapter over the existing shared `ArrayIterator<Any>` demonstrates live parsed-array advancement, shared cursor position, sticky exhaustion, present undefined, and string code-point advancement followed by the existing UTF-16 indexed reads, including a lone surrogate. The adapter exists only inside the fixture; it is not an installed iterator API or generated Bezier implementation. Foreign `Symbol.iterator` remains explicitly unrepresented. These results reuse the previously measured original-source oracle; neither the original 20 cases nor the added eight cases were rerun for this component-only change.

No public ABI activation or new physical storage family is included. The inherited getter value operation and complete foreign iterator protocol still need representation at their actual use sites; an own-only read or array-only factory cannot silently stand in for them.

## Implementation sequence after the ABI decision

Keep the existing refusal until a whole original module and its real consumer can use the chosen raw representation. First qualify the presence/owner/read and array/string iterator operations, then original flatten/unflatten and named return/callback forwarding, then the original Path/track physical storage and sampler consumers. Compare the reused 20 source cases plus the added eight return/iteration cases, retain the eight owner controls, and exercise numeric native ABI controls separately. A component probe or emitted helper alone does not establish whole Bezier support. No normal generation or inventory change belongs to this design task.
