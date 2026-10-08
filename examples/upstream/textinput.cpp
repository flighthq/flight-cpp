// Naive C++ port of @flighthq/example-textinput
// Ported from .dependencies/flight/examples/packages/textinput/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>
#include <flight/text/rich_text.hpp>
#include <flight/textinput/text_input.hpp>
#include <flight/signals/connect.hpp>
#include <flight/app/app_loop.hpp>

#include <iostream>

namespace {

constexpr double FIELD_WIDTH = 340.0;
constexpr double FIELD_HEIGHT = 28.0;
constexpr double FIELD_X = 30.0;
constexpr double LABEL_X = 30.0;
constexpr double FIELD_GAP = 70.0;
constexpr double START_Y = 40.0;

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::shape;
  using namespace flight::scene2d;

  auto root = create_display_object(std::nullopt);

  // Three editable text fields: normal, numeric-only, and password.
  auto normalField = flight::text::create_rich_text();
  normalField->x = FIELD_X;
  normalField->y = START_Y + 22.0;
  normalField->data.width = FIELD_WIDTH;
  normalField->data.height = FIELD_HEIGHT;
  normalField->data.multiline = false;
  normalField->data.selectable = true;
  normalField->data.background = true;
  normalField->data.backgroundColor = 0xffffffff;
  normalField->data.border = true;
  normalField->data.borderColor = 0x999999ff;
  invalidate_node_local_transform(normalField);

  auto numericField = flight::text::create_rich_text();
  numericField->x = FIELD_X;
  numericField->y = START_Y + FIELD_GAP + 22.0;
  numericField->data.width = FIELD_WIDTH;
  numericField->data.height = FIELD_HEIGHT;
  numericField->data.multiline = false;
  numericField->data.selectable = true;
  numericField->data.background = true;
  numericField->data.backgroundColor = 0xffffffff;
  invalidate_node_local_transform(numericField);

  auto passwordField = flight::text::create_rich_text();
  passwordField->x = FIELD_X;
  passwordField->y = START_Y + FIELD_GAP * 2.0 + 22.0;
  passwordField->data.width = FIELD_WIDTH;
  passwordField->data.height = FIELD_HEIGHT;
  passwordField->data.multiline = false;
  passwordField->data.selectable = true;
  passwordField->data.background = true;
  passwordField->data.backgroundColor = 0xffffffff;
  invalidate_node_local_transform(passwordField);

  // Enable text input with appropriate options.
  flight::textinput::enable_text_input(normalField);
  flight::textinput::enable_text_input(numericField);
  flight::textinput::enable_text_input(passwordField);

  // Seed editable values.
  flight::textinput::insert_text_input(normalField, flight::String("Edit this Flight text"));
  flight::textinput::insert_text_input(numericField, flight::String("2026"));
  flight::textinput::insert_text_input(passwordField, flight::String("flightdeck"));
  flight::textinput::set_text_input_selection(normalField, 5, 16);

  // Labels above each field.
  auto normalLabel = flight::text::create_text_label();
  normalLabel->data.text = flight::String("Normal Text Field");
  normalLabel->x = LABEL_X;
  normalLabel->y = START_Y;
  invalidate_node_local_transform(normalLabel);

  auto numericLabel = flight::text::create_text_label();
  numericLabel->data.text = flight::String("Numeric Only (digits 0-9)");
  numericLabel->x = LABEL_X;
  numericLabel->y = START_Y + FIELD_GAP;
  invalidate_node_local_transform(numericLabel);

  auto passwordLabel = flight::text::create_text_label();
  passwordLabel->data.text = flight::String("Password Field");
  passwordLabel->x = LABEL_X;
  passwordLabel->y = START_Y + FIELD_GAP * 2.0;
  invalidate_node_local_transform(passwordLabel);

  // Focus highlight shape.
  auto focusHighlight = create_shape(std::nullopt);

  // HUD text for state info.
  auto hudText = flight::text::create_text_label();
  hudText->x = 30.0;
  hudText->y = START_Y + FIELD_GAP * 3.0 + 10.0;
  invalidate_node_local_transform(hudText);

  // Instructions label.
  auto instructionsText = flight::text::create_text_label();
  instructionsText->data.text = flight::String(
    "Click a field to focus. Type to enter text.\n"
    "Arrow keys: move caret | Shift+Arrow: select\n"
    "Ctrl+A: select all | Ctrl+Z: undo | Ctrl+Y: redo\n"
    "Backspace/Delete: delete | Ctrl+Backspace: delete word");
  instructionsText->x = 30.0;
  instructionsText->y = START_Y + FIELD_GAP * 3.0 + 110.0;
  invalidate_node_local_transform(instructionsText);

  // Scene graph assembly.
  add_node_child(root, focusHighlight);
  add_node_child(root, normalLabel);
  add_node_child(root, normalField);
  add_node_child(root, numericLabel);
  add_node_child(root, numericField);
  add_node_child(root, passwordLabel);
  add_node_child(root, passwordField);
  add_node_child(root, hudText);
  add_node_child(root, instructionsText);

  // Query text input state.
  auto displayText = flight::textinput::get_text_input_display_text(normalField);
  auto caretIndex = flight::textinput::get_text_input_caret_index(normalField);
  auto selBegin = flight::textinput::get_text_input_selection_begin_index(normalField);
  auto selEnd = flight::textinput::get_text_input_selection_end_index(normalField);

  std::cout << "Flight textinput example (naive C++ port): "
            << "3 text input fields created, "
            << "caret at " << caretIndex << ", "
            << "selection [" << selBegin << ", " << selEnd << "]\n";
  return 0;
}
