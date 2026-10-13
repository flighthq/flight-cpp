# `@flighthq/tilemap-formats` native-compile defect inventory

Measured at Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, after the declared
`tilemap-formats-record-materialization` source patch and the two package name repairs. Package-scoped
generation emits 8 of 13 modules. Independent compilation at two jobs passes 6 of 13 headers.

## Refused declarations

| Module | First refusal | Existing request |
| --- | --- | --- |
| `tiledLayerData` | `Uint32Array.from` has no ambient value binding | [typed-array-static-from-binding](typed-array-static-from-binding.md) |
| `tiledJsonParse` | an `unknown` value cannot enter the property-value union | The same erased boundary later emits the invalid `to_string(Any)` described by [erased-value-stringification](erased-value-stringification.md). |
| `tiledProject` | a narrowed layer arm has no expression evidence for the `TiledLayer` multi-variant | [compiler-contextual-imported-union-owner](compiler-contextual-imported-union-owner.md) records this diagnostic family. |
| `tiledXmlParse` | `Uint32Array.from` has no ambient value binding | [typed-array-static-from-binding](typed-array-static-from-binding.md) |
| `tilemapDetect` | the `?? []` fallback loses its sibling array's element type | [contextual-empty-array-fallback-element-evidence](contextual-empty-array-fallback-element-evidence.md) |

The JSON refusal omits `objectField`, `parseJson`, `parsePointsField`,
`buildTiledLayerDataFromJson`, and `buildTiledTilesetTileFromJson` from the partial header. The XML
refusal omits `buildTiledLayerDataFromXml`. Those omissions are behavior gaps, not merely header
ordering problems.

After the declared portability patch and complete JSON parser, JSON formatter, project projection,
and XML layer-construction overrides, the current scoped result is 11 of 13 generated modules plus
faithful supplies for both refused modules. Ten of 13 headers compile independently. The only three
failures are `tilemapDetect` and the two aggregate headers that include it, all at the frozen older
registry row's missing `KeyedTable`. No compiler-refused tilemap-formats behavior remains unsupplied.

## Emitted C++ that does not compile

After the first diagnostic is expanded, the failures reduce to these boundaries:

1. `String(unknown)` in `coercePropertyValue` becomes `flight::to_string(Any)`, for which the runtime
   intentionally has no contract.
2. `Record<string, unknown>` literals and writes in the TMJ serializer are emitted as heterogeneous
   braced initializers and `record.get(key).value() = value`. The former cannot select `Any`; the
   latter assumes a key is already present and would throw even if it compiled.
3. `JSON.stringify({ ...jsonTileset(...), type: 'tileset' }, null, 2)` loses the spread result and
   emits only the contextual literal. Other calls pass a designated initializer for an erased value.
4. `formatPoints(readonly { x; y }[])` invents an anonymous point owner for its callback although the
   input is the canonical `Vector2Like[]`, so `Array::map` cannot invoke the callback.
5. diagnostic detail literals passed to an optional `Record<string, string | number | boolean>` are
   emitted as initializer lists for the outer `optional`, rather than constructing the Record first.
6. object literals returned as `TiledLayer | null` are emitted directly as designated initializers for
   the optional multi-variant instead of constructing the narrowed nominal layer arm.
7. destructuring `array.entries()` emits `.element()` calls on `std::tuple` rather than `std::get`.
8. `buildTiledLayerFromXml` evaluates the shared `{ ...base, type, ... }` layer object spread but drops
   every base field from the concrete aggregate. The result compiles after the contextual union-owner
   repair, yet silently loses id, name, opacity, visibility, offsets, tint, parallax, class, and
   properties for tile, object, image, and group layers.

Items 2, 3, 5, 6, 7, and 8 do not yet have a narrower request. They are listed here before any
carrying patch so a compiling rewrite cannot erase the compiler evidence.

## Frozen dependency boundary

`tilemap_detect.hpp` also includes the current registry contract, whose generated row in this checkout
is deliberately frozen at the older pin while the coordinated registry migration is owned elsewhere.
Clang therefore stops in `registry_table.hpp` because `flight::types::KeyedTable` is absent. This is a
measured dependency skew, not a tilemap-formats defect, and this unit must not repair or regenerate the
registry row.

## Acceptance

- Preserve `unknown` string conversion, Record identity and insertion behavior, narrowed layer owners,
  tuple iteration order, and diagnostic detail values.
- Emit both parsers and both serializers without dropping their refused helpers or replacing failure
  paths with stubs.
- Compile `tilemapDetect` only against the coordinated current-pin registry output; do not duplicate
  `KeyedTable` locally.
- Exercise the four upstream TMX/TMJ/TSX/TSJ fixtures, byte-exact serializer output, and distinct
  malformed/compression failures natively before calling the package complete.
