# A callback member that names a constrained type parameter is emitted with the constraint

An interface member whose callable parameter mentions a constrained generic must retain the generic
parameter in C++. The current type emitter substitutes the constraint into that nested callback even
though sibling members of the same interface retain the parameter.

## Minimal TypeScript

```ts
interface Base { readonly kind: string }
interface Events<T extends Base> {
  onActive: Signal<(value: T | null) => void>;
  onChange: Signal<(values: readonly T[]) => void>;
}
```

The required C++ uses `std::optional<T>` for `on_active` and `Array<T>` for `on_change`. At
flight-compiler `2b687ea`, the second member is correct while the first is fixed to the constraint's
owner.

## Affected Flight module

Flight `a627849`, `packages/types/src/SelectionState.ts`:

```cpp
template <typename NodeType>
struct SelectionSignals : public flight::ReferenceEnabled {
  std::shared_ptr<Signal<std::function<void(
      std::optional<std::shared_ptr<Node<flight::Any>>>)>>> on_active_change;
  std::shared_ptr<Signal<std::function<void(flight::Array<NodeType>)>>> on_change;
};
```

`createSelectionState<Node3D>()` is valid TypeScript, but a `Node3D` active value cannot enter the first
signal without changing identity. This is distinct from a local being stored as its constraint: the
wrong carrier is baked into the emitted generic declaration before any function is instantiated.

## Carrying workaround

The selection type override respells only that member as
`Signal<std::function<void(std::optional<NodeType>)>>`. It expires when nested callback members retain
their enclosing type parameter.
