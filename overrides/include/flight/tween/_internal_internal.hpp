// Re-derived from the generated tween internal module at a627849. Every function is carried verbatim except
// the keyed reads and writes of the erased target and property map, which go through the object's own row
// owner, and the registration target, which is the `ErasedRef` the manager's map is keyed by. See
// overrides/manifest.json (tween-internal-erased-numeric-record) for what changed, why, and what remains.
#pragma once
#include <cmath>
#include <flight/any.hpp>
#include <flight/erased_ref.hpp>
#include <flight/error.hpp>
#include <flight/object.hpp>
#include <flight/record.hpp>
#include <flight/structural_ref.hpp>
#include <flight/weak_map.hpp>
#include <functional>
#include <optional>
#include <type_traits>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::types { struct Entity; }
namespace flight::types { struct TweenManager; }
namespace flight::types { struct TweenOptions; }
namespace flight::types { struct TweenPropertyDetail; }
namespace flight::types { template <typename T> struct Signal; }
namespace flight::types { template <typename T> struct Tween; }

#include <flight/entity/entity.hpp>
#include <flight/signals/signal.hpp>
#include <flight/types/easing_function.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/signal.hpp>
#include <flight/types/tween.hpp>
#include <flight/types/tween_manager.hpp>
#include <flight/types/tween_options.hpp>
#include <flight/types/tween_property_detail.hpp>

namespace flight::tween {

using flight::types::Tween;

// OVERRIDE: the keyed numeric view every tween module takes of an erased target or property map.
//
// The source writes `tween.target as Record<string, number>` and `tween.propertyMap as
// Record<string, number>`. The emitter erases `NumericProps<T>` to `flight::Any`
// (docs/compiler-requests/generic-mapped-type-erased-to-any.md) and lowers each assertion to a
// `static_cast` to a fresh `flight::Record` (docs/compiler-requests/assertion-lowered-to-unchecked-static-cast.md),
// which no conversion answers -- and which, if one did, would write the tweened values into a copy
// rather than onto the target. What the assertion stands for is a keyed read and write of the value's
// own properties, and an erased value reaches those one of two ways:
//
//  * an object's own members, through `flight::named_properties`. It resolves the object's one row
//    owner, so a write here is the write a typed reader of the same object sees, with no copy and no
//    second identity;
//  * a `flight::Record<String, number>`, which an `Any` carries as `Any::external` -- the recovery the
//    emitted code uses for a Record carrier. A record's copies share their entries, so the recovered
//    record IS the caller's, and a write adds or replaces an entry exactly as the source's does.
//
// A write an object cannot take -- a key it does not declare, or a value its member cannot hold --
// throws rather than being dropped: the source's write always lands, so a silent `false` would leave
// the target untweened with nothing to say why.
namespace override_detail {

class NumericRecord final {
 public:
  explicit NumericRecord(flight::NamedProperties named) : named_(std::move(named)) {}
  explicit NumericRecord(flight::Record<flight::String, double> record) : record_(std::move(record)) {}

  // `record[key] ?? fallback`: an absent key and a nullish value both read as the fallback.
  [[nodiscard]] double read(const flight::String& key, double fallback) const {
    if (record_.has_value()) return record_->get(key).value_or(fallback);
    const auto value = named_->get(key);
    if (value.is_nullish()) return fallback;
    return value.as_number();
  }

  // `record[key]`, for a caller that tells `undefined` apart from a number.
  [[nodiscard]] flight::Any get(const flight::String& key) const {
    if (record_.has_value()) {
      const auto value = record_->get(key);
      return value.has_value() ? flight::Any(*value) : flight::Any();
    }
    return named_->get(key);
  }

  void write(const flight::String& key, double value) const {
    if (record_.has_value()) {
      record_->set(key, value);
      return;
    }
    if (!named_->set(key, flight::Any(value))) {
      throw flight::TypeError(flight::String("tween target cannot hold numeric property '") + key + flight::String("'"));
    }
  }

  // `key in record`.
  [[nodiscard]] bool has(const flight::String& key) const {
    return record_.has_value() ? record_->has(key) : named_->has(key);
  }

  // `Object.keys(record)`: insertion order for a record, source declaration order for an object.
  [[nodiscard]] flight::Array<flight::String> keys() const {
    if (record_.has_value()) return flight::object_keys(*record_);
    flight::Array<flight::String> keys;
    for (auto& key : named_->keys()) keys.push(std::move(key));
    return keys;
  }

 private:
  std::optional<flight::NamedProperties> named_;
  std::optional<flight::Record<flight::String, double>> record_;
};

[[nodiscard]] inline NumericRecord numeric_record(const flight::Any& value) {
  if (const auto* record = value.external_if<flight::Record<flight::String, double>>()) return NumericRecord(*record);
  return NumericRecord(flight::named_properties(value));
}

template <typename Object>
[[nodiscard]] NumericRecord numeric_record(const std::shared_ptr<Object>& value) {
  return NumericRecord(flight::named_properties(value));
}

// The `object` a `Map<object, Tween<any>[]>` is keyed by: the target itself, by identity.
[[nodiscard]] inline flight::ErasedRef object_key(const flight::Any& value) {
  if (const auto* record = value.external_if<flight::Record<flight::String, double>>()) return record->erased_ref();
  return value.erased_ref();
}

template <typename Object>
[[nodiscard]] flight::ErasedRef object_key(const std::shared_ptr<Object>& value) {
  return flight::ErasedRef(value);
}

} // namespace override_detail

template <typename T>
inline void initialize_tween(flight::Ref<flight::types::Tween<T>> tween) {
  const auto target = override_detail::numeric_record(tween->target);
  const auto property_map = override_detail::numeric_record(tween->property_map);
  for (auto detail : tween->properties) {
    const double start = target.read(detail->key, 0.0);
    const double end = property_map.read(detail->key, 0.0);
    (detail->start = start);
    (detail->change = (end - start));
    if (tween->smart_rotation) {
      double change = std::fmod((std::fmod(detail->change, 360.0) + 360.0), 360.0);
      if ((change > 180.0)) {
        (change -= 360.0);
      }
      (detail->change = change);
    }
  }
  (tween->initialized = true);
}

template <typename T>
inline flight::Ref<flight::types::Tween<T>> make_tween(T target, double duration, flight::types::NumericProps<T> property_map, std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::TweenOptions>>>>> options, flight::types::EasingFunction default_ease) {
  flight::Array<flight::String> keys = override_detail::numeric_record(property_map).keys();
  flight::Array<flight::Ref<flight::types::TweenPropertyDetail>> properties = keys.map([=](flight::String key) { return flight::make_ref<flight::types::TweenPropertyDetail>(flight::types::TweenPropertyDetail{.change = 0.0, .key = key, .start = 0.0}); });
  flight::types::EntityConstruction<flight::Ref<flight::types::Tween<T>>> out = flight::entity::allocate_entity<flight::Ref<flight::types::Tween<T>>>();
  flight::row_set<flight::RowKey<"complete">>(out, false);
  flight::row_set<flight::RowKey<"delay">>(out, ([&]() -> double { auto nullish_coalesce_left = ([&]() -> std::optional<double> { auto optional_chain_receiver = options; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"delay">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return 0.0; }()));
  flight::row_set<flight::RowKey<"duration">>(out, duration);
  flight::row_set<flight::RowKey<"ease">>(out, ([&]() -> std::function<double(double)> { auto nullish_coalesce_left = ([&]() -> std::optional<flight::types::EasingFunction> { auto optional_chain_receiver = options; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"ease">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return default_ease; }()));
  flight::row_set<flight::RowKey<"elapsed">>(out, 0.0);
  flight::row_set<flight::RowKey<"initialized">>(out, false);
  flight::row_set<flight::RowKey<"onComplete">>(out, flight::signals::create_signal<std::function<void()>>());
  flight::row_set<flight::RowKey<"onRepeat">>(out, flight::signals::create_signal<std::function<void()>>());
  flight::row_set<flight::RowKey<"onUpdate">>(out, flight::signals::create_signal<std::function<void()>>());
  flight::row_set<flight::RowKey<"onYoyo">>(out, flight::signals::create_signal<std::function<void()>>());
  flight::row_set<flight::RowKey<"paused">>(out, false);
  flight::row_set<flight::RowKey<"properties">>(out, properties);
  flight::row_set<flight::RowKey<"propertyMap">>(out, property_map);
  flight::row_set<flight::RowKey<"reflect">>(out, ([&]() -> bool { auto nullish_coalesce_left = ([&]() -> std::optional<bool> { auto optional_chain_receiver = options; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"reflect">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return false; }()));
  flight::row_set<flight::RowKey<"repeat">>(out, ([&]() -> double { auto nullish_coalesce_left = ([&]() -> std::optional<double> { auto optional_chain_receiver = options; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"repeat">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return 0.0; }()));
  flight::row_set<flight::RowKey<"reverse">>(out, ([&]() -> bool { auto nullish_coalesce_left = ([&]() -> std::optional<bool> { auto optional_chain_receiver = options; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"reverse">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return false; }()));
  flight::row_set<flight::RowKey<"smartRotation">>(out, ([&]() -> bool { auto nullish_coalesce_left = ([&]() -> std::optional<bool> { auto optional_chain_receiver = options; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"smartRotation">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return false; }()));
  flight::row_set<flight::RowKey<"snapping">>(out, ([&]() -> bool { auto nullish_coalesce_left = ([&]() -> std::optional<bool> { auto optional_chain_receiver = options; if (!optional_chain_receiver.has_value()) return std::nullopt; return flight::row_get<flight::RowKey<"snapping">>(optional_chain_receiver.value()); }()); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return false; }()));
  flight::row_set<flight::RowKey<"target">>(out, target);
  return flight::entity::finish_entity<flight::Ref<flight::types::Tween<T>>>(out);
}

inline flight::WeakMap<flight::Ref<void>, flight::Array<flight::String>> registration_property_keys = flight::WeakMap<flight::Ref<void>, flight::Array<flight::String>>();

template <typename T>
inline bool has_tween_property(flight::Ref<flight::types::Tween<T>> tween, flight::String key) {
  std::optional<flight::Array<flight::String>> registration_property_keys_2 = registration_property_keys.get(tween);
  if (registration_property_keys_2.has_value()) {
    return registration_property_keys_2.value().includes(key);
  }
  return override_detail::numeric_record(tween->property_map).has(key);
}

template <typename T>
inline void add_tween_to_manager(flight::Ref<flight::types::TweenManager> manager, flight::Ref<flight::types::Tween<T>> tween, bool overwrite, std::optional<flight::ErasedRef> registration_target = std::nullopt, std::optional<flight::Array<flight::String>> registration_property_keys_2 = std::nullopt) {
  registration_target = registration_target.value_or(override_detail::object_key(tween->target));
  if (registration_property_keys_2.has_value()) {
    registration_property_keys.set(tween, registration_property_keys_2.value());
  }
  std::optional<flight::Array<flight::Ref<flight::types::Tween<flight::Any>>>> list = manager->tweens.get(registration_target.value());
  if (!list.has_value()) {
    (list = std::optional<flight::Array<flight::Ref<flight::types::Tween<flight::Any>>>>{flight::Array<flight::Ref<flight::types::Tween<flight::Any>>>{}});
    manager->tweens.set(registration_target.value(), list.value());
  }
  if (overwrite) {
    {
      double i = (static_cast<double>(list.value().size()) - 1.0);
      while ((i >= 0.0)) {
        {
          auto existing = list.value().element(i);
          bool overlaps = false;
          if (!registration_property_keys_2.has_value()) {
            for (auto detail : tween->properties) {
              if (has_tween_property<flight::Any>(existing, detail->key)) {
                (overlaps = true);
                break;
              }
            }
          }
          else {
            for (auto key : registration_property_keys_2.value()) {
              if (has_tween_property<flight::Any>(existing, key)) {
                (overlaps = true);
                break;
              }
            }
          }
          if (overlaps) {
            (existing->complete = true);
          }
        }
        (i -= 1.0);
      }
    }
  }
  list.value().push(tween);
}

} // namespace flight::tween
