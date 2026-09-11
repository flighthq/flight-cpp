#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <flight/string.hpp>

namespace flight {

class Symbol {
 public:
  Symbol() : key_(std::make_shared<const String>()) {}

  [[nodiscard]] static Symbol for_key(const String& key) {
    static std::mutex mutex;
    static std::unordered_map<std::string, std::shared_ptr<const String>> registry;
    const std::string encoded = key.to_utf8();
    const std::lock_guard lock(mutex);
    auto& stored = registry[encoded];
    if (!stored) stored = std::make_shared<const String>(key);
    return Symbol(stored);
  }

  [[nodiscard]] const String& key() const noexcept {
    static const String empty;
    return key_ ? *key_ : empty;
  }

  [[nodiscard]] friend bool operator==(const Symbol&, const Symbol&) noexcept = default;

 private:
  explicit Symbol(std::shared_ptr<const String> key) : key_(std::move(key)) {}

  std::shared_ptr<const String> key_;
};

} // namespace flight
