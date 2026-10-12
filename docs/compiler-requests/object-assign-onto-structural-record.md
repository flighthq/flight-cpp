# `Object.assign` does not lower onto an ordinary structural target

The flight-cpp backend lowers `Object.assign` for represented `Record<K, V>` values and for one
special construction pattern: a closed callable-object reference completed by an exact compatible
object literal. It refuses an ordinary interface or entity target even when its fields and the source
are closed and representation-compatible.

## Minimal TypeScript

```ts
interface Box {
  a: number;
  b: string;
}

export function assignBox(source: Box): Box {
  const target: Box = { a: 0, b: '' };
  return Object.assign(target, source);
}
```

The same refusal occurs when `source` is the exact literal `{ a: 1, b: 'x' }`. A `Box` parameter, a
local `Box`, and an entity returned by an allocation or factory all reach the same rule.

## Controls that already work

| source | target | result at `2b687ea` |
| --- | --- | --- |
| `Box` | `Box` | refused |
| `{ a: 1, b: 'x' }` | `Box` | refused |
| `Record<string, number>` | `Record<string, number>` | emitted through `flight::object_assign` |
| complete exact object literal | closed callable-object intersection | emitted as writes through the retained callable-object reference |

The callable-object control is important: this request is not asking to relax the completeness or
compatibility checks on that construction path. It asks for an identity-preserving field plan for an
ordinary structural owner.

## Expected behavior

Evaluate the target and source once, retain the target's identity, and assign every source field that
the closed target accepts through the target's existing cells. For the minimal case, the observable
operation is equivalent to:

```cpp
target->a = source->a;
target->b = source->b;
return target;
```

Optional source fields still need ordinary JavaScript `Object.assign` behavior: an absent property is
not a write, while a present property whose value is `undefined` is. A lowering must therefore use the
compiler's property-presence evidence rather than default-constructing every target cell. Where the
source carrier has already collapsed those two states, the backend must keep refusing instead of
pretending an `optional<T>` payload supplies own-property presence.

## Observed at `2b687ea`, Flight `a627849`

```text
Object.assign requires one closed callable-object target and an exact compatible object literal source
```

The message describes the implemented callable-object construction path, not the structural target in
the failing expression. Supplying an exact literal does not make an ordinary structural target
callable and does not clear the refusal.

## Affected modules

- `packages/scene3d/src/sceneDocument.ts` calls
  `Object.assign(createMaterial3D(source.kind), source)`. Replacing the fresh `Material3D` entity would
  lose the canonical live material owner that meshes retain. `MaterialLike.name` is optional, so the
  required-field `Box` reduction proves the ordinary target gap but does not by itself prove that this
  source's absent-versus-present-`undefined` property state is representable. A first fix may advance
  this module to a more specific presence refusal rather than emit it.
- `packages/notification/src/notification.ts` allocates a subscription entity and assigns its generic
  closed fields before finishing that same entity. The refusal takes the module that all five
  subscription factories use.

The notification module is supplied by `notification-subscription-lifecycle`; its declared
`remaining` records the unsupplied lifecycle behavior. Scene3D retains its refusal until the compiler
can mutate the factory-created material without replacing its identity.

## Carrying workaround

There is no expression-level equivalent that preserves a freshly allocated entity's identity for an
arbitrary closed generic source. Object spread constructs a different object, and a source-authored
field list would duplicate a schema the compiler already knows. The compiler should extend its
existing closed property plan to ordinary structural targets when every assigned property's presence
is represented, rather than routing them through `Record`, copying them, or weakening the
callable-object checks. A source whose optional property presence is not represented must remain a
truthful refusal.
