# Video frame advance asserts an image source that can lack a host handle

At Flight `a62784923f3be814463286c9fb28edfe6ab15789`, the public
`advanceVideoTexture(hostVideo: Readonly<HostVideoCapability>, texture: TextureLike)`
accepts a 2D texture whose source is a `Bitmap`. Its private `getVideoImage` checks
only `dimension === '2d'`, then asserts `texture.source as ImageResource | null`.
The assertion preserves that Bitmap object and adds no runtime guard or member.

A minimal witness, with `bitmap` an actual `Bitmap` initialized to width 2, height 3
and version 17, is:

```ts
const texture = createTexture({ source: bitmap, version: 17 });
const received: unknown[] = [];
const host: HostVideoCapability = {
  canPlayType: () => true,
  getWidth: element => { received.push(element); return 23; },
  getHeight: element => { received.push(element); return 11; },
};
advanceVideoTexture(host, texture);
```

The exact pinned implementation produces `received = [undefined, undefined]`.
It writes width 23, height 11 and version 18 on the **same Bitmap** and mirrors
version 18 on the texture. Optional callback invocation does not skip an absent
argument: it tests whether the callback exists. With both callbacks absent, the
Bitmap dimensions become zero and its revision still advances.

This is source-emitted `undefined`, despite the host declaration's promise.
`packages/types/src/HostVideo.ts` declares:

```ts
getHeight?(element: HostImageSource): number;
getWidth?(element: HostImageSource): number;
```

`HostImageSource` is `CanvasImageSource`, without `undefined`. The SDL binding
`bindings/sdl-image.json` maps it with `nullability: "non-null"` to
`flight::host_sdl::ImageSource`; generated host callbacks are optional
`std::function<double(HostImageSource)>`. An absent callback is representable;
an actual `undefined` argument to a present callback is not that native domain.
A default-constructed handle would fabricate a source, and a source-kind guard
would suppress calls and writes the pinned implementation performs.

The smallest implementation boundary to resolve is this helper's asserted image
view plus its invocation argument: preserve the original source owner for width,
height and version writes, and preserve a missing host-source property as actual
`undefined` when invoking a present callback. A compiler/runtime representation
for that crossing must make both operations explicit. This request does not
propose widening the global host callback declarations or changing accepted
inputs; an upstream narrowing or validation would be a separate source-contract
change. No such redesign is carried here.

`videoTextureHelpersOracle.mjs` parses and executes the exact pinned declarations,
checks the host parameter declarations and alias, and retains this witness beside
the eight representable readonly helper regressions. The readonly-host
`advanceVideoTexture` carrier remains explicitly unsupplied and the video override
remains partial. Its native convenience overload is not evidence of this surface:
it filters non-ImageResource variants and does not reproduce the witness.
