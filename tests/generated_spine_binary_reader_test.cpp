// The generated @flighthq/skeleton2d-formats Spine binary reader at a627849, exercised natively. Every
// binary .skel field is decoded through these primitives, so they are checked on the format's own terms,
// ported from packages/skeleton2d-formats/src/spineBinaryReader.test.ts: big-endian fixed-width numbers,
// Kryo varints (unsigned and zigzag-signed) across every byte-count boundary, length-prefixed UTF-8 strings
// with their absent/empty distinction, subarray windows that must not leak the backing buffer, and the
// sticky overrun sentinel that replaces throwing on truncated input.
#include <flight/skeleton2d_formats/spine_binary_reader.hpp>

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace spine = flight::skeleton2d_formats;
using Bytes = flight::Uint8Array;

flight::Ref<flight::types::ByteReader> reader(const Bytes& bytes) { return spine::create_spine_binary_reader(bytes); }
bool overrun(const flight::Ref<flight::types::ByteReader>& r) { return spine::is_spine_binary_reader_overrun(view(r)); }

void construction() {
  auto r = reader(Bytes{1, 2, 3});
  check(r->offset == 0 && !overrun(r), "a new reader starts at 0 and is not overrun");

  Bytes backing{0xaa, 0xbb, 0x07, 0xcc};
  auto window = reader(backing.subarray(2, 3));
  check(window->view.byte_length == 1, "a subarray reader sees only its window");
  check(spine::read_spine_binary_byte(window) == 0x07, "reads the window's byte");
  check(spine::read_spine_binary_byte(window) == 0 && overrun(window), "the neighbouring byte is not leaked");
}

void bounds() {
  auto r = reader(Bytes{1, 2});
  check(spine::has_spine_binary_bytes(view(r), 2) && !spine::has_spine_binary_bytes(view(r), 3), "bytes remaining");
  spine::read_spine_binary_byte(r);
  check(spine::has_spine_binary_bytes(view(r), 1) && !spine::has_spine_binary_bytes(view(r), 2), "after one read");
  spine::read_spine_binary_float(r);
  check(!spine::has_spine_binary_bytes(view(r), 0), "nothing remains once overrun");

  auto end = reader(Bytes{1});
  spine::read_spine_binary_byte(end);
  check(end->offset == 1 && !overrun(end), "a cursor exactly at the end is a clean end of stream");

  auto sticky = reader(Bytes{0x41});
  check(spine::read_spine_binary_float(sticky) == 0 && overrun(sticky), "a short read returns 0 and overruns");
  check(spine::read_spine_binary_byte(sticky) == 0, "the unread byte is not handed out afterwards");
  check(!spine::read_spine_binary_string(sticky).has_value(), "a string read after overrun is null");
  check(!spine::read_spine_binary_boolean(sticky) && overrun(sticky), "the overrun mark is sticky");

  auto partial = reader(Bytes{1, 2, 3});
  spine::read_spine_binary_float(partial);
  check(overrun(partial) && partial->offset > partial->view.byte_length, "a short read parks past the end");

  auto skip = reader(Bytes{1, 2, 3, 4});
  spine::skip_spine_binary_bytes(skip, 3);
  check(spine::read_spine_binary_byte(skip) == 4, "skip advances past unmodelled fields");
  auto overskip = reader(Bytes{1, 2});
  spine::skip_spine_binary_bytes(overskip, 5);
  check(overrun(overskip), "skipping past the end overruns");
}

void fixed_width() {
  auto b = reader(Bytes{0, 1, 0xff});
  check(!spine::read_spine_binary_boolean(b) && spine::read_spine_binary_boolean(b) && spine::read_spine_binary_boolean(b),
        "booleans: zero false, nonzero true");
  auto u8 = reader(Bytes{0x00, 0x7f, 0x80, 0xff});
  check(spine::read_spine_binary_byte(u8) == 0 && spine::read_spine_binary_byte(u8) == 0x7f &&
            spine::read_spine_binary_byte(u8) == 0x80 && spine::read_spine_binary_byte(u8) == 0xff,
        "unsigned bytes in order");
  auto f = reader(Bytes{0x3f, 0x80, 0x00, 0x00, 0xc0, 0x20, 0x00, 0x00});
  check(spine::read_spine_binary_float(f) == 1.0 && spine::read_spine_binary_float(f) == -2.5, "big-endian floats");
  auto i = reader(Bytes{0x00, 0x00, 0x01, 0x00, 0xff, 0xff, 0xff, 0xff});
  check(spine::read_spine_binary_int(i) == 256 && spine::read_spine_binary_int(i) == -1, "big-endian signed ints");
  auto s = reader(Bytes{0x01, 0x00, 0xff, 0xff});
  check(spine::read_spine_binary_unsigned_short(s) == 256 && spine::read_spine_binary_unsigned_short(s) == 65535,
        "big-endian unsigned shorts");
}

void varints() {
  auto v = reader(Bytes{0x00, 0x7f, 0x80, 0x01, 0xff, 0x7f, 0x80, 0x80, 0x01});
  check(spine::read_spine_binary_varint(v) == 0, "varint 0");
  check(spine::read_spine_binary_varint(v) == 127, "varint 127, the largest one-byte value");
  check(spine::read_spine_binary_varint(v) == 128, "varint 128, the smallest two-byte value");
  check(spine::read_spine_binary_varint(v) == 16383, "varint 16383, the largest two-byte value");
  check(spine::read_spine_binary_varint(v) == 16384, "varint 16384");
  auto wide = reader(Bytes{0xff, 0xff, 0xff, 0xff, 0x0f});
  check(spine::read_spine_binary_varint(wide) == 4294967295.0, "the widest pattern reads unsigned");
  auto cut = reader(Bytes{0x80});
  check(spine::read_spine_binary_varint(cut) == 0 && overrun(cut), "a missing continuation byte overruns");

  auto z = reader(Bytes{0, 1, 2, 3, 4});
  check(spine::read_spine_binary_signed_varint(z) == 0 && spine::read_spine_binary_signed_varint(z) == -1 &&
            spine::read_spine_binary_signed_varint(z) == 1 && spine::read_spine_binary_signed_varint(z) == -2 &&
            spine::read_spine_binary_signed_varint(z) == 2,
        "zigzag folds 0,1,2,3,4 to 0,-1,1,-2,2");
  auto minimum = reader(Bytes{0xff, 0xff, 0xff, 0xff, 0x0f});
  check(spine::read_spine_binary_signed_varint(minimum) == -2147483648.0, "the widest pattern folds to INT32_MIN");
}

void strings() {
  auto absent = reader(Bytes{0x00, 0x01});
  check(!spine::read_spine_binary_string(absent).has_value(), "prefix 0 is an absent string");
  auto empty = spine::read_spine_binary_string(absent);
  check(empty.has_value() && empty.value() == flight::String(""), "prefix 1 is a present empty string");

  auto text = reader(Bytes{0x04, 0x61, 0x62, 0x63, 0x03, 0xc3, 0xa9});
  auto abc = spine::read_spine_binary_string(text);
  auto accent = spine::read_spine_binary_string(text);
  check(abc.has_value() && abc.value() == flight::String("abc"), "count-1 bytes of UTF-8");
  check(accent.has_value() && accent.value() == flight::String("\xc3\xa9") && !overrun(text), "a multi-byte code point");

  Bytes backing{0xaa, 0x03, 0x68, 0x69, 0xbb};
  auto window = reader(backing.subarray(1, 4));
  auto hi = spine::read_spine_binary_string(window);
  check(hi.has_value() && hi.value() == flight::String("hi"), "a string read from a subarray window");

  auto truncated = reader(Bytes{0x09, 0x61, 0x62});
  check(!spine::read_spine_binary_string(truncated).has_value() && overrun(truncated), "a length past the end is null and overruns");
}

}  // namespace

int main() {
  construction();
  bounds();
  fixed_width();
  varints();
  strings();
  return failures == 0 ? 0 : 1;
}
