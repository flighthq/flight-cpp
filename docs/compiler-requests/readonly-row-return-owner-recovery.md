# A readonly row returned as its declared nominal owner keeps the owner recovery

`resolveInheritedColorScaleBias` returns `ColorScaleBias | null`; the `local === null` branch returns
the `Readonly<ColorScaleBias> | null` parameter directly. The emitter lowers the sibling `return local`
branch through `flight::structural_ref_cast<flight::Ref<ColorScaleBias>>(...)`, but emits the
`return parent` branch as a bare `std::optional<...>{parent.value()}`, which has no conversion from the
readonly structural view. The two branches carry the same declared type and the same source value
shape, so one of them is mis-lowered.

## Minimal TypeScript

```ts
interface Bias { kind: string }
declare function createBias(): Bias;
export function resolve(
  previous: Bias | null,
  parent: Readonly<Bias> | null,
  local: Readonly<Bias> | null,
): Bias | null {
  if (parent === null) return local;
  if (local === null) return parent;
  return createBias();
}
```

## Expected C++

The same recovery the union-wrapped `return local` already gets:

```cpp
if (!local.has_value()) {
  return std::optional<flight::Ref<flight::types::Bias>>{
      flight::structural_ref_cast<flight::Ref<flight::types::Bias>>(parent.value())};
}
```

`structural_ref_cast` recovers the exact shared owner and throws `bad_cast` when the view is not that
object, so reference identity, control block and lifetime are preserved. A copied owner, a writable
row, or an optional reinterpretation would not be equivalent.

## Affected modules

Measured at flight `a627849` / flight-compiler `806d257`, `@flighthq/render` `enableColorAdjustments.ts`,
`resolveInheritedColorScaleBias`:

```
flight/render/enable_color_adjustments.hpp:92:12: error: no matching constructor for initialization of
  'std::optional<flight::Ref<flight::types::ColorScaleBias>>'
```

The four headers are `render/enable_color_adjustments.hpp`, `render/_internal_index.hpp` and
`scene2d_gl/gl_color_adjustment_material_feature.hpp` (the cluster `other-b860ec87`). The same header
keeps four further families after this one — reference comparison between the nominal optional and the
readonly row (`:94`, see
[readonly structural nominal identity comparison](readonly-structural-nominal-identity-comparison.md)),
a readonly-to-writable projection at the `concat_color_scale_bias` call (`:95`), and refused callees and
scratch locals (`:147`-`:149`, `:198`) — so this repair moves the first diagnostic and does not make
the header pass.

The carried workaround is emission repair
`render-inherited-color-scale-bias-parent-owner-recovery`, which adds exactly the cast the emitter
already emits one branch up. It expires when a readonly structural row returned as its declared
nominal owner is emitted with the owner recovery.
