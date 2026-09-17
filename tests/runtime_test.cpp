#include <flight/runtime.hpp>
#include <flight/host/console.hpp>
#include <flight/host/performance.hpp>
#include <flight/host/timers.hpp>

#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

namespace {

int failures = 0;

class TestUnicodeService final : public flight::UnicodeService {
 public:
  [[nodiscard]] std::u16string lower(std::u16string_view value) const override {
    return value == u"\u00C4" ? u"\u00E4" : std::u16string(value);
  }

  [[nodiscard]] std::u16string upper(std::u16string_view value) const override {
    return value == u"\u00E4" ? u"\u00C4" : std::u16string(value);
  }
};

struct TestReference : public flight::ReferenceEnabled {
  int value;
};

struct TestEntity : public flight::ReferenceEnabled {
  std::optional<flight::Ref<TestReference>> entity_runtime_key;
};

struct TestImageFacet final {};
struct TestMenuFacet final {};
struct TestTrayCapabilities {
  bool image;
  bool menu;
};
struct TestImageHost {
  TestTrayCapabilities tray;
};
struct TestOptionalTrayHost {
  std::optional<TestTrayCapabilities> tray;
};

using TestImagePath = flight::MemberPath<
    []<typename Value>(Value& value) -> decltype((value.tray)) { return value.tray; },
    []<typename Value>(Value& value) -> decltype((value.image)) { return value.image; }>;
using TestMenuPath = flight::MemberPath<
    []<typename Value>(Value& value) -> decltype((value.tray)) { return value.tray; },
    []<typename Value>(Value& value) -> decltype((value.menu)) { return value.menu; }>;
using TestImageRule = flight::RequiredMemberFacet<TestImageFacet, TestImagePath>;
using TestMenuRule = flight::RequiredMemberFacet<TestMenuFacet, TestMenuPath>;
using TestImageCapabilities =
    flight::ConditionalFacetRef<TestReference, TestImageHost, TestImageRule, TestMenuRule>;
using TestOptionalCapabilities =
    flight::ConditionalFacetRef<TestReference, TestOptionalTrayHost, TestImageRule>;

static_assert(std::convertible_to<TestImageCapabilities, flight::FacetRef<TestReference, TestImageFacet>>);
static_assert(std::convertible_to<TestImageCapabilities, flight::FacetRef<TestReference, TestMenuFacet>>);
static_assert(!std::convertible_to<TestOptionalCapabilities,
                                   flight::FacetRef<TestReference, TestImageFacet>>);
static_assert(sizeof(flight::FacetRef<TestReference, TestImageFacet>) ==
              sizeof(flight::Ref<TestReference>));

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

FlightTask<int> doubled(FlightTask<int> input) {
  co_return (co_await input) * 2;
}

FlightTask<int> fails() {
  throw std::runtime_error("expected failure");
  co_return 0;
}

FlightTask<int> reuse(FlightTask<int> input) {
  const auto first = co_await input;
  const auto second = co_await input;
  co_return first + second;
}

FlightTask<void> set_flag(bool& flag) {
  flag = true;
  co_return;
}

void test_array() {
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  flight::Array<double> values{nan, -0.0};
  auto alias = values;
  check(alias.push(4.0) == 3, "push returns the new array length");
  check(values.size() == 3, "array copies retain reference identity");
  check(values.length == 3, "array length property follows shared storage");
  check(values.includes(nan), "array includes uses SameValueZero for NaN");
  check(values.index_of(-0.0) == 1, "array index_of finds signed zero");

  auto independent = values.clone();
  independent.push(8.0);
  check(values.size() == 3 && independent.size() == 4, "clone creates independent storage");
  check(!values.at(20).has_value(), "out-of-range array access is absent");
  const auto captured_alias = values;
  captured_alias[0] = 2.0;
  captured_alias.push(6.0);
  check(values[0] == 2.0 && values.size() == 4,
        "const array handles retain mutable JavaScript referent semantics");
  captured_alias[0] = nan;
  static_cast<void>(values.pop());

  const auto mapped = values.map([](double value) { return std::isnan(value) ? 0.0 : value + 1.0; });
  check(mapped.size() == 3 && mapped[2] == 5.0, "array map creates transformed storage");
  const auto filtered = mapped.filter([](double value) { return value > 0.0; });
  check(filtered.size() == 2, "array filter preserves matching values");
  check(mapped.every([](double value, double index) { return value >= index; }),
        "array callbacks receive TypeScript numeric indexes");
  check(mapped.find_index([](double value) { return value == 5.0; }) == 2,
        "array find_index returns a numeric position sentinel");
  check(mapped.reduce([](double total, double value) { return total + value; }, 0.0) == 6.0,
        "array reduce preserves accumulator order");

  const flight::Set<double> iterable{3.0, 1.0, 4.0};
  const auto copied = flight::array_from(iterable);
  const auto transformed = flight::array_from(iterable, [](double value, double index) {
    return value + index;
  });
  check(copied.size() == 3 && copied[0] == 3.0 && copied[2] == 4.0 &&
            transformed.size() == 3 && transformed[0] == 3.0 && transformed[2] == 6.0,
        "Array.from preserves iterable order and supplies numeric mapper indexes");
  const auto tail = mapped.slice(-2);
  check(tail.size() == 2 && tail[0] == 1.0, "array slice normalizes negative boundaries");
  check(mapped.join(flight::String("|")) == flight::String("0|1|5"),
        "array join returns the semantic string type");
  check(flight::Array<double>{1.0e-6, 1.0e21}.join(flight::String(",")) ==
            flight::String("0.000001,1e+21"),
        "array join uses source numeric formatting");
  flight::Array<double> spliced{1.0, 4.0};
  const auto removed = spliced.splice(1, 0, 2.0, 3.0);
  check(removed.empty() && spliced.size() == 4 && spliced[1] == 2.0 && spliced[2] == 3.0,
        "array splice inserts forwarded values in source order");

  flight::Array<flight::String> live_values{"first"};
  auto entries = live_values.entries();
  auto entries_alias = entries;
  const auto first_entry = entries.next();
  live_values.push("second");
  const auto second_entry = entries_alias.next();
  const auto exhausted = entries.next();
  live_values.push("late");
  const auto still_exhausted = entries_alias.next();
  check(first_entry.value == std::optional(std::tuple(0.0, flight::String("first"))) &&
            second_entry.value == std::optional(std::tuple(1.0, flight::String("second"))) &&
            exhausted.done && still_exhausted.done,
        "array entry iterator copies share a live cursor that stays exhausted after done");

  auto keys = live_values.keys();
  auto array_values = live_values.values();
  const auto first_key = keys.next();
  const auto first_value = array_values.next();
  check(first_key.value == std::optional(0.0) &&
            first_value.value == std::optional(flight::String("first")),
        "array key and value iterators preserve their source domains");
}

void test_binary_data() {
  flight::ArrayBuffer buffer(16.0);
  flight::Uint8Array bytes(buffer);
  auto alias = bytes;
  auto tail = bytes.subarray(8);
  bytes[8] = 42;
  check(alias.buffer == buffer && tail.buffer == buffer && tail.byte_offset == 8 && tail.byte_length == 8 &&
            tail[0] == 42,
        "typed arrays retain shared ArrayBuffer storage and byte offsets");

  const flight::Uint8Array little_endian{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x3F};
  const flight::Uint8Array big_endian{0x3F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  check(flight::DataView(little_endian.buffer).get_float64(0.0, true) == 1.5 &&
            flight::DataView(big_endian.buffer).get_float64(0.0, false) == 1.5,
        "DataView decodes unaligned-independent float64 values in both byte orders");

  const flight::Uint8Array infinity_bytes{0x7F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  const flight::Uint8Array nan_bytes{0x7F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  check(std::isinf(flight::DataView(infinity_bytes.buffer).get_float64(0.0)) &&
            std::isnan(flight::DataView(nan_bytes.buffer).get_float64(0.0)),
        "DataView preserves IEEE infinity and NaN payload classes");

  flight::DataView writable(buffer);
  writable.set_uint32(0.0, 3735928559.0, true);
  writable.set_int16(4.0, -2.0, false);
  writable.set_float32(8.0, 1.25, true);
  check(writable.get_uint32(0.0, true) == 3735928559.0 && writable.get_int16(4.0) == -2.0 &&
            writable.get_float32(8.0, true) == 1.25,
        "DataView numeric setters share storage and preserve integer and float bit patterns");
  const auto captured_view = writable;
  captured_view.set_uint8(6.0, 0xA5);
  const auto captured_bytes = bytes;
  captured_bytes[7] = 0x5A;
  check(writable.get_uint8(6.0) == 0xA5 && bytes[7] == 0x5A,
        "const binary-view handles retain mutable JavaScript referent semantics");

  bool bounds_failed = false;
  try {
    static_cast<void>(flight::DataView(buffer, 9.0).get_float64(0.0));
  } catch (const std::range_error&) {
    bounds_failed = true;
  }
  check(bounds_failed, "DataView rejects reads beyond its declared view");
}

void test_audio_buffer() {
  const flight::AudioBuffer buffer({
      .length = 4.0,
      .number_of_channels = 2.0,
      .sample_rate = 48'000.0,
  });
  check(buffer.length == 4.0 && buffer.number_of_channels == 2.0 &&
            buffer.sample_rate == 48'000.0 && buffer.duration == 4.0 / 48'000.0,
        "AudioBuffer exposes decoded PCM dimensions and duration");
  const flight::AudioBuffer mono({.length = 1.0, .sample_rate = 48'000.0});
  check(mono.number_of_channels == 1.0,
        "AudioBuffer defaults an omitted numberOfChannels option to one");

  const auto first_channel = buffer.get_channel_data(0.0);
  const auto second_channel = buffer.get_channel_data(1.0);
  check(first_channel != second_channel && first_channel.size() == 4 &&
            second_channel.size() == 4,
        "AudioBuffer allocates an independent live typed-array view for each channel");

  buffer.copy_to_channel(flight::Float32Array{0.25F, -0.5F, 0.75F}, 1.0, 1.0);
  const auto second_alias = buffer.get_channel_data(1.0);
  check(second_alias == second_channel && second_alias[0] == 0.0F &&
            second_alias[1] == 0.25F && second_alias[2] == -0.5F &&
            second_alias[3] == 0.75F,
        "AudioBuffer copyToChannel truncates to its range and updates the stable live view");

  flight::Float32Array destination{9.0F, 9.0F, 9.0F};
  buffer.copy_from_channel(destination, 1.0, 2.0);
  check(destination[0] == -0.5F && destination[1] == 0.75F && destination[2] == 9.0F,
        "AudioBuffer copyFromChannel copies available samples without overwriting the tail");

  const auto alias = buffer;
  alias.get_channel_data(0.0)[0] = 1.0F;
  check(alias == buffer && alias.identity() == buffer.identity() &&
            buffer.get_channel_data(0.0)[0] == 1.0F,
        "AudioBuffer copies retain object identity and shared channel storage");

  bool invalid_channel_failed = false;
  bool invalid_offset_failed = false;
  bool invalid_options_failed = false;
  try {
    static_cast<void>(buffer.get_channel_data(2.0));
  } catch (const std::range_error&) {
    invalid_channel_failed = true;
  }
  try {
    buffer.copy_to_channel(flight::Float32Array{1.0F}, 0.0, 5.0);
  } catch (const std::range_error&) {
    invalid_offset_failed = true;
  }
  try {
    static_cast<void>(flight::AudioBuffer({
        .length = 0.0,
        .number_of_channels = 1.0,
        .sample_rate = 48'000.0,
    }));
  } catch (const std::range_error&) {
    invalid_options_failed = true;
  }
  check(invalid_channel_failed && invalid_offset_failed && invalid_options_failed,
        "AudioBuffer rejects invalid channels, offsets, and construction options");
}

void test_blob() {
  const flight::Blob text_blob(
      flight::Array<flight::String>{"Flight ", flight::String::from_utf8("\xF0\x9F\x98\x80")},
      {.type = "TEXT/PLAIN"});
  const auto text_alias = text_blob;
  check(text_blob.size == 11 && text_blob.type == flight::String("text/plain") &&
            text_blob.identity() == text_alias.identity() &&
            text_blob.text().get() == flight::String::from_utf8("Flight \xF0\x9F\x98\x80"),
        "Blob joins UTF-8 string parts, normalizes MIME types, and preserves identity");

  const flight::Blob binary_blob(
      flight::Array<flight::Uint8Array>{flight::Uint8Array{0x00, 0x7F, 0xFF}});
  const auto binary_buffer = binary_blob.array_buffer().get();
  const flight::Uint8Array binary_bytes(binary_buffer);
  check(binary_bytes.size() == 3 && binary_bytes[0] == 0x00 && binary_bytes[1] == 0x7F &&
            binary_bytes[2] == 0xFF,
        "Blob copies typed-array bytes into an independently owned ArrayBuffer");

  const auto middle = text_blob.slice(1.0, -1.0, "APPLICATION/X-FLIGHT");
  check(middle.size == 9 && middle.type == flight::String("application/x-flight") &&
            middle.text().get() == flight::String(u"light \uFFFD"),
        "Blob slice applies signed byte indexes and normalizes its replacement MIME type");

  const flight::Blob nested(flight::Array<flight::Blob>{middle, binary_blob});
  check(nested.size == middle.size + binary_blob.size,
        "Blob parts can retain and concatenate existing immutable byte sequences");

  const flight::Uint8Array view_part{0x21};
  const flight::Blob typed_parts(
      flight::Array<flight::BlobPart>{
          flight::String("Flight"),
          flight::ArrayBufferView(view_part),
          flight::Blob(flight::Array<flight::String>{" SDK"}),
      },
      flight::BlobPropertyBag{.type = "TEXT/PLAIN"});
  check(typed_parts.text().get() == flight::String("Flight! SDK") &&
            typed_parts.type == flight::String("text/plain"),
        "BlobPart and BlobPropertyBag preserve the standard constructor domains");

  const auto weak = text_blob.weaken();
  const auto recovered = flight::Blob::lock_weak(weak);
  check(recovered.has_value() && recovered->identity() == text_blob.identity() &&
            recovered->type == flight::String("text/plain"),
        "Blob weak recovery preserves object identity and MIME metadata");

  const flight::Blob invalid_type(flight::Array<flight::String>{"value"}, {.type = "text/\nplain"});
  check(invalid_type.type.empty(), "Blob rejects MIME types outside the printable ASCII range");
}

void test_base64() {
  const flight::String binary(std::u16string{0x00, 0x66, 0x7F, 0x80, 0xFF});
  const auto encoded = flight::btoa(binary);
  check(encoded == flight::String("AGZ/gP8=") && flight::atob(encoded) == binary,
        "base64 functions round-trip the browser binary-string byte domain");
  check(flight::atob(" Z m 8 =\n") == flight::String("fo") &&
            flight::atob("AB==").char_code_at(0) == 0.0,
        "base64 decoding follows forgiving whitespace, padding, and trailing-bit behavior");

  bool invalid_base64_failed = false;
  try {
    static_cast<void>(flight::atob("A==="));
  } catch (const flight::InvalidCharacterError&) {
    invalid_base64_failed = true;
  }
  bool non_byte_failed = false;
  try {
    static_cast<void>(flight::btoa(flight::String(u"\u0100")));
  } catch (const flight::InvalidCharacterError&) {
    non_byte_failed = true;
  }
  check(invalid_base64_failed && non_byte_failed,
        "base64 functions reject invalid encodings and non-byte input strings");
}

void test_array_like_views() {
  flight::Array<double> array{1.0, 2.0, 3.0};
  const flight::SequenceView<double> array_view(array);
  check(array_view.identity() == array.identity() && array_view.length == 3 && array_view.element(1.0) == 2.0,
        "ArrayLike views preserve Array identity, length, and numeric access");
  array[1] = 8.0;
  double total = 0.0;
  for (const auto value : array_view) total += value;
  check(array_view[1] == 8.0 && total == 12.0,
        "ArrayLike views observe mutations without copying Array storage");
  check(array_view.at(-1) == std::optional<double>(3.0) && !array_view.get(3.0).has_value(),
        "ArrayLike views provide bounded and negative convenience access");

  flight::Uint16Array typed{4, 5, 6};
  const auto typed_tail = typed.subarray(1);
  const flight::SequenceView<double> typed_view(typed_tail);
  typed[1] = 9;
  check(typed_view.identity() == typed_tail.identity() && typed_view.length == 2 && typed_view[0] == 9.0,
        "ArrayLike typed-array adapters retain view identity and shared backing");

  auto owned_source = std::make_shared<std::vector<int>>(std::initializer_list<int>{7, 8});
  const auto erased_view = flight::SequenceView<double>::from_shared(owned_source);
  (*owned_source)[0] = 11;
  owned_source.reset();
  check(erased_view.length == 2 && erased_view[0] == 11.0,
        "ArrayLike structural adapters retain caller-owned sources and observe their elements");

  flight::ArrayBuffer buffer(12.0);
  flight::Uint16Array words(buffer, 2.0, 3.0);
  flight::DataView data(buffer, 4.0, 4.0);
  const flight::ArrayBufferView word_view(words);
  const flight::ArrayBufferView data_view(data);
  words[1] = 0x1234;
  check(word_view.kind == flight::ArrayBufferViewKind::uint16_array && word_view.byte_offset == 2 &&
            word_view.byte_length == 6 && word_view.bytes_per_element() == 2 &&
            word_view.identity() == words.identity(),
        "ArrayBufferView retains the typed view kind, range, and object identity");
  check(data_view.kind == flight::ArrayBufferViewKind::data_view && data_view.byte_offset == 4 &&
            data_view.byte_length == 4 && data_view.bytes_per_element() == 1 &&
            data_view.identity() == data.identity() &&
            data_view.same_backing(word_view),
        "ArrayBufferView preserves DataView identity and shared backing identity");
  check(data_view.data()[0] == buffer.data()[4] && data_view.data()[1] == buffer.data()[5],
        "ArrayBufferView exposes the original byte range without copying it");
}

void test_array_buffer_like() {
  flight::ArrayBuffer ordinary(8.0);
  const auto ordinary_alias = ordinary;
  check(ordinary.kind() == flight::ArrayBufferKind::array_buffer && ordinary.is_writable() &&
            ordinary.concurrency() == flight::ArrayBufferConcurrency::single_threaded &&
            ordinary.identity() == ordinary_alias.identity(),
        "ArrayBufferLike preserves ordinary backing identity and policy");

  flight::SharedArrayBuffer shared(8.0);
  flight::Uint8Array shared_bytes(shared);
  shared_bytes[0] = 42;
  check(shared_bytes.buffer.kind() == flight::ArrayBufferKind::shared_array_buffer &&
            shared_bytes.buffer.concurrency() == flight::ArrayBufferConcurrency::shared &&
            shared.data()[0] == std::byte{42},
        "typed arrays retain shared-memory backing without copying");

  std::weak_ptr<std::vector<std::uint16_t>> weak_external;
  {
    auto storage = std::make_shared<std::vector<std::uint16_t>>(std::initializer_list<std::uint16_t>{3, 4});
    weak_external = storage;
    auto external = flight::ArrayBufferLike::from_external(
        storage, reinterpret_cast<std::byte*>(storage->data()), storage->size() * sizeof(std::uint16_t));
    flight::Uint16Array words(external);
    const auto backing_identity = external.identity();
    storage.reset();
    external = flight::ArrayBufferLike{};
    words[1] = 9;
    check(!weak_external.expired() && words[0] == 3 && words[1] == 9 &&
              words.buffer.identity() == backing_identity &&
              words.buffer.kind() == flight::ArrayBufferKind::external &&
              words.buffer.concurrency() == flight::ArrayBufferConcurrency::caller_synchronized,
          "typed arrays retain external backing lifetime, identity, and mutability policy");
  }
  check(weak_external.expired(), "external backing is released with the last buffer view");

  const auto read_only_storage = std::make_shared<const std::array<std::byte, 2>>(
      std::array<std::byte, 2>{std::byte{0x12}, std::byte{0x34}});
  const auto read_only = flight::ArrayBufferLike::from_external(
      read_only_storage, read_only_storage->data(), read_only_storage->size());
  flight::DataView read_only_view(read_only);
  check(read_only.mutability() == flight::ArrayBufferMutability::read_only &&
            read_only_view.get_uint16(0.0) == 0x1234,
        "DataView reads owner-retained read-only external backing");
  bool data_view_write_failed = false;
  try {
    read_only_view.set_uint8(0.0, 1.0);
  } catch (const std::logic_error&) {
    data_view_write_failed = true;
  }
  bool typed_array_construction_failed = false;
  try {
    static_cast<void>(flight::Uint8Array(read_only));
  } catch (const std::invalid_argument&) {
    typed_array_construction_failed = true;
  }
  check(data_view_write_failed && typed_array_construction_failed,
        "mutable binary views reject read-only external backing deterministically");
}

void test_contract() {
  check(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
        "runtime advertises the compiler contract it implements");
  check(flight::runtime_contract.cpp_abi == 1, "runtime C++ ABI is explicit");
  check(flight::runtime_contract.cpp_abi == flight::abi_version && FLIGHT_CPP_ABI_VERSION == 1,
        "runtime contract and public version header agree on the C++ ABI");
  check(flight::runtime_contract.task_contract == "flight-runtime-task-capability-abi/1",
        "task capability ABI is explicit");
  check(flight::runtime_capability_status("array") == flight::RuntimeCapabilityStatus::initial,
        "implemented capabilities are queryable");
  check(flight::runtime_capability_status("array-buffer") == flight::RuntimeCapabilityStatus::initial &&
            flight::runtime_capability_status("array-buffer-like") == flight::RuntimeCapabilityStatus::initial &&
            flight::runtime_capability_status("array-buffer-view") == flight::RuntimeCapabilityStatus::initial &&
            flight::runtime_capability_status("data-view") == flight::RuntimeCapabilityStatus::initial,
        "binary storage capabilities are advertised after their implementation lands");
  check(flight::runtime_capability_status("blob") == flight::RuntimeCapabilityStatus::initial,
        "immutable Blob storage is advertised after its runtime implementation lands");
  check(flight::runtime_capability_status("erased-value") == flight::RuntimeCapabilityStatus::initial &&
            flight::runtime_capability_status("structured-clone") == flight::RuntimeCapabilityStatus::initial &&
            flight::runtime_capability_status("attached-properties") == flight::RuntimeCapabilityStatus::initial &&
            flight::runtime_capability_status("task-settlement-arms") == flight::RuntimeCapabilityStatus::initial,
        "the erased value, structured clone, attached properties, and settled arms are advertised "
        "only now that their semantic tests exist");
  check(flight::runtime_capability_status("canvas-2d-state") == flight::RuntimeCapabilityStatus::initial &&
            flight::runtime_capability_status("canvas-2d") == flight::RuntimeCapabilityStatus::unavailable,
        "the Canvas 2D drawing state is advertised while a Canvas 2D implementation is not");
  check(flight::runtime_capability_status("readable-stream") == flight::RuntimeCapabilityStatus::initial &&
            flight::runtime_capability_status("writable-stream") == flight::RuntimeCapabilityStatus::initial &&
            flight::runtime_capability_status("async-iterable") == flight::RuntimeCapabilityStatus::initial,
        "native streaming carriers are advertised after their runtime implementation lands");
  check(flight::runtime_capability_status("sequence-view") == flight::RuntimeCapabilityStatus::initial,
        "owner-preserving sequence views are advertised after their implementation lands");
  check(flight::runtime_capability_status("record") == flight::RuntimeCapabilityStatus::initial,
        "ordered Record storage is advertised after its runtime implementation lands");
  check(flight::runtime_capability_status("weak-set") == flight::RuntimeCapabilityStatus::initial,
        "weak identity sets are advertised after their runtime implementation lands");
  check(flight::runtime_capability_status("weak-map") == flight::RuntimeCapabilityStatus::initial,
        "weak identity maps are advertised after their runtime implementation lands");
  check(flight::runtime_capability_status("unicode-case-service") == flight::RuntimeCapabilityStatus::planned,
        "planned capabilities remain distinguishable");
  check(flight::runtime_capability_status("unknown") == flight::RuntimeCapabilityStatus::unavailable,
        "unknown capabilities are not treated as planned");
}

void test_error() {
  const flight::Error error(flight::String("expected"));
  check(error.message() == flight::String("expected") && std::string(error.what()) == "expected",
        "error preserves its semantic and native messages");
  check(flight::Error::name() == flight::String("Error"), "error exposes its source-language name");

  bool caught_range_error = false;
  try {
    throw flight::RangeError("outside");
  } catch (const flight::Error& caught) {
    caught_range_error = caught.message() == flight::String("outside");
  }
  check(caught_range_error && flight::RangeError::name() == flight::String("RangeError"),
        "RangeError accepts semantic strings and retains the Error exception base");

  const flight::TypeError type_error("wrong type");
  check(type_error.message() == flight::String("wrong type") &&
            flight::TypeError::name() == flight::String("TypeError"),
        "TypeError preserves its source-language name and message");

  const flight::DOMException dom_error("bad dimensions", "IndexSizeError");
  check(dom_error.message == flight::String("bad dimensions") &&
            dom_error.name == flight::String("IndexSizeError") && dom_error.code == 1.0 &&
            std::string(dom_error.what()) == "bad dimensions",
        "DOMException exposes its Web IDL name, message, and legacy code");
}

// A rasterizer that records the resolved commands it is handed, which is what lets the portable
// half of the Canvas 2D contract be tested without a rasterizer existing.
class RecordingRasterizer final : public flight::Canvas2DRasterizer {
 public:
  struct FillRecord final {
    flight::String fill_rule;
    std::size_t segments;
    flight::CanvasStyle style;
    double global_alpha;
    flight::DomMatrix first_transform;
  };

  struct ImageRecord final {
    flight::Canvas2DRect destination;
    flight::Canvas2DRect source;
    std::type_index source_type{typeid(void)};
  };

  void clear_rect(const flight::Canvas2DRect& rect, const flight::Canvas2DDrawingState&) override {
    cleared.push_back(rect);
  }

  void fill_path(const flight::Canvas2DPath& path, const flight::String& fill_rule,
                 const flight::Canvas2DDrawingState& state) override {
    fills.push_back(FillRecord{
        fill_rule, path.segments().size(), state.fill_style, state.global_alpha,
        path.segments().empty() ? flight::DomMatrix() : path.segments().front().transform});
  }

  void stroke_path(const flight::Canvas2DPath& path,
                   const flight::Canvas2DDrawingState& state) override {
    strokes.push_back(FillRecord{flight::String("stroke"), path.segments().size(),
                                 state.stroke_style, state.line_width,
                                 state.transform});
  }

  void draw_image(const flight::Canvas2DImageSource& image, const flight::Canvas2DRect& source,
                  const flight::Canvas2DRect& destination,
                  const flight::Canvas2DDrawingState&) override {
    images.push_back(ImageRecord{destination, source, image.source_type()});
  }

  void draw_text(const flight::String& text, double x, double y, std::optional<double> max_width,
                 bool stroke, const flight::Canvas2DDrawingState& state) override {
    texts.push_back(text);
    text_positions.push_back({x, y});
    text_stroked.push_back(stroke);
    text_max_widths.push_back(max_width);
    text_fonts.push_back(state.font);
  }

  [[nodiscard]] flight::WebTextMetrics measure_text(
      const flight::String& text, const flight::Canvas2DDrawingState&) override {
    flight::WebTextMetrics metrics;
    metrics.width = static_cast<double>(text.length()) * 7.0;
    return metrics;
  }

  [[nodiscard]] flight::ImageData read_image_data(
      const flight::Canvas2DRect& rect, const flight::ImageDataSettings& settings) override {
    reads.push_back(rect);
    return flight::ImageData(rect.width, rect.height, settings);
  }

  void write_image_data(const flight::ImageData& image, double x, double y,
                        const std::optional<flight::Canvas2DRect>& dirty) override {
    writes.push_back({image.width, image.height, x, y});
    dirty_rects.push_back(dirty);
  }

  [[nodiscard]] bool is_point_in_path(const flight::Canvas2DPath&, double, double,
                                      const flight::String&,
                                      const flight::Canvas2DDrawingState&) override {
    return true;
  }

  [[nodiscard]] bool is_point_in_stroke(const flight::Canvas2DPath&, double, double,
                                        const flight::Canvas2DDrawingState&) override {
    return false;
  }

  std::vector<flight::Canvas2DRect> cleared;
  std::vector<std::optional<flight::Canvas2DRect>> dirty_rects;
  std::vector<FillRecord> fills;
  std::vector<flight::String> text_fonts;
  std::vector<ImageRecord> images;
  std::vector<flight::Canvas2DRect> reads;
  std::vector<FillRecord> strokes;
  std::vector<flight::String> texts;
  std::vector<std::optional<double>> text_max_widths;
  std::vector<std::pair<double, double>> text_positions;
  std::vector<bool> text_stroked;
  std::vector<std::array<double, 4>> writes;
};

struct TestCanvasImage final {
  int handle;
};

struct TestNativeHandle final {
  int window;

  [[nodiscard]] friend bool operator==(const TestNativeHandle&, const TestNativeHandle&) = default;
};

struct TestSnapshotNode final {
  flight::Array<double> samples;
  std::optional<flight::Ref<TestSnapshotNode>> next;
};

} // namespace

namespace flight {

template <>
struct structured_clone_traits<TestSnapshotNode> {
  [[nodiscard]] static TestSnapshotNode clone(const TestSnapshotNode& value,
                                              detail::StructuredCloneMemo& memo) {
    return TestSnapshotNode{structured_clone(value.samples, memo),
                            structured_clone(value.next, memo)};
  }
};

} // namespace flight

namespace {

void test_attached_properties() {
  const auto object = flight::make_ref<TestReference>(TestReference{{}, 11});
  const auto key = flight::Symbol::for_key(flight::String("ParticleEmitterSignals"));
  const auto other = flight::Symbol::for_key(flight::String("Other"));

  auto view = flight::attached_properties(object);
  check(static_cast<bool>(view) && !view.has(key) && !view.get(key).has_value(),
        "an object with nothing attached reports no entry for a key");

  view.set(key, flight::Any());
  check(view.has(key) && view.get(key).has_value() && view.get(key)->is_undefined(),
        "an entry written with undefined is present, and is not the same as no entry");
  check(!view.has(other) && !view.get(other).has_value(),
        "a key that was never written still reports no entry");

  view.set(key, flight::Any(7.0));
  check(view.get(key)->as_number() == 7.0, "an attached entry is overwritten in place");

  // The attachment is reachable from every projection of the same object, which is the property a
  // per-type owner table did not have.
  const auto second_view = flight::attached_properties(object);
  check(second_view == view && second_view.get(key)->as_number() == 7.0,
        "a second erased view of one object reaches the same attached entries");

  const flight::StructuralRef<flight::RowOf<TestReference>> row(object);
  check(flight::attached_properties(row) == view,
        "a structural projection of the object shares its attached entries");
  const flight::Ref<void> erased = std::static_pointer_cast<void>(object);
  const auto erased_view = flight::attached_properties(erased);
  check(erased_view == view && erased_view.get(key)->as_number() == 7.0,
        "an erased Ref<void> reaches the same attached entries as the typed projection");
  erased_view.set(key, flight::Any(8.0));
  check(view.get(key)->as_number() == 8.0,
        "a write through the erased view is visible through the typed projection");

  // The attachment outlives the view that created it, because it belongs to the object.
  {
    auto transient = flight::attached_properties(object);
    transient.set(other, flight::Any(flight::String("kept")));
  }
  check(flight::attached_properties(object).get(other)->as_string() == flight::String("kept"),
        "an attachment survives the view that wrote it and belongs to the object");

  check(!flight::attached_properties(flight::Ref<TestReference>()),
        "an absent reference has no attached properties");

  // The spelling the compiler already emits for `Record<symbol, T | undefined>` over an erased
  // object, and the reason the three views must be one store rather than three that agree.
  const flight::Record<flight::Symbol, std::optional<flight::Ref<TestReference>>> projected(erased);
  projected.set(key, std::optional<flight::Ref<TestReference>>());
  check(projected.has(key) && projected.get(key).has_value() && !projected.get(key)->has_value(),
        "an entry present with an absent value is not the same as no entry at all");
  projected.set(other, std::optional<flight::Ref<TestReference>>(object));
  check(*projected.get(other) == object, "a record view stores and returns the object it was given");
  check(flight::attached_properties(object).has(other),
        "a write through the record view is visible through the attached-property view");
  check(projected.begin() == projected.end(),
        "an attached view is not enumerable, because symbol properties are not enumerable");
  const auto detached = projected.clone();
  check(detached.has(other) && detached.identity() != projected.identity(),
        "cloning an attached view copies its entries into ordinary storage");
  detached.erase(other);
  check(projected.has(other), "a clone is detached: erasing from it does not touch the object");
  check(projected.erase(other) && !projected.has(other),
        "an entry removed through the record view is gone from the shared store");

  const flight::StructuralRef<flight::RowOf<TestReference>> structural(object);
  flight::row_set(structural, other, std::optional<flight::Ref<TestReference>>(object));
  check(projected.has(other) && *projected.get(other) == object,
        "a computed-symbol write through a structural row is the same entry the record view reads");

  // A row keeps its object alive, and the owner does not: dropping every reference releases both.
  const void* identity = nullptr;
  {
    const auto scoped = flight::make_ref<TestReference>(TestReference{{}, 1});
    identity = flight::attached_properties(scoped).identity();
    flight::attached_properties(scoped).set(key, flight::Any(1.0));
  }
  const auto replacement = flight::make_ref<TestReference>(TestReference{{}, 2});
  const auto replacement_view = flight::attached_properties(replacement);
  check(replacement_view.identity() != identity || !replacement_view.get(key).has_value(),
        "a destroyed object never hands its attached entries to a later object");
}

void test_structured_clone() {
  const flight::Array<double> samples{1.0, 2.0};
  const auto cloned_samples = flight::structured_clone(samples);
  check(cloned_samples.size() == 2 && cloned_samples[0] == 1.0,
        "a cloned array carries the same contents");
  cloned_samples.push(3.0);
  check(samples.size() == 2,
        "a cloned array has its own storage rather than sharing the source's");

  flight::Map<flight::String, flight::Array<double>> source;
  source.set(flight::String("a"), samples);
  const auto cloned_map = flight::structured_clone(source);
  check(cloned_map.size() == 1 &&
            cloned_map.get(flight::String("a"))->size() == 2,
        "a cloned map carries its entries");
  cloned_map.get(flight::String("a"))->push(9.0);
  check(samples.size() == 2, "a cloned map clones its values rather than aliasing them");

  // Sharing is preserved: two members that pointed at one object still do.
  const auto shared = flight::make_ref<TestSnapshotNode>(
      TestSnapshotNode{flight::Array<double>{5.0}, std::nullopt});
  const auto holder = flight::make_ref<TestSnapshotNode>(
      TestSnapshotNode{flight::Array<double>{}, shared});
  const flight::Array<flight::Ref<TestSnapshotNode>> graph{holder, shared};
  const auto cloned_graph = flight::structured_clone(graph);
  check(cloned_graph[0] != holder && cloned_graph[1] != shared,
        "cloned references are new objects rather than the originals");
  check(*cloned_graph[0]->next == cloned_graph[1],
        "a reference reached twice is cloned once, so the clone keeps the source's sharing");

  // Cycles terminate: the clone is recorded before its contents are copied.
  const auto first = flight::make_ref<TestSnapshotNode>(
      TestSnapshotNode{flight::Array<double>{1.0}, std::nullopt});
  first->next = first;
  const auto cloned_cycle = flight::structured_clone(first);
  check(cloned_cycle != first && *cloned_cycle->next == cloned_cycle,
        "a cyclic reference graph clones into an equally cyclic one");

  bool symbol_refused = false;
  bool function_refused = false;
  bool erased_object_refused = false;
  try {
    static_cast<void>(flight::structured_clone(flight::Symbol(flight::String("s"))));
  } catch (const flight::DataCloneError& error) {
    symbol_refused = error.exception.name == flight::String("DataCloneError");
  }
  try {
    static_cast<void>(flight::structured_clone(
        flight::Any::function(flight::Function<void()>([] {}))));
  } catch (const flight::DataCloneError&) {
    function_refused = true;
  }
  try {
    static_cast<void>(flight::structured_clone(flight::Any::object(shared)));
  } catch (const flight::DataCloneError&) {
    erased_object_refused = true;
  }
  check(symbol_refused && function_refused,
        "structuredClone refuses symbols and functions, as JavaScript does");
  check(erased_object_refused,
        "an erased object with no clone definition is refused rather than shallow copied");

  check(flight::structured_clone(flight::Any(2.5)).as_number() == 2.5 &&
            flight::structured_clone(flight::Any(flight::String("x"))).as_string() ==
                flight::String("x"),
        "the primitive alternatives of an erased value clone exactly");
}

void test_settled_task_arms() {
  const flight::TaskSettlement<double> fulfilled{
      flight::TaskStatus::fulfilled, std::optional<double>(4.0), std::nullopt};
  const flight::TaskSettlement<double> rejected{
      flight::TaskStatus::rejected, std::nullopt,
      std::optional<flight::Rejection>(flight::Rejection::from_value(flight::String("bad")))};

  const auto fulfilled_arm = flight::as_fulfilled(fulfilled);
  check(fulfilled_arm.has_value() && fulfilled_arm->status == flight::String("fulfilled") &&
            fulfilled_arm->value == 4.0,
        "a fulfilled settlement narrows to an arm that carries only a value");
  check(!flight::as_rejected(fulfilled).has_value(),
        "a fulfilled settlement does not narrow to the rejected arm");

  const auto rejected_arm = flight::as_rejected(rejected);
  check(rejected_arm.has_value() && rejected_arm->status == flight::String("rejected") &&
            rejected_arm->reason.as<flight::String>() == flight::String("bad"),
        "a rejected settlement narrows to an arm that carries only a reason");
  check(!flight::as_fulfilled(rejected).has_value(),
        "a rejected settlement does not narrow to the fulfilled arm");

  const flight::TaskSettlement<double> pending{flight::TaskStatus::pending, std::nullopt,
                                               std::nullopt};
  check(!flight::as_fulfilled(pending).has_value() && !flight::as_rejected(pending).has_value(),
        "a pending settlement is neither arm");
}

void test_number_to_fixed() {
  // Every expectation here was read out of Node, not derived from the implementation. The two that
  // matter are the tie cases, where printf and ECMAScript disagree: printf rounds a tie to even and
  // the language rounds it away from zero.
  struct FixedCase final {
    double value;
    double digits;
    const char* expected;
  };
  const std::array<FixedCase, 21> cases{{
      {1.005, 2, "1.00"},          {2.5, 0, "3"},
      {0.5, 0, "1"},               {1.45, 1, "1.4"},
      {-1.5, 0, "-2"},             {0.0, 2, "0.00"},
      {-0.0, 2, "0.00"},           {1e21, 2, "1e+21"},
      {123.456, 2, "123.46"},      {0.000001, 2, "0.00"},
      {1.25, 1, "1.3"},            {1.35, 1, "1.4"},
      {-2.5, 0, "-3"},             {9.995, 2, "9.99"},
      {1e-7, 3, "0.000"},          {1234.5678, 3, "1234.568"},
      {100.0, 0, "100"},           {0.1, 20, "0.10000000000000000555"},
      {-1e-7, 2, "-0.00"},         {9.99, 1, "10.0"},
      {99.99, 1, "100.0"},
  }};
  bool all_match = true;
  for (const auto& entry : cases) {
    if (flight::number_to_fixed(entry.value, entry.digits) != flight::String(entry.expected)) {
      all_match = false;
      std::cerr << "  toFixed(" << entry.value << ", " << entry.digits
                << ") = " << flight::number_to_fixed(entry.value, entry.digits).to_utf8()
                << ", expected " << entry.expected << "\n";
    }
  }
  check(all_match, "number_to_fixed matches Number.prototype.toFixed, ties included");

  check(flight::number_to_fixed(std::numeric_limits<double>::quiet_NaN(), 2) ==
            flight::String("NaN"),
        "toFixed reports NaN as NaN rather than formatting it");
  check(flight::number_to_fixed(std::numeric_limits<double>::infinity(), 2) ==
            flight::String("Infinity"),
        "toFixed falls back to the number's own string above the fixed-notation range");

  bool digits_rejected = false;
  bool negative_digits_rejected = false;
  try {
    static_cast<void>(flight::number_to_fixed(1.0, 101.0));
  } catch (const std::range_error&) {
    digits_rejected = true;
  }
  try {
    static_cast<void>(flight::number_to_fixed(1.0, -1.0));
  } catch (const std::range_error&) {
    negative_digits_rejected = true;
  }
  check(digits_rejected && negative_digits_rejected,
        "toFixed rejects a digit count outside the range the language permits");
}

void test_any_domain() {
  const flight::Any absent;
  check(absent.is_undefined() && absent.type_of() == flight::String("undefined") &&
            absent.is_nullish(),
        "a default-constructed erased value is undefined");
  check(flight::Any(flight::null).is_null() &&
            flight::Any(flight::null).type_of() == flight::String("object"),
        "null keeps the typeof result the language actually produces");
  check(flight::Any(nullptr).is_null(), "a null pointer literal erases to null, not to undefined");

  const flight::Any number(3.5);
  const flight::Any integer(7);
  const flight::Any truth(true);
  const flight::Any text(flight::String("hi"));
  const flight::Any literal("hi");
  check(number.type_of() == flight::String("number") && number.as_number() == 3.5,
        "a number is represented as a number rather than as an object reference");
  check(integer.as_number() == 7.0, "an integral value widens to the single number domain");
  check(truth.type_of() == flight::String("boolean") && truth.as_boolean(),
        "a boolean keeps its own domain and is not confused with a number");
  check(text.type_of() == flight::String("string") && text.as_string() == flight::String("hi") &&
            literal.as_string() == flight::String("hi"),
        "a string keeps its own domain");

  const auto symbol = flight::Symbol(flight::String("tag"));
  const flight::Any symbolic(symbol);
  check(symbolic.type_of() == flight::String("symbol") && symbolic.as_symbol() == symbol,
        "a symbol keeps its own domain and its identity");

  bool wrong_access_failed = false;
  try {
    static_cast<void>(text.as_number());
  } catch (const flight::BadAnyAccess&) {
    wrong_access_failed = true;
  }
  check(wrong_access_failed,
        "reading a value out of the wrong domain throws rather than coercing silently");

  const auto reference = flight::make_ref<TestReference>(TestReference{{}, 4});
  const auto object = flight::Any::object(reference);
  check(object.type_of() == flight::String("object") && object.identity() == reference.get(),
        "an object reference keeps reference identity");
  check(object.object_if<TestReference>() == reference,
        "an object reference comes back at the type it went in as");
  check(object.object_if<TestEntity>() == nullptr,
        "an object reference refuses to be read as a type it does not hold");
  check(flight::Any::object(flight::Ref<TestReference>()).is_null(),
        "an object binding holding nothing erases to null rather than to an object");

  const auto handle = flight::Any::external(TestNativeHandle{9});
  check(handle.type_of() == flight::String("object"),
        "an opaque host handle reports the typeof a host object has");
  check(handle.external_if<TestNativeHandle>() != nullptr &&
            handle.external_if<TestNativeHandle>()->window == 9,
        "an opaque host handle hands the host back its own value");
  check(handle.external_if<double>() == nullptr,
        "an opaque host handle refuses to be read as a type it does not hold");
  check(handle.held_type() == std::type_index(typeid(TestNativeHandle)),
        "an erased value reports the concrete type it holds");
  check(number.held_type() == std::type_index(typeid(void)),
        "a primitive has no held object type");

  const flight::Function<int(int)> callable([](int value) { return value + 1; });
  const auto erased_callable = flight::Any::function(callable);
  check(erased_callable.type_of() == flight::String("function"),
        "a callable reports typeof function rather than typeof object");
  const auto* recovered = erased_callable.external_if<flight::Function<int(int)>>();
  check(recovered != nullptr && (*recovered)(1) == 2 && *recovered == callable,
        "a callable comes back invocable and with the identity its copies share");
}

void test_any_equality_and_absence() {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const flight::Any not_a_number(nan);
  check(!not_a_number.strict_equals(flight::Any(nan)),
        "NaN is not strictly equal to itself, as the language defines it");
  check(not_a_number.same_value_zero(flight::Any(nan)),
        "SameValueZero matches NaN with NaN so it can be a Map key");
  check(flight::Any(0.0).strict_equals(flight::Any(-0.0)) &&
            flight::Any(0.0).same_value_zero(flight::Any(-0.0)),
        "positive and negative zero are strictly equal");
  check(!flight::Any(0.0).same_value(flight::Any(-0.0)),
        "Object.is distinguishes positive from negative zero");
  check(!flight::Any(0.0).strict_equals(flight::Any(false)),
        "a number and a boolean are never strictly equal: there is no loose coercion");
  check(!flight::Any().strict_equals(flight::Any(flight::null)),
        "undefined and null are distinct values under strict equality");

  const auto first = flight::make_ref<TestReference>(TestReference{{}, 1});
  const auto second = flight::make_ref<TestReference>(TestReference{{}, 1});
  check(flight::Any::object(first) == flight::Any::object(first) &&
            !(flight::Any::object(first) == flight::Any::object(second)),
        "object references compare by identity rather than by contents");

  // The distinction the erased value exists to keep: a missing entry is not an entry whose value
  // is absent.
  flight::Record<flight::String, flight::Any> record;
  record.set(flight::String("present"), flight::Any());
  const flight::AnySlot missing = record.get(flight::String("absent"));
  const flight::AnySlot present = record.get(flight::String("present"));
  check(!missing.has_value(), "a key that was never written reports no entry at all");
  check(present.has_value() && present->is_undefined(),
        "a key written with undefined reports an entry whose value is undefined");
  check(!record.has(flight::String("absent")) && record.has(flight::String("present")),
        "the same distinction is visible through has");

  flight::Map<flight::Any, flight::String> keyed;
  keyed.set(flight::Any(nan), flight::String("nan"));
  keyed.set(flight::Any(flight::String("k")), flight::String("string"));
  check(keyed.get(flight::Any(nan)) == std::optional<flight::String>(flight::String("nan")),
        "an erased NaN key is retrievable, because Map keys use SameValueZero");
  check(keyed.get(flight::Any(flight::String("k"))) ==
            std::optional<flight::String>(flight::String("string")),
        "an erased string key matches by contents rather than by identity");
  check(!keyed.get(flight::Any()).has_value(), "an absent erased key finds nothing");
}

void test_any_conversion_and_ordering() {
  check(!flight::Any().to_boolean() && !flight::Any(flight::null).to_boolean() &&
            !flight::Any(0.0).to_boolean() &&
            !flight::Any(std::numeric_limits<double>::quiet_NaN()).to_boolean() &&
            !flight::Any(flight::String()).to_boolean() && !flight::Any(false).to_boolean(),
        "ToBoolean reports every falsy value as false");
  check(flight::Any(1.0).to_boolean() && flight::Any(flight::String("0")).to_boolean() &&
            flight::Any::external(TestNativeHandle{0}).to_boolean(),
        "ToBoolean reports non-empty strings and host objects as true");

  check(flight::relational_compare(flight::Any(1.0), flight::Any(2.0)) ==
            std::partial_ordering::less,
        "two numbers order numerically");
  check(flight::relational_compare(flight::Any(flight::String("a")),
                                   flight::Any(flight::String("b"))) == std::partial_ordering::less,
        "two strings order by their code units, not by a numeric conversion");
  check(flight::relational_compare(flight::Any(flight::String("10")), flight::Any(9.0)) ==
            std::partial_ordering::greater,
        "a string compared with a number converts to a number, as the language requires");
  check(flight::relational_compare(flight::Any(std::numeric_limits<double>::quiet_NaN()),
                                   flight::Any(1.0)) == std::partial_ordering::unordered,
        "NaN is unordered against every value, which is what makes every comparison false");
  check(flight::relational_compare(flight::Any(flight::null), flight::Any(0.0)) ==
            std::partial_ordering::equivalent,
        "null converts to zero in a relational comparison");

  bool symbol_order_failed = false;
  bool object_order_failed = false;
  try {
    static_cast<void>(
        flight::relational_compare(flight::Any(flight::Symbol()), flight::Any(1.0)));
  } catch (const flight::TypeError&) {
    symbol_order_failed = true;
  }
  try {
    static_cast<void>(flight::relational_compare(
        flight::Any::external(TestNativeHandle{1}), flight::Any(1.0)));
  } catch (const flight::TypeError&) {
    object_order_failed = true;
  }
  check(symbol_order_failed, "ordering a symbol throws TypeError, as it does in JavaScript");
  check(object_order_failed,
        "ordering a non-primitive throws rather than inventing a ToPrimitive result");
}

void test_canvas_2d_state() {
  auto rasterizer = std::make_shared<RecordingRasterizer>();
  auto context = flight::CanvasRenderingContext2D::create(
      rasterizer, flight::Canvas2DSurface::create(64.0, 32.0),
      flight::CanvasRenderingContext2DSettings{.alpha = false,
                                              .color_space = std::nullopt,
                                              .desynchronized = std::nullopt,
                                              .will_read_frequently = std::nullopt});

  check(context.canvas.width == 64.0 && context.canvas.height == 32.0,
        "a 2D context reports the dimensions of the surface it was created for");
  check(context.get_context_attributes().alpha == std::optional<bool>(false) &&
            !context.get_context_attributes().desynchronized.has_value(),
        "getContextAttributes returns the settings the context was created with, and keeps an "
        "omitted member distinct from an explicit false");
  check(!context.is_context_lost(), "a context with a rasterizer attached is not lost");
  context.canvas.width = 80.0;
  context.fill_rect(0.0, 0.0, 1.0, 1.0);
  check(context.canvas.shared_width() == 80.0 && context.canvas.shared_height() == 32.0,
        "a surface dimension written through a context copy is published to the shared surface "
        "before the next operation that depends on it");

  context.global_alpha = 0.25;
  context.fill_style = flight::String("#112233");
  context.line_width = 4.0;
  context.save();
  check(context.saved_state_depth() == 1, "save pushes one drawing state");
  context.global_alpha = 0.75;
  context.fill_style = flight::String("#445566");
  context.line_width = 9.0;
  context.restore();
  check(context.global_alpha == 0.25 && context.fill_style.color() == flight::String("#112233") &&
            context.line_width == 4.0 && context.saved_state_depth() == 0,
        "restore returns every drawing-state attribute to the saved values");

  context.restore();
  check(context.global_alpha == 0.25,
        "restore with an empty state stack leaves the current state unchanged");

  context.translate(10.0, 20.0);
  context.scale(2.0, 4.0);
  const auto transform = context.get_transform();
  check(transform.a == 2.0 && transform.d == 4.0 && transform.e == 10.0 && transform.f == 20.0,
        "translate and scale compose into the current transformation matrix");
  const auto mapped = transform.transform_point(1.0, 1.0);
  check(mapped.first == 12.0 && mapped.second == 24.0,
        "the current transformation matrix maps a user-space point to device space");

  context.save();
  context.rotate(0.0);
  context.set_transform(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
  check(context.get_transform() == flight::DomMatrix(),
        "setTransform replaces the current transformation matrix rather than composing with it");
  context.restore();
  check(context.get_transform().a == 2.0,
        "restore returns the current transformation matrix with the rest of the state");

  context.reset();
  check(context.get_transform() == flight::DomMatrix() && context.global_alpha == 1.0 &&
            context.current_path().empty(),
        "reset returns the transform, the attributes, and the path to their initial values");
}

void test_canvas_2d_path() {
  auto rasterizer = std::make_shared<RecordingRasterizer>();
  auto context = flight::CanvasRenderingContext2D::create(
      rasterizer, flight::Canvas2DSurface::create(8.0, 8.0));

  context.begin_path();
  context.translate(5.0, 0.0);
  context.move_to(1.0, 1.0);
  context.scale(2.0, 2.0);
  context.line_to(3.0, 3.0);
  const auto& segments = context.current_path().segments();
  check(segments.size() == 2, "each path call appends one segment");
  check(segments[0].transform.e == 5.0 && segments[0].transform.a == 1.0,
        "a segment retains the transform in force when it was added");
  check(segments[1].transform.a == 2.0,
        "a later segment retains the later transform rather than being re-transformed");

  context.begin_path();
  context.line_to(4.0, 5.0);
  check(context.current_path().segments().size() == 1 &&
            std::holds_alternative<flight::Canvas2DMoveSegment>(
                context.current_path().segments()[0].verb),
        "lineTo with no subpath starts one instead of drawing from the origin");

  context.begin_path();
  context.move_to(0.0, 0.0);
  context.quadratic_curve_to(1.0, 2.0, 3.0, 0.0);
  const auto& cubic = std::get<flight::Canvas2DCubicSegment>(
      context.current_path().segments()[1].verb);
  check(std::abs(cubic.control1_x - 2.0 / 3.0) < 1e-12 &&
            std::abs(cubic.control1_y - 4.0 / 3.0) < 1e-12 && cubic.x == 3.0 && cubic.y == 0.0,
        "a quadratic curve is raised to the exact cubic with the same shape");

  context.begin_path();
  context.arc(2.0, 2.0, 3.0, 0.0, 1.5);
  const auto& arc = std::get<flight::Canvas2DArcSegment>(
      context.current_path().segments()[0].verb);
  check(arc.radius_x == 3.0 && arc.radius_y == 3.0 && arc.start_angle == 0.0 &&
            arc.end_angle == 1.5 && !arc.counterclockwise,
        "an arc is retained as an arc rather than flattened at a tolerance the runtime picks");

  bool negative_radius_failed = false;
  try {
    context.arc(0.0, 0.0, -1.0, 0.0, 1.0);
  } catch (const flight::DOMException& error) {
    negative_radius_failed = error.name == flight::String("IndexSizeError");
  }
  check(negative_radius_failed, "a negative arc radius throws IndexSizeError");

  context.begin_path();
  context.move_to(0.0, 0.0);
  context.arc_to(10.0, 0.0, 10.0, 10.0, 0.0);
  check(context.current_path().segments().size() == 2 &&
            std::holds_alternative<flight::Canvas2DLineSegment>(
                context.current_path().segments()[1].verb),
        "arcTo with a zero radius degenerates to a straight line, as the specification requires");

  context.begin_path();
  context.rect(1.0, 2.0, 3.0, 4.0);
  check(context.current_path().segments().size() == 5 &&
            std::holds_alternative<flight::Canvas2DCloseSegment>(
                context.current_path().segments()[4].verb),
        "rect appends a closed four-sided subpath");

  context.begin_path();
  context.round_rect(0.0, 0.0, 10.0, 10.0, 2.0);
  std::size_t corner_arcs = 0;
  for (const auto& segment : context.current_path().segments()) {
    if (std::holds_alternative<flight::Canvas2DArcSegment>(segment.verb)) ++corner_arcs;
  }
  check(corner_arcs == 4, "roundRect rounds all four corners when given one radius");

  context.begin_path();
  context.round_rect(0.0, 0.0, 10.0, 10.0, 0.0);
  for (const auto& segment : context.current_path().segments()) {
    check(!std::holds_alternative<flight::Canvas2DArcSegment>(segment.verb),
          "roundRect with a zero radius emits no corner arc");
  }

  bool negative_corner_failed = false;
  try {
    context.round_rect(0.0, 0.0, 1.0, 1.0, -1.0);
  } catch (const flight::RangeError&) {
    negative_corner_failed = true;
  }
  check(negative_corner_failed, "a negative roundRect radius throws RangeError");

  context.set_line_dash(flight::Array<double>{1.0, 2.0, 3.0});
  check(context.get_line_dash().size() == 6,
        "an odd dash list is repeated to an even length, as the specification requires");
  context.set_line_dash(flight::Array<double>{1.0, -2.0});
  check(context.get_line_dash().size() == 6,
        "a dash list containing a negative segment is ignored rather than partly applied");
}

void test_canvas_2d_styles() {
  auto rasterizer = std::make_shared<RecordingRasterizer>();
  auto context = flight::CanvasRenderingContext2D::create(
      rasterizer, flight::Canvas2DSurface::create(4.0, 4.0));

  auto gradient = context.create_linear_gradient(0.0, 0.0, 1.0, 1.0);
  gradient.add_color_stop(1.0, flight::String("#ffffff"));
  gradient.add_color_stop(0.0, flight::String("#000000"));
  gradient.add_color_stop(0.5, flight::String("#808080"));
  const auto stops = gradient.color_stops();
  check(stops.size() == 3 && stops[0].offset == 0.0 && stops[1].offset == 0.5 &&
            stops[2].offset == 1.0,
        "gradient color stops are retained in offset order regardless of insertion order");

  const auto shared_gradient = gradient;
  shared_gradient.add_color_stop(0.25, flight::String("#404040"));
  check(gradient.color_stops().size() == 4 && gradient == shared_gradient &&
            gradient.identity() == shared_gradient.identity(),
        "gradient copies share one stop list and one identity");

  bool out_of_range_failed = false;
  try {
    gradient.add_color_stop(1.5, flight::String("#ffffff"));
  } catch (const flight::DOMException& error) {
    out_of_range_failed = error.name == flight::String("IndexSizeError");
  }
  check(out_of_range_failed, "a color stop outside [0, 1] throws IndexSizeError");

  bool negative_radius_failed = false;
  try {
    static_cast<void>(context.create_radial_gradient(0.0, 0.0, -1.0, 0.0, 0.0, 1.0));
  } catch (const flight::DOMException& error) {
    negative_radius_failed = error.name == flight::String("IndexSizeError");
  }
  check(negative_radius_failed, "a negative radial gradient radius throws IndexSizeError");

  const auto pattern = context.create_pattern(TestCanvasImage{7}, flight::String("repeat-x"));
  check(pattern.has_value() && pattern->repetition() == flight::String("repeat-x"),
        "createPattern retains the repetition it was given");
  check(pattern->source().source_if<TestCanvasImage>() != nullptr &&
            pattern->source().source_if<TestCanvasImage>()->handle == 7,
        "an erased image source hands the host back its own value with its own type");
  check(pattern->source().source_if<flight::ImageData>() == nullptr,
        "an erased image source refuses to be read as a type it does not hold");

  check(!context.create_pattern(flight::Canvas2DImageSource(), flight::String("repeat")).has_value(),
        "createPattern with no usable source returns an absent pattern");

  bool bad_repetition_failed = false;
  try {
    static_cast<void>(context.create_pattern(TestCanvasImage{1}, flight::String("tile")));
  } catch (const flight::DOMException& error) {
    bad_repetition_failed = error.name == flight::String("SyntaxError");
  }
  check(bad_repetition_failed, "an unrecognized pattern repetition throws SyntaxError");

  context.fill_style = gradient;
  check(context.fill_style.is_gradient() && context.fill_style.gradient() == gradient,
        "fillStyle accepts a gradient and retains its identity");
  context.fill_style = *pattern;
  check(context.fill_style.is_pattern(), "fillStyle accepts a pattern");
  context.fill_style = std::optional<flight::CanvasPattern>();
  check(context.fill_style.is_pattern(),
        "assigning an absent pattern leaves the previous fillStyle in place");
  context.fill_style = flight::String("#abcdef");
  check(context.fill_style.is_color() && context.fill_style.color() == flight::String("#abcdef"),
        "fillStyle accepts a color string");

  const auto matrix = flight::DomMatrix::from_init(
      flight::DomMatrix2DInit{2.0, std::nullopt, std::nullopt, 3.0, std::nullopt, std::nullopt,
                              std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                              std::nullopt, std::nullopt});
  check(matrix.a == 2.0 && matrix.b == 0.0 && matrix.d == 3.0,
        "DOMMatrix2DInit fills unmentioned components with the identity matrix");
  bool conflicting_alias_failed = false;
  try {
    static_cast<void>(
        flight::DomMatrix::from_init(flight::DomMatrix2DInit{2.0, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, 3.0, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt}));
  } catch (const flight::TypeError&) {
    conflicting_alias_failed = true;
  }
  check(conflicting_alias_failed,
        "a DOMMatrix2DInit whose alias pair disagrees is a TypeError rather than a silent winner");
}

void test_canvas_2d_drawing() {
  auto rasterizer = std::make_shared<RecordingRasterizer>();
  auto context = flight::CanvasRenderingContext2D::create(
      rasterizer, flight::Canvas2DSurface::create(16.0, 16.0));

  context.global_alpha = 0.5;
  context.fill_style = flight::String("#ff0000");
  context.translate(3.0, 4.0);
  context.fill_rect(0.0, 0.0, 2.0, 2.0);
  check(rasterizer->fills.size() == 1 && rasterizer->fills[0].segments == 5 &&
            rasterizer->fills[0].global_alpha == 0.5 &&
            rasterizer->fills[0].style.color() == flight::String("#ff0000") &&
            rasterizer->fills[0].first_transform.e == 3.0,
        "fillRect hands the rasterizer a resolved rectangle with the current state and transform");

  context.clear_rect(1.0, 2.0, 3.0, 4.0);
  check(rasterizer->cleared.size() == 1 && rasterizer->cleared[0].x == 1.0 &&
            rasterizer->cleared[0].height == 4.0,
        "clearRect reaches the rasterizer with the requested rectangle");

  context.line_width = 6.0;
  context.stroke_rect(0.0, 0.0, 1.0, 1.0);
  check(rasterizer->strokes.size() == 1 && rasterizer->strokes[0].global_alpha == 6.0,
        "strokeRect reaches the rasterizer with the current line width");

  context.draw_image(TestCanvasImage{3}, 1.0, 2.0);
  check(rasterizer->images.size() == 1 &&
            rasterizer->images[0].source_type == std::type_index(typeid(TestCanvasImage)) &&
            rasterizer->images[0].destination.x == 1.0,
        "drawImage carries the host's own source type through to the rasterizer");
  context.draw_image(TestCanvasImage{3}, 0.0, 0.0, 4.0, 4.0, 1.0, 1.0, 2.0, 2.0);
  check(rasterizer->images.size() == 2 && rasterizer->images[1].source.width == 4.0 &&
            rasterizer->images[1].destination.width == 2.0,
        "the nine-argument drawImage keeps the source and destination rectangles separate");

  bool empty_source_failed = false;
  try {
    context.draw_image(TestCanvasImage{3}, 0.0, 0.0, 0.0, 4.0, 0.0, 0.0, 1.0, 1.0);
  } catch (const flight::DOMException& error) {
    empty_source_failed = error.name == flight::String("IndexSizeError");
  }
  check(empty_source_failed, "drawImage with an empty source rectangle throws IndexSizeError");

  context.font = flight::String("16px serif");
  context.fill_text(flight::String("hi"), 2.0, 3.0);
  check(rasterizer->texts.size() == 1 && !rasterizer->text_stroked[0] &&
            rasterizer->text_fonts[0] == flight::String("16px serif") &&
            !rasterizer->text_max_widths[0].has_value(),
        "fillText forwards the current font and keeps an omitted maxWidth absent");
  context.stroke_text(flight::String("hi"), 0.0, 0.0, 12.0);
  check(rasterizer->text_stroked[1] && rasterizer->text_max_widths[1] == std::optional<double>(12.0),
        "strokeText is distinguished from fillText and forwards an explicit maxWidth");
  check(context.measure_text(flight::String("abc")).width == 21.0,
        "measureText returns the metrics the rasterizer produced");

  const auto read = context.get_image_data(0.0, 0.0, 2.0, 3.0);
  check(read.width == 2.0 && read.height == 3.0 && rasterizer->reads.size() == 1,
        "getImageData reads through the rasterizer");
  context.put_image_data(read, 1.0, 1.0);
  check(rasterizer->writes.size() == 1 && !rasterizer->dirty_rects[0].has_value(),
        "putImageData without a dirty rectangle reports none rather than the whole image");
  context.put_image_data(read, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0);
  check(rasterizer->dirty_rects[1].has_value() && rasterizer->dirty_rects[1]->width == 1.0,
        "the seven-argument putImageData forwards its dirty rectangle");

  bool empty_read_failed = false;
  try {
    static_cast<void>(context.get_image_data(0.0, 0.0, 0.0, 1.0));
  } catch (const flight::DOMException& error) {
    empty_read_failed = error.name == flight::String("IndexSizeError");
  }
  check(empty_read_failed, "getImageData with an empty rectangle throws IndexSizeError");

  context.begin_path();
  context.rect(0.0, 0.0, 1.0, 1.0);
  context.clip(flight::String("evenodd"));
  check(context.drawing_state().clip_regions.size() == 1 &&
            context.drawing_state().clip_regions[0].fill_rule == flight::String("evenodd"),
        "clip retains the fill rule it was applied under");

  const auto shared_context = context;
  check(shared_context == context && shared_context.identity() == context.identity(),
        "context copies share one identity");
  const auto fills_before_copy = rasterizer->fills.size();
  shared_context.fill_rect(0.0, 0.0, 1.0, 1.0);
  check(rasterizer->fills.size() == fills_before_copy + 1,
        "a context copy draws through the same rasterizer");
}

void test_canvas_2d_unattached() {
  flight::CanvasRenderingContext2D detached;
  check(!static_cast<bool>(detached), "a default-constructed context holds no state");

  bool save_failed = false;
  bool fill_failed = false;
  try {
    detached.save();
  } catch (const flight::Canvas2DUnattachedError&) {
    save_failed = true;
  }
  try {
    detached.fill();
  } catch (const flight::Canvas2DUnattachedError&) {
    fill_failed = true;
  }
  check(save_failed && fill_failed,
        "a context with no state throws rather than silently accepting drawing");

  auto lost = flight::CanvasRenderingContext2D::create(
      nullptr, flight::Canvas2DSurface::create(1.0, 1.0));
  check(lost.is_context_lost(), "a context created without a rasterizer reports itself lost");
  bool lost_fill_failed = false;
  try {
    lost.fill_rect(0.0, 0.0, 1.0, 1.0);
  } catch (const flight::Canvas2DUnattachedError&) {
    lost_fill_failed = true;
  }
  check(lost_fill_failed,
        "a lost context throws from a drawing call rather than reporting a silent success");
  lost.save();
  lost.translate(1.0, 1.0);
  check(lost.get_transform().e == 1.0,
        "a lost context still maintains the state and transform the runtime owns");
}

void test_image_data() {
  const flight::ImageData blank(2.0, 3.0);
  check(blank.width == 2.0 && blank.height == 3.0 && blank.data.size() == 24 &&
            blank.color_space == flight::String("srgb") &&
            static_cast<std::uint8_t>(blank.data[0]) == 0,
        "ImageData allocates transparent RGBA storage and applies the default color space");

  const flight::Uint8ClampedArray pixels{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
  const flight::ImageData sourced(
      pixels, 2.0, std::nullopt,
      flight::ImageDataSettings{.color_space = flight::String("display-p3")});
  check(sourced.data == pixels && sourced.width == 2.0 && sourced.height == 1.0 &&
            sourced.color_space == flight::String("display-p3"),
        "ImageData retains its supplied typed array and derives an omitted height");
  sourced.data[0] = 99.0;
  check(static_cast<std::uint8_t>(pixels[0]) == 99,
        "ImageData exposes the supplied typed array as live pixel storage");

  const auto alias = sourced;
  const flight::ImageData distinct(pixels, 2.0, 1.0);
  check(alias == sourced && alias.identity() == sourced.identity() && distinct != sourced,
        "ImageData copies preserve object identity while separate constructions do not");

  bool zero_dimension_failed = false;
  bool invalid_data_failed = false;
  bool inconsistent_height_failed = false;
  bool unsupported_color_space_failed = false;
  bool unaddressable_storage_failed = false;
  try {
    static_cast<void>(flight::ImageData(0.0, 1.0));
  } catch (const flight::DOMException& error) {
    zero_dimension_failed = error.name == flight::String("IndexSizeError");
  }
  try {
    static_cast<void>(flight::ImageData(flight::Uint8ClampedArray(3.0), 1.0));
  } catch (const flight::DOMException& error) {
    invalid_data_failed = error.name == flight::String("InvalidStateError");
  }
  try {
    static_cast<void>(flight::ImageData(pixels, 1.0, 1.0));
  } catch (const flight::DOMException& error) {
    inconsistent_height_failed = error.name == flight::String("IndexSizeError");
  }
  try {
    static_cast<void>(flight::ImageData(
        1.0, 1.0,
        flight::ImageDataSettings{.color_space = flight::String("unsupported")}));
  } catch (const flight::TypeError&) {
    unsupported_color_space_failed = true;
  }
  try {
    static_cast<void>(flight::ImageData(-1.0, -1.0));
  } catch (const flight::RangeError&) {
    unaddressable_storage_failed = true;
  }
  check(zero_dimension_failed && invalid_data_failed && inconsistent_height_failed &&
            unsupported_color_space_failed && unaddressable_storage_failed,
        "ImageData rejects invalid dimensions, source lengths, enums, and storage sizes");
}

void test_boolean_conversion() {
  check(!flight::to_boolean(flight::String()) && flight::to_boolean(flight::String("0")),
        "Boolean conversion uses string length rather than string contents");
  check(!flight::to_boolean(0.0) && !flight::to_boolean(-0.0) &&
            !flight::to_boolean(std::numeric_limits<double>::quiet_NaN()) &&
            flight::to_boolean(std::numeric_limits<double>::infinity()) &&
            flight::to_boolean(-3.0),
        "Boolean conversion preserves JavaScript number truthiness");
  check(!flight::to_boolean(flight::null) && !flight::to_boolean(flight::undefined) &&
            !flight::to_boolean(nullptr),
        "Boolean conversion rejects each nullish representation");

  const std::optional<flight::String> missing;
  const std::optional<flight::String> empty{flight::String()};
  const std::optional<flight::String> present{flight::String("flight")};
  check(!flight::to_boolean(missing) && !flight::to_boolean(empty) && flight::to_boolean(present),
        "Boolean conversion projects optional presence before converting its value");

  std::variant<flight::Null, double, flight::String> union_value{flight::null};
  check(!flight::to_boolean(union_value) && !union_value,
        "Boolean conversion and negation visit a null union member");
  union_value = flight::String("flight");
  check(flight::to_boolean(union_value) && !(!union_value),
        "Boolean conversion and negation visit a string union member");
  union_value = flight::String();
  check(!union_value, "closed-union negation preserves empty-string falsiness");

  const flight::Array<double> empty_array;
  const flight::Record<flight::String, double> empty_record;
  check(flight::to_boolean(empty_array) && flight::to_boolean(empty_record),
        "empty JavaScript collection objects remain truthy");

  const auto compact = flight::Array<flight::String>{"", "flight", "", "cpp"}.filter(
      flight::to_boolean);
  check(compact.size() == 2 && compact[0] == flight::String("flight") &&
            compact[1] == flight::String("cpp"),
        "Boolean remains a first-class filter predicate");
}

void test_host() {
  const auto previous_executor = flight::current_executor();
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const auto unicode = std::make_shared<TestUnicodeService>();
  {
    const flight::HostScope scope({.executor = executor, .unicode = unicode});
    check(flight::current_executor() == executor, "host scope installs its task executor");
    check(flight::String(u"\u00C4").to_lower() == flight::String(u"\u00E4"),
          "host scope installs its Unicode service");
  }
  check(flight::current_executor() == previous_executor, "host scope restores the previous executor");
  const auto first_monotonic_time = flight::host::performance_now();
  const auto second_monotonic_time = flight::host::performance_now();
  check(first_monotonic_time >= 0.0 && second_monotonic_time >= first_monotonic_time,
        "headless performance binding reports monotonic milliseconds from a stable origin");

  flight::host::TimerQueue timers;
  int timeout_calls = 0;
  int interval_calls = 0;
  const auto cancelled = timers.set_timeout([&] { ++timeout_calls; }, 0.0);
  timers.clear(cancelled);
  static_cast<void>(timers.set_timeout([&] { ++timeout_calls; }, 0.0));
  const auto interval = timers.set_interval([&] { ++interval_calls; }, 0.0);
  check(timers.pump() == 2 && timeout_calls == 1 && interval_calls == 1,
        "headless timers run due callbacks once per serialized host turn");
  check(timers.pump() == 1 && interval_calls == 2,
        "zero-delay intervals wait for the next host turn before repeating");
  timers.clear(interval);
  check(timers.empty(), "headless timer cancellation removes the scheduled callback");
}

void test_date() {
  const FlightDate epoch(0.0);
  check(epoch.get_time() == 0.0, "date retains epoch milliseconds");
  check(epoch.get_full_year() == 1970.0, "date exposes its UTC calendar year");
  check(epoch.get_month() == 0.0 && epoch.get_date() == 1.0 && epoch.get_day() == 4.0,
        "date exposes zero-based UTC month and Sunday-based weekday");
  check(epoch.get_hours() == 0.0 && epoch.get_minutes() == 0.0 && epoch.get_seconds() == 0.0 &&
            epoch.get_milliseconds() == 0.0,
        "date exposes UTC time fields");
  check(epoch.to_isostring() == "1970-01-01T00:00:00.000Z", "date formats an ISO UTC instant");
  check(std::isfinite(FlightDate::now()), "date now returns epoch milliseconds");
  check(FlightDate(1.9).get_time() == 1.0, "date applies integer TimeClip semantics");
  check(std::isnan(FlightDate(8.64e15 + 1.0).get_time()), "date clips instants outside the valid range");
}

void test_map() {
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  flight::Map<double, std::string> values{{1.0, "one"}, {nan, "missing"}};
  values.set(nan, "nan").set(-0.0, "zero");
  check(values.size() == 3, "map overwrites SameValueZero keys without changing size");
  check(values.get(nan) == std::optional<std::string>("nan"), "map retrieves NaN keys");
  check(values.has(0.0), "map treats signed zero as the same key");

  auto alias = values;
  check(alias.erase(1.0), "map erase reports an existing key");
  check(!values.has(1.0), "map copies retain reference identity");

  const auto independent = values.clone();
  check(std::isnan(independent.begin()->first), "map iteration retains insertion order after erase");
  alias.clear();
  check(independent.size() == 2 && values.empty(), "map clone creates independent ordered storage");
  std::string visited;
  independent.for_each([&](const std::string& value, double key) {
    static_cast<void>(key);
    visited += value;
  });
  check(visited == "nanzero", "map for_each visits value then key in insertion order");

  const flight::Map<flight::String, double> captured_map;
  captured_map.set("value", 4.0);
  check(captured_map.get("value") == std::optional<double>(4.0),
        "const map handles retain mutable JavaScript referent semantics");

  flight::Map<flight::String, double> live{{"first", 1.0}, {"second", 2.0}};
  auto keys = live.keys();
  auto shared_cursor = keys;
  const auto first = keys.next();
  live.erase("second");
  live.set("third", 3.0).set("second", 4.0);
  const auto third = shared_cursor.next();
  const auto second = keys.next();
  check(!first.done && first.value == std::optional<flight::String>("first") &&
            !third.done && third.value == std::optional<flight::String>("third") &&
            !second.done && second.value == std::optional<flight::String>("second"),
        "map iterator copies share a live insertion-order cursor");
  check(keys.next().done, "map iterator reports exhaustion");
  live.set("after-done", 5.0);
  check(shared_cursor.next().done, "an exhausted map iterator stays exhausted after additions");

  flight::Array<double> iterated_values;
  for (const auto value : live.values()) iterated_values.push(value);
  check(iterated_values.size() == 4 && iterated_values[0] == 1.0 && iterated_values[1] == 3.0 &&
            iterated_values[2] == 4.0 && iterated_values[3] == 5.0,
        "map value iterators support native range traversal in insertion order");
  const auto first_entry = live.entries().next();
  check(!first_entry.done && first_entry.value.has_value() &&
            first_entry.value->first == flight::String("first") && first_entry.value->second == 1.0,
        "map entry iterators retain key-value pairs");
}

void test_new_runtime_services() {
  static_assert(std::same_as<flight::Ref<flight::Ref<TestReference>>, flight::Ref<TestReference>>);
  static_assert(std::same_as<flight::Ref<flight::String>, flight::String>);
  check(flight::String("frame=") + 12.0 == flight::String("frame=12"),
        "string concatenation converts emitted numeric operands");

  flight::AbortController abort_controller;
  const auto abort_signal = abort_controller.signal;
  int abort_calls = 0;
  const std::function<void()> abort_listener = [&] { ++abort_calls; };
  abort_signal.add_event_listener("abort", abort_listener, {.once = true});
  abort_signal.add_event_listener("abort", abort_listener, {.once = true});
  abort_controller.abort(flight::String("cancelled"));
  abort_controller.abort(flight::String("ignored"));
  check(abort_signal.aborted && abort_calls == 1 &&
            abort_signal.reason.get<flight::String>() == flight::String("cancelled"),
        "AbortController shares state, fires listeners once, and retains the first reason");
  bool abort_threw = false;
  try {
    abort_signal.throw_if_aborted();
  } catch (const flight::AbortError& error) {
    abort_threw = error.reason().get<flight::String>() == flight::String("cancelled");
  }
  check(abort_threw, "AbortSignal throw_if_aborted carries the cancellation reason");

  flight::AbortController removed_abort_controller;
  int removed_abort_calls = 0;
  const std::function<void()> removed_abort_listener = [&] { ++removed_abort_calls; };
  removed_abort_controller.signal.add_event_listener("abort", removed_abort_listener);
  removed_abort_controller.signal.remove_event_listener("abort", removed_abort_listener);
  removed_abort_controller.abort();
  check(removed_abort_calls == 0, "AbortSignal removes a registered listener by callable identity");

  flight::AbortController identified_abort_controller;
  int identified_abort_calls = 0;
  flight::Function<void()> identified_abort_listener = [&] { ++identified_abort_calls; };
  identified_abort_controller.signal.add_event_listener("abort", identified_abort_listener);
  const auto captured_abort_listener = identified_abort_listener;
  identified_abort_listener = nullptr;
  identified_abort_controller.signal.remove_event_listener("abort", captured_abort_listener);
  identified_abort_controller.abort();
  check(
      identified_abort_calls == 0,
      "AbortSignal removes a copied identity-preserving Function listener");

  const auto json = flight::Json::stringify(flight::Array<double>{1.0, 2.0}, nullptr, 2.0);
  check(json == flight::String("[\n  1,\n  2\n]"),
        "JSON stringification covers semantic arrays and bounded indentation");
  const auto parsed_json = flight::Json::parse(
      R"({"name":"Flight","enabled":true,"values":[null,-1.5e2,"\ud83d\ude00"]})");
  const auto parsed_name = parsed_json.as_object().get("name");
  const auto parsed_values = parsed_json.as_object().get("values");
  check(parsed_name.has_value() && parsed_name->as_string() == flight::String("Flight") &&
            parsed_values.has_value() && parsed_values->as_array().size() == 3 &&
            parsed_values->as_array()[0].is_null() &&
            parsed_values->as_array()[1].as_number() == -150.0 &&
            parsed_values->as_array()[2].as_string() == flight::String::from_utf8("\xF0\x9F\x98\x80") &&
            flight::Json::stringify(parsed_json) ==
                flight::String::from_utf8(
                    R"({"name":"Flight","enabled":true,"values":[null,-150,"😀"]})"),
        "JSON parse and stringify preserve every JSON value domain and source object order");
  bool invalid_json_failed = false;
  try {
    static_cast<void>(flight::Json::parse("[1,]"));
  } catch (const flight::JsonSyntaxError&) {
    invalid_json_failed = true;
  }
  check(invalid_json_failed, "JSON parse rejects invalid source text");

  auto key = flight::make_ref<TestReference>(TestReference{.value = 4});
  std::weak_ptr<TestReference> weak_key = key;
  flight::WeakMap<flight::Ref<TestReference>, flight::String> weak_map;
  weak_map.set(key, "retained value");
  check(weak_map.get(key) == std::optional<flight::String>("retained value") && weak_map.has(key),
        "WeakMap retrieves values by reference identity");
  key.reset();
  check(weak_key.expired(), "WeakMap does not retain its key");

  auto erased_key = flight::make_ref<TestReference>(TestReference{.value = 5});
  flight::WeakMap<flight::Ref<void>, flight::ErasedValue> erased_map;
  auto numeric_view =
      flight::checked_weak_map_view<flight::Ref<TestReference>, double>(erased_map);
  auto numeric_alias =
      flight::checked_weak_map_view<flight::Ref<TestReference>, double>(erased_map);
  auto string_view =
      flight::checked_weak_map_view<flight::Ref<TestReference>, flight::String>(erased_map);
  numeric_view.set(erased_key, 4.0);
  check(numeric_alias.get(erased_key) == std::optional<double>(4.0) &&
            string_view.has(erased_key),
        "checked WeakMap views share storage and key operations across value types");
  bool wrong_erased_type_failed = false;
  try {
    static_cast<void>(string_view.get(erased_key));
  } catch (const flight::BadErasedValueCast&) {
    wrong_erased_type_failed = true;
  }
  check(wrong_erased_type_failed, "checked WeakMap reads reject a different erased value tag");
  string_view.set(erased_key, "updated");
  check(string_view.get(erased_key) == std::optional<flight::String>("updated"),
        "WeakMap overwrite updates the erased value tag");
  check(numeric_view.erase(erased_key) && !string_view.has(erased_key),
        "checked WeakMap deletion remains a shared key operation");
  const auto captured_weak_view = string_view;
  captured_weak_view.set(erased_key, "captured");
  check(string_view.get(erased_key) == std::optional<flight::String>("captured"),
        "const WeakMap view handles retain mutable JavaScript referent semantics");

  auto set_key = flight::make_ref<TestReference>(TestReference{.value = 6});
  std::weak_ptr<TestReference> weak_set_key = set_key;
  flight::WeakSet<flight::Ref<TestReference>> weak_set;
  auto weak_set_alias = weak_set;
  auto& add_identity = weak_set.add(set_key);
  check(&add_identity == &weak_set && weak_set_alias.has(set_key),
        "WeakSet add returns the set and copies share key storage");
  check(weak_set_alias.delete_key(set_key) && !weak_set.has(set_key),
        "WeakSet deletion is visible through every alias");
  weak_set.add(set_key);
  check(weak_set.delete_(set_key), "WeakSet exposes the compiler's escaped delete member spelling");
  weak_set.add(set_key).add(set_key);
  const auto captured_weak_set = weak_set;
  captured_weak_set.add(set_key);
  set_key.reset();
  check(weak_set_key.expired(), "WeakSet does not retain its key");

  flight::WeakSet<flight::Array<double>> weak_arrays;
  std::weak_ptr<std::vector<double>> weak_array_storage;
  {
    flight::Array<double> weak_array{1.0, 2.0};
    weak_array_storage = weak_array.weaken();
    weak_arrays.add(weak_array);
    check(weak_arrays.has(weak_array), "WeakSet accepts Flight reference-value wrappers by identity");
  }
  check(weak_array_storage.expired(), "WeakSet does not retain Flight array storage");

  const auto ignored_arguments = flight::bind_callable_v1<std::function<void(double)>>([] {});
  ignored_arguments(3.0);
  check(flight::callable_signature_v1<std::function<void(double)>>::accepts<double>,
        "callable ABI binds source functions that ignore emitted arguments");
  static_assert(!flight::callable_signature_v1<std::function<void(double)>>::accepts<int>);
  static_assert(flight::callable_signature_v1<std::function<void(double)>>::arity == 1);
  static_assert(std::same_as<
                flight::callable_signature_v1<std::function<void(double)>>::parameter_types,
                std::tuple<double>>);

  int identified_calls = 0;
  flight::Function<void(double)>::weak_type weak_function;
  {
    flight::Function<void(double)> identified = [&](double value) {
      identified_calls += static_cast<int>(value);
    };
    weak_function = identified.weaken();
    auto copied = identified;
    identified = nullptr;
    const auto recovered = flight::Function<void(double)>::lock_weak(weak_function);
    check(
        recovered.has_value() && copied == *recovered &&
            copied.identity() == recovered->identity(),
        "Function copies and weak recovery preserve callable identity");
    copied(2.0);
    (*recovered)(3.0);
    check(identified_calls == 5, "Function copies invoke one shared callable state");

    flight::WeakMap<flight::Function<void(double)>, flight::String> function_metadata;
    function_metadata.set(copied, flight::String("listener"));
    check(
        function_metadata.get(*recovered) == std::optional<flight::String>("listener"),
        "Function supplies the generic weak-key identity contract");
  }
  check(weak_function.expired(), "Function weak identity does not retain callable state");
  const auto first_function = flight::Function<void()>([] {});
  const auto second_function = flight::Function<void()>([] {});
  check(first_function != second_function, "distinct Function constructions retain distinct identities");

  int identified_ignored_calls = 0;
  const auto identified_ignored_arguments =
      flight::bind_callable_v1<flight::Function<void(double)>>([&] { ++identified_ignored_calls; });
  identified_ignored_arguments(4.0);
  check(
      identified_ignored_calls == 1 &&
          flight::callable_signature_v1<flight::Function<void(double)>>::accepts<double>,
      "identity-preserving callable ABI binds source functions that ignore emitted arguments");
  static_assert(!flight::callable_signature_v1<flight::Function<void(double)>>::accepts<int>);
  static_assert(flight::callable_signature_v1<flight::Function<void(double)>>::arity == 1);

  auto facet_source = flight::make_ref<TestReference>(TestReference{.value = 7});
  auto conditional = flight::assume_conditional_facets<TestImageCapabilities>(facet_source);
  flight::FacetRef<TestReference, TestImageFacet> image_facet = conditional;
  image_facet->value = 8;
  check(facet_source->value == 8 && image_facet.shared_reference() == facet_source &&
            conditional.shared_reference() == facet_source,
        "conditional facets retain exactly one shared base referent");

  auto entity = flight::make_ref<TestEntity>();
  using EntityView = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestEntity>>>>;
  EntityView entity_view(entity);
  EntityView second_entity_view(entity);
  check(entity_view == second_entity_view,
        "structural views over one source object reuse one row owner");
  auto entity_runtime = flight::make_ref<TestReference>(TestReference{.value = 9});
  const auto entity_runtime_symbol = flight::Symbol::for_key("EntityRuntime");
  flight::row_set(entity_view, entity_runtime_symbol, entity_runtime);
  check(flight::row_get<std::optional<flight::Ref<TestReference>>>(entity_view, entity_runtime_symbol)
                .value()
                ->value == 9,
        "computed structural symbols share the generated Entity runtime slot");
  check(flight::row_has(entity_view, entity_runtime_symbol),
        "computed structural slots expose presence without changing the value");

  int guard_calls = 0;
  auto guarded = flight::make_structural_write_proxy<EntityView::schema_type>(
      entity_view, entity_runtime_symbol, [&] { ++guard_calls; });
  check(guarded != entity_view, "a structural write proxy has distinct reference identity");
  const auto arbitrary_field = flight::Symbol::for_key("caller-field");
  flight::row_set(guarded, arbitrary_field, 12);
  check(flight::row_get<int>(entity_view, arbitrary_field) == 12 && guard_calls == 0,
        "a structural write proxy forwards arbitrary non-intercepted fields");
  flight::row_set(entity_view, arbitrary_field, 13);
  check(flight::row_get<int>(guarded, arbitrary_field) == 13,
        "target writes are visible through the structural proxy");
  auto guarded_runtime = flight::make_ref<TestReference>(TestReference{.value = 14});
  flight::row_set(guarded, entity_runtime_symbol, guarded_runtime);
  check(guard_calls == 1 && entity->entity_runtime_key.value() == guarded_runtime,
        "the structural proxy invokes its hook before forwarding an intercepted write");

  bool rejected_write = false;
  auto rejecting = flight::make_structural_write_proxy<EntityView::schema_type>(
      entity_view, entity_runtime_symbol, [&] {
        rejected_write = true;
        throw std::runtime_error("blocked structural write");
      });
  try {
    flight::row_set(rejecting, entity_runtime_symbol, entity_runtime);
  } catch (const std::runtime_error&) {
  }
  check(rejected_write && entity->entity_runtime_key.value() == guarded_runtime,
        "a structural proxy propagates a hook exception before changing its target");

  std::vector<int> proxy_order;
  auto inner = flight::make_structural_write_proxy<EntityView::schema_type>(
      entity_view, entity_runtime_symbol, [&] { proxy_order.push_back(1); });
  auto outer = flight::make_structural_write_proxy<EntityView::schema_type>(
      inner, entity_runtime_symbol, [&] { proxy_order.push_back(2); });
  flight::row_set(outer, entity_runtime_symbol, entity_runtime);
  using ReadonlyEntityView =
      flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<TestEntity>>>>;
  ReadonlyEntityView projected = outer;
  check(proxy_order == std::vector<int>({2, 1}) && projected == outer &&
            projected.shared_owner() == outer.shared_owner(),
        "nested structural proxies run outer-to-inner and projections retain proxy identity");

  check(flight::parse_int("  -0x10tail") == -16.0 && flight::parse_int("101", 2.9) == 5.0 &&
            flight::parse_int("10", std::numeric_limits<double>::quiet_NaN()) == 10.0,
        "parse_int applies TypeScript sign, prefix, partial-parse, and radix coercion rules");
  check(flight::parse_float("  -1.25e2tail") == -125.0 && flight::parse_float("1e+") == 1.0 &&
            std::isinf(flight::parse_float("+Infinityrest")) &&
            std::isnan(flight::parse_float("not-a-number")),
        "parse_float consumes the longest valid JavaScript numeric prefix");
  check(flight::is_safe_integer(9007199254740991.0) &&
            !flight::is_safe_integer(9007199254740992.0) &&
            !flight::is_safe_integer(1.5) &&
            !flight::is_safe_integer(std::numeric_limits<double>::infinity()),
        "is_safe_integer enforces the finite integral safe-number range");
  check(flight::to_number(flight::String(" ")) == 0.0 &&
            flight::to_number(flight::String("0b101")) == 5.0 &&
            flight::to_number(flight::String("-1.25e2")) == -125.0 &&
            std::isnan(flight::to_number(flight::String("-0x10"))),
        "to_number handles empty, prefixed, decimal, and invalid signed-prefix inputs");
  check(flight::number_to_string(255.0, 16.0) == flight::String("ff") &&
            flight::number_to_string(-10.5, 2.0) == flight::String("-1010.1") &&
            flight::number_to_string(0.1, 2.0) ==
                flight::String("0.0001100110011001100110011001100110011001100110011001101") &&
            flight::number_to_string(-0.0, 36.0) == flight::String("0"),
        "number_to_string follows JavaScript radix conversion for integer and fractional numbers");
  bool invalid_radix = false;
  try {
    static_cast<void>(flight::number_to_string(1.0, 1.0));
  } catch (const std::range_error&) {
    invalid_radix = true;
  }
  check(invalid_radix, "number_to_string rejects radices outside JavaScript's 2 through 36 range");

  flight::Map<flight::String, double> record{{"first", 1.0}, {"second", 2.0}};
  const auto keys = flight::object_keys(record);
  check(keys.size() == 2 && keys[0] == flight::String("first") && keys[1] == flight::String("second"),
        "object_keys preserves emitted key types and deterministic iteration order");
  const auto entries = flight::object_entries(record);
  flight::Map<flight::String, double> assigned{{"first", 0.0}, {"retained", 3.0}};
  auto& assigned_identity = flight::object_assign(assigned, record);
  check(entries.size() == 2 && std::get<0>(entries[1]) == flight::String("second") &&
            std::get<1>(entries[1]) == 2.0 && &assigned_identity == &assigned &&
            assigned.get("first") == std::optional<double>(1.0) &&
            assigned.get("retained") == std::optional<double>(3.0),
        "object entries preserve pairs and assign mutates and returns the original target");

  const auto first_symbol = flight::Symbol::for_key("entity");
  const auto same_symbol = flight::Symbol::for_key("entity");
  const flight::Symbol described_symbol("entity");
  const flight::Symbol second_described_symbol("entity");
  check(first_symbol == same_symbol && first_symbol != flight::Symbol::for_key("other") &&
            described_symbol != second_described_symbol && described_symbol != first_symbol &&
            described_symbol.key() == flight::String("entity") && flight::Symbol() != flight::Symbol(),
        "Symbol.for interns keys while direct described and anonymous symbols retain distinct identity");

  check(flight::Url("HTTP://example.test/path").protocol == flight::String("http:") &&
            flight::Url("child", "https://example.test/base").protocol == flight::String("https:"),
        "Url exposes normalized protocols for absolute and relative inputs");

  const auto decoded = flight::TextDecoder().decode(flight::Uint8Array{0x66, 0xF0, 0x9F, 0x98, 0x80});
  const auto malformed = flight::TextDecoder().decode(flight::Uint8Array{0xE0, 0x80, 0x80});
  const auto after_bom = flight::TextDecoder().decode(flight::Uint8Array{0xEF, 0xBB, 0xBF, 0x78});
  check(decoded == flight::String::from_utf8("f\xF0\x9F\x98\x80") &&
            malformed == flight::String(u"\uFFFD\uFFFD\uFFFD") && after_bom == flight::String("x") &&
            flight::TextDecoder().decode().empty(),
        "TextDecoder follows UTF-8 scalar and malformed-sequence replacement behavior");
  const auto encoded = flight::TextEncoder().encode(
      flight::String(std::u16string{u'f', 0xD83D, 0xDE00, 0xD800}));
  check(encoded.size() == 8 && encoded[0] == 0x66 && encoded[1] == 0xF0 && encoded[2] == 0x9F &&
            encoded[3] == 0x98 && encoded[4] == 0x80 && encoded[5] == 0xEF && encoded[6] == 0xBF &&
            encoded[7] == 0xBD && flight::TextEncoder().encoding == flight::String("utf-8"),
        "TextEncoder emits UTF-8 and replaces unpaired UTF-16 surrogates");

  flight::RegExp expression("^flight$", "gi");
  auto expression_alias = expression;
  check(expression.test("FLIGHT") && !expression_alias.test("flight") && expression.test("flight"),
        "global RegExp copies share match position and reset it after failure");
  const auto captures = flight::RegExp("([a-z]+):(\\d+)").exec("id:42");
  check(captures.has_value() && captures->size() == 3 && (*captures)[1] == flight::String("id") &&
            (*captures)[2] == flight::String("42") && captures->index == 0.0,
        "RegExp exec returns the complete match, captures, and source index");
  const auto unmatched = flight::RegExp("(a)?b").exec("b");
  check(unmatched.has_value() && unmatched->has_capture(0) && !unmatched->has_capture(1) &&
            !unmatched->capture(1).has_value() && (*unmatched)[1].empty(),
        "RegExp exec preserves unmatched-capture absence independently from its string projection");
  const auto global_matches = flight::String("a1b2").match(flight::RegExp("[0-9]", "g"));
  check(global_matches.has_value() && global_matches->size() == 2 &&
            (*global_matches)[0] == flight::String("1") && (*global_matches)[1] == flight::String("2"),
        "String match returns complete matches rather than captures for global expressions");
  check(flight::String("a1 b2").replace(flight::RegExp("([a-z])([0-9])", "g"),
                                         flight::String("$2$1")) == flight::String("1a 2b") &&
            flight::String("x=12").replace(
                flight::RegExp("([a-z]+)=([0-9]+)"),
                [](const flight::String&, const flight::String& name, const flight::String& value) {
                  return name.concat(flight::String(":"), value);
                }) == flight::String("x:12"),
        "RegExp global replacement expands captures and invokes replacement callbacks");
  check(flight::String("abc").replace(flight::RegExp("(a)(b)?(z)?"),
                                      flight::String("$$|$&|$`|$'|$1|$2|$3|$12")) ==
            flight::String("$|ab||c|a|b||a2c") &&
            flight::String("-").replace(flight::RegExp("(?:)", "g"), flight::String("_")) ==
                flight::String("_-_"),
        "RegExp replacement implements JavaScript substitution tokens and advances empty global matches");
  check(flight::String("b").replace(
            flight::RegExp("(a)?b"),
            [](const flight::String&, const std::optional<flight::String>& capture,
               double offset, const flight::String& input) {
              return (capture ? *capture : flight::String("undefined")) + flight::String(":") +
                     flight::String::from_number(offset) + flight::String(":") + input;
            }) == flight::String("undefined:0:b") &&
            flight::String::from_utf8("\xC3\xA9" "0x0").replace(
                flight::RegExp("[0-9]", "g"),
                [](const flight::String&, double offset) {
                  return flight::String::from_number(offset);
                }) == flight::String::from_utf8("\xC3\xA9" "1x3"),
        "RegExp replacement callbacks preserve absent captures and report UTF-16 source offsets");

  check(flight::IntlCollator().compare("a", "b") < 0.0 &&
            flight::IntlListFormat().format(flight::Array<flight::String>{"a", "b", "c"}) ==
                flight::String("a, b, and c") &&
            flight::IntlNumberFormat().format(1.25) == flight::String("1.25") &&
            flight::IntlPluralRules().select(1.0) == flight::String("one") &&
            flight::IntlRelativeTimeFormat().format(-2.0, "day") == flight::String("2 days ago"),
        "internationalization types provide a deterministic locale-neutral baseline");
}

void test_presence_and_math() {
  flight::Presence<int> value = flight::undefined;
  check(std::holds_alternative<flight::Undefined>(value), "undefined is distinct from a value");
  value = flight::null;
  check(std::holds_alternative<flight::Null>(value), "null is distinct from undefined");
  check(flight::round(3.5) == 4.0 && flight::round(-3.5) == -3.0,
        "Math.round resolves half-integer ties toward positive infinity");
  check(flight::round(-3.5000000000000004) == -4.0 &&
            flight::round(-3.4999999999999996) == -3.0,
        "Math.round distinguishes values immediately around a negative half boundary");
  check(std::signbit(flight::round(-0.5)) && std::signbit(flight::round(-0.1)) &&
            std::signbit(flight::round(-0.0)),
        "Math.round preserves negative zero where JavaScript does");
  check(flight::round(std::numeric_limits<double>::infinity()) ==
            std::numeric_limits<double>::infinity() &&
            flight::round(-std::numeric_limits<double>::infinity()) ==
                -std::numeric_limits<double>::infinity(),
        "Math.round preserves infinities");
  check(std::isnan(flight::round(std::numeric_limits<double>::quiet_NaN())),
        "Math.round preserves NaN");
  check(std::signbit(flight::sign(-0.0)), "Math.sign preserves negative zero");
  check(std::isnan(flight::sign(std::numeric_limits<double>::quiet_NaN())),
        "Math.sign preserves NaN");
  check(flight::power(2.0, 10.0) == 1024.0 && flight::power(4.0, -0.5) == 0.5,
        "exponentiation uses the compiler runtime spelling");
  check(flight::minimum(3.0, 2.0) == 2.0 && flight::maximum(3.0, 2.0) == 3.0 &&
            std::signbit(flight::minimum(0.0, -0.0)) &&
            !std::signbit(flight::maximum(0.0, -0.0)) &&
            std::isnan(flight::minimum(std::numeric_limits<double>::quiet_NaN(), 1.0)),
        "Math minimum and maximum preserve NaN and signed-zero semantics");
  check(flight::minimum(flight::Array<double>{3.0, -2.0, 8.0}) == -2.0 &&
            flight::maximum(flight::Array<double>{3.0, -2.0, 8.0}) == 8.0 &&
            flight::minimum(flight::Array<double>{}) == std::numeric_limits<double>::infinity() &&
            flight::maximum(flight::Array<double>{}) == -std::numeric_limits<double>::infinity(),
        "spread minimum and maximum handle ranges and empty inputs");
  check(flight::is_integer(-0.0) && flight::is_integer(42.0) && !flight::is_integer(0.5) &&
            !flight::is_integer(std::numeric_limits<double>::infinity()) &&
            !flight::is_integer(std::numeric_limits<double>::quiet_NaN()),
        "Number.isInteger recognizes finite integral doubles");
  check(flight::fround(1.337) == 1.3370000123977661 &&
            flight::fround(16777217.0) == 16777216.0 &&
            std::signbit(flight::fround(-0.0)) &&
            std::isfinite(flight::fround(3.4028235677973362e38)) &&
            std::isinf(flight::fround(3.4028235677973366e38)) &&
            std::isinf(flight::fround(std::numeric_limits<double>::max())) &&
            std::isnan(flight::fround(std::numeric_limits<double>::quiet_NaN())),
        "Math.fround preserves binary32 rounding, signed zero, overflow, and NaN");
  check(flight::Uint16Array::bytes_per_element == 2.0 &&
            flight::Uint32Array::bytes_per_element == 4.0,
        "typed-array byte widths match JavaScript constructor constants");
  check(flight::bitwise_and(-1.0, 255.0) == 255.0 &&
            flight::bitwise_or(4294967301.0, 2.0) == 7.0 &&
            flight::bitwise_xor(15.0, 5.0) == 10.0 && flight::bitwise_not(0.0) == -1.0,
        "bitwise helpers apply JavaScript ToInt32 conversion");
  check(flight::left_shift(1073741824.0, 1.0) == -2147483648.0 &&
            flight::left_shift(1.0, 33.0) == 2.0 &&
            flight::signed_right_shift(-2147483648.0, 31.0) == -1.0,
        "signed shifts wrap results and mask their count to five bits");
  check(flight::unsigned_right_shift(-1.0, 0.0) == 4294967295.0 &&
            flight::unsigned_right_shift(-1.0, 1.0) == 2147483647.0 &&
            flight::unsigned_right_shift(std::numeric_limits<double>::quiet_NaN(), 4.0) == 0.0,
        "unsigned right shift returns the JavaScript uint32 number result");
  check(std::abs(flight::pi - std::acos(-1.0)) < 1.0e-15 &&
            std::abs(flight::e - std::exp(1.0)) < 1.0e-15,
        "Math constants are portable and retain double precision");
}

void test_reference() {
  auto value = flight::make_ref<TestReference>(TestReference{.value = 3});
  flight::Ref<flight::ReferenceEnabled> base = value;
  auto alias = value;
  alias->value = 7;
  check(value->value == 7 && base == value, "references preserve shared object identity and support upcasts");

  const auto cell = flight::make_binding_cell(2);
  auto captured = cell;
  captured.rebind(5);
  const auto updated = cell.update_binding([](int& current) {
    current *= 3;
    return current;
  });
  check(updated == 15 && captured.read_binding() == 15,
        "binding-cell copies share mutable closure storage");
}

void test_record() {
  const auto first_symbol = flight::Symbol::for_key("first-record-symbol");
  const auto second_symbol = flight::Symbol::for_key("second-record-symbol");
  flight::Record<flight::PropertyKey, flight::String> values;
  values.set(flight::String("later"), "later")
      .set(flight::String("10"), "ten")
      .set(2.0, "two")
      .set(flight::String("01"), "leading")
      .set(-0.0, "zero")
      .set(4294967294.0, "largest index")
      .set(flight::String("4294967295"), "not an index")
      .set(first_symbol, "first symbol")
      .set(flight::String("after"), "after")
      .set(second_symbol, "second symbol");

  const auto keys = flight::object_keys(values);
  const flight::Array<flight::String> expected_keys{
      "0", "2", "10", "4294967294", "later", "01", "4294967295", "after"};
  check(keys.size() == expected_keys.size() &&
            std::equal(keys.begin(), keys.end(), expected_keys.begin()),
        "Record enumerates integer keys numerically before strings and excludes symbols");

  const auto entries = flight::object_entries(values);
  const auto object_values = flight::object_values(values);
  check(object_values.size() == expected_keys.size() &&
            object_values[0] == flight::String("zero") &&
            object_values[7] == flight::String("after"),
        "Object.values follows Record string-key order and excludes symbols");
  check(entries.size() == expected_keys.size() && std::get<0>(entries[0]) == flight::String("0") &&
            std::get<1>(entries[0]) == flight::String("zero") &&
            std::get<0>(entries[7]) == flight::String("after"),
        "Record entries follow Object.entries string-key order");

  const auto original_size = values.size();
  values.set(flight::String("2"), "updated through string key");
  check(values.size() == original_size && values.get(2.0) == std::optional<flight::String>("updated through string key") &&
            values.get(flight::String("2")) == std::optional<flight::String>("updated through string key"),
        "Record canonicalizes numeric and numeric-string property identity");
  check(!values.get(flight::String("missing")).has_value() && values.size() == original_size,
        "Record missing-key reads preserve absence without insertion");

  auto alias = values;
  auto clone = values.clone();
  alias.set(flight::String("shared"), "yes");
  check(alias == values && values.has(flight::String("shared")) && clone != values &&
            !clone.has(flight::String("shared")),
        "Record copies preserve object identity while clone creates independent storage");

  flight::Record<flight::PropertyKey, flight::String> assigned;
  auto& assigned_identity = flight::object_assign(assigned, values);
  check(&assigned_identity == &assigned && assigned.get(first_symbol) == std::optional<flight::String>("first symbol") &&
            assigned.get(flight::String("later")) == std::optional<flight::String>("later"),
        "Object.assign preserves Record target identity and copies string and symbol entries");
  const auto captured_record = values;
  captured_record.set(flight::String("captured"), "yes");
  check(values.get(flight::String("captured")) == std::optional<flight::String>("yes"),
        "const Record handles retain mutable JavaScript referent semantics");
}

void test_set() {
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  flight::Set<double> values{nan, nan, -0.0};
  check(values.size() == 2, "set uses SameValueZero and suppresses duplicate insertion");
  auto alias = values;
  alias.add(4.0);
  check(values.has(4.0), "set copies retain reference identity");
  const auto independent = values.clone();
  std::size_t visits = 0;
  independent.for_each([&](double) { ++visits; });
  values.clear();
  check(independent.size() == 3 && values.empty() && visits == 3,
        "set clone and for_each preserve independent ordered storage");
  const auto captured_set = independent;
  captured_set.add(8.0);
  check(independent.has(8.0), "const set handles retain mutable JavaScript referent semantics");
}

void test_string() {
  const auto emoji = flight::String::from_utf8("\xF0\x9F\x98\x80");
  check(emoji.length() == 2, "string length counts UTF-16 code units");
  check(emoji.to_utf8() == "\xF0\x9F\x98\x80", "valid supplementary code points round-trip through UTF-8");
  check(emoji.at(-1).has_value(), "negative string indexing addresses code units from the end");
  check(emoji.slice(0, 1).to_utf8() == "\xEF\xBF\xBD",
        "an isolated surrogate encodes as the Unicode replacement character");

  const flight::String text(u"\u00A0 one,two \uFEFF");
  const auto pieces = text.trim().split(",");
  check(pieces.size() == 2 && pieces[0] == flight::String("one") && pieces[1] == flight::String("two"),
        "trim and split use TypeScript string boundaries");
  check(flight::String("prefix").starts_with("pre") && flight::String("prefix").ends_with("fix"),
        "string prefix and suffix checks accept UTF-8 boundaries");
  check(flight::String("ABC").to_lower() == flight::String("abc"),
        "ASCII case conversion is deterministic without a provider");
  check(flight::String("ab").pad_start(5, "01") == flight::String("010ab"),
        "string pad_start truncates repeated fill at UTF-16 boundaries");
  check(flight::String("ab").pad_end(5, "01") == flight::String("ab010"),
        "string pad_end truncates repeated fill at UTF-16 boundaries");
  check(flight::String("ab").repeat(3) == flight::String("ababab"),
        "string repeat uses code-unit-preserving concatenation");
  check(flight::String("flight").substring(4, 1) == flight::String("lig"),
        "string substring clamps and swaps its boundaries");
  check(flight::String::from_char_code(65, 0xD83D, 0xDE00).length() == 3,
        "from_char_code preserves exact UTF-16 code units");
  check(flight::String::from_code_point(65, 0x1F600) == flight::String::from_utf8("A\xF0\x9F\x98\x80"),
        "from_code_point encodes basic and astral Unicode scalars");
  bool invalid_code_point = false;
  try {
    static_cast<void>(flight::String::from_code_point(0xD800));
  } catch (const std::range_error&) {
    invalid_code_point = true;
  }
  check(invalid_code_point, "from_code_point rejects surrogate values");
  check(flight::to_string(true) == flight::String("true") &&
            flight::String::from_number(-0.0) == flight::String("0") &&
            flight::String::from_number(1.0e-6) == flight::String("0.000001") &&
            flight::String::from_number(1.0e21) == flight::String("1e+21"),
        "source string coercion handles booleans and numeric format boundaries");

  bool provider_required = false;
  try {
    static_cast<void>(flight::String(u"\u00C4").to_lower());
  } catch (const flight::UnicodeServiceUnavailable&) {
    provider_required = true;
  }
  check(provider_required, "non-ASCII case conversion refuses a missing Unicode provider");

  const auto service = std::make_shared<TestUnicodeService>();
  {
    const flight::UnicodeServiceScope scope(service);
    check(flight::String(u"\u00C4").to_lower() == flight::String(u"\u00E4"),
          "Unicode case conversion delegates to the configured provider");
  }
}

void test_streams() {
  flight::Array<double> written;
  bool closed = false;
  flight::WritableStream<double> writable({
      .write = [&](const double& value) {
        written.push(value);
        return FlightTask<void>::resolve();
      },
      .close = [&] {
        closed = true;
        return FlightTask<void>::resolve();
      },
      .abort = {},
  });
  auto writer = writable.get_writer();
  writer.write(2.0).get();
  writer.write(3.0).get();
  bool duplicate_writer_failed = false;
  try {
    static_cast<void>(writable.get_writer());
  } catch (const std::logic_error&) {
    duplicate_writer_failed = true;
  }
  writer.close().get();
  check(written.size() == 2 && written[0] == 2.0 && written[1] == 3.0 &&
            closed && duplicate_writer_failed &&
            writer.write(4.0).status() == flight::TaskStatus::rejected,
        "WritableStream serializes sink calls, locks one writer, and rejects writes after close");
  writer.release_lock();
  static_cast<void>(writable.get_writer());

  int reads = 0;
  bool cancelled = false;
  flight::ReadableStream<double> readable({
      .read = [&] {
        ++reads;
        return FlightTask<flight::ReadableStreamReadResult<double>>::resolve(
            reads == 1 ? flight::ReadableStreamReadResult<double>{.done = false, .value = 7.0}
                       : flight::ReadableStreamReadResult<double>{.done = true, .value = std::nullopt});
      },
      .cancel = [&](flight::AbortReason) {
        cancelled = true;
        return FlightTask<void>::resolve();
      },
  });
  auto reader = readable.get_reader();
  const auto first = reader.read().get();
  const auto second = reader.read().get();
  reader.cancel(flight::AbortReason("done")).get();
  check(!first.done && first.value == std::optional<double>(7.0) && second.done &&
            !second.value.has_value() && cancelled,
        "ReadableStream returns presence-bearing reads and forwards cancellation");

  int next = 0;
  const flight::AsyncIterable<double> iterable([&] {
    return FlightTask<std::optional<double>>::resolve(
        next++ == 0 ? std::optional<double>(9.0) : std::nullopt);
  });
  check(iterable.next().get() == std::optional<double>(9.0) &&
            !iterable.next().get().has_value(),
        "AsyncIterable preserves a shared asynchronous next operation");
}

void test_typed_array() {
  flight::Int16Array values{1, 2, 3};
  auto alias = values;
  check(alias == values && values.subarray(0) != values && values.slice(0) != values,
        "typed-array copies preserve identity while views and slices create objects");
  auto view = values.subarray(1);
  view[0] = 7;
  check(values[1] == 7 && view.size() == 2, "typed-array subarray shares its backing storage");
  auto copy = values.slice(1);
  copy[0] = 9;
  check(values[1] == 7 && copy[0] == 9, "typed-array slice copies its selected range");
  check(values.at(-1) == std::optional<std::int16_t>(3), "typed-array negative access addresses the tail");
  view.set(flight::Int16Array{8, 9});
  check(values[1] == 8 && values[2] == 9, "typed-array set snapshots and writes through a shared view");

  const flight::Uint8ClampedArray clamped{-1.0, 0.5, 1.5, 2.5, 300.0};
  check(static_cast<std::uint8_t>(clamped[0]) == 0 &&
            static_cast<std::uint8_t>(clamped[1]) == 0 &&
            static_cast<std::uint8_t>(clamped[2]) == 2 &&
            static_cast<std::uint8_t>(clamped[3]) == 2 &&
            static_cast<std::uint8_t>(clamped[4]) == 255,
        "Uint8ClampedArray uses saturating ties-to-even conversion");

  const flight::Array<double> source{-1.0, 4294967297.0,
                                     std::numeric_limits<double>::quiet_NaN()};
  const auto unsigned_values = flight::Uint32Array::from(source);
  const auto signed_values = flight::Int8Array::from(
      flight::Array<double>{127.0, 128.0, 255.0, 256.0, -129.0});
  const auto transformed = flight::Uint16Array::from(
      flight::Array<double>{1.0, 2.0}, [](double value, double index) {
        return value + index + 65535.0;
      });
  check(unsigned_values.size() == 3 && unsigned_values[0] == 4294967295U &&
            unsigned_values[1] == 1U && unsigned_values[2] == 0U,
        "typed-array from applies JavaScript unsigned modulo conversion");
  check(signed_values.size() == 5 && signed_values[0] == 127 && signed_values[1] == -128 &&
            signed_values[2] == -1 && signed_values[3] == 0 && signed_values[4] == 127,
        "typed-array from applies JavaScript signed modulo conversion");
  check(transformed.size() == 2 && transformed[0] == 0 && transformed[1] == 2,
        "typed-array from invokes its mapper before element conversion");

  const flight::ArrayBuffer buffer(8.0);
  const flight::DataView data_view(buffer);
  const std::variant<flight::ArrayBuffer, flight::Uint8Array> source_variant =
      flight::Uint8Array(buffer);
  check(!flight::is_array_buffer_view(buffer) && flight::is_array_buffer_view(data_view) &&
            flight::is_array_buffer_view(values) &&
            flight::is_array_buffer_view(source_variant),
        "ArrayBuffer.isView distinguishes buffers from typed and data views");
}

void test_uri_components() {
  const auto source = flight::String::from_utf8("flight /?=&# \xF0\x9F\x98\x80");
  const auto encoded = flight::encode_uri_component(source);
  check(encoded == flight::String("flight%20%2F%3F%3D%26%23%20%F0%9F%98%80") &&
            flight::decode_uri_component(encoded) == source,
        "URI component encoding preserves its unescaped set and UTF-8 round trip");
  check(flight::encode_uri_component("AZaz09-_.!~*'()") == flight::String("AZaz09-_.!~*'()") &&
            flight::decode_uri_component("literal-%2f-%00") ==
                flight::String(std::u16string{u'l', u'i', u't', u'e', u'r', u'a', u'l', u'-', u'/', u'-', 0}),
        "URI component operations preserve safe characters and decode hexadecimal bytes");

  bool invalid_encoding_failed = false;
  try {
    static_cast<void>(flight::decode_uri_component("%E0%A4%A"));
  } catch (const flight::UriError&) {
    invalid_encoding_failed = true;
  }
  bool unpaired_surrogate_failed = false;
  try {
    static_cast<void>(flight::encode_uri_component(flight::String(std::u16string{0xD800})));
  } catch (const flight::UriError&) {
    unpaired_surrogate_failed = true;
  }
  check(invalid_encoding_failed && unpaired_surrogate_failed,
        "URI component operations reject malformed percent UTF-8 and unpaired surrogates");
}

void test_web_types() {
  flight::WebTextMetrics metrics;
  metrics.width = 12.0;
  metrics.actual_bounding_box_ascent = 8.0;
  const auto alias = metrics;
  const flight::WebTextMetrics distinct;
  check(alias == metrics && alias.identity() == metrics.identity() && distinct != metrics &&
            alias.width == 12.0 && alias.actual_bounding_box_ascent == 8.0,
        "Web TextMetrics copies preserve provider result identity and measured values");

  const flight::WebImageEncodeOptions encode{
      .quality = 0.75,
      .type = flight::String("image/webp"),
  };
  const flight::WebPositionOptions position{
      .enable_high_accuracy = true,
      .maximum_age = std::nullopt,
      .timeout = 500.0,
  };
  check(encode.quality == 0.75 && encode.type == flight::String("image/webp") &&
            position.enable_high_accuracy == true && !position.maximum_age.has_value() &&
            position.timeout == 500.0,
        "portable Web dictionaries preserve optional member presence");
}

void test_task() {
  const auto source = FlightTask<int>::ready(21);
  check(source.is_ready() && source.get() == 21, "ready task exposes its settled value");
  check(doubled(source).get() == 42, "task composes through co_await");
  check(reuse(source).get() == 42, "settled task supports repeated await");

  const auto joined = FlightTask<int>::join_all(
      std::vector<FlightTask<int>>{FlightTask<int>::ready(2), FlightTask<int>::ready(3)});
  check(joined.get() == std::vector<int>({2, 3}), "join_all preserves task order");

  bool flag = false;
  const auto completion = set_flag(flag);
  completion.get();
  check(flag, "void task completes normally");

  bool rejected = false;
  try {
    FlightTask<int>::reject(std::make_exception_ptr(std::runtime_error("expected failure"))).get();
  } catch (const std::runtime_error& error) {
    rejected = std::string(error.what()) == "expected failure";
  }
  check(rejected, "reject preserves the exact exception");

  bool thrown = false;
  try {
    fails().get();
  } catch (const std::runtime_error& error) {
    thrown = std::string(error.what()) == "expected failure";
  }
  check(thrown, "coroutine exceptions settle the task as rejected");

  const auto value_rejection = FlightTask<int>::reject(std::string("reason"));
  const auto settlement = value_rejection.settle();
  check(settlement.status == flight::TaskStatus::rejected && settlement.rejection->is<std::string>(),
        "task settlement retains a non-exception rejection value");

  const auto recovered = value_rejection.catch_error([](const flight::Rejection& reason) {
    return static_cast<int>(reason.as<std::string>().size());
  });
  check(recovered.get() == 6, "catch_error recovers an exact rejection value");

  bool continuation_called = false;
  const auto continued = source.then([&](int value) {
    continuation_called = true;
    return FlightTask<int>::ready(value + 1);
  });
  check(!continuation_called, "task continuations are queued rather than invoked inline");
  check(continued.get() == 22 && continuation_called, "then assimilates a returned task");

  bool cleanup_called = false;
  const auto finalized = continued.finally([&] {
    cleanup_called = true;
    return FlightTask<void>::ready();
  });
  check(finalized.get() == 22 && cleanup_called, "finally awaits cleanup and preserves fulfillment");

  const auto assimilated = FlightTask<int>::create([](auto resolve, auto reject) {
    static_cast<void>(reject);
    resolve(FlightTask<int>::ready(12));
  });
  check(assimilated.get() == 12, "task resolver assimilates another task");

  const auto resolve_only = FlightTask<int>::create([](auto resolve) { resolve(13); });
  check(resolve_only.get() == 13, "task construction accepts an omitted reject callback");

  check(flight::resolve_task(14).get() == 14 && flight::resolve_task<int>(15).get() == 15,
        "Promise resolve helpers support inferred and explicit result types");
  const auto all = flight::all_tasks(flight::Array{FlightTask<int>::ready(3), FlightTask<int>::ready(4)}).get();
  check(all.size() == 2 && all[0] == 3 && all[1] == 4,
        "Promise all helper bridges semantic arrays and tasks");

  const auto all_settled = flight::all_settled_tasks(flight::Array{
      FlightTask<int>::ready(3), FlightTask<int>::reject(flight::String("bad")),
      FlightTask<int>::ready(4)}).get();
  check(all_settled.size() == 3 &&
            all_settled[0].status == flight::TaskStatus::fulfilled &&
            all_settled[0].value == std::optional<int>(3) &&
            all_settled[1].status == flight::TaskStatus::rejected &&
            all_settled[1].rejection->as<flight::String>() == flight::String("bad") &&
            all_settled[2].status == flight::TaskStatus::fulfilled &&
            all_settled[2].value == std::optional<int>(4),
        "Promise allSettled helper preserves source order and exact rejection values");

  const auto all_settled_void = FlightTask<void>::all_settled(
      std::vector<FlightTask<void>>{FlightTask<void>::ready(),
                                    FlightTask<void>::reject(flight::String("void bad"))}).get();
  check(all_settled_void.size() == 2 &&
            all_settled_void[0].status == flight::TaskStatus::fulfilled &&
            all_settled_void[1].status == flight::TaskStatus::rejected &&
            all_settled_void[1].rejection->as<flight::String>() == flight::String("void bad"),
        "Promise allSettled supports void tasks without rejecting the aggregate");

  const auto preserved_rejection = value_rejection.finally([] {});
  const auto preserved_settlement = preserved_rejection.settle();
  check(preserved_settlement.status == flight::TaskStatus::rejected &&
            preserved_settlement.rejection->is<std::string>(),
        "finally preserves an existing rejection");

  const auto cleanup_rejection = source.finally([] {
    return FlightTask<void>::reject(std::string("cleanup"));
  });
  const auto cleanup_settlement = cleanup_rejection.settle();
  check(cleanup_settlement.status == flight::TaskStatus::rejected &&
            cleanup_settlement.rejection->as<std::string>() == "cleanup",
        "finally replaces fulfillment with a cleanup rejection");

  const auto executor = std::make_shared<flight::QueueExecutor>();
  bool initializer_called = false;
  const auto created = FlightTask<int>::create(
      [&](auto resolve, auto reject) {
        static_cast<void>(reject);
        initializer_called = true;
        executor->post([resolve] {
          resolve(7);
          resolve(9);
        });
      },
      executor);
  check(initializer_called && created.status() == flight::TaskStatus::pending,
        "task executor runs synchronously while settlement remains queued");
  check(executor->run_until_idle() == 1 && created.get() == 7,
        "first settlement wins for executor-created tasks");
}

} // namespace

int main() {
  test_array();
  test_array_buffer_like();
  test_array_like_views();
  test_audio_buffer();
  test_binary_data();
  test_base64();
  test_blob();
  test_boolean_conversion();
  test_contract();
  test_date();
  test_error();
  test_host();
  test_number_to_fixed();
  test_any_domain();
  test_attached_properties();
  test_structured_clone();
  test_settled_task_arms();
  test_any_equality_and_absence();
  test_any_conversion_and_ordering();
  test_canvas_2d_state();
  test_canvas_2d_path();
  test_canvas_2d_styles();
  test_canvas_2d_drawing();
  test_canvas_2d_unattached();
  test_image_data();
  test_map();
  test_new_runtime_services();
  test_presence_and_math();
  test_record();
  test_reference();
  test_set();
  test_string();
  test_streams();
  test_task();
  test_typed_array();
  test_uri_components();
  test_web_types();
  return failures == 0 ? 0 : 1;
}
