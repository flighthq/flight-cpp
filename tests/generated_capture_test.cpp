// Native port of all 30 test declarations in packages/capture/src/*.test.ts at Flight a627849,
// plus capture lifecycle, bitmap dimension/RGBA-byte-layout, result identity, and failure evidence.
#include <flight/capture/contract.hpp>

#include <cmath>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

namespace bitmap = flight::bitmap;
namespace capture = flight::capture;
namespace types = flight::types;

int failures = 0;

void check(bool condition, std::string_view message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return View(value);
}

auto provenance() {
  return flight::make_ref<types::CaptureBaselineProvenance>(types::CaptureBaselineProvenance{
      .computation_id = flight::String("grid-average-rgb-v1"),
      .frames = 1,
      .source_hash = flight::String("a1b2c3"),
      .target_kind = flight::String("webgl"),
      .verify_published = true,
      .warmup_frames = 0,
  });
}

bool provenance_equals(
    const flight::Ref<types::CaptureBaselineProvenance>& actual,
    const flight::Ref<types::CaptureBaselineProvenance>& expected) {
  return actual->computation_id == expected->computation_id && actual->frames == expected->frames &&
         actual->source_hash == expected->source_hash && actual->target_kind == expected->target_kind &&
         actual->verify_published == expected->verify_published &&
         actual->warmup_frames == expected->warmup_frames;
}

void baseline_evidence() {
  // createCaptureBaseline: allocates empty storage and allocates a distinct record on every call.
  const auto empty = capture::create_capture_baseline();
  check(empty.empty() &&
            !capture::get_capture_baseline_field(
                 empty, flight::String("canvas"), flight::String("fingerprint"))
                 .has_value(),
        "createCaptureBaseline allocates an empty record");
  const auto distinct = capture::create_capture_baseline();
  capture::set_capture_baseline_field(
      empty, flight::String("canvas"), flight::String("fingerprint"), flight::String("1:000000"));
  check(empty.identity() != distinct.identity() &&
            !capture::get_capture_baseline_field(
                 distinct, flight::String("canvas"), flight::String("fingerprint"))
                 .has_value(),
        "createCaptureBaseline returns distinct storage each call");

  // formatCaptureBaseline: sorted columns, canonical member order, two spaces, one final newline.
  auto ordered = capture::create_capture_baseline();
  capture::set_capture_baseline_field(
      ordered, flight::String("flight:webgl"), flight::String("sha256"), flight::String("aaa"));
  capture::set_capture_baseline_field(
      ordered, flight::String("flight:webgl"), flight::String("fingerprint"), flight::String("1:ffffff"));
  ordered.get(flight::String("flight:webgl")).value()->source_hash = flight::String("legacy-source");
  capture::set_capture_baseline_field(
      ordered, flight::String("canvas"), flight::String("fingerprint"), flight::String("1:000000"));
  check(capture::format_capture_baseline(ordered) ==
            flight::String("{\n"
                           "  \"canvas\": {\n"
                           "    \"fingerprint\": \"1:000000\"\n"
                           "  },\n"
                           "  \"flight:webgl\": {\n"
                           "    \"fingerprint\": \"1:ffffff\",\n"
                           "    \"sourceHash\": \"legacy-source\",\n"
                           "    \"sha256\": \"aaa\"\n"
                           "  }\n"
                           "}\n"),
        "formatCaptureBaseline preserves exact sorted canonical byte layout");
  auto omitted = capture::create_capture_baseline();
  capture::set_capture_baseline_field(
      omitted, flight::String("canvas"), flight::String("sha256"), flight::String("hash"));
  check(capture::format_capture_baseline(omitted) ==
            flight::String("{\n  \"canvas\": {\n    \"sha256\": \"hash\"\n  }\n}\n"),
        "formatCaptureBaseline omits undefined fields");
  check(capture::format_capture_baseline(capture::create_capture_baseline()) == flight::String("{}\n"),
        "formatCaptureBaseline writes the empty record with one newline");

  // getCaptureBaselineField: missing values stay absent and independent columns retain their values.
  auto fields = capture::create_capture_baseline();
  capture::set_capture_baseline_field(
      fields, flight::String("canvas"), flight::String("fingerprint"), flight::String("1:000000"));
  check(!capture::get_capture_baseline_field(fields, flight::String("canvas"), flight::String("sha256"))
             .has_value() &&
            !capture::get_capture_baseline_field(
                 fields, flight::String("webgl"), flight::String("fingerprint"))
                 .has_value(),
        "getCaptureBaselineField returns null for absent columns and fields");
  capture::set_capture_baseline_field(
      fields, flight::String("webgl"), flight::String("fingerprint"), flight::String("1:ffffff"));
  check(capture::get_capture_baseline_field(
            fields, flight::String("canvas"), flight::String("fingerprint")) ==
            std::optional<flight::String>{flight::String("1:000000")} &&
            capture::get_capture_baseline_field(
                fields, flight::String("webgl"), flight::String("fingerprint")) ==
                std::optional<flight::String>{flight::String("1:ffffff")},
        "getCaptureBaselineField reads independent columns");

  // Provenance is field-specific, legacy absence remains unknown, and writes copy rather than alias.
  const auto expected_provenance = provenance();
  auto provenance_baseline = capture::create_capture_baseline();
  capture::set_capture_baseline_provenance(
      provenance_baseline, flight::String("webgl"), flight::String("sha256"),
      readonly(expected_provenance));
  const auto read_sha = capture::get_capture_baseline_provenance(
      provenance_baseline, flight::String("webgl"), flight::String("sha256"));
  check(read_sha.has_value() && provenance_equals(*read_sha, expected_provenance),
        "getCaptureBaselineProvenance reads back the recorded value");
  check(!capture::get_capture_baseline_provenance(
             provenance_baseline, flight::String("webgl"), flight::String("fingerprint"))
             .has_value() &&
            read_sha.has_value(),
        "provenance remains independent for each captured value");
  auto legacy = capture::create_capture_baseline();
  capture::set_capture_baseline_field(
      legacy, flight::String("webgl"), flight::String("sha256"), flight::String("abc"));
  check(!capture::get_capture_baseline_provenance(
             legacy, flight::String("webgl"), flight::String("sha256"))
             .has_value() &&
            !capture::get_capture_baseline_provenance(
                 legacy, flight::String("absent"), flight::String("sha256"))
                 .has_value(),
        "legacy and absent provenance both read as unknown");

  // parseCaptureBaseline: a formatted record round-trips stably; malformed/non-object input fails.
  const auto ordered_text = capture::format_capture_baseline(ordered);
  const auto parsed = capture::parse_capture_baseline(ordered_text);
  check(parsed.has_value() && capture::format_capture_baseline(*parsed) == ordered_text &&
            capture::get_capture_baseline_field(
                *parsed, flight::String("flight:webgl"), flight::String("sha256")) ==
                std::optional<flight::String>{flight::String("aaa")},
        "parseCaptureBaseline round-trips formatted records stably");
  check(!capture::parse_capture_baseline(flight::String("{not json")).has_value(),
        "parseCaptureBaseline returns null for malformed JSON");
  check(!capture::parse_capture_baseline(flight::String("42")).has_value() &&
            !capture::parse_capture_baseline(flight::String("[]")).has_value() &&
            !capture::parse_capture_baseline(flight::String("null")).has_value(),
        "parseCaptureBaseline rejects every non-object top-level shape");

  // setCaptureBaselineField creates, merges, and overwrites one retained column owner.
  auto set_fields = capture::create_capture_baseline();
  capture::set_capture_baseline_field(
      set_fields, flight::String("canvas"), flight::String("fingerprint"), flight::String("1:000000"));
  capture::set_capture_baseline_field(
      set_fields, flight::String("canvas"), flight::String("sha256"), flight::String("hash"));
  check(capture::get_capture_baseline_field(
            set_fields, flight::String("canvas"), flight::String("fingerprint")) ==
            std::optional<flight::String>{flight::String("1:000000")} &&
            capture::get_capture_baseline_field(
                set_fields, flight::String("canvas"), flight::String("sha256")) ==
                std::optional<flight::String>{flight::String("hash")},
        "setCaptureBaselineField creates a column and merges later fields");
  capture::set_capture_baseline_field(
      set_fields, flight::String("canvas"), flight::String("fingerprint"), flight::String("1:ffffff"));
  check(capture::get_capture_baseline_field(
            set_fields, flight::String("canvas"), flight::String("fingerprint")) ==
            std::optional<flight::String>{flight::String("1:ffffff")},
        "setCaptureBaselineField overwrites in place");

  auto provenance_write = capture::create_capture_baseline();
  capture::set_capture_baseline_field(
      provenance_write, flight::String("webgl"), flight::String("sha256"), flight::String("abc"));
  auto mutable_source = provenance();
  capture::set_capture_baseline_provenance(
      provenance_write, flight::String("webgl"), flight::String("sha256"), readonly(mutable_source));
  check(capture::get_capture_baseline_field(
            provenance_write, flight::String("webgl"), flight::String("sha256")) ==
            std::optional<flight::String>{flight::String("abc")} &&
            capture::get_capture_baseline_provenance(
                provenance_write, flight::String("webgl"), flight::String("sha256"))
                .has_value(),
        "setCaptureBaselineProvenance creates or merges without changing other fields");
  const auto stored = capture::get_capture_baseline_provenance(
      provenance_write, flight::String("webgl"), flight::String("sha256"));
  mutable_source->frames = 99;
  check(stored.has_value() && (*stored).get() != mutable_source.get() && (*stored)->frames == 1,
        "setCaptureBaselineProvenance copies all values into a distinct owner");
}

void comparison_evidence() {
  const flight::String black("1:000000");
  const flight::String nudge_15("1:0f0000");
  const flight::String nudge_18("1:120000");
  const flight::String nudge_45("1:2d0000");
  const flight::String nudge_48("1:300000");
  const flight::String white("1:ffffff");

  check(capture::compare_capture_fingerprints(black, black) == 0 &&
            capture::compare_capture_fingerprints(white, white) == 0,
        "compareCaptureFingerprints is zero for identical values");
  check(capture::compare_capture_fingerprints(black, nudge_15) == 5 &&
            capture::compare_capture_fingerprints(black, white) == 255,
        "compareCaptureFingerprints measures exact mean absolute per-channel distance");
  check(std::isinf(capture::compare_capture_fingerprints(flight::String("not-a-fingerprint"), black)) &&
            std::isinf(capture::compare_capture_fingerprints(black, flight::String(""))),
        "compareCaptureFingerprints returns Infinity for either unparseable input");
  check(std::isinf(capture::compare_capture_fingerprints(
            black, flight::String("2:000000000000000000000000"))),
        "compareCaptureFingerprints returns Infinity for incompatible grid dimensions");

  const auto parity_boundary = capture::evaluate_capture_parity(black, nudge_45);
  check(parity_boundary->tolerance == capture::capture_parity_tolerance &&
            parity_boundary->difference == 15 && parity_boundary->pass,
        "evaluateCaptureParity defaults to 15 and passes at the exact boundary");
  const auto parity_past = capture::evaluate_capture_parity(black, nudge_48);
  check(parity_past->difference == 16 && !parity_past->pass,
        "evaluateCaptureParity fails immediately past its boundary");
  const auto parity_invalid = capture::evaluate_capture_parity(black, flight::String("broken"));
  check(std::isinf(parity_invalid->difference) && !parity_invalid->pass,
        "evaluateCaptureParity turns the Infinity sentinel into failure");
  check(capture::evaluate_capture_parity(black, nudge_48, 20)->pass,
        "evaluateCaptureParity honors an explicit tolerance");

  const auto regression_boundary = capture::evaluate_capture_regression(black, nudge_15);
  check(regression_boundary->tolerance == capture::capture_regression_tolerance &&
            regression_boundary->difference == 5 && regression_boundary->pass,
        "evaluateCaptureRegression defaults to 5 and passes at the exact boundary");
  const auto regression_identical = capture::evaluate_capture_regression(white, white);
  check(regression_identical->difference == 0 && regression_identical->pass,
        "evaluateCaptureRegression passes an identical fingerprint");
  const auto regression_past = capture::evaluate_capture_regression(black, nudge_18);
  check(regression_past->difference == 6 && !regression_past->pass,
        "evaluateCaptureRegression fails immediately past its boundary");
  const auto regression_invalid =
      capture::evaluate_capture_regression(black, flight::String("no-baseline"));
  check(std::isinf(regression_invalid->difference) && !regression_invalid->pass,
        "evaluateCaptureRegression turns an invalid baseline into failure");
  check(capture::evaluate_capture_regression(black, nudge_18, 10)->pass,
        "evaluateCaptureRegression honors an explicit tolerance");

  const auto second_result = capture::evaluate_capture_parity(black, nudge_45);
  check(parity_boundary.get() != second_result.get(),
        "each capture evaluation allocates a distinct result owner");
}

void lifecycle_dimensions_and_bytes() {
  // Two RGBA pixels in row-major order. The capture fingerprint retains RGB byte order and ignores A.
  auto source = flight::make_ref<types::Bitmap>(types::Bitmap{});
  source->width = 2;
  source->height = 1;
  source->data = flight::Uint8ClampedArray{
      0x10, 0x20, 0x30, 0x00,
      0xa0, 0xb0, 0xc0, 0xff,
  };
  const auto fingerprint = bitmap::create_bitmap_fingerprint(readonly(source), 2);
  check(fingerprint->grid_size == 2 && fingerprint->cells.size() == 12,
        "capture fingerprint dimensions are gridSize squared times three RGB bytes");
  check(bitmap::format_bitmap_fingerprint(readonly(fingerprint)) ==
            flight::String("2:102030a0b0c0102030a0b0c0"),
        "capture preserves row-major RGBA pixel order as RGB bytes and ignores alpha");

  const auto parsed_fingerprint = bitmap::parse_bitmap_fingerprint(
      flight::String("2:0102030405060708090a0b0c"));
  check(parsed_fingerprint.has_value() && (*parsed_fingerprint)->grid_size == 2 &&
            (*parsed_fingerprint)->cells.size() == 12 &&
            (*parsed_fingerprint)->cells.get_index(0) == 0x01 &&
            (*parsed_fingerprint)->cells.get_index(11) == 0x0c &&
            bitmap::format_bitmap_fingerprint(readonly(*parsed_fingerprint)) ==
                flight::String("2:0102030405060708090a0b0c"),
        "fingerprint parse/format retains dimension and exact byte layout");

  // Full lifecycle: acquire a fingerprint, write it, format, parse, and read the same value back.
  auto baseline = capture::create_capture_baseline();
  const auto text_fingerprint = bitmap::format_bitmap_fingerprint(readonly(fingerprint));
  capture::set_capture_baseline_field(
      baseline, flight::String("canvas"), flight::String("fingerprint"), text_fingerprint);
  const auto serialized = capture::format_capture_baseline(baseline);
  const auto restored = capture::parse_capture_baseline(serialized);
  check(restored.has_value() &&
            capture::get_capture_baseline_field(
                *restored, flight::String("canvas"), flight::String("fingerprint")) ==
                std::optional<flight::String>{text_fingerprint},
        "capture lifecycle preserves the acquired fingerprint through baseline serialization");
}

} // namespace

int main() {
  baseline_evidence();
  comparison_evidence();
  lifecycle_dimensions_and_bytes();
  if (failures != 0) {
    std::cerr << failures << " generated capture assertion(s) failed\n";
    return 1;
  }
  return 0;
}
