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
