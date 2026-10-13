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
