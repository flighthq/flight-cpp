// The generated @flighthq/skeleton2d-formats Spine binary surface that compiles at a627849, exercised
// natively: the header version accessor and its diagnosis, the section census and the requirement set
// built on it, the parse helpers, the stub section and timeline handlers that consume what the importer
// does not model, and the shared draw-order resolver. Ported from the package's own tests where they
// assert behaviour (spineBinaryVersion, spineBinarySectionCounts, spineBinaryRequirements, spineDrawOrder);
// where they only assert that an export exists (spineBinaryParseHelpers, spineBinaryStubHandlers,
// spineBinaryStubTimelineHandlers), compiling is that check, and the cases here state the byte layout each
// one consumes, the diagnostic or tally it reports, and where it stops on malformed input.
#include <flight/skeleton2d_formats/spine_binary_parse_helpers.hpp>
#include <flight/skeleton2d_formats/spine_binary_registry.hpp>
#include <flight/skeleton2d_formats/spine_binary_requirements.hpp>
#include <flight/skeleton2d_formats/spine_binary_section_counts.hpp>
#include <flight/skeleton2d_formats/spine_binary_stub_handlers.hpp>
#include <flight/skeleton2d_formats/spine_binary_stub_timeline_handlers.hpp>
#include <flight/skeleton2d_formats/spine_binary_version.hpp>
#include <flight/skeleton2d_formats/spine_draw_order.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

namespace spine = flight::skeleton2d_formats;
namespace types = flight::types;
using Bytes = std::vector<std::uint8_t>;

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

flight::Uint8Array u8(const Bytes& bytes) { return flight::Uint8Array(bytes); }

Bytes cat(std::initializer_list<Bytes> parts) {
  Bytes out;
  for (const auto& part : parts) out.insert(out.end(), part.begin(), part.end());
  return out;
}

Bytes zeros(std::size_t count) { return Bytes(count, 0); }

// A Spine varint: 7 payload bits per byte, high bit set while more follow.
Bytes varint(std::uint32_t value) {
  Bytes out;
  while (value > 0x7f) {
    out.push_back(static_cast<std::uint8_t>((value & 0x7f) | 0x80));
    value >>= 7;
  }
  out.push_back(static_cast<std::uint8_t>(value));
  return out;
}

// Spine writes a string as varint(byteCount + 1) then the bytes; 0 means null.
Bytes spine_string(const std::string& value) {
  return cat({varint(static_cast<std::uint32_t>(value.size() + 1)), Bytes(value.begin(), value.end())});
}

const Bytes v4_hash{0x8a, 0xd7, 0xc5, 0x11, 0x20, 0xe3, 0x33, 0x57};

Bytes spine38_header(const std::string& version, const std::string& hash = "aBcDeFgHiJkLmNoPqRsTuVwXyZ0") {
  return cat({spine_string(hash), spine_string(version), zeros(4)});
}

Bytes spine4x_header(const std::string& version) { return cat({v4_hash, spine_string(version), zeros(4)}); }

// The census header: hash, version, bounds, nonessential flag, string table; then the section counts.
Bytes skel_head(const std::string& version) { return cat({v4_hash, spine_string(version), zeros(16), {0}, varint(0)}); }

Bytes minimal_skel(const std::string& version) {
  Bytes out = skel_head(version);
  for (int i = 0; i < 9; ++i) out = cat({out, varint(0)});
  return out;
}

Bytes skel_with_bones(const std::string& version, std::uint32_t count) {
  Bytes out = cat({skel_head(version), varint(count)});
  for (std::uint32_t i = 0; i < count; ++i) {
    out = cat({out, spine_string("bone" + std::to_string(i)), i > 0 ? varint(0) : Bytes{}, zeros(32), varint(0), {0}});
  }
  for (int i = 0; i < 8; ++i) out = cat({out, varint(0)});
  return out;
}

Bytes skel_with_slots(const std::string& version, std::uint32_t count) {
  Bytes out = cat({skel_head(version), varint(0), varint(count)});
  for (std::uint32_t i = 0; i < count; ++i) {
    out = cat({out, spine_string("slot" + std::to_string(i)), varint(0), zeros(8), varint(0), varint(0)});
  }
  for (int i = 0; i < 7; ++i) out = cat({out, varint(0)});
  return out;
}

Bytes skel_with_events(const std::string& version, std::uint32_t count) {
  Bytes out = skel_head(version);
  for (int i = 0; i < 7; ++i) out = cat({out, varint(0)});
  out = cat({out, varint(count)});
  for (std::uint32_t i = 0; i < count; ++i) out = cat({out, varint(0), varint(0), zeros(4), {0}, {0}});
  return cat({out, varint(0)});
}

// Two animations. The first carries an attachment and an RGBA slot timeline, an IK timeline and a deform
// timeline, each with two frames so every per-frame skip and last-frame exit runs; the second carries a
// draw-order and an event timeline. A wrong skip anywhere in the first misreads the second.
Bytes skel_with_animations() {
  Bytes out = skel_head("4.1.0");
  for (int i = 0; i < 8; ++i) out = cat({out, varint(0)});  // bones .. skins, events
  out = cat({out, varint(2)});
  out = cat({out, spine_string("a"), varint(0),
             // slot timelines: one slot, two timelines
             varint(1), varint(0), varint(2),
             {0}, varint(2), zeros(4), varint(0), zeros(4), varint(0),     // attachment: 4 bytes + varint per frame
             {1}, varint(2), varint(0), zeros(8), zeros(8), {0},            // rgba: curve frames
             varint(0),                                                     // bone timelines
             varint(1), varint(0), varint(2), varint(0), zeros(12),         // ik: one timeline, two frames
             zeros(3), zeros(12), {0}, zeros(3),
             varint(0), varint(0),                                          // transform, path
             varint(1), varint(0), varint(1), varint(0), varint(1),         // deform: skin, slot, attachment
             varint(0), {0}, varint(2), varint(0), zeros(4),
             varint(1), varint(0), zeros(4), zeros(4), {0}, varint(0),
             varint(0), varint(0)});                                        // draw order, events
  out = cat({out, spine_string("b"), varint(0), varint(0), varint(0), varint(0), varint(0), varint(0), varint(0),
             varint(1), zeros(4), varint(1), varint(0), varint(0),           // draw order: one frame, one offset
             varint(1), zeros(4), varint(0), varint(0), zeros(4), {0}});     // events: one frame
  return out;
}

bool is_null(const std::optional<flight::String>& value) { return !value.has_value(); }
bool is(const std::optional<flight::String>& value, const char* text) {
  return value.has_value() && value.value() == flight::String(text);
}

// --- spineBinaryVersion ---------------------------------------------------------------------------------

void version_failure() {
  check(spine::explain_spine_binary_version_failure(u8({}))->reason == flight::String("too-short"), "empty is too-short");
  auto four = spine::explain_spine_binary_version_failure(u8(zeros(4)));
  check(four->bytes == 4 && four->reason == flight::String("too-short"), "four bytes is too-short and reports its length");

  auto noise = spine::explain_spine_binary_version_failure(
      u8({0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4}));
  check(noise->reason == flight::String("no-strategy-matched") && is_null(noise->v3_candidate) &&
            is_null(noise->v4_candidate),
        "noise matches no strategy and both candidates are null");

  auto readable = spine::explain_spine_binary_version_failure(u8(spine4x_header("4.1.17")));
  check(is_null(readable->v3_candidate) && is(readable->v4_candidate, "4.1.17"), "explain names the strategy that read it");
}

void version_accessor() {
  check(is(spine::get_spine_binary_version(u8(spine4x_header("4.1.17"))), "4.1.17"), "reads a 4.1.17 header");
  check(is(spine::get_spine_binary_version(u8(spine4x_header("4.2.22"))), "4.2.22"), "reads a 4.2.22 header");
  check(is(spine::get_spine_binary_version(u8(spine38_header("3.8.55"))), "3.8.55"), "reads a 3.8 header");

  check(is_null(spine::explain_spine_binary_version_failure(u8(spine4x_header("4.1.17")))->v3_candidate),
        "the 3.x strategy declines a 4.x header");
  check(is_null(spine::explain_spine_binary_version_failure(u8(spine38_header("3.8.55")))->v4_candidate),
        "the 4.x strategy declines a 3.8 header");

  // Upstream's guard: the case is asserted only when both strategies really do produce a candidate.
  Bytes ambiguous = cat({spine_string("1.0"), spine_string("2.0"), zeros(4)});
  auto explanation = spine::explain_spine_binary_version_failure(u8(ambiguous));
  if (explanation->v3_candidate.has_value() && explanation->v4_candidate.has_value()) {
    check(explanation->reason == flight::String("strategies-disagree"), "two candidates is strategies-disagree");
    check(is_null(spine::get_spine_binary_version(u8(ambiguous))), "the accessor refuses to pick a winner");
  }
  // That fixture never reaches the guarded branch: its 4.x read lands on a zero. A seven-character hash puts
  // the version string exactly at offset 8, so both strategies read it, and the branch is exercised.
  Bytes both = spine38_header("2.0", "abcdefg");
  auto disagree = spine::explain_spine_binary_version_failure(u8(both));
  check(is(disagree->v3_candidate, "2.0") && is(disagree->v4_candidate, "2.0") &&
            disagree->reason == flight::String("strategies-disagree"),
        "a header both strategies read is strategies-disagree");
  check(is_null(spine::get_spine_binary_version(u8(both))), "and the accessor returns null rather than a winner");

  check(is_null(spine::get_spine_binary_version(u8(spine4x_header("not-a-version")))), "not version-shaped");
  check(is_null(spine::get_spine_binary_version(u8(spine4x_header("4")))), "a bare major is not version-shaped");
  check(is_null(spine::get_spine_binary_version(u8(spine4x_header("x4.1.17")))), "the pattern is anchored at the start");
  check(is_null(spine::get_spine_binary_version(u8(spine4x_header("4.1.17x")))), "the pattern is anchored at the end");

  Bytes truncated = spine4x_header("4.1.17");
  truncated.resize(9);
  check(is_null(spine::get_spine_binary_version(u8(truncated))), "a truncated file is declined");

  // The 10-byte floor: the shortest 4.x header ("1.0" needs 8 + 1 + 3 = 12) is read, and one byte under
  // the floor is too-short even when the bytes would otherwise read.
  Bytes shortest = cat({v4_hash, spine_string("1.0")});
  check(is(spine::get_spine_binary_version(u8(shortest)), "1.0"), "a header with nothing after the version is read");
  check(spine::explain_spine_binary_version_failure(u8(zeros(9)))->reason == flight::String("too-short") &&
            spine::explain_spine_binary_version_failure(u8(zeros(10)))->reason == flight::String("no-strategy-matched"),
        "the minimum header is ten bytes");

  // The 3.x hash must be printable ASCII; a control character or an empty hash declines the 3.x strategy.
  check(is_null(spine::explain_spine_binary_version_failure(u8(spine38_header("3.8.55", "abc\x1f" "def")))->v3_candidate),
        "a non-printable 3.x hash declines");
  check(is_null(spine::explain_spine_binary_version_failure(u8(spine38_header("3.8.55", "")))->v3_candidate),
        "an empty 3.x hash declines");
  check(is(spine::explain_spine_binary_version_failure(u8(spine38_header("3.8.55", " ~")))->v3_candidate, "3.8.55"),
        "space and tilde bound the printable range");
}

// --- spineBinarySectionCounts / spineBinaryRequirements -------------------------------------------------

void section_counts() {
  check(!spine::collect_spine_binary_section_counts(u8({})).has_value(), "empty is unreadable");
  check(!spine::collect_spine_binary_section_counts(u8(zeros(4))).has_value(), "too short is unreadable");
  check(!spine::collect_spine_binary_section_counts(
             u8({0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4}))
             .has_value(),
        "noise is unreadable");
  check(!spine::collect_spine_binary_section_counts(u8(minimal_skel("3.8.55"))).has_value(), "3.8 is unsupported");
  check(!spine::collect_spine_binary_section_counts(u8(minimal_skel("4.2.0"))).has_value(),
        "4.2 is not the census layout either");

  auto empty = spine::collect_spine_binary_section_counts(u8(minimal_skel("4.1.0")));
  check(empty.has_value() && empty->size() == 0, "a readable empty file is an empty map, not null");

  Bytes truncated = minimal_skel("4.1.0");
  truncated.resize(12);
  check(!spine::collect_spine_binary_section_counts(u8(truncated)).has_value(), "a file cut mid-header is unreadable");

  auto bones = spine::collect_spine_binary_section_counts(u8(skel_with_bones("4.1.0", 3)));
  check(bones.has_value() && bones->get(types::spine_binary_section_kind->bones) == 3.0, "counts three bones");
  check(bones.has_value() && !bones->has(types::spine_binary_section_kind->slots) &&
            !bones->has(types::spine_binary_section_kind->animations),
        "absent kinds are omitted");

  auto slots = spine::collect_spine_binary_section_counts(u8(skel_with_slots("4.1.0", 2)));
  check(slots.has_value() && slots->get(types::spine_binary_section_kind->slots) == 2.0, "counts two slots");

  auto events = spine::collect_spine_binary_section_counts(u8(skel_with_events("4.1.0", 1)));
  check(events.has_value() && events->get(types::spine_binary_section_kind->events) == 1.0, "counts one event");

  auto animated = spine::collect_spine_binary_section_counts(u8(skel_with_animations()));
  const auto& kind = types::spine_binary_timeline_kind;
  check(animated.has_value() && animated->get(types::spine_binary_section_kind->animations) == 2.0,
        "counts two animations");
  check(animated.has_value() && animated->get(kind->slot) == 2.0 && animated->get(kind->ik) == 1.0 &&
            animated->get(kind->deform) == 1.0 && animated->get(kind->draw_order) == 1.0 &&
            animated->get(kind->event) == 1.0 && !animated->has(kind->bone) && animated->size() == 6,
        "tallies every timeline kind across both animations, and only those");

  // The census checks for overrun only through the header. A section cut short past it still yields the
  // counts declared before the cut, and the sections after it read as empty -- the source's behaviour,
  // pinned here so a change to it is a deliberate one.
  Bytes cut = skel_with_bones("4.1.0", 3);
  cut.resize(cut.size() - 20);
  auto partial = spine::collect_spine_binary_section_counts(u8(cut));
  check(partial.has_value() && partial->size() == 1 && partial->get(types::spine_binary_section_kind->bones) == 3.0,
        "a census cut inside the bones keeps the declared bone count and nothing after it");
}

void requirements() {
  check(!spine::is_readable_spine_binary(u8({})), "empty is not readable");
  check(spine::is_readable_spine_binary(u8(minimal_skel("4.1.0"))), "a minimal 4.1 file is readable");
  check(!spine::is_readable_spine_binary(u8(minimal_skel("3.8.55"))), "3.8 is not readable");

  check(spine::parse_spine_binary_requirements(u8({}))->requirements.size() == 0, "an unreadable file requires nothing");
  check(spine::parse_spine_binary_requirements(u8(minimal_skel("4.1.0")))->requirements.size() == 0,
        "an empty file requires nothing");

  auto two = spine::parse_spine_binary_requirements(u8(skel_with_bones("4.1.0", 2)));
  check(two->requirements.size() == 1 && two->requirements.element(0)->key == flight::String("spine-binary.bones"),
        "one namespaced key per present kind, not per element");
  check(two->requirements.element(0)->facet == types::requirement_facet->document_format, "the DocumentFormat facet");
  check(two->covers.size() == 1 && two->covers.element(0) == types::requirement_facet->document_format,
        "the set covers DocumentFormat");

  auto one = spine::parse_spine_binary_requirements(u8(skel_with_bones("4.1.0", 1)));
  bool none_absent = true;
  for (std::size_t i = 0; i < one->requirements.size(); ++i) {
    const auto key = one->requirements.element(static_cast<double>(i))->key;
    if (key == flight::String("spine-binary.slots") || key == flight::String("spine-binary.animations")) none_absent = false;
  }
  check(none_absent, "absent kinds are not required");
  check(spine::spine_binary_requirement_key_namespace == flight::String("spine-binary"), "the key namespace");
}

// --- spineBinaryParseHelpers ----------------------------------------------------------------------------

void parse_helpers() {
  check(spine::clamp_spine_binary_unit(-0.5) == 0 && spine::clamp_spine_binary_unit(0) == 0 &&
            spine::clamp_spine_binary_unit(0.25) == 0.25 && spine::clamp_spine_binary_unit(1) == 1 &&
            spine::clamp_spine_binary_unit(1.5) == 1,
        "clamp to the unit interval");

  // References are 1-based into the string table; 0 and anything past the end are null, and a table slot
  // that itself holds null reads as null.
  flight::Array<std::optional<flight::String>> strings{flight::String("a"), std::nullopt, flight::String("c")};
  auto refs = spine::create_spine_binary_reader(u8({0, 1, 2, 3, 4}));
  check(is_null(spine::read_spine_binary_string_reference(refs, strings)), "reference 0 is null");
  check(is(spine::read_spine_binary_string_reference(refs, strings), "a"), "reference 1 is the first entry");
  check(is_null(spine::read_spine_binary_string_reference(refs, strings)), "a null table entry reads null");
  check(is(spine::read_spine_binary_string_reference(refs, strings), "c"), "reference 3 is the last entry");
  check(is_null(spine::read_spine_binary_string_reference(refs, strings)), "a reference past the table is null");

  flight::Array<flight::Ref<types::ImportDiagnostic>> sink;
  spine::report_spine_binary_crumb(sink, 0, flight::String("k"), flight::String("o"), flight::String("u"));
  check(sink.size() == 0, "a zero count reports nothing");
  spine::report_spine_binary_crumb(sink, 3, flight::String("spine.k"), flight::String("origin"), flight::String("units"));
  check(sink.size() == 1, "a positive count reports one crumb into the caller's array");
  if (sink.size() == 1) {
    auto crumb = sink.element(0);
    check(crumb->severity == types::import_diagnostic_severity->skip && crumb->kind == flight::String("spine.k") &&
              crumb->origin == flight::String("origin"),
          "the crumb is a skip with its kind and origin");
    auto unit = crumb->detail.has_value() ? crumb->detail->get(flight::String("units")) : std::nullopt;
    check(unit.has_value() && std::get<double>(unit.value()) == 3, "the detail names the unit and count");
  }
  spine::report_spine_binary_crumb(std::nullopt, 3, flight::String("k"), flight::String("o"), flight::String("u"));

  flight::Map<flight::String, double> counts;
  spine::tally(counts, flight::String("b"));
  spine::tally(counts, flight::String("a"));
  spine::tally(counts, flight::String("b"));
  check(counts.get(flight::String("b")) == 2.0 && counts.get(flight::String("a")) == 1.0, "tally counts per kind");
  check(*counts.keys().begin() == flight::String("b"), "tally keeps first-seen order");

  // A curve tag is one byte; only BEZIER (2) carries curveValues * 16 bytes of handles.
  auto linear = spine::create_spine_binary_reader(u8({0, 1, 0xaa}));
  spine::skip_spine_binary_curve_tag(linear, 3);
  spine::skip_spine_binary_curve_tag(linear, 3);
  check(linear->offset == 2, "linear and stepped tags carry no handles");
  auto bezier = spine::create_spine_binary_reader(u8(cat({{2}, zeros(32), {0x7e}})));
  spine::skip_spine_binary_curve_tag(bezier, 2);
  check(spine::read_spine_binary_byte(bezier) == 0x7e, "a bezier tag skips curveValues * 16 bytes");

  // Frames: the first is time plus payload; each later frame is time plus payload followed by a curve tag.
  auto none = spine::create_spine_binary_reader(u8({1, 2, 3}));
  spine::skip_spine_binary_curve_frames(none, 0, 8, 1);
  check(none->offset == 0, "zero frames consume nothing");
  auto single = spine::create_spine_binary_reader(u8(zeros(20)));
  spine::skip_spine_binary_curve_frames(single, 1, 8, 1);
  check(single->offset == 12, "one frame is time plus payload, with no curve tag");
  auto two = spine::create_spine_binary_reader(u8(cat({zeros(12), zeros(12), {2}, zeros(16), {0x55}})));
  spine::skip_spine_binary_curve_frames(two, 2, 8, 1);
  check(spine::read_spine_binary_byte(two) == 0x55 && !spine::is_spine_binary_reader_overrun(view(two)),
        "a second frame is time, payload, then its curve tag");
  auto cut = spine::create_spine_binary_reader(u8(zeros(5)));
  spine::skip_spine_binary_curve_frames(cut, 1000, 8, 1);
  check(spine::is_spine_binary_reader_overrun(view(cut)) && cut->offset <= 5 + 12,
        "a frame count past the data stops at the first overrun");

  check(spine::spine_binary_attachment_types.size() == 7 &&
            spine::spine_binary_attachment_types.element(0) == flight::String("region") &&
            spine::spine_binary_attachment_types.element(2) == flight::String("mesh") &&
            spine::spine_binary_attachment_types.element(6) == flight::String("clipping"),
        "attachment types are indexed in Spine's order");
  check(spine::spine_binary_curve_bezier == 2 && spine::spine_binary_attachment_sequence == 1 &&
            spine::spine_binary_path_mix == 2 && spine::spine_binary_hash_bytes == 8 &&
            spine::spine_binary_default_skin_name == flight::String("default"),
        "format constants");
}

// --- spineBinaryStubHandlers ----------------------------------------------------------------------------

struct Section {
  flight::Ref<types::SpineBinarySectionContext> context;
  flight::Array<flight::Ref<types::ImportDiagnostic>> diagnostics;
};

Section section(const Bytes& bytes, bool collect = true) {
  Section out;
  out.context = flight::make_ref<types::SpineBinarySectionContext>(types::SpineBinarySectionContext{
      .diagnostics = collect ? std::optional{out.diagnostics} : std::nullopt,
      .nonessential = false,
      .reader = spine::create_spine_binary_reader(u8(bytes)),
      .registry = view(spine::create_spine_binary_registry()),
  });
  return out;
}

bool clean_end(const Section& s, std::size_t length) {
  return s.context->reader->offset == static_cast<double>(length) &&
         !spine::is_spine_binary_reader_overrun(view(s.context->reader));
}

bool one_crumb(const Section& s, const char* kind, const char* origin, const char* unit, double count) {
  if (s.diagnostics.size() != 1) return false;
  auto crumb = s.diagnostics.element(0);
  auto value = crumb->detail.has_value() ? crumb->detail->get(flight::String(unit)) : std::nullopt;
  return crumb->kind == flight::String(kind) && crumb->origin == flight::String(origin) &&
         crumb->severity == types::import_diagnostic_severity->skip && value.has_value() &&
         std::get<double>(value.value()) == count;
}

// Events: name index, int, float bits, string, then an audio path whose presence adds volume and balance.
const Bytes two_events = cat({varint(2), varint(0), varint(0), zeros(4), {0}, {0}, varint(1), varint(0), zeros(4),
                              spine_string("n"), spine_string("a"), zeros(8)});
// IK: constraint head (name, order, skin flag, bone list), target, then 8 + 4 bytes of mix/softness/flags.
const Bytes one_ik = cat({varint(1), spine_string("ik"), varint(0), {1}, varint(2), varint(0), varint(1), varint(0),
                          zeros(12)});
const Bytes one_transform = cat({varint(1), {0}, varint(0), {0}, varint(0), varint(0), zeros(2), zeros(48)});
const Bytes one_path = cat({varint(1), {0}, varint(0), {0}, varint(1), varint(5), varint(0), varint(0), varint(0),
                            varint(0), zeros(24)});

void stub_section_handlers() {
  auto events = section(two_events);
  spine::spine_binary_events_section_handler(events.context);
  check(clean_end(events, two_events.size()), "events: consumed exactly, including the audio tail");
  check(one_crumb(events, "spine.event-unsupported", "skipSpineBinaryEvents", "events", 2), "events: one crumb of 2");

  auto ik = section(one_ik);
  spine::spine_binary_ik_constraints_section_handler(ik.context);
  check(clean_end(ik, one_ik.size()), "ik: head, bones, target and mix consumed exactly");
  check(one_crumb(ik, "spine.ik-constraint-unsupported", "skipSpineBinaryConstraints", "constraints", 1), "ik: crumb");

  auto transform = section(one_transform);
  spine::spine_binary_transform_constraints_section_handler(transform.context);
  check(clean_end(transform, one_transform.size()), "transform: 2 + 48 bytes after the head");
  check(one_crumb(transform, "spine.transform-constraint-unsupported", "skipSpineBinaryConstraints", "constraints", 1),
        "transform: crumb");

  auto path = section(one_path);
  spine::spine_binary_path_constraints_section_handler(path.context);
  check(clean_end(path, one_path.size()), "path: four varints and 24 bytes after the head");
  check(one_crumb(path, "spine.path-constraint-unsupported", "skipSpineBinaryConstraints", "constraints", 1),
        "path: crumb");

  // The skip variants consume the same bytes and report nothing; the reader aliases are the handlers.
  auto skipped = section(two_events);
  spine::skip_spine_binary_events_section(skipped.context);
  check(clean_end(skipped, two_events.size()) && skipped.diagnostics.size() == 0, "skip consumes silently");
  auto skipped_ik = section(one_ik);
  spine::skip_spine_binary_ik_constraints_section(skipped_ik.context);
  check(clean_end(skipped_ik, one_ik.size()) && skipped_ik.diagnostics.size() == 0, "ik skip consumes silently");
  auto skipped_transform = section(one_transform);
  spine::skip_spine_binary_transform_constraints_section(skipped_transform.context);
  check(clean_end(skipped_transform, one_transform.size()), "transform skip consumes the same bytes");
  auto skipped_path = section(one_path);
  spine::skip_spine_binary_path_constraints_section(skipped_path.context);
  check(clean_end(skipped_path, one_path.size()), "path skip consumes the same bytes");

  auto aliased = section(two_events);
  spine::spine_binary_events_section_reader(aliased.context);
  check(clean_end(aliased, two_events.size()) && aliased.diagnostics.size() == 1, "the events reader runs the handler");
  auto aliased_ik = section(one_ik);
  spine::spine_binary_ik_constraints_section_reader(aliased_ik.context);
  auto aliased_transform = section(one_transform);
  spine::spine_binary_transform_constraints_section_reader(aliased_transform.context);
  auto aliased_path = section(one_path);
  spine::spine_binary_path_constraints_section_reader(aliased_path.context);
  check(aliased_ik.diagnostics.size() == 1 && aliased_transform.diagnostics.size() == 1 &&
            aliased_path.diagnostics.size() == 1,
        "the constraint readers run their handlers");

  auto empty = section(varint(0));
  spine::spine_binary_events_section_handler(empty.context);
  check(clean_end(empty, 1) && empty.diagnostics.size() == 0, "an empty section reports nothing");

  auto unsunk = section(two_events, false);
  spine::spine_binary_events_section_handler(unsunk.context);
  check(clean_end(unsunk, two_events.size()), "without a diagnostics sink the section is still consumed");

  // A declared count past the data stops at the overrun but still reports what was declared.
  auto lying = section(cat({varint(40), varint(0)}));
  spine::spine_binary_events_section_handler(lying.context);
  check(spine::is_spine_binary_reader_overrun(view(lying.context->reader)) &&
            one_crumb(lying, "spine.event-unsupported", "skipSpineBinaryEvents", "events", 40),
        "a truncated events section overruns and reports the declared count");
}

// --- spineBinaryStubTimelineHandlers --------------------------------------------------------------------

flight::Ref<types::SpineBinaryTimelineContext> timeline(const Bytes& bytes) {
  return flight::make_ref<types::SpineBinaryTimelineContext>(types::SpineBinaryTimelineContext{
      .section = section(bytes).context,
  });
}

bool timeline_end(const flight::Ref<types::SpineBinaryTimelineContext>& context, std::size_t length) {
  return context->section->reader->offset == static_cast<double>(length) &&
         !spine::is_spine_binary_reader_overrun(view(context->section->reader));
}

double count_of(const flight::Map<flight::String, double>& counts, const char* kind) {
  return counts.get(flight::String(kind)).value_or(0);
}

// IK: bone index, frame count, bezier count, the first frame's 12 bytes, then per frame 3 flag bytes and,
// before every frame but the last, 12 bytes and a curve tag carrying two values.
const Bytes ik_timelines = cat({varint(1), varint(0), varint(2), varint(1), zeros(12), zeros(3), zeros(12), {2},
                                zeros(32), zeros(3)});
// Transform: 24-byte payload frames with six curve values, here a linear tag.
const Bytes transform_timelines = cat({varint(1), varint(0), varint(2), varint(0), zeros(28), zeros(28), {0}});
// Path: one constraint with a PATH_MIX timeline (3 values) and a position timeline (1 value), one frame each.
const Bytes path_timelines = cat({varint(1), varint(0), varint(2), {2}, varint(1), varint(0), zeros(16), {0},
                                  varint(1), varint(0), zeros(8)});
// Deform: one skin, one slot, a SEQUENCE attachment (12 bytes per frame) and a deform attachment with a
// two-frame run.
const Bytes deform_timelines = cat({varint(1), varint(0), varint(1), varint(0), varint(2),
                                    varint(0), {1}, varint(2), zeros(24),
                                    varint(1), {0}, varint(2), varint(0), zeros(4),
                                    varint(2), varint(0), zeros(8), zeros(4), {0},
                                    varint(0)});
// Events: two frames, the second carrying a string because its flag is set.
const Bytes event_timelines = cat({varint(2), zeros(4), varint(0), varint(0), zeros(4), {0}, zeros(4), varint(0),
                                   varint(0), zeros(4), {1}, spine_string("x")});

void stub_timeline_handlers() {
  auto ik = timeline(ik_timelines);
  spine::spine_binary_ik_timeline_handler(ik);
  check(timeline_end(ik, ik_timelines.size()), "ik timelines: the last frame has no curve");
  check(count_of(ik->unmodeled_timeline_counts, "ik") == 1 && ik->unregistered_timeline_counts.size() == 0,
        "ik timelines tally as unmodeled");

  auto transform = timeline(transform_timelines);
  spine::spine_binary_transform_timeline_handler(transform);
  check(timeline_end(transform, transform_timelines.size()), "transform timelines: 24-byte payloads");
  check(count_of(transform->unmodeled_timeline_counts, "transform") == 1, "transform tallies");

  auto path = timeline(path_timelines);
  spine::spine_binary_path_timeline_handler(path);
  check(timeline_end(path, path_timelines.size()), "path timelines: PATH_MIX carries three values");
  check(count_of(path->unmodeled_timeline_counts, "path") == 2, "path tallies per timeline, not per constraint");

  auto deform = timeline(deform_timelines);
  spine::spine_binary_deform_timeline_handler(deform);
  check(timeline_end(deform, deform_timelines.size()), "deform timelines: a sequence skips 12 bytes a frame");
  check(count_of(deform->unmodeled_timeline_counts, "deform") == 1 &&
            count_of(deform->unmodeled_timeline_counts, "attachment-sequence") == 1,
        "deform and sequence tally separately");

  auto events = timeline(event_timelines);
  spine::spine_binary_event_timeline_handler(events);
  check(timeline_end(events, event_timelines.size()), "event timelines: the flag gates the string");
  check(count_of(events->unmodeled_timeline_counts, "event") == 1, "an event timeline tallies once, not per frame");

  auto no_events = timeline(varint(0));
  spine::spine_binary_event_timeline_handler(no_events);
  check(no_events->unmodeled_timeline_counts.size() == 0, "an empty event timeline tallies nothing");

  // The skip variants tally the registry's timeline kind into the unregistered counts instead, and deform
  // folds the sequence into the deform kind.
  auto skipped_deform = timeline(deform_timelines);
  spine::skip_spine_binary_deform_timelines(skipped_deform);
  check(timeline_end(skipped_deform, deform_timelines.size()) && skipped_deform->unmodeled_timeline_counts.size() == 0 &&
            skipped_deform->unregistered_timeline_counts.get(types::spine_binary_timeline_kind->deform) == 2.0,
        "deform skip tallies both attachments under the deform kind");
  auto skipped_ik = timeline(ik_timelines);
  spine::skip_spine_binary_ik_timelines(skipped_ik);
  auto skipped_transform = timeline(transform_timelines);
  spine::skip_spine_binary_transform_timelines(skipped_transform);
  auto skipped_path = timeline(path_timelines);
  spine::skip_spine_binary_path_timelines(skipped_path);
  auto skipped_events = timeline(event_timelines);
  spine::skip_spine_binary_event_timelines(skipped_events);
  check(skipped_ik->unregistered_timeline_counts.get(types::spine_binary_timeline_kind->ik) == 1.0 &&
            skipped_transform->unregistered_timeline_counts.get(types::spine_binary_timeline_kind->transform) == 1.0 &&
            skipped_path->unregistered_timeline_counts.get(types::spine_binary_timeline_kind->path) == 2.0 &&
            skipped_events->unregistered_timeline_counts.get(types::spine_binary_timeline_kind->event) == 1.0,
        "skips tally the registry kinds");
  check(timeline_end(skipped_ik, ik_timelines.size()) && timeline_end(skipped_transform, transform_timelines.size()) &&
            timeline_end(skipped_path, path_timelines.size()) && timeline_end(skipped_events, event_timelines.size()),
        "skips consume the same bytes");

  auto aliased = timeline(ik_timelines);
  spine::spine_binary_ik_timeline_reader(aliased);
  auto aliased_transform = timeline(transform_timelines);
  spine::spine_binary_transform_timeline_reader(aliased_transform);
  auto aliased_path = timeline(path_timelines);
  spine::spine_binary_path_timeline_reader(aliased_path);
  auto aliased_deform = timeline(deform_timelines);
  spine::spine_binary_deform_timeline_reader(aliased_deform);
  auto aliased_events = timeline(event_timelines);
  spine::spine_binary_event_timeline_reader(aliased_events);
  check(count_of(aliased->unmodeled_timeline_counts, "ik") == 1 &&
            count_of(aliased_transform->unmodeled_timeline_counts, "transform") == 1 &&
            count_of(aliased_path->unmodeled_timeline_counts, "path") == 2 &&
            count_of(aliased_deform->unmodeled_timeline_counts, "deform") == 1 &&
            count_of(aliased_events->unmodeled_timeline_counts, "event") == 1,
        "the timeline readers run their handlers");

  // A declared count past the data stops at the first overrun instead of tallying phantom timelines.
  auto lying = timeline(varint(30));
  spine::spine_binary_ik_timeline_handler(lying);
  check(spine::is_spine_binary_reader_overrun(view(lying->section->reader)) &&
            count_of(lying->unmodeled_timeline_counts, "ik") == 1,
        "a truncated timeline list tallies only what it began");
}

// --- spineDrawOrder -------------------------------------------------------------------------------------

using Move = spine::offset_slot_index_78b23845efacf0f9;

flight::Array<flight::Ref<Move>> moves(std::initializer_list<std::pair<double, double>> list) {
  flight::Array<flight::Ref<Move>> out;
  for (const auto& [offset, slot] : list) out.push(flight::make_ref<Move>(Move{.offset = offset, .slot_index = slot}));
  return out;
}

bool ordering_is(const std::optional<flight::Array<double>>& ordering, std::vector<double> expected) {
  if (!ordering.has_value() || ordering->size() != expected.size()) return false;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (ordering->element(static_cast<double>(i)) != expected[i]) return false;
  }
  return true;
}

void draw_order() {
  check(ordering_is(spine::resolve_spine_draw_ordering(moves({}), 3), {0, 1, 2}), "nothing moves");
  check(ordering_is(spine::resolve_spine_draw_ordering(moves({{-2, 2}}), 3), {1, 2, 0}), "one move closes the gap");
  check(ordering_is(spine::resolve_spine_draw_ordering(moves({{2, 0}, {-1, 2}}), 3), {2, 0, 1}), "several moves");
  check(!spine::resolve_spine_draw_ordering(moves({{9, 0}}), 3).has_value(), "a destination past the end");
  check(!spine::resolve_spine_draw_ordering(moves({{-1, 0}}), 3).has_value(), "a destination before the start");
  check(!spine::resolve_spine_draw_ordering(moves({{1, 0}, {0, 1}}), 3).has_value(), "two slots claim one position");
  check(!spine::resolve_spine_draw_ordering(moves({{0, 7}}), 3).has_value(), "a slot the rig does not have");

  auto ordering = spine::resolve_spine_draw_ordering(moves({{3, 1}}), 5);
  std::vector<double> sorted;
  if (ordering.has_value()) {
    for (std::size_t i = 0; i < ordering->size(); ++i) sorted.push_back(ordering->element(static_cast<double>(i)));
  }
  std::sort(sorted.begin(), sorted.end());
  check(sorted == std::vector<double>{0, 1, 2, 3, 4}, "every accepted keyframe is a permutation");
  check(!spine::resolve_spine_draw_ordering(moves({}), 0).has_value(), "a rig with no slots");

  // Boundaries the upstream cases leave implicit: the last position is a valid destination, a slot named
  // twice is refused even when its destinations differ, and fractional indices or offsets are not slots.
  check(ordering_is(spine::resolve_spine_draw_ordering(moves({{2, 0}}), 3), {2, 0, 1}), "the last position is in range");
  check(!spine::resolve_spine_draw_ordering(moves({{1, 0}, {2, 0}}), 3).has_value(), "a slot moved twice");
  check(!spine::resolve_spine_draw_ordering(moves({{0, 0.5}}), 3).has_value(), "a fractional slot index");
  check(!spine::resolve_spine_draw_ordering(moves({{0.5, 0}}), 3).has_value(), "a fractional offset");
  check(!spine::resolve_spine_draw_ordering(moves({{0, -1}}), 3).has_value(), "a negative slot index");
  check(ordering_is(spine::resolve_spine_draw_ordering(moves({{0, 1}}), 3), {0, 1, 2}), "a zero move keeps its place");
}

}  // namespace

int main() {
  version_failure();
  version_accessor();
  section_counts();
  requirements();
  parse_helpers();
  stub_section_handlers();
  stub_timeline_handlers();
  draw_order();
  return failures == 0 ? 0 : 1;
}
