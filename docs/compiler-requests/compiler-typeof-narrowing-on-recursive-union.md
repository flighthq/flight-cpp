# `typeof` on a recursively union-typed value lowers to a constant comparison

A `typeof x === 'string'` guard on a value whose type is the recursive union `FlightDocumentValue`
does not become a runtime test. The emitter folds `typeof` from the declared type and emits a
comparison of two string literals, so the guard is a **constant** — and the narrowed binding after it
is never narrowed.

Found at flight `a62784923f` while regenerating `@flighthq/tokens`. This one is worth calling out
separately from its package because the emitted code is not merely rejected: a guard that
`std::bool_constant`-folds to `false` is a silently wrong branch, and only the follow-on conversion
error happens to stop it compiling.

## Minimal TypeScript

The reduction below is the shape of the real code, with the declaration copied from
`packages/types/src/FlightDocumentFieldSchema.ts`:

```ts
export type FlightDocumentScalar = boolean | number | string | null;
export type FlightDocumentValue = FlightDocumentScalar | FlightDocumentValue[] | FlightDocumentFields;
export interface FlightDocumentFields { [name: string]: FlightDocumentValue; }

export function isReference(value: FlightDocumentValue): value is string {
  if (typeof value !== 'string') return false;
  const text: string = value;
  return text.startsWith('$') && !text.startsWith('$$');
}
```

## REDUCTION DOES NOT REPRODUCE THE FOLD — measured, 2026-10-12

This is the most important thing in this file. Written as a backend unit test against
`lowerTypeScriptSources` + `createCppCompilerBackend`, the guard above lowers **correctly**:

```cpp
inline bool is_reference(flight::types::FlightDocumentValue value) {
  if (!(([&]() { const auto& typeof_value = value;
                  return typeof_value.has_value() && typeof_value.value().index() == 4; }()))) {
    return false;
  }
  const flight::String text = std::get<4>(value.value());
  return (text.starts_with(flight::String("$")) && !text.starts_with(flight::String("$$")));
}
```

Variants that were tried and all lowered correctly:

| reduction | result |
| --- | --- |
| single module, union declared inline | correct variant-index test |
| cross-module import of the union | correct |
| the exact recursive declaration above (index-signature interface) | correct |
| positive guard `=== 'string'` and negative guard `!== 'string'` | both correct |
| as a type predicate (`value is string`) | correct |
| a guard that excludes only ONE member (`!== 'number'`) | correct, and safely: it emits a re-check over the remaining alternatives with a `throw std::logic_error("narrowed union holds an excluded alternative")` fallback rather than a cast |

So the fold is not caused by the guard's shape, by the import boundary, or by the declaration shape. The
one visible difference between the reduced case and the real one is the parameter carrier: the real
header takes `flight::Ref<FlightDocumentValue>` while every reduction emits the union **by value**, and
in the reductions the index-signature member is `flight::Record<flight::String, FlightDocumentValue>`
whereas an earlier reduced form produced an anonymous `flight::Ref<anonymous_...>` for it instead.

A fix cannot honestly be written against a defect that does not reproduce. What is needed next is the
piece of the real graph that makes the carrier a `Ref` — most likely the binding/structural-member
inputs the repository passes and a unit fixture does not. Until that is reduced, this stays a request.

## Observed C++ in the real package

```cpp
inline bool is_flight_document_token_reference(flight::Ref<FlightDocumentValue> value) {
  if ((flight::String("object") != flight::String("string"))) {   // constant, and false
    return false;
  }
  const flight::String text = value;                              // error: no viable conversion
  return (text.starts_with(reference_sigil) && !text.starts_with(reference_escape));
}
```

`"object"` is the declared type's name, not the value's runtime type. For a union there is no single name
to fold, and the safe answer is a real test, as the reductions already produce.

## Expected C++

```cpp
if (!value.has_value() || value.value().index() != 4) return false;
const flight::String text = std::get<4>(value.value());
```

The runtime already carries the discriminator; the guard should consult it.

## Affected modules

`@flighthq/tokens`' `flightDocumentTokenReference.ts` (both `isFlightDocumentTokenReference` and
`substituteFlightDocumentTokenValue`, which use the same guard) and `flightDocumentSceneTokens.ts`.
Anywhere a `typeof` guard narrows a recursive value union — `FlightDocumentValue` is the shape that
makes it obvious, but the pattern is general.

## Carrying workaround

There is a patch, [`tokens-string-narrowing-binding`](../../source-patches/tokens-string-narrowing-binding.patch),
which restates the guard as an early return and binds the narrowed value to an explicit `string` local.
At `a627849` **it no longer helps**: the emitted header above *is* its output, so the rewrite reaches
the same fold. It is recorded here rather than silently left in place, because a carried patch that
appears to address a defect while emitting a constant guard is worse than no patch — it hides the
defect behind work that looks done. Reverting or re-deriving it should wait until this lowering exists,
since without it the module does not emit at all.

## Related but distinct: typeof on a `JsonValue` (linked evidence, 2026-10-12)

`@flighthq/bitmapfont-formats` is refused twice for a `typeof` whose reason text names the missing piece
explicitly:

```
cpp emission failed for .../bitmapFontJson.ts: typeof follows an indexed array-element read rooted in the
exact flight::JsonValue returned by JSON.parse. JsonValue's closed kind tag can answer typeof on the
retained value itself, but the runtime contract exposes no owner-preserving read ...
cpp emission failed for .../explainBitmapFontParse.ts: typeof follows a named-property read rooted in the
exact flight::JsonValue returned by JSON.parse. ...
```

This is NOT the same defect as the fold above, and the difference matters for whoever fixes them:

- **This file (tokens)** — the guard on a *recursive union* folds to a constant comparison of type-name
  literals. Reported as a lowering defect; a reduction does not reproduce it (see above).
- **bitmapfont-formats** — the guard is on a `JsonValue` whose *closed kind tag already answers typeof*;
  what is missing is an owner-preserving read the runtime contract does not expose. That is a **runtime
  capability**, and its remedy is a `JsonValue` kind accessor, not a change to union-guard lowering.

They are linked because both surface as "typeof is wrong", and because a reader meeting one will assume
it explains the other. They should be fixed as two things; if a single change happens to clear both, that
is worth saying explicitly rather than assuming.

## Third package, and a second shape inside it (linked evidence, 2026-10-13)

`@flighthq/bitmapfont-formats` is refused twice for `typeof` on `JsonValue` (bitmapFontJson.ts and
explainBitmapFontParse.ts), and its emitted `bitmap_font_format_kind.hpp` fails for a related reason:

```
no member named 'object_if' in 'flight::JsonValue'   (bitmap_font_format_kind.hpp:41)
```

`object_if` is the emitter reaching for an owner-preserving object read on `JsonValue` -- the same missing
runtime accessor the two refusals describe, now visible as emitted code rather than as a refusal. So this
package needs the SAME `JsonValue` kind/owner accessor as the bitmapfont-formats refusals above, which are
already recorded as a runtime capability rather than a lowering defect.

One more distinction worth keeping: the same package's other failure is a `.width` read on the texture
source variant (`bitmap_font_fnt.hpp:187`), which belongs to the interface-heritage family in
compiler-interface-heritage-owner-widening.md, NOT to this one. The two appear side by side in one package and
should not be conflated when it is next picked up.
