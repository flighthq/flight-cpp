#include <flight/useragent/user_agent.hpp>

#include <iostream>
#include <variant>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

}  // namespace

int main() {
  using flight::Any;
  using flight::Record;
  using flight::String;

  if (!check(flight::useragent::parse_user_agent_form_factor(
                 String("Mozilla Android Auto"), -1.0) == String("Car") &&
                 flight::useragent::parse_user_agent_form_factor(
                     String("Mozilla Smart-TV"), -1.0) == String("TV") &&
                 flight::useragent::parse_user_agent_form_factor(
                     String("Mozilla Watch OS"), -1.0) == String("Watch"),
             "special form-factor precedence changed") ||
      !check(flight::useragent::parse_user_agent_form_factor(
                 String("Mozilla Macintosh Safari"), 5.0) == String("Tablet") &&
                 flight::useragent::parse_user_agent_form_factor(
                     String("Mozilla X11"), -1.0) == String("Desktop") &&
                 flight::useragent::parse_user_agent_form_factor(
                     String("unidentified"), -1.0) == String("Unknown"),
             "tablet, desktop, or unknown form-factor fallback changed")) {
    return 1;
  }

  if (!check(flight::useragent::parse_user_agent_os_name(
                 String("Mozilla CrOS x86_64")) == String("ChromeOS") &&
                 flight::useragent::parse_user_agent_os_name(
                     String("Mozilla FreeBSD")) == String("FreeBSD"),
             "OS-name parsing changed") ||
      !check(flight::useragent::parse_user_agent_os_version(
                 String("Mozilla Android   14.0")) == String("14.0") &&
                 flight::useragent::parse_user_agent_os_version(
                     String("Mozilla iPhone OS 17_0_1")) == String("17.0.1") &&
                 flight::useragent::parse_user_agent_os_version(
                     String("Mozilla Windows NT 10.0")) == String("10.0") &&
                 flight::useragent::parse_user_agent_os_version(
                     String("Mozilla Mac OS X 10_15_7")) == String("10.15.7") &&
                 flight::useragent::parse_user_agent_os_version(
                     String("Mozilla CrOS x86_64 14541.0.0")) ==
                     String("14541.0.0"),
             "OS-version capture or normalization changed")) {
    return 1;
  }

  const auto endianness = flight::useragent::detect_endianness();
  if (!check(endianness == String("little") || endianness == String("big"),
             "native endianness probe did not read its shared buffer") ||
      !check(flight::useragent::parse_user_agent_arch(
                 String("generic"), String("Linux")) == String("x64") &&
                 flight::useragent::parse_user_agent_arch(
                     String("Mozilla AArch64")) == String("arm64") &&
                 flight::useragent::parse_user_agent_pointer_width(String("arm64")) ==
                     64.0 &&
                 flight::useragent::parse_user_agent_pointer_width(String("wasm")) ==
                     -1.0,
             "architecture or pointer-width parsing changed")) {
    return 1;
  }

  const String ios_edge("Mozilla iPhone EdgiOS/120 AppleWebKit/605");
  const String legacy_edge("Mozilla Windows Edge/18.19041 Safari/537");
  const String modern_edge("Mozilla Chrome/119.0 Edg/120.1 Safari/537");
  if (!check(flight::useragent::parse_user_agent_engine(ios_edge) ==
                 String("webkit") &&
                 flight::useragent::parse_user_agent_engine(legacy_edge) ==
                     String("unknown") &&
                 flight::useragent::parse_user_agent_engine(modern_edge) ==
                     String("blink"),
             "engine precedence changed") ||
      !check(flight::useragent::parse_user_agent_engine_version(
                 modern_edge, String("blink")) == String("120.1") &&
                 flight::useragent::parse_user_agent_engine_version(
                     String("Mozilla Firefox/121.0"), String("gecko")) ==
                     String("121.0"),
             "engine-version extraction changed") ||
      !check(flight::useragent::parse_user_agent_name(
                 String("Mozilla Android")) == String("android") &&
                 flight::useragent::parse_user_agent_kind(String("android")) ==
                     String("mobile") &&
                 flight::useragent::parse_user_agent_kind(String("windows")) ==
                     String("web"),
             "platform name or kind parsing changed")) {
    return 1;
  }

  using DynamicRecord = Record<String, Any>;
  const DynamicRecord versions{{String("electron"), Any(String("28.0"))}};
  const DynamicRecord process{
      {String("versions"), Any::external(versions)}};
  const DynamicRecord electron_window{
      {String("process"), Any::external(process)}};
  const DynamicRecord tauri_window{{String("__TAURI__"), Any(true)}};
  const DynamicRecord capacitor_window{{String("Capacitor"), Any(true)}};
  const DynamicRecord web_window;
  if (!check(flight::useragent::parse_user_agent_runtime(electron_window) ==
                 String("electron") &&
                 flight::useragent::parse_user_agent_runtime(tauri_window) ==
                     String("tauri") &&
                 flight::useragent::parse_user_agent_runtime(capacitor_window) ==
                     String("capacitor") &&
                 flight::useragent::parse_user_agent_runtime(web_window) ==
                     String("web") &&
                 flight::useragent::parse_user_agent_runtime(flight::Null{}) ==
                     String("unknown"),
             "runtime probe precedence or nullish fallback changed")) {
    return 1;
  }

  if (!check(flight::useragent::parse_user_agent_version(
                 String("Mozilla Android 14"), String("android")) == String("14") &&
                 flight::useragent::parse_user_agent_version(
                     String("Mozilla Android 14"), String("windows")) == String("") &&
                 flight::useragent::parse_user_agent_version(
                     String("Mozilla Linux"), String("linux")) == String(""),
             "platform-vocabulary OS version guard changed")) {
    return 1;
  }

  return 0;
}
