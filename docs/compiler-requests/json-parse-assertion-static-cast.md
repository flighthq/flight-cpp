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
