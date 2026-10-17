# An assertion over a `JsonValue` uses the erased-value pattern the runtime does not offer

## Reproduction

`flight/tilemap_formats/tilemap_detect.hpp` at `806d257`, line 112, from
`packages/tilemap-formats/src/tilemapDetect.ts`:

```cpp
auto obj = ([&]() -> flight::Ref<image_layers_tilecount_tileheight_tiles_tilewidth_type_4210b3843317f5f0> {
  const auto& erased_value = raw;
  auto erased_object = erased_value.object_if<image_layers_..._4210b3843317f5f0>();
  if (!erased_object) throw std::logic_error("erased value does not hold the asserted object type");
  return erased_object;
}());
```

`raw` is a `flight::JsonValue`; the assertion is lowered with the erased-dynamic-value pattern, asking the
value for an exact-type object view:

```
error: no member named 'object_if' in 'flight::JsonValue'
```

`flight::Any` has `object_if<T>()` because it boxes objects at a declared type and can answer an exact-type
lookup. `JsonValue` is a tagged JSON tree (`Kind::object` over a `JsonObject`); there is no exact C++ type
in it to recover, and no generic path to materialize a declared structural row from a JSON object.

## Required shape

Lower the assertion against a `JsonValue` by reading the JSON tree through the declared owner -- the same
path the format importers already take -- or refuse the assertion with the rule that names it. Emitting the
`Any` erased-value recovery over a `JsonValue` is the one answer that is neither.

## Affected

3 headers in the G1 sweep, all `tilemap-formats`. This became the first diagnostic for them once the
`String::starts_with(prefix, position)` runtime gap was closed, which is why it is filed separately from
`tilemap-terminated-json-slice.md`'s neighbours rather than folded into that cluster.

## What this repository carries meanwhile

Nothing: the cluster is open. `String::starts_with(prefix, position)` was added to the runtime because
emitted code calls it and the runtime genuinely lacked it; this one is an emitter decision.

## Note on the runtime overload that was added

`String::starts_with(const String&, double)` implements `String.prototype.startsWith`'s own position
contract: ToIntegerOrInfinity, clamped to [0, length], NaN and negatives to 0, +Infinity to the length, and
comparison over UTF-16 code units -- which is what `flight::String` stores. It is not an argument discard.

## G2 BMFont JSON detection

`flight/bitmapfont_formats/bitmap_font_format_kind.hpp:41` at Flight `a627849` / compiler `806d257` is the
same lowering. `isBmFontJsonObject` checks that `JSON.parse` produced a non-null, non-array object and then
reads `raw as { chars?: unknown; common?: unknown }`. The emitter asks the `JsonValue` for
`object_if<chars_common_5b29ec8a252e2596>()` (GCC 15.2: `'const class flight::JsonValue' has no member named
'object_if'`).

A faithful lowering reads `chars` and `common` from the JSON object, and keeps absent members distinct from
present ones. The detector must still give the same answer as `parseBitmapFontJson`, which returns null
without either block. Nothing is carried for this:
- A `JsonValue::object_if` would have to build the anonymous struct from the JSON tree. That is the
  materialization this request rules out.
- Rewriting the source to `as Record<string, unknown>` reaches the missing conversion of
  [parsed-json-object-shape-reads](parsed-json-object-shape-reads.md) step 1.
- A fixed detector answer would fake format detection.
