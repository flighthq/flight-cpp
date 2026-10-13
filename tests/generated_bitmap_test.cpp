// Focused native parity for @flighthq/bitmap at Flight a627849.
// These fixtures pin storage identity, row-major stride, clipped update order, packed formats,
// and failure behavior rather than duplicating the package's full image-filter test matrix.
#include <flight/bitmap/bitmap.hpp>
#include <flight/bitmap/bitmap_compare.hpp>
#include <flight/bitmap/bitmap_composite.hpp>
#include <flight/bitmap/bitmap_copy.hpp>
#include <flight/bitmap/bitmap_crop.hpp>
#include <flight/bitmap/bitmap_format.hpp>
#include <flight/bitmap/bitmap_pixel.hpp>
#include <flight/bitmap/bitmap_region.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

namespace {

namespace bitmap = flight::bitmap;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return View(value);
}

flight::Ref<flight::types::Rectangle> rectangle(
    double x, double y, double width, double height) {
  auto value = flight::make_ref<flight::types::Rectangle>();
  value->entity_runtime_key = std::nullopt;
  value->height = height;
  value->width = width;
  value->x = x;
  value->y = y;
  return value;
}

std::uint32_t pixel(
    const flight::Ref<flight::types::Bitmap>& value, double x, double y) {
  return static_cast<std::uint32_t>(bitmap::get_bitmap_pixel(readonly(value), x, y));
}

void construction_clone_and_region_identity() {
  const auto source = bitmap::create_bitmap(3.0, 2.0, 0x11223344u);
  check(source->width == 3.0 && source->height == 2.0 && source->data.length == 24,
        "createBitmap dimensions determine a four-byte row-major buffer");
  check(source->version == 0.0 && source->format == flight::String("rgba8unorm") &&
            source->gamut == flight::String("srgb") && pixel(source, 2.0, 1.0) == 0x11223344u,
        "createBitmap initializes metadata and every packed RGBA pixel");

  const void* source_storage = source->data.identity();
  bitmap::invalidate_bitmap(source);
  check(source->version == 1.0 && source->data.identity() == source_storage,
        "invalidateBitmap changes version without replacing pixel storage");

  const auto clone = bitmap::clone_bitmap(readonly(source));
  check(clone != source && clone->data.identity() != source->data.identity() &&
            clone->data.length == source->data.length && pixel(clone, 2.0, 1.0) == 0x11223344u,
        "cloneBitmap copies values into a distinct bitmap and byte buffer");
  clone->data.set_index(0.0, 0xff);
  check(source->data.get_index(0.0) == 0x11,
        "mutating clone storage cannot alias the source buffer");

  const auto region = bitmap::create_bitmap_region(source);
  check(region->bitmap == source && region->x == 0.0 && region->y == 0.0 &&
            region->width == 3.0 && region->height == 2.0,
        "createBitmapRegion defaults to the exact owning bitmap dimensions");
  const auto replacement = bitmap::create_bitmap(7.0, 5.0);
  const auto returned = bitmap::set_bitmap_region(region, replacement, 1.0, 2.0, 3.0, 4.0);
  check(returned == region && region->bitmap == replacement && region->x == 1.0 &&
            region->y == 2.0 && region->width == 3.0 && region->height == 4.0,
        "setBitmapRegion mutates and returns the same region while replacing bitmap identity");
}

void pixel_stride_and_bulk_update_order() {
  const auto source = bitmap::create_bitmap(3.0, 2.0);
  const std::uint32_t colors[] = {
      0x01020304u, 0x11121314u, 0x21222324u,
      0x31323334u, 0x41424344u, 0x51525354u,
  };
  for (double index = 0.0; index < 6.0; index += 1.0) {
    bitmap::set_bitmap_pixel(
        source,
        static_cast<double>(static_cast<int>(index) % 3),
        static_cast<double>(static_cast<int>(index) / 3),
        colors[static_cast<std::size_t>(index)]);
  }
  check(source->data.get_index(20.0) == 0x51 && source->data.get_index(23.0) == 0x54 &&
            pixel(source, 2.0, 1.0) == 0x51525354u,
        "last-pixel access uses (y * width + x) * 4 stride");

  const auto middle = bitmap::create_bitmap_region(source, 1.0, 0.0, 2.0, 2.0);
  flight::Uint32Array packed(4.0);
  bitmap::extract_bitmap_pixels32(packed, readonly(middle));
  check(packed.get_index(0.0) == colors[1] && packed.get_index(1.0) == colors[2] &&
            packed.get_index(2.0) == colors[4] && packed.get_index(3.0) == colors[5],
        "extractBitmapPixels32 is tightly packed in source-region row order");

  flight::Uint8ClampedArray bytes(16.0);
  bitmap::extract_bitmap_pixels(bytes, readonly(middle));
  check(bytes.get_index(0.0) == 0x11 && bytes.get_index(7.0) == 0x24 &&
            bytes.get_index(8.0) == 0x41 && bytes.get_index(15.0) == 0x54,
        "byte extraction uses region width rather than bitmap width as output stride");

  const auto destination = bitmap::create_bitmap(3.0, 2.0);
  const auto destination_storage = destination->data.identity();
  const auto destination_region =
      bitmap::create_bitmap_region(destination, 0.0, 0.0, 2.0, 2.0);
  const double version = destination->version;
  bitmap::write_bitmap_pixels32(readonly(destination_region), packed);
  check(destination->version == version + 1.0 &&
            destination->data.identity() == destination_storage,
        "writeBitmapPixels32 mutates shared storage then invalidates exactly once");
  check(pixel(destination, 0.0, 0.0) == colors[1] &&
            pixel(destination, 1.0, 0.0) == colors[2] &&
            pixel(destination, 0.0, 1.0) == colors[4] &&
            pixel(destination, 1.0, 1.0) == colors[5] &&
            pixel(destination, 2.0, 1.0) == 0u,
        "writeBitmapPixels32 consumes tightly packed input without crossing region bounds");
}

void clipped_copy_crop_and_failure_behavior() {
  const auto source = bitmap::create_bitmap(3.0, 2.0);
  bitmap::set_bitmap_pixel(source, 0.0, 0.0, 0xaabbccddu);
  bitmap::set_bitmap_pixel(source, 1.0, 0.0, 0x10203040u);
  bitmap::set_bitmap_pixel(source, 0.0, 1.0, 0x50607080u);
  bitmap::set_bitmap_pixel(source, 1.0, 1.0, 0x90a0b0c0u);

  const auto destination = bitmap::create_bitmap(3.0, 2.0);
  const auto source_region = bitmap::create_bitmap_region(source, -1.0, 0.0, 3.0, 2.0);
  const auto destination_region =
      bitmap::create_bitmap_region(destination, 0.0, 0.0, 3.0, 2.0);
  bitmap::copy_bitmap_pixels(readonly(destination_region), readonly(source_region));
  check(pixel(destination, 0.0, 0.0) == 0u &&
            pixel(destination, 1.0, 0.0) == 0xaabbccddu &&
            pixel(destination, 2.0, 0.0) == 0x10203040u &&
            pixel(destination, 1.0, 1.0) == 0x50607080u,
        "copyBitmapPixels clips each paired coordinate without compacting skipped columns");

  const auto crop_bounds = rectangle(-1.0, 0.0, 3.0, 2.0);
  const auto cropped = bitmap::crop_bitmap(readonly(source), readonly(crop_bounds));
  check(cropped != source && cropped->data.identity() != source->data.identity() &&
            cropped->width == 3.0 && cropped->height == 2.0 &&
            cropped->format == source->format && cropped->gamut == source->gamut &&
            cropped->alpha_type == source->alpha_type,
        "cropBitmap allocates requested dimensions and preserves pixel metadata");
  check(pixel(cropped, 0.0, 0.0) == 0u &&
            pixel(cropped, 1.0, 0.0) == 0xaabbccddu &&
            pixel(cropped, 2.0, 1.0) == 0x90a0b0c0u,
        "cropBitmap keeps transparent holes at out-of-bounds coordinates");

  const auto empty = bitmap::crop_bitmap(readonly(source), readonly(rectangle(0.0, 0.0, -2.0, -3.0)));
  check(empty->width == 0.0 && empty->height == 0.0 && empty->data.length == 0,
        "negative crop dimensions clamp to an empty bitmap");

  const auto mismatched = bitmap::create_bitmap(1.0, 1.0);
  const double source_version = source->version;
  const void* source_storage = source->data.identity();
  bool threw = false;
  try {
    static_cast<void>(bitmap::compare_bitmap(readonly(source), readonly(mismatched)));
  } catch (const flight::Error&) {
    threw = true;
  }
  check(threw && source->version == source_version && source->data.identity() == source_storage,
        "compareBitmap rejects dimension mismatch without mutating either source");
}

void pixel_format_and_aliasing() {
  const flight::Uint8ClampedArray rgba{
      0x11, 0x22, 0x33, 0x44,
      0xaa, 0xbb, 0xcc, 0xdd,
  };
  flight::Uint8ClampedArray bgra(8.0);
  bitmap::convert_bitmap_pixel_order(
      bgra, rgba, 8.0, flight::String("RGBA"), flight::String("BGRA"));
  check(bgra.get_index(0.0) == 0x33 && bgra.get_index(2.0) == 0x11 &&
            bgra.get_index(4.0) == 0xcc && bgra.get_index(6.0) == 0xaa,
        "RGBA to BGRA converts every pixel without changing alpha");

  flight::Uint8ClampedArray round_trip(8.0);
  bitmap::convert_bitmap_pixel_order(
      round_trip, bgra, 8.0, flight::String("BGRA"), flight::String("RGBA"));
  check(round_trip.get_index(0.0) == 0x11 && round_trip.get_index(3.0) == 0x44 &&
            round_trip.get_index(4.0) == 0xaa && round_trip.get_index(7.0) == 0xdd,
        "pixel-order conversion round-trips both pixels exactly");

  flight::Uint8ClampedArray in_place{0x11, 0x22, 0x33, 0x44};
  const void* in_place_storage = in_place.identity();
  bitmap::convert_bitmap_pixel_order(
      in_place, in_place, 4.0, flight::String("RGBA"), flight::String("ARGB"));
  check(in_place.identity() == in_place_storage && in_place.get_index(0.0) == 0x44 &&
            in_place.get_index(1.0) == 0x11 && in_place.get_index(3.0) == 0x33,
        "in-place RGBA to ARGB conversion reads all channels before writing");

  flight::Uint8ClampedArray alpha{0xc0, 0x80, 0x40, 0x80};
  bitmap::premultiply_bitmap_pixels(alpha, alpha, 4.0);
  check(alpha.get_index(0.0) == 96.0 && alpha.get_index(1.0) == 64.0 &&
            alpha.get_index(2.0) == 32.0 && alpha.get_index(3.0) == 128.0,
        "in-place premultiplication rounds channels and preserves alpha");
  bitmap::unpremultiply_bitmap_pixels(alpha, alpha, 4.0);
  check(alpha.get_index(0.0) == 191.0 && alpha.get_index(1.0) == 128.0 &&
            alpha.get_index(2.0) == 64.0 && alpha.get_index(3.0) == 128.0,
        "unpremultiplication restores the rounded straight-alpha pixel");
}

}  // namespace

int main() {
  construction_clone_and_region_identity();
  pixel_stride_and_bulk_update_order();
  clipped_copy_crop_and_failure_behavior();
  pixel_format_and_aliasing();
  return failures == 0 ? 0 : 1;
}
