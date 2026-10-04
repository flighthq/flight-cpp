#pragma once

#include <optional>
#include <variant>

#include <flight/any.hpp>
#include <flight/array_buffer.hpp>
#include <flight/boolean.hpp>
#include <flight/record.hpp>
#include <flight/regexp.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/platform.hpp>
#include <flight/useragent/user_agent_parse.hpp>

namespace flight::useragent {

[[nodiscard]] inline flight::types::PlatformEndianness detect_endianness() {
  try {
    flight::ArrayBuffer buffer(2.0);
    flight::Uint16Array words(buffer);
    words.set_index(0.0, 0x0102);
    flight::Uint8Array bytes(buffer);
    if (bytes.get_index(0.0) == 0x01) return flight::String("big");
    if (bytes.get_index(0.0) == 0x02) return flight::String("little");
  } catch (...) {
  }
  return flight::String("unknown");
}

[[nodiscard]] inline flight::String parse_user_agent_arch(
    flight::String ua,
    std::optional<flight::String> uad_platform = std::nullopt) {
  if (uad_platform.has_value() && uad_platform->length() > 0.0) {
    const auto platform = uad_platform->to_lower();
    if (platform.includes(flight::String("arm"))) return flight::String("arm64");
    if (platform.includes(flight::String("x86")) ||
        platform.includes(flight::String("windows")) ||
        platform.includes(flight::String("linux")) ||
        platform.includes(flight::String("mac")) ||
        platform.includes(flight::String("chrome"))) {
      return flight::String("x64");
    }
  }
  const auto matches = [&](const char* pattern) {
    return flight::RegExp(flight::String(pattern), flight::String("i")).test(ua);
  };
  if (matches("arm64|aarch64")) return flight::String("arm64");
  if (matches("arm")) return flight::String("arm");
  if (matches("x86_64|win64|wow64|x64")) return flight::String("x64");
  if (matches("i[3-6]86|x86")) return flight::String("x86");
  if (matches("riscv64")) return flight::String("riscv64");
  if (matches("mips64")) return flight::String("mips64");
  if (matches("mips")) return flight::String("mips");
  return flight::String("");
}

[[nodiscard]] inline flight::types::PlatformEngine parse_user_agent_engine(
    flight::String ua) {
  const auto matches = [&](const char* pattern) {
    return flight::RegExp(flight::String(pattern), flight::String("i")).test(ua);
  };
  if (matches("iphone|ipad|ipod")) return flight::String("webkit");
  if (matches("firefox")) return flight::String("gecko");
  if (matches("edge\\/\\d")) return flight::String("unknown");
  if (matches("chrome|chromium|edg|opr|samsung")) return flight::String("blink");
  if (matches("safari|webkit")) return flight::String("webkit");
  return flight::String("unknown");
}

[[nodiscard]] inline flight::String regexp_capture(
    const flight::String& ua, const char* pattern) {
  auto match =
      flight::RegExp(flight::String(pattern), flight::String("i")).exec(ua);
  if (!match.has_value()) return flight::String("");
  return match->capture(1).value_or(flight::String(""));
}

[[nodiscard]] inline flight::String parse_user_agent_engine_version(
    flight::String ua, flight::types::PlatformEngine engine) {
  if (engine == flight::String("gecko")) {
    return regexp_capture(ua, "firefox\\/([\\d.]+)");
  }
  if (engine == flight::String("blink")) {
    auto version = regexp_capture(ua, "edg\\/([\\d.]+)");
    if (version.length() > 0.0) return version;
    version = regexp_capture(ua, "opr\\/([\\d.]+)");
    if (version.length() > 0.0) return version;
    return regexp_capture(ua, "chrome\\/([\\d.]+)");
  }
  if (engine == flight::String("webkit")) {
    auto version = regexp_capture(ua, "version\\/([\\d.]+)");
    if (version.length() > 0.0) return version;
    return regexp_capture(ua, "applewebkit\\/([\\d.]+)");
  }
  return flight::String("");
}

[[nodiscard]] inline flight::types::PlatformKind parse_user_agent_kind(
    flight::types::PlatformName name) {
  return name == flight::String("ios") || name == flight::String("android")
             ? flight::String("mobile")
             : flight::String("web");
}

[[nodiscard]] inline flight::types::PlatformName parse_user_agent_name(
    flight::String ua) {
  const auto matches = [&](const char* pattern) {
    return flight::RegExp(flight::String(pattern), flight::String("i")).test(ua);
  };
  if (matches("android")) return flight::String("android");
  if (matches("iphone|ipad|ipod")) return flight::String("ios");
  if (matches("win")) return flight::String("windows");
  if (matches("mac")) return flight::String("macos");
  if (matches("linux")) return flight::String("linux");
  return flight::String("web");
}

[[nodiscard]] inline double parse_user_agent_pointer_width(
    flight::String arch) {
  if (arch == flight::String("x64") || arch == flight::String("arm64")) {
    return 64.0;
  }
  if (arch == flight::String("x86") || arch == flight::String("arm")) {
    return 32.0;
  }
  return -1.0;
}

[[nodiscard]] inline flight::Any record_value(
    const flight::Record<flight::String, flight::Any>& record,
    const char* key) {
  return record.get(flight::String(key)).value_or(flight::Any(flight::undefined));
}

[[nodiscard]] inline flight::Any object_record_value(const flight::Any& object,
                                                     const char* key) {
  using DynamicRecord = flight::Record<flight::String, flight::Any>;
  if (const auto* record = object.external_if<DynamicRecord>()) {
    return record_value(*record, key);
  }
  return flight::named_properties(object).get(flight::String(key));
}

[[nodiscard]] inline flight::types::PlatformRuntime parse_user_agent_runtime(
    std::variant<flight::Record<flight::String, flight::Any>, flight::Null,
                 flight::Undefined>
        win) {
  if (!std::holds_alternative<flight::Record<flight::String, flight::Any>>(win)) {
    return flight::String("unknown");
  }
  const auto& window =
      std::get<flight::Record<flight::String, flight::Any>>(win);
  const auto process = record_value(window, "process");
  if (process.to_boolean()) {
    const auto versions = object_record_value(process, "versions");
    if (versions.to_boolean()) {
      if (object_record_value(versions, "electron").to_boolean()) {
        return flight::String("electron");
      }
    }
  }
  if (record_value(window, "__TAURI__").to_boolean()) return flight::String("tauri");
  if (record_value(window, "Capacitor").to_boolean()) {
    return flight::String("capacitor");
  }
  return flight::String("web");
}

[[nodiscard]] inline flight::String parse_user_agent_version(
    flight::String ua, flight::types::PlatformName name) {
  if (name == flight::String("linux") || name == flight::String("web")) {
    return flight::String("");
  }
  if (parse_user_agent_name(ua) != name) return flight::String("");
  return parse_user_agent_os_version(std::move(ua));
}

}  // namespace flight::useragent
