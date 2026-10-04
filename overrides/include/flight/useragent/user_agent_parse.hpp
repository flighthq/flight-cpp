#pragma once

#include <optional>

#include <flight/regexp.hpp>
#include <flight/runtime.hpp>
#include <flight/types/device_form_factor.hpp>

namespace flight::useragent {

[[nodiscard]] inline flight::types::DeviceFormFactor
parse_user_agent_form_factor(flight::String ua, double max_touch_points) {
  const auto matches = [&](const char* pattern) {
    return flight::RegExp(flight::String(pattern), flight::String("i")).test(ua);
  };
  if (matches("android auto|car browser|automotive")) {
    return flight::types::device_form_factor_car;
  }
  if (matches("smart[-_]?tv|smarttv|googletv|appletv|hbbtv|netcast|webos.*tv|tizen.*tv|tv safari")) {
    return flight::types::device_form_factor_tv;
  }
  if (matches("watch\\s*os|watch[_ ]?kit|wearable")) {
    return flight::types::device_form_factor_watch;
  }
  if (matches("ipad")) return flight::types::device_form_factor_tablet;
  if (matches("android") && !matches("mobile")) {
    return flight::types::device_form_factor_tablet;
  }
  if (matches("tablet\\s*pc|silk|kindle fire")) {
    return flight::types::device_form_factor_tablet;
  }
  if (matches("iphone|ipod|android.*mobile|windows phone|blackberry|bb\\d+|mobile safari")) {
    return flight::types::device_form_factor_phone;
  }
  if (max_touch_points > 1.0 && matches("macintosh|mac os x")) {
    return flight::types::device_form_factor_tablet;
  }
  if (matches("win(?:dows)?nt|macintosh|mac os x|linux(?!.*android)|x11")) {
    return flight::types::device_form_factor_desktop;
  }
  if (max_touch_points == 0.0) {
    return flight::types::device_form_factor_desktop;
  }
  return flight::types::device_form_factor_unknown;
}

[[nodiscard]] inline flight::String parse_user_agent_os_name(flight::String ua) {
  const auto matches = [&](const char* pattern) {
    return flight::RegExp(flight::String(pattern), flight::String("i")).test(ua);
  };
  if (matches("android")) return flight::String("Android");
  if (matches("ipad")) return flight::String("iPadOS");
  if (matches("iphone|ipod")) return flight::String("iOS");
  if (matches("cros")) return flight::String("ChromeOS");
  if (matches("windows nt|windows phone")) return flight::String("Windows");
  if (matches("macintosh|mac os x")) return flight::String("macOS");
  if (matches("freebsd")) return flight::String("FreeBSD");
  if (matches("openbsd")) return flight::String("OpenBSD");
  if (matches("netbsd")) return flight::String("NetBSD");
  if (matches("linux")) return flight::String("Linux");
  return flight::String("");
}

[[nodiscard]] inline flight::String parse_user_agent_os_version(
    flight::String ua) {
  const auto capture = [&](const char* pattern) -> std::optional<flight::String> {
    auto match =
        flight::RegExp(flight::String(pattern), flight::String("i")).exec(ua);
    if (!match.has_value()) return std::nullopt;
    return match->capture(1);
  };
  if (auto android = capture("android\\s+([\\d.]+)")) return *android;
  if (auto ios = capture("(?:iphone|ipad|ipod).*?os\\s+([\\d_]+)")) {
    return ios->replace(flight::RegExp(flight::String("_"), flight::String("g")),
                        flight::String("."));
  }
  if (auto windows = capture("windows nt\\s+([\\d.]+)")) return *windows;
  if (auto mac = capture("mac os x\\s+([\\d_.]+)")) {
    return mac->replace(flight::RegExp(flight::String("_"), flight::String("g")),
                        flight::String("."));
  }
  if (auto cros = capture("cros\\s+\\S+\\s+([\\d.]+)")) return *cros;
  return flight::String("");
}

}  // namespace flight::useragent
