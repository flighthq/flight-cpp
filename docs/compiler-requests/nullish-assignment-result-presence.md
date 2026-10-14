# The result of `??=` is present

`a ??= b` evaluates to `a` when `a` is present and to `b` otherwise, so its value is never null or undefined
when `b` is not. The emitter does not record that. A local bound from the expression keeps the optional carrier
of the left-hand side, and the first member read through it is refused for lack of a presence proof.

## Minimal TypeScript

```ts
interface Scene { cleanups?: (() => void)[] | null }

export function addCleanup(scene: Scene, cleanup: () => void): void {
  const cleanups = (scene.cleanups ??= []);
  if (!cleanups.includes(cleanup)) cleanups.push(cleanup);
}
```

## Refusal (measured)

Flight `a627849`, compiler `2b687ea` with this repository's compiler patches applied. `@flighthq/scene3d-gl`
`glScene3DRuntime.ts` `addGlScene3DResourceCleanup` (pin line 21):

```
member includes on optional C++ storage requires proven present payload: nothing proved the payload present, so
the read would unpack storage that may be holding absence. Guard the receiver, read it through an optional chain,
or narrow it into a present local first; the compiler will not call value() without that proof.
```

## Expected C++

The assignment establishes the cell when it is absent, then yields the cell's present value:

```cpp
if (!scene->cleanups.has_value()) scene->cleanups = flight::Array<std::function<void()>>{};
auto cleanups = scene->cleanups.value();  // present by construction of the assignment
```

The local must be the stored array (reference identity), not a copy. Pushing through `cleanups` has to be
visible through `scene->cleanups`, as it is in JavaScript.

## Affected modules

- `@flighthq/scene3d-gl` `glScene3DRuntime.ts` `addGlScene3DResourceCleanup`. Refused at the live base behind
  the pending [`scene3d-gl-erased-upload-cache`](scene3d-gl-erased-upload-cache.md) patch, this becomes the
  module's first refusal once that patch is applied. A scratch probe rewrote only this line into the guard form
  below; `glScene3DRuntime` and its dependants then emit, and scene3d-gl refusal rows fall from 60 to 47.
- `@flighthq/scene2d-canvas` `canvasTextureResolver.ts`, `registry ??= new Map()`, carried by the
  `canvas-texture-resolver-present-registry` source patch.
- `@flighthq/scene2d-canvas` `bitmapElementCache ??= new WeakMap()`, carried by
  `scene2d-canvas-bitmap-host-image-cache`.

## Carrying workaround

Each carrying patch rewrites the expression into an explicit guard that already emits:

```ts
let cleanups = scene.cleanups;
if (cleanups == null) {
  cleanups = [];
  scene.cleanups = cleanups;
}
```

This is equivalent for a plain data slot: one read, one conditional write of the same new object, and the same
object returned. No patch is carried for the scene3d-gl site yet. This request expires when the emitter treats a
`??=` result as present whenever its right-hand side is.
