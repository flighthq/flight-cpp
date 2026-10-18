// Focused native behavior for the compiler-symbol-own-presence patch at Flight a627849. Two emitted sites:
// @flighthq/log createFileLogSink builds `{ [EntityRuntimeKey]: undefined, sink }`, and @flighthq/scene3d
// isMaterialEntity answers `EntityRuntimeKey in source`, emitted as has_own_property(source, entity_runtime_key).
// flight/scene3d/scene_document.hpp itself is still header-blocked (it calls the refused materializeDocumentMaterial
// and buildDocumentNode), so the second fixture evaluates that exact emitted expression on real materials rather
// than including the header. These fixtures pin the source contract: the literal's
// computed member is written (present-undefined, not missing) without dropping the sink beside it, and `in`
// distinguishes an entity the factory wrote from a plain document object that never had the key.
#include <flight/log/log.hpp>
#include <flight/materials/material.hpp>

#include <iostream>
#include <string>

namespace {

namespace types = flight::types;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

} // namespace

int main() {
  // createFileLogSink: the computed key is present with undefined, and the sink keeps its value.
  {
    auto written = std::make_shared<flight::Array<flight::String>>();
    auto transport = flight::make_ref<types::LogTransport>();
    transport->write = [written](flight::String line) -> types::LogTransportWriteOutcome {
      written->push(line);
      return flight::make_ref<types::reason_43a745d20647bfb6>(types::reason_43a745d20647bfb6{.reason = flight::String("ok")});
    };
    const auto handle = flight::log::create_file_log_sink(transport);
    check(flight::has_own_property(handle, types::entity_runtime_key),
          "`EntityRuntimeKey in sink` is true: the literal wrote the key");
    check(!handle->entity_runtime_key.has_value(), "the key holds undefined, as the literal wrote it");
    check(static_cast<bool>(handle->sink), "the sink beside the computed member is not dropped");
    auto entry = flight::make_ref<types::LogEntry>(
        types::LogEntry{.level = types::LogLevel::Info, .channel = std::nullopt, .data = flight::String("hello")});
    handle->sink(entry);
    check(written->length() == 1.0, "the sink writes one formatted line to the transport");
  }

  // isMaterialEntity's emitted body: a factory-built material was written the key; a plain document object was not.
  {
    const auto entity = flight::materials::create_material(flight::String("StandardMaterial"));
    check(flight::has_own_property(entity, types::entity_runtime_key),
          "a material from createMaterial is an entity: allocateEntity wrote the key as undefined");
    const auto plain = flight::make_ref<types::Material>(types::Material{});
    plain->kind = flight::String("StandardMaterial");
    check(!flight::has_own_property(plain, types::entity_runtime_key), "a plain document material never had the key");
  }

  if (failures != 0) return 1;
  std::cout << "symbol own presence behaves as specified\n";
  return 0;
}
