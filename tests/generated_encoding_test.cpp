#include <flight/encoding/contract.hpp>

#include <initializer_list>
#include <iostream>
#include <string>

// Native port of packages/encoding/src/utf8.test.ts (a627849), all eight cases, against the generated
// headers (no override). Strings that need exact UTF-16 code units -- lone surrogates, NUL -- are built
// from std::u16string so the input is the same sequence of code units the JavaScript string holds.

namespace {

using flight::String;
using flight::Uint8Array;
namespace encoding = flight::encoding;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool bytes_equal(const Uint8Array& actual, std::initializer_list<int> expected) {
  if (actual.size() != expected.size()) return false;
  std::size_t index = 0;
  for (const int byte : expected) {
    if (static_cast<int>(actual[index++]) != byte) return false;
  }
  return true;
}

Uint8Array bytes(std::initializer_list<std::uint8_t> values) { return Uint8Array(values); }

String units(std::u16string value) { return String(std::move(value)); }

template <typename Error, typename Run>
bool throws(Run run) {
  try {
    run();
  } catch (const Error&) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

}  // namespace

int main() {
  // decodeUTF8
  check(encoding::decode_utf8(Uint8Array()) == String("") &&
            encoding::decode_utf8(bytes({0, 72, 101, 108, 108, 111, 0x7f})) == units(std::u16string(u"\0Hello\u007f", 7)),
        "decodeUTF8 decodes empty and ASCII input");
  check(encoding::decode_utf8(bytes({0xc2, 0xa2, 0xe2, 0x82, 0xac, 0xf0, 0x9f, 0x98, 0x80})) == String("¢€\U0001F600"),
        "decodeUTF8 decodes two-, three-, and four-byte sequences");
  check(encoding::decode_utf8(bytes({0x80, 0xc0, 0xaf})) == units(u"���") &&
            encoding::decode_utf8(bytes({0xe2, 0x28, 0xa1})) == units(u"�(�") &&
            encoding::decode_utf8(bytes({0xed, 0xa0, 0x80})) == units(u"���") &&
            encoding::decode_utf8(bytes({0xe2, 0x82})) == units(u"�"),
        "decodeUTF8 replaces malformed and incomplete sequences without consuming the next valid byte");
  const auto window = bytes({0xff, 72, 0xc3, 0xa9, 0xff});
  check(encoding::decode_utf8(window, 1.0, 3.0) == String("Hé") && encoding::decode_utf8(window, 1.0, 0.0) == String("") &&
            encoding::decode_utf8(window, 5.0) == String("") && encoding::decode_utf8(bytes({0xe2, 0x82, 0xac}), 0.0, 2.0) == units(u"�"),
        "decodeUTF8 honors offset and length windows without reading adjacent bytes");
  const auto one = bytes({65});
  check(throws<flight::RangeError>([&] { static_cast<void>(encoding::decode_utf8(one, -1.0)); }) &&
            throws<flight::RangeError>([&] { static_cast<void>(encoding::decode_utf8(one, 0.0, 2.0)); }),
        "decodeUTF8 rejects windows outside the byte array with a RangeError");

  // encodeUTF8
  check(bytes_equal(encoding::encode_utf8(String("")), {}) &&
            bytes_equal(encoding::encode_utf8(units(std::u16string(u"\0Hello\u007f", 7))), {0, 72, 101, 108, 108, 111, 0x7f}),
        "encodeUTF8 encodes empty and ASCII input");
  check(bytes_equal(encoding::encode_utf8(String("¢€\U0001F600")), {0xc2, 0xa2, 0xe2, 0x82, 0xac, 0xf0, 0x9f, 0x98, 0x80}),
        "encodeUTF8 encodes two-, three-, and four-byte sequences");
  std::u16string lone;
  lone.push_back(static_cast<char16_t>(0xd800));
  lone.push_back(u'A');
  lone.push_back(static_cast<char16_t>(0xdc00));
  check(bytes_equal(encoding::encode_utf8(units(lone)), {0xef, 0xbf, 0xbd, 65, 0xef, 0xbf, 0xbd}) &&
            bytes_equal(encoding::encode_utf8(units(u"�")), {0xef, 0xbf, 0xbd}),
        "encodeUTF8 encodes unpaired surrogates as the replacement character");

  return failures == 0 ? 0 : 1;
}
