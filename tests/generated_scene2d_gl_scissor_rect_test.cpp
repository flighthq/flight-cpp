// The generated compute_scissor_rect of @flighthq/scene2d-gl's glClipRectangle.ts at a627849, emitted
// once the scene2d-gl-clip-rectangle-viewport-number-domain source patch and the two scoped
// drawing-buffer repairs made it lowerable. This runs the ACTUAL emitted function, not a copy.
//
// Every expected rectangle below was produced by running the pinned TypeScript computeScissorRect itself
// (type annotations erased, nothing else) on the same inputs. The patched and unpatched source gave
// identical results on every case, because unary plus is the identity on the declared numbers and is
// applied after the nullish selection. The cases cover:
// - the default drawing buffer (no render target viewport) against an explicit viewport, whose x/y
//   offsets are added and whose height drives the bottom-left GL orientation flip;
// - fractional edges (floor of the minimum, ceil of the maximum);
// - negative edges and overflow, clamped to [0, viewport];
// - a rectangle wholly outside, which is empty and keeps the source's own flipped y;
// - scale with fractional translate, a 90 degree rotation, a negative x scale;
// - a zero-width viewport.
//
// The default-buffer cases need the real drawing-buffer size, so the test runs in one actual offscreen
// GlApplication of 64x48 and first checks that the drawing buffer is 64x48, the size the source cases
// assumed. No GL draw call is made.
#include <flight/host_sdl/gl_app.hpp>
#include <flight/render_gl/gl_render_state.hpp>
#include <flight/scene2d_gl/gl_clip_rectangle.hpp>

#include <exception>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>

namespace {

namespace scene2d_gl = flight::scene2d_gl;
namespace types = flight::types;

int failures = 0;

struct Box {
  double x;
  double y;
  double width;
  double height;
};

struct Transform {
  double a;
  double b;
  double c;
  double d;
  double tx;
  double ty;
};

struct Case {
  const char* name;
  std::optional<Box> viewport;
  Box rect;
  Transform transform;
  Box expected;
};

constexpr Transform kIdentity{1, 0, 0, 1, 0, 0};

const Case kCases[] = {
    {"identity, default drawing buffer", std::nullopt, {10, 5, 20, 8}, kIdentity, {10, 35, 20, 8}},
    {"fractional edges floor/ceil", std::nullopt, {10.4, 5.6, 20.2, 8.1}, kIdentity, {10, 34, 21, 9}},
    {"negative edges clamp to 0", std::nullopt, {-5.5, -3.25, 20, 10}, kIdentity, {0, 41, 15, 7}},
    {"overflow clamps to drawing buffer", std::nullopt, {50, 40, 30, 30}, kIdentity, {50, 0, 14, 8}},
    {"fully outside is empty", std::nullopt, {-50, -50, 10, 10}, kIdentity, {0, 88, 0, 0}},
    {"explicit viewport offsets and flips", Box{100, 200, 32, 16}, {4, 2, 8, 6}, kIdentity, {104, 208, 8, 6}},
    {"explicit viewport clamps", Box{7, 3, 20, 10}, {15, -2, 10, 20}, kIdentity, {22, 3, 5, 10}},
    {"scale and fractional translate", std::nullopt, {1, 2, 3, 4}, {2, 0, 0, 2, 3.5, -1.25}, {5, 37, 7, 9}},
    {"90 degree rotation", std::nullopt, {2, 3, 4, 5}, {0, 1, -1, 0, 20, 1}, {12, 41, 5, 4}},
    {"negative x scale flip", std::nullopt, {0, 0, 10, 10}, {-1, 0, 0, 1, 40, 0}, {30, 38, 10, 10}},
    {"zero-width viewport", Box{0, 0, 0, 10}, {1, 1, 2, 2}, kIdentity, {1, 7, 0, 2}},
};

void check_cases() {
  flight::host_sdl::GlApplication app({.title = "Flight scene2d-gl scissor rect", .width = 64, .height = 48, .hidden = true});
  const auto gl = app.context();
  if (gl.drawing_buffer_width() != 64 || gl.drawing_buffer_height() != 48) {
    std::cerr << "FAIL: the offscreen drawing buffer is " << gl.drawing_buffer_width() << 'x'
              << gl.drawing_buffer_height() << ", not the 64x48 the source cases assume\n";
    ++failures;
    return;
  }
  const auto state = flight::render_gl::create_gl_render_state(gl);
  const auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  for (const Case& entry : kCases) {
    runtime->render_target_viewport =
        entry.viewport ? std::optional{flight::make_ref<types::GlViewportRect>(types::GlViewportRect{
                             .height = entry.viewport->height, .width = entry.viewport->width,
                             .x = entry.viewport->x, .y = entry.viewport->y})}
                       : std::nullopt;
    const auto rect = flight::make_ref<types::Rectangle>(types::Rectangle{
        .height = entry.rect.height, .width = entry.rect.width, .x = entry.rect.x, .y = entry.rect.y});
    const auto matrix = flight::make_ref<types::Matrix>(types::Matrix{
        .a = entry.transform.a, .b = entry.transform.b, .c = entry.transform.c, .d = entry.transform.d,
        .tx = entry.transform.tx, .ty = entry.transform.ty});
    const auto out = scene2d_gl::compute_scissor_rect(
        state, flight::StructuralRef<flight::RowReadonly<flight::RowOf<types::RectangleLike>>>(rect),
        flight::StructuralRef<flight::RowReadonly<flight::RowOf<types::MatrixLike>>>(matrix));
    if (out->x != entry.expected.x || out->y != entry.expected.y || out->width != entry.expected.width ||
        out->height != entry.expected.height) {
      std::cerr << "FAIL: " << entry.name << ": got {" << out->x << ", " << out->y << ", " << out->width << ", "
                << out->height << "}, the source gives {" << entry.expected.x << ", " << entry.expected.y << ", "
                << entry.expected.width << ", " << entry.expected.height << "}\n";
      ++failures;
    }
  }
  runtime->render_target_viewport = std::nullopt;
}

}  // namespace

int main() {
  try {
    check_cases();
    if (failures != 0) {
      std::cerr << failures << " scissor rect case(s) differ from the pinned source\n";
      return 1;
    }
    std::cout << "compute_scissor_rect matches the pinned source on all " << std::size(kCases) << " cases\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "scissor rect check failed: " << error.what() << '\n';
    return 1;
  }
}
