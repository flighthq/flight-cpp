# A readonly nominal array element must keep its exact owner when indexed

An indexed read from a class field declared as a readonly array of readonly nominal interfaces is
lowered to a fresh anonymous structural owner. The field itself is emitted with the exact nominal
element carrier, so assigning the element read to the inferred local then fails native compilation.

Measured with flight-compiler `2b687ea` and Flight `a627849`.

## Minimal TypeScript

```ts
interface Token {
  readonly value: string;
}

interface Line {
  readonly indent: number;
  readonly tokens: readonly Token[];
}

class Parser {
  private readonly lines: readonly Line[];

  public constructor(lines: readonly Line[]) {
    this.lines = lines;
  }

  public firstIndent(): number {
    const first = this.lines[0];
    return first.indent;
  }
}
```

## Observed C++

The class field retains the correct carrier:

```cpp
flight::Array<flight::Ref<Line>> lines;
```

but the indexed local is declared as an unrelated shape-derived owner:

```cpp
flight::Ref<indent_tokens_...> first = this->lines.element(0.0);
```

Clang rejects the conversion from `flight::Ref<Line>` to `flight::Ref<indent_tokens_...>`. The two
owners happen to expose the same fields, but substituting one reference owner for another would lose
the source object's nominal identity.

## Required behavior

The element access must retain `flight::Ref<Line>`. Readonly is an access restriction over the same
source object, not a request to synthesize a second owner from its visible fields.

## Affected module

`@flighthq/scene-document`, `sceneDocumentYamlSubset.ts`: `SceneDocumentYamlSubsetParser.parse`
fails at its first `this.lines[0]` read, preventing the only fully emitted behavioral module in the
package from compiling.

## Carrying workaround

The package source explicitly names the existing element type on bounds-dominated reads and routes
possibly absent reads through a private bounds-checked helper returning that same nominal owner. The
annotations erase from JavaScript, and the helper returns the identical element or the same undefined
result as ordinary indexing. Both should expire when indexed readonly-array reads retain the nominal
element carrier.
