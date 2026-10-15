# Readonly video helper carriers

The partial video override supplies eight source readonly boundaries at pinned
Flight `a62784923f3be814463286c9fb28edfe6ab15789`:

- `cloneVideoTexture` and `copyVideoTexture` retain a readonly four-arm source row.
- `getVideoTextureUvMatrix` and `getVideoTextureInverseUvMatrix` read that same union.
- `getVideoTextureWidth`, `getVideoTextureHeight` and `isVideoTextureFrameReady` read
  readonly host callbacks and a readonly texture union.
- `setVideoTextureSource` reads readonly host callbacks and mutates its native texture.

`ReadonlyVideoTexture` visits the concrete readonly row for each `TextureLike` arm.
The pinned emitter currently erases `Readonly<TextureLike>` to the native variant;
the additional represented rows expose a live readonly input without recovering a
native owner. Existing native convenience overloads remain separate. Every added
signature is assignable as a function value.

Clone follows the upstream common-field object literal before reading dimension
and content. Sampling and UV owners are independently cloned, scalar source owners
remain shared, and source lists receive fresh identity. Copy captures scalar values,
then performs nested writes in source order before copying content and final
rotation/version. It keeps destination nested owners, including when a source
getter replaces a destination field after that field's argument was evaluated.
Self-copy and cross-aliased UV owners retain those same effects.

The UV path reads the source fields at each original access, including repeated
flip/vector reads, then writes the output matrix. It retains exact quarter turns
and uses the existing in-place inverse, including NaNs for singular scale. Host
queries read one borrowed source and short-circuit callback lookup/invocation in
order. A width callback may replace the later height callback. Source replacement
creates its fresh image before assigning it, preserving old texture source/version
when a callback fails. Non-2D textures and closed non-image source shapes have no
borrowed source for the query helpers and do not invoke host callbacks.

The exact pinned TypeScript oracle and native tests cover these ownership, alias,
getter/callback ordering, error and function-value boundaries. The constructor
candidate remains a separate publication unit.

`advanceVideoTexture` is still unsupplied for its readonly host carrier; its
[asserted source and undefined callback argument](video-advance-asserted-host-source.md)
requires an explicit representation crossing. The override remains partial, also
retaining the constructor's bounded declared option-shape limitation. Generic
mapped-union lowering, extra dynamic own-property spreads, the cube setter and
render factory remain separate gaps.
