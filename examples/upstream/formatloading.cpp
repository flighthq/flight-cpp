// Naive C++ port of @flighthq/example-formatloading
// Ported from .dependencies/flight/examples/packages/formatloading/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>
#include <flight/mesh/mesh_geometry.hpp>
#include <flight/scene3d_formats/gltf_parse.hpp>
#include <flight/spritesheet_formats/texture_packer_parse.hpp>
#include <flight/tilemap_formats/tiled_json_parse.hpp>

#include <iostream>
#include <string>

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;

  auto root = create_display_object(std::nullopt);

  // Synthetic glTF fixture (JSON)
  flight::String gltfFixture(R"({
    "accessors": [{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],
    "asset": {"generator":"Flight fixture","version":"2.0"},
    "bufferViews": [{"buffer":0,"byteLength":36}],
    "buffers": [{"byteLength":36}],
    "meshes": [{"name":"Triangle","primitives":[{"attributes":{"POSITION":0}}]}],
    "nodes": [{"mesh":0,"name":"Triangle Node"}],
    "scene": 0,
    "scenes": [{"name":"Fixture","nodes":[0]}]
  })");

  auto gltf = flight::scene3d_formats::parse_gltf(gltfFixture);

  // Synthetic TexturePacker fixture
  flight::String tpFixture(R"({
    "frames": {
      "idle.png": {"frame":{"h":64,"w":56,"x":0,"y":0},"rotated":false,"sourceSize":{"h":64,"w":64}},
      "jump.png": {"frame":{"h":64,"w":64,"x":56,"y":0},"rotated":false,"sourceSize":{"h":64,"w":64}}
    },
    "meta": {"format":"RGBA8888","image":"atlas.png","size":{"h":64,"w":128},"version":"1.0"}
  })");

  auto spritesheet = flight::spritesheet_formats::parse_texture_packer_spritesheet(tpFixture);

  // Synthetic Tiled TMJ fixture
  flight::String tiledFixture(R"({
    "height":3,"width":4,"tileheight":32,"tilewidth":32,
    "orientation":"orthogonal","renderorder":"right-down",
    "layers":[{"data":[1,2,1,2,2,1,2,1,1,2,1,2],"height":3,"width":4,"name":"terrain","type":"tilelayer"}],
    "tilesets":[{"firstgid":1,"name":"terrain","tilecount":2,"tileheight":32,"tilewidth":32}]
  })");

  auto tiled = flight::tilemap_formats::parse_tiled_tmj(tiledFixture);

  // Draw panels for each format
  auto panel1 = create_shape(std::nullopt);
  append_shape_begin_fill(panel1, 0x192438ff);
  append_shape_rectangle(panel1, 24.0, 96.0, 232.0, 390.0);
  append_shape_end_fill(panel1);
  append_shape_begin_fill(panel1, 0x61dafbff);
  append_shape_rectangle(panel1, 24.0, 96.0, 232.0, 5.0);
  append_shape_end_fill(panel1);
  add_node_child(root, panel1);

  auto panel2 = create_shape(std::nullopt);
  append_shape_begin_fill(panel2, 0x192438ff);
  append_shape_rectangle(panel2, 284.0, 96.0, 232.0, 390.0);
  append_shape_end_fill(panel2);
  append_shape_begin_fill(panel2, 0xffc857ff);
  append_shape_rectangle(panel2, 284.0, 96.0, 232.0, 5.0);
  append_shape_end_fill(panel2);
  add_node_child(root, panel2);

  auto panel3 = create_shape(std::nullopt);
  append_shape_begin_fill(panel3, 0x192438ff);
  append_shape_rectangle(panel3, 544.0, 96.0, 232.0, 390.0);
  append_shape_end_fill(panel3);
  append_shape_begin_fill(panel3, 0x55d187ff);
  append_shape_rectangle(panel3, 544.0, 96.0, 232.0, 5.0);
  append_shape_end_fill(panel3);
  add_node_child(root, panel3);

  // Labels
  auto title = flight::text::create_text_label();
  title->data.text = flight::String("Standard Format Loading");
  title->x = 24.0;
  title->y = 20.0;
  invalidate_node_local_transform(title);
  add_node_child(root, title);

  auto gltfLabel = flight::text::create_text_label();
  gltfLabel->data.text = flight::String("glTF 2.0");
  gltfLabel->x = 42.0;
  gltfLabel->y = 118.0;
  invalidate_node_local_transform(gltfLabel);
  add_node_child(root, gltfLabel);

  auto tpLabel = flight::text::create_text_label();
  tpLabel->data.text = flight::String("TexturePacker");
  tpLabel->x = 302.0;
  tpLabel->y = 118.0;
  invalidate_node_local_transform(tpLabel);
  add_node_child(root, tpLabel);

  auto tiledLabel = flight::text::create_text_label();
  tiledLabel->data.text = flight::String("Tiled TMJ");
  tiledLabel->x = 562.0;
  tiledLabel->y = 118.0;
  invalidate_node_local_transform(tiledLabel);
  add_node_child(root, tiledLabel);

  // Draw parsed triangle from glTF
  auto gltfShape = create_shape(std::nullopt);
  append_shape_begin_fill(gltfShape, 0x3478c9ff);
  append_shape_move_to(gltfShape, 62.0, 358.0);
  append_shape_line_to(gltfShape, 218.0, 358.0);
  append_shape_line_to(gltfShape, 140.0, 202.0);
  append_shape_line_to(gltfShape, 62.0, 358.0);
  append_shape_end_fill(gltfShape);
  append_shape_line_style(gltfShape, 3.0, 0x8ee7ffff);
  append_shape_move_to(gltfShape, 62.0, 358.0);
  append_shape_line_to(gltfShape, 218.0, 358.0);
  append_shape_line_to(gltfShape, 140.0, 202.0);
  append_shape_line_to(gltfShape, 62.0, 358.0);
  add_node_child(root, gltfShape);

  // Draw parsed atlas layout from TexturePacker
  auto atlasShape = create_shape(std::nullopt);
  append_shape_begin_fill(atlasShape, 0x0f1726ff);
  append_shape_rectangle(atlasShape, 310.0, 190.0, 198.0, 99.0);
  append_shape_end_fill(atlasShape);
  append_shape_begin_fill(atlasShape, 0xf08a5dff);
  append_shape_rectangle(atlasShape, 310.0, 190.0, 87.0, 99.0);
  append_shape_end_fill(atlasShape);
  append_shape_begin_fill(atlasShape, 0xf9c74fff);
  append_shape_rectangle(atlasShape, 397.0, 190.0, 99.0, 99.0);
  append_shape_end_fill(atlasShape);
  add_node_child(root, atlasShape);

  // Draw parsed tilemap
  auto tileShape = create_shape(std::nullopt);
  constexpr double cellSize = 30.0;
  constexpr double mapX = 570.0;
  constexpr double mapY = 190.0;
  std::uint32_t tileColors[] = {0x3f8f5fff, 0x4b84c6ff};
  int tileData[] = {1,2,1,2,2,1,2,1,1,2,1,2};
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 4; ++col) {
      int gid = tileData[row * 4 + col];
      append_shape_begin_fill(tileShape, tileColors[gid - 1]);
      append_shape_rectangle(tileShape,
        mapX + col * cellSize, mapY + row * cellSize,
        cellSize - 2.0, cellSize - 2.0);
      append_shape_end_fill(tileShape);
    }
  }
  add_node_child(root, tileShape);

  std::cout << "Flight formatloading example (naive C++ port): "
            << "3 format parsers exercised.\n";
  return 0;
}
