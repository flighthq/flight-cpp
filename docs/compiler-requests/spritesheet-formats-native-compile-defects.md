# `@flighthq/spritesheet-formats` native-compile defect inventory

Measured at Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`. Package-scoped generation emits 11 of 13 modules;
independent compilation at two jobs passes 5 of 13 headers before the carrying repairs below.

## Refused declarations

| Module | First refusal | Existing request |
| --- | --- | --- |
| `asepriteParse` | The spritesheet Aseprite document union cannot enter the parallel texture-atlas Aseprite schema union. | [parallel-schema-union-structural-conversion](parallel-schema-union-structural-conversion.md) |
| `texturePackerParse` | The spritesheet TexturePacker document union cannot enter the parallel texture-atlas TexturePacker schema union. | [parallel-schema-union-structural-conversion](parallel-schema-union-structural-conversion.md) |

The omissions remove each module's `documentToData` implementation. Their public parse functions are
therefore declared around a missing helper and are not usable merely because a partial header exists.

## Emitted C++ that does not compile

Expanding the first diagnostics reduces the eight failing headers to four boundaries:

1. `asepriteParse`, `cocosPlistParse`, `texturePackerParse` and the package aggregates create the
   anonymous option objects accepted by `createSpritesheetFrameData`, `createSpritesheetAnimationData`
   and `createSpritesheetData` in another package. The emitter gives the byte-identical shapes separate
   namespace owners, so the calls cannot bind. This is the spritesheet-formats reach of
   [cross-package-anonymous-object-owner](cross-package-anonymous-object-owner.md).
2. The Cocos parser passes `{ frame: frameName }` directly to an optional
   `Record<string, boolean | number | string>` parameter. The emitter writes the entries as an outer
   `std::optional` initializer, so overload resolution cannot construct the Record. The already
   contextualized literal needs to materialize its declared Record carrier before entering the
   optional, the same diagnostic family carried by the existing particles-formats, font-formats and
   SWF diagnostic-detail patches.
3. `libgdxAtlasParse` and `starlingParse` include textureatlas-formats parsers whose calls to
   `createTextureAtlasRegion` encounter the same anonymous-owner split. Those dependency headers are
   outside this package group's regeneration and repair scope.
4. `spritesheetDetect` and package aggregates reach the deliberately frozen older Registry output,
   whose row lacks `flight::types::KeyedTable`. The coordinated atomic Registry migration owns that
   generated row.
5. Aseprite and TexturePacker frame tags declare `direction` as a required string-literal union, but
   their defensive `tag.direction ?? 'forward'` expressions are emitted as
   `tag->direction.value_or(...)` even though the member correctly has the non-optional
   `flight::String` storage its declaration requires. A valid C++ lowering may either retain the
   source's redundant nullish test without asking a string value for optional operations, or simplify
   it to the required member read. It must not make the schema property optional.
6. Cocos `parsePlistPair` uses the JavaScript-compatible pattern
   `/{?\s*([-\d.]+)\s*,\s*([-\d.]+)\s*}?/`, where braces that cannot form a valid quantifier are
   literals. The native `std::regex` ECMAScript implementation rejects the same source with
   `regex_error` before matching. Escaping both optional literal braces is accepted by both engines
   and denotes the same language; a correct RegExp binding should either accept JavaScript's legacy
   literal-brace grammar or normalize it without changing captures.
7. The emitted Aseprite and TexturePacker public string parsers apply
   `static_cast<NominalDocumentUnion>(Json::parse(json))`. `Json::parse` returns `JsonValue`, which has
   no unchecked C++ conversion to either nominal schema union, so both partial headers fail even
   before they call the refused document converter. The assertion must lower through a checked schema
   materialization or refuse with its converter; an unchecked cast is the defect recorded by
   [assertion-lowered-to-unchecked-static-cast](assertion-lowered-to-unchecked-static-cast.md).

## Carrying repairs

The spritesheet-formats anonymous owners can be aliases of the spritesheet declarations because each
pair has an identical source contract, field order, member types and structural suffix. The alias
changes no allocation, field value, call, identity or control flow. A typed local for the Cocos detail
object likewise evaluates the same one-field object once in the same branch and sends that object to
the same diagnostic sink. Removing the two redundant frame-tag fallbacks preserves every represented
document: the required direction member is always the left operand, so TypeScript never evaluates the
fallback and the animation receives the identical string. Explicitly escaping the two optional plist
braces likewise preserves every match and capture while avoiding a host regex grammar divergence.

None of these repairs supplies the two refused `documentToData` functions, rewrites textureatlas-formats, or
duplicates `KeyedTable`. Those remain explicit residual boundaries.

After the carrying repairs, independent compilation passes 6 of 13 headers. Cocos parsing joins the
three formatters/serializers already in the passing set; the seven remaining failures are the two
JSON/schema casts (and two aggregates that include Aseprite), two textureatlas-formats ownership
failures, and Registry-backed detection.

## Acceptance

- Preserve parser frame order, names, geometry, rotation, trim, pivots, animations and diagnostics.
- Exercise Cocos parsing with exact document and spritesheet shapes plus malformed-frame diagnostics.
- Keep byte-exact libGDX formatting and shape-exact Cocos and Starling serialization coverage.
- Do not bless the Aseprite and TexturePacker JSON serializers' current `{}` result while
  [json-stringify-nominal-ref-empty-object](json-stringify-nominal-ref-empty-object.md) remains open.
- Compile registry-backed detection only against the coordinated current-pin Registry output; do not
  patch or regenerate its row in this package group.
