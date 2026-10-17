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
