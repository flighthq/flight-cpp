// Native SDL port of @flighthq/example-text.
// Source: .dependencies/flight/examples/packages/text/src/app.ts

#include "sdl_host.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;

void bordered_text(
    Canvas& canvas,
    double x,
    double y,
    double width,
    double height,
    std::string_view text,
    std::uint32_t foreground,
    std::uint32_t background,
    std::uint32_t border,
    double text_x) {
  canvas.fill_rectangle(x, y, width, height, background);
  canvas.stroke_rectangle(x, y, width, height, border);
  canvas.text(text, text_x, y + 6.0, foreground, 12.0);
}

void draw_text_gallery(const Frame& frame) {
  auto& canvas = frame.canvas;
  canvas.text("TextLabel Basics", 30.0, 20.0, 0x222222ffU, 18.0);
  canvas.text("Sans-serif 16px", 30.0, 48.0, 0x333333ffU, 16.0);
  canvas.text("Serif 16px", 200.0, 48.0, 0x333333ffU, 16.0);
  canvas.text("Monospace 16px", 340.0, 48.0, 0x333333ffU, 16.0);
  canvas.text("Large 28px", 540.0, 40.0, 0x1a6b3cffU, 28.0);
  canvas.text("Small 12px in a different color", 30.0, 74.0, 0x7a0026ffU, 12.0);

  canvas.text("Text Alignment", 30.0, 110.0, 0x222222ffU, 18.0);
  bordered_text(canvas, 30.0, 138.0, 220.0, 24.0, "Left aligned (default)", 0x444444ffU, 0xffffffffU, 0xccccccffU, 34.0);
  bordered_text(canvas, 270.0, 138.0, 220.0, 24.0, "Center aligned", 0x444444ffU, 0xffffffffU, 0xccccccffU, 316.0);
  bordered_text(canvas, 510.0, 138.0, 220.0, 24.0, "Right aligned", 0x444444ffU, 0xffffffffU, 0xccccccffU, 620.0);

  canvas.text("Word Wrapping", 30.0, 185.0, 0x222222ffU, 18.0);
  canvas.text("Flight is a tree-shakable graphics and", 30.0, 213.0, 0x444444ffU, 12.0);
  canvas.text("application SDK. It spans a scene graph,", 30.0, 233.0, 0x444444ffU, 12.0);
  canvas.text("four interchangeable renderers, offscreen", 30.0, 253.0, 0x444444ffU, 12.0);
  canvas.text("image processing, and a full application", 30.0, 273.0, 0x444444ffU, 12.0);
  canvas.text("layer. This paragraph demonstrates wrapping.", 30.0, 293.0, 0x444444ffU, 12.0);
  canvas.text("The same paragraph rendered in serif at a", 410.0, 213.0, 0x555555ffU, 12.0);
  canvas.text("slightly larger size. Each renderable node", 410.0, 233.0, 0x555555ffU, 12.0);
  canvas.text("type is identified by a Kind string, and", 410.0, 253.0, 0x555555ffU, 12.0);
  canvas.text("renderers are registered by the caller.", 410.0, 273.0, 0x555555ffU, 12.0);

  canvas.text("Text Styles", 30.0, 345.0, 0x222222ffU, 18.0);
  canvas.text("Bold text", 30.0, 373.0, 0x333333ffU, 16.0);
  canvas.text("Italic text", 150.0, 373.0, 0x333333ffU, 16.0);
  canvas.text("Bold + Italic", 285.0, 373.0, 0x333333ffU, 16.0);
  canvas.text("Underlined text", 475.0, 373.0, 0x2255aaffU, 16.0);
  canvas.line(475.0, 392.0, 645.0, 392.0, 0x2255aaffU);
  canvas.text("L e t t e r   s p a c i n g :   4", 30.0, 400.0, 0x666666ffU, 10.0);
  canvas.text("Serif bold italic", 280.0, 400.0, 0x8b4513ffU, 16.0);
  canvas.text("Mono bold", 500.0, 400.0, 0x2a6e3fffU, 14.0);

  canvas.text("Background & Border", 30.0, 440.0, 0x222222ffU, 18.0);
  bordered_text(canvas, 30.0, 468.0, 220.0, 30.0, "Light background", 0x333333ffU, 0xf0f0f0ffU, 0xccccccffU, 38.0);
  bordered_text(canvas, 270.0, 468.0, 220.0, 30.0, "Colored background", 0xffffffffU, 0x336699ffU, 0x224466ffU, 278.0);
  bordered_text(canvas, 510.0, 468.0, 220.0, 30.0, "Warning style", 0x856404ffU, 0xfff3cdffU, 0xffc107ffU, 518.0);

  canvas.fill_rectangle(30.0, 515.0, 350.0, 70.0, 0xeef6ffffU);
  canvas.stroke_rectangle(30.0, 515.0, 350.0, 70.0, 0x99bbddffU);
  canvas.text("A multiline RichText with background and border.", 36.0, 522.0, 0x333333ffU, 10.0);
  canvas.text("Word wrapping is enabled so the text flows", 36.0, 540.0, 0x333333ffU, 10.0);
  canvas.text("within the specified width and content area.", 36.0, 558.0, 0x333333ffU, 10.0);
  canvas.fill_rectangle(410.0, 515.0, 320.0, 70.0, 0x1e1e1effU);
  canvas.stroke_rectangle(410.0, 515.0, 320.0, 70.0, 0x444444ffU);
  canvas.text("const label = createTextLabel();", 416.0, 522.0, 0xd4d4d4ffU, 10.0);
  canvas.text("label.data.text = 'Hello Flight';", 416.0, 540.0, 0xd4d4d4ffU, 10.0);
  canvas.text("label.data.textFormat = { font: 'monospace' };", 416.0, 558.0, 0xd4d4d4ffU, 9.0);
}

} // namespace

int main(int argc, char** argv) {
  try {
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight text - SDL",
        draw_text_gallery,
        0xffffffffU);
  } catch (const std::exception& error) {
    std::cerr << "text example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
