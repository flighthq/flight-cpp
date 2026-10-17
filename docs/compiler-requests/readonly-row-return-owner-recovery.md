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

**No workaround is carried.** A bounded fix was attempted and withdrawn (2026-10-17), and the reasons
are worth keeping:

- An emission repair may only correct text with no operation of its own; `structural_ref_cast` is an
  operation, so the repair mechanism cannot carry this. The attempted entry
  `render-inherited-color-scale-bias-parent-owner-recovery` was removed rather than kept under a
  different name.
- A header override would reproduce the emitted header plus the cast, but the header still fails on the
  reference-comparison family at `:94` and on omitted helpers at `:147`-`:149` and `:198`, so the
  override would not compile. An override that does not compile is `incomplete` and may not name
  `supplies` — it would shadow the generated file to supply nothing.
- A source patch has no clearly equivalent expression: the two source branches are structurally
  identical, and the emitter emits the recovery for one and not the other, so no rewrite of
  `return parent` chooses the lowering rather than guessing at it.
- The recovery's boundary was measured on this exact schema. `structural_ref_cast<Ref<ColorScaleBias>>`
  returns the same object for an owner-backed `RowReadonly<RowOf<Ref<ColorScaleBias>>>` view, and a
  fresh `make_structural_ref` row of that schema is owner-backed too, so it also round-trips. A bare
  row owner with no native object throws `bad_cast`; no emitted constructor of this schema produces
  one, but the declared parameter type does not itself promise an owner, so the compiler fix still has
  to make the owner decision where the nominal owner is known rather than leaving it to the caller's
  construction path.

The cluster therefore stays failing at `enable_color_adjustments.hpp:92`.
