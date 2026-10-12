# An inferred `any` type argument leaks its parameter name into a readonly conversion

When a generic function takes `Readonly<Node<Traits>>` and is called with a `Node<any>` value, the
emitter infers `Traits = any`. It then omits the template argument at the call, and spells the
structural conversion of each argument with the unsubstituted parameter name `Traits`. That name is
undeclared in the non-generic caller, so the header does not compile.

## Minimal TypeScript

```ts
export interface Node<Traits extends object = object> {
  kind: string;
  traits: Traits;
}
export type NodeAny = Node<any>;

export function childIndex<Traits extends object>(
  source: Readonly<Node<Traits>>,
  child: Readonly<Node<Traits>>,
): number {
  return source === child ? 0 : -1;
}

export function capture(parent: NodeAny, child: NodeAny): number {
  return childIndex(parent, child);
}
```

At flight-compiler `2b687ea` with all 38 of this repository's compiler source patches applied (re-measured
after `compiler-readonly-view-argument-substitution` landed), `capture` emits:

```cpp
inline double capture(NodeAny parent, NodeAny child) {
  return child_index(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<
      flight::RowOf<flight::Ref<Node<Traits>>>>>>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<NodeAny>>>(parent)), /* child likewise */);
}
```

The required shape substitutes the inferred argument, exactly as the explicit form below already
emits: `child_index<flight::Any>(...)`, converting to `Node<flight::Any>`.

## Pinned boundary

| call | result |
| --- | --- |
| `childIndex(parent, child)` with `NodeAny` arguments, readonly parameters | leaks `Node<Traits>`; no template argument |
| the same with `NodeOf<Traits> = Node<Traits> & Traits` parameters | leaks both `Node<Traits>` and `RowOf<Traits>` |
| `childIndex<any>(parent, child)` | emits `child_index<flight::Any>` with `Node<flight::Any>` |
| `Node<Sprite>` arguments, inferred | emits `child_index<flight::Ref<Sprite>>` with the substituted row |
| `NodeAny` arguments, writable parameters (no conversion) | emits `child_index(parent, child)` |

The controls isolate the defect to the conjunction of an inferred `any` argument and a readonly
structural conversion. A concrete inferred argument is substituted. An explicit `any` is substituted.
A call that needs no conversion does not spell the parameter at all.

A concrete inferred argument is not always safe, though: when every argument reaches `Traits` only
through interface heritage (`Node2D`, `Sprite`), a checker-inferred concrete `Traits` also leaks, in the
readonly and writable forms alike. That trigger, with its own measured controls, is recorded in
[heritage-argument-generic-deduction](heritage-argument-generic-deduction.md); a fix should be checked
against both reductions.

## Affected Flight modules

`@flighthq/command` `command.ts` at flight `a627849`: `initializeRemoveNodeChildCommand` and
`initializeReorderNodeChildCommand` call `getNodeChildIndex(parent, child)` with `NodeAny` arguments.
The emitted `command.hpp` fails with `use of undeclared identifier 'Traits'` at both calls.

## Workaround carried

The `command-undeclared-node-helpers` override in `overrides/manifest.json` spells the two conversions
with `flight::Any`. That is the substitution the explicit `<any>` call already emits, and it makes no
other change to the calls. A source patch adding `<any>` at the two call sites would be equivalent. It
was not used because the override already exists to supply the refused `readNodeProperty`, and the
two edits expire together with it.

## Related leaks and how they differ

Three requests record a type-parameter name spelled where it is undeclared. They share the emission step
named as the residual boundary in
[compiler-generic-signal-row-projection](compiler-generic-signal-row-projection.md): when deduction leaves
a parameter undetermined, the conversion target is written with the parameter's name instead of being
refused. They do not share a cause. Each is a different reason deduction leaves the parameter undetermined,
so each needs its own deduction fix, and they are kept as separate requests.

Measured together in one scratch compiler at `2b687ea` with all 38 compiler source patches, against the same
`Node<Traits>` fixture (`interface Node2D extends Node<Node2DTraits>`, `interface Sprite extends Node2D`):

| argument evidence | call | result | request |
| --- | --- | --- | --- |
| `Readonly<Signal<T>>` parameter, `Signal<() => void>` argument | inferred | substituted (fixed by `compiler-readonly-view-argument-substitution`) | [signal row projection](compiler-generic-signal-row-projection.md) |
| `NodeAny` (`Node<any>`) into `Readonly<Node<Traits>>` | inferred | **leaks `Node<Traits>`** | this request |
| `NodeAny` into `Readonly<Node<Traits>>` | `<any>` | `child_index<flight::Any>` | control |
| `Node<Node2DTraits>` into `Readonly<Node<Traits>>` | inferred | `child_index<flight::Ref<Node2DTraits>>` | control |
| `Node2D`, `Sprite` (interfaces extending `Node<Node2DTraits>`) into `Readonly<Node<Traits>>` | inferred | **leaks `Node<Traits>`** | [heritage argument deduction](heritage-argument-generic-deduction.md) |
| `Node2D`, `Sprite` into `Readonly<Node<Traits>>` | `<Node2DTraits>` | `child_index<flight::Ref<Node2DTraits>>`, compiles | control |
| `Node2D`, `Sprite` into `NodeOf<Traits>` (`Node<Traits> & Traits`, writable) | inferred | **leaks `NodeOf<Traits>`**, the movieclip shape | [heritage argument deduction](heritage-argument-generic-deduction.md) |
| an unrelated owner (`Emitter<T>`) into `Readonly<Signal<T>>` | inferred | leaks, correctly undetermined | residual in [signal row projection](compiler-generic-signal-row-projection.md) |

The `any` row and the heritage row are distinguished by their controls. An explicit `<any>` fixes only the
first, and a literal `Node<Node2DTraits>` argument works while an interface extending it does not. So:
- teaching deduction to bind `any` would not fix the heritage leak;
- walking interface heritage would not fix the `any` leak;
- the residual change (refuse an undetermined parameter at emission) would turn both into refusals but
  make neither compile.
