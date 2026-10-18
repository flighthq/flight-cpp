// Native override of pinned colladaXml.ts (Flight a627849 / compiler 806d257).
// Retains the emitted module with one corrected lowering: the finite-number callback.
#pragma once
#include <cmath>
#include <flight/boolean.hpp>
#include <flight/callable.hpp>
#include <flight/number.hpp>
#include <flight/regexp.hpp>
#include <functional>
#include <limits>
#include <optional>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::types { struct XmlElement; }

#include <flight/types/xml_element.hpp>

namespace flight::scene3d_formats {

using flight::types::XmlElement;

inline std::optional<flight::Ref<flight::types::XmlElement>> collada_child(std::optional<flight::Ref<flight::types::XmlElement>> element, flight::String name) {
  return ([&]() -> std::optional<flight::Ref<flight::types::XmlElement>> { auto optional_chain_receiver = ([&]() -> std::optional<flight::Array<flight::Ref<flight::types::XmlElement>>> { auto optional_chain_receiver = element; if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->children; }()); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value().find([=](flight::Ref<flight::types::XmlElement> entry) { return ((entry->name == name) || entry->name.ends_with(flight::String(":") + flight::to_string(name) + flight::String(""))); }); }());
}

inline flight::Array<flight::Ref<flight::types::XmlElement>> collada_children(std::optional<flight::Ref<flight::types::XmlElement>> element, flight::String name) {
  if (!element.has_value()) {
    return flight::Array<flight::Ref<flight::types::XmlElement>>{};
  }
  return element.value()->children.filter(flight::Function<bool(flight::Ref<flight::types::XmlElement>)>{[=](flight::Ref<flight::types::XmlElement> entry) { return ((entry->name == name) || entry->name.ends_with(flight::String(":") + flight::to_string(name) + flight::String(""))); }});
}

inline std::optional<flight::String> collada_id_of(flight::Ref<flight::types::XmlElement> element) {
  return element->attributes.get(flight::String("id"));
}

inline flight::String collada_local_name(flight::Ref<flight::types::XmlElement> element) {
  const double colon = element->name.index_of(flight::String(":"));
  return ((colon >= 0.0) ? element->name.slice((colon + 1.0)) : element->name);
}

inline flight::Array<double> collada_numbers(std::optional<flight::Ref<flight::types::XmlElement>> element) {
  return ([&]() -> flight::Array<double> { auto nullish_coalesce_left = ([&]() -> std::optional<flight::Array<double>> { auto optional_chain_receiver = ([&]() -> std::optional<flight::Array<double>> { auto optional_chain_receiver = ([&]() -> std::optional<flight::Array<flight::String>> { auto optional_chain_receiver = ([&]() -> std::optional<flight::Array<flight::String>> { auto optional_chain_receiver = ([&]() -> std::optional<flight::String> { auto optional_chain_receiver = ([&]() -> std::optional<flight::String> { auto optional_chain_receiver = element; if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->text; }()); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value().trim(); }()); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value().split(flight::RegExp(flight::String("\\s+"), flight::String(""))); }()); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value().filter(flight::to_boolean); }()); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value().map(flight::to_number); }()); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value().filter([](double value) { return std::isfinite(value); }); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return flight::Array<double>{}; }());
}

inline std::optional<flight::String> collada_text(std::optional<flight::Ref<flight::types::XmlElement>> element, flight::String name) {
  return ([&]() -> std::optional<flight::String> { auto logical_or_value = ([&]() -> std::optional<flight::String> { auto optional_chain_receiver = ([&]() -> std::optional<flight::String> { auto optional_chain_receiver = collada_child(element, name); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->text; }()); if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value().trim(); }()); if (flight::to_boolean(logical_or_value)) return logical_or_value.value(); return std::nullopt; }());
}

inline flight::Array<double> parse_collada_floats(flight::String text) {
  flight::Array<flight::String> parts = text.trim().split(flight::RegExp(flight::String("\\s+"), flight::String("")));
  flight::Array<double> result = flight::Array<double>{};
  {
    double i = 0.0;
    while ((i < static_cast<double>(parts.size()))) {
      {
        const double v = flight::to_number(parts.element(i));
        if (!std::isfinite(v)) {
          (i += 1.0);
          continue;
        }
        result.push(v);
      }
      (i += 1.0);
    }
  }
  return result;
}

inline void walk_collada_element(flight::Ref<flight::types::XmlElement> element, std::function<void(flight::Ref<flight::types::XmlElement>)> visit) {
  visit(element);
  for (auto entry : element->children) {
    walk_collada_element(entry, visit);
  }
}

inline flight::Array<flight::Ref<flight::types::XmlElement>> collada_descendants(flight::Ref<flight::types::XmlElement> element, flight::String name) {
  flight::Array<flight::Ref<flight::types::XmlElement>> out = flight::Array<flight::Ref<flight::types::XmlElement>>{};
  walk_collada_element(element, flight::Function<void(flight::Ref<flight::types::XmlElement>)>{[=](flight::Ref<flight::types::XmlElement> e) {
  if (((e->name == name) || e->name.ends_with(flight::String(":") + flight::to_string(name) + flight::String("")))) {
    out.push(e);
  }
}});
  return out;
}

} // namespace flight::scene3d_formats
