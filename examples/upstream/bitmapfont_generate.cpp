// Naive C++ port of @flighthq/example-bitmapfont-generate
// Ported from .dependencies/flight/examples/packages/bitmapfont-generate/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/scene2d/sprite.hpp>
#include <flight/text/text_label.hpp>
#include <flight/texture/texture.hpp>
#include <flight/bitmaptext/bitmap_text.hpp>
#include <flight/glyphatlas/glyph_atlas.hpp>
#include <flight/node/node_color_adjustments.hpp>

#include <iostream>

int main() {
  using namespace flight::node;
  using namespace flight::scene2d;
  using namespace flight::bitmaptext;
  using namespace flight::glyphatlas;

  auto root = create_display_object(std::nullopt);

  auto atlas = create_glyph_atlas({
    .fontFamily = flight::String("sans-serif"),
    .fontSize = 52.0,
    .height = 320.0,
    .padding = 2.0,
    .width = 640.0,
  });

  auto glyphSource = create_glyph_source_from_glyph_atlas(atlas);

  auto text1 = create_bitmap_text(glyphSource, {
    .text = flight::String("FLIGHT"),
    .letterSpacing = 4.0,
  });
  flight::node::set_node_color_adjustments_tint(text1, 0x00d9ffff);
  text1->x = 36.0;
  text1->y = 32.0;
  invalidate_node_local_transform(text1);
  update_bitmap_text(text1);
  add_node_child(root, text1);

  auto text2 = create_bitmap_text(glyphSource, {
    .text = flight::String("Runtime Glyph Atlas"),
    .letterSpacing = 1.0,
  });
  flight::node::set_node_color_adjustments_tint(text2, 0xffd166ff);
  text2->x = 36.0;
  text2->y = 112.0;
  invalidate_node_local_transform(text2);
  update_bitmap_text(text2);
  add_node_child(root, text2);

  auto text3 = create_bitmap_text(glyphSource, {
    .text = flight::String("ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
    .letterSpacing = 2.0,
    .wrapWidth = 700.0,
  });
  flight::node::set_node_color_adjustments_tint(text3, 0xef476fff);
  text3->x = 36.0;
  text3->y = 188.0;
  invalidate_node_local_transform(text3);
  update_bitmap_text(text3);
  add_node_child(root, text3);

  auto text4 = create_bitmap_text(glyphSource, {
    .text = flight::String("0123456789  Lazy . Packed . Reused"),
    .letterSpacing = 1.0,
    .lineHeight = 1.15,
    .wrapWidth = 700.0,
  });
  flight::node::set_node_color_adjustments_tint(text4, 0x06d6a0ff);
  text4->x = 36.0;
  text4->y = 318.0;
  invalidate_node_local_transform(text4);
  update_bitmap_text(text4);
  add_node_child(root, text4);

  // Re-layout after atlas may have repacked
  refresh_bitmap_text_glyph_layout(text1);
  refresh_bitmap_text_glyph_layout(text2);
  refresh_bitmap_text_glyph_layout(text3);
  refresh_bitmap_text_glyph_layout(text4);

  // Atlas preview sprite (would need bitmap materialization in C++)
  auto atlasBitmap = get_glyph_atlas_bitmap(atlas);

  std::cout << "Flight bitmapfont-generate example (naive C++ port): "
            << "4 bitmap text strings rendered via runtime glyph atlas.\n";
  return 0;
}
