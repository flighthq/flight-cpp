// compressedTexImage2D / compressedTexSubImage3D with WebGL's `optional GLuint srcOffset = 0` and
// `optional GLuint srcLengthOverride`, run against one actual offscreen GL context.
//
// A compressed upload's byte count must equal the format's exact image size, and GL rejects any other
// count with INVALID_VALUE. The test uses that validation as its witness of which bytes the host
// selected, for a single 4x4 ETC2 RGB8 block (8 bytes):
// - absent offset and length on an 8-byte view: offset 0, the whole view, so 8 bytes and accepted;
// - absent offset on a 16-byte view: offset 0, the whole view, so 16 bytes and rejected;
// - explicit offset 8 on a 16-byte view: the last 8 bytes, accepted;
// - explicit offset 4: 12 bytes, rejected;
// - explicit offset 8 with an explicit length of 8: accepted.
// It checks the 2D upload and a TEXTURE_2D_ARRAY sub-image of storage allocated with texStorage3D. The
// host's existing range semantics are asserted unchanged: an offset or length past the view throws
// std::range_error before GL is called.
//
// The format is probed, not assumed. If the context rejects ETC2 RGB8 outright, the test prints that
// capability limitation with the GL error, skips only the GPU-acceptance checks, and still verifies the
// host's offset/default range handling. It never invents an extension or reports an upload that GL did
// not accept.
#include <flight/host_sdl/gl_app.hpp>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

using flight::host_sdl::WebGl2Context;

constexpr std::uint32_t kEtc2Rgb8 = 0x9274;  // GL_COMPRESSED_RGB8_ETC2: 4x4 texels in 8 bytes
constexpr std::uint32_t kInvalidValue = 0x0501;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void drain(const WebGl2Context& gl) {
  while (gl.get_error() != gl.no_error) {
  }
}

std::uint32_t upload_2d(const WebGl2Context& gl, const flight::Uint8Array& data, std::optional<std::size_t> offset,
                        std::optional<std::size_t> length) {
  drain(gl);
  gl.compressed_tex_image_2d(gl.texture_2_d, 0, kEtc2Rgb8, 4, 4, 0, data, offset, length);
  return gl.get_error();
}

std::uint32_t upload_3d(const WebGl2Context& gl, const flight::Uint8Array& data, std::optional<std::size_t> offset,
                        std::optional<std::size_t> length) {
  drain(gl);
  gl.compressed_tex_sub_image_3d(gl.texture_2_d_array, 0, 0, 0, 0, 4, 4, 1, kEtc2Rgb8, data, offset, length);
  return gl.get_error();
}

bool throws_range(auto&& call) {
  try {
    call();
  } catch (const std::range_error&) {
    return true;
  }
  return false;
}

void run() {
  flight::host_sdl::GlApplication app({.title = "Flight compressed upload", .width = 16, .height = 16, .hidden = true});
  const WebGl2Context gl = app.context();
  flight::Uint8Array block(8U);
  flight::Uint8Array pair(16U);
  for (std::size_t i = 0; i < 16; ++i) pair[i] = static_cast<std::uint8_t>(i * 17U);
  for (std::size_t i = 0; i < 8; ++i) block[i] = pair[8 + i];

  // Host range semantics, independent of GL format support.
  check(throws_range([&] { (void)upload_2d(gl, pair, std::size_t{17}, std::nullopt); }),
        "an offset past the view did not throw range_error");
  check(throws_range([&] { (void)upload_2d(gl, pair, std::size_t{8}, std::size_t{9}); }),
        "a length past the view did not throw range_error");

  check(throws_range([&] { (void)upload_3d(gl, pair, std::size_t{17}, std::nullopt); }),
        "3D: an offset past the view did not throw range_error");
  check(throws_range([&] { (void)upload_3d(gl, pair, std::size_t{8}, std::size_t{9}); }),
        "3D: a length past the view did not throw range_error");

  const auto texture = gl.create_texture();
  gl.bind_texture(gl.texture_2_d, texture);
  const std::uint32_t probe = upload_2d(gl, block, std::nullopt, std::nullopt);
  if (probe != gl.no_error) {
    std::cout << "capability limitation: this context rejects ETC2 RGB8 (GL error 0x" << std::hex << probe << std::dec
              << "); GPU acceptance checks skipped, host range checks still ran\n";
    gl.delete_texture(texture);
    return;
  }
  check(upload_2d(gl, block, std::size_t{0}, std::nullopt) == gl.no_error,
        "2D: explicit offset 0 did not match the absent default on an exact 8-byte view");
  check(upload_2d(gl, pair, std::nullopt, std::nullopt) == kInvalidValue,
        "2D: an absent offset on a 16-byte view did not select all 16 bytes");
  check(upload_2d(gl, pair, std::size_t{8}, std::nullopt) == gl.no_error,
        "2D: explicit offset 8 did not select exactly the last 8 bytes");
  check(upload_2d(gl, pair, std::size_t{4}, std::nullopt) == kInvalidValue,
        "2D: explicit offset 4 did not select 12 bytes");
  check(upload_2d(gl, pair, std::size_t{8}, std::size_t{8}) == gl.no_error,
        "2D: explicit offset 8 with length 8 was not accepted");
  gl.delete_texture(texture);

  const auto array = gl.create_texture();
  gl.bind_texture(gl.texture_2_d_array, array);
  drain(gl);
  gl.tex_storage_3d(gl.texture_2_d_array, 1, kEtc2Rgb8, 4, 4, 1);
  const std::uint32_t storage = gl.get_error();
  if (storage != gl.no_error) {
    std::cout << "capability limitation: ETC2 RGB8 texStorage3D for TEXTURE_2D_ARRAY rejected (GL error 0x" << std::hex
              << storage << std::dec << "); 3D acceptance checks skipped\n";
  } else {
    check(upload_3d(gl, block, std::nullopt, std::nullopt) == gl.no_error,
          "3D: absent offset and length on an exact 8-byte view were not accepted");
    check(upload_3d(gl, block, std::size_t{0}, std::nullopt) == gl.no_error,
          "3D: explicit offset 0 did not match the absent default on an exact 8-byte view");
    check(upload_3d(gl, pair, std::nullopt, std::nullopt) == kInvalidValue,
          "3D: an absent offset on a 16-byte view did not select all 16 bytes");
    check(upload_3d(gl, pair, std::size_t{8}, std::nullopt) == gl.no_error,
          "3D: explicit offset 8 did not select exactly the last 8 bytes");
    check(upload_3d(gl, pair, std::size_t{4}, std::nullopt) == kInvalidValue,
          "3D: explicit offset 4 did not select 12 bytes");
    check(upload_3d(gl, pair, std::size_t{8}, std::size_t{8}) == gl.no_error,
          "3D: explicit offset 8 with length 8 was not accepted");
  }
  gl.delete_texture(array);
  drain(gl);
  std::cout << "ETC2 RGB8 accepted by this context; 2D" << (storage == gl.no_error ? " and 3D array" : "")
            << " offset/default selection verified by GL size validation\n";
}

}  // namespace

int main() {
  try {
    run();
    if (failures != 0) {
      std::cerr << failures << " compressed upload check(s) failed\n";
      return 1;
    }
    std::cout << "compressed upload checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "compressed upload check failed: " << error.what() << '\n';
    return 1;
  }
}
