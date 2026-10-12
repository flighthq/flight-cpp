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

At flight-compiler `2b687ea` (with this repository's compiler source patches applied), `capture` emits:

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
