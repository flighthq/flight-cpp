# Structurally assignable schema unions need identity-preserving call conversion

Two imported JSON schema families describe the same document shapes under different public names. The
source schema is structurally assignable to the atlas schema, so TypeScript passes the same document
object across the call. The C++ emitter expands both named unions to nominal `Ref` alternatives and
refuses because none of the source owners is exactly the destination owner.

Measured at Flight `a627849` / flight-compiler `2b687ea` in two
`@flighthq/spritesheet-formats` helpers:

```ts
declare function parseAtlasAseprite(doc: TextureAtlasAsepriteDocument): void;
declare function parseAtlasTexturePacker(doc: TexturePackerAtlasDocument): void;

function readAseprite(doc: AsepriteDocument): void {
  parseAtlasAseprite(doc);
}

function readTexturePacker(doc: TexturePackerDocument): void {
  parseAtlasTexturePacker(doc);
}
```

`AsepriteDocument` and `TextureAtlasAsepriteDocument` each have array and hash arms; likewise for the
TexturePacker pair. The spritesheet schemas satisfy every required atlas field and add only compatible
requirements or metadata. The calls are accepted by TypeScript's structural type system.

## Observed

Both complete parser modules are refused with `cpp-contextual-union-inequivalent`. The Aseprite site,
for example, reports:

```text
target multiVariant carriers
[Ref<TextureAtlasAsepriteArrayDocument>, Ref<TextureAtlasAsepriteHashDocument>]
from identifier source multiVariant carriers
[Ref<AsepriteArrayDocument>, Ref<AsepriteHashDocument>]
```

The TexturePacker site reports the corresponding four owners. Alternative order is not the issue; the
owners are distinct even though each source arm is structurally assignable to exactly one target arm.

## Required lowering

The conversion must retain the original object identity and expose a readonly structural view for the
uniquely matching destination arm. It must not materialize a second document, copy nested records or
arrays, or recover write authority that the callee does not need. A discriminated visit over the source
union can select the corresponding destination row view while keeping the same owner.

This differs from `named-union-arms-not-canonical.md`: that request asks one declared union alias to use
one carrier everywhere. Here the source intentionally names one schema union and the destination names
another, structurally compatible schema union.

## Carrying workaround

None. Copying the whole JSON document into the atlas schema would change identity and evaluation costs,
while a source assertion merely asks the emitter for the same unsupported cross-owner conversion. Both
parsers remain refused.

## G2 TexturePacker document converter is still omitted

Inspected at compiler `806d257` / Flight `a627849` for original
`other-acb4b229`, `spritesheet_formats/texture_packer_parse.hpp`.
The exact `NOT GENERATED: function documentToData -- source line 58` marker and refusal ledger
retain `cpp-contextual-union-inequivalent [source-portability]` at:

```ts
const regions = parseTexturePackerAtlasDocument(doc, createTextureAtlas()).regions;
```

The target alternatives are `Ref<TexturePackerAtlasArrayDocument>` and
`Ref<TexturePackerAtlasHashDocument>`; the source carries `Ref<TexturePackerHashDocument>` and
`Ref<TexturePackerArrayDocument>`. These are separate public schema names, with compatible nested
rectangles/sizes/pivots/frames/metadata. Spritesheet frame-tag direction is required while the atlas
schema allows it absent. Neither arm order nor the extra spritesheet guarantee is native inheritance.
Preserve the same document, nested frames, tag arrays and metadata through the actual structural
read contract rather than copying into atlas owners, retyping only the top-level pointer or narrowing
the public parser to one variant.

After geometry, the omitted function creates the actual spritesheet frames and animations and reads
image/size/scale from the original metadata. `parseTexturePackerSpritesheetDocument` returns that
same parsed document, so a geometry-only atlas parser cannot supply its retained-data contract.
The sibling native JSON helper drops the document/tags and normalizes atlas scale differently from
the source spritesheet scale; neither that helper nor a declaration supplies this missing converter.
The [JSON assertion request](json-parse-assertion-static-cast.md#g2-texturepacker-retains-its-original-document-as-well-as-spritesheet-metadata)
records the independent first compile failure, both public parse/catch boundaries and exact source
semantics. The actual one-header check is **0/1 pass** at its line128 cast, before a call to this
helper. This refusal is source/ledger inspection, not an additional measured diagnostic or native
behavior claim. No parser/body/view copy or other C++ carry is supplied.
