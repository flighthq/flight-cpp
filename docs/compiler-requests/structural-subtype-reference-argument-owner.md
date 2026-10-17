# A structural subtype argument must retain an identity-preserving view

TypeScript accepts a reference to a structurally wider interface where a readonly smaller interface
is required. The C++ emitter instead gives both named interfaces unrelated owning reference types and
passes one directly as the other, which does not compile.

Measured with flight-compiler `2b687ea` and Flight `a627849`.

## Minimal TypeScript

```ts
interface Position {
  readonly offset: number;
  readonly line: number;
  readonly column: number;
}

interface Token {
  readonly kind: string;
  readonly offset: number;
  readonly line: number;
  readonly column: number;
}

function positionLine(position: Position): number {
  return position.line;
}

function tokenLine(token: Token): number {
  return positionLine(token);
}
```

## Observed C++

```cpp
double position_line(flight::Ref<Position> position);

double token_line(flight::Ref<Token> token) {
  return position_line(token);
}
```

`flight::Ref<Token>` and `flight::Ref<Position>` are sibling `shared_ptr` owners, so Clang rejects the
call even though every member the callee may read is present on the source object.

## Required behavior

A readonly structural parameter must receive an identity-preserving view over the original owner, or
both compatible declarations must share a carrier that preserves that owner. The compiler must not
silently copy the visible fields: identity and later reads of the original object can be observable in
the general case.

## Affected module

`@flighthq/scene-document`, `sceneDocumentYamlSubset.ts`: the parser passes its private token record to
`createSceneDocumentYamlSubsetRefusal`, whose position parameter reads only the token's `offset`,
`line`, and `column`. The generated call passes `Ref<Token>` to `Ref<Position>` and prevents the parser
and every header including it from compiling.

## Carrying workaround

At this private, read-only boundary the package constructs the declared position record from the same
three token fields. The callee immediately copies those scalar fields into the published refusal and
does not expose or compare the position object's identity, so the fresh record preserves all observable
results. The workaround should expire when readonly structural subtype arguments use an
identity-preserving view.

## G2 Scene3D reveal optional ease owner projection

At Flight a627849 / compiler806d257, revealScene3DResourcesOnResolve forms
`tweenOptions = options?.ease !== undefined ? { ease: options.ease } : undefined` and passes it as
createTween's optional readonly TweenOptions. The generated local is optional<Ref<ease-record>>;
the argument instead constructs RowWritable<ease-record> directly from the **whole optional** before
casting to the readonly TweenOptions row. StructuralRef accepts a supported present owner/view,
not an optional container, so actual unused-header compilation after the scratch collector declaration
fails there (candidate183/original181). See the
[retained-callee measurement](retained-declaration-calls-omitted-callee.md#g2-scene3d-resource-reveal-collector-declaration-fails-before-link).

This extends the structural argument request to a compatible optional projection: absence must stay
std::nullopt; only a present payload may become a readonly view of the same anonymous ease owner,
with the same EasingFunction and all other optional TweenOptions fields absent. Unconditional value(),
constructing a default owner for absence, copying the original options or granting writable authority
is not that handoff. The runtime's rejection is correct; no permissive optional-to-row constructor,
source patch or override is carried. The independent manager/target union-order failure remains.
