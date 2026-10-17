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

## G2 current-pin Pixi inventory qualification

At compiler `806d257` / Flight `a627849`, group `other-d6be4352` still first fails at
`particles_formats/pixi_parse.hpp:55`, converting JsonValue to PixiRaw (`Record<String, Any>`).
The source root guard is retained. The emitted nested lifetime, color, angle, spawn and acceleration
reads still ask Any for exact anonymous struct owners; those owners are not in the parsed JSON tree.
The emitted fixed-key reads also still use `get(key).value()`, although Record::get returns an empty
optional for a missing key. Those are related emitted barriers, not additional measured first errors.

The source has no mandatory configuration fields and explicitly supports an empty object through
defaults. Syntax errors retain their JSON-prefixed error, while null, arrays and primitive roots
receive the separate expected-object error. Config mapping and document diagnostics consume the same
parsed raw object; an assertion adds no fresh JavaScript object or native anonymous-struct identity.
The existing native particles-formats test intentionally excludes this parser and its built-in
registration table, so its passing coverage is not Pixi parser evidence.

Only the four Pixi-dependent inventory rows are linked in this arc, with
[the JSON assertion request](json-parse-assertion-static-cast.md#g2-pixi-particle-parser-the-destination-is-a-dynamic-record)
as the primary first-error cause and this file as related required work. They retain **0 pass / 4 fail**
from G1, with unchanged diagnostics and no C++ recompile. No conversion, parser replacement, new
request, global sweep or upstream test port is carried; other parser rows are not changed.


## G2 Unity particle JSON returns a named view of the wrong carrier

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` / compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, cluster `call-has-no-matching-c287f892` contains
`flight/particles_formats/unity_parse.hpp`. Its recorded first failure is native77:
`no matching function for call to 'named_properties'`. Source
`packages/particles-formats/src/unityParse.ts:35-47` parses to unknown, rejects null/nonobject/array
roots, then returns that original object as Record<string,unknown>. The retained C++ is:

```cpp
inline flight::Record<flight::String, flight::Any> parse_unity_json(flight::String json) {
  flight::JsonValue raw;
  // JSON.parse, format-tagged catch and the root guard are retained here.
  return flight::named_properties(raw); // native77
}
```

This is this existing parsed-JSON/dynamic-Record request, already listing unityParse.ts, rather than
a new binding name or a missing declaration. Current named_properties overloads accept an existing
shared native object, StructuralRef or Any row owner. JsonValue instead owns a tagged JSON tree with
JsonObject/JsonArray storage and has no such native row. Moreover, even a named-view overload would
not match the function's declared Record<String,Any> return: NamedProperties and Record are different
carriers. Returning an empty view/Record or adding an unchecked conversion would conceal the real
JSON values. An asserted UnityParticleDocument/native anonymous struct is not an owner that the
parsed object was allocated with.

Faithful lowering must let one parsed object and its nested values answer the source dynamic Record
reads and arrays, with missing keys yielding undefined, through a coherent parse carrier or checked
JSON-backed member view. Do not invent schema owners, clone a second tree to satisfy an unrelated row,
restrict the parser to prevalidated native Unity structs, require mandatory properties or coerce
unknown values. The existing C++ helpers visibly use get(key).value() on Records and even on Any reads
from NamedProperties; missing fields must reach the source default paths instead. Erased-array loops
also retain named length reads/indexing without recovering array storage. These are inspected adjacent
lowering barriers, not freshly measured next diagnostics.

The catch remains an independent inspected error boundary: source catches JSON.parse's error and
throws Error with the Invalid Unity particle JSON prefix plus its message. Native Json::parse throws
JsonSyntaxError (a std::runtime_error), while the emitted catch binds const std::exception& and
casts it to flight::Error before reading message. Preserve the source exception kind/message and
separate expected-object error; no new native exception test or error-order claim is made here.

### Real particle configuration, document and schema owners

Both public entry points accept JSON strings and optional pixelsPerUnit, default100. The direct
parseUnityParticle path returns rawToConfig(parseUnityJson(...), ppu), with no intermediate document.
parseUnityParticleDocument parses once, then evaluates config, diagnostics and document in that order
from the same raw object. Its existing UnityParseResult carries the real ParticleEmitterConfig owner,
UnityParticleDocument owner and ImportDiagnostic array, not a substitute plain row or empty success.
An empty JSON object is valid and takes defaults; syntax errors and null/array/primitive roots throw.

Preserve the source conversion rules, including these distinctions:

- rn/rb/rs use typeof number/boolean/string and their actual fallbacks, without numeric coercion or
  an added finite-only schema. mmLow/mmHigh accept a number directly; twoConstants/twoCurves select
  constantMin/constantMax with constant fallback; other modes use constant. Preserve the source
  min/max and NaN/infinite/-0 domain, rather than adding range normalization or swapping endpoints.
- PPU scales speed, emitter radius/box dimensions and gravityModifier*physicsGravity (default9.81).
  It does not scale lifetimes or start size. Spawn rate averages the low/high rateOverTime values
  (default10). Only the first burst feeds config count/repeat interval; cycleCount1 selects interval0.
  maxParticles uses the source bitwise-or0 conversion. Looping defaults true and maps duration to0;
  nonlooping uses max(0,duration default5). Rotation-over-lifetime converts degrees/sec to radians/sec.
- Config shape handling defaults disabled, point, direction(0,-1), spread pi and dimensions0.
  Enabled Sphere/Hemisphere/Circle map to circle; Box/Rectangle to rect; Cone uses the authored
  angle in radians and becomes circle only for positive radius. The typed document readShape defaults
  enabled=true, Cone and radius1, and preserves authored shape fields/scale. Routing config through
  that typed document would change defaults and is not an equivalent fix. Unsupported shape strings
  retain the actual point fallback and Recover diagnostic, not a new mandatory enum validator.
- Keep start/end RGB and alpha selection from colorOverLifetime versus startColor, white/fade
  defaults, size endpoint division by max(0.001,average start size), and enabled rotation controls.
  Color, alpha and size curves bake only when their corresponding arrays have more than two keys.
  Color keys may hold nested color or inline RGB; missing time uses i/(length-1). Delegate to the
  existing particleColorCurveFromKeyframes/particleCurveFromKeyframes with real ColorKeyframe/
  CurveKeyframe owners and their default33 samples; do not reduce multi-stop data to endpoints,
  replace curves with null or invent a new interpolation algorithm. Disabled/short-curve paths retain
  their actual null curves and endpoint/default behavior.
- rawToDocument preserves its explicit name/loop/prewarm, authored units, min/max records, all burst
  records, shape and lifetime-reader fields using their existing defaults. Its color/size lifetime
  readers currently populate endpoint/size fields, not the optional gradient/curve members advertised
  by the schema. Preserve that actual mapping; this disposition does not claim arbitrary raw JSON
  field preservation or upgrade the document's round-trip fidelity. Source allocations of config,
  document subobjects and curve arrays remain where the source creates them, with no repair copies.
- unityBlendMode reads only the top-level blendMode despite a comment mentioning renderMode. Its
  lowercase additive/add, multiply, screen, normal/alpha mapping and null fallback remain unchanged.
  Do not add a renderMode fallback or force every unrecognized mode to normal.

### Diagnostics and the three omitted helpers

collectUnityDiagnostics scans the existing eleven unsupported-module key/label pairs in source order:
velocityOverLifetime, forceOverLifetime, limitVelocityOverLifetime, inheritVelocity, noise, collision,
subEmitters, trails, textureSheetAnimation, externalForces and lights. Only an object with boolean
enabled=true contributes. Collect first, then emit one Skip unity.module-unsupported per distinct
module label, origin collectUnityDiagnostics. Preserve labels and insertion order rather than merging
all losses into a count or reporting every field as unsupported.

Then report Skip unity.extra-bursts-unsupported with the entire burstCount when there is more than one
burst, Recover unity.shape-unsupported for an enabled unmapped shape, Skip unity.prewarm-unsupported,
and Skip unity.start-rotation-unsupported for nonzero constant/min/max or curve/twoCurves modes.
The comment above the collector still calls the extra-burst case Drop; the actual code and test say
Skip and govern this contract. Multi-stop gradients now bake and must not gain an obsolete unsupported
diagnostic. Both the real diagnostics array and the selected source fallback behavior must survive.

The header still explicitly omits rawToConfig85 and readEmission364 with
cpp-erased-array-assertion-runtime-required, and collectUnityDiagnostics216 with
cpp-optional-member-access-unproven (property bursts lacks present-value proof). The existing
[erased-array request](erased-array-element-reading.md) covers array<unknown>/Record reads and notes
that AnyArray has exact recovery but no generic element view; the optional em?.bursts test and later
em.bursts need coherent [present-flow evidence](optional-index-member-present-flow.md), not the
already-carried optional-aware is_array binding alone. The [retained-callee request](retained-declaration-calls-omitted-callee.md)
covers calls to those missing algorithms from public parsers/rawToDocument. Adding their declarations
would neither repair native77 nor supply config mapping, diagnostics or burst parsing. No candidate,
positive unused compile/link/run or qualified negative-link gate was attempted or claimed.

Existing unityParse.test.ts covers valid config defaults/conversions, invalid roots and tagged syntax
errors, multi-stop color/alpha/size sampling, config/document agreement, preserved name/loop/prewarm,
diagnostic severities/details and serialization/re-import expectations. These were read as source
requirements only. No native Unity config/parser/document/curve/serialization/particle execution,
header-availability or link success is claimed.

**Recorded 0/1 pass, 1 fail is retained.** Only this existing request appendix, worklist105 and two
compact product lines (one cluster/one failed row) change. C++, particle/schema owners, runtime,
repairs, source patches, generation inventory and pins are unchanged; unchanged C++ is not rerun.
Foreign compact products/worklist entries and historical/G1 totals remain intact. No foreign coordinator
attachment was applied, and no fake parser/counts, copied owner or narrowed input is supplied.
