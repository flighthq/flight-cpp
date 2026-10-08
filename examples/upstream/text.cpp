// Naive C++ port of @flighthq/example-text
// Ported from .dependencies/flight/examples/packages/text/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/text/text_label.hpp>
#include <flight/text/rich_text.hpp>

#include <iostream>

int main() {
  using namespace flight::node;
  using namespace flight::scene2d;

  auto root = create_display_object(std::nullopt);

  auto heading = flight::text::create_text_label();
  heading->x = 30.0;
  heading->y = 20.0;
  heading->data.text = flight::String("TEXT LABEL BASICS");
  invalidate_node_local_transform(heading);
  add_node_child(root, heading);

  auto label1 = flight::text::create_text_label();
  label1->data.text = flight::String("Hello, Flight!");
  label1->x = 30.0;
  label1->y = 60.0;
  invalidate_node_local_transform(label1);
  add_node_child(root, label1);

  auto label2 = flight::text::create_text_label();
  label2->data.text = flight::String("Smaller text in gray");
  label2->x = 30.0;
  label2->y = 100.0;
  invalidate_node_local_transform(label2);
  add_node_child(root, label2);

  auto label3 = flight::text::create_text_label();
  label3->data.text = flight::String("Bold heading style");
  label3->x = 30.0;
  label3->y = 140.0;
  invalidate_node_local_transform(label3);
  add_node_child(root, label3);

  auto headingRich = flight::text::create_text_label();
  headingRich->data.text = flight::String("RICH TEXT");
  headingRich->x = 30.0;
  headingRich->y = 220.0;
  invalidate_node_local_transform(headingRich);
  add_node_child(root, headingRich);

  auto richText = flight::text::create_rich_text();
  auto markup = flight::text::parse_text_markup(
    flight::String("This is <b>bold</b> and <i>italic</i> and <u>underlined</u>."));
  flight::text::set_rich_text_content(richText, markup);
  richText->x = 30.0;
  richText->y = 260.0;
  invalidate_node_local_transform(richText);
  add_node_child(root, richText);

  auto headingWrap = flight::text::create_text_label();
  headingWrap->data.text = flight::String("WORD WRAP & ALIGNMENT");
  headingWrap->x = 30.0;
  headingWrap->y = 340.0;
  invalidate_node_local_transform(headingWrap);
  add_node_child(root, headingWrap);

  auto wrapLabel = flight::text::create_text_label();
  wrapLabel->data.text = flight::String(
    "This is a longer paragraph of text that should wrap within "
    "a fixed width region. Word wrapping distributes text across "
    "multiple lines when it exceeds the available horizontal space.");
  wrapLabel->x = 30.0;
  wrapLabel->y = 380.0;
  invalidate_node_local_transform(wrapLabel);
  add_node_child(root, wrapLabel);

  std::cout << "Flight text example (naive C++ port): "
            << "7 text elements created.\n";
  return 0;
}
