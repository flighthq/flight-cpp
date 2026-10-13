# A source string containing U+0000 is emitted truncated

A C++ narrow string literal carries its own length, but `flight::String`'s `const char*` constructor
measures its argument with `strlen`. A TypeScript string containing U+0000 -- which is how Flight's own
modules join two fields into one composite map key -- was therefore emitted as `flight::String("a\u0000b")`,
which reads as `"a"`. The emitted C++ compiled, so no gate saw it: the program simply held a shorter string
than the source declared, and two distinct keys could become one.

Measured at flight-compiler `2b687ea`, flight `a627849`, on `@flighthq/requirement`'s `requirementIdentity` --
the first of eleven such literals in eight committed packages.

## Minimal TypeScript

```ts
export function joinedKey(facet: string, key: string): string {
  return `${facet}\u0000${key}`;
}
```

## Observed

`generated/include/flight/requirement/requirement_set.hpp`:

```cpp
return flight::String("") + flight::to_string(facet) + flight::String("\u0000") + flight::to_string(key) + flight::String("");
```

`flight::String("\u0000")` receives a `const char*` whose first byte is the terminator, so it is the empty
string, and the join is a bare concatenation: `requirementIdentity('scene.a', 'bc')` and
`requirementIdentity('scene.ab', 'c')` answer the same key. `createRequirementSet` deduplicates by that
identity, so one distinct requirement is silently dropped. The defect is invisible to the header compile
gate and to every structural gate that reads emitted text as text; it took a behavioral test asserting the
two pairs stay distinct to expose it.

## Affected

Eleven literals across eight committed packages, every one of them a composite key: `@flighthq/requirement`
(`requirementIdentity`), `@flighthq/requirement_codegen`, `@flighthq/textshaper` (two, one of which is the
text-shaper cache's lookup key), `@flighthq/scene3d-formats` (the collada material key), `@flighthq/collision`
(`collision_support2_d` and `collision_support3_d`), `@flighthq/registry_catalog`, `@flighthq/registry_codegen`,
and the requirement/registry row projections. Where the key is a cache key the wrong answer is worse than a
collision: `text_shaper_cache.hpp`'s join decides which cached shaping a lookup returns.

## Carried by a compiler patch

`source-patches/compiler-nul-string-literal.patch`. Three emission paths build a Flight string from a source
literal, and all three now go through one `emitCppStringLiteralCpp`:

- the template-literal chunk emitter (`case 'template'`), which is the path the measured defect takes, because
  a joined key is written as a template literal whose text chunks were built with `JSON.stringify` directly;
- `emitLiteral`, the expression and object-literal member path;
- the string-enum constant emitter, whose declared member type is the shared `string | number` IR union, so a
  non-string value keeps the plain form.

A value without U+0000 keeps the plain `flight::String("...")` form byte for byte, so nothing else in the
emitted tree changes. A value with one is emitted as `flight::String(std::string_view("...", sizeof("...") - 1))`:
the literal is handed over with its own extent, which `sizeof` measures including the terminator, to the
`String(std::string_view)` overload `String(const char*)` already delegated to. No count is computed beside
the text, so the two cannot drift, and an escape for a quote or a backslash still measures correctly because
`sizeof` counts the already-decoded characters.

Controls: `cppStringLiteralEmission.test.ts`, 8 cases -- the NUL alone, text on either side of it, a
template-literal chunk (the measured shape), an object-literal member value, and a string-enum member value,
all length-carrying; plus a plain value, the empty string, and a *different* control character (U+0001, which
`strlen` does not truncate), all three byte-identical to the previous emission. Mutations, all with teeth: the
U+0000 rule disabled fails the 5 positive controls, the rule widened to every value fails the 3 negative ones,
and reverting the template-chunk site alone fails exactly its own control.
