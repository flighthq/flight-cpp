// Derived from @flighthq/capture/packages/capture/src/captureBaseline.ts at
// a62784923f3be814463286c9fb28edfe6ab15789. The generated module refuses every public function
// because an imported Record alias loses the representation emitted by @flighthq/types.
#pragma once

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <flight/json.hpp>
#include <flight/structural_ref.hpp>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/capture_baseline.hpp>
#include <flight/types/capture_baseline_field.hpp>
#include <flight/types/capture_baseline_provenance.hpp>
#include <flight/types/capture_baseline_provenance_field.hpp>
#include <flight/types/capture_column_baseline.hpp>

namespace flight::capture {

using flight::types::CaptureBaseline;
using flight::types::CaptureBaselineProvenance;
using flight::types::CaptureColumnBaseline;

namespace detail {

inline flight::JsonValue nullable_string_json(const std::optional<flight::String>& value) {
  return value.has_value() ? flight::JsonValue(*value) : flight::JsonValue(nullptr);
}

inline flight::JsonObject provenance_json(const flight::Ref<CaptureBaselineProvenance>& provenance) {
  flight::JsonObject result;
  result.set(flight::String("computationId"), nullable_string_json(provenance->computation_id));
  result.set(flight::String("frames"), flight::JsonValue(provenance->frames));
  result.set(flight::String("sourceHash"), nullable_string_json(provenance->source_hash));
  result.set(flight::String("targetKind"), nullable_string_json(provenance->target_kind));
  result.set(flight::String("verifyPublished"), flight::JsonValue(provenance->verify_published));
  result.set(flight::String("warmupFrames"), flight::JsonValue(provenance->warmup_frames));
  return result;
}

inline flight::JsonObject column_json(const flight::Ref<CaptureColumnBaseline>& column) {
  flight::JsonObject result;
  if (column->fingerprint.has_value()) {
    result.set(flight::String("fingerprint"), flight::JsonValue(*column->fingerprint));
  }
  if (column->source_hash.has_value()) {
    result.set(flight::String("sourceHash"), flight::JsonValue(*column->source_hash));
  }
  if (column->sha256.has_value()) {
    result.set(flight::String("sha256"), flight::JsonValue(*column->sha256));
  }
  if (column->fingerprint_provenance.has_value()) {
    result.set(flight::String("fingerprintProvenance"), provenance_json(*column->fingerprint_provenance));
  }
  if (column->sha256_provenance.has_value()) {
    result.set(flight::String("sha256Provenance"), provenance_json(*column->sha256_provenance));
  }
  return result;
}

inline bool read_optional_string(
    const flight::JsonObject& object,
    const flight::String& key,
    std::optional<flight::String>& output) {
  const auto value = object.get(key);
  if (!value.has_value() || value->is_null()) {
    output = std::nullopt;
    return true;
  }
  if (value->kind() != flight::JsonValue::Kind::string) return false;
  output = value->as_string();
  return true;
}

inline std::optional<flight::Ref<CaptureBaselineProvenance>> parse_provenance(
    const flight::JsonValue& value) {
  if (value.kind() != flight::JsonValue::Kind::object) return std::nullopt;
  const auto& object = value.as_object();
  auto provenance = flight::make_ref<CaptureBaselineProvenance>(CaptureBaselineProvenance{});
  if (!read_optional_string(object, flight::String("computationId"), provenance->computation_id) ||
      !read_optional_string(object, flight::String("sourceHash"), provenance->source_hash) ||
      !read_optional_string(object, flight::String("targetKind"), provenance->target_kind)) {
    return std::nullopt;
  }

  const auto frames = object.get(flight::String("frames"));
  const auto verify_published = object.get(flight::String("verifyPublished"));
  const auto warmup_frames = object.get(flight::String("warmupFrames"));
  if (!frames.has_value() || frames->kind() != flight::JsonValue::Kind::number ||
      !verify_published.has_value() || verify_published->kind() != flight::JsonValue::Kind::boolean ||
      !warmup_frames.has_value() || warmup_frames->kind() != flight::JsonValue::Kind::number) {
    return std::nullopt;
  }
  provenance->frames = frames->as_number();
  provenance->verify_published = verify_published->as_boolean();
  provenance->warmup_frames = warmup_frames->as_number();
  return provenance;
}

inline bool read_provenance(
    const flight::JsonObject& object,
    const flight::String& key,
    std::optional<flight::Ref<CaptureBaselineProvenance>>& output) {
  const auto value = object.get(key);
  if (!value.has_value()) {
    output = std::nullopt;
    return true;
  }
  const auto parsed = parse_provenance(*value);
  if (!parsed.has_value()) return false;
  output = *parsed;
  return true;
}

inline std::optional<flight::Ref<CaptureColumnBaseline>> parse_column(const flight::JsonValue& value) {
  if (value.kind() != flight::JsonValue::Kind::object) return std::nullopt;
  const auto& object = value.as_object();
  auto column = flight::make_ref<CaptureColumnBaseline>(CaptureColumnBaseline{});
  if (!read_optional_string(object, flight::String("fingerprint"), column->fingerprint) ||
      !read_optional_string(object, flight::String("sourceHash"), column->source_hash) ||
      !read_optional_string(object, flight::String("sha256"), column->sha256) ||
      !read_provenance(object, flight::String("fingerprintProvenance"), column->fingerprint_provenance) ||
      !read_provenance(object, flight::String("sha256Provenance"), column->sha256_provenance)) {
    return std::nullopt;
  }
  return column;
}

inline flight::Ref<CaptureColumnBaseline> ensure_column(
    const CaptureBaseline& baseline,
    const flight::String& column) {
  const auto existing = baseline.get(column);
  if (existing.has_value()) return *existing;
  auto created = flight::make_ref<CaptureColumnBaseline>(CaptureColumnBaseline{});
  baseline.set(column, created);
  return created;
}

} // namespace detail

inline CaptureBaseline create_capture_baseline() {
  return CaptureBaseline{};
}

inline flight::String format_capture_baseline(const CaptureBaseline& baseline) {
  std::vector<flight::String> columns;
  columns.reserve(baseline.size());
  for (const auto& [column, ignored] : baseline) {
    static_cast<void>(ignored);
    columns.push_back(column);
  }
  std::sort(columns.begin(), columns.end());

  flight::JsonObject sorted;
  for (const auto& column : columns) {
    sorted.set(column, detail::column_json(*baseline.get(column)));
  }
  return flight::Json::stringify(sorted, nullptr, 2.0) + flight::String("\n");
}

inline std::optional<flight::String> get_capture_baseline_field(
    const CaptureBaseline& baseline,
    flight::String column,
    flight::types::CaptureBaselineField field) {
  const auto entry = baseline.get(column);
  if (!entry.has_value()) return std::nullopt;
  return field == flight::String("fingerprint") ? (*entry)->fingerprint : (*entry)->sha256;
}

inline flight::String provenance_member(flight::types::CaptureBaselineProvenanceField field) {
  return field == flight::String("fingerprint")
             ? flight::String("fingerprintProvenance")
             : flight::String("sha256Provenance");
}

inline std::optional<flight::Ref<CaptureBaselineProvenance>> get_capture_baseline_provenance(
    const CaptureBaseline& baseline,
    flight::String column,
    flight::types::CaptureBaselineProvenanceField field) {
  const auto entry = baseline.get(column);
  if (!entry.has_value()) return std::nullopt;
  return field == flight::String("fingerprint")
             ? (*entry)->fingerprint_provenance
             : (*entry)->sha256_provenance;
}

inline std::optional<CaptureBaseline> parse_capture_baseline(flight::String text) {
  flight::JsonValue parsed;
  try {
    parsed = flight::Json::parse(text);
  } catch (...) {
    return std::nullopt;
  }
  if (parsed.kind() != flight::JsonValue::Kind::object) return std::nullopt;

  CaptureBaseline baseline;
  for (const auto& [column, value] : parsed.as_object()) {
    const auto entry = detail::parse_column(value);
    if (!entry.has_value()) return std::nullopt;
    baseline.set(column, *entry);
  }
  return baseline;
}

inline void set_capture_baseline_field(
    CaptureBaseline baseline,
    flight::String column,
    flight::types::CaptureBaselineField field,
    flight::String value) {
  const auto entry = detail::ensure_column(baseline, column);
  if (field == flight::String("fingerprint")) entry->fingerprint = std::move(value);
  else entry->sha256 = std::move(value);
}

inline void set_capture_baseline_provenance(
    CaptureBaseline baseline,
    flight::String column,
    flight::types::CaptureBaselineProvenanceField field,
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<CaptureBaselineProvenance>>>> provenance) {
  const auto source = provenance.shared_exact_object<CaptureBaselineProvenance>();
  if (!source) throw std::bad_cast();
  auto copy = flight::make_ref<CaptureBaselineProvenance>(CaptureBaselineProvenance{
      .computation_id = source->computation_id,
      .frames = source->frames,
      .source_hash = source->source_hash,
      .target_kind = source->target_kind,
      .verify_published = source->verify_published,
      .warmup_frames = source->warmup_frames,
  });
  const auto entry = detail::ensure_column(baseline, column);
  if (field == flight::String("fingerprint")) entry->fingerprint_provenance = std::move(copy);
  else entry->sha256_provenance = std::move(copy);
}

} // namespace flight::capture
