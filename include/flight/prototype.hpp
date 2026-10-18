#pragma once

#include <cstdint>
#include <flight/out_of_profile.hpp>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace flight {

// THE PROTOTYPE CHAIN OF AN EMITTED CLASS. Emitted objects are aggregates: an interface or object
// literal becomes a struct whose own members are its cells, and its prototype is Object.prototype. A
// class is different: its methods and accessors live on its prototype, not on the instance, and the
// emitted struct carries them as member functions that no row cell reports. So the emitter registers
// each class's prototype member names here, keyed by the class's exact C++ type, with the class it
// extends. `key in object` and `typeof object.key` consult it after the object's own cells.
//
// A class that an emitted class extends is registered with `has_subclasses`, because a reference to it
// may hold a subclass instance whose prototype adds members the registered table does not list; a
// query that reaches such a class without the object's exact type refuses rather than answering from
// the base's table.
// One OWN instance field of a class. Flight compiles with define semantics for class fields, so every
// declared (non-`declare`, non-abstract, non-static) field is an own property from construction, whatever
// its value; `in` needs only the name. `type_of` reads the field's current value as typeof would, from an
// object of the class that declares it, and returns null where the stored value cannot say (a disengaged
// carrier whose source type admits both null and undefined, or a value kind this runtime does not map).
struct ClassField final {
  std::string name;
  const char* (*type_of)(const void* object){nullptr};
};

struct ClassPrototype final {
  std::vector<std::string> methods;
  std::vector<std::string> accessors;
  std::optional<std::type_index> base;
  bool has_subclasses = false;
};

// A presence or typeof question this runtime cannot answer the way the source would -- an array, function
// or external receiver, an inherited accessor whose value would require running its getter, a class whose
// subclasses the stored type cannot rule out, a reference with no allocation provenance -- raises the one
// out-of-profile family, OutOfProfileError, with that reason. It is an explicit boundary, never a stand-in
// for the source's own answer.

enum class InheritedMemberKind : std::uint8_t { none, method, accessor };

namespace detail {

struct ClassPrototypeRegistry final {
  std::mutex mutex;
  std::map<std::type_index, ClassPrototype> classes;
  std::map<std::type_index, std::vector<ClassField>> fields;
};

inline ClassPrototypeRegistry& class_prototype_registry() {
  static ClassPrototypeRegistry registry;
  return registry;
}

} // namespace detail

// Registers `Class`'s prototype members. Used as a namespace-scope initializer by emitted code:
// `inline const bool flight_class_prototype_X = flight::register_class_prototype<X>({...});`.
template <typename Class>
bool register_class_prototype(ClassPrototype prototype) {
  auto& registry = detail::class_prototype_registry();
  const std::lock_guard lock(registry.mutex);
  registry.classes.insert_or_assign(std::type_index(typeid(Class)), std::move(prototype));
  return true;
}

// Registers `Class`'s own instance fields, alongside its prototype. A separate call so that a class
// registered before fields were recorded keeps its existing registration line unchanged.
template <typename Class>
bool register_class_fields(std::vector<ClassField> fields) {
  auto& registry = detail::class_prototype_registry();
  const std::lock_guard lock(registry.mutex);
  registry.fields.insert_or_assign(std::type_index(typeid(Class)), std::move(fields));
  return true;
}

[[nodiscard]] inline bool is_registered_class(std::type_index type) {
  auto& registry = detail::class_prototype_registry();
  const std::lock_guard lock(registry.mutex);
  return registry.classes.contains(type);
}

// Walks the registered prototype chain of `type` for `key`. An unregistered type has no class
// prototype (it is a plain object, whose prototype is Object.prototype) and answers `none`. Reaching a
// registered class with subclasses refuses, because the stored type is not proven to be the exact one.
// The class field named `key` on the chain of `type`, and whether `type` itself declares it (only then may
// its reader be applied to an object of `type`). Refuses, like find_inherited_member, a class with emitted
// subclasses, because the stored type is not proven to be the object's exact one.
struct ClassFieldHit final {
  ClassField field;
  bool declared_by_exact_type = false;
};

[[nodiscard]] inline std::optional<ClassFieldHit> find_class_field(std::type_index type, std::string_view key) {
  auto& registry = detail::class_prototype_registry();
  const std::lock_guard lock(registry.mutex);
  std::optional<std::type_index> current = type;
  bool exact = true;
  while (current) {
    const auto found = registry.classes.find(*current);
    if (found == registry.classes.end()) return std::nullopt;
    if (found->second.has_subclasses) {
      throw OutOfProfileError("a class with emitted subclasses is reached without the object's exact type");
    }
    if (const auto fields = registry.fields.find(*current); fields != registry.fields.end()) {
      for (const auto& field : fields->second) {
        if (field.name == key) return ClassFieldHit{field, exact};
      }
    }
    current = found->second.base;
    exact = false;
  }
  return std::nullopt;
}

// Whether every class on the chain of `type` registered both its prototype and its own fields, so those
// fields plus the prototype members are the complete set of names an instance answers for (besides
// Object.prototype's). A class registered without fields keeps refusing names it cannot place.
[[nodiscard]] inline bool is_complete_registered_class(std::type_index type) {
  auto& registry = detail::class_prototype_registry();
  const std::lock_guard lock(registry.mutex);
  std::optional<std::type_index> current = type;
  while (current) {
    const auto found = registry.classes.find(*current);
    if (found == registry.classes.end() || !registry.fields.contains(*current)) return false;
    current = found->second.base;
  }
  return true;
}

[[nodiscard]] inline InheritedMemberKind find_inherited_member(std::type_index type, std::string_view key) {
  auto& registry = detail::class_prototype_registry();
  const std::lock_guard lock(registry.mutex);
  std::optional<std::type_index> current = type;
  while (current) {
    const auto found = registry.classes.find(*current);
    if (found == registry.classes.end()) return InheritedMemberKind::none;
    if (found->second.has_subclasses) {
      throw OutOfProfileError("a class with emitted subclasses is reached without the object's exact type");
    }
    for (const auto& name : found->second.methods) {
      if (name == key) return InheritedMemberKind::method;
    }
    for (const auto& name : found->second.accessors) {
      if (name == key) return InheritedMemberKind::accessor;
    }
    current = found->second.base;
  }
  return InheritedMemberKind::none;
}

} // namespace flight
