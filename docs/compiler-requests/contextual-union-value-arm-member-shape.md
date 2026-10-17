# A constructed value must match the destination alternative's declared members

The emitter refuses an expression whose value is a reference to an existing owner while the destination
alternative declares a *different* member shape — a member the destination requires optional or absent, or
one given an incompatible type. Eleven declarations of `@flighthq/preferences`' storage module are refused
by this one rule, all of them outcome readers or writers that construct a result arm from a value already
in hand.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/preferences`.

## Observed

```
cpp emission failed for @flighthq/types/packages/types/src/Storage.ts: contextual union value type
flight::Ref<reason_value_8c1689e7820d42eb> is not a represented runtime domain: the value leaves a member
the destination requires optional or absent, or gives it an incompatible type, so it is not the shape an
alternative declares. This expression refers to an existing owner. Keep the value in the declared
destination arm from the point it is created, or explicitly rebuild the declared result in source where
changing identity is intended. The compiler will not retype, copy, or materialize the existing object into
a sibling owner.
```

Eleven declarations, all in `packages/preferences/src/storage.ts`:

| declaration | source line |
| --- | --- |
| `clearStorageNamespace` | 59 |
| `getNamespacedStorageEntries` | 109 |
| `getStorageBoolean` | 152 |
| `getStorageJSON` | 230 |
| `getStorageNumber` | 263 |
| `migrateStorage` | 293 |
| `removeStorageItems` | 349 |
| `setStorageItems` | 391 |
| `setStorageJSON` | 405 |
| `removeStorageItemFromBackend` | 467 |
| `setStorageItemOnBackend` | 481 |

The emitted module is therefore a 642-line partial with those eleven omissions; `@flighthq/preferences` is
`assisted` through `overrides/include/flight/preferences/storage.hpp`, which supplies the module and
declares one gap of its own (a generic JSON read outside the represented domains).

## What the rule is asking for

The message is unusually specific, and it is worth quoting for the fix rather than paraphrasing: keep the
value in the declared destination arm **from the point it is created**, or **rebuild** the declared result
in source where changing identity is intended. The sites above do neither: they read a value into a local
and then construct an outcome arm from it, so the arm's member shape and the local's type are decided in
two different places and the emitter will not reconcile them by copying, retyping, or materializing a
sibling owner.

## Distinguish from the requests already filed

[`task-of-union-domain-unrepresented.md`](task-of-union-domain-unrepresented.md) carries the same refusal
code on a `Task<…>` carrier. [`nullable-optional-presence.md`](nullable-optional-presence.md) is about an
absence channel a *partial row* needs. This one is the member-shape half: the destination arm declares the
member optional or absent, or at another type, and the value expression does not come from that arm.

## Carrying workaround

`overrides/include/flight/preferences/storage.hpp`, status `partial`, supplies the module with its own
outcome-variant vocabulary and its declared JSON gap. The copy predates this emission and is retained
rather than re-derived in this pass: re-deriving would replace a working copy with another copy of the same
eleven declarations carrying the *same* declared gap, because that gap is a runtime representation limit
around the same rule rather than a defect of the copy.

## G2 SVG image preserves the original Sprite resource and geometry

At Flight `a627849` / compiler `806d257`, `svgImageElement.ts:24` refuses createSvgImageNode
with `cpp-contextual-union-value-type-unrepresented`: existing `Ref<types::Sprite>` does not
match the destination alternative's member shape. Source Sprite extends Node2D, an intersection of
Node<Node2DTraits> and Node2DTraits. Native Sprite is an independent ReferenceEnabled struct, while
Node2D is a StructuralRef<RowMerge<...>> alias; the compiler must preserve the actual Sprite owner,
including its data, texture and runtime, through that view and nullable result. SpriteData/Node2DData
and SpriteRuntime/Node2DRuntime are also independent emitted owners. Those declarations expose
representation seams; the refusal does not name an exact offending member, so none is guessed.

The complete source image contract remains required:

- Read href through svgAttribute, which prefers the ordinary attribute then xlink:href by nullish
  fallback, retaining an empty string. Read the optional caller resolver without adding I/O or decoding.
  Do not call it for a missing href or unwired hook. Otherwise call once, normalize its nullish result
  to null, and preserve exceptions. Return null with one Drop diagnostic: missing href,
  resolver-unwired or unresolved-image, distinguished in that order; only a present href carries
  detail {href}. The older introductory comment says skip, but the executable branches all use Drop.
  No default picture or silent success can replace these paths.
- Width/height default to intrinsic image dimensions and x/y to0 through parseSvgLength. That helper
  accepts finite parseFloat prefixes (including unit suffixes), otherwise uses the fallback; it is not
  new strict validation or a percentage resolver. Keep the resolver's original ImageResource as the
  2d texture's source, and the resulting texture in the newly created Sprite data. It is a borrowed
  host drawable, not a Bitmap copy or narrowed host-image representation.
- With positive intrinsic dimensions and nonnegative requested width/height, map the intrinsic
  [0,0,image.width,image.height] viewBox into {height,width,x,y}. Preserve default xMidYMid meet,
  explicit none, defer/alignment choices and meet/slice behavior of createSvgViewBoxMatrix. Otherwise
  retain the source translation-only matrix; do not clamp invalid dimensions or always substitute
  identity/uniform scaling. Zero requested dimensions follow the actual nonnegative guard.
- Apply the shared appearance on that same Sprite with the original element, parent style, context
  and geometry. It resolves style/opacity/visibility/id, reports unsupported filters, and composes the
  author transform before geometry (author*geometry). Keep original transform order and style domain.
  Record a fresh rectangle (0,0,intrinsic width,intrinsic height) under this same node in the context's
  objectBoundingBoxes Map before invoking the registered clip family, then return this Sprite as the
  original Node2D owner. The intrinsic box remains distinct from the laid-out viewport. No clip family
  means no callback; a registered family sees the original node/context/element, not a copy.

The pinned image-family/import/conformance tests establish resolver source identity, transform
composition, aspect ratio and intrinsic objectBoundingBox clipping requirements. They are source
requirements rather than native image or SVG parser tests in this unit. The
[retained-callee repair evidence](retained-declaration-calls-omitted-callee.md#g2-svg-image-handler-retains-its-refused-image-helper)
qualifies only declaration/header availability: actual unused compile/link/run passes; using the
real handler compiles then fails to link on the exact absent helper. Normal generation retains the
source refusal and complete original body excerpt. No runtime owner retag/copy, fake image/decoder,
parser implementation, narrowed input or full SVG/native image functionality is supplied.
