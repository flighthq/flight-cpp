# An assertion on an optional value must cast the present value, not the optional

A parameter declared `obj?: Readonly<PartialNode<T>>` is narrowed with
`obj as Readonly<PartialNode<T>>` at the call site. In TypeScript the assertion is type-level only:
the value stays `... | undefined`, and the callee's `obj?` parameter accepts it. The emitter lowers
the assertion to `flight::structural_ref_cast<Target>(obj)` **with `obj` still an
`std::optional<StructuralRef<...>>`**, and neither runtime overload accepts an optional.

## Minimal TypeScript

```ts
interface Node { kind: string }
type PartialNode<T> = Partial<T> & { kind?: string };

declare function createNode<T>(kind: string, obj?: Readonly<PartialNode<T>>): T;

export function createSprite(obj?: Readonly<PartialNode<Node>>): Node {
  return createNode('Sprite', obj as Readonly<PartialNode<Node>>);
}
```

## Observed C++

`@flighthq/scene2d` `scale9Sprite.ts` `createScale9Sprite`:

```cpp
inline flight::Ref<flight::types::Scale9Sprite> create_scale9_sprite(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::RectangleLike>>> scale9_grid,
    std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<data_enabled_..._70fdef28304382b4>>>>> obj = std::nullopt) {
  return flight::structural_ref_cast<...Scale9Sprite...>(
      flight::scene2d::create_node_2d<...>(
          flight::types::scale9_sprite_kind,
          flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<data_enabled_..._70fdef28304382b4>>>>>(obj),
          ...));
}
```

```
flight/scene2d/scale9_sprite.hpp:162:255: error: no matching function for call to 'structural_ref_cast'
  candidate template ignored: could not match 'StructuralRef' against 'std::optional'
  candidate template ignored: could not match 'std::shared_ptr' against 'std::optional'
```

The cast target is exactly the parameter's element type, so no conversion is wanted at all; only the
absence has to be preserved.

## Required C++ shape

Unwrap the present value and keep the absence, the way the emitter's contextual-union paths already
lower a `T | undefined` carrier:

```cpp
([&]() -> std::optional<Target> {
  auto source = obj;
  if (!source.has_value()) return std::nullopt;
  return std::optional<Target>{flight::structural_ref_cast<Target>(source.value())};
}())
```

When the target already equals the element type, the cast can be elided and `obj` passed through
unchanged. Neither form changes identity, absence, or the callee's parameter.

## Affected modules

- `@flighthq/scene2d` `scale9Sprite.ts` at flight `a627849`: `createScale9Sprite`, four headers
  (`scene2d/scale9_sprite.hpp` and `scene2d/_internal_index.hpp`, `scene2d/contract.hpp`,
  `render/render_viewport.hpp` through their includes). Measured at `-j1`: no matching call at
  `scale9_sprite.hpp:162`. The same header carries a second, independent family
  (`use of undeclared identifier 'create_scale9_sprite_runtime'`), so this request does not by itself
  make the header pass.
- The generated `@flighthq/shape` `scale9Shape.ts` form carries the same emitted cast
  (`shape/scale9_shape.hpp`), but that path is shadowed by the `shape-prior-emission-scale9-shape`
  override; its compile was not measured here.
