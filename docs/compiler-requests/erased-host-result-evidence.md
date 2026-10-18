# Results that name an unresolved external host type have no type evidence

The lowering's program does not resolve external host types (WebGPU), so the checker erases every result
naming one to `any`. Three common shapes then have no evidence for the C++ backend:

```ts
export interface Screen { context: Ctx; presentationView: View | null; acquireCaptureTexture: ((t: Screen) => Tex) | null }
export function bind(t: Screen): void {
  const texture = t.acquireCaptureTexture?.(t) ?? t.context.getCurrentTexture();  // texture: Tex | any
  t.presentationView = texture.createView();                                       // createView(): any
}
```

- The optional call lowers to `std::optional<flight::Any>`, although the member DECLARES `(t: Screen) => Tex`.
- `getCurrentTexture()` and `createView()` have no result type, so they cannot enter `std::optional<View>`.
- The local is typed `Tex | any`, and its storage cannot be planned.

## Expected

Evidence the source or the host profile already declares, used ONLY where the checker erased the result:
the member's declared callable return; the binding profile's declared `callResultType`, read back as the one
source type bound to it; and, for a local typed `T | any`, its initializer's complete evidence. A result the
checker recorded is never replaced, and a declared result with no unique source type stays unproven.

## Carrying workaround

`source-patches/compiler-erased-host-result-evidence.patch`, plus `bindings/sdl-wgpu.json` declaring
`GPUCanvasContext.getCurrentTexture` (callResultType `flight::host_sdl::WgpuTexture`), with a matching
unavailable host method in `flight/host_sdl/wgpu.hpp`. It expires when the lowering's program resolves the
external host types.
