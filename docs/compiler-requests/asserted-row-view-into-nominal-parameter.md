# A downcast assertion kept as a structural view must be recovered before a nominal parameter

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, a `Node2D` asserted `as MovieClip` is lowered correctly to a
structural view over the same owner. That view is then passed directly to functions whose parameter is the
nominal `Ref<MovieClip>`. No conversion exists, so the header does not compile.

## Source

```ts
// packages/swf/src/swfFrameAction.ts
return (target: Node2D): void => {
  const clip = target as MovieClip;        // MovieClip extends Node2D
  ...
  if (command.kind === 'stop') stopMovieClip(clip);
  else if (command.kind === 'play') playMovieClip(clip);
  ...
};
```

## Emitted mismatch

```cpp
flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::MovieClip>>>> clip =
    flight::structural_ref_cast<flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::MovieClip>>>>>(target);
...
flight::movieclip::stop_movie_clip(clip);   // stop_movie_clip(flight::Ref<flight::types::MovieClip>)
```

```
flight/swf/swf_frame_action.hpp:88: error: could not convert 'clip' from
'flight::StructuralRef<flight::RowWritable<flight::RowOf<Ref<flight::types::MovieClip>>>>' to 'flight::Ref<flight::types::MovieClip>'
```

GCC 15.2 gives the same error for all six movie-clip calls in the frame action (lines 88-100+). The view
keeps the original shared owner, so the identity is preserved up to this point.

## Expected lowering

Where a structural view of exactly one nominal row reaches a parameter of that nominal reference type,
recover the reference with the runtime's existing checked path. The emitter already writes this path
elsewhere, for example `image/image_resource.hpp`:

```cpp
flight::movieclip::stop_movie_clip(flight::structural_ref_cast<flight::Ref<flight::types::MovieClip>>(clip));
```

`structural_ref_cast<Ref<T>>` on a single-row view returns the same owner through
`shared_exact_object<T>()`, never a copy or a native pointer cast. A scratch copy of the header with only
these six recoveries compiles (GCC 15.2, one worker).

## Why nothing is carried

The TypeScript assertion is unchecked, but the recovery is checked: a `Node2D` that is not a `MovieClip`
throws `std::bad_cast`. Whether the downcast should be checked at the assertion, at the boundary, or proven
by a frame-action target contract is the emitter's decision. Inserting the cast by repair or override would
silently choose an exception shape the source does not state. An implicit view-to-`Ref` conversion in the
runtime would change overload resolution SDK-wide. The rows stay open on this request.

## Affected

Cluster `no-viable-conversion-4ca7626f`, 3 headers: `swf/swf_frame_action.hpp`, and `swf_script_handler` and
`swf_script_tag_family`, which include it. The latter two were not recompiled.
