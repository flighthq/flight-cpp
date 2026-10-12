#include <flight/compression/contract.hpp>

#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// Native port of packages/compression/src/{deflateFormat,compress,deflate,lzma,lzmaCompress}.test.ts
// (a627849) against the generated headers (no override). The compressed fixtures are the upstream ones,
// precomputed by Node's zlib and Python's lzma and embedded as base64, so they are independent oracles.
// The fixture builders below (base64, hex, the DEFLATE bit packers) are ported from the test file itself.
//
// The deflate cases depend on source-patches/compiler-constructor-parameter-property-assignment.patch.
// Without it, InflateState's parameter properties are never assigned and decompressDeflate returns null for
// every input (docs/compiler-requests/constructor-parameter-properties-dropped.md).
//
// Not ported: 'sdkHostDecompressDeflate is a ready-made deflate slot' and its LZMA twin, which assert
// `slot.decompress` is the SAME function object as decompressDeflate. std::function has no identity to
// compare; the behavioural twin ('... exactly as the bare function does') is ported for both.
//
// MUTATION SENSITIVITY, measured 2026-10-12: these fixtures do not merely pass, they catch the regression
// they were written for. Two mutations were applied to a SHADOW COPY of the generated
// flight/compression/lzma.hpp (the generated tree was never touched, and the baseline passes):
//   * the short-rep path taking rep3 instead of rep2 -- three checks fail, including both fixtures the
//     upstream LZMA fix added to lzma.test.ts at the pinned commit;
//   * rep3 clobbered on the rep1 path, which is the pre-fix shape those fixtures guard -- the fixture
//     'a rep1 match must not clobber rep2' fails, and it is the only one.
// Package-scoped regeneration at a627849 reports 7/7 modules emitted, 0 refused and 0 of 7 headers
// changed, so the committed slice is current and already carries the fix.

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

Uint8Array decode_hex(const std::string& input) {
  Uint8Array out(input.size() / 2);
  for (std::size_t i = 0; i < out.size(); ++i) out[i] = static_cast<std::uint8_t>(std::stoi(input.substr(i * 2, 2), nullptr, 16));
  return out;
}

// --- the DEFLATE bit-stream builders from deflate.test.ts ---

Uint8Array pack_deflate_bits(const std::vector<int>& bits) {
  Uint8Array out((bits.size() + 7) / 8);
  for (std::size_t i = 0; i < bits.size(); ++i) out[i >> 3] = static_cast<std::uint8_t>(out[i >> 3] | (bits[i] << (i & 7)));
  return out;
}

void push_lsb_value(std::vector<int>& bits, int value, int count) {
  for (int i = 0; i < count; ++i) bits.push_back((value >> i) & 1);
}

void push_canonical_code(std::vector<int>& bits, int code, int count) {
  for (int i = count - 1; i >= 0; --i) bits.push_back((code >> i) & 1);
}

void push_fixed_literal_code(std::vector<int>& bits, int symbol) {
  if (symbol <= 143) push_canonical_code(bits, 0x30 + symbol, 8);
  else if (symbol <= 255) push_canonical_code(bits, 0x190 + symbol - 144, 9);
  else if (symbol <= 279) push_canonical_code(bits, symbol - 256, 7);
  else push_canonical_code(bits, 0xc0 + symbol - 280, 8);
}

Uint8Array create_fixed_back_reference_stream(int length_symbol, std::optional<int> distance_symbol = std::nullopt) {
  std::vector<int> bits;
  push_lsb_value(bits, 1, 1);
  push_lsb_value(bits, 1, 2);
  push_fixed_literal_code(bits, length_symbol);
  if (distance_symbol.has_value()) push_canonical_code(bits, *distance_symbol, 5);
  return pack_deflate_bits(bits);
}

const int code_length_order[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

void push_dynamic_header(std::vector<int>& bits, int literal_count, int distance_count, const std::map<int, int>& code_lengths) {
  const auto length_of = [&](int symbol) {
    const auto found = code_lengths.find(symbol);
    return found == code_lengths.end() ? 0 : found->second;
  };
  push_lsb_value(bits, 1, 1);
  push_lsb_value(bits, 2, 2);
  push_lsb_value(bits, literal_count - 257, 5);
  push_lsb_value(bits, distance_count - 1, 5);
  int code_length_count = 4;
  for (int i = 0; i < 19; ++i)
    if (length_of(code_length_order[i]) != 0) code_length_count = i + 1;
  push_lsb_value(bits, code_length_count - 4, 4);
  for (int i = 0; i < code_length_count; ++i) push_lsb_value(bits, length_of(code_length_order[i]), 3);
}

Uint8Array create_dynamic_repeat_overflow_stream(int symbol) {
  std::vector<int> bits;
  push_dynamic_header(bits, 257, 1, {{0, 1}, {1, 2}, {symbol, 2}});
  for (int i = 0; i < 256; ++i) push_canonical_code(bits, 0, 1);
  push_canonical_code(bits, 2, 2);
  push_canonical_code(bits, 3, 2);
  push_lsb_value(bits, 0, symbol == 16 ? 2 : symbol == 17 ? 3 : 7);
  push_canonical_code(bits, 0, 1);
  return pack_deflate_bits(bits);
}

Uint8Array create_valid_dynamic_repeat_stream(int symbol) {
  std::vector<int> bits;
  if (symbol == 16) {
    push_dynamic_header(bits, 257, 1, {{0, 1}, {2, 2}, {16, 2}});
    for (int i = 0; i < 254; ++i) push_canonical_code(bits, 0, 1);
    push_canonical_code(bits, 2, 2);
    push_canonical_code(bits, 3, 2);
    push_lsb_value(bits, 0, 2);
    push_canonical_code(bits, 2, 2);
  } else {
    push_dynamic_header(bits, 257, 1, {{0, 1}, {1, 2}, {symbol, 2}});
    push_canonical_code(bits, 3, 2);
    push_lsb_value(bits, 0, symbol == 17 ? 3 : 7);
    const int repeat_count = symbol == 17 ? 3 : 11;
    for (int i = repeat_count; i < 256; ++i) push_canonical_code(bits, 0, 1);
    push_canonical_code(bits, 2, 2);
    push_canonical_code(bits, 0, 1);
    push_canonical_code(bits, 0, 1);
  }
  return pack_deflate_bits(bits);
}

Uint8Array create_reserved_dynamic_distance_stream() {
  std::vector<int> bits;
  push_dynamic_header(bits, 258, 31, {{0, 1}, {1, 1}});
  for (int i = 0; i < 256; ++i) push_canonical_code(bits, 0, 1);
  push_canonical_code(bits, 1, 1);
  push_canonical_code(bits, 1, 1);
  for (int i = 0; i < 30; ++i) push_canonical_code(bits, 0, 1);
  push_canonical_code(bits, 1, 1);
  push_canonical_code(bits, 1, 1);
  push_canonical_code(bits, 0, 1);
  return pack_deflate_bits(bits);
}

// xorshift32 with the JavaScript int32 semantics the upstream test relies on: the same bits in uint32.
struct XorShift {
  std::uint32_t seed;
  std::uint32_t next() {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
  }
};

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
  check(same(inflate(compression::compress_deflate(Uint8Array(0)), 0.0, raw()), Uint8Array(0)), "compressDeflate round-trips an empty input as a raw stream");
  const auto fox = text("the quick brown fox jumps over the lazy dog");
  check(same(inflate(compression::compress_deflate(fox), static_cast<double>(fox.size()), raw()), fox), "compressDeflate round-trips text through the raw framing");
  const auto abc = text(repeat("abcabcabc", 400));
  const auto abc_compressed = compression::compress_deflate(abc);
  check(same(inflate(abc_compressed, static_cast<double>(abc.size()), raw()), abc) && abc_compressed.size() < abc.size() / 4,
        "compressDeflate round-trips highly repetitive text and actually shrinks it");
  Uint8Array noise(4096);
  XorShift noise_seed{0x2545f491u};
  for (std::size_t i = 0; i < noise.size(); ++i) noise[i] = static_cast<std::uint8_t>(noise_seed.next() & 0xff);
  const auto noise_compressed = compression::compress_deflate(noise);
  check(same(inflate(noise_compressed, static_cast<double>(noise.size()), raw()), noise) && noise_compressed.size() <= noise.size() + 16,
        "compressDeflate round-trips incompressible binary without expanding it much");
  Uint8Array long_input(70000);
  for (std::size_t i = 0; i < long_input.size(); ++i) long_input[i] = static_cast<std::uint8_t>(i & 0xff);
  check(same(inflate(compression::compress_deflate(long_input), static_cast<double>(long_input.size()), raw()), long_input),
        "compressDeflate round-trips an input longer than one stored block can carry");
  Uint8Array every(256);
  for (std::size_t i = 0; i < 256; ++i) every[i] = static_cast<std::uint8_t>(i);
  check(same(inflate(compression::compress_deflate(every), 256.0, raw()), every), "compressDeflate round-trips every byte value");
  XorShift sweep{0x9e3779b9u};
  bool sweep_ok = true;
  for (int length = 0; length < 300; ++length) {
    Uint8Array bytes(static_cast<std::size_t>(length));
    const std::uint32_t alphabet = 1 + static_cast<std::uint32_t>(length % 7);
    for (int i = 0; i < length; ++i) bytes[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(sweep.next() % alphabet);
    sweep_ok = sweep_ok && same(inflate(compression::compress_deflate(bytes), length, raw()), bytes) &&
               same(inflate(compression::compress_deflate_zlib(bytes), length, rfc1950()), bytes);
  }
  check(sweep_ok, "compressDeflate round-trips a deterministic sweep of shapes a fixed corpus would miss");
  Uint8Array sevens(2048);
  for (std::size_t i = 0; i < sevens.size(); ++i) sevens[i] = 7;
  check(same(inflate(compression::compress_deflate(sevens), 2048.0, raw()), sevens), "compressDeflate round-trips a long run that exercises the maximum match length");
  Uint8Array window(40000);
  for (std::size_t i = 0; i < window.size(); ++i) window[i] = static_cast<std::uint8_t>((i * 31) & 0xff);
  window.set(window.subarray(0, 64), static_cast<std::ptrdiff_t>(window.size() - 64));
  check(same(inflate(compression::compress_deflate(window), static_cast<double>(window.size()), raw()), window),
        "compressDeflate round-trips a repeat at the far edge of the window");
  const auto deterministic = text(repeat("deterministic output for a fixed input", 10));
  check(same(compression::compress_deflate(deterministic), compression::compress_deflate(deterministic)), "compressDeflate produces the same bytes for the same input");
  const auto raw_means_raw = text(repeat("raw means raw", 20));
  check(!inflate(compression::compress_deflate(raw_means_raw), static_cast<double>(raw_means_raw.size()), rfc1950()).has_value(),
        "compressDeflate emits no zlib wrapper, so the zlib reader refuses it");
  const auto untouched = text("untouched");
  const auto untouched_copy = copy_of(untouched);
  static_cast<void>(compression::compress_deflate(untouched));
  check(same(untouched, untouched_copy), "compressDeflate does not mutate the input it was given");
}

void test_compress_deflate_zlib() {
  check(same(inflate(compression::compress_deflate_zlib(Uint8Array(0)), 0.0, rfc1950()), Uint8Array(0)), "compressDeflateZlib round-trips an empty input");
  const auto fox = text("the quick brown fox jumps over the lazy dog");
  check(same(inflate(compression::compress_deflate_zlib(fox), static_cast<double>(fox.size()), rfc1950()), fox), "compressDeflateZlib round-trips text");
  Uint8Array binary(1024);
  for (std::size_t i = 0; i < binary.size(); ++i) binary[i] = static_cast<std::uint8_t>((i * 37) & 0xff);
  check(same(inflate(compression::compress_deflate_zlib(binary), 1024.0, rfc1950()), binary), "compressDeflateZlib round-trips binary bytes");
  const auto framing = text(repeat("framing is the only difference", 8));
  const auto raw_stream = compression::compress_deflate(framing);
  const auto wrapped = compression::compress_deflate_zlib(framing);
  check(wrapped.size() == raw_stream.size() + 6 && same(wrapped.subarray(2, static_cast<std::ptrdiff_t>(wrapped.size() - 4)), raw_stream),
        "compressDeflateZlib wraps the same raw stream in a header and an Adler-32 trailer");
  const auto header = compression::compress_deflate_zlib(text("header check"));
  const int cmf = header[0];
  const int flg = header[1];
  check((cmf & 0x0f) == 8 && (cmf >> 4) <= 7 && ((cmf << 8) | flg) % 31 == 0 && (flg & 0x20) == 0, "compressDeflateZlib emits a header the zlib reader accepts");
  const auto checksum_me = text(repeat("checksum me", 30));
  const auto with_trailer = compression::compress_deflate_zlib(checksum_me);
  check(same(inflate(with_trailer, static_cast<double>(checksum_me.size()), rfc1950()), checksum_me) &&
            !same(with_trailer.subarray(static_cast<std::ptrdiff_t>(with_trailer.size() - 4)), Uint8Array{0, 0, 0, 0}),
        "compressDeflateZlib emits a non-zero Adler-32 trailer the reader verifies");
  const auto zlib_means_zlib = text(repeat("zlib means zlib", 20));
  const auto as_raw = inflate(compression::compress_deflate_zlib(zlib_means_zlib), static_cast<double>(zlib_means_zlib.size()), raw());
  check(!same(as_raw, zlib_means_zlib), "compressDeflateZlib emits a wrapper, so the raw reader does not read it back unchanged");
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
  const auto round = text(repeat("round trip through the slot", 8));
  check(same(inflate(slot->compress(round, raw()), static_cast<double>(round.size()), raw()), round) &&
            same(inflate(slot->compress(round, rfc1950()), static_cast<double>(round.size()), rfc1950()), round),
        "sdkHostCompressDeflate round-trips through decompressDeflate for both framings");
}

void test_decompress_deflate() {
  const std::string lorem = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore. ";
  const auto empty = decode_base64("eJwDAAAAAAE=");
  const auto high_ratio = decode_base64(
      "eJztxbEBADAEADC0/H+yQyRLIuv9npAkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIkSZIk3W4BATsAKQ==");
  const auto literal = decode_base64("eJxLy8lMzyjJKFQoTk7NS9VNyy/KTSwpBgBjVQiv");
  const auto raw_literal = decode_base64("S8vJTM8oyShUKE5OzUvVTcsvyk0sKQYA");
  const auto repetitive = decode_base64("eJztxjEBACAIALBMYgIxCdC/g0HcrlXPybtil4iIiIiIiIiIiMgfeU6Y4Pw=");
  const auto lorem_fixture = decode_base64(
      "eJztzdENQyEMQ9FVPEDVSd4SlESVJUIQSfYvQ/STb+v6PL7VwBVlEB++EUw003yh+wztqVkbTbgYnfMLHTxjqJwAygpzQaqtE3N2CqVmohKjfc79G89FLnKRi/wX+QG1gr9k");
  const auto stored = decode_base64("eAEBKwDU/3RoZSBxdWljayBicm93biBmb3gganVtcHMgb3ZlciB0aGUgbGF6eSBkb2dhPA/6");

  check(same(inflate(empty, 0.0, rfc1950()), Uint8Array(0)), "decompressDeflate round-trips empty input");
  Uint8Array sevens(64 * 1024);
  for (std::size_t i = 0; i < sevens.size(); ++i) sevens[i] = static_cast<std::uint8_t>(i % 7);
  check(same(inflate(high_ratio, 0.0, rfc1950()), sevens), "decompressDeflate keeps inflating a stream whose expansion ratio is large but bounded");
  const auto bounded = inflate(high_ratio, 64 * 1024, rfc1950());
  check(bounded.has_value() && bounded->size() == 64 * 1024 && !inflate(high_ratio, 64 * 1024 - 1, rfc1950()).has_value(),
        "decompressDeflate stops expansion at the container-declared output bound");
  check(same(inflate(literal, 0.0, rfc1950()), text("flighthq scene-formats")), "decompressDeflate round-trips a short literal run (zlib-wrapped)");
  check(same(inflate(raw_literal, 0.0, raw()), text("flighthq scene-formats")), "decompressDeflate round-trips a headerless raw stream when requested");
  Uint8Array payload(156);
  for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<std::uint8_t>(i);
  Uint8Array stream(1 + 4 + payload.size() + 5);
  stream.set(Uint8Array{0x78, 0x9c, 0x00, 0x63, 0xff});
  stream.set(payload, 5);
  stream.set(Uint8Array{0x01, 0x00, 0x00, 0xff, 0xff}, static_cast<std::ptrdiff_t>(5 + payload.size()));
  check(same(inflate(stream, static_cast<double>(payload.size()), raw()), payload) && !inflate(stream, static_cast<double>(payload.size()), rfc1950()).has_value(),
        "decompressDeflate does not mistake a valid raw stream beginning 78 9c for zlib framing");
  check(same(inflate(repetitive, 0.0, rfc1950()), text(repeat("abcABC123", 600))), "decompressDeflate round-trips repetitive data and grows the output buffer");
  check(same(inflate(lorem_fixture, 0.0, rfc1950()), text(repeat(lorem, 12))), "decompressDeflate round-trips prose through a dynamic-Huffman block");
  check(same(inflate(stored, 0.0, rfc1950()), text("the quick brown fox jumps over the lazy dog")), "decompressDeflate round-trips a stored (level 0) block");
  check(!inflate(repetitive.subarray(0, 6), 0.0, rfc1950()).has_value(), "decompressDeflate returns null on a truncated stream rather than throwing");
  check(!inflate(Uint8Array{0x78, 0x9c, 0xff, 0xff, 0, 0, 0, 0}, 0.0, rfc1950()).has_value(), "decompressDeflate returns null on an invalid block type");
  check(!inflate(Uint8Array{0x78, 0x9c, 0, 0, 0}, 0.0, rfc1950()).has_value() && !inflate(raw_literal, 0.0, String("Unknown")).has_value(),
        "decompressDeflate rejects undersized zlib wrappers and unknown framing values");
  check(!inflate(Uint8Array{0x01, 0, 0}, 0.0, raw()).has_value() && !inflate(Uint8Array{0x01, 0x02, 0, 0xfd, 0xff, 0x41}, 0.0, raw()).has_value(),
        "decompressDeflate rejects truncated stored-block headers and payloads");
  check(!inflate(create_fixed_back_reference_stream(286), 0.0, raw()).has_value() && !inflate(create_fixed_back_reference_stream(257, 0), 0.0, raw()).has_value(),
        "decompressDeflate rejects reserved fixed length symbols and back-references before the output");
  check(!inflate(create_reserved_dynamic_distance_stream(), 0.0, raw()).has_value(), "decompressDeflate rejects a reserved dynamic distance symbol");
  auto corrupt = copy_of(literal);
  corrupt[corrupt.size() - 1] = static_cast<std::uint8_t>(corrupt[corrupt.size() - 1] ^ 1);
  check(!inflate(literal.subarray(0, static_cast<std::ptrdiff_t>(literal.size() - 4)), 0.0, rfc1950()).has_value() && !inflate(corrupt, 0.0, rfc1950()).has_value(),
        "decompressDeflate rejects a missing or corrupt zlib Adler-32 trailer");
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
  check(same(inflate(decode_hex("05c0050900000000a0ffaf15"), 0.0, raw()), Uint8Array()) && !inflate(decode_hex("05c0050900000000a0ffaf0d"), 0.0, raw()).has_value(),
        "decompressDeflate rejects a dynamic code-length repeat that exceeds the declared table");
  for (const int symbol : {16, 17, 18}) {
    check(!inflate(create_dynamic_repeat_overflow_stream(symbol), 0.0, raw()).has_value(),
          "decompressDeflate rejects dynamic repeat symbol " + std::to_string(symbol) + " when it exceeds the declared table");
  }
  bool valid_repeats = true;
  for (const int symbol : {16, 17, 18}) valid_repeats = valid_repeats && same(inflate(create_valid_dynamic_repeat_stream(symbol), 0.0, raw()), Uint8Array());
  check(valid_repeats, "decompressDeflate accepts every dynamic repeat form when it ends inside the declared table");
  std::vector<int> bits;
  push_dynamic_header(bits, 257, 1, {{0, 1}, {1, 2}, {16, 2}});
  push_canonical_code(bits, 3, 2);
  check(!inflate(pack_deflate_bits(bits), 0.0, raw()).has_value(), "decompressDeflate rejects a previous-length repeat before any length has been declared");
  check(same(inflate(decode_hex("edc0210900000000a0ffaf5d22"), 0.0, raw()), Uint8Array()) && !inflate(decode_hex("f5c0210900000000a0ffaf7d22"), 0.0, raw()).has_value() &&
            !inflate(decode_hex("fdc0210900000000a0ffaf9d22"), 0.0, raw()).has_value(),
        "decompressDeflate rejects forbidden dynamic literal counts without rejecting the maximum valid count");
  check(same(compression::sdk_host_decompress_deflate->decompress(literal, 0.0, rfc1950()), text("flighthq scene-formats")),
        "sdkHostDecompressDeflate inflates through the slot exactly as the bare function does");
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
