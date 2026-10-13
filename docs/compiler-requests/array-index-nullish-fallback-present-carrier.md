# An indexed fallback must keep the statically present element carrier

An indexed read from a readonly array is lowered to the runtime's optional index-miss carrier even
when TypeScript gives the expression the non-optional element type. If that local is the fallback of
a nullish expression, the generated lambda promises to return the present element but returns the
whole optional carrier on the fallback path, which does not compile.

Measured with flight-compiler `2b687ea` and Flight `a627849` under Flight's TypeScript configuration,
where unchecked indexed reads retain the array element type.

## Minimal TypeScript

```ts
interface Token {
  readonly kind: string;
}

function chooseToken(tokens: readonly Token[], fallbackIndex: number): Token {
  const fallback = tokens[fallbackIndex];
  return tokens[0] ?? fallback;
}
```

## Observed C++

```cpp
std::optional<flight::Ref<Token>> fallback = tokens.get(fallback_index);
return ([&]() -> flight::Ref<Token> {
  auto left = tokens.get(0.0);
  if (left.has_value()) return left.value();
  return fallback;
})();
```

Clang rejects the final return: `std::optional<flight::Ref<Token>>` cannot become
`flight::Ref<Token>`.

## Required behavior

The local's storage must respect its statically non-optional `Token` type, or the fallback projection
must select the present payload before returning it. The emitter must not expose its bounds-safe
runtime wrapper as though that wrapper were the source value's declared type.

## Affected module

`@flighthq/scene-document`, `sceneDocumentYamlSubset.ts`: `addMappingEntry` obtains `colonIndex` from
the parser's top-level-colon scan after rejecting a negative result. The colon token is therefore
present, but `tokens[0] ?? colon` emits the invalid carrier above and prevents the YAML-subset parser
and every header including it from compiling.

## Carrying workaround

The package source explicitly names `SceneDocumentYamlSubsetToken` on the existing `colon` local.
The annotation erases from JavaScript; it preserves the same token identity, fallback evaluation,
diagnostics, and parser state, and should expire when inferred indexed locals keep their declared
source carrier through a nullish fallback.
