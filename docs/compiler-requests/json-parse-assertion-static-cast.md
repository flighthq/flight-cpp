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
