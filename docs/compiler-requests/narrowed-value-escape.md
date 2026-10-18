# A value narrowed by a user predicate escapes as the derived nominal owner

A user type predicate narrows a base interface to a derived one, and the narrowed value then flows into
parameters, fields and callbacks typed as the derived interface. TypeScript admits there ANY object that
declares the derived members — not only one allocated as the derived type. The C++ backend spells every
such site `flight::Ref<Derived>`, which only an allocation of `Derived` (or a subclass) can satisfy. The
narrowed value has no faithful conversion into that slot, so the escape is refused.

## Minimal TypeScript

```ts
export interface Target { context: Ctx | null; width: number }
export interface Screen extends Target {
  readonly context: Ctx;
  presentationView: number | null;
  encodeCapture: ((target: Screen, n: number) => void) | null;
}
export interface Runtime { frame: Screen | null }
export function isScreen(t: Readonly<Target>): t is Screen { return t.context !== null; }
export function bindScreen(t: Screen): number {
  if (t.presentationView === null) t.presentationView = 7;
  return t.presentationView;
}
export function bind(r: Runtime, t: Target): number {
  if (isScreen(t)) { const v = bindScreen(t); r.frame = t as Screen; return v; }
  return 0;
}
```

A caller may legally pass its own `{ context, width, presentationView: null, encodeCapture: null }`
literal typed by an interface it declared itself; `bind` must write `presentationView` on THAT object,
store THAT object in `r.frame`, and hand THAT object to `encodeCapture`. Under `tsc --strict` this runs and
observes the write on the caller's object; the C++ equivalent is `CallerScreen` in
`tests/nominal_or_structural_view_test.cpp`.

The pinned backend refuses `t as Screen` with `cpp-reference-assertion-without-heritage`, and has no
spelling for passing `t` to `bindScreen`.

## Expected C++

The sites the narrowed value reaches carry it as the same object, with member access by name falling back
from the derived type's own cell:

```cpp
inline double bind_screen(flight::NominalOrStructuralView<Screen, Target> t) {
  if (!t.template read_optional<flight::RowKey<"presentationView">, std::optional<double>,
                                &Screen::presentation_view>().has_value()) {
    t.template write<flight::RowKey<"presentationView">, std::optional<double>,
                     &Screen::presentation_view>(std::optional<double>{7.0});
  }
  return t.template read_optional<...>().value();
}
// bind: bind_screen(flight::NominalOrStructuralView<Screen, Target>::of(t));
```

A factory-made `Screen` converts implicitly and reads its fields directly, with no allocation; a look-alike
reads and writes its own members through the structural row machinery, keyed by object identity.

## What upstream should decide

Which sites carry a structural value is not something this repository should infer, and it does not:
`structural-value-carriers.json` lists them, and the carried patch applies the carrier only there. The
general fix is for the backend to derive that site set itself — the closure of parameters, fields and
callback parameters a predicate-narrowed value flows into — and to choose the carrier spelling where a
narrowed value can reach and the nominal owner everywhere else.

## Affected modules

`@flighthq/render-wgpu` `bindWgpuRenderPassTarget` (and so the render-pass lifecycle functions behind it),
whose narrowed `WgpuScreenRenderTarget` reaches `bindWgpuScreenRenderTarget`, `syncWgpuScreenRenderTargetExtent`,
`ensureWgpuScreenRenderTargetDepthStencil`, `endWgpuScreenRenderTargetFrame`, the four capture and antialias
callback implementations, `WgpuRenderStateRuntime.frameScreenTarget` and `BorrowerFrameState.frameScreenTarget`.
GL's `GlRenderPass.target` path has the same shape and is not carried.

## Carrying workaround

`source-patches/compiler-structural-value-carrier.patch` adds the backend option `structuralValueCarriers`,
fed from `structural-value-carriers.json` by `scripts/sdkGeneration.mjs`. Generation refuses a declared site
the pinned source no longer has. The runtime half is `flight/nominal_or_structural_view.hpp`, whose tests
cover the nominal fast path, a look-alike's live writes and identity, the non-retention of a read, and a
member of an unreachable type failing at the access rather than reading as absent.
