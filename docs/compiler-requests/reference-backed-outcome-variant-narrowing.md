# A reference-backed outcome variant is narrowed as if it were a record

An external capability's method returning a generic outcome union gives back a reference-backed
`std::variant`. Reading its discriminant emits a member access on the variant itself, so the header
does not compile and the module is refused at emission.

## The shape

`@flighthq/types`, `packages/types/src/Storage.ts`:

```ts
export type StorageValueOutcome<Value, FailureReason extends string> =
  | { readonly reason: 'ok'; readonly value: Value }
  | { readonly reason: FailureReason; readonly value: null };
export type StorageKeysResult = StorageValueOutcome<readonly string[], StorageKeysFailureReason>;
```

and its caller, `@flighthq/preferences`, `packages/preferences/src/storage.ts:135`:

```ts
export function getNamespacedStorageKeys(
  hostPreferences: Readonly<HostPreferencesCapability>,
  namespace: Readonly<StorageNamespace>,
): StorageKeysResult {
  const prefix = namespace.prefix + '.';
  const result = hostPreferences.keys();
  if (result.reason !== 'ok') return result;
  return { reason: 'ok', value: result.value.filter((key) => key.startsWith(prefix)).map((key) => key.slice(prefix.length)) };
}
```

`hostPreferences` is an external bound capability, so `keys()` returns the variant through a
reference. The four functions that narrow a result this way (`getNamespacedStorageKeys`,
`getStorageBoolean`, `getStorageBooleanOr`, and the JSON readers) are what refuses the module.

## Expected behavior

The discriminant read lowers through the variant — `std::visit` over the alternatives, or a
`holds_alternative` test — and the projected `value` is read from the arm the test proved, as the
emitter already does for outcome unions that arrive by value.

## Observed at `2b687ea`, flight `a627849`

The module is refused at emission (`unsupported-ir`, `storage.ts:152`), and the header written for it
does not compile on its own — nine errors, the first at `generated/include/flight/preferences/storage.hpp:79`:

```
error: member reference type 'flight::Ref<StorageKeysResult>'
       (aka 'std::variant<std::shared_ptr<flight::types::reason_value_d00d25898e81ab85<...>>,
                          std::shared_ptr<flight::types::reason_value_70f2c7605208d474<...>>>')
       is not a pointer; did you mean to use '.'?
error: no member named 'reason' in 'std::variant<...>'
```

The emitter treats the reference-backed union as a record throughout: it writes `result->reason` and
`result->value` where the variant needs a discrimination step first. Runtime `pkg:compile --
preferences` reports the same failure for three headers.

## The reduction, and where it stops

Two attempts were made, and the second recorded the boundary rather than closing it:

- Lowering a plain interface method with the same generic alias **emits correctly** — the variant is
  constructed and narrowed properly. The alias alone is not the trigger.
- Lowering the **real** `packages/preferences/src/storage.ts` as a single module against
  `bindings/sdl-app.json` stops *earlier*, at `a presence test against null has no absence channel in
  the emitted C++ storage for property`, so it never reaches this defect.

So the ingredient is the capability's **external** method result: the reference-backed variant a bound
member call returns, not a value-domain variant. A faithful reduction has to lower the module with the
full generation environment (module graph, package targets, binding profile) rather than a one-module
fixture — that is where a reader should put it, and it is why no patch is carried.

## Carrying workaround

[`preferences-storage-outcome-dispatch`](../../overrides/manifest.json) replaces
`flight/preferences/storage.hpp` with a hand-written module: typed outcome factories and accessors over
the generated carriers, JSON conversion for the runtime's represented JSON domains, exact optional
failed-key states, signal ownership, namespace operations, and the migrations. Its declared `remaining`
records the one behavioral boundary it cannot close — a generic JSON read whose `Value` the target does
not represent reports `parse-failed`, where the source's unchecked `as Value` would have returned the
parsed value. The module is therefore **not** declared as `supplies`, and `@flighthq/preferences`
stays `partial` rather than shippable while the emitter refuses it.

## G2 cube-image value variant

At compiler `806d257` / Flight `a627849`, the WGPU scene-runtime declaration repair exposes the
same direct-member-on-variant failure at `flight/scene3d_wgpu/wgpu_environment_cube.hpp:50`.
This site uses an ordinary value variant from an array, rather than the externally returned
reference-backed outcome above; no bound capability result or generic outcome alias is involved.

`hasWgpuCubeFacePixels` loops over exactly six cube faces, rejecting a missing face or one whose
`kind` is neither `ImageTextureSourceKind` nor `BitmapTextureSourceKind`. The source reads
`cube.sources[face]`; emitted storage is an optional variant of references to `VoxelGrid`,
`TextureSource`, `ExternalTexture`, `Bitmap`, `CompressedImageResource`, `RenderTarget` and
`ImageResource`. After its presence test, the emitted condition reads `image.value().kind` twice
directly on that variant:

```text
no member named 'kind' in 'std::variant<...>'
```

The read must dispatch through the active stored arm and read that object's actual kind, retaining
the six-face order and short circuit rejection. A common-member read does not prove that every face
is a bitmap or image resource, so selecting either alternative unconditionally or copying into a
base `TextureSource` would not implement the source. The existing concrete variant and its owner
references already exist; adding a runtime `.kind` member to `std::variant` is not a runtime fix.

Only this owned WGPU header was measured for this follow-on family. It stays failed and linked here;
no claim is made about the analogous GL headers, no new C++ repair is carried, and the separately
refused upload/environment bodies remain unavailable.

## G2 FNT formatter reads dimensions from the same source-value family

At compiler `806d257` / Flight `a627849`, `bitmapfont-formats/bitmapFontFnt.ts:24` emits its
formatter without a refusal marker, but the first failing read is
`flight/bitmapfont_formats/bitmap_font_fnt.hpp:187`. The source is:

```ts
const primaryTexture = font.pages[0]?.texture;
const primaryImage = primaryTexture?.dimension === '2d' ? primaryTexture.source : null;
const scaleW = primaryImage !== null ? primaryImage.width : 0;
const scaleH = primaryImage !== null ? primaryImage.height : 0;
```

The emitted `primary_texture` is `std::optional<Ref<Texture2D>>`. Its source becomes an optional
variant of `Ref<VoxelGrid>`, `Ref<TextureSource>`, `Ref<ExternalTexture>`, `Ref<Bitmap>`,
`Ref<CompressedImageResource>`, `Ref<RenderTarget>` and `Ref<ImageResource>`. Each actual emitted
owner already declares `double width` and `double height`. After checking the optional, the
formatter incorrectly reads `primary_image.value().width` and `.height` on the variant itself.
This is a common-member projection failure on an ordinary value variant, like the cube-image
site above; it needs no outcome discriminant, host capability binding or new image fields.

The same generated inventory already shows the required operation in
`flight/texture/texture.hpp`'s `get_texture_width`: after its absence check, it returns
`std::visit([](const auto& value) { return value->width; }, source.value())`. The formatter needs
that active-owner read for each dimension, retaining its own zero fallback. The texture helper's
minus-one fallback is a different contract; replacing the formatter's captured source with repeated
texture-source lookups is not evidence of the same read order or source selection.

Read the first page's texture/source once, preserve absence, then read width followed by height
from the same retained owner. The base `TextureSource` alternative carries an open resolver kind;
dispatch must accept that arm as well as the concrete owners, without requiring a Bitmap or known
kind, copying dimensions into a base object, consulting an erased Any value, or fabricating a size.
Metrics, page count/order, glyph and kerning iteration, and the shared kerning scratch pair must
keep their source behavior.

This winning failure is in `formatBitmapFontFnt`, not in the parser. The module also contains the
FNT field/number readers and parser that builds the neutral record and resolves page atlases;
none was rewritten or declared repaired here. Four recorded header failures remain failing and
link this request: `_internal_index.hpp`, `bitmap_font_detect.hpp`, `bitmap_font_fnt.hpp` and
`contract.hpp` in `flight/bitmapfont_formats/`. Their diagnostics and prior include-guard repair
provenance are retained. No C++ input changed, so no unchanged native rerun or parser-functionality
claim was made.

## G2 grid track fraction still requires an active owner

At Flight `a627849` / compiler `806d257`, `getGridTrackSize` is refused as
`cpp-union-member-access-unguarded`: `property fraction on a C++ variant requires proven union member
access`. Its actual `GridLayoutTrack` carrier contains three reference alternatives: kind-only auto,
kind/size fixed, and fraction/kind. The source checks fixed and auto kinds before its final fraction
read; its accumulation loop also has a fraction read in the remaining branch. The diagnostic names
`fraction` but does not identify which expression, so neither is claimed as an independently measured
failure site.

Preserve the active referenced track through the source kind tests and read only the owner's member
that those tests prove. Do not materialize a merged owner, drop auto/fixed tracks, coerce weights or
supply a zero size. Fixed and auto contributions and gaps are subtracted before the remaining space is
clamped and distributed by fractional weight; the zero-total guard and source read order remain intact.
The grid declaration repairs leave this entire helper refused. Header availability does not implement
track sizing or prove native grid layout behavior.
