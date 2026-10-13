#include <flight/texture_formats/contract.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// Native, byte-exact port of the tests in packages/texture-formats/src at Flight a627849.
// The package is parser-only: build_atf, build_basis, build_dds, and build_ktx2 below are the
// upstream test serializers, ported independently from the generated parsers. They write each
// container header, index, and length field byte-for-byte before the native parser consumes it.
//
// MUTATION SENSITIVITY, measured 2026-10-12 from a clean committed baseline and then restored:
//   * reporting ATF's declared mip count instead of its populated mip count fails the dedicated
//     empty-mipmap assertion;
//   * bypassing KTX2's mip-major to image-major normalization fails both the cubemap and array
//     order assertions.

namespace {

using flight::Array;
using flight::Ref;
using flight::String;
using flight::Uint8Array;
using flight::types::TextureContainer;
using flight::types::TextureContainerLevel;
namespace formats = flight::texture_formats;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool is(const std::optional<String>& value, const char* expected) {
  return value.has_value() && *value == String(expected);
}

template <typename T>
bool is(const T& value, const char* expected) {
  return value == String(expected);
}

void write_le(Uint8Array& bytes, std::size_t offset, std::uint64_t value, int width) {
  for (int i = 0; i < width; ++i) {
    bytes[offset + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value >> (i * 8));
  }
}

void write_be(Uint8Array& bytes, std::size_t offset, std::uint64_t value, int width) {
  for (int i = 0; i < width; ++i) {
    bytes[offset + static_cast<std::size_t>(i)] =
        static_cast<std::uint8_t>(value >> ((width - 1 - i) * 8));
  }
}

Uint8Array prefix(const Uint8Array& bytes, std::size_t length) {
  Uint8Array out(length);
  for (std::size_t i = 0; i < length; ++i) out[i] = bytes[i];
  return out;
}

bool level_is(const Ref<TextureContainerLevel>& level,
              double offset,
              double length,
              double width,
              double height) {
  return level->byte_offset == offset && level->byte_length == length && level->width == width &&
         level->height == height;
}

struct AtfFixture {
  Uint8Array bytes;
  std::vector<std::size_t> block_offsets;
};

AtfFixture build_atf(int version,
                     int format_code,
                     bool cube,
                     int log2_width,
                     int log2_height,
                     int mip_count,
                     const std::vector<std::size_t>& block_lengths) {
  const bool versioned = version != 0;
  const std::size_t header_offset = versioned ? 12 : 6;
  const int length_width = versioned ? 4 : 3;
  std::size_t body_size = 0;
  for (const auto length : block_lengths) body_size += static_cast<std::size_t>(length_width) + length;
  Uint8Array bytes(header_offset + 4 + body_size);
  bytes[0] = 0x41;
  bytes[1] = 0x54;
  bytes[2] = 0x46;
  const auto payload_length = bytes.size() - header_offset;
  if (versioned) {
    bytes[6] = 0xff;
    bytes[7] = static_cast<std::uint8_t>(version);
    write_be(bytes, 8, payload_length, 4);
  } else {
    write_be(bytes, 3, payload_length, 3);
  }
  bytes[header_offset] = static_cast<std::uint8_t>((cube ? 0x80 : 0) | format_code);
  bytes[header_offset + 1] = static_cast<std::uint8_t>(log2_width);
  bytes[header_offset + 2] = static_cast<std::uint8_t>(log2_height);
  bytes[header_offset + 3] = static_cast<std::uint8_t>(mip_count);

  std::vector<std::size_t> offsets;
  std::size_t offset = header_offset + 4;
  for (const auto length : block_lengths) {
    write_be(bytes, offset, length, length_width);
    offset += static_cast<std::size_t>(length_width);
    offsets.push_back(offset);
    for (std::size_t i = 0; i < length; ++i) bytes[offset + i] = static_cast<std::uint8_t>(offset + i);
    offset += length;
  }
  return {std::move(bytes), std::move(offsets)};
}

struct BasisSlice {
  std::uint32_t image;
  std::uint8_t level;
  std::uint16_t width;
  std::uint16_t height;
  std::uint32_t offset;
  std::uint32_t length;
};

Uint8Array build_basis(const std::vector<BasisSlice>& slices,
                       std::uint32_t total_images = 1,
                       std::uint8_t format = 0,
                       std::uint8_t type = 0) {
  constexpr std::size_t header_size = 77;
  constexpr std::size_t descriptor_size = 23;
  const std::size_t table_offset = header_size;
  std::size_t total = table_offset + slices.size() * descriptor_size;
  for (const auto& slice : slices) total = std::max(total, static_cast<std::size_t>(slice.offset + slice.length));
  Uint8Array bytes(total);
  bytes[0] = 0x73;
  bytes[1] = 0x42;
  write_le(bytes, 14, slices.size(), 3);
  write_le(bytes, 17, total_images, 3);
  bytes[20] = format;
  bytes[23] = type;
  write_le(bytes, 65, table_offset, 4);
  std::size_t cursor = table_offset;
  for (const auto& slice : slices) {
    write_le(bytes, cursor, slice.image, 3);
    bytes[cursor + 3] = slice.level;
    write_le(bytes, cursor + 5, slice.width, 2);
    write_le(bytes, cursor + 7, slice.height, 2);
    write_le(bytes, cursor + 13, slice.offset, 4);
    write_le(bytes, cursor + 17, slice.length, 4);
    cursor += descriptor_size;
  }
  return bytes;
}

std::uint32_t four_cc(const char* text) {
  return static_cast<std::uint32_t>(static_cast<unsigned char>(text[0])) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(text[1])) << 8) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(text[2])) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(text[3])) << 24);
}

struct DdsOptions {
  std::uint32_t width = 4;
  std::uint32_t height = 4;
  std::uint32_t mip_count = 1;
  std::uint32_t pixel_flags = 4;
  std::uint32_t code = 0;
  std::uint32_t bits = 0;
  std::uint32_t red = 0;
  std::uint32_t green = 0;
  std::uint32_t blue = 0;
  std::uint32_t alpha = 0;
  std::uint32_t caps2 = 0;
  std::uint32_t data_bytes = 256;
  bool dx10 = false;
  std::uint32_t dxgi = 0;
  std::uint32_t misc = 0;
  std::uint32_t layers = 1;
};

Uint8Array build_dds(const DdsOptions& options) {
  const std::size_t data_offset = options.dx10 ? 148 : 128;
  Uint8Array bytes(data_offset + options.data_bytes);
  bytes[0] = 0x44;
  bytes[1] = 0x44;
  bytes[2] = 0x53;
  bytes[3] = 0x20;
  write_le(bytes, 4, 124, 4);
  write_le(bytes, 12, options.height, 4);
  write_le(bytes, 16, options.width, 4);
  write_le(bytes, 28, options.mip_count, 4);
  write_le(bytes, 76, 32, 4);
  write_le(bytes, 80, options.pixel_flags, 4);
  write_le(bytes, 84, options.code, 4);
  write_le(bytes, 88, options.bits, 4);
  write_le(bytes, 92, options.red, 4);
  write_le(bytes, 96, options.green, 4);
  write_le(bytes, 100, options.blue, 4);
  write_le(bytes, 104, options.alpha, 4);
  write_le(bytes, 112, options.caps2, 4);
  if (options.dx10) {
    write_le(bytes, 128, options.dxgi, 4);
    write_le(bytes, 132, 3, 4);
    write_le(bytes, 136, options.misc, 4);
    write_le(bytes, 140, options.layers, 4);
  }
  return bytes;
}

struct KtxLevel {
  std::uint64_t offset;
  std::uint64_t length;
};

Uint8Array build_ktx2(std::uint32_t vk_format,
                      std::uint32_t width,
                      std::uint32_t height,
                      const std::vector<KtxLevel>& levels,
                      std::uint32_t depth = 0,
                      std::uint32_t layers = 0,
                      std::uint32_t faces = 1,
                      std::uint32_t scheme = 0) {
  constexpr std::uint8_t magic[] = {0xab, 0x4b, 0x54, 0x58, 0x20, 0x32,
                                    0x30, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
  std::size_t total = 80 + levels.size() * 24;
  for (const auto& level : levels) total = std::max(total, static_cast<std::size_t>(level.offset + level.length));
  Uint8Array bytes(total);
  for (std::size_t i = 0; i < std::size(magic); ++i) bytes[i] = magic[i];
  write_le(bytes, 12, vk_format, 4);
  write_le(bytes, 16, 4, 4);
  write_le(bytes, 20, width, 4);
  write_le(bytes, 24, height, 4);
  write_le(bytes, 28, depth, 4);
  write_le(bytes, 32, layers, 4);
  write_le(bytes, 36, faces, 4);
  write_le(bytes, 40, levels.size(), 4);
  write_le(bytes, 44, scheme, 4);
  std::size_t cursor = 80;
  for (const auto& level : levels) {
    write_le(bytes, cursor, level.offset, 8);
    write_le(bytes, cursor + 8, level.length, 8);
    write_le(bytes, cursor + 16, level.length, 8);
    cursor += 24;
  }
  return bytes;
}

void test_byte_reader() {
  auto reader = formats::create_byte_reader(Uint8Array{0x34, 0x12, 0x56, 0x34, 0x12}, 0);
  check(formats::read_byte_reader_u16(reader) == 0x1234, "reader decodes little-endian u16");
  check(formats::read_byte_reader_u24(reader) == 0x123456, "reader decodes little-endian u24");
  check(reader->offset == 5, "reader advances across reads");

  reader = formats::create_byte_reader(Uint8Array{0x12, 0x34, 0x56, 0x78}, 0);
  check(formats::read_byte_reader_u24_big_endian(reader) == 0x123456, "reader decodes big-endian u24");
  check(formats::read_byte_reader_u8(reader) == 0x78, "reader decodes u8");
  reader = formats::create_byte_reader(Uint8Array{0xff, 0xff, 0xff, 0xff}, 0);
  check(formats::read_byte_reader_u32_big_endian(reader) == 0xffffffffu, "reader preserves unsigned u32");

  Uint8Array u64(8);
  write_le(u64, 0, 0x200001000ULL, 8);
  reader = formats::create_byte_reader(u64, 0);
  check(formats::read_byte_reader_u64(reader) == 0x200001000ULL, "reader decodes little-endian u64");
  check(reader->offset == 8, "u64 advances eight bytes");
  reader = formats::create_byte_reader(Uint8Array(8), 4);
  check(formats::has_byte_reader_bytes(reader, 4), "remaining reader range is accepted");
  check(!formats::has_byte_reader_bytes(reader, 5), "reader overrun is rejected");
  formats::skip_byte_reader(reader, 3);
  check(reader->offset == 7, "reader skip advances offset");
}

void test_detection_and_explanation() {
  auto ktx = build_ktx2(37, 1, 1, {{104, 1}});
  check(is(formats::detect_texture_container(ktx), "ktx2"), "detects KTX2 magic");
  check(is(formats::detect_texture_container(Uint8Array{0x41, 0x54, 0x46}), "atf"), "detects ATF signature");
  check(is(formats::detect_texture_container(Uint8Array{0x44, 0x44, 0x53, 0x20}), "dds"), "detects DDS magic");
  check(is(formats::detect_texture_container(Uint8Array{0x73, 0x42}), "basis"), "detects Basis signature");
  check(!formats::detect_texture_container(Uint8Array{0x89, 0x50, 0x4e, 0x47}).has_value(),
        "rejects unrecognized signatures");
  check(!formats::detect_texture_container(prefix(ktx, 8)).has_value(), "rejects truncated KTX2 magic");

  auto explanation = formats::explain_texture_container_parse(Uint8Array{0, 1, 2, 3});
  check(explanation.has_value() && !(*explanation)->container.has_value() &&
            is((*explanation)->reason, "container-unrecognized"),
        "explains an unrecognized container");
  explanation = formats::explain_texture_container_parse(Uint8Array{0x44, 0x44, 0x53, 0x20});
  check(explanation.has_value() && is((*explanation)->container, "dds") &&
            is((*explanation)->reason, "header-truncated"),
        "explains a truncated detected header");
  explanation = formats::explain_texture_container_parse(build_ktx2(999, 1, 1, {{104, 0}}));
  check(explanation.has_value() && is((*explanation)->reason, "format-unsupported"),
        "explains an unsupported format");
  explanation = formats::explain_texture_container_parse(ktx);
  check(!explanation.has_value(), "a successfully parsed container has no explanation");
}

void test_level_layout() {
  check(formats::get_texture_container_level_byte_length(String("rgba8unorm"), 4, 4) == 64,
        "uncompressed layout multiplies bytes per pixel");
  check(formats::get_texture_container_level_byte_length(String("r8unorm"), 4, 4) == 16,
        "one-byte pixel layout is exact");
  check(formats::get_texture_container_level_byte_length(String("bc1"), 4, 4) == 8,
        "BC1 block length is exact");
  check(formats::get_texture_container_level_byte_length(String("bc7"), 5, 5) == 64,
        "compressed layout rounds up partial blocks");
  check(formats::get_texture_container_level_byte_length(String("astc6x6"), 6, 6) == 16,
        "ASTC block length is exact");
  check(formats::get_texture_container_level_byte_length(String("etc1s"), 4, 4) == -1,
        "variable-rate format has no fixed layout");

  auto layout = formats::compute_texture_container_levels(String("bc1"), 8, 8, 4, 1, 1, 100);
  check(layout.has_value() && (*layout)->levels.size() == 4, "layout creates complete mip chain");
  check(layout.has_value() && level_is((*layout)->levels.element(0), 100, 32, 8, 8),
        "layout base level range is exact");
  check(layout.has_value() && level_is((*layout)->levels.element(3), 148, 8, 1, 1),
        "layout tail level range is exact");
  check(layout.has_value() && (*layout)->end_offset == 156, "layout reports exact end offset");
  check(!formats::compute_texture_container_levels(String("etc1s"), 4, 4, 1, 1, 1, 0).has_value(),
        "variable-rate layout is refused");
}

void test_atf() {
  const auto cross = build_atf(3, 5, false, 3, 3, 3,
                               {64, 32, 24, 64, 16, 8, 6, 16, 16, 8, 6, 16});
  auto parsed = formats::parse_atf(cross.bytes);
  check(parsed.has_value() && parsed->size() == 4, "ATF exposes four populated format peers");
  if (parsed.has_value() && parsed->size() == 4) {
    const auto bc3 = parsed->element(0);
    check(is(bc3->format, "bc3") && is(parsed->element(1)->format, "etc1") &&
              is(parsed->element(2)->format, "pvrtc4bppRgba") && is(parsed->element(3)->format, "etc2Rgba"),
          "ATF maps format slots in file order");
    check(bc3->width == 8 && bc3->height == 8 && bc3->mip_levels == 3 && bc3->faces == 1,
          "ATF preserves dimensions and populated mip count");
    check(level_is(bc3->levels.element(0), cross.block_offsets[0], 64, 8, 8) &&
              level_is(bc3->levels.element(1), cross.block_offsets[4], 16, 4, 4) &&
              level_is(bc3->levels.element(2), cross.block_offsets[8], 16, 2, 2),
          "ATF slot ranges follow side-level-slot serialization");
    const auto chosen = formats::select_texture_container(*parsed, Array<String>{String("bc3")});
    check(chosen.has_value() && is((*chosen)->format, "bc3"), "selection chooses a supported peer");
  }

  const auto sparse = build_atf(3, 5, false, 3, 3, 3,
                                {64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  parsed = formats::parse_atf(sparse.bytes);
  check(parsed.has_value() && parsed->size() == 1 && parsed->element(0)->mip_levels == 1,
        "ATF counts populated mips rather than declared empty mips");
  check(parsed.has_value() && level_is(parsed->element(0)->levels.element(0), sparse.block_offsets[0], 64, 8, 8),
        "ATF drops zero-length slot levels");

  parsed = formats::parse_atf(build_atf(2, 3, false, 2, 2, 1, {8, 8, 8}).bytes);
  check(parsed.has_value() && parsed->size() == 3, "ATF v2 walks exactly three slots");
  parsed = formats::parse_atf(build_atf(3, 3, true, 1, 1, 1, std::vector<std::size_t>(24, 8)).bytes);
  check(parsed.has_value() && parsed->size() == 4 && parsed->element(0)->faces == 6 &&
            parsed->element(0)->levels.size() == 6,
        "ATF cubemap exposes six side ranges per peer");

  const auto legacy = build_atf(0, 5, false, 2, 2, 2, {16, 0, 0, 16, 0, 0});
  parsed = formats::parse_atf(legacy.bytes);
  check(parsed.has_value() && parsed->size() == 1 && parsed->element(0)->levels.size() == 2,
        "ATF v0 decodes 24-bit block lengths");
  check(parsed.has_value() && level_is(parsed->element(0)->levels.element(1), legacy.block_offsets[3], 16, 2, 2),
        "ATF v0 second mip range is byte-exact");
  check(is(formats::get_atf_parse_failure_reason(Uint8Array{0x41, 0x54, 0x46}), "header-truncated"),
        "ATF reports truncated detected header");
  check(!formats::parse_atf(Uint8Array{0x44, 0x44, 0x53, 0x20}).has_value(), "ATF rejects DDS bytes");
  check(!formats::parse_atf(build_atf(3, 0, false, 2, 2, 1, {0, 0, 0, 0}).bytes).has_value(),
        "ATF rejects raw format codes");
  check(!formats::parse_atf(prefix(cross.bytes, cross.bytes.size() - 8)).has_value(),
        "ATF rejects a truncated payload");
  auto corrupt = sparse;
  write_be(corrupt.bytes, 16, 0xffffff, 4);
  check(!formats::parse_atf(corrupt.bytes).has_value(), "ATF rejects a block range overrun");
}

void test_basis() {
  auto bytes = build_basis({{0, 0, 4, 4, 100, 8}});
  auto parsed = formats::parse_basis(bytes);
  check(parsed.has_value() && is((*parsed)->format, "etc1s") && (*parsed)->width == 4 &&
            (*parsed)->height == 4 && (*parsed)->mip_levels == 1,
        "Basis parses one ETC1S slice");
  check(parsed.has_value() && level_is((*parsed)->levels.element(0), 100, 8, 4, 4),
        "Basis slice range is byte-exact");

  bytes = build_basis({{0, 0, 4, 4, 123, 8}, {0, 1, 2, 2, 131, 4}});
  parsed = formats::parse_basis(bytes);
  check(parsed.has_value() && (*parsed)->mip_levels == 2 && (*parsed)->levels.size() == 2 &&
            (*parsed)->levels.element(1)->width == 2 && (*parsed)->levels.element(1)->byte_offset == 131,
        "Basis derives a mip chain from slice descriptors");

  parsed = formats::parse_basis(build_basis({{0, 0, 4, 4, 100, 16}}, 1, 1));
  check(parsed.has_value() && is((*parsed)->format, "uastc"), "Basis maps UASTC format");
  parsed = formats::parse_basis(build_basis({{70000, 0, 4, 4, 100, 8}}, 70001));
  check(parsed.has_value() && (*parsed)->layers == 70001, "Basis preserves 24-bit image counts");

  std::vector<BasisSlice> cube_slices;
  for (std::uint32_t image = 0; image < 12; ++image) cube_slices.push_back({image, 0, 4, 4, 353 + image * 8, 8});
  parsed = formats::parse_basis(build_basis(cube_slices, 12, 0, 2));
  check(parsed.has_value() && (*parsed)->faces == 6 && (*parsed)->layers == 2 && (*parsed)->levels.size() == 12,
        "Basis preserves cubemap-array shape");
  const std::vector<BasisSlice> volume_slices = {
      {0, 0, 4, 4, 146, 8}, {1, 0, 4, 4, 154, 8}, {2, 0, 4, 4, 162, 8}};
  parsed = formats::parse_basis(build_basis(volume_slices, 3, 0, 4));
  check(parsed.has_value() && (*parsed)->depth == 3 && (*parsed)->faces == 1 && (*parsed)->layers == 1,
        "Basis preserves volume depth");
  check(!formats::parse_basis(build_basis(volume_slices, 3, 0, 3)).has_value(), "Basis rejects video frames");
  check(!formats::parse_basis(build_basis(volume_slices, 3, 0, 2)).has_value(), "Basis rejects malformed cube counts");

  // One of the upstream fixture manifest's measured cases, reconstructed with its exact descriptors.
  parsed = formats::parse_basis(build_basis({{0, 0, 768, 512, 45273, 62601},
                                             {0, 0, 768, 512, 107874, 40817}}));
  check(parsed.has_value() && (*parsed)->width == 768 && (*parsed)->height == 512 &&
            (*parsed)->levels.size() == 2 && (*parsed)->mip_levels == 1,
        "Basis matches measured alpha3.basis dimensions and slice count");
  check(parsed.has_value() && level_is((*parsed)->levels.element(1), 107874, 40817, 768, 512),
        "Basis matches measured alpha3.basis second slice range");

  check(is(formats::get_basis_parse_failure_reason(Uint8Array{0x73, 0x42}), "header-truncated"),
        "Basis reports truncated detected header");
  check(!formats::parse_basis(build_basis({{0, 0, 4, 4, 100, 8}}, 1, 5)).has_value(),
        "Basis rejects unknown texture format");
  bytes = build_basis({{0, 0, 4, 4, 100, 8}});
  write_le(bytes, 65, 1000000, 4);
  check(!formats::parse_basis(bytes).has_value(), "Basis rejects slice-table overrun");
}

void test_dds() {
  DdsOptions options;
  options.code = four_cc("DXT1");
  auto parsed = formats::parse_dds(build_dds(options));
  check(parsed.has_value() && is((*parsed)->format, "bc1") && (*parsed)->width == 4 &&
            (*parsed)->height == 4 && (*parsed)->mip_levels == 1 && (*parsed)->faces == 1,
        "DDS parses a single-mip DXT1 texture");
  check(parsed.has_value() && level_is((*parsed)->levels.element(0), 128, 8, 4, 4),
        "DDS DXT1 range is byte-exact");

  options = {};
  options.width = 2;
  options.height = 2;
  options.pixel_flags = 0x40;
  options.bits = 32;
  options.red = 0x00ff0000;
  options.green = 0x0000ff00;
  options.blue = 0x000000ff;
  options.alpha = 0xff000000;
  parsed = formats::parse_dds(build_dds(options));
  check(parsed.has_value() && is((*parsed)->format, "bgra8unorm") &&
            level_is((*parsed)->levels.element(0), 128, 16, 2, 2),
        "DDS maps BGRA masks and byte length");
  options.red = 0x000000ff;
  options.blue = 0x00ff0000;
  parsed = formats::parse_dds(build_dds(options));
  check(parsed.has_value() && is((*parsed)->format, "rgba8unorm"), "DDS maps RGBA masks");

  options = {};
  options.code = four_cc("DXT5");
  options.caps2 = 0x200;
  options.data_bytes = 96;
  parsed = formats::parse_dds(build_dds(options));
  check(parsed.has_value() && (*parsed)->faces == 6 && (*parsed)->levels.size() == 6,
        "DDS cubemap has six face subresources");
  check(parsed.has_value() && (*parsed)->levels.element(5)->byte_offset == 208,
        "DDS cubemap face offsets are contiguous");

  options = {};
  options.width = 8;
  options.height = 8;
  options.mip_count = 4;
  options.code = four_cc("DXT1");
  parsed = formats::parse_dds(build_dds(options));
  check(parsed.has_value() && (*parsed)->levels.size() == 4 &&
            level_is((*parsed)->levels.element(3), 176, 8, 1, 1),
        "DDS computes descending BC1 mip ranges");

  options = {};
  options.code = four_cc("DX10");
  options.dx10 = true;
  options.dxgi = 98;
  options.layers = 3;
  parsed = formats::parse_dds(build_dds(options));
  check(parsed.has_value() && is((*parsed)->format, "bc7") && (*parsed)->layers == 3 &&
            (*parsed)->levels.size() == 3,
        "DDS parses DX10 BC7 arrays");
  check(parsed.has_value() && level_is((*parsed)->levels.element(2), 180, 16, 4, 4),
        "DDS DX10 array offset includes extension header");

  check(is(formats::get_dds_parse_failure_reason(Uint8Array{0x44, 0x44, 0x53, 0x20}), "header-truncated"),
        "DDS reports truncated detected header");
  options = {};
  options.code = four_cc("ZZZZ");
  check(!formats::parse_dds(build_dds(options)).has_value(), "DDS rejects unknown FourCC");
  options.code = four_cc("DXT1");
  options.caps2 = 0x200000;
  check(!formats::parse_dds(build_dds(options)).has_value(), "DDS rejects volumes");
}

void test_ktx2() {
  auto parsed = formats::parse_ktx2(build_ktx2(37, 4, 4, {{104, 64}}));
  check(parsed.has_value() && is((*parsed)->format, "rgba8unorm") && (*parsed)->width == 4 &&
            (*parsed)->height == 4 && (*parsed)->depth == 1 && is((*parsed)->supercompression, "None"),
        "KTX2 parses an uncompressed 2D texture");
  check(parsed.has_value() && level_is((*parsed)->levels.element(0), 104, 64, 4, 4),
        "KTX2 2D level range is byte-exact");

  parsed = formats::parse_ktx2(build_ktx2(37, 4, 4, {{104, 384}}, 0, 0, 6));
  check(parsed.has_value() && (*parsed)->faces == 6 && (*parsed)->levels.size() == 6,
        "KTX2 splits an uncompressed cubemap level");
  check(parsed.has_value() && (*parsed)->levels.element(5)->byte_offset == 424 &&
            (*parsed)->levels.element(5)->byte_length == 64,
        "KTX2 cubemap split offsets are exact");
  parsed = formats::parse_ktx2(build_ktx2(37, 4, 4, {{104, 192}}, 0, 3));
  check(parsed.has_value() && (*parsed)->layers == 3 && (*parsed)->levels.size() == 3 &&
            (*parsed)->levels.element(2)->byte_offset == 232,
        "KTX2 splits an uncompressed array level");

  parsed = formats::parse_ktx2(build_ktx2(37, 8, 8,
                                          {{176, 256}, {432, 64}, {496, 16}, {512, 4}}));
  check(parsed.has_value() && (*parsed)->mip_levels == 4 && (*parsed)->levels.element(3)->width == 1 &&
            (*parsed)->levels.element(3)->byte_offset == 512,
        "KTX2 reads the level index mip chain");
  parsed = formats::parse_ktx2(build_ktx2(37, 8, 8, {{128, 1536}, {1664, 384}}, 0, 0, 6));
  check(parsed.has_value() && (*parsed)->levels.size() == 12 &&
            level_is((*parsed)->levels.element(1), 1664, 64, 4, 4) &&
            level_is((*parsed)->levels.element(2), 384, 256, 8, 8),
        "KTX2 normalizes mip-major cubemap data to face-major order");
  parsed = formats::parse_ktx2(build_ktx2(37, 8, 8, {{128, 512}, {640, 128}}, 0, 2));
  check(parsed.has_value() && (*parsed)->levels.size() == 4 &&
            level_is((*parsed)->levels.element(1), 640, 64, 4, 4) &&
            level_is((*parsed)->levels.element(2), 384, 256, 8, 8),
        "KTX2 normalizes mip-major array data to layer-major order");

  parsed = formats::parse_ktx2(build_ktx2(0, 4, 4, {{104, 48}}, 0, 0, 1, 1));
  check(parsed.has_value() && is((*parsed)->format, "etc1s") && is((*parsed)->supercompression, "BasisLZ") &&
            (*parsed)->levels.size() == 1,
        "KTX2 preserves a BasisLZ blob");
  parsed = formats::parse_ktx2(build_ktx2(0, 4, 4, {{104, 64}}));
  check(parsed.has_value() && is((*parsed)->format, "uastc"), "KTX2 maps undefined uncompressed vkFormat to UASTC");
  for (const auto& entry : std::vector<std::pair<std::uint32_t, const char*>>{{2, "Zstd"}, {3, "ZLIB"}}) {
    parsed = formats::parse_ktx2(build_ktx2(37, 4, 4, {{104, 48}}, 0, 2, 1, entry.first));
    check(parsed.has_value() && is((*parsed)->supercompression, entry.second) && (*parsed)->levels.size() == 1 &&
              (*parsed)->levels.element(0)->byte_length == 48,
          std::string("KTX2 preserves indivisible ") + entry.second + " blob");
  }

  const auto magic = prefix(build_ktx2(37, 4, 4, {{104, 64}}), 12);
  check(is(formats::get_ktx2_parse_failure_reason(magic), "header-truncated"),
        "KTX2 reports truncated detected header");
  check(!formats::parse_ktx2(build_ktx2(9999, 4, 4, {{104, 64}})).has_value(),
        "KTX2 rejects unmapped vkFormat");
  auto corrupt = build_ktx2(37, 4, 4, {{104, 64}});
  write_le(corrupt, 88, 1000000, 8);
  check(!formats::parse_ktx2(corrupt).has_value(), "KTX2 rejects level range overrun");
}

}  // namespace

int main() {
  test_byte_reader();
  test_detection_and_explanation();
  test_level_layout();
  test_atf();
  test_basis();
  test_dds();
  test_ktx2();
  if (failures != 0) {
    std::cerr << failures << " texture-formats assertion(s) failed\n";
    return 1;
  }
  std::cout << "texture-formats byte-exact native evidence passed\n";
  return 0;
}
