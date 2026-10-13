#include <flight/tilemap_formats/tiled_color.hpp>
#include <flight/tilemap_formats/tiled_gid.hpp>
#include <flight/tilemap_formats/tiled_json_parse.hpp>
#include <flight/tilemap_formats/tiled_layer_data.hpp>
#include <flight/tilemap_formats/tiled_project.hpp>
#include <flight/tilemap_formats/tiled_tmj_format.hpp>
#include <flight/tilemap_formats/tiled_tmx_format.hpp>
#include <flight/tilemap_formats/tiled_xml_parse.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Native ports of the current-pin tiledColor, tiledGid, tiledLayerData, tiledTmxFormat, and
// tiledXmlParse fixtures. JSON/TMJ, project conversion, and registry-backed detection remain
// intentionally outside this executable until their recorded compiler/dependency boundaries close.

namespace {

namespace formats = flight::tilemap_formats;
namespace types = flight::types;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return View(value);
}

bool gids_are(const flight::Uint32Array& actual, const std::vector<double>& expected) {
  if (actual.length != expected.size()) return false;
  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (actual.get_index(static_cast<double>(index)) != expected[index]) return false;
  }
  return true;
}

bool tiles_are(const flight::Int16Array& actual, const std::vector<double>& expected) {
  if (actual.length != expected.size()) return false;
  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (actual.get_index(static_cast<double>(index)) != expected[index]) return false;
  }
  return true;
}

const flight::String rich_tmx(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<map version=\"1.10\" tiledversion=\"1.10.2\" orientation=\"orthogonal\" "
    "renderorder=\"right-down\" width=\"2\" height=\"2\" tilewidth=\"16\" tileheight=\"16\" "
    "infinite=\"0\" backgroundcolor=\"#204060\">\n"
    " <properties><property name=\"name\" value=\"demo\"/><property name=\"score\" type=\"int\" "
    "value=\"10\"/></properties>\n"
    " <tileset firstgid=\"1\" name=\"base\" tilewidth=\"16\" tileheight=\"16\" tilecount=\"4\" "
    "columns=\"2\">\n"
    "  <image source=\"base.png\" width=\"32\" height=\"32\"/>\n"
    "  <tile id=\"0\" type=\"lava\"><animation><frame tileid=\"0\" duration=\"200\"/>"
    "</animation></tile>\n"
    " </tileset>\n"
    " <tileset firstgid=\"5\" source=\"extra.tsx\"/>\n"
    " <layer id=\"1\" name=\"ground\" width=\"2\" height=\"2\"><data encoding=\"csv\">"
    "1,2,5,2147483649</data></layer>\n"
    " <objectgroup id=\"2\" name=\"objs\">\n"
    "  <object id=\"1\" name=\"p\" type=\"marker\" x=\"1\" y=\"2\"><point/></object>\n"
    "  <object id=\"2\" type=\"poly\" x=\"0\" y=\"0\"><polygon points=\"0,0 8,0 8,8\"/>"
    "<properties><property name=\"tag\" value=\"zone\"/></properties></object>\n"
    " </objectgroup>\n"
    " <imagelayer id=\"3\" name=\"bg\"><image source=\"sky.png\"/></imagelayer>\n"
    " <group id=\"4\" name=\"grp\">\n"
    "  <layer id=\"5\" name=\"over\" width=\"2\" height=\"2\"><data encoding=\"csv\">"
    "0,0,1,1</data></layer>\n"
    " </group>\n"
    "</map>");

const flight::String rich_tsx(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<tileset name=\"base\" tilewidth=\"16\" tileheight=\"16\" tilecount=\"4\" columns=\"2\" "
    "objectalignment=\"center\">\n"
    " <tileoffset x=\"1\" y=\"2\"/>\n"
    " <image source=\"base.png\" width=\"32\" height=\"32\"/>\n"
    " <tile id=\"1\" type=\"wall\"><properties><property name=\"solid\" type=\"bool\" "
    "value=\"true\"/></properties></tile>\n"
    "</tileset>");

const flight::String expected_rich_tmx(R"(<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.10.2" orientation="orthogonal" renderorder="right-down" width="2" height="2" tilewidth="16" tileheight="16" infinite="0" backgroundcolor="#ff204060">
<tileset firstgid="1" name="base" tilewidth="16" tileheight="16" tilecount="4" columns="2">
<image source="base.png" width="32" height="32"/>
<tile id="0" type="lava">
<animation>
<frame tileid="0" duration="200"/>
</animation>
</tile>
</tileset>
<tileset firstgid="5" source="extra.tsx"/>
<layer id="1" name="ground" width="2" height="2">
<data encoding="csv">
1,2,5,2147483649
</data>
</layer>
<objectgroup id="2" name="objs">
<object id="1" name="p" type="marker" x="1" y="2">
<point/>
</object>
<object id="2" type="poly" x="0" y="0">
<polygon points="0,0 8,0 8,8"/>
<properties>
<property name="tag" value="zone"/>
</properties>
</object>
</objectgroup>
<imagelayer id="3" name="bg">
<image source="sky.png"/>
</imagelayer>
<group id="4" name="grp">
<layer id="5" name="over" width="2" height="2">
<data encoding="csv">
0,0,1,1
</data>
</layer>
</group>
<properties>
<property name="name" value="demo"/>
<property name="score" type="int" value="10"/>
</properties>
</map>)");

const flight::String expected_rich_tsx(R"(<?xml version="1.0" encoding="UTF-8"?>
<tileset name="base" tilewidth="16" tileheight="16" tilecount="4" columns="2" objectalignment="center">
<tileoffset x="1" y="2"/>
<image source="base.png" width="32" height="32"/>
<tile id="1" type="wall">
<properties>
<property name="solid" type="bool" value="true"/>
</properties>
</tile>
</tileset>)");

const flight::String expected_rich_tmj(R"({
  "compressionlevel": -1,
  "height": 2,
  "infinite": false,
  "layers": [
    {
      "id": 1,
      "name": "ground",
      "opacity": 1,
      "type": "tilelayer",
      "visible": true,
      "x": 0,
      "y": 0,
      "data": [
        1,
        2,
        5,
        2147483649
      ],
      "height": 2,
      "width": 2
    },
    {
      "id": 2,
      "name": "objs",
      "opacity": 1,
      "type": "objectgroup",
      "visible": true,
      "x": 0,
      "y": 0,
      "objects": [
        {
          "height": 0,
          "id": 1,
          "name": "p",
          "rotation": 0,
          "type": "marker",
          "visible": true,
          "width": 0,
          "x": 1,
          "y": 2,
          "point": true
        },
        {
          "height": 0,
          "id": 2,
          "name": "",
          "rotation": 0,
          "type": "poly",
          "visible": true,
          "width": 0,
          "x": 0,
          "y": 0,
          "polygon": [
            {
              "x": 0,
              "y": 0
            },
            {
              "x": 8,
              "y": 0
            },
            {
              "x": 8,
              "y": 8
            }
          ],
          "properties": [
            {
              "name": "tag",
              "type": "string",
              "value": "zone"
            }
          ]
        }
      ]
    },
    {
      "id": 3,
      "name": "bg",
      "opacity": 1,
      "type": "imagelayer",
      "visible": true,
      "x": 0,
      "y": 0,
      "image": "sky.png"
    },
    {
      "id": 4,
      "name": "grp",
      "opacity": 1,
      "type": "group",
      "visible": true,
      "x": 0,
      "y": 0,
      "layers": [
        {
          "id": 5,
          "name": "over",
          "opacity": 1,
          "type": "tilelayer",
          "visible": true,
          "x": 0,
          "y": 0,
          "data": [
            0,
            0,
            1,
            1
          ],
          "height": 2,
          "width": 2
        }
      ]
    }
  ],
  "nextlayerid": 6,
  "nextobjectid": 3,
  "orientation": "orthogonal",
  "renderorder": "right-down",
  "tiledversion": "1.10.2",
  "tileheight": 16,
  "tilesets": [
    {
      "firstgid": 1,
      "columns": 2,
      "margin": 0,
      "name": "base",
      "spacing": 0,
      "tilecount": 4,
      "tileheight": 16,
      "tilewidth": 16,
      "image": "base.png",
      "imageheight": 32,
      "imagewidth": 32,
      "tiles": [
        {
          "id": 0,
          "type": "lava",
          "animation": [
            {
              "duration": 200,
              "tileid": 0
            }
          ]
        }
      ]
    },
    {
      "firstgid": 5,
      "source": "extra.tsx"
    }
  ],
  "tilewidth": 16,
  "type": "map",
  "version": "1.10",
  "width": 2,
  "backgroundcolor": "#ff204060",
  "properties": [
    {
      "name": "name",
      "type": "string",
      "value": "demo"
    },
    {
      "name": "score",
      "type": "int",
      "value": 10
    }
  ]
})");

const flight::String expected_rich_tsj(R"({
  "columns": 2,
  "margin": 0,
  "name": "base",
  "spacing": 0,
  "tilecount": 4,
  "tileheight": 16,
  "tilewidth": 16,
  "image": "base.png",
  "imageheight": 32,
  "imagewidth": 32,
  "tileoffset": {
    "x": 1,
    "y": 2
  },
  "objectalignment": "center",
  "tiles": [
    {
      "id": 1,
      "type": "wall",
      "properties": [
        {
          "name": "solid",
          "type": "bool",
          "value": true
        }
      ]
    }
  ],
  "type": "tileset"
})");

void color_and_gid_evidence() {
  check(formats::format_tiled_color(0xff8800ff) == flight::String("#ffff8800"),
        "formatTiledColor emits alpha-first AARRGGBB");
  check(formats::parse_tiled_color(flight::String("#8000ff00")) ==
            std::optional<double>{0x00ff0080},
        "parseTiledColor converts AARRGGBB to packed RGBA");
  check(formats::parse_tiled_color(flight::String("ff8800")) ==
            std::optional<double>{0xff8800ff},
        "parseTiledColor accepts an unprefixed opaque color");
  check(!formats::parse_tiled_color(flight::String("#12345")).has_value() &&
            !formats::parse_tiled_color(flight::String("#nothex")).has_value(),
        "parseTiledColor rejects malformed lengths and digits");

  const auto gid = formats::decode_tiled_gid(0xe0000007);
  check(gid->tile_id == 7.0 && gid->flip_horizontal && gid->flip_vertical && gid->flip_diagonal,
        "decodeTiledGid separates all flip flags from the tile id");
  const auto plain = formats::decode_tiled_gid(42.0);
  check(plain->tile_id == 42.0 && !plain->flip_horizontal && !plain->flip_vertical &&
            !plain->flip_diagonal,
        "decodeTiledGid preserves an unflagged id");
}

void layer_data_evidence() {
  const std::vector<double> expected{1.0, 5.0, 2147483649.0, 6.0};
  const auto csv = formats::decode_tiled_csv_layer(
      flight::String("\n1, 5,\n2147483649, 6\n"));
  const auto base64 = formats::decode_tiled_base64_layer(
      flight::String("AQAAAAUAAAABAACABgAAAA=="), std::nullopt);
  check(gids_are(csv, expected), "decodeTiledCsvLayer preserves exact unsigned GIDs");
  check(base64.has_value() && gids_are(base64.value(), expected),
        "decodeTiledBase64Layer decodes little-endian unsigned GIDs");

  std::vector<std::string> guarded;
  formats::set_tiled_layer_data_guard(std::function<void(flight::String, flight::String)>(
      [&](flight::String reason, flight::String compression) {
        guarded.push_back(reason.to_utf8() + ":" + compression.to_utf8());
      }));
  const auto missing = formats::decode_tiled_base64_layer(
      flight::String("AQAAAAIAAAA="), flight::String("zlib"));
  const types::TiledInflate failing_inflate = [](flight::Uint8Array, flight::String) {
    return std::optional<flight::Uint8Array>{};
  };
  const auto failed = formats::decode_tiled_base64_layer(
      flight::String("AQAAAAIAAAA="), flight::String("gzip"), failing_inflate);
  formats::set_tiled_layer_data_guard(std::nullopt);
  formats::decode_tiled_base64_layer(
      flight::String("AQAAAAIAAAA="), flight::String("zlib"));
  check(!missing.has_value() && !failed.has_value(),
        "compressed layer decode preserves zero-grid failure as absence");
  check(guarded == std::vector<std::string>({"compressed-without-inflate:zlib", "inflate-failed:gzip"}),
        "layer-data guard distinguishes missing and failed inflate seams exactly once");

  const auto missing_explanation = formats::explain_tiled_layer_data(
      flight::String("AQAAAAIAAAA="), flight::String("zlib"));
  const auto failed_explanation = formats::explain_tiled_layer_data(
      flight::String("AQAAAAIAAAA="), flight::String("gzip"), failing_inflate);
  check(missing_explanation->preserved_as_zero_grid &&
            missing_explanation->reason == flight::String("compressed-without-inflate"),
        "explainTiledLayerData reports a missing inflate seam");
  check(failed_explanation->preserved_as_zero_grid &&
            failed_explanation->reason == flight::String("inflate-failed"),
        "explainTiledLayerData reports a failed inflate seam");

  const types::TiledInflate identity_inflate = [](flight::Uint8Array bytes, flight::String) {
    return std::optional<flight::Uint8Array>{bytes};
  };
  const auto inflated = formats::decode_tiled_base64_layer(
      flight::String("AQAAAAIAAAA="), flight::String("gzip"), identity_inflate);
  check(inflated.has_value() && gids_are(inflated.value(), {1.0, 2.0}),
        "decodeTiledBase64Layer accepts a successful inflate seam");
}

void xml_parse_and_tmx_format_evidence() {
  const auto parsed = formats::parse_tiled_tmx(rich_tmx);
  check(parsed.has_value(), "parseTiledTmx accepts the upstream rich TMX fixture");
  if (!parsed.has_value()) return;
  const auto& map = parsed.value();
  check(map->version == flight::String("1.10") &&
            map->tiled_version == std::optional<flight::String>{flight::String("1.10.2")} &&
            map->orientation == flight::String("orthogonal") &&
            map->render_order == flight::String("right-down") && map->width == 2.0 &&
            map->height == 2.0 && map->tile_width == 16.0 && map->tile_height == 16.0 &&
            !map->infinite,
        "parseTiledTmx preserves exact map metadata");
  check(map->background_color == std::optional<double>{0x204060ff} &&
            map->properties.length == 2 && map->tilesets.length == 2 && map->layers.length == 4,
        "parseTiledTmx preserves map properties, color, tilesets, and every layer");

  const auto tile_layer = std::get<flight::Ref<types::TiledTileLayer>>(map->layers.element(0.0));
  const auto object_layer = std::get<flight::Ref<types::TiledObjectGroup>>(map->layers.element(1.0));
  const auto image_layer = std::get<flight::Ref<types::TiledImageLayer>>(map->layers.element(2.0));
  const auto group_layer = std::get<flight::Ref<types::TiledGroupLayer>>(map->layers.element(3.0));
  check(tile_layer->name == flight::String("ground") && tile_layer->width == 2.0 &&
            tile_layer->height == 2.0,
        "XML tile layer keeps its name and dimensions");
  check(gids_are(tile_layer->data, {1.0, 2.0, 5.0, 2147483649.0}),
        "XML tile layer keeps row order and flipped GID bits");
  check(object_layer->objects.length == 2 && object_layer->objects.element(0.0)->point &&
            object_layer->objects.element(1.0)->polygon.has_value() &&
            object_layer->objects.element(1.0)->polygon->length == 3,
        "XML object layer keeps point and polygon objects");
  check(image_layer->image == flight::String("sky.png") && !image_layer->repeat_x &&
            !image_layer->repeat_y,
        "XML image layer keeps source and repeat defaults");
  check(group_layer->layers.length == 1 &&
            std::holds_alternative<flight::Ref<types::TiledTileLayer>>(group_layer->layers.element(0.0)),
        "XML group layer keeps its nested tile layer");
  check(map->tilesets.element(0.0)->tileset.has_value() &&
            map->tilesets.element(0.0)->tileset.value()->name == flight::String("base") &&
            map->tilesets.element(1.0)->source ==
                std::optional<flight::String>{flight::String("extra.tsx")},
        "XML parser distinguishes embedded and external tilesets");

  const flight::String formatted = formats::format_tiled_tmx(readonly(map));
  check(formatted == expected_rich_tmx,
        "formatTiledTmx matches the current-pin rich fixture byte for byte");
  check(formatted.index_of(flight::String("<data encoding=\"csv\">\n1,2,5,2147483649\n</data>")) >= 0.0,
        "formatTiledTmx emits the exact CSV GID order");
  check(formatted.index_of(flight::String("<tileset firstgid=\"5\" source=\"extra.tsx\"/>")) >= 0.0 &&
            formatted.index_of(flight::String("firstgid=\"1\" name=\"base\"")) >= 0.0,
        "formatTiledTmx emits external and embedded tilesets in order");
  const auto reparsed = formats::parse_tiled_tmx(formatted);
  check(reparsed.has_value() && reparsed.value()->layers.length == 4 &&
            reparsed.value()->tilesets.length == 2 &&
            gids_are(std::get<flight::Ref<types::TiledTileLayer>>(
                         reparsed.value()->layers.element(0.0))->data,
                     {1.0, 2.0, 5.0, 2147483649.0}),
        "TMX parse-format-parse preserves the modeled document structure");

  const auto tileset = formats::parse_tiled_tileset(rich_tsx);
  check(tileset.has_value() && tileset.value()->name == flight::String("base") &&
            tileset.value()->object_alignment == flight::String("center") &&
            tileset.value()->tile_offset_x == 1.0 && tileset.value()->tile_offset_y == 2.0 &&
            tileset.value()->tiles.length == 1,
        "parseTiledTileset accepts the upstream rich TSX fixture");
  if (tileset.has_value()) {
    const flight::String formatted_tileset = formats::format_tiled_tileset(readonly(tileset.value()));
    check(formatted_tileset == expected_rich_tsx,
          "formatTiledTileset matches the current-pin rich fixture byte for byte");
    check(formatted_tileset.index_of(flight::String("firstgid")) < 0.0 &&
              formatted_tileset.index_of(flight::String("<tileset name=\"base\"")) >= 0.0 &&
              formats::parse_tiled_tileset(formatted_tileset).has_value(),
          "TSX parse-format-parse omits map-only firstgid and remains readable");
  }

  flight::Array<flight::Ref<types::ImportDiagnostic>> malformed_diagnostics;
  check(!formats::parse_tiled_tmx(
             flight::String("garbage"), std::nullopt,
             std::optional<flight::Array<flight::Ref<types::ImportDiagnostic>>>{malformed_diagnostics})
             .has_value(),
        "parseTiledTmx rejects malformed XML");
  check(malformed_diagnostics.length == 1 &&
            malformed_diagnostics.element(0.0)->kind == flight::String("tiled.xml-malformed") &&
            malformed_diagnostics.element(0.0)->origin == flight::String("parseTiledTmx") &&
            malformed_diagnostics.element(0.0)->severity == flight::String("Reject") &&
            malformed_diagnostics.element(0.0)->detail.has_value() &&
            std::get<flight::String>(malformed_diagnostics.element(0.0)->detail->get(
                                         flight::String("path")).value()) == flight::String("map"),
        "malformed TMX reports the exact rejection diagnostic and path");

  flight::Array<flight::Ref<types::ImportDiagnostic>> root_diagnostics;
  check(!formats::parse_tiled_tmx(
             flight::String("<tileset/>"), std::nullopt,
             std::optional<flight::Array<flight::Ref<types::ImportDiagnostic>>>{root_diagnostics})
             .has_value(),
        "parseTiledTmx rejects a tileset root");
  check(root_diagnostics.length == 1 &&
            root_diagnostics.element(0.0)->kind == flight::String("tiled.root-unexpected") &&
            std::get<flight::String>(root_diagnostics.element(0.0)->detail->get(
                                         flight::String("actual")).value()) == flight::String("tileset") &&
            std::get<flight::String>(root_diagnostics.element(0.0)->detail->get(
                                         flight::String("path")).value()) == flight::String("map"),
        "unexpected TMX root reports its exact actual and expected path");
  check(!formats::parse_tiled_tileset(flight::String("<map/>")).has_value(),
        "parseTiledTileset rejects a map root");
}

void json_parse_and_tmj_format_evidence() {
  const auto xml_map = formats::parse_tiled_tmx(rich_tmx);
  check(xml_map.has_value(), "TMJ evidence starts from the readable rich TMX fixture");
  if (!xml_map) return;

  const auto formatted = formats::format_tiled_tmj(readonly(*xml_map));
  check(formatted == expected_rich_tmj,
        "formatTiledTmj matches the current-pin rich fixture byte for byte");
  const auto parsed = formats::parse_tiled_tmj(formatted);
  check(parsed.has_value() && parsed.value()->layers.length == 4 &&
            parsed.value()->tilesets.length == 2 && parsed.value()->properties.length == 2,
        "parseTiledTmj restores the rich map's layers, tilesets, and properties");
  if (parsed) {
    const auto tile_layer =
        std::get<flight::Ref<types::TiledTileLayer>>(parsed.value()->layers.element(0.0));
    const auto group =
        std::get<flight::Ref<types::TiledGroupLayer>>(parsed.value()->layers.element(3.0));
    check(tile_layer->name == flight::String("ground") &&
              gids_are(tile_layer->data, {1.0, 2.0, 5.0, 2147483649.0}) &&
              group->layers.length == 1,
          "TMJ round trip preserves common layer fields, unsigned GIDs, and nesting");
    check(formats::format_tiled_tmj(readonly(parsed.value())) == expected_rich_tmj,
          "TMJ parse-format-parse is byte stable");
  }

  const auto xml_tileset = formats::parse_tiled_tileset(rich_tsx);
  check(xml_tileset.has_value(), "TSJ evidence starts from the readable rich TSX fixture");
  if (xml_tileset) {
    const auto formatted_tileset = formats::format_tiled_tileset_json(readonly(*xml_tileset));
    check(formatted_tileset == expected_rich_tsj,
          "formatTiledTilesetJson matches the current-pin rich fixture byte for byte");
    const auto parsed_tileset = formats::parse_tiled_tileset_json(formatted_tileset);
    check(parsed_tileset.has_value() && parsed_tileset.value()->name == flight::String("base") &&
              parsed_tileset.value()->tile_offset_x == 1.0 &&
              parsed_tileset.value()->tile_offset_y == 2.0 &&
              parsed_tileset.value()->tiles.length == 1,
          "TSJ parse-format-parse preserves offset and per-tile properties");
    if (parsed_tileset) {
      check(formats::format_tiled_tileset_json(readonly(parsed_tileset.value())) == expected_rich_tsj,
            "TSJ parse-format-parse is byte stable");
    }
  }

  flight::Array<flight::Ref<types::ImportDiagnostic>> malformed;
  check(!formats::parse_tiled_tmj(
             flight::String("{ not json"), std::nullopt,
             std::optional<flight::Array<flight::Ref<types::ImportDiagnostic>>>{malformed})
             .has_value() &&
            malformed.length == 1 &&
            malformed.element(0.0)->kind == flight::String("tiled.json-malformed") &&
            malformed.element(0.0)->origin == flight::String("parseTiledTmj") &&
            std::get<flight::String>(malformed.element(0.0)->detail->get(
                                         flight::String("path")).value()) == flight::String("map"),
        "malformed TMJ reports the exact rejection diagnostic");

  flight::Array<flight::Ref<types::ImportDiagnostic>> missing;
  const auto empty_map = formats::parse_tiled_tmj(
      flight::String("{}"), std::nullopt,
      std::optional<flight::Array<flight::Ref<types::ImportDiagnostic>>>{missing});
  check(empty_map.has_value() && missing.length == 4 &&
            std::get<flight::String>(missing.element(0.0)->detail->get(
                                         flight::String("path")).value()) == flight::String("map.height") &&
            std::get<flight::String>(missing.element(3.0)->detail->get(
                                         flight::String("path")).value()) == flight::String("map.width"),
        "TMJ required-field recovery reports all four exact paths in order");
  check(!formats::parse_tiled_tmj(flight::String("[]")).has_value() &&
            !formats::parse_tiled_tileset_json(flight::String("42")).has_value(),
        "JSON front ends reject non-object roots");
}

flight::Ref<types::TilemapData> resolved_layout(double tile_size) {
  return flight::make_ref<types::TilemapData>(types::TilemapData{
      .entity_runtime_key = std::nullopt,
      .atlas = std::nullopt,
      .columns = 0.0,
      .rows = 0.0,
      .tile_height = tile_size,
      .tile_width = tile_size,
      .tiles = flight::Int16Array{},
      .material_data = std::nullopt,
  });
}

flight::Ref<types::TiledMap> project_map(flight::String data, double first_gid = 1.0) {
  const auto text = flight::String("<map version=\"1\" width=\"2\" height=\"2\" tilewidth=\"16\" tileheight=\"16\">") +
                    flight::String("<tileset firstgid=\"") + flight::to_string(first_gid) +
                    flight::String("\" source=\"a.tsx\"/>") +
                    flight::String("<tileset firstgid=\"5\" source=\"b.tsx\"/>") +
                    flight::String("<layer id=\"1\" name=\"g\" width=\"2\" height=\"2\"><data encoding=\"csv\">") +
                    data + flight::String("</data></layer>") +
                    flight::String("<objectgroup id=\"2\" name=\"o\"><object id=\"1\" x=\"0\" y=\"0\"/></objectgroup></map>");
  return formats::parse_tiled_tmx(text).value();
}

void project_evidence() {
  const auto layout_a = resolved_layout(16.0);
  const auto layout_b = resolved_layout(16.0);
  int resolved_a = 0;
  int resolved_b = 0;
  const types::TiledTilesetResolver resolve =
      [&](formats::ProjectTilesetView ref) -> std::optional<flight::Ref<types::TilemapData>> {
    const auto first_gid = flight::row_get<flight::RowKey<"firstGid">>(ref);
    if (first_gid == 1.0) {
      ++resolved_a;
      return layout_a;
    }
    if (first_gid == 5.0) {
      ++resolved_b;
      return layout_b;
    }
    return std::nullopt;
  };

  flight::Array<flight::Ref<types::ImportDiagnostic>> clean_diagnostics;
  const auto split = formats::build_tilemap_layers_from_tiled(
      readonly(project_map(flight::String("1,5,2147483649,6"))), 0.0, resolve,
      std::optional<flight::Array<flight::Ref<types::ImportDiagnostic>>>{clean_diagnostics});
  check(split.has_value() && split->length == 2 &&
            tiles_are(split->element(0.0)->tiles, {0.0, -1.0, 0.0, -1.0}) &&
            tiles_are(split->element(1.0)->tiles, {-1.0, 0.0, -1.0, 1.0}),
        "project splits two tilesets into stable local-id grids and strips flip flags");
  check(split.has_value() && split->element(1.0)->columns == 2.0 &&
            split->element(1.0)->rows == 2.0 && split->element(1.0)->tile_width == 16.0 &&
            split->element(1.0)->tile_height == 16.0 && resolved_a == 1 && resolved_b == 1 &&
            clean_diagnostics.length == 0,
        "project preserves dimensions, resolves each firstgid once, and stays silent on success");

  flight::Array<flight::Ref<types::ImportDiagnostic>> unresolved_diagnostics;
  int unresolved_calls = 0;
  const types::TiledTilesetResolver only_a =
      [&](formats::ProjectTilesetView ref) -> std::optional<flight::Ref<types::TilemapData>> {
    const auto first_gid = flight::row_get<flight::RowKey<"firstGid">>(ref);
    if (first_gid == 1.0) return layout_a;
    ++unresolved_calls;
    return std::nullopt;
  };
  const auto partial = formats::build_tilemap_layers_from_tiled(
      readonly(project_map(flight::String("1,5,1,5"))), 0.0, only_a,
      std::optional<flight::Array<flight::Ref<types::ImportDiagnostic>>>{unresolved_diagnostics});
  check(partial.has_value() && partial->length == 1 &&
            tiles_are(partial->element(0.0)->tiles, {0.0, -1.0, 0.0, -1.0}) &&
            unresolved_calls == 1,
        "project caches an unresolved tileset and leaves its cells empty");
  check(unresolved_diagnostics.length == 1 &&
            unresolved_diagnostics.element(0.0)->kind == flight::String("tiled.tileset-unresolved") &&
            std::get<double>(unresolved_diagnostics.element(0.0)->detail->get(
                                 flight::String("cells")).value()) == 2.0 &&
            std::get<double>(unresolved_diagnostics.element(0.0)->detail->get(
                                 flight::String("tilesets")).value()) == 1.0,
        "project reports unresolved cells once per layer with exact counts");

  flight::Array<flight::Ref<types::ImportDiagnostic>> outside_diagnostics;
  const auto outside = formats::build_tilemap_layers_from_tiled(
      readonly(project_map(flight::String("1,5"), 5.0)), 0.0, resolve,
      std::optional<flight::Array<flight::Ref<types::ImportDiagnostic>>>{outside_diagnostics});
  check(outside.has_value() && outside_diagnostics.length == 1 &&
            outside_diagnostics.element(0.0)->kind ==
                flight::String("tiled.tile-outside-every-tileset") &&
            std::get<double>(outside_diagnostics.element(0.0)->detail->get(
                                 flight::String("cells")).value()) == 1.0,
        "project aggregates GIDs below every declared tileset into one diagnostic");

  const types::TiledTilesetResolver none =
      [](formats::ProjectTilesetView) -> std::optional<flight::Ref<types::TilemapData>> {
    return std::nullopt;
  };
  const auto map = project_map(flight::String("1,5,2147483649,6"));
  check(!formats::build_tilemap_layers_from_tiled(readonly(map), 0.0, none).has_value(),
        "project returns absence when no referenced tileset resolves");
  check(!formats::build_tilemap_layers_from_tiled(readonly(map), 1.0, resolve).has_value() &&
            !formats::build_tilemap_layers_from_tiled(readonly(map), 9.0, resolve).has_value(),
        "project rejects non-tile and out-of-range layers");
}

} // namespace

int main() {
  color_and_gid_evidence();
  layer_data_evidence();
  xml_parse_and_tmx_format_evidence();
  json_parse_and_tmj_format_evidence();
  project_evidence();
  if (failures == 0) {
    std::cout << "generated tilemap-formats tests passed\n";
    return 0;
  }
  std::cerr << failures << " generated tilemap-formats test(s) failed\n";
  return 1;
}
