#pragma once

#include <optional>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <flight/string.hpp>

namespace flight {

class Symbol {
 public:
  Symbol() : key_(std::make_shared<const String>()) {}
  explicit Symbol(const String& description) : key_(std::make_shared<const String>(description)) {}

  // `Symbol()` in ECMAScript takes an OPTIONAL description, and the emitter spells the absent case
  // explicitly as `flight::Symbol(std::nullopt)` -- `const kExit = Symbol()` becomes
  // `flight::Symbol(std::nullopt)`. This forwards to the default constructor, so it allocates a fresh key
  // and the symbol is UNIQUE: two of them never compare equal, which is `Symbol() !== Symbol()`. It is not
  // an interned symbol and must never be confused with `for_key`, which deliberately returns the same
  // symbol for the same string.
  explicit Symbol(std::nullopt_t) : Symbol() {}

  [[nodiscard]] static Symbol for_key(const String& key) {
    static std::mutex mutex;
    static std::unordered_map<std::string, std::shared_ptr<const String>> registry;
    const std::string encoded = key.to_utf8();
    const std::lock_guard lock(mutex);
    auto& stored = registry[encoded];
    if (!stored) stored = std::make_shared<const String>(key);
    return Symbol(stored, InternedTag{});
  }

  [[nodiscard]] const String& key() const noexcept {
    static const String empty;
    return key_ ? *key_ : empty;
  }

  [[nodiscard]] const void* identity() const noexcept { return key_.get(); }

  [[nodiscard]] friend bool operator==(const Symbol&, const Symbol&) noexcept = default;

 private:
  struct InternedTag {};

  Symbol(std::shared_ptr<const String> key, InternedTag) : key_(std::move(key)) {}

  std::shared_ptr<const String> key_;
};

} // namespace flight
