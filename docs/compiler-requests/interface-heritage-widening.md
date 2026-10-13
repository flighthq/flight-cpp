# Emit declared interface heritage so derived references widen to their base

Reference-backed interfaces are emitted as independent flattened structs: each derived interface
repeats its bases' fields and inherits only `flight::ReferenceEnabled`. TypeScript widens a derived
object to any base interface implicitly, but in C++ `Ref<Derived>` has no conversion to `Ref<Base>`,
so every widening site fails to compile. Copying the derived object into a base struct would break
reference identity, so the only faithful lowering is real heritage.

## Minimal TypeScript

```ts
interface Adjustment { kind: string }
interface ColorMatrixAdjustment extends Adjustment { colorMatrix: readonly number[] }
interface TintAdjustment extends ColorMatrixAdjustment { kind: 'TintAdjustment' }

declare function createTintAdjustment(rgba: number): TintAdjustment;
declare function setAdjustments(list: Adjustment[]): void;
export function tint(rgba: number): void {
  setAdjustments([createTintAdjustment(rgba)]);
}
```

## Expected C++

```cpp
struct Adjustment : public flight::ReferenceEnabled { /* entity_runtime_key, kind */ };
struct ColorMatrixAdjustment : public Adjustment { flight::Array<double> color_matrix; };
struct TintAdjustment : public ColorMatrixAdjustment {};
```

`flight::Array<flight::Ref<Adjustment>>{create_tint_adjustment(rgba)}` then compiles, and the array
element is the same object `create_tint_adjustment` returned.

Observed at `2b687ea` (flight `a627849`, and identically at `7e2fc7d`) in
`flight/node/node_color_adjustment.hpp:71`:

```
no matching constructor for initialization of 'flight::Array<flight::Ref<flight::types::Adjustment>>'
```

## Affected modules

`packages/node/src/nodeColorAdjustment.ts` (`setNodeColorAdjustmentsTint`). The same flattening
was already repaired for the command family (`command-interface-heritage`). The opposite direction,
refining a base to a derived interface, is
[flattened-interface-refinement-identity](flattened-interface-refinement-identity.md).

The same missing heritage blocks `@flighthq/tilemap` at Flight `a627849` / flight-compiler
`2b687ea`. `setTilemapTileTint` constructs a `TintMaterialData`, which extends `MaterialData`, and
stores that exact object in the tilemap's `Array<MaterialData | null>`. The emitted owners are
independent (`TintMaterialData` and `MaterialData` each inherit only `ReferenceEnabled`), so the
derived reference cannot enter `Array<optional<Ref<MaterialData>>>` without splitting identity. The
tilemap override preserves the single object through the runtime's structural owner attachment until
declared C++ heritage makes the ordinary widening possible.

## Carrying workaround

Emission repairs `adjustments-color-matrix-interface-heritage` and `adjustments-tint-interface-heritage`
respell the two structs with their declared heritage, removing only the duplicated member declarations.
Every field keeps its name, type and declaration order. They expire when the emitter retains
declared interface heritage.
