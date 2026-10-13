#include <flight/image_codec/contract.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <variant>

// Native, byte-exact port of every case in packages/image-codec/src/*.test.ts at Flight a627849.
// The MIME fixtures below are the upstream byte sequences and FileTypeBox builder, independently
// reconstructed before the generated detector consumes them. Decode and encode use faithful host
// capability doubles so byte identity, decoded metadata, option forwarding, and missing-capability
// failures are all observable without pretending that @flighthq/image's currently refused browser
// and erased-value modules are available.

namespace {

using flight::ArrayBuffer;
using flight::Ref;
using flight::String;
using flight::StructuralRef;
using flight::Uint8Array;
using flight::Uint8ClampedArray;
using flight::types::DecodedImage;
using flight::types::HostImageDecodeCapabilities;
using flight::types::HostImageDecodeFormatCapability;
using flight::types::HostImageEncodeCapabilities;
using flight::types::HostImageEncodeFormatCapability;
using flight::types::ImageDecodeOptions;
using flight::types::ImageEncodeOptions;
namespace codec = flight::image_codec;

using DecodeCapsView =
    StructuralRef<flight::RowReadonly<flight::RowOf<Ref<HostImageDecodeCapabilities>>>>;
using DecodeOptionsView =
    StructuralRef<flight::RowReadonly<flight::RowOf<Ref<ImageDecodeOptions>>>>;
using EncodeCapsView =
    StructuralRef<flight::RowReadonly<flight::RowOf<Ref<HostImageEncodeCapabilities>>>>;
using EncodeOptionsView =
    StructuralRef<flight::RowReadonly<flight::RowOf<Ref<ImageEncodeOptions>>>>;
using ImageView = StructuralRef<flight::RowReadonly<flight::RowOf<Ref<DecodedImage>>>>;
using DetectInput = std::variant<ArrayBuffer, Uint8Array>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool bytes_equal(const Uint8Array& actual, const Uint8Array& expected) {
  if (actual.size() != expected.size()) return false;
  for (std::size_t i = 0; i < actual.size(); ++i) {
    if (actual[i] != expected[i]) return false;
  }
  return true;
}

bool pixels_equal(const Uint8ClampedArray& actual, const Uint8ClampedArray& expected) {
  if (actual.size() != expected.size()) return false;
  for (std::size_t i = 0; i < actual.size(); ++i) {
    if (actual[i] != expected[i]) return false;
  }
  return true;
}

ArrayBuffer buffer_with(std::size_t size, const Uint8Array& prefix) {
  auto buffer = ArrayBuffer::allocate(size);
  Uint8Array(buffer).set(prefix);
  return buffer;
}

void write_ascii(Uint8Array& bytes, std::size_t offset, const char* value) {
  for (std::size_t i = 0; i < 4; ++i) {
    bytes[offset + i] = static_cast<std::uint8_t>(value[i]);
  }
}

void write_be32(Uint8Array& bytes, std::uint32_t value) {
  bytes[0] = static_cast<std::uint8_t>(value >> 24);
  bytes[1] = static_cast<std::uint8_t>(value >> 16);
  bytes[2] = static_cast<std::uint8_t>(value >> 8);
  bytes[3] = static_cast<std::uint8_t>(value);
}

Uint8Array file_type_box(const char* major,
                         std::optional<const char*> compatible = std::nullopt) {
  Uint8Array bytes(compatible.has_value() ? 20 : 16);
  write_be32(bytes, static_cast<std::uint32_t>(bytes.byte_length));
  write_ascii(bytes, 4, "ftyp");
  write_ascii(bytes, 8, major);
  if (compatible.has_value()) write_ascii(bytes, 16, *compatible);
  return bytes;
}

std::optional<String> detect(const Uint8Array& bytes) {
  return codec::detect_image_mime_type(
      DetectInput{std::in_place_type<Uint8Array>, bytes});
}

std::optional<String> detect(const ArrayBuffer& buffer) {
  return codec::detect_image_mime_type(
      DetectInput{std::in_place_type<ArrayBuffer>, buffer});
}

void check_mime(const std::optional<String>& actual,
                const char* expected,
                const std::string& message) {
  check(actual.has_value() && *actual == String(expected), message);
}

void test_detect_image_mime_type() {
  check(!detect(ArrayBuffer::allocate(2)).has_value(),
        "a buffer shorter than four bytes has no MIME type");
  check(!detect(buffer_with(16, Uint8Array{0x00, 0x01, 0x02, 0x03})).has_value(),
        "an unrecognized header has no MIME type");

  check_mime(detect(buffer_with(16, Uint8Array{0x89, 0x50, 0x4e, 0x47, 0x0d,
                                                0x0a, 0x1a, 0x0a})),
             "image/png", "PNG is detected from its byte-exact signature");
  check_mime(detect(buffer_with(16, Uint8Array{0xff, 0xd8, 0xff, 0xe0})),
             "image/jpeg", "JPEG is detected from its byte-exact signature");
  check_mime(detect(buffer_with(16, Uint8Array{0x47, 0x49, 0x46, 0x38, 0x39, 0x61})),
             "image/gif", "GIF is detected from its byte-exact signature");
  check_mime(detect(buffer_with(16, Uint8Array{0x52, 0x49, 0x46, 0x46, 0, 0,
                                                0, 0, 0x57, 0x45, 0x42, 0x50})),
             "image/webp", "WebP requires both RIFF and WEBP signatures");

  check_mime(detect(file_type_box("avif")), "image/avif",
             "AVIF is detected from its major brand");
  check_mime(detect(file_type_box("mif1", "avis")), "image/avif",
             "an AVIF sequence is detected from a compatible brand");
  check(!detect(file_type_box("isom", "mp42")).has_value(),
        "a non-AVIF FileTypeBox is rejected");
  check(!detect(file_type_box("avif").subarray(0, 12)).has_value(),
        "a truncated AVIF FileTypeBox is rejected");

  auto brand_outside_box = Uint8Array(20);
  brand_outside_box.set(file_type_box("isom"));
  write_ascii(brand_outside_box, 16, "avif");
  check(!detect(brand_outside_box).has_value(),
        "an AVIF brand outside the declared box is rejected");

  auto undersized_box = file_type_box("avif");
  write_be32(undersized_box, 15);
  check(!detect(undersized_box).has_value(), "a FileTypeBox smaller than its fixed fields is rejected");
  auto oversized_box = file_type_box("avif");
  write_be32(oversized_box, 20);
  check(!detect(oversized_box).has_value(), "a FileTypeBox extending past the carrier is rejected");
  auto misaligned_box = Uint8Array(18);
  write_be32(misaligned_box, 18);
  write_ascii(misaligned_box, 4, "ftyp");
  write_ascii(misaligned_box, 8, "isom");
  check(!detect(misaligned_box).has_value(),
        "a FileTypeBox with a partial compatible-brand word is rejected");

  check_mime(detect(Uint8Array{0x00, 0x00, 0x01, 0x00}), "image/x-icon",
             "ICO is detected from its byte-exact signature");
  check_mime(detect(Uint8Array{0x49, 0x49, 0x2a, 0x00}), "image/tiff",
             "little-endian TIFF is detected");
  check_mime(detect(Uint8Array{0x4d, 0x4d, 0x00, 0x2a}), "image/tiff",
             "big-endian TIFF is detected");
  check_mime(detect(buffer_with(16, Uint8Array{0x42, 0x4d})), "image/bmp",
             "BMP is detected from an ArrayBuffer carrier");
  check_mime(detect(Uint8Array{0x89, 0x50, 0x4e, 0x47}), "image/png",
             "a Uint8Array carrier is accepted directly");

  check(!detect(Uint8Array{0x52, 0x49, 0x46, 0x46, 0, 0, 0, 0}).has_value(),
        "a short RIFF carrier is not guessed to be WebP");
  check(!detect(Uint8Array{0x52, 0x49, 0x46, 0x46, 0, 0, 0, 0,
                           0x57, 0x45, 0x42, 0x58})
             .has_value(),
        "a RIFF carrier with the wrong form type is rejected");
}

Ref<DecodedImage> fake_decoded() {
  return flight::make_ref<DecodedImage>(DecodedImage{
      .data = Uint8ClampedArray{1, 2, 3, 4}, .width = 1, .height = 1});
}

struct DecodeCapture {
  int calls = 0;
  std::optional<Uint8Array> bytes;
  bool saw_options = false;
  std::optional<bool> premultiply_alpha;
};

Ref<HostImageDecodeFormatCapability> decode_slot(DecodeCapture& capture,
                                                  const Ref<DecodedImage>& result) {
  auto slot = flight::make_ref<HostImageDecodeFormatCapability>();
  slot->decode = [&](Uint8Array bytes, std::optional<DecodeOptionsView> options) {
    ++capture.calls;
    capture.bytes = bytes;
    capture.saw_options = options.has_value();
    capture.premultiply_alpha =
        options.has_value()
            ? flight::row_get<flight::RowKey<"premultiplyAlpha">>(*options)
            : std::nullopt;
    return flight::resolve_task(result);
  };
  return slot;
}

void check_decoded_result(const std::optional<Ref<DecodedImage>>& actual,
                          const Ref<DecodedImage>& expected,
                          const std::string& message) {
  check(actual.has_value() && *actual == expected, message + " preserves result identity");
  check(actual.has_value() && (*actual)->width == 1 && (*actual)->height == 1,
        message + " preserves dimensions");
  check(actual.has_value() &&
            pixels_equal((*actual)->data, Uint8ClampedArray{1, 2, 3, 4}),
        message + " preserves RGBA bytes");
}

void test_decode_image() {
  const auto png_bytes = Uint8Array{0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
  const auto decoded = fake_decoded();

  DecodeCapture detected_capture;
  auto detected_slot = decode_slot(detected_capture, decoded);
  auto detected_caps = flight::make_ref<HostImageDecodeCapabilities>();
  detected_caps->png = detected_slot;
  const auto detected = codec::decode_image(DecodeCapsView(detected_caps), png_bytes).get();
  check_decoded_result(detected, decoded, "auto-detected decode");
  check(detected_capture.calls == 1 && detected_capture.bytes.has_value() &&
            detected_capture.bytes->identity() == png_bytes.identity(),
        "auto-detected decode dispatches once with the exact input byte view");
  check(!detected_capture.saw_options,
        "straight decode omits the premultiplication option bag");

  DecodeCapture explicit_capture;
  auto explicit_caps = flight::make_ref<HostImageDecodeCapabilities>();
  explicit_caps->jpeg = decode_slot(explicit_capture, decoded);
  const auto explicit_result = codec::decode_image(
      DecodeCapsView(explicit_caps), png_bytes, String("image/jpeg")).get();
  check_decoded_result(explicit_result, decoded, "explicit-MIME decode");
  check(explicit_capture.calls == 1,
        "an explicit MIME type takes precedence over the detected PNG type");

  const auto empty_caps = flight::make_ref<HostImageDecodeCapabilities>();
  check(!codec::decode_image(DecodeCapsView(empty_caps), png_bytes).get().has_value(),
        "decode returns null when the detected format has no capability slot");
  check(!codec::decode_image(DecodeCapsView(detected_caps), Uint8Array{0, 1, 2, 3})
             .get()
             .has_value(),
        "decode returns null when the MIME type cannot be determined");

  DecodeCapture premultiplied_capture;
  auto premultiplied_caps = flight::make_ref<HostImageDecodeCapabilities>();
  premultiplied_caps->png = decode_slot(premultiplied_capture, decoded);
  const auto premultiplied = codec::decode_image_premultiplied(
      DecodeCapsView(premultiplied_caps), png_bytes, String("image/png")).get();
  check_decoded_result(premultiplied, decoded, "premultiplied decode");
  check(premultiplied_capture.calls == 1 && premultiplied_capture.saw_options &&
            premultiplied_capture.premultiply_alpha == std::optional<bool>{true},
        "premultiplied decode forwards premultiplyAlpha true");
  check(premultiplied_capture.bytes.has_value() &&
            premultiplied_capture.bytes->identity() == png_bytes.identity(),
        "premultiplied decode preserves the input byte-view identity");

  DecodeCapture automatic_premultiplied_capture;
  auto automatic_premultiplied_caps = flight::make_ref<HostImageDecodeCapabilities>();
  automatic_premultiplied_caps->png =
      decode_slot(automatic_premultiplied_capture, decoded);
  const auto automatic_premultiplied = codec::decode_image_premultiplied(
      DecodeCapsView(automatic_premultiplied_caps), png_bytes).get();
  check_decoded_result(automatic_premultiplied, decoded,
                       "auto-detected premultiplied decode");
  check(automatic_premultiplied_capture.calls == 1 &&
            automatic_premultiplied_capture.premultiply_alpha ==
                std::optional<bool>{true},
        "premultiplied decode auto-detects PNG and forwards its option");
  check(!codec::decode_image_premultiplied(DecodeCapsView(empty_caps), png_bytes)
             .get()
             .has_value(),
        "premultiplied decode returns null when its capability slot is absent");
}

struct EncodeCapture {
  int calls = 0;
  std::optional<ImageView> image;
  bool saw_options = false;
  std::optional<double> quality;
};

Ref<HostImageEncodeFormatCapability> encode_slot(EncodeCapture& capture,
                                                  const Uint8Array& result) {
  auto slot = flight::make_ref<HostImageEncodeFormatCapability>();
  slot->encode = [&](ImageView image, std::optional<EncodeOptionsView> options) {
    ++capture.calls;
    capture.image = image;
    capture.saw_options = options.has_value();
    capture.quality = options.has_value()
                          ? flight::row_get<flight::RowKey<"quality">>(*options)
                          : std::nullopt;
    return flight::resolve_task(result);
  };
  return slot;
}

void test_encode_image() {
  const auto image = fake_decoded();
  const ImageView image_view(image);
  const auto encoded_bytes = Uint8Array{9, 8, 7};
  EncodeCapture png_capture;
  auto png_caps = flight::make_ref<HostImageEncodeCapabilities>();
  png_caps->png = encode_slot(png_capture, encoded_bytes);

  const auto encoded = codec::encode_image(
      EncodeCapsView(png_caps), image_view, String("image/png")).get();
  check(encoded.has_value() && encoded->identity() == encoded_bytes.identity(),
        "encode returns the exact Uint8Array produced by the host slot");
  check(encoded.has_value() && bytes_equal(*encoded, Uint8Array{9, 8, 7}),
        "encode preserves every host result byte");
  check(png_capture.calls == 1 && png_capture.image.has_value() &&
            png_capture.image->identity() == image_view.identity(),
        "encode dispatches once with the exact decoded-image view");
  check(!png_capture.saw_options, "encode preserves an omitted option bag");

  EncodeCapture jpeg_capture;
  auto jpeg_caps = flight::make_ref<HostImageEncodeCapabilities>();
  jpeg_caps->jpeg = encode_slot(jpeg_capture, Uint8Array{});
  const auto options = flight::make_ref<ImageEncodeOptions>(
      ImageEncodeOptions{.quality = std::optional<double>{0.5}});
  static_cast<void>(codec::encode_image(
      EncodeCapsView(jpeg_caps), image_view, String("image/jpeg"),
      std::optional<EncodeOptionsView>{EncodeOptionsView(options)}).get());
  check(jpeg_capture.calls == 1 && jpeg_capture.saw_options &&
            jpeg_capture.quality == std::optional<double>{0.5},
        "encode forwards quality metadata byte-for-byte as the same double value");

  const auto empty_caps = flight::make_ref<HostImageEncodeCapabilities>();
  check(!codec::encode_image(EncodeCapsView(empty_caps), image_view, String("image/png"))
             .get()
             .has_value(),
        "encode returns null when the requested capability slot is absent");
  check(!codec::encode_image(EncodeCapsView(png_caps), image_view, String("image/avif"))
             .get()
             .has_value(),
        "encode rejects an unsupported MIME type without calling another slot");
  check(png_capture.calls == 1,
        "an unsupported encode MIME type never falls back to the PNG slot");
}

}  // namespace

int main() {
  test_detect_image_mime_type();
  test_decode_image();
  test_encode_image();
  if (failures != 0) {
    std::cerr << failures << " image-codec assertion(s) failed\n";
    return 1;
  }
  std::cout << "image-codec generated semantics passed\n";
  return 0;
}
