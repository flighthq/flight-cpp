// Native SDL port of @flighthq/example-textinput.
// Source: .dependencies/flight/examples/packages/textinput/src/app.ts

#include "sdl_host.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;

constexpr double field_width = 340.0;
constexpr double field_height = 28.0;
constexpr double field_x = 30.0;
constexpr double field_gap = 70.0;
constexpr double start_y = 40.0;

struct Field final {
  std::string name;
  std::string label;
  std::string value;
  double y{};
  bool digits_only{};
  bool password{};
  std::size_t selection_begin{};
  std::size_t selection_end{};
  std::vector<std::string> undo;
  std::vector<std::string> redo;
};

class TextInputExample final {
 public:
  TextInputExample()
      : fields_{{
            {"Normal", "Normal Text Field", "Edit this Flight text", start_y + 22.0, false, false, 5, 16, {}, {}},
            {"Numeric Only", "Numeric Only (digits 0-9)", "2026", start_y + field_gap + 22.0, true, false, 4, 4, {}, {}},
            {"Password", "Password Field", "flightdeck", start_y + field_gap * 2.0 + 22.0, false, true, 10, 10, {}, {}},
        }} {}

  void frame(const Frame& frame) {
    if (!text_input_started_) {
      frame.canvas.start_text_input();
      text_input_started_ = true;
    }
    handle_pointer(frame);
    handle_keyboard(frame);
    draw(frame.canvas, frame.time);
  }

 private:
  [[nodiscard]] Field* focused() {
    return focus_ < fields_.size() ? &fields_[focus_] : nullptr;
  }

  void handle_pointer(const Frame& frame) {
    if (!frame.input.pointer_pressed) return;
    focus_ = fields_.size();
    for (std::size_t index = 0; index < fields_.size(); ++index) {
      if (flight::examples::contains(
              field_x,
              fields_[index].y,
              field_width,
              field_height,
              frame.input.pointer_x,
              frame.input.pointer_y)) {
        focus_ = index;
        fields_[index].selection_begin = fields_[index].value.size();
        fields_[index].selection_end = fields_[index].value.size();
      }
    }
  }

  void handle_keyboard(const Frame& frame) {
    if (frame.input.key_pressed(SDLK_TAB)) {
      if (focus_ >= fields_.size()) {
        focus_ = 0;
      } else if (frame.input.shift) {
        focus_ = (focus_ + fields_.size() - 1) % fields_.size();
      } else {
        focus_ = (focus_ + 1) % fields_.size();
      }
    }
    Field* field = focused();
    if (field == nullptr) return;

    if (frame.input.control && frame.input.key_pressed(SDLK_A)) {
      field->selection_begin = 0;
      field->selection_end = field->value.size();
      return;
    }
    if (frame.input.control && frame.input.key_pressed(SDLK_Z)) {
      if (!field->undo.empty()) {
        field->redo.push_back(field->value);
        field->value = field->undo.back();
        field->undo.pop_back();
        collapse_to_end(*field);
      }
      return;
    }
    if (frame.input.control && frame.input.key_pressed(SDLK_Y)) {
      if (!field->redo.empty()) {
        field->undo.push_back(field->value);
        field->value = field->redo.back();
        field->redo.pop_back();
        collapse_to_end(*field);
      }
      return;
    }
    if (frame.input.key_pressed(SDLK_LEFT)) {
      move_caret(*field, false, frame.input.shift);
    }
    if (frame.input.key_pressed(SDLK_RIGHT)) {
      move_caret(*field, true, frame.input.shift);
    }
    if (frame.input.key_pressed(SDLK_HOME)) set_caret(*field, 0, frame.input.shift);
    if (frame.input.key_pressed(SDLK_END)) set_caret(*field, field->value.size(), frame.input.shift);
    if (frame.input.key_pressed(SDLK_BACKSPACE)) erase_backward(*field, frame.input.control);
    if (frame.input.key_pressed(SDLK_DELETE)) erase_forward(*field);
    if (!frame.input.text.empty() && !frame.input.control && !frame.input.alt) {
      std::string accepted;
      for (const char character : frame.input.text) {
        if (!field->digits_only || std::isdigit(static_cast<unsigned char>(character)) != 0) {
          accepted += character;
        }
      }
      if (!accepted.empty()) replace_selection(*field, accepted);
    }
  }

  static void checkpoint(Field& field) {
    field.undo.push_back(field.value);
    if (field.undo.size() > 64) field.undo.erase(field.undo.begin());
    field.redo.clear();
  }

  static void normalized_selection(const Field& field, std::size_t& begin, std::size_t& end) {
    begin = std::min(field.selection_begin, field.selection_end);
    end = std::max(field.selection_begin, field.selection_end);
  }

  static void replace_selection(Field& field, std::string_view text) {
    checkpoint(field);
    std::size_t begin{};
    std::size_t end{};
    normalized_selection(field, begin, end);
    field.value.replace(begin, end - begin, text);
    field.selection_begin = begin + text.size();
    field.selection_end = field.selection_begin;
  }

  static void erase_backward(Field& field, bool word) {
    std::size_t begin{};
    std::size_t end{};
    normalized_selection(field, begin, end);
    if (begin == end && begin > 0) {
      if (word) {
        while (begin > 0 && field.value[begin - 1] == ' ') --begin;
        while (begin > 0 && field.value[begin - 1] != ' ') --begin;
      } else {
        --begin;
      }
    }
    if (begin == end) return;
    checkpoint(field);
    field.value.erase(begin, end - begin);
    field.selection_begin = begin;
    field.selection_end = begin;
  }

  static void erase_forward(Field& field) {
    std::size_t begin{};
    std::size_t end{};
    normalized_selection(field, begin, end);
    if (begin == end && end < field.value.size()) ++end;
    if (begin == end) return;
    checkpoint(field);
    field.value.erase(begin, end - begin);
    field.selection_begin = begin;
    field.selection_end = begin;
  }

  static void move_caret(Field& field, bool right, bool selecting) {
    const std::size_t next = right ? std::min(field.selection_end + 1, field.value.size())
                                   : (field.selection_end == 0 ? 0 : field.selection_end - 1);
    if (!selecting) field.selection_begin = next;
    field.selection_end = next;
  }

  static void set_caret(Field& field, std::size_t position, bool selecting) {
    if (!selecting) field.selection_begin = position;
    field.selection_end = position;
  }

  static void collapse_to_end(Field& field) {
    field.selection_begin = field.value.size();
    field.selection_end = field.value.size();
  }

  void draw(Canvas& canvas, double time) const {
    for (std::size_t index = 0; index < fields_.size(); ++index) {
      const auto& field = fields_[index];
      canvas.text(field.label, 30.0, field.y - 22.0, 0x444444ffU, 14.0);
      if (focus_ == index) {
        canvas.fill_rectangle(field_x - 3.0, field.y - 3.0, field_width + 6.0, field_height + 6.0, 0x3399ffffU, 0.3);
      }
      canvas.fill_rectangle(field_x, field.y, field_width, field_height, 0xffffffffU);
      canvas.stroke_rectangle(field_x, field.y, field_width, field_height, 0x999999ffU);
      const std::string displayed = field.password ? std::string(field.value.size(), '*') : field.value;
      if (focus_ == index) {
        std::size_t begin{};
        std::size_t end{};
        normalized_selection(field, begin, end);
        const double selection_x = canvas.text_width(displayed.substr(0, begin), 16.0);
        const double selection_width = canvas.text_width(displayed.substr(begin, end - begin), 16.0);
        if (begin != end) {
          canvas.fill_rectangle(field_x + 5.0 + selection_x, field.y + 3.0, selection_width, 22.0, 0x3399ffffU, 0.35);
        }
        if (begin == end && std::fmod(time, 1.0) < 0.55) {
          canvas.line(field_x + 5.0 + selection_x, field.y + 4.0, field_x + 5.0 + selection_x, field.y + 24.0, 0x222222ffU);
        }
      }
      canvas.text(displayed, field_x + 5.0, field.y + 6.0, 0x222222ffU, 16.0);
    }

    const Field* field = focus_ < fields_.size() ? &fields_[focus_] : nullptr;
    double y = start_y + field_gap * 3.0 + 10.0;
    canvas.text("Focus: " + std::string(field == nullptr ? "None" : field->name), 30.0, y, 0x333333ffU, 13.0);
    if (field != nullptr) {
      std::size_t begin{};
      std::size_t end{};
      normalized_selection(*field, begin, end);
      canvas.text("Caret: " + std::to_string(field->selection_end), 30.0, y + 20.0, 0x333333ffU, 13.0);
      canvas.text("Selection: " + std::to_string(begin) + " - " + std::to_string(end), 30.0, y + 40.0, 0x333333ffU, 13.0);
      canvas.text("Chars: " + std::to_string(field->value.size()), 30.0, y + 60.0, 0x333333ffU, 13.0);
      const std::string displayed = field->password ? std::string(field->value.size(), '*') : field->value;
      canvas.text("Display: \"" + displayed + "\"", 30.0, y + 80.0, 0x333333ffU, 13.0);
      if (field->digits_only) canvas.text("Restrict: 0-9", 30.0, y + 100.0, 0x333333ffU, 13.0);
      if (field->password) canvas.text("Password mode: on", 30.0, y + 100.0, 0x333333ffU, 13.0);
    }
    const double instructions_y = start_y + field_gap * 3.0 + 110.0;
    canvas.text("Click a field to focus. Type to enter text.", 30.0, instructions_y, 0x666666ffU, 12.0);
    canvas.text("Arrow keys: move caret | Shift+Arrow: select", 30.0, instructions_y + 20.0, 0x666666ffU, 12.0);
    canvas.text("Ctrl+A: select all | Ctrl+Z: undo | Ctrl+Y: redo", 30.0, instructions_y + 40.0, 0x666666ffU, 12.0);
    canvas.text("Backspace/Delete: delete | Ctrl+Backspace: delete word", 30.0, instructions_y + 60.0, 0x666666ffU, 12.0);
  }

  std::array<Field, 3> fields_;
  std::size_t focus_{};
  bool text_input_started_{};
};

} // namespace

int main(int argc, char** argv) {
  try {
    TextInputExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight text input - SDL",
        [&](const Frame& frame) { example.frame(frame); },
        0xffffffffU);
  } catch (const std::exception& error) {
    std::cerr << "textinput example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
