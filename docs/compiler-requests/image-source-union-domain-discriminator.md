# Distinct source domains mapped to one carrier need their discriminator declared

Six element domains reach the same SDL carrier through the external binding profile, and the emitter
refuses the union because it sees one carrier with no discriminator. **The carrier has one** — the
binding simply cannot declare it, and that is the actionable half of this request.

Measured at `2b687ea`, flight `a627849`.

## Minimal TypeScript

`@flighthq/video`, `packages/video/src/videoResourceFrom.ts:32`:

```ts
export function loadVideoResourceFromUrl(
  hostVideo: Readonly<HostVideoCapability>,
  url: string,
  options?: Readonly<VideoResourceLoadOptions>,
  signal?: AbortSignal,
): Promise<VideoResource> {
  if (hostVideo.loadUrl === undefined) return Promise.reject(new Error('No video element backend available'));
  return hostVideo.loadUrl(url, options, signal).then((element) => createVideoResource(element, undefined, true));
}
```

`HostVideoCapability.loadUrl` resolves to `CanvasImageSource`, which is the union of
`HTMLImageElement | HTMLVideoElement | ImageBitmap | OffscreenCanvas | SVGImageElement | VideoFrame`.

## Observed

```
cpp emission failed for @flighthq/video/packages/video/src/videoResourceFrom.ts: C++ runtime contract
maps distinct union domains to one carrier without a discriminator or checked projection
(HTMLImageElement | HTMLVideoElement | ImageBitmap | OffscreenCanvas | SVGImageElement | VideoFrame ->
flight::host_sdl::ImageSource); give the domains distinguishable key, element, or member types so each
reaches a carrier of its own -- a union of two declarations that differ only in name is one carrier
whatever the emitter does -- or provide that runtime contract
```

The module is refused for `loadVideoResourceFromUrl` (line 32) and the emitter omits that declaration
alone: its sibling `loadVideoResourceFromBlob` (line 11) is emitted at the current binding profiles, and
`loadVideoResourceFromUrls` (line 37) with it. Three of the package's five headers still fail, now because
both retained callers call the omitted function — that second-order failure is filed separately as
[`retained-declaration-calls-omitted-callee.md`](retained-declaration-calls-omitted-callee.md), and this
refusal is what would have to lift first.

## What makes this actionable: the discriminator already exists

`include/flight/host_sdl/image.hpp:19` — the runtime's own carrier, with its own note on why:

```cpp
// Native image sources share one decoded RGBA representation across GL and WebGPU. The kind retains
// the provider's source domain for future compiler-lowered external type tests without making either
// graphics API own image decoding or video-frame acquisition.
enum class ImageSourceKind {
  decoded_rgba,
  image_element,
  video_element,
  image_bitmap,
  offscreen_canvas,
  svg_image_element,
  video_frame,
};
```

`ImageSource` carries that kind. So the refusal's premise — "without a discriminator or checked
projection" — is not true of this carrier: it is exactly the projection the message asks for, and the
runtime comment says it was put there for this.

## Where the gap actually is

`bindings/sdl-image.json` maps nine source names (`CanvasImageSource`, `HTMLImageElement`,
`HTMLVideoElement`, `ImageBitmap`, `OffscreenCanvas`, `SVGImageElement`, `TexImageSource`,
`VideoFrame`, `GPUCopyExternalImageSource`) to `flight::host_sdl::ImageSource`. The **binding schema
has no field to say which kind each name maps onto**: the entries carry `headers`, `nullability`,
`ownership`, `sourceName`, `space`, `targetName` and `weakKeyPolicyTargetName`, and searching the
compiler's contracts finds no discriminator, type-test or kind field. So a target that *has* a
discriminator cannot tell the emitter about it, and the emitter's only honest answer is to refuse a
union it cannot project.

Two shapes would close it, and either is upstream's call: a per-binding discriminator the emitter emits
an external type test against, or the emitter accepting the union where the named runtime carrier
declares a kind covering every source domain.

## Affected modules

`@flighthq/video` — `videoResourceFrom.ts` (51 lines, three exported functions) and three headers
behind it. The same profile family is what `render-wgpu`'s `HTMLImageElement[value]` refusals come
from (AGENTS.md names them as `copyExternalImageToTexture` sources), so the reach is wider than video.

## Carrying workaround

No body workaround, and the reason is not size — the refused module is 51 lines and a hand supply is available. It is
that the defect is in the *profile mapping*: a hand-written module would reproduce the mapping the
profile should express, which is the "fork nobody chose" the repository's own rules are written to
prevent. If the mapping is wanted in the profile, this belongs upstream; if a supply is wanted
regardless, that decision should be explicit.

At the G2 `806d257` follow-up, `video-declare-refused-url-loader` declares only the missing
`load_video_resource_from_url` boundary, using the two retained callers' exact task/argument
carriers. The refusal marker and unavailable body remain. Its unused-header and expected
negative-link checks are tracked in
[retained callers](retained-declaration-calls-omitted-callee.md); header compilation is not
evidence of a working URL load or of a supplied host provider. The existing generated host method
already carries `Task<HostImageSource>`, so this does not need another runtime handle, fake decoder,
global callback change or narrower image/video-only source domain.

## G2 WGPU texture upload reaches the same copy after the writeTexture binding

At `806d257` / Flight `a627849`, `render_wgpu/wgpu_texture_upload.hpp` first failed on
`queue.writeTexture` because the host's forwarding binding could not deduce through the two
dictionary literals; naming `WgpuTexelCopyTextureInfo` and `WgpuTexelCopyBufferLayout` in a concrete
queue overload resolved that call without changing the unavailable host operation. The header's next
failure is this request: `upload_wgpu_texture_element` retains
`try_copy_wgpu_external_image_to_texture`, which `wgpuExternalImageSource.ts` refuses because the
runtime external symbol plan is missing the `HTMLImageElement[value]` / `HTMLVideoElement[value]`
bindings. The host side confirms the second half: `flight/host_sdl/wgpu.hpp` declares no
`copyExternalImageToTexture` operation at all (only the adoption surface and forwarding queue
commands), `src/host_sdl/wgpu.cpp` implements no queue command, and every operation — including the
newly named `writeTexture` — reports the SDL WGPU profile unavailable. A declaration repair or a host
stub here would either reproduce the refused profile mapping in a hand-written module or invent the
upload and its pixels, so neither is carried. Closing this site needs both a profile mapping the
emitter can name and a real provider operation; the row stays failed on this request.
