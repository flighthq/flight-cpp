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

`compiler-readonly-json-record-view` carries that view through exact
`Readonly<Record<string, unknown>>` helpers and aliases. Its reads return bare
`Any`, whose own tags answer null/undefined tests. Nullish coalescing evaluates its
left side once and preserves the lazy source fallback, including
`anim.drawOrder ?? anim.draworder`. Readonly literal arguments still construct the
original ordinary Record once and then retain its storage through a view.
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
