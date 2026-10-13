# Generic owners behind `Ref` must remain deducible at exported calls

An exported generic function takes a generated reference-backed owner whose own type argument is the
function type parameter. The emitted parameter spells that owner through `flight::Ref<T>`, whose
conditional `reference_shape` expansion is a non-deduced C++ context. Ordinary calls therefore cannot
recover the generic argument that TypeScript infers from the owner.

## Minimal TypeScript

```ts
interface Box<T> { readonly values: readonly T[] }
export function first<T>(box: Box<T>): T | undefined {
  return box.values[0]
}
```

For a reference-backed `Box<T>`, the exported C++ call `first(box)` must infer `T`. A signature shaped
as `first(flight::Ref<Box<T>> box)` cannot do so because `Ref` expands through a dependent conditional
alias. The emitter can expose the deducible concrete owner (`std::shared_ptr<Box<T>>` for a generated
reference-enabled interface), add a deducing wrapper, or otherwise preserve source inference without
weakening the stored owner.

## Affected Flight module

At Flight `a627849` / flight-compiler `2b687ea`, every generic operation exported by
`packages/selection/src/selectionState.ts` has a parameter shaped as
`flight::Ref<SelectionState<NodeType>>`. A direct native call such as
`getSelectedNodes(state)` fails with:

```
candidate template ignored: couldn't infer template argument 'NodeType'
```

The same boundary reaches `pointerSelectionPolicy.ts`, whose generic policy calls selection-state
operations. The generated policy retains explicit `<NodeType>` arguments, while a native completion
must do the same internally and native callers must currently provide the argument explicitly. This
request expires when exported generic reference-owner parameters remain deducible at ordinary calls.
