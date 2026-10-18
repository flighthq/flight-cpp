# A TypeScript `as` on `JSON.parse` is emitted as a `static_cast` between unrelated types

## Reproduction

`flight/scene2d_formats/lottie_document.hpp` at `806d257`, line 324, from
`packages/scene2d-formats/src/lottieDocument.ts`:

```cpp
static_cast<flight::Ref<flight::types::LottieDocument>>(flight::Json::parse(std::get<1>(source)))
```

`flight::Json::parse` yields a `JsonValue`; `flight::Ref<LottieDocument>` is a reference to a declared
row type. The two are unrelated, so the cast does not compile:

```
error: no matching conversion for static_cast from 'JsonValue' to
       'flight::Ref<flight::types::LottieDocument>'
```

The source writes `JSON.parse(text) as LottieDocument`, a TypeScript assertion over a value that came from
`JSON.parse`, and the emitter lowers the assertion as a C++ cast.

## Required shape

An assertion between unrelated types is not a cast. Either materialize the document from the parsed value
through the declared owner the source asserts (the same path the format importers already take), or refuse
the assertion with the rule that names it. Emitting `static_cast` is the one answer that is neither.

## Affected

15 headers in the G1 sweep, all in `scene2d-formats` and the `scene2d-resources` importers that include it.
Once the `SequenceView::filter` runtime gap was closed this became the first diagnostic for all of them,
which is why it is filed separately from `local-reexport-using-before-declaration.md`.

## What this repository carries meanwhile

Nothing: the cluster is open. `SequenceView::filter` was added to the runtime because emitted code calls it
and the runtime genuinely lacked it; this one is an emitter decision and is left to the request.

## G2 Aseprite document union

The same lowering defect is the first failure of four `spritesheet-formats` headers in G1 group
`other-4e22e4de`, at compiler `806d257` / Flight `a627849`. The destination here is a value variant
of two document references, rather than Lottie's single reference. Both
`parseAsepriteSpritesheet` and `parseAsepriteSpritesheetDocument` write:

```ts
document = JSON.parse(json) as AsepriteDocument;
```

The first emitted cast is `flight/spritesheet_formats/aseprite_parse.hpp:144`; the document-returning
entry point repeats it later in the same header:

```cpp
document = static_cast<flight::types::AsepriteDocument>(flight::Json::parse(json));
```

`flight::types::AsepriteDocument` is exactly
`std::variant<Ref<AsepriteArrayDocument>, Ref<AsepriteHashDocument>>`. Array documents retain an
ordered array of frames with filenames; hash documents retain named frame entries. Both retain the
same schema's metadata, including numeric-or-string scale, tags, optional layers and optional slices.
`Json::parse` already returns a real JSON tree with distinct null, boolean, number, string, array and
object cases and checked kind accessors. That tree is neither of the generated document owners;
no cast can choose an arm and recursively populate its fields.

The public contract prevents treating this as just an atlas geometry conversion.
`parseAsepriteSpritesheetDocument` returns both mapped spritesheet data and the original parsed
document. Its source tests explicitly preserve the hash/array distinction and layer metadata.
`documentToData` also reads frame durations and animation tags. The existing native
`textureatlas_formats::detail::parse_texture_atlas_aseprite_json_checked` returns a `TextureAtlas`:
it extracts region geometry and page image/size/scale, without retaining the original document,
frame durations, tags or layers. It cannot supply this API's result without additional, faithful
document materialization. Returning an empty document or only that atlas would discard required data.

The source's `try` surrounds JSON parsing and the erased TypeScript assertion, then reports
`spritesheet.aseprite.malformed-json` with Reject severity on syntax failure. The document-returning
sibling also supplies the real frame-less array-document sentinel. There is no schema validator in
that assertion; inventing one that turns valid JSON into this malformed-JSON fallback changes when
the catch runs. A generic permissive `JsonValue` conversion, unconditional selection of either
document arm, or a parser stub therefore cannot repair the emitted cast.

The existing required shape applies: lower the asserted document through its actual represented
owner/union contract with the source behavior intact, or refuse the assertion explicitly. The
separately omitted `documentToData` also has a recorded contextual-union refusal when passing this
document to the atlas schema; fixing the cast alone is not evidence that the parser works.

No C++ runtime, binding, source patch, declaration repair or override changed for this disposition.
The four owned rows remain **0 pass / 4 fail**, retained from G1 and now linked here with compiler
route; no unchanged native rerun or new behavior improvement is claimed. The Lottie group and other
headers retain their existing counts and attribution.

## G2 Pixi particle parser: the destination is a dynamic record

The original 0/4 disposition below is historical. Final normal 82 patch qualification now promotes Pixi
and moves its three barrels to the existing Spine owner cause; see the final Pixi section below.

At compiler `806d257` / Flight `a627849`, `other-d6be4352` covers four particles-formats headers:
`pixi_parse.hpp`, `register_built_in_particle_formats.hpp`, `contract.hpp` and `_internal_index.hpp`.
Their retained first failure is `pixi_parse.hpp:55`:

```cpp
using PixiRaw = flight::Record<flight::String, flight::Any>;
// raw is flight::JsonValue, already checked as a non-null object rather than an array.
return static_cast<PixiRaw>(raw);
```

This is the same assertion/cast defect with a dynamic Record destination, rather than Lottie's
nominal document or Aseprite's document union. `parsePixiJson` parses once, wraps syntax failures in
the format-tagged JSON error, then separately rejects null, arrays and non-object roots. Its final
`raw as PixiRaw` is an erased assertion, not a constructor or an additional schema check.

No config field is mandatory: the pinned source and tests require `{}` to use finite defaults.
The mapping reads optional lifetime/angle bounds, numeric or `{value}` entries in start/end pairs,
color fields and spawn-shape payloads. It also preserves frequency-to-rate conversion, clockwise
screen-space angle mapping and blend-mode choices. The document entry point feeds the same parsed
raw object to config mapping and ordered diagnostics for burst, polygon, acceleration and behaviors;
it returns config plus diagnostics, not the original raw document.

The already-filed [parsed JSON object-shape request](parsed-json-object-shape-reads.md) covers this
exact Pixi path and its additional barriers: anonymous nested shapes are recovered with
`Any::object_if<T>()` although parsed JSON never held those C++ structs, and missing record keys
are unconditionally unwrapped instead of reading as undefined. A permissive conversion for the
outer record alone would leave those failures, including the valid empty-object case. Replacing the
parser with defaults would lose both authored values and diagnostic behavior.

No runtime, binding, source patch, declaration repair or override changes for this disposition.
The four owned rows retain **0 pass / 4 fail** from G1 and now name this request plus the existing
shape-read request. No C++ input changed, so there was no native recompile, parser parity claim,
global sweep or upstream test port. Other JSON assertion groups keep their own counts and attribution.

## G2 TexturePacker retains its original document as well as spritesheet metadata

At compiler `806d257` / Flight `a627849`, original cluster `other-acb4b229` contains
`spritesheet_formats/texture_packer_parse.hpp`. Its completed Clang22.1.8 one-worker check is
**0/1 pass, 1 fail, 0 unattempted**. Both public parser bodies assert
`JSON.parse(json) as TexturePackerDocument`; the first emitted cast fails at line128, and the
document-returning entry point repeats it at line145:

```cpp
document = static_cast<flight::types::TexturePackerDocument>(flight::Json::parse(json));
// TexturePackerDocument = variant<Ref<TexturePackerHashDocument>, Ref<TexturePackerArrayDocument>>
```

`JsonValue` holds the real JSON tree, with separately shared JsonArray/JsonObject owners and checked
kind accessors. It never held either generated nominal document struct. No runtime constructor,
external binding or erased assertion can reinterpret that object into either Ref owner, select the
correct array/hash arm and populate all of its nested fields. A permissive Any conversion or an
unconditional choice of one arm would hide this representation gap.

This API cannot be supplied by returning the existing native atlas parser's geometry. The source
`documentToData` delegates geometry to `parseTexturePackerAtlasDocument`, then maps regions into
frames: upright dimensions swap for rotated rectangles, trim offsets and original sizes are retained,
missing pivots remain null, and names retain region order. It separately derives animations
from `meta.frameTags`, using each inclusive from/to range, direction, name, 100ms frame duration and
repeatCount -1. Image name/size and the original numeric-or-string scale come from this same document.
The document-returning sibling returns that original document together with the mapped data. Source
tests require both array/hash variants and retained app/version/format fields, not an empty sentinel
on a valid input or a newly reconstructed atlas-shaped document.

The existing `textureatlas_formats::detail::parse_texture_packer_atlas_json_checked` populates an
atlas and returns it. It neither returns the parsed document nor retains frameTags or the additional
spritesheet metadata. Its finite-positive atlas scale normalization also differs from this source's
`metaScale`: numeric scales pass through, while string scales use `parseFloat(scale) || 1`.
Using the atlas's normalized scale would change zero/negative/non-finite or fallback behavior.

The source try/catch encloses parsing and the erased assertion only. Syntax failure appends one
Reject `spritesheet.texture-packer.malformed-json` diagnostic with the appropriate public function
name and returns empty data; the document-returning sibling also returns the real frame-less array
sentinel with blank metadata, zero size and numeric scale1. Valid JSON is not schema-validated by the
assertion. A new conversion must not turn later shape/mapping errors into this syntax fallback or
report every input as malformed. Preserve document identity/shape, nested data, ordering and the
source catch boundary through an explicit represented JSON/document contract, or refuse the assertion
coherently. A copied view, fake parser or narrowed accepted domain is not carried.

The independently refused `documentToData` still crosses the
[parallel schema union boundary](parallel-schema-union-structural-conversion.md#g2-texturepacker-document-converter-is-still-omitted).
A declaration for that helper cannot repair either invalid cast and is not a parser implementation.
No C++, runtime, binding, source patch, repair, override or generated inventory changes for this
disposition, so no changed generation needs reproducing. Only this original header was checked; its
row remains failed with no native parsing, round-trip or functionality pass claimed. The earlier
Aseprite, Pixi and Lottie rows and historical totals retain their own attribution.

## G2 glTF feature collection keeps the JSON and document input domains

At Flight `a627849` / compiler `806d257`, original `other-4703cbbe` contains one header,
`scene3d_formats/gltf_features.hpp`. Clang22.1.8 jobs1 completes **1 attempted / 0 pass / 1 fail /
0 unattempted**,7.8s, at line59:

```cpp
doc = static_cast<flight::Ref<flight::types::GltfDocument>>(
    flight::Json::parse(std::get<1>(source)));
```

The source collectGltfFeatures accepts GltfDocument|string and asserts JSON.parse(source) as
GltfDocument. All three public functions are emitted; there is no omitted helper or refusal marker
whose forward declaration could repair this cast. Json::parse already supplies a real JsonValue
tree with checked kinds and shared JsonArray/JsonObject owners. It did not allocate a nominal
GltfDocument. The actual generated document is a ReferenceEnabled struct containing optional asset,
arrays of references to schema owners, extensionsUsed/Required and document extensions. Its asset
uses the canonical version_fe118d6bc798d137 owner with String version. Neither that owner nor a
GltfDocument reference exists inside the parsed tree to recover with a cast or Any::object_if.

This is feature inspection, not scene/resource import. GltfBuffer carries byteLength and optional uri;
GltfBufferView carries its buffer index, byteLength and optional offset/stride; GltfImage retains either
uri or bufferView plus optional MIME type. The collector does not read/fetch those resources, decode
images, validate meshes or construct a scene. It accepts partial entries such as meshes:[{}] because
it only inspects collection length. A full importer, fabricated resource owners or a copied feature-only
document is not an equivalent replacement for either original input arm.

The complete source contract requires:

- Parse a string once, with the try/catch enclosing JSON.parse and its erased TypeScript assertion.
  That assertion performs no schema validation. Otherwise read the original document object. After the catch, return
  null for a null/non-object root or a nonstring asset?.version; any string version, including empty,
  passes. Do not impose glTF version2.0, require resource fields or route later access/iteration errors
  into the syntax-failure return. The retained emission folds the root null/type guard to false and
  replaces the version typeof check with optional presence; fixing only the cast does not restore
  those dynamic checks for JSON null/primitives or wrong-typed asset/version fields.
- Allocate a fresh Set for each accepted document. Test meshes, animations, cameras and skins through
  the source optional length/nullish-zero/>0 expressions, adding Mesh, Animation, Camera, Skin in that
  order. No entry traversal is required. There is no Array.isArray guard: JSON strings and objects
  with a length member still follow JavaScript property/comparison semantics. A strict arrays-only
  projection or conversion that requires complete native mesh/camera/skin owners narrows this input.
- If extensionsUsed is not undefined, iterate it in source order and add each value to the same Set,
  retaining source Set insertion/deduplication semantics and existing objects. Do not filter against
  GLTF_FEATURE_NAMES, sort, infer from extensionsRequired/extension payloads, or coerce entries.
  The public feature-name list is Animation,Camera,Mesh,Skin; it differs from collection insertion
  order. The unchecked JSON assertion also admits wrong-typed extension values: strings are iterable,
  and null/noniterables can throw after the parse catch. A Set<String>-only nominal conversion must
  not silently stringify/drop values or turn such errors into null. The valid minimal document returns
  an empty Set, while invalid roots return null; universal empty success is not feature collection.
- collectGlbFeatures first uses extractGlbJsonChunk, returning null when no chunk is found, then uses
  the same JSON collector. Extraction reads the exact Uint8Array buffer/byteOffset/byteLength view.
  Require at least12 bytes, little-endian glTF magic and GLB header version2; use
  min(declaredLength,view.byteLength), not a new strict equality check. Starting at12, require a full
  8-byte chunk header and a payload within that end. Skip unknown chunks by their declared size and
  return the first JSON chunk decoded by TextDecoder from that subarray, retaining padding/decoder
  behavior. Do not require a BIN chunk or resource availability, assume zero view offset, impose new
  alignment checks, or skip a malformed first JSON chunk in favor of a later one. This extraction
  returns decoded text; document/feature checks belong to the collector afterward.

The existing [parsed JSON member-shape request](parsed-json-object-shape-reads.md) covers nested
owner/absence reads that a nominal cast cannot establish. A faithful compiler/runtime boundary must
read the original JSON through a represented document/view contract with kinds, optionality, identity,
Set values and exception boundaries preserved, or explicitly refuse the unsupported assertion. Adding
a permissive JsonValue conversion, changing JSON.parse's binding, copying a document or disabling the
string/GLB arm cannot make that contract true. No bounded faithful runtime/binding/source fix is carried.

Pinned source tests cover invalid strings/null/primitives, minimal/empty-array documents, each feature,
extensions and GLB header/truncation/JSON selection. They are requirements, not native parser/feature
behavior results in this unit. No C++, runtime, binding, source patch, repair, override, manifest or
inventory change is carried, so no changed normal generation needs reproducing. The owned root/row
remains failed with compiler route. No native feature, parser, resource, header or functionality gain
is claimed; earlier JSON cases and all foreign glTF ledger/edge corrections retain their evidence.

### Original JSON owner read views (2026-10-17)

The first applicable public consumer is `collectSpineJsonSectionCounts` in
`skeleton2d-formats`. Its source keeps one `JSON.parse` result, rejects a null or
non-object root, asserts a string-keyed unknown Record for reads, and passes that
same owner to readonly helpers. Arrays remain objects in the source's domain.

The runtime now exposes the parsed container's original shared owner to `Any`
and `NamedProperties`. Reading a member boxes just that value; nested objects and
arrays retain their original owners. Readonly views observe later alias updates,
missing properties read undefined, and own integer keys precede other keys.
JSON array views enumerate present indices, retain nonenumerable `length`, and
support the erased array reads without rebuilding an `Array<Any>`. A native
`Record<String, Any>` can supply the same live readonly read contract over its
existing shared storage.

`compiler-readonly-json-record-view` selects that implementation carrier only for
private helpers referenced exclusively by direct calls, with nonoptional readonly
unknown Record parameters used exclusively as property/index receivers. Public
functions, aliases, returned or compared owners, forwarded parameters, and helpers
exposed as callbacks keep their existing Record contracts. This avoids a global
readonly Record ABI change. Selected reads return bare `Any`, whose own tags
answer null/undefined tests. Nullish coalescing evaluates its
left side once and preserves the lazy source fallback, including
`anim.drawOrder ?? anim.draworder`. Literal arguments still construct the original ordinary Record once; the private
reader retains that existing storage through a view.
Ordinary mutable and concrete-valued Record carriers remain unchanged; the
separate typed Record missing-key patch composes with this route.

The patch also recognizes a no-binding catch around one built-in `JSON.parse`
assignment from an already-string binding or literal and catches
`JsonSyntaxError` there. It does not catch unrelated native resource failures.
Other catch bodies retain their original lowering.

This is a dynamic read capability, not evidence that `LottieDocument`, Aseprite,
TexturePacker, or glTF nominal document carriers are represented. No schema
validation, copied Record/document, replacement parser, or unchecked nominal
owner assertion is supplied. Historical applicability remains 25/25; the two
current related follow-on rows are tracked separately. Header and consumer gains
must be measured after normal scoped generation rather than inferred from this
runtime capability or a compiler fixture.

Valid JSON inputs also include overflowing/underflowing decimal numbers and lone
UTF-16 surrogates. The parser now preserves UTF-16 code units directly instead
of rejecting isolated escapes or replacing literal surrogates through UTF-8.
Dynamic JSON and native Record readers enumerate and look up exact String keys,
so distinct surrogates and U+FFFD remain distinct live properties. Decimal grammar
validation remains strict; valid range loss produces signed infinity or signed
zero, while finite/subnormal values retain the standard correctly rounded parse.
Malformed numbers and escapes still raise `JsonSyntaxError`.

Preparation verification: the complete ordered 73-patch compiler builds normally;
nine same-stack controls preserve the existing public, alias, escape, callback,
comparison, capture, export-alias and optional Record contracts. Runtime JSON,
core and structural-row tests pass, as do the four touched standalone public
headers. The archived diagnostic generated original counter matches the untouched
pinned TypeScript body and actual registry constants in 188 fixed/seeded cases,
including the new numeric range and surrogate-key cases, with exact Map order and
one source parse per invocation. That artifact is not the final public gain:
the final normal generation must include the atomically landed corrected typed
Record patch and preserve the JSON counter, DragonBones missing-key counter and
Spine clamp consumers together. No broad readonly ABI prototype output is carried.

### Final original Spine counter qualification (2026-10-18)

One final normal `npm run pkg:generate -- skeleton2d-formats --progress` ran
on the delivered `ff634a55` floor with 74 ordered compiler patches, including
this bounded private-reader patch, corrected `compiler-typed-record-missing-key`,
and `compiler-optional-record-literal-materialization`. It exited zero: 1,321
of 1,342 closure modules emitted across 18 packages; the requested slice remains
50/59 emitted with nine honest refusals. Ten normal headers plus its manifest
changed. Both pinned sibling checkouts were restored clean. Subsequent unrelated
nullable-element compiler commits were reconciled without changing this emitted
artifact; this is a measured 74-patch run, not a claim of regenerating later inputs.

The actual original public counter now compiles, links and runs through
`flight/skeleton2d_formats/spine_json_section_counts.hpp`. The new registered
CMake/Bazel test covers 28 fixed boundaries, a fresh Map per valid invocation,
and distinct literal UTF-16 surrogate/U+FFFD keys. A fresh driver using the final
product include tree matches the untouched pinned TypeScript body and actual
registry constants in all 188 fixed/seeded cases with exact Map order; the source
oracle verifies exactly one parse per invocation. Numeric overflow/underflow,
missing/null/wrong-typed fields, array/object sections, DragonBones rejection and
the lower-case `draworder` fallback are included. The native literal UTF-16 case
is checked directly, avoiding a lossy UTF-8 test transport. This is bounded
coverage rather than an exhaustive source-domain claim.

All three required original consumers pass together: Spine JSON counter,
DragonBones missing-key counter, and Spine clamp/parse helpers. The runtime/core/
structural-row controls also pass (6/6 CTest total). All 227 override derivations
match; the Spine generated hash remains `67479d32dce9cd72f1d07788ba1797b224c7121dc22a752b07dbc88afbd68cc2`,
so its re-derived override required no edit or hash bump. Direct package labels
and source-patch automation pass. Nine same-stack public/alias/escape/callback/
comparison/capture/export/optional ABI controls pass on the final backend, whose
source was byte-checked against the actual running normal compiler. The first
consumer build needed the existing local SDL/GLES include paths added to its
runtime-only build directory; no dependency install or source workaround was used.

The complete -j2 standalone check of these ten changed headers is **3 pass /
7 fail**, with none unattempted. DragonBones parse and section-count headers keep
their prior passes; the Spine JSON counter is the one new pass. Other failures
remain public named-view/Record carriers, the existing nullable-array slot,
omitted `censusDragonBones`, the transform preset owner, and callback carriers.
In particular `spine_parse.hpp` now reaches its public
`SpineJsonSectionContext.doc` initializer at line 151, where the original named
JSON owner cannot become an owning Record. It remains failed as `no-viable-conversion-c35940ab`.
The old two-row `no-matching-function-b37ddf91` owner-read failure is retired
by one pass and this explicit follow-on. Global historical sweep totals were
not rerun or recast as current measurements.

This counter is outside the original historical-25 assertion inventory: **zero
historical-25 rows are promoted by this unit**. Their 25/25 applicability and
the current 27 related rows remain distinct; nominal document owners still need
their faithful represented contract. No parser body replacement, parsed tree/
Record copy, schema gate, nominal cast, global readonly ABI change, source-domain
narrowing, refusal suppression or Canvas work is carried.

### Final original Pixi qualification (2026-10-18)

Normal particle-format generation on frozen `f91a5bc8` with 82 compiler / 283 total patches exited zero:
1134/1141 closure modules emitted; target 15/18 emitted with 3 honest refusals. The installed original
Pixi config/document entry points compile/link/run and match the untouched pinned parser, factory,
diagnostics and constants on 568 inputs / 1136 calls across all 52 config fields. The oracle checks one
parse/call, exact signed-zero/nonfinite kinds, application root messages and diagnostic order; finite
fields are exact except directionX/Y libm tolerance 1e-12, and built-in error wording is implementation
dependent. The CMake/Bazel consumer covers source defaults, authored nested values, error boundaries,
Unicode whitespace/casing and literal UTF16 units. Existing affected consumers pass 5/5; nine complete
ABI controls pass on the actual normal backend.

All 13 changed headers plus 2 unchanged Pixi barrels were attempted:10 pass / 5 fail. The original four-row
Pixi cause retires through 1 pass and 3follow-ons to `require_spine_record` Any→Record at 103. Unity still
fails its NamedProperties→Record return at 77. The9 other passes were already passing. Thus this unit
promotes exactly 1 historical-25 row; the prior Spine counter promoted 0 because it was outside 25.
All 25 are applicable; current 27 related rows, other nominal-document causes and package completeness
remain separate. Global sweep totals are unchanged. The
[full Pixi qualification](parsed-json-object-shape-reads.md#g2-current-pin-pixi-inventory-qualification)
records carrier proofs, source coverage and limits. No parser/tree/Record copy, schema gate, nominal
owner fabrication, public ABI change, domain narrowing, override/hash bump or Canvas work is carried.

## Capability analysis at the current floor (G2)

The emitted shape has moved since the reproduction above, and the remaining gap is now specific. At
`3a3c5b1`-era generation the function is:

```cpp
inline std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::LottieDocument>>>>>
parse_lottie_document(std::variant<flight::StructuralRef<..LottieDocument..>, flight::String> source) {
  if (!(source.index() == 1)) return std::optional<...>{std::get<0>(source)};
  try {
    return std::optional<...>{flight::structural_ref_cast<...>(
        flight::StructuralRef<...>(static_cast<flight::Ref<flight::types::LottieDocument>>(flight::Json::parse(std::get<1>(source)))))};
  } catch (...) { return std::nullopt; }
}
```

The outer carrier is already the declared structural row -- only the innermost `static_cast<Ref<T>>(JsonValue)` is
unrepresentable.

**What the runtime already has.** `include/flight/structural_ref.hpp` defines `JsonRowOwner<JsonObject>` and
`JsonRowOwner<JsonArray>`: a `RowOwner` over the parsed JSON object whose `named_cell` reads a member live and
whose `enumerable_string_keys` reproduces own-key order. `owner_for<JsonObject>` constructs one, and
`named_properties(const JsonValue&)` exposes it. So a JSON object can back a structural row.

**Why a JSON-backed row is not sufficient here.** The asserted type's members are *typed*, and the row reads
them as such: `is_valid_lottie_document` emits `std::isfinite(flight::row_get<flight::RowKey<"fr">>(document))`
and `flight::is_array(flight::row_get<flight::RowKey<"layers">>(document))`. `LottieDocument` declares
`double fr; ... flight::Array<flight::Ref<LottieLayer>> layers;`. A JSON cell is a `ReadonlyNamedCell` with
`value_type() == typeid(Any)`, while `RowOwner::named_value_if<T>` returns a value only from a `TypedRowCell<T>`.
A scalar member could be projected on demand (`Any` -> `double`), but `layers` needs a `flight::Array` of
`Ref<LottieLayer>` owners, and every layer's own members are typed the same way: that is recursive
materialization of the declared owner graph, not a cell projection.

**What materialization has to decide.** `JSON.parse` can yield `null` (a successful parse), and JavaScript then
throws `TypeError` when a member is read on it -- outside the `try`, so it propagates. A null-valued *required*
member (`fr: null`) is not representable in `double fr` at all; the current `isValid` happens to answer false for
both `null` and `0`, but that is a coincidence of this predicate, not a rule. A faithful materializer therefore
needs a stated policy: which JSON values map to an absent optional, which throw, and which are unrepresentable
and refused by name.

**Recommendation.** This is a capability, not a lowering: either a compiler-generated JSON -> declared-owner
materializer for an asserted type (recursive over its schema, with the null/absence policy above), or a
JSON-backed row whose typed reads project a scalar and refuse a structured member unless a nested row is
requested. It covers the 15 Lottie rows, the 2 `scene2d-resources` importers, and the 4 Aseprite rows in
`other-4e22e4de`. A per-site source patch cannot express it, and a cast or a hand-written mapper would carry a
copy of a schema the compiler already knows.

## Required-scalar mismatch: options and counterexamples

Pinned counterexamples, all in the bounded Lottie unit (`lottieDocument.ts:288,591-604`,
`lottieCounts.ts:54-55`):

- `fr: 30` — division and `isValid` work; this is the supported case.
- `fr: null` — `document.fr` is `null`; `(op - ip) / null` is `±Infinity`/`NaN`, and
  `typeof fr === 'number'` is false, so `isValid` answers false. A typed `double fr` cannot store
  `null`; coercing it to `0` happens to keep `isValid` false but changes the division result.
- `fr` absent — `document.fr` is `undefined`; `typeof` false and the division is `NaN`. A typed cell
  must represent absence, not a default `0`.
- `fr: "30"` (wrong type) — `typeof` false so `isValid` false, while `(op - ip) / "30"` coerces
  numerically to the same 30. A strict double projection and a coercing one disagree at use, not at
  the read.

Options:

1. Lazy typed projection with a presence-aware scalar cell: `null`, absence and wrong type are
   retained distinctly; numeric/boolean operations follow JavaScript at the use; structured members
   are refused unless a nested projection is requested. Partial, well-typed projections are marked
   explicitly. Smallest faithful option; reuses the existing member-value/view family (one family,
   shared with builder2).
2. Throw or refuse at the member read. Simple, but it changes the source's numeric coercion and
   `isValid` answers (the `"30"` case), so it cannot be the universal rule.
3. Runtime-wide scalar carrier: every typed cell uses a JS-numeric value with nullish/coercion
   semantics. Broadest; only if the runtime intends JavaScript coercion globally, and it must still
   keep null-vs-absence and access-time errors.

Decision requested from Foreman/Manager before implementation: 1 with an explicit
null/absence/wrong-type policy (recommended), or 3. No coercion or runtime-wide scalar change will be
chosen unilaterally, and no wrong-typed fixture will be claimed to pass.

## Implementation shape for the approved presence-aware cell (G2)

Approved: a bounded presence-aware JSON cell in the shared view family (option 1); the only escape to a
native `double` accepts an actual number and otherwise raises a dedicated out-of-profile error.

One hard constraint decides the shape, and it is worth recording before any edit. `row_get<Key>(row)` returns
`decltype(auto)`: for a native owner it yields a reference into the real cell, and generated code relies on
that (writes through it, live reads after other writes). A projection that computes a `double` from an erased
`Any` cannot be one more `return` in that function -- `decltype(auto)` cannot deduce both `const Value&` and a
materialized `Value` for one instantiation. The naive "fall back to a projection when there is no
`TypedRowCell`" is therefore ill-formed, not merely inelegant.

Two shapes fit, and only these:
1. A cell: the JSON owner answers a typed read with a lazily created `TypedRowCell<Value>` whose read closes
   over the JSON object. `named_value_if<Value>` keeps its pointer contract, `row_get` is untouched, and the
   projection error happens when the cell is created. The cost is owner-side mutable cache state on first
   read, which must be tested for: the object stays read-only and identity-stable, a second read sees the
   same cell, and a write through the view is still refused.
2. A by-value read: a second entry point (for schemas whose members may be JSON-backed) that returns `Value`
   and never a reference, used by generated code only where the destination is not written through. This
   avoids the cache but splits the read API and needs the emitter to choose the entry point per schema.

Use 1 unless writing through a JSON-backed member must be refused at compile time; the approved read/write
tests can be written against either, but not against a projection spliced into the reference-returning
`row_get`.

## Compiler-half design for the JSON scalar projection (G2, owner: builder6)

Scope, per Manager: this lowers scalar member reads into `flight::JsonScalarCell`; it does not build a
recursive materializer, copy a schema, or invent a null policy. Structured members are reported as remaining
work, not closed by a blanket refusal.

**The value.** `JSON.parse(source) as T` keeps the parsed object's identity: the asserted value is a structural
row over the `JsonObject` owner (`detail::owner_for<JsonObject>`), not a `Ref<T>` cast. One runtime seam is
required and nothing more: a way to construct `StructuralRef<Schema>` from that owner with no native subject
(`StructuralRef` already supports owner-only rows; it needs a constructor/factory reachable for a
`shared_ptr<JsonObject>`). Everything else the cell already provides.

**Provenance.** Only a value whose origin is the assertion (or a local/parameter initialized from one) is
JSON-backed. This is a binding-provenance set computed once per module, like the existing
`privateJsonRecordProducerIds`: the assertion seeds it, assignment and direct initialization propagate it, and
anything that escapes the analysis keeps its current lowering.

**Reads.** A member read on a JSON-backed receiver whose declared member is a JavaScript scalar emits
`flight::JsonScalarCell` built from the row's own presence:
- no bound cell -> `JsonScalarCell{}` (missing, evaluates as undefined);
- bound -> `JsonScalarCell{view.get(name)}` (present, value or explicit undefined).
The cell is the value; nothing is coerced at the read.

**Operations, by the operation the source writes (not by the declared type):**
- `Number.isFinite(x)` / `Number.isNaN(x)` -> `cell.is_finite()` / `cell.is_nan()` (non-coercing);
- unary `+x` / `-x` -> `cell.unary_plus()` / `cell.unary_minus()`;
- arithmetic and relational operands -> `cell.to_number()` (ToNumber: null 0, absent NaN, string parse);
- string concatenation/templates -> `cell.to_string()`;
- `===` / `!==` -> `cell.strict_equals(other)`; `==` / `!=` -> `cell.loose_equals(other)`;
- `??` / `?.` / truthiness -> `is_nullish()` / `to_boolean()`.

**Escapes.** A JSON-backed value reaching a native `double` parameter, field, return or array element lowers to
`cell.as_native_number()`, which converts only an actual number and otherwise raises the dedicated
`NativeScalarEscapeError`. The cell has no implicit numeric conversion, so a site the lowering misses does not
compile rather than silently coercing -- that is the negative control for "every escape is intercepted", and
it must be covered by a native-parameter/return/array fixture, not only by the cell's own test.

**Remaining after this cut.** Structured members of a JSON-backed value (`layers`, nested objects) are object
and array reads, not scalars; they need a nested JSON row or their own explicit operation set. Until they are
lowered, any header that reads one stays PARTIAL, and the 21-header count reports fully-faithful versus
mismatch/structured partial separately.

## Measurement set and the fully-faithful / partial split

The approved count is over 21 headers: the 17 in `static-cast-assertion-9674a66c` (15 `scene2d-formats`
Lottie rows plus 2 `scene2d-resources` importers) and the 4 Aseprite rows in `other-4e22e4de`
(`spritesheet_formats/_internal_index`, `aseprite_parse`, `contract`, `spritesheet_detect`).

Classification, decided by what the header actually reaches, not by whether it compiles:
- **fully faithful**: every JSON-asserted value the header reaches is read through operations this design
  represents -- scalar members through `JsonScalarCell`, and no structured member read on a JSON-backed
  receiver is reachable from it. Its consumer must build, run, and preserve the source's behavior.
- **mismatch / structured partial**: the header reaches a structured member (`layers`, nested objects) or an
  escape whose value is not an actual number where the source would have coerced or compared it. It stays
  PARTIAL by name; it is not counted as faithful and no full PASS is claimed for it.

Neither class is closed by a blanket refusal: a refusal of a structured read is a report of remaining work,
not a qualification.

## Coverage check: the data-read hook does not reach the Lottie cluster

Measured on the current emission: `flight/scene2d_formats/lottie_document.hpp` contains **42 typed
`row_get<flight::RowKey<"...">>(document)` reads and zero `read_json_data_property` calls**. The asserted
document is carried as a structural row (`RowReadonly<RowOf<Ref<LottieDocument>>>`), and every member read is a
typed row read -- `fr`, `ip`, `op`, `w`, `h` as `double`, `layers` as `Array<Ref<LottieLayer>>`.

So substituting the private-JSON data read (`read_json_data_property` -> `read_json_scalar_property`) cannot
make these 17 rows compile: the call site the emitter would change appears zero times in the file. The read that
fails is `row_get<double>`, which requires the owner to answer a `TypedRowCell<double>`; a JSON-backed
`JsonRowOwner` supplies erased cells only.

The minimum path that can compile them: give `JsonRowOwner` a typed materialization for the scalar members a
row declares -- for `double`, an actual number only; null, a string, a boolean, an object or an absent member
raise the same `flight::NativeScalarEscapeError` at that read, which is the approved native escape boundary and
keeps the escape-only rule intact. Structured members (`layers`) have no such projection and remain explicit
remaining work; a header that reaches one stays PARTIAL by name. This is the shape-1 option recorded earlier,
now placed at the exact reads the cluster actually performs.

Without this, the emitter lane can be implemented exactly as planned and the 21-header result stays at 0/21,
with no diagnostic change to show for it. Either the typed projection lands with the lane, or the lane is
scoped to clusters that genuinely use the data read and the Lottie cluster is reported as dependent on the
typed projection instead.

## The row construction spelling matters (probe result)

Building the JSON-backed row two ways from the same parsed value, with a runtime probe:

```cpp
using Row = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Any>>>;
const auto properties = flight::named_properties(parsed);
Row::from_owner(properties.shared_owner());                    // reads the member correctly
flight::structural_ref_cast<Row>(properties.shared_owner());   // COMPILES, reads the member wrongly
```

`from_owner` goes through the owner the JSON value already has (`owner_for<JsonObject>`), so the row's
cells are the parsed object's members. `structural_ref_cast` over a `shared_ptr<JsonObject>` instead builds
`StructuralRef<RowWritable<RowOf<std::shared_ptr<JsonObject>>>>` and asks `owner_for` for a
`std::shared_ptr<JsonObject>` subject -- which is not the `JsonObject` the JSON specialization covers, so the
owner is a native-binding owner over the wrong subject and every member read misses.

The emitter's assertion lowering must therefore emit
`Row::from_owner(flight::named_properties(<jsonValue>).shared_owner())`, not `structural_ref_cast` over the
`JsonObject`. The probe is two lines and worth running in the emitted fixture, because the wrong spelling does
not fail to compile -- it silently reads absence.

## Implementation notes for the emitter lane (exact edit points)

Both halves are located, and one simplifies the other.

**(a) Assertion -> JSON-backed row.** The generic assertion fallback is
`cppCompilerBackend.ts:5661` (`static_cast<${emitType(expression.type)}>(<source>)`). When the source is a
`JSON.parse` call (the shape `isCppSyntaxOnlyJsonParseTryCpp` already recognizes: property `parse` on the
ambient `JSON`, one argument) and the target is a row-representable object type, emit
`<RowType>::from_owner(flight::named_properties(<parse>).shared_owner())` instead. `<RowType>` comes from the
structural row plan for `expression.type`; the surrounding `StructuralRef<...>(...)` conversion accepts a row
of the same schema. Use `from_owner`, not `structural_ref_cast` over the `JsonObject` -- see the probe above.

**(b) Row reads need no interprocedural provenance.** `read_json_scalar_row_member(row, key)` reads through
`NamedProperties(row)`, which serves a native owner (typed cell -> erased Any) and a JSON owner (raw value)
identically. So the substitution does not need to know which function produced the row, and does not need a
`jsonRowBindingIds` flow at all. It applies at the structural-row member read,
`cppCompilerBackend.ts:6800`, whenever the declared member type is number/boolean/string.

**What the substitution then requires.** The read returns a `JsonScalarCell`, so every use site of that exact
expression must lower by operation: `Number.isFinite` -> `cell.is_finite()`; arithmetic/relational operands
-> the cell's operators or `to_number()`; `typeof` -> `type_of()`; truthiness -> `to_boolean()`; string
context -> `to_string()`; and a destination declared as a native `double` (variable initializer, argument,
return, array element) -> `as_native_number()`. The last one is the escape boundary and the only place a
non-number may fail. A per-expression marker recorded when the read is emitted, consulted by the operation
emitters, is the mechanism; the predicate must key on that marker, not on the row type, so a native document
and a parsed one take the same path.

The multi-site operation half is the remaining edit. It cannot be landed as the read substitution alone: the
cell has no implicit numeric conversion, so an unmapped use site fails to compile rather than silently
coercing -- which is the intended safety property, and also why the two halves must land together.

## A binding-scoped JSON-row set cannot reach the Lottie reads

The source splits the two operations across functions:

```ts
function parseLottieDocument(source: string | Readonly<LottieDocument>): Readonly<LottieDocument> | null {
  if (typeof source !== 'string') return source;
  try { return JSON.parse(source) as LottieDocument; } catch { return null; }
}
function isValidLottieDocument(document: Readonly<LottieDocument>): boolean { /* reads document.fr ... */ }
```

The assertion is in `parseLottieDocument`; every one of the 42 `row_get` reads is in `isValidLottieDocument` (and
its callers), on a PARAMETER that may hold either the caller's native document or the parsed one. A provenance
set keyed to the binding the assertion initializes stops at that function's boundary, so it marks none of the 42
reads and the cluster cannot move.

Two routes, and only these:
- key the read substitution on the row read itself (declared scalar member of a structural row), which needs no
  provenance because `read_json_scalar_row_member` reads a native owner and a JSON owner identically; or
- carry the set interprocedurally, which means proving that every caller passes a parsed row into the parameter
  -- for `isValidLottieDocument` that is false by construction, since callers pass a native document too.

The first is the only route that matches the source's own shape: one function serves both owners, so its reads
must be owner-agnostic. Worth settling before the backend patch, because the second route cannot be made
sound here and the first changes which expressions the operation lane must cover.


## Qualified lazy-row and scalar compiler preparation

At Flight `a627849` / compiler `806d257`, the declared
`compiler-json-assertion-scalar-row` patch elects only original named schemas
reached by JSON assertions. Assertions produce the existing `StructuralRef` ABI
over the original parsed owner; primitive and null roots retain their raw kind.
Declared scalar reads on those schemas use the shared presence-aware cell.
Unrelated equal-shape rows, nominal owners, structured members and unknown
members retain their existing paths. The three earlier private JSON lanes, the
new private graph lane, their predicates and the declared carrier rules remain.

The backend retains scalar locals and lowers operations at their source use:
`typeof`, noncoercing finite/NaN predicates, truth/nullish selection, unary and
numeric operations, string-sensitive addition, UTF-16 relational comparison,
strict/loose equality and templates. Native parameter, return, field and array
boundaries explicitly escape through the shared out-of-profile error family.
This changes no global scalar ABI and copies no document schema or source body.

All **109 patches apply from a clean raw pin**, the strict build passes and
**125/125 focused tests** pass across eight files, including the three-test
portable native oracle. A separate probe imports the **complete original raw
LottieDocument type module** and emits controlled TypeScript operation functions;
it is not an original public Lottie parser execution. Its unchanged 24 field
cases, nine roots and four native boundaries compile, link and pass **586/586
checks**. The first run's 30 composite-value failures remain recorded; builder6's
already-owned JSON ToPrimitive runtime correction resolves them without narrowing
the input matrix. Seven unrelated emission controls remain byte-identical.

The bounded existing-backend comparison selects 28 JSON/scalar/row-assertion
tests on both the 108-patch baseline and 109-patch candidate: **27 pass / 1 fail**
on each, with 1548 unselected. All 1576 assertion occurrence names and statuses
match; no new failure or timeout occurs. The retained failure unwraps a
proven-present optional property before receiver and structural assertion use.

This qualified compiler preparation authorized the scoped installed qualification
recorded below. Structured JSON reads and Aseprite's two-owner
nominal union remain separate partial boundaries. No installed header promotion,
fully faithful parser/package result or global sweep follows from these fixture
results. Compact evidence is in
[the qualification record](../../artifacts/json-assertion-scalar-row-qualification.json).


### Nullable-root readiness correction

A later readiness probe used the **whole original `parseLottieDocument`
function and complete original type module**, with controlled kind/null wrappers.
After the source null guard, narrowed nominal evidence incorrectly folded six
primitive-root `typeof` results to `"object"` (6/20 failures; native build passed,
run exited 1). The backend now resolves the elected row from its stored binding
type and uses occurrence presence to unwrap the optional exactly once. The same
raw parser matches its executed TypeScript oracle **20/20**, native exit 0.

The portable fourth fixture reproduces the nullable parse/guard shape without an
external checkout and compares 30 checks across ten roots against its own
executed TypeScript. Current verification is **4/4 owned tests plus 99/99 private
JSON preservation tests**, all unskipped. All 109 ordered patches and the strict
build pass; the relevant existing backend comparison remains **27 pass / 1 same
failure** against the retained 108-patch baseline, all 1576 names/statuses equal.
The preceding 125/125 and raw-schema 586/586 results are historical focused proof;
they do not establish original public parser behavior. The approved single scoped
Lottie qualification is recorded below. No generated header changed in the guard
correction itself.

The incoming symbol-presence compiler patch advances the declared floor to 110.
It clean-applies with this correction, passes the strict build and passes the
103/103 focused tests without skips. These results are separate from the historical
108/109 baseline comparison in the qualification record.

### Installed scoped qualification

The single normal scene2d-formats generation **exited 0** on frozen
`286d0d80`, with 110 compiler patches / 311 total source patches. All frozen
inputs stayed unchanged and both raw pinned dependencies were restored clean.
Only the named package moved: three headers plus its manifest (`410d6eae`).
The Lottie JSON assertion now retains the parsed root, its document scalar reads
use cells, its null guard retains raw null, and its final native frame-rate
boundary escapes explicitly. Rive/SVG differences are normal output from the
same frozen stack; they confer no additional capability claim.

The **whole original public TypeScript module**, with 1,225 original dependency
modules and no stubs, executes 32 calls: two accepted, 30 rejected, no exceptions.
The corresponding native consumer includes the whole installed original header
and callees. Its compile **exits 1** at `lottie_document.hpp:356`: undeclared
`lottie_numeric_value`. That call and the absent definition are unchanged from
the delivered base; the generated refusal is
`cpp-erased-array-predicate-runtime-required`. Other bounded diagnostics include
callback arity, nominal `row_get`, `handle_varies` and erased subscripting.
Native link/run is **not reached**: zero of the 32 native cases ran.

All **21 exact approved headers** are attempted with Clang 22, the existing local
SDL headers and two jobs: **0 pass / 21 fail / 0 unattempted**. The selected
membership is 14 scene2d-formats, three scene2d-resources and four Aseprite headers;
the qualification record lists every observed first cause. The earlier missing
SDL include was a corrected setup omission; its partial check is excluded.

The resulting accounting is **0 faithfully qualified / 21 PARTIAL**, with zero
header-ledger promotions and no package/global pass. The focused compiler proof
remains valid; omitted helpers, callbacks/structured JSON operations and
Aseprite's nominal union require their own work. No original function was
extracted or stubbed to manufacture a public-consumer pass.

After the clean rebase onto the codec preparation, all **111 compiler patches**
apply from the clean raw pin, the strict build passes and **103/103 focused tests**
pass without skips. The qualified JSON patch, runtime helper and three generated
headers are byte-identical across that rebase. This is current compiler/runtime
compatibility proof; the installed 21-header/public-consumer measurements remain
those of the frozen 110-patch generation. No additional SDK run was performed.


## Erased-array helper follow-up

The numeric helper's original array selection now retains the selected `Any` array owner and immutable aliases. Guarded element reads retain null, undefined and live mutations; nullish fallbacks remain lazy, and `Number` uses the shared scalar operation carrier, including Unicode whitespace and source `TypeError` for noncallable conversion members and Symbols. A sized output array is admitted only after a complete-fill proof over an unchanged numeric bound. Native array escapes, partial fills and intervening observations still refuse. Compound typed-array guards retain their branch evidence, writes target the original variant storage, and unresolved third instance-array callback parameters recover the receiver array type.

The carried patch passes 136 focused tests and two patch-automation tests on the ordered 114-compiler-patch stack. The complete original numeric and handle helpers separately compile, link and pass 301 native checks against their source oracle. These helper checks are preparation: the whole original public Lottie body/callee closure and its 32-case installed consumer remain pending, and this change makes no SDK or header promotion claim. The separate qualification artifact records the exact input hashes and the failing root-wide fixture typecheck lane.


The generic-array and nested-owner preparation is qualified in
[generic-array-owner-qualification.json](../../artifacts/generic-array-owner-qualification.json).
The compiler keeps the original generic data owner for the exact ambient array predicate, carries
live guarded reads, and selects narrowed fields using the original named alias or row carrier's
slot order. Presence-context refinement is bounded to original absence-carrying bindings; ordinary
calls, unguarded generic fields, class owners and invalidated field facts do not gain that proof.
The new native fixtures exercise both slot order and mutations through the same nested owners.
Production and owned strict checks pass, as do 38 focused tests without skips, 15 native fixture
checks and 301 checks on the two untouched original helpers. Three guarded-path controls fail in
both the preceding 117-patch floor and this 118-patch composition; those results are recorded as
failures. Complete original-module emission now retains every body except
`createLottiePositionSampler`, whose filtered-track callback still lacks nullable-element presence
evidence. This preparation changes no SDK output and makes no public 32-case or exact 21-header promotion.


The guarded filter and optional-subset preparation is qualified in
[narrowed-filter-qualification.json](../../artifacts/narrowed-filter-qualification.json).
The compiler retains the standard source filter predicate, proves success excludes the original
absence sentinels before selecting each original element handle, and reuses stored native handles
for a strict optional-union subset. Annotation-only or reassigned-parameter predicates do not gain
a projection. On the ordered 120-patch compiler floor, production and owned strict checks pass,
as do 48 focused tests with no skips and 28 native fixture checks. The same three previously
recorded guarded-path controls still fail. Every body of the untouched original Lottie module
now emits from a freshly lowered 1,467-module dependency graph. The whole public native consumer
still fails compilation at remaining readonly-record, array and carrier boundaries; zero of its
32 cases ran. No SDK output or exact 21-header result is promoted by this preparation.

## Aseprite additional raw document view preparation

The carried `compiler-lazy-json-union-document-view` patch keeps native document
variant aliases and parameter overloads, and adds a shared structural row path for
raw JSON documents and their public `Parsed` result. Parsing elects no union arm
and reads no discriminator. Null/primitive roots, extra fields and nested owners
survive until the original source uses them. Native overloads retain their original
owners; raw nested reads and writes use the existing scalar-cell/runtime family.
Declared member vocabulary uses compile-time `RowKey` references for the normal
metadata collector, without running getters or validating a parsed schema.

On the preceding 118-patch compiler floor, the strict production build and five
focused suites pass **41/41 without skips**. The complete original public Aseprite
consumer and real dependency bodies also compile, link and run: all **25 original
tests** supply **28 public calls**, with **1,568 returned-data/diagnostic checks**
and **nine native array/hash document, Record, tag and owner compatibility checks**
passing. The separate original TypeScript oracle passes 154 checks. Three parser
headers and production shared metadata were emitted in an isolated overlay;
current compiler emission is byte-identical to those executed inputs.

This is compiler and public-consumer preparation. The four normal installed
headers and coherent SDK qualification remain pending; no previous header failure
is promoted. Undefined/hole writes and unsupported native scalar boundaries retain
explicit errors/refusals and PARTIAL status. Exact hashes, measured scopes and
commands are recorded in [the qualification artifact](../../artifacts/aseprite-json-document-view-qualification.json).

After the incoming typed-empty-fallback iteration patch, the **120-patch** ordered
composition applies, reverses to the raw compiler, and reapplies byte-identically.
Its strict production build, six focused suites (**64/64, zero skips**) and two
patch-automation tests pass after the implementation commit. All three original
parser headers and production metadata remain byte-identical to the executed
1,577-check consumer inputs. The normal four-header qualification is still pending.

The subsequent guarded-filter preparation composes on the **121-patch** floor.
Ordered apply/reverse/reapply and the strict build pass; the document, new filter
and generic-owner suites pass **17/17 without skips**. The original three headers
and metadata again remain byte-identical. Incoming work and both appended request
sections are preserved; no inactive forwarding proposal is activated.

[Original optional-slot qualification](../../artifacts/original-optional-slot-qualification.json)
records the next preparation: present source narrowing forwards the unique stored native reference
into its contextual union, and literal membership on an erased value calls original-owner presence.
On the ordered 121-patch compiler floor, strict checks and 31 focused tests pass without skips,
including 38 native checks; the same three guarded-path controls still fail. The whole original
consumer still fails compilation, now past the flat, native-cast and raw-membership sites and at
later readonly record, mask-result, event-data and sequence operations. No native public case has run.


[Closed readonly-record qualification](../../artifacts/closed-readonly-record-qualification.json)
retains the original narrow diagnostic detail owner through the source-declared wider readonly
parameter and escaped field. The bounded facade observes caller writes and erase, preserves key
order and identity, and retains lifetime; it exposes no writable widened storage. Mutable, generic,
erased and numeric-keyed contracts keep their prior representation, and writable escapes refuse.
Production and owned strict checks pass. The 124-patch composition retains incoming particle
forwarding and passes 53 focused tests without skips, with 10 new emitted-native and 17 separate
runtime checks. All bodies in the original Lottie, diagnostic type and collector modules emit.
The whole public consumer still requires its native closure proof; no SDK or Types generation,
public 32-case success, or exact 21-header promotion is inferred.


### Normal formats output with 121 compiler patches output and stronger public qualification (2026-10-18)

The approved single grouped formats run exited zero on the archived 121 compiler / 323
patch input set and provisional B 6e1aa571 dependencies. All 1,150 frozen B dependency
headers and 323 initial patch payload hashes still match. The 19 own generated paths
were committed before native checks; the superseded atlas Aseprite override was removed
so the checks consume the original emitted parser bodies. Root shared metadata was
canonically merged rather than importing the foreign B package slices. The output
retains explicit mixed-profile warnings for 14 dependency packages.

The official original four-header check attempted all four with Zig/Clang 22 and
real SDL3 flags: **1 pass / 3 fail**. `aseprite_parse.hpp` passes. The two barrels and
`spritesheet_detect.hpp` fail because retained callers name refused `inferAnimations`
helpers in LibGDX and Starling. Their successful regex matches have mandatory groups,
but the conservative capture carrier lacks proof at the Map key argument. A separate
source-equivalence preparation adds only erased assertions at those proven uses; no
normal replacement generation or header promotion has occurred.

The expanded whole original public consumer compiles and links with GNU C++15 against
the full archived 9ec runtime and exact provisional B dependencies. Its strict run
**fails** at the first newly checked syntax-fallback document snapshot: native
`AsepriteMeta` enumerates omitted `layers` and `slices` keys. A diagnostic run continues
while retaining this failure, reaches 2,443 checks, and finishes nonzero with exactly
three such key-count failures. All remaining checks report no failure, including
valid raw document snapshots, returned owner identity, live nested writes, array/hash
replacement before the next authored discriminator use, separate parse roots and
eight valid-JSON error-at-use cases. Builder6 owns the constructed native presence
question. Earlier 1,577-check proof covered data/diagnostics and native compatibility;
it did not assert these seven source document snapshots.

The required `sdk:check` reproduction remains pending a coordinated generation slot.
The current no-further-generation hold is honored. B remains provisional, and none of
these results establishes a complete SDK, public GL path, ABI or capability gain.


The six erased capture assertions now have complete-module JavaScript equivalence
and 64 source controls on both the raw pin and composed input. All 203 Flight patches
reverse to a clean pin and reapply byte-exactly. The two whole original helper headers
emit without refusal and compile/link/run 34 native controls on the archived runtime.
This remains isolated preparation. The follow-up four-header diagnostic still reports
1 pass / 3 fail, with the first cause moving to an obsolete TexturePacker atlas override
whose native-only signature rejects the newly emitted raw document view.

The unchanged 2,443-check public consumer passes on the complete `7d06f37d` runtime,
but this is diagnostic evidence only. A separate native counterexample shows that
runtime's global optional-engagement rule hides required nullable own properties.
Actual GL context source authors required nullable keys as `null`; their generated
C++ storage is also optional, so storage engagement cannot prove source omission.
Foreman accepted the regression and assigned builder6 the runtime correction and
builder4 the compiler construction hook. Explicit authored presence and order must
remain on the same owner, with null, undefined, omission, aliases and deletion kept
distinct. A safe runtime and compiler API is being agreed before qualification.

Native literal construction now needs authored own-key metadata. The separate compiler-native-literal-own-presence patch records explicit source keys after the existing value evaluations and make_ref on the same owner, with a null fallback for null-only optional storage. It retains live native cells, records subsequent typed field assignments, and uses row_delete for exact optional native data-field deletion. Spreads, accessors, and computed construction retain their specialized lowering.

The unchanged expanded original public consumer passes2443 field checks with all seven document snapshots on the corrected complete runtime and isolated whole-module emission. The required-nullable and live-alias/rejected-write counterexamples also pass. This is preparation evidence: final packaged checks and the normal four-header/sdk:check reproduction remain open. Numeric index keys exposed a separate runtime ordering counterexample, reported to builder6; this work does not claim that broader key ordering is qualified.
