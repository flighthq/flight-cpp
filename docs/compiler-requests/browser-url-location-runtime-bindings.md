# Browser URL and location values need runtime bindings

The image resource helpers use the standard browser URL surface to compare origins and to turn a
`Blob` into a temporary loadable URL. The C++ runtime profile binds `Blob`, but it declares neither the
`location` value nor the `URL` constructor and its static object-URL operations. The compiler therefore
refuses the whole module during external-symbol planning:

```text
cpp emission failed for .../imageResourceFrom.ts: runtime external symbol binding plan is incomplete
(missing: location[value])
```

## Minimal TypeScript

```ts
export function sameOrigin(value: string): boolean {
  return new URL(value, location.href).origin === location.origin;
}

export function borrowBlobUrl(blob: Blob): string {
  return URL.createObjectURL(blob);
}
```

The real module also pairs `URL.createObjectURL(blob)` with `URL.revokeObjectURL(url)` in a `finally`
block, so the temporary URL's lifetime is part of the required behavior.

## Expected C++ contract

The runtime profile must either bind the browser URL/location surface, including origin resolution and
object-URL lifetime, or expose an equivalent host capability that the source can call explicitly. The
compiler must retain the `finally` cleanup around the asynchronous image load. A hard-coded origin,
unconditionally returning true, or treating a blob URL as the blob's bytes changes observable behavior.

## Affected module

`@flighthq/image` `imageResourceFrom.ts`. The refusal also hides the portable
`createImageResourceFromBitmap` and `loadImageResourceFromUrl` declarations because external-symbol
planning stops the module before per-function emission.

`@flighthq/textureatlas` `textureAtlasFrom.ts` inherits that refusal after its own local texture
options mismatch is removed: its four load helpers call the absent image declarations. URL loading
itself and the base64 data-URL wrapper can delegate directly to the represented
`HostImageCapability.loadImageFromUrl` callable. Blob and byte loading cannot be reconstructed
faithfully in textureatlas, because their source contract requires object-URL creation, revocation
in `finally`, and the same asynchronous settlement described above.

## Carrying workaround

None for the refused image module as a whole. Rewriting it around an SDL-specific URL implementation
would invert the host boundary, while an override has no declared runtime operation with which to
reproduce URL resolution or blob-URL creation and revocation. A dependent package may faithfully
delegate its URL and base64 entry points to the already represented host capability, but blob and
byte entry points remain request-backed omissions until this binding exists.
