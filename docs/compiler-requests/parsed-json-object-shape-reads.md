# Parsed JSON read through `Record<string, unknown>` and `as { ... }` has no faithful lowering

JSON parsers written the usual way take `JSON.parse(text)`, check that the result is an object, treat it as
`Record<string, unknown>`, and read nested objects with an assertion such as `value as { start?: unknown;
end?: unknown }`. All three steps lower to C++ that either does not compile or would throw on every real
document:

1. `raw as Record<string, unknown>` on a parsed value is `static_cast<flight::Record<flight::String,
   flight::Any>>(flight::JsonValue)`. No such conversion exists, and the header does not compile.
2. `value as { start?: unknown; end?: unknown }` on an erased `flight::Any` is `value.object_if<start_end_...>()`.
   It throws unless the `Any` holds that exact anonymous struct. A value that came from `JSON.parse` never
   does, so even with a conversion for step 1, every nested read would throw.
3. `raw.alpha` for a key the document does not contain is `raw.get("alpha").value()`, which throws
   `bad_optional_access` where JavaScript reads `undefined`.

A runtime conversion for step 1 alone would compile and then fail on the first nested object. That is
permissive fallback behavior, so it is deliberately not added. The adoption doc already records JSON as
partial (`docs/flight-compiler-adoption.md`: "Generated ordinary structs still need compiler-provided member
reflection").

Measured at flight-compiler `2b687ea` with all 44 compiler source patches applied: 1 module emitted, 0
refused, and 1 compile error (step 1). Steps 2 and 3 are visible in the emitted text below.

## Minimal TypeScript (measured)

```ts
type Raw = Record<string, unknown>;

function parseRaw(json: string): Raw {
  const raw: unknown = JSON.parse(json);
  if (raw === null || typeof raw !== 'object' || Array.isArray(raw)) throw new Error('expected an object');
  return raw as Raw;
}

export function readStart(json: string): number {
  const raw = parseRaw(json);
  const alpha = raw.alpha;
  if (alpha === null || typeof alpha !== 'object') return 1;
  const o = alpha as { start?: unknown; end?: unknown };
  return typeof o.start === 'number' ? o.start : 1;
}
```

## Observed

```cpp
  return static_cast<Raw>(raw);                                        // (1) no JsonValue -> Record conversion
  auto alpha = raw.get(flight::String("alpha")).value();               // (3) throws for a missing key
  auto o = ... alpha.object_if<start_end_b52f33757daf8771>(); if (!erased_object) throw std::logic_error(...);  // (2)
```

## Expected behavior

A parsed JSON object needs one dynamic carrier that both `Record<string, unknown>` and an asserted object shape
can read. One option is a JSON object as a `flight::Record<flight::String, flight::Any>` (nested objects as
records in `Any`, arrays as `AnyArray`), with `as { start?: unknown }` lowered to a named read of that record
rather than an `object_if` of a struct it can never hold. Another is the compiler-provided member reflection
the adoption doc names. In either design a missing key reads as `undefined`.

## Affected modules

`@flighthq/particles-formats`:
- `pixiParse.ts`: `parsePixiJson` (step 1) and every `read*` helper (step 2).
- `spineParse.ts`: also refused for erased array reads ([erased-array-element-reading](erased-array-element-reading.md)).
- `unityParse.ts`: also refused for erased array reads.
- `registerBuiltInParticleFormats.ts`: imports the Pixi parser, and is itself refused for `options.formats ?? []`.

Their headers, and `contract.hpp` and `_internal_index.hpp` that include them, are the package's residual.
Other JSON-reading packages share the same gap, among them the `textureatlas-formats`, `spritesheet-formats`
and `scene3d-formats` `static_cast<T>(Json::parse(...))` sites.

## `parseSpineJson`: emitted before, refused at a627849

`spineParse.ts`'s `parseSpineJson` wraps `JSON.parse` in a `try` whose `catch` builds its message from
`error instanceof Error ? error.message : String(error)`.
- **Old pin:** the row emitted this function, but the body was not C++. It contained the literal
  `(error instanceof flight::Error)` and called `.message()` on a `const std::exception&`. It never compiled, so
  its loss changes no compile verdict.
- **a627849:** the emitter refuses it (`cpp-instanceof-operand-unresolved`), and the header marks it NOT GENERATED.
- **Why the refusal is right:** the runtime lowers `JSON.parse` to `flight::Json::parse`, which returns a
  `flight::JsonValue` and throws `flight::JsonSyntaxError`, a `std::runtime_error`, not a `flight::Error`. In
  TypeScript, `JSON.parse` throws a `SyntaxError`, for which `instanceof Error` is true. So the caught C++ carrier
  cannot answer that test faithfully.
- **No fix taken:** the function's return is also step 1 of this request. A stub or a permissive conversion would
  be the fallback this file rules out, so none is carried.
- **Evidence:** measured on the particles-formats row refresh. `pkg:compile` is 12/18 both before and after,
  and the three native particles-formats tests pass on both. None of them reaches `parseSpineJson`; Spine
  parsing has no native parity coverage, and none is claimed.

## Carrying workaround

None. A conversion or an override would compile code that throws on real documents, or would fork the parser.
The three parsers stay non-compiling and are reported as the residual.
