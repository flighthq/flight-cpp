# A call to a generic function converts its argument to the callee's unsubstituted parameter type

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, the generic callee is emitted correctly as a C++ template.
But a non-generic caller materializes or converts the argument to the callee's parameter type as
written, with the callee's own type parameter left in it. That parameter is not in scope at the call
site, so the header fails on an undeclared identifier.

## Source

```ts
// packages/shape/src/shapeBoundsRegistry.ts
export function registerShapeBoundsCommand<K extends ShapeCommandKey>(command: ShapeBoundsCommand<K>): void { ... }

// packages/shape/src/registerDefaultShapeBoundsCommands.ts
registerShapeBoundsCommand({ fillBounds: defaultShapeBoundsFlush, key: 'beginTextureFill', strokeBounds: defaultShapeBoundsFlush });
```

## Emitted mismatch

The callee is a proper template (`shape_bounds_registry.hpp:43`, which compiles):

```cpp
template <typename K>
inline void register_shape_bounds_command(flight::Ref<flight::types::ShapeBoundsCommand<K>> command) { ... }
```

The non-generic caller builds the literal's owner with that `K` still in it:

```cpp
inline void register_default_shape_bounds_commands() {
  flight::shape::register_shape_bounds_command(([&]() { ...
    return flight::make_ref<flight::types::ShapeBoundsCommand<K>>(flight::types::ShapeBoundsCommand<K>{.key = object_member_key, ...});
  }()));
```

```
flight/shape/register_default_shape_bounds_commands.hpp:17: error: use of undeclared identifier 'K'
```

The same defect appears with a conversion rather than a construction. A non-generic caller of a
`Traits`-generic node helper emits `flight::structural_ref_cast<flight::types::NodeOf<Traits>>(...)`
(`flight/scene3d/scene_document.hpp:106`).

## Expected lowering

At the call site, substitute the type arguments TypeScript inferred for the call before naming the
parameter type, then construct or convert to that instantiation. Here the inferred `K` is the
`'beginTextureFill'` literal, whose carrier is the constraint `ShapeCommandKey`, which is the same
instantiation the registry already stores:

```cpp
flight::make_ref<flight::types::ShapeBoundsCommand<flight::types::ShapeCommandKey>>(
    flight::types::ShapeBoundsCommand<flight::types::ShapeCommandKey>{...})
```

Alternatively, leave the template argument to deduction from an argument whose type is already
instantiated. Neither the runtime nor a repair can choose the argument: it is the checker's
inference, and a fixed constant would be a guess.

## Affected clusters

- `undeclared-identifier-a74215ca`, `K`, 6 headers: `shape/register_default_shape_bounds_commands.hpp` and
  the five `scene2d_formats` SVG headers that include it.
- `undeclared-identifier-993e82f1`, `Traits`, 29 headers. That cluster belongs to builder4 (E0a), and its
  rows are not changed here.
