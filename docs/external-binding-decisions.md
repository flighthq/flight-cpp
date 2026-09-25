# Which external symbols flight-cpp declares, and which it refuses on purpose

Every compiler corpus check reports symbols the emitted SDK reaches that no binding profile
declares. Most of those are work. Some are not, and without a written decision they come back in
every worklist and someone eventually "fixes" one by declaring it.

This is the record of the ones we refuse and why. Measured against flight `7e2fc7d` +
flight-compiler `839d91e`, SDL profile.

## Closed by declaring, for contrast

`DOMException`, `AudioContext`, and the eleven Intl carriers (`LocalesArgument`, the five
`*Options` bags, and `Intl.Segmenter`/`Segments`/`SegmentData`). Thirteen requirements, gone from
the ledger. `AudioContext` is the shape to copy: the surface Flight actually reaches is one method,
so that is what the binding declares.

## Refused: the module is the WEB backend

Seven modules. They are not incomplete native code, they are the browser implementation, and
Flight has a web host that provides them. A native SDL host declaring the DOM would be claiming a
document it does not have.

| module | reaches | why refusing is correct |
| --- | --- | --- |
| `scene2d-dom/domReconcile.ts` | `Node`, `ChildNode` | DOM tree reconciliation. There is no DOM. |
| `scene2d-dom/domRichText.ts` | `Text` | DOM text nodes. |
| `scene2d-dom/domSprite.ts` | `HTMLImageElement[value]`, `HTMLVideoElement[value]` | DOM sprite backing. |
| `scene2d-dom/domSvgFilter.ts` | `SVGSVGElement`, `SVGDefsElement`, `SVGFilterElement` | SVG filter graph. |
| `textshaper-canvas/canvasTextShaper.ts` | `OffscreenCanvas[value]`, `OffscreenCanvasRenderingContext2D` | Canvas-measured shaping. The native answer is a freetype/harfbuzz shaper, a different module. |
| `render-wgpu/wgpuHost.ts` | `WebGLRenderingContext`, `ImageBitmapRenderingContext` | `canvas.getContext('webgpu')`. The overload set drags in every context type even though only one is used. |
| `image/imageResourceFrom.ts` | `location[value]` | `new URL(url, location.href).origin === location.origin` -- a same-origin check. |

The last one deserves its own sentence, because it is the most tempting to declare. A fake
`location` would not fail; it would return a WRONG same-origin answer, silently, for every image
URL. There is no document origin on a native host, so the honest state is that the check does not
apply -- not that it answers.

## Refused: browser-branch guards that must stay false

`HTMLImageElement[value]`, `HTMLVideoElement[value]`, `ImageBitmap[value]`, `OffscreenCanvas[value]`,
`VideoFrame[value]` in `render-wgpu/wgpuDraw.ts` and `wgpuExternalImageSource.ts`.

Every use is the same shape:

```ts
if (typeof HTMLImageElement !== 'undefined' && source instanceof HTMLImageElement) { ... }
```

These globals genuinely do not exist on a native host, so `typeof X !== 'undefined'` is honestly
false and the branch is dead. Worse, all five already map onto ONE C++ type
(`flight::host_sdl::ImageSource`), so an `instanceof` could not tell them apart even if we bound
them -- it would have to pick an answer, and any answer it picked would be wrong for four of the
five.

**Compiler ask**: lower `typeof X !== 'undefined'` against an ABSENT binding as a compile-time
`false` and prune the branch, rather than refusing the module for a missing symbol. That turns
seven refusals into emitted code with the dead branches removed, and it needs nothing from this
repository.

## Refused: test-only modules that should not be in a runtime corpus

Four modules, and one of the symbols settles it.

| module | reaches |
| --- | --- |
| `render-gl/glTestHelper.ts` | `PropertyDescriptorMap`, **`vi[value]`** |
| `scene2d-gl/glTestHelper.ts` | `PropertyDescriptorMap` |
| `effects-gl/glShaderTestHelper.ts` | `Function[value]` |
| `render-wgpu/wgpuTestHelper.ts` | `Navigator`, `PropertyDescriptor`, `PropertyDescriptorMap` |

`vi` is **vitest**. `wgpuTestHelper.ts` reaches `Navigator` to `defineProperty` a mock GPU onto
`globalThis.navigator` when a worker has none. These are test scaffolding, and the reflection
surface (`PropertyDescriptor`, `PropertyDescriptorMap`, `Function`) is there to build mocks.

Declaring `vi` in a runtime binding profile would be declaring a test runner as part of the
runtime. We will not.

**Compiler ask**: exclude `*TestHelper.ts` and anything importing `vitest` from the corpus check,
or classify them under their own policy class. Four of the thirteen remaining findings are this,
and they inflate every count.

## The rule this leaves

Declare a symbol when a native host can answer it honestly. Refuse it when the honest answer is
that the capability is absent -- and say so here, rather than binding something that returns a
plausible wrong answer. A missing binding is a loud failure at emission. A fake binding is a quiet
wrong result at run time, somewhere else, later.
