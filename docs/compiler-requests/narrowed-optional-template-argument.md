# A nullable generic value narrowed present must be unwrapped at calls

Control flow narrows a nullable generic value before it is passed to another generic function. The
emitter preserves the outer `std::optional` at the call instead of passing its present payload, so the
header parses but every instantiation fails.

## Minimal TypeScript

```ts
function has<T>(value: T): boolean { return true }
export function narrowed<T>(value: T | null): boolean {
  if (value === null) return false;
  return has(value);
}
```

The required call is `has<T>(value.value())`. Passing `value` asks a function taking `T` to accept
`std::optional<T>` and either fails deduction or invents a different `T`.

## Affected Flight module

At Flight `a627849` / flight-compiler `2b687ea`,
`packages/selection/src/pointerSelectionPolicy.ts` narrows `hit` with `!hit` in the early-return guard.
The emitted `apply_pointer_up_selection_policy` then calls both functions with the optional itself:

```cpp
is_node_selected<NodeType>(state, hit);
select_node<NodeType>(state, hit);
```

Its sibling pointer-down function uses `hit.value()` correctly after an equivalent guard, which is the
passing control. The focused header gate misses this because the function is a template; native selection
instantiation exposes it.

## Carrying workaround

The selection policy override preserves the emitted branches and passes `hit.value()` after the same
presence guard. It expires when narrowing consistently projects the present payload at generic calls.
