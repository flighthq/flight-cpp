#include <flight/compression/contract.hpp>

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

// Native port of packages/compression/src/{deflateFormat,compress,deflate,lzma,lzmaCompress}.test.ts
// (a627849) against the generated headers (no override). The compressed fixtures are the upstream ones,
// precomputed by Node's zlib and Python's lzma and embedded as base64, so they are independent oracles.
// The base64 fixture decoder below is ported from the test file itself.
//
// BLOCKED: every case that needs decompressDeflate to inflate. raw_inflate's InflateState is emitted with
// its constructor parameter properties (input, position, outputLimit) declared but never assigned, so
// read_bit() throws "deflate: read past end of stream" on any input and decompressDeflate returns null for
// everything; see docs/compiler-requests/constructor-parameter-properties-dropped.md. That removes all
// compressDeflate and compressDeflateZlib round trips, the sdk slot round trips, and every decompressDeflate
// success case. It also removes the rejection cases that reach raw_inflate, which would pass only because
// everything is null. The cases kept below do not inflate, or are rejected by the zlib header and framing
// checks before raw_inflate runs.
//
// Not ported: 'sdkHostDecompressDeflate is a ready-made deflate slot' and its LZMA twin, which assert
// `slot.decompress` is the SAME function object as decompressDeflate. std::function has no identity to
// compare; the behavioural twin ('... exactly as the bare function does') is ported for both.

namespace {

using flight::String;
using flight::Uint8Array;
namespace compression = flight::compression;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

const String& raw() { return flight::types::compression_framing->raw; }
const String& rfc1950() { return flight::types::compression_framing->rfc1950; }

Uint8Array text(const std::string& value) {
  Uint8Array out(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) out[i] = static_cast<std::uint8_t>(value[i]);
  return out;
}

std::string repeat(const std::string& value, int count) {
  std::string out;
  for (int i = 0; i < count; ++i) out += value;
  return out;
}

bool same(const Uint8Array& left, const Uint8Array& right) {
  if (left.size() != right.size()) return false;
  for (std::size_t i = 0; i < left.size(); ++i)
    if (left[i] != right[i]) return false;
  return true;
}

bool same(const std::optional<Uint8Array>& left, const Uint8Array& right) { return left.has_value() && same(*left, right); }

Uint8Array copy_of(const Uint8Array& value) { return value.slice(); }

Uint8Array decode_base64(const std::string& input) {
  const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string clean = input;
  while (!clean.empty() && clean.back() == '=') clean.pop_back();
  Uint8Array out((clean.size() * 3) >> 2);
  std::uint32_t accumulator = 0;
  int bits = 0;
  std::size_t o = 0;
  for (const char c : clean) {
    accumulator = (accumulator << 6) | static_cast<std::uint32_t>(alphabet.find(c));
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out[o++] = static_cast<std::uint8_t>((accumulator >> bits) & 0xff);
    }
  }
  return out;
}

std::optional<Uint8Array> inflate(const Uint8Array& bytes, double length, const String& framing) {
  return compression::decompress_deflate(bytes, length, framing);
}

std::optional<Uint8Array> unlzma(const Uint8Array& bytes, double length, const String& framing) {
  return compression::decompress_lzma(bytes, length, framing);
}

void test_deflate_format() {
  check(compression::compute_adler32(Uint8Array(0)) == 1.0, "computeAdler32 returns the initial value for an empty input");
  check(compression::compute_adler32(text("Wikipedia")) == static_cast<double>(0x11e60398), "computeAdler32 matches the RFC 1950 checksum for 'Wikipedia'");
  check(compression::compute_adler32(Uint8Array{1, 2}) != compression::compute_adler32(Uint8Array{2, 1}), "computeAdler32 is order sensitive");
  Uint8Array ff(10000);
  for (std::size_t i = 0; i < ff.size(); ++i) ff[i] = 0xff;
  const double checksum = compression::compute_adler32(ff);
  check(checksum >= 0.0 && checksum <= 4294967295.0 && checksum == static_cast<double>(static_cast<std::uint64_t>(checksum)),
        "computeAdler32 stays a 32-bit unsigned value past the point the sums wrap");

  const auto& length_base = compression::length_base;
  const auto& length_extra = compression::length_extra;
  const auto& distance_base = compression::distance_base;
  const auto& distance_extra = compression::distance_extra;
  check(length_base.size() == length_extra.size() && length_base[0] == 3.0 && length_base[length_base.size() - 1] == 258.0 &&
            length_extra[length_extra.size() - 1] == 0.0,
        "LENGTH tables pair every code with its extra-bit count and end at the maximum match");
  const auto last = distance_base.size() - 1;
  check(distance_base.size() == distance_extra.size() && distance_base[0] == 1.0 &&
            distance_base[last] + static_cast<double>(1 << static_cast<int>(distance_extra[last])) - 1.0 == 32768.0,
        "DISTANCE tables pair every code with its extra-bit count and cover the whole window");
  bool distance_tiles = true;
  for (std::size_t i = 0; i + 1 < distance_base.size(); ++i)
    distance_tiles = distance_tiles && distance_base[i] + static_cast<double>(1 << static_cast<int>(distance_extra[i])) == distance_base[i + 1];
  check(distance_tiles, "the distance alphabet tiles without gap or overlap");
  bool length_tiles = true;
  for (std::size_t i = 0; i + 2 < length_base.size(); ++i)
    length_tiles = length_tiles && length_base[i] + static_cast<double>(1 << static_cast<int>(length_extra[i])) == length_base[i + 1];
  const auto penultimate = length_base.size() - 2;
  check(length_tiles && length_base[penultimate] + static_cast<double>(1 << static_cast<int>(length_extra[penultimate])) - 1.0 == 258.0 &&
            length_base[length_base.size() - 1] == 258.0,
        "the length alphabet tiles except for the final pair, which the format leaves irregular");
}

void test_compress_deflate() {
  // Every upstream round trip is verified through decompressDeflate, which is blocked (see the header), so
  // only the cases that do not inflate are ported here.
  const auto deterministic = text(repeat("deterministic output for a fixed input", 10));
  check(same(compression::compress_deflate(deterministic), compression::compress_deflate(deterministic)), "compressDeflate produces the same bytes for the same input");
  const auto untouched = text("untouched");
  const auto untouched_copy = copy_of(untouched);
  static_cast<void>(compression::compress_deflate(untouched));
  check(same(untouched, untouched_copy), "compressDeflate does not mutate the input it was given");
}

void test_compress_deflate_zlib() {
  const auto framing = text(repeat("framing is the only difference", 8));
  const auto raw_stream = compression::compress_deflate(framing);
  const auto wrapped = compression::compress_deflate_zlib(framing);
  check(wrapped.size() == raw_stream.size() + 6 && same(wrapped.subarray(2, static_cast<std::ptrdiff_t>(wrapped.size() - 4)), raw_stream),
        "compressDeflateZlib wraps the same raw stream in a header and an Adler-32 trailer");
  const auto header = compression::compress_deflate_zlib(text("header check"));
  const int cmf = header[0];
  const int flg = header[1];
  check((cmf & 0x0f) == 8 && (cmf >> 4) <= 7 && ((cmf << 8) | flg) % 31 == 0 && (flg & 0x20) == 0, "compressDeflateZlib emits a header the zlib reader accepts");
  const auto deterministic = text(repeat("deterministic zlib output", 10));
  check(same(compression::compress_deflate_zlib(deterministic), compression::compress_deflate_zlib(deterministic)), "compressDeflateZlib produces the same bytes for the same input");
}

void test_sdk_host_compress_deflate() {
  const auto& slot = compression::sdk_host_compress_deflate;
  check(static_cast<bool>(slot->compress), "sdkHostCompressDeflate is a ready-made deflate slot carrying the portable encoder");
  const auto raw_bytes = text(repeat("slot raw framing", 10));
  check(same(slot->compress(raw_bytes, raw()), compression::compress_deflate(raw_bytes)), "sdkHostCompressDeflate dispatches Raw framing to compressDeflate");
  const auto zlib_bytes = text(repeat("slot zlib framing", 10));
  check(same(slot->compress(zlib_bytes, rfc1950()), compression::compress_deflate_zlib(zlib_bytes)), "sdkHostCompressDeflate dispatches Rfc1950 framing to compressDeflateZlib");
}

void test_decompress_deflate() {
  // Only rejections decided by the zlib header and framing checks, which return before InflateState is
  // built, are genuine today. Every case that reaches raw_inflate is excluded: success cases fail, and
  // rejection cases would pass for the wrong reason (see the header).
  const auto literal = decode_base64("eJxLy8lMzyjJKFQoTk7NS9VNyy/KTSwpBgBjVQiv");
  const auto raw_literal = decode_base64("S8vJTM8oyShUKE5OzUvVTcsvyk0sKQYA");
  check(!inflate(Uint8Array{0x78, 0x9c, 0, 0, 0}, 0.0, rfc1950()).has_value() && !inflate(raw_literal, 0.0, String("Unknown")).has_value(),
        "decompressDeflate rejects undersized zlib wrappers and unknown framing values");
  auto illegal_method = copy_of(literal);
  illegal_method[0] = 0x77;
  illegal_method[1] = 0x09;
  auto illegal_window = copy_of(literal);
  illegal_window[0] = 0xf8;
  illegal_window[1] = 0x00;
  auto illegal_check = copy_of(literal);
  illegal_check[1] = static_cast<std::uint8_t>(illegal_check[1] ^ 1);
  auto preset_dictionary = copy_of(literal);
  preset_dictionary[1] = 0xbb;
  check(!inflate(illegal_method, 0.0, rfc1950()).has_value() && !inflate(illegal_window, 0.0, rfc1950()).has_value() &&
            !inflate(illegal_check, 0.0, rfc1950()).has_value() && !inflate(preset_dictionary, 0.0, rfc1950()).has_value(),
        "decompressDeflate rejects illegal zlib methods, window sizes, check bits, and preset dictionaries");
}

void test_decompress_lzma() {
  const std::string lorem = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore. ";
  const auto empty = decode_base64("XQAAgAAAAAAAAAAAAACD//v//8AAAAA=");
  const auto literal = decode_base64("XQAAgAAWAAAAAAAAAAAzGwlhGvxuQ0djOEOOegx+SF/9GpGR4mriTv//OtQAAA==");
  const auto repetitive = decode_base64("XQAAgAAYFQAAAAAAAAAwmIiVWA1cL8CWPd4EwdfDUzb4YxrC89ZwXRCrpoeyUdvTx5o45hzWZ//+5qgA");
  const auto lorem_fixture = decode_base64(
      "XQAAgAC8BAAAAAAAAAAmG8pGZ1ryd7h9hthB2wU1zYOlfBKlBduQvS8U03FylqiKfYRWcY1qIpirnj3DVe/MpcPdW46/A4EhQNYmkQJFT5KheLuKAK+QKiaSAiPlXLMt4+hcLPsyIhobwQyDdnLg9O1dkZ1vqP+nSEAA");
  const auto high_ratio = decode_base64(
      "XQAAgAAAAAEAAAAAAAAAAFJQCoT5shSEwNQfjcv62XEVMXFIzslqZUB7WJdNx+nyYL98NdWkQKtgY2m70luPToPtjk2J35Y9+g1XU+rioRtXt20WiAZxVK393ZkH6XV7zoP//30wAAA=");
  const auto binary = decode_base64(
      "XQAAgAAABAAAAAAAAAAAAFJQCoT5m7KAIalp1ifgPgZaXwSNU9QEujlXBQnBVSTenbhxWTFgoZ/5b0lz8sjqjLoaiylpIYD+M4Nmr0Zt7J6JiguD8DwOiY4/7V/nnpDZHP8y9LLgOVGy0hQVtMVxutsG43man7s4wbAArJMLqgYZAxIIFVubyEjwMi7+LaCHyPCk4NJR641nVpKyTYTF8YYx32piW8J5Ldn3PHO6dHQH2DypViIkoWb4WoRfMGfS9ktJLn8g69v4EA6UeHfHP2vvtM2V4m/2RG4GzwuCGsvbevBXjZj/kMA+5sESQXXuAyiW6xP7pyjMryzes4P///iKnQA=");
  const auto non_default = decode_base64("EgAAgACEAwAAAAAAAAAqGtnUdETvKUbQpX7kuGSBCVmJ/pkOyXrALgn2sW0227s2SUzMgfoYYCWHaBoAHVQGrWUzhTDeGD//2rVAAA==");
  const auto eos = decode_base64("XQAAgAD//////////wAzGwlhGvxuQ0djOEOOegx+SF/9GpGR4mriTv//OtQAAA==");
  const auto flight_text = text("flighthq scene-formats");

  check(same(unlzma(empty, 0.0, raw()), Uint8Array(0)), "decompressLzma round-trips empty input");
  check(same(unlzma(literal, 0.0, raw()), flight_text), "decompressLzma round-trips a short literal run");
  check(same(unlzma(repetitive, 0.0, raw()), text(repeat("abcABC123", 600))), "decompressLzma round-trips repetitive data and grows the output buffer");
  check(same(unlzma(lorem_fixture, 0.0, raw()), text(repeat(lorem, 12))), "decompressLzma round-trips prose through genuine LZ matches");
  Uint8Array sevens(64 * 1024);
  for (std::size_t i = 0; i < sevens.size(); ++i) sevens[i] = static_cast<std::uint8_t>(i % 7);
  check(same(unlzma(high_ratio, 0.0, raw()), sevens), "decompressLzma keeps decompressing a large but bounded expansion");
  Uint8Array every(1024);
  for (std::size_t i = 0; i < every.size(); ++i) every[i] = static_cast<std::uint8_t>(i % 256);
  check(same(unlzma(binary, 0.0, raw()), every), "decompressLzma round-trips binary data covering all byte values");
  check(same(unlzma(non_default, 0.0, raw()), text(repeat("The quick brown fox jumps over the lazy dog. ", 20))),
        "decompressLzma round-trips with non-default LZMA properties (lc=0, lp=2, pb=0)");
  check(same(unlzma(eos, 0.0, raw()), flight_text), "decompressLzma decompresses an EOS-terminated stream when the header declares unknown size");
  check(same(unlzma(literal, static_cast<double>(flight_text.size()), raw()), flight_text) &&
            !unlzma(literal, static_cast<double>(flight_text.size() - 1), raw()).has_value(),
        "decompressLzma stops expansion at the container-declared output bound");
  check(same(unlzma(eos, static_cast<double>(flight_text.size()), raw()), flight_text), "decompressLzma uses the caller uncompressedLength even when the header declares unknown size");
  check(!unlzma(literal.subarray(0, 15), 0.0, raw()).has_value(), "decompressLzma returns null on a truncated stream rather than throwing");
  auto corrupted = copy_of(literal);
  corrupted[20] = static_cast<std::uint8_t>(corrupted[20] ^ 0xff);
  check(!unlzma(corrupted, 0.0, raw()).has_value(), "decompressLzma returns null on a corrupt stream");
  check(!unlzma(Uint8Array(12), 0.0, raw()).has_value(), "decompressLzma returns null when the header is too short");
  auto bad_properties = copy_of(literal);
  bad_properties[0] = 225;
  check(!unlzma(bad_properties, 0.0, raw()).has_value(), "decompressLzma returns null for an invalid properties byte");
  check(!unlzma(literal, 0.0, rfc1950()).has_value() && !unlzma(literal, 0.0, String("Unknown")).has_value(), "decompressLzma returns null for unsupported framing values");
  auto bad_start = copy_of(literal);
  bad_start[13] = 0x01;
  check(!unlzma(bad_start, 0.0, raw()).has_value(), "decompressLzma returns null when the range coder start byte is not zero");
  const auto rep1 = decode_base64("XQAQAABYAAAAAAAAAAABAONpl4/4ZVy4Rhy9I1ecQB4qjQSwkD7GlIZGbwxgbka7aCCbU0OngOD6Z9oSV58DkqTY45dVqGmAHVVO7+Q1iTY=");
  const auto rep1_expected = decode_base64("AgMCBAMFBAEFAgIGAAIEAQYGBQQGBAMDAgICBQABAwAAAQMBAwQBBQEDAQEBAwAAAQMCAQEAAQAGAwAFAAADBAQEBQMDAQABAwUCBgIEBgAEBQABBAYDAg==");
  check(same(unlzma(rep1, static_cast<double>(rep1_expected.size()), raw()), rep1_expected), "decompressLzma: a rep1 match must not clobber rep2");
  const auto shuffle = decode_base64("XQAQAAA/AAAAAAAAAAAAglNJ8CRbC1Bvfw4Bn3KRgimvIEUMESap889L1RxPvXH3baBJ5QGxV6SlI8Mwg8/3Hg==");
  const auto shuffle_expected = decode_base64("AQgBAAcAAwIBAAUIAQADAAkGCQYJAAMAAQABBgEGCQAFAAEIAQgJCAcGBwAFCAcCAwQHCAMCBwgHAgMCBwIJ");
  check(same(unlzma(shuffle, static_cast<double>(shuffle_expected.size()), raw()), shuffle_expected), "decompressLzma decodes a rep-shuffle stream");
  check(same(compression::sdk_host_decompress_lzma->decompress(literal, 0.0, raw()), flight_text),
        "sdkHostDecompressLzma decompresses through the slot exactly as the bare function does");
}

void test_compress_lzma() {
  const auto round_trips = [](const Uint8Array& input) {
    return same(unlzma(compression::compress_lzma(input), static_cast<double>(input.size()), raw()), input);
  };
  check(same(unlzma(compression::compress_lzma(Uint8Array(0)), 0.0, raw()), Uint8Array(0)), "compressLzma round-trips empty input");
  check(round_trips(text("Hello")), "compressLzma round-trips a short literal string");
  Uint8Array mod27(5400);
  for (std::size_t i = 0; i < mod27.size(); ++i) mod27[i] = static_cast<std::uint8_t>(i % 27);
  check(round_trips(mod27) && compression::compress_lzma(mod27).size() < mod27.size(), "compressLzma round-trips repetitive data");
  const auto prose = text(repeat(
      "The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor jugs. How vexingly quick daft zebras jump. The five boxing wizards jump quickly. ", 5));
  check(round_trips(prose) && compression::compress_lzma(prose).size() < prose.size(), "compressLzma round-trips prose text");
  Uint8Array every(256);
  for (std::size_t i = 0; i < 256; ++i) every[i] = static_cast<std::uint8_t>(i);
  check(round_trips(every), "compressLzma round-trips all byte values");
  Uint8Array mod100(65536);
  for (std::size_t i = 0; i < mod100.size(); ++i) mod100[i] = static_cast<std::uint8_t>(i % 100);
  check(round_trips(mod100) && compression::compress_lzma(mod100).size() < mod100.size() / 2, "compressLzma round-trips a large repetitive block");
  check(round_trips(Uint8Array{42}), "compressLzma round-trips single-byte input");
  check(round_trips(Uint8Array{0xab, 0xcd}), "compressLzma round-trips two-byte input");
  Uint8Array runs(1000);
  for (std::size_t i = 0; i < runs.size(); ++i) runs[i] = 0x55;
  check(round_trips(runs) && compression::compress_lzma(runs).size() < 100, "compressLzma round-trips data with long repeated runs");
  const auto header_input = text("header check");
  const auto header = compression::compress_lzma(header_input);
  const std::uint32_t size = static_cast<std::uint32_t>(header[5]) | (static_cast<std::uint32_t>(header[6]) << 8) | (static_cast<std::uint32_t>(header[7]) << 16) |
                             (static_cast<std::uint32_t>(header[8]) << 24);
  check(header.size() >= 13 && header[0] <= 224 && size == header_input.size(), "compressLzma writes a valid LZMA alone header");
  Uint8Array mod10(4096);
  for (std::size_t i = 0; i < mod10.size(); ++i) mod10[i] = static_cast<std::uint8_t>(i % 10);
  check(compression::compress_lzma(mod10).size() < mod10.size(), "compressLzma produces compressed output smaller than input for compressible data");
  const char* tokens[] = {"ab", "cd", "ef", "gh", "ij", "kl", "mn", "op"};
  bool seeds_ok = true;
  for (int seed = 0; seed < 500; ++seed) {
    std::string joined;
    int s = seed;
    const int length = 30 + (seed % 60);
    for (int i = 0; i < length; ++i) {
      if (i > 0 && seed % 3 == 0) joined += ' ';
      joined += tokens[s % 8];
      s = (s * 31 + 7) & 0xffff;
    }
    seeds_ok = seeds_ok && round_trips(text(joined));
  }
  check(seeds_ok, "compressLzma round-trips token-shaped small-alphabet inputs across many seeds");
  check(round_trips(text(repeat("ALPHABETA_GAMMA", 4) + repeat("BETA_ALPHAGAMMA", 4) + repeat("GAMMABETA_ALPHA", 4))),
        "compressLzma round-trips alternating match distances that force rep shuffles");
  const auto header_only = text("header size only");
  check(same(unlzma(compression::compress_lzma(header_only), 0.0, raw()), header_only), "compressLzma: decompresses with uncompressedLength=0 (size from header)");
  const auto capability = text("capability test");
  check(same(unlzma(compression::sdk_host_compress_lzma->compress(capability, raw()), static_cast<double>(capability.size()), raw()), capability),
        "sdkHostCompressLzma compresses with Raw framing");
  bool rejected = false;
  try {
    static_cast<void>(compression::sdk_host_compress_lzma->compress(Uint8Array{1, 2, 3}, rfc1950()));
  } catch (const flight::Error& error) {
    rejected = error.message() == String("lzma: only Raw framing is supported");
  }
  check(rejected, "sdkHostCompressLzma rejects non-Raw framing with the documented message");
}

}  // namespace

int main() {
  test_deflate_format();
  test_compress_deflate();
  test_compress_deflate_zlib();
  test_sdk_host_compress_deflate();
  test_decompress_deflate();
  test_decompress_lzma();
  test_compress_lzma();
  return failures == 0 ? 0 : 1;
}
