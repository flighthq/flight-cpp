// The generated @flighthq/swf SwfReader at a627849, exercised natively. Every SWF structure is decoded
// through this bounded reader, so its constructor is checked directly first: `source` and `end` are
// TypeScript constructor parameter properties, which the emitter used to declare and never assign
// (docs/compiler-requests/constructor-parameter-properties-dropped.md). The reader must hold the caller's
// own byte buffer, start where it was told, and stop at `end` even when the buffer continues past it.
// The remaining expectations are ported from packages/swf/src/swfReader.test.ts.
#include <flight/swf/swf_reader.hpp>

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

using flight::swf::SwfReader;
using Bytes = flight::Uint8Array;

flight::Ref<SwfReader> reader(const Bytes& bytes, double start, double end) {
  return flight::make_ref<SwfReader>(bytes, start, end);
}

void construction() {
  Bytes bytes{0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  auto r = reader(bytes, 0, 6);
  check(r->source == bytes && r->source.identity() == bytes.identity(), "source is the caller's own buffer");
  check(r->source.size() == 6, "source keeps its length");
  check(r->end == 6, "end is the constructor's end");
  check(r->pos == 0 && r->bit_position == 0 && r->valid, "starts at start, byte-aligned and valid");

  // A window inside a larger buffer: start at 2, stop at 4 although bytes continue to 6.
  auto window = reader(bytes, 2, 4);
  check(window->pos == 2 && window->end == 4, "a window keeps its own start and end");
  check(window->read_uint8() == 0x03 && window->read_uint8() == 0x04 && window->valid, "reads inside the window");
  check(window->read_uint8() == 0 && !window->valid, "stops at end, not at the buffer's length");

  // The reader reads the caller's bytes, not a copy: a write through the caller's view is seen.
  Bytes shared{0x10, 0x20};
  auto live = reader(shared, 0, 2);
  shared[1] = 0x7f;
  check(live->read_uint16() == 0x7f10, "reads through the shared buffer");
}

void integers() {
  auto r = reader(Bytes{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}, 0, 6);
  check(r->read_uint8() == 0x01, "uint8");
  check(r->read_uint16() == 0x0302, "little-endian uint16");
  check(r->valid, "still valid");
  // Only three bytes remain, so the fourth reads as zero; valid, not the value, reports the overrun.
  r->read_uint32();
  check(!r->valid, "an overrun clears valid instead of throwing");
  check(r->read_uint8() == 0, "reads past the end return zero");
}

void bits() {
  // 1011 0100 1111 0000: a 3-bit 5, a 5-bit signed -12, then an 8-bit 240.
  auto r = reader(Bytes{0b10110100, 0b11110000}, 0, 2);
  check(r->read_unsigned_bits(3) == 0b101, "big-endian unsigned bits");
  check(r->read_signed_bits(5) == -12, "signed bits across the byte boundary");
  check(r->read_unsigned_bits(8) == 0b11110000, "bits continuing into the next byte");
  check(r->valid, "bit reads stay valid in range");

  auto zero = reader(Bytes{0xff}, 0, 1);
  check(zero->read_signed_bits(0) == 0, "a zero-width field is zero");
  check(zero->read_uint8() == 0xff, "and consumes no input");

  auto align = reader(Bytes{0b10000000, 0x42}, 0, 2);
  check(align->read_unsigned_bits(1) == 1, "one bit");
  check(align->read_uint8() == 0x42, "a byte read aligns to the next byte first");
}

void strings_and_encodings() {
  Bytes bytes{0x68, 0x69, 0x00, 0x6e, 0x6f};
  auto r = reader(bytes, 0, 5);
  check(r->read_string() == flight::String("hi"), "a null-terminated string");
  check(r->read_string() == flight::String("") && !r->valid, "an unterminated string fails");

  auto e = reader(Bytes{0x7f, 0xc8, 0x01, 0x80, 0xff}, 0, 5);
  check(e->read_encoded_uint32() == 127, "single-byte EncodedU32");
  check(e->read_encoded_uint32() == 200, "multi-byte EncodedU32");
  check(e->read_fixed8() == -0.5, "signed 8.8 fixed");
}

}  // namespace

int main() {
  construction();
  integers();
  bits();
  strings_and_encodings();
  return failures == 0 ? 0 : 1;
}
