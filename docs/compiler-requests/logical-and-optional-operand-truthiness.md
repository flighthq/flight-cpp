# An optional boolean operand of `&&` must be tested for its value, not its presence

`a && b.flag`, where `flag?: boolean`, is true in TypeScript only when `flag` is `true`: `undefined` and `false`
are both falsy. The emitter lowers the operand to `std::optional<bool>` and wraps the WHOLE expression in
`flight::to_boolean(...)`. By then C++ has already converted the optional operand of the built-in `&&` through
its contextual `operator bool`, which answers presence -- so an explicit `false` reads as `true`. Nothing is
refused and nothing fails to compile; the condition is silently wrong. A standalone `if (b.flag)` is correct,
because there `to_boolean` receives the optional itself.

Measured at flight-compiler `2b687ea` with all 44 compiler source patches applied, one module per case.

## Minimal TypeScript (measured)

```ts
export interface Layer { below?: boolean }
export function walk(layers: Layer[], start: number): number {
  let i = start;
  while (i > 0 && layers[i].below) i--;
  return i;
}
export function walkExplicit(layers: Layer[], start: number): number {
  let i = start;
  while (i > 0 && layers[i].below === true) i--;
  return i;
}
export function alone(layer: Layer): boolean {
  if (layer.below) return true;
  return false;
}
```

```cpp
while (flight::to_boolean(((i > 0.0) && layers.element(i)->below))) {     // walk: presence, wrong
while (((i > 0.0) && (layers.element(i)->below == true))) {              // walkExplicit: correct
if (flight::to_boolean(layer->below)) {                                  // alone: correct
```

## Required C++ shape

Apply JavaScript truthiness to each operand, not to the C++ result:

```cpp
while (flight::to_boolean(i > 0.0) && flight::to_boolean(layers.element(i)->below)) {
```

The same holds for `||` and `!`, and for any operand whose carrier has a contextual `operator bool` that is not
its JavaScript truthiness (`std::optional<T>` for every `T`, `std::optional<double>` holding `0` or `NaN`, ...).

## Affected Flight modules

`@flighthq/flow` `flow.ts`, measured natively at a627849 on a stack whose top sets `updateBelow: false` and
`renderBelow: false`:

- `updateFlowStack`: `while (index > 0 && states[index].updateBelow)` ticks the state beneath (1 tick; JS 0);
- `getFlowStackVisibleStates`: `while (lowest > 0 && states[lowest].renderBelow)` reports 2 visible states (JS 1).

Other modules have not been audited for the shape; any `x && y.optionalFlag` is affected.

## Carrying workaround

`flow-optional-overlay-flags-explicit`, a package-local source patch, spells both tests `=== true`. That is the
same predicate over `boolean | undefined` and lowers to `optional<bool> == true`, which is false for both
`undefined` and `false`. `generated_flow_test` checks explicit `false` flags in both walks.
