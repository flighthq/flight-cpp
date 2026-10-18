# An interface that narrows a base member only by absence loses its native heritage

`WgpuScreenRenderTarget` redeclares `WgpuRenderTarget`'s `context: GPUCanvasContext | null` as
`readonly context: GPUCanvasContext`. The carried native-heritage rule requires identical member storage, so
the derived interface is emitted as an unrelated flattened struct, and no derived value widens to its base.

## Minimal TypeScript

```ts
export interface Target { context: Ctx | null; width: number }
export interface Screen extends Target { readonly context: Ctx }
export function widthOf(t: Target): number { return t.width; }
export function screenWidth(s: Screen): number { return widthOf(s); }
```

## Expected C++

One cell represents both declarations; the derived struct inherits it and reads the present payload:

```cpp
struct Screen : public Target { using flight_native_base = Target; };
// s->context.value() where Screen declares the member present
```

## Affected modules

The WGPU and GL render-target subtypes (`flight::types::WgpuScreenRenderTarget`,
`WgpuTextureRenderTarget`), and through them `bindWgpuRenderPassTarget`.

## Carrying workaround

`source-patches/compiler-interface-absence-refinement-heritage.patch`: a derived member that narrows its
base member by absence only (the same present payload with fewer absent kinds) keeps native heritage, and
reads of it through the derived type take the present payload. Every value the narrower member admits is one
the base admits, so no shadow member, copy or second storage exists. It expires when upstream preserves
heritage for absence-only refinements.
