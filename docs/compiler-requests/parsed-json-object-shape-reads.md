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

The initial measurements below are historical. The final Pixi and Spine qualifications later in this request
record bounded private graphs over the original JSON owner; other parser contracts remain open.

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

The compiler-private-json-read-flow patch now carries the proven private Pixi producer/read graph
through the original JSON owner, with runtime data-property reads. It supplies no general JsonValue-to-Record
conversion or parser override. The original public Spine parser is now qualified below; Unity remains
non-compiling at its owning Record return.

## G2 current-pin Pixi inventory qualification

### Final original Pixi qualification (2026-10-18)

One normal `npm run pkg:generate -- particles-formats -j2 --progress` ran on frozen `f91a5bc8`
with 82 compiler / 283 total patches and exited zero. It emitted 1134/1141 closure modules across 10 packages;
the requested particle-format slice is 15/18 emitted,3 refused. Thirteen generated headers plus README
and manifest changed. Both pinned sibling checkouts were restored clean. Later peer compiler/runtime
work was reconciled without a second generation claim; the mixed schema 2 manifest retains each slice's
actual provenance. There is no new inventory/hash framework or override/hash change in this unit.

The installed `flight/particles_formats/pixi_parse.hpp` now compiles, links and runs both original
public entry points. The CMake/Bazel consumer covers defaults/fresh config ownership, nested numeric
and wrapped endpoints, color parsing, clockwise angles, spawn precedence, blend modes, ordered
diagnostics, syntax/root errors, null acceleration, signed zero/nonfinite values, Unicode whitespace,
non-ASCII casing and literal UTF16 surrogates. The existing registry fixture initializes inherited
texture_size on its actual options owner before the unchanged public call. The new consumer, three
existing particle-format consumers and the Spine parse helper affected by shared parseInt pass 5/5.
No unchanged Spine SDK or counter check was repeated for this unit's final qualification.

A separate driver uses that actual installed public parser and real factory/diagnostics/constants,
with no copied parser body. It matches the untouched pinned TypeScript modules on 568 inputs / 1136 calls
and all 52 config fields, checking ordered diagnostics and exactly one source parse per invocation.
Signed zero/NaN/Infinity kinds and application root messages are exact. Finite fields are exact except
directionX/Y's relative/absolute 1e-12 libm tolerance. Built-in syntax/TypeError wording is implementation
dependent. Seven explicit test-host UnicodeService mappings cover this corpus, including contextual
Greek casing and lone UTF16 units; this is bounded coverage, not a universal Unicode host claim.
Nine complete public/alias/escape/callback/compare/capture/export/optional ABI byte/refusal controls
also pass on the actual production-built 82 patch backend. Focused Vitest execution remains unavailable.

The complete Clang22.1.8 -j2 checks cover all 13 changed headers and 2 unchanged Pixi-dependent barrels:
**10 pass / 5 fail,0 unattempted**. The four original `other-d6be4352` rows are **1 pass / 3 follow-ons**:
Pixi passes, while `_internal_index`, `contract` and `register_built_in_particle_formats` now reach
the existing Spine particle `require_spine_record` Any→Record error at line 103. That cluster
`no-viable-conversion-31b754c5` therefore has four measured failed rows including `spine_parse.hpp`.
Unity's existing cause `call-has-no-matching-c287f892` now first fails at NamedProperties→Record
return at 77 rather than a missing JSON named-view overload; its full private read graph remains unproven.
The nine other passes were already passing. Initial scoped wrappers also reported13 unrelated Scene3D
GL override drifts, subsequently delivered as peer 4 work; these were not repaired or restamped here.

This unit promotes **exactly 1 of the original 25 historical rows**. The earlier Spine JSON counter
was outside 25 and promoted 0 of them. All 25 remain applicable and the current 27 related rows are a
distinct inventory. Nominal Lottie/Aseprite/TexturePacker/glTF document contracts and the other particle
parsers remain unresolved; no whole-package/all 25 completion or refreshed global sweep is claimed.
One parsed graph and descendant owners remain live, missing fields read undefined, direct nullish
reads retain TypeError and syntax-only catches leave later failures outside the parse catch. There is
no parser/body replacement, parsed-tree/Record copy, schema gate, fabricated nominal owner, public
Record ABI change, input narrowing, refusal suppression or Canvas work.

The earlier Pixi inventory first failed at native 55's JsonValue→PixiRaw cast. The bounded compiler
proof, runtime prerequisites and diagnostic qualification are described in the preparation section
below. This final measurement supersedes its earlier 0/4 retained disposition.

## G2 Unity particle JSON returns a named view of the wrong carrier

The following is the earlier source audit. Final normal 82 patch generation still fails this row,
now at NamedProperties→Record return at 77; the missing JSON named-view overload itself is resolved.

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


## G2 Spine particle object guard cannot return an unprojected Any as Record

The following is the earlier source audit. Final normal 82 patch generation freshly measures the same
Any→Record error at native 103, with three former Pixi-dependent barrels joining this failed source row.

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` / compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, cluster `no-viable-conversion-31b754c5` contains
`flight/particles_formats/spine_parse.hpp`. Its recorded first failure is native102:
`no viable conversion from returned value of type 'flight::Any' to function return type 'flight::Record<flight::String, flight::Any>'`.
Source `packages/particles-formats/src/spineParse.ts:93-99` declares a dynamic Record type predicate
and a required-object helper. The helper's emitted signature carries Any into Record:

```cpp
inline flight::Record<flight::String, flight::Any>
require_spine_record(flight::Any value, flight::String path) {
  if (!is_unknown_record(value)) invalid_spine_entry(path, flight::String("a JSON object"));
  return value; // native102: no dynamic object/Record projection
}
```

isUnknownRecord excludes null, nonobjects and arrays. It establishes the source object's dynamic
member view, not the exact native storage type Record<String,Any> or an anonymous keyframe schema.
The current Record has its own shared storage; Any may instead hold a native object owner, an exact
externally boxed Record, or values reached through a JSON tree. external_if<Record<String,Any>>() is
honest only for that exact boxed type. Using it as the only accepted path, copying entries into a new
Record, adding an implicit Any conversion, or asserting SpineTintKeyframe/SpineParticleDocument would
reject valid dynamic objects or manufacture identity/schema evidence. The existing parsed-JSON request
already lists this module and its missing-key/nested-object carrier contract; this is its current
predicate-return form, rather than a new request or binding name.

Faithful emission must return the same object's checked dynamic member view and let absent members
read undefined. It must agree with the JSON.parse carrier at the root and the original object owners
at nested reads. A local helper annotation already declares Record, and a forward declaration cannot
replace this retained return expression. No bounded local runtime conversion or declaration supplies
that coherent carrier. No source/compiler/runtime prototype or normal generation is carried here.

### Validation, config and curve behavior required

- Empty object input is valid and uses defaults. Syntax errors keep the Invalid Spine particle JSON
  prefix and source Error/message handling; null/array/primitive roots use the separate expected-object
  error. After a valid root, validate tint, then alpha, then images before config mapping, even for
  the direct config-only entry point. Nonarray tint/alpha fields are ignored by these validators;
  every entry of an array must be a non-null, non-array object or throw the exact path-bearing
  Invalid Spine particle document error. Do not filter bad entries, validate only endpoints, relax
  nested arrays into objects or replace failures with empty results.
- readSpineImages returns a fresh empty list for a nonarray; an array must contain strings at every
  position, preserving order and duplicates. Any nonstring throws images[index] must be a string.
  The full document retains those image strings; this task adds no texture loading or native image
  binding. Missing properties must reach the source default path rather than Record.get(key).value()
  throwing. Existing emitted erased length/index reads and fixed-key .value() accesses remain inspected
  JSON/array barriers, not freshly measured next compiler diagnostics.
- Preserve the real createParticleEmitterConfig owner and its defaults. Lifetime/duration milliseconds
  convert to seconds; angles and spin degrees convert to radians. Direction is cos(midAngle),
  -sin(midAngle), and spread is half the authored angle range. Velocity remains pixels/sec, wind/gravity
  remain the corresponding average accelerations. Ranges accept only non-array objects and numeric
  low/high fields with the actual independent defaults; do not coerce, sort endpoints, impose finiteness
  or normalize negative ranges. maxParticles retains bitwise-or0. Continuous defaults true; duration is
  positive authored ms/1000 only when noncontinuous, otherwise0.
- For an ellipse, equal averaged spawn width/height produces circle with radius=width/2; unequal
  dimensions produce rect. Other source shapes produce the actual point fallback, while the document
  retains line/ellipse/point according to spineSpawnShape. ScaleEnd is averaged end scale divided by
  positive averaged start scale, otherwise0. Do not replace this with a different epsilon/clamp policy.
  Config blend accepts additive/multiply/screen/normal and returns null otherwise; the typed document
  falls back to normal. Keep those source distinctions rather than routing config through document.
- Endpoint tint uses first/last original entries, white fallback, and the exact hex conversion:
  remove the first #, pad right with f to six characters, parse each pair base16, use1 only for a
  non-finite channel parse. It is not a strict six-digit validator. Alpha endpoints default1/0 on
  missing/nonarray/empty input. Every endpoint still requires its real validated object.
- More than two tint/alpha entries bake complete curves through particleColorCurveFromKeyframes and
  particleCurveFromKeyframes using the existing ColorKeyframe/CurveKeyframe owners and default33 samples.
  Missing time uses i/(length-1); missing tint/alpha uses white/1. Shorter timelines retain null curves
  and first/last interpolation. Do not keep only endpoints, substitute null curves, or replace the real
  sorting/interpolation helpers. The document's keyframe readers preserve source order/strings/values
  and use their actual time default0, with distinct nonarray defaults and valid empty arrays retained.
- parseSpineParticle maps one parsed raw object with no intermediate document. The document entry point
  parses once, then evaluates config, diagnostics and document in that source order. SpineParseResult
  retains the real ParticleEmitterConfig, SpineParticleDocument and ImportDiagnostic-array owners.
  The document keeps its explicit name/ranges/authored units/tint/alpha/images/blend fields; no copied
  native schema is substituted for the dynamic raw object and no parser-only empty success is supplied.
- collectSpineDiagnostics emits Skip crumbs in source order for nonzero lifeOffset range, nonzero
  x/y ranges (one position crumb), and strictly true premultiplied. Origin is collectSpineDiagnostics.
  Numeric-only range checks and exact equality matter; preserve original diagnostic array and details.
  Multi-stop curves are supported by baking and do not gain obsolete unsupported-tint/alpha warnings.

The module explicitly omits parseSpineJson26 as cpp-instanceof-operand-unresolved and
readSpineImages102 as cpp-erased-array-predicate-runtime-required. The earlier section of this existing
request already records the JSON exception-carrier mismatch: Json::parse produces JsonValue and
JsonSyntaxError, not a source Error instance. [Erased-array reading](erased-array-element-reading.md)
records that AnyArray has exact identity-preserving extraction but no generic erased element view;
its old no-array-alternative refusal wording is stale, not permission to narrow this parser's domain.
[Retained-callee coverage](retained-declaration-calls-omitted-callee.md) applies to the direct public
parse calls and validate/document image calls. A declaration would not repair native102 or implement
syntax handling/image validation. No candidate, positive unused compile/link/run, qualified negative
link or newly measured next-error claim is made. Both source tests and serialization/re-import/curve
sampling expectations were read as requirements only; no native Spine particle execution is claimed.

**Recorded 0/1 pass, 1 fail is retained.** Only this existing request appendix, worklist128 and two
compact owned product lines (one root/one failed row) change. C++, particle/curve/schema owners,
runtime, source patches, repairs, generation inventory and pins are unchanged; unchanged C++ is not rerun.
Foreign compact products/worklist entries and historical/G1 totals remain intact. No fake parser,
empty-result substitute, copied owner, narrowed input or native parser/curve/serialization/link gain
is supplied or claimed.


## Private JSON read graph preparation

The private Pixi parse/config/diagnostic graph now has a bounded compiler preparation in
`compiler-private-json-read-flow.patch`. It retains the original parsed JsonObject/JsonArray owner
through `NamedProperties` only when the private producer performs one parse, applies its original pure
root guard, and every result reaches read-only private consumers without escaping. The public
PixiRaw alias and public/returned/callback/captured/compared Record ABIs remain their existing types.

An immutable local assertion to an unknown-field shape can retain `Any` along proven JSON paths.
It reads the same live owner rather than recovering a native anonymous struct. Direct nullish reads
throw TypeError; optional chaining skips null and undefined; primitive missing fields retain the
source defaults. Boxed strings expose their exact UTF16 own indices and length. Prototype operations,
concrete fields, captures, mutation and opaque forwarding do not enter this data-read lane.

Runtime prerequisites are integrated: Array.isArray recognizes original JSON carriers, the restricted
JSON data reader retains owner identity, and shared ToBoolean dispatches to the erased value's own
kind predicate. The new catch proof narrows only an already-string parse-only try whose binding is
observed solely through asserted Error.message. JSON parser diagnostic wording is implementation
dependent; no equality to V8's built-in syntax message is claimed.

The current compiler preparation builds on 79 preceding patches. Direct compiler controls retain
nine public/private ABI comparisons and reject twelve escaping/invalid producer shapes and eight
ineligible nested shape paths. Seven catch controls distinguish the syntax-only lane. The native
private fixture passes, including null acceleration throwing after parse while config-only parsing
succeeds. Runtime/core/structural and all three original Spine/DragonBones consumer regressions pass
6/6. Vitest is unavailable; the focused fixture is carried without a Vitest run claim.

The untouched full Pixi body emits in a diagnostic over ten packages and 1135 original sources.
After the normal duplicate-struct alias repair, both public entry points compile/link/run and match
the untouched pinned TypeScript parser plus actual factory, diagnostics and constants on 568 inputs
(1136 invocations), including all 52 config fields and diagnostic order. Signed zero and nonfinite
kinds are exact; finite fields are exact apart from directionX/Y's 1e-12 libm tolerance. The source
oracle verifies exactly one parse per call. Seven explicit test-host UnicodeService mappings cover
non-ASCII blend modes, contextual Greek casing and lone UTF16 units; no universal Unicode host
implementation is claimed. Built-in syntax/TypeError wording remains implementation dependent.

The comparison exposed parseInt's UTF8/ASCII whitespace trimming. The shared runtime now uses its
existing UTF16 leading-whitespace operation, with all 25 whitespace units and six non-whitespace
controls checked. No parser source or accepted input domain changed.

The preparation alone promoted no historical inventory row. Final normal generation and installed-header
qualification are recorded in the Pixi section above.


## Spine particle guarded read graph preparation

`compiler-private-guarded-json-read-flow.patch` extends the private read proof to the original
Spine particle parser. The two-statement `requireSpineRecord` guard returns its original value;
every actual caller must trace to the original one-parse producer or one of its JSON descendants.
Its result may only be ignored, passed to a proven private reader, or stored in an immutable
uncaptured local used solely for property reads. Exported, escaping, compared, aliased, mutated,
defaulted and mixed native/JSON callers retain the prior Record boundary.

Synchronous immutable local functions used only as direct callees can read the same private owner.
The exact `error instanceof Error ? error.message : String(error)` observation is accepted only in
an already-string parse-only catch. Guarded erased array elements retain their checked kind, and a
literal mapper with a closed mutation-free private call graph visits the original dense array.
Only the source-authored result array and fresh keyframe literals allocate. A fresh local range
literal chooses its one representation-equivalent contextual owner at its authored allocation;
it does not convert an existing object into a schema. Its high-before-low evaluation and exact
thrown owner are preserved. Stable numeric field tag checks supply the immediate bitwise/default
expression without an unchecked Any conversion.

The ordered 88-patch compiler stack applies, builds and emits the complete unchanged
original module byte-identically to the diagnostic compiler. Direct API checks pass nine prior ABI
comparisons, two guarded producer cases and fourteen exclusions, direct capture (one/four), syntax
fallback (one/four), indexed narrowing (one/one), fresh factory (one/one) and erased map (one/four).
An emitted native fixture checks fresh range owner identity, authored order/count, exact thrown owner,
and fresh mapped results retaining live original JSON element aliases. The carried Vitest fixture
has not run because Vitest is unavailable.

Both original public entrypoints compile, link and execute against the diagnostic header with the
normal duplicate-struct alias repair. They match the untouched pinned TypeScript parser, real
factory, curve and diagnostic bodies for 704 inputs and 1408 calls: all 52 config and 24 document
fields, 99,505 numeric reads, 6,408 exact UTF16 strings and 268 curve arrays. Each source invocation
parses exactly once. Signed zero and nonfinite kinds are exact; finite values are exact except
directionX/Y's 1e-12 libm tolerance. Application validation messages and ordering are exact;
built-in JSON syntax diagnostic wording remains implementation dependent.

This is preparation evidence. Normal scoped generation, installed public consumer qualification
and the four requested header measurements are pending. No inventory row or historical25 gain is
claimed. The independent Unity, returned schema, Lottie, Aseprite, TexturePacker and glTF nominal
JSON boundaries remain open.


The closed mapper proof also excludes assertions, arbitrary indexed observations and opaque
property reads. A property can invoke a getter outside the analyzed private call graph; only the
ambient Array predicate and data reads on immutable locals from a selected original JSON producer
are admitted. Two getter controls and an indexed-reader control retain refusal. The first normal
run was stopped before inventory writes to add this exclusion; both declared patch sets recovered
cleanly. Its partial emission is not qualification evidence. The original Spine module stays
byte-identical; corrected final normal generation remains pending.

## Final original Spine particle qualification (2026-10-18)

The original installed `spine_parse.hpp` compiles, links and runs both public entrypoints. The
registered CMake/Bazel consumer checks defaults and fresh owners, independent numeric range fallbacks,
authored document units and keyframe order, exact validation paths/order, UTF16 image strings and
duplicates, diagnostics, bitwise conversion, nonfinite values and real sorted multi-stop curves.
A fresh installed-header driver matches the untouched pinned parser, factory, curve and diagnostic
bodies on **704 inputs / 1408 calls**, all **52 config and 24 document fields**, 99,505 numeric reads,
6,408 exact UTF16 strings and 268 curve arrays. Each source invocation parses once. Signed zero,
NaN/Infinity kinds and application messages/order are exact; finite values are exact except
directionX/Y's relative/absolute 1e-12 libm tolerance. Built-in syntax wording is implementation dependent.

Normal generation with 89 compiler patches first supplied the complete parser and retained all 65 existing definitions
across its four changed headers, adding the real parse/image algorithms and removing exactly their
two omission markers. That normal product exposed shared RegExp capture regressions in formerly
passing ParticleDesigner/Starling code. Its initial four-header result was 1 pass / 3 failures at
ParticleDesigner's line 104 `Array.includes(optional capture)`; Starling's optional `trimEnd` was also
inspected. This intermediate result was held as partial. Builder5 qualified the shared exact-value
optional search and checked String trim projection; no parser rewrite or generated repair was used.

The required final normal scoped reconciliation ran on frozen `12c2c3f3` with **92 compiler /
293 total patches**, exited zero and restored both pinned checkouts clean. It emitted 1135/1141
closure modules across ten packages and retained the particle slice at **16/18 emitted, 2 refused**.
Only one Starling header and three total paths changed relative to that 89-patch product; the complete Spine header
is unchanged. Frozen input hashes match. The original installed **Spine, ParticleDesigner and Starling
consumers pass 3/3 CTest**, so both capture regressions are cleared. Source-patch automation passes 2/2
and package labels pass 1/1. The manifest preserves the measured mixed binding-profile provenance;
no unchanged Pixi/counter SDK or global sweep was repeated.

The complete Clang 22.1.8 `-j2` check of exactly the four assigned headers is **1 pass / 3 failures,
0 unattempted**, in 203.1 seconds. Spine passes. `_internal_index`, `contract` and
`register_built_in_particle_formats` now first fail at the pre-existing Unity
`NamedProperties` to `Record<String,Any>` return at line 77. The old Spine cause `31b754c5` is retired;
the existing Unity cause `c287f892` holds its own row and these three callers. No Unity consumer,
serialization, whole-package or all 25 completion is claimed. The Spine source row is outside the
original historical 25, so this unit adds **zero** promotions to that set; the prior Pixi promotion
remains distinct. Other nominal Lottie/Aseprite/TexturePacker/glTF document boundaries remain open.

The original parsed graph remains live through private views, with no Record/tree copy, nominal
reconstruction, schema gate, unchecked cast, public Record ABI change or input narrowing. The document
and keyframes allocate only at their original authored fresh literals. Private proof controls retain
nine prior ABI comparisons and exclude guarded escapes/mixed callers, unsafe captures/catches and
opaque getter/index observations. The seven mapper exclusions and emitted native order/count/exact
thrown-owner/live-alias checks pass. The first attempted run was stopped before inventory writes
to add that getter exclusion, and its patches recovered cleanly; its partial emission is not evidence
of a successful generation. The carried own Vitest fixture was not run because Vitest is unavailable.

## Unity preparation: private Record forwarding (2026-10-18)

The next bounded Unity unit starts from the integrated Spine product and its qualified capture floor.
The original `rawToConfig` reads its Record and forwards it to private `unityBlendMode`, which only
reads it. The earlier proof accepted direct property/index receivers, so this forwarding kept
`rawToConfig` outside the private read lane and prevented the original `parseUnityJson` result from
retaining its parsed owner as a view.

The declared `compiler-private-record-forwarded-reads` patch grows selection from established private
readers through exact arguments to those readers. Every use must be a read or a proven forwarding
argument. The least fixed point rejects a recursive forwarding cycle with no established reader.
Exported signatures, returned owners, aliases, comparison, mutation, escaped callbacks/closures and
optional/defaulted parameters retain their prior ABI or refusal. No tree/Record copy, nominal
reconstruction, schema gate, input restriction or algorithm replacement is supplied.

The strict production compiler build on the full 96-patch floor plus the exact change passes.
Direct production API controls pass three positives and thirteen byte-or-refusal-equivalent excluded
cases. A compiled native control passes sixteen checkpoints for the public Record ABI, live native
and parsed owners/aliases, missing versus present undefined/null, signed zero/NaN and original JSON
root/syntax failures. Existing Spine, Pixi, ParticleDesigner and Starling diagnostic-context headers
remain byte-identical to their baseline; no SDK was repeated. The focused compiler Vitest fixture is
carried but was not executed because Vitest is unavailable.

This is preparation only. The original Unity diagnostic now returns `NamedProperties`, while
`rawToConfig`, `collectUnityDiagnostics` and `readEmission` still have their original erased-array or
optional-member refusals. Installed Unity execution, the four assigned headers and inventory promotion
remain unqualified; no historical 25 gain is recorded. The next step is original-owner array/member
lowering and a complete original public consumer, preserving all earlier Spine/capture results.

### Unity continuation: private JSON Record graph

The next compiler preparation is `compiler-private-json-record-graph-reads.patch`, composed after the corrected 105-patch floor. It retains original JSON descendants as `Any` through immutable private Record aliases, exact `Array.isArray(path) ? path as Record[] : []` aliases, guarded numeric reads/length, and closed synchronous fresh-output maps. Color aliases preserve `k.color ?? k` with one lazy nullish tag test. Only source-authored empty literals, map output arrays and callback object literals allocate owners. Public/escaping/callback-visible/mutating/mixed-native flows remain excluded; unsupported selected maps produce a structured refusal.

`keys` is not globally admitted as a JSON data property. The narrow observer proof requires the local initialized by `(provenJson as Record).keys`, the immediately following `if (!Array.isArray(local) || ...) return null`, and only dominated length/index reads thereafter. Thus an inherited array `keys` function and an absent own key are rejected by the same first predicate before anything can observe their difference. A `typeof`, truthiness, return, opaque call or use before that guard rejects this proof. There is no fabricated function or schema filter.

Guarded string extraction keeps the original `typeof x === 'string'` branch and calls the existing `String::to_lower`. Its Unicode capability remains material: without `UnicodeServiceScope`, `{"blendMode":"İ"}` and a lone-surrogate blend string raise `UnicodeServiceUnavailable` in the current runtime, while the original TypeScript returns null blend. No approximation or input rejection hides this gap. In a diagnostic native consumer, all 900 untouched-source inputs / 1,800 public invocations match with a finite-input Unicode test provider; service-free comparison matches 1,796 invocations and exposes those four capability failures. The comparison covers all 52 config fields, 17 actual document fields, nested presence, diagnostics and curves (126,480 numeric reads, 11,914 UTF-16 string values, 360 curve arrays). This provider verifies delegation for the tested strings; it is not a complete Unicode runtime implementation.

Corrected105 baseline/candidate strict production builds and production API controls pass: five positive flows, sixteen exclusions and five unsafe mapper refusals, plus the prior private-reader/guard/capture/syntax/index/factory/map controls. A carried focused compiler fixture records these boundaries. Native evidence uses the complete original diagnostic module and the repository's normal declaration-only owner alias repair, with no parser-body edits. Normal scoped SDK generation, actual installed consumer and the four header results remain pending; this preparation claims no product/header or historical-corpus gain.

The carried fixture's 28 cases also execute through a dependency-free `node:assert` adapter against the production compiler API; the Vitest runner is unavailable here, so this is not a Vitest execution claim. The final declaration patch applies as the 106th compiler patch to a clean pinned extraction and reproduces the tested backend and fixture bytes exactly. Retained diagnostic modules are checked separately: ParticleDesigner and Starling are byte-identical; Spine's two guarded member projections and Pixi's unused anonymous declarations change. Their actual consumers therefore need checking with the one combined particles generation; no separate unchanged-parser SDK is requested.
