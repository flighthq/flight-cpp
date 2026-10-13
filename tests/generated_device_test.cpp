// Native parity test for @flighthq/device, ported from its own device.test.ts (12 cases).
//
// The package is two things: four descriptor constructors that fill every field with a declared sentinel, and five
// pass-through reads of a host device provider, each of which takes a CALLER-OWNED out object, fills it and returns
// that same object. So the portable semantics are the sentinels, the identity of the out parameter, and the
// provider's own values -- there is no state, no task and no lifecycle here beyond the optional `refresh` hint.
//
// THE SENTINELS ARE THE POINT, and they are not all zero: the display metrics and the device info start their
// numerics at -1, "not reported", so a host that supplies only some of them leaves the rest visibly unknown rather
// than reading as a real zero; the capability flags start false, and the four safe-area edges start 0 because a zero
// inset is a real and common value there. A test that asserted "zeroed" for all four would be asserting the wrong
// thing for three of them, so each one is stated with the sentinel its own upstream suite names.
//
// The provider is a recording stand-in, as upstream's own `fakeBackend` is: it writes the same values upstream's
// writes, so the assertions are upstream's numbers. Nothing is invented for a real device -- there is no ambient
// slot to install into and no SDK/browser/log host, and the R3 boundary below is asserted against that.
#include <flight/device/device.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace device_ns = flight::device;
namespace types = flight::types;

using Capabilities = types::DeviceCapabilities;
using DisplayMetrics = types::DeviceDisplayMetrics;
using Info = types::DeviceInfo;
using Insets = types::SafeAreaInsets;

using HostSchema = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostDeviceCapability>>>;
using HostRow = flight::StructuralRef<HostSchema>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double value, double expected, const std::string& message) {
  if (value == expected) return;
  std::cerr << "FAIL: " << message << " (got " << value << ", want " << expected << ")\n";
  ++failures;
}

void check_text(const flight::String& value, const std::string& expected, const std::string& message) {
  if (value.to_utf8() == expected) return;
  std::cerr << "FAIL: " << message << " (got \"" << value.to_utf8() << "\", want \"" << expected << "\")\n";
  ++failures;
}

std::vector<std::string> abis_of(const flight::Array<flight::String>& values) {
  std::vector<std::string> out;
  for (double i = 0.0; i < static_cast<double>(values.size()); i += 1.0) out.push_back(values.element(i).to_utf8());
  return out;
}

// The provider upstream's `fakeBackend` is: it writes these exact values into the caller's out object.
struct Provider {
  int refresh_calls = 0;
  bool include_refresh = false;

  flight::Ref<types::HostDeviceCapability> capability = flight::make_ref<types::HostDeviceCapability>();

  void build() {
    capability->get_capabilities = [](flight::Ref<Capabilities> out) {
      out->has_keyboard = true;
      out->has_mouse = true;
      out->has_stylus = false;
      return out;
    };
    capability->get_display_metrics = [](flight::Ref<DisplayMetrics> out) {
      out->color_depth = 8.0;
      out->density_dpi = 440.0;
      out->logical_height = 800.0;
      out->logical_width = 360.0;
      out->physical_height = 1600.0;
      out->physical_width = 720.0;
      out->pixel_ratio = 2.0;
      return out;
    };
    capability->get_id = []() { return flight::String("test-device-id"); };
    capability->get_info = [](flight::Ref<Info> out) {
      out->arch = flight::String("arm64");
      out->available_memory = 3000000000.0;
      out->board_name = flight::String("msm8998");
      out->color_gamut = flight::String("display-p3");
      out->cpu_cores = 8.0;
      out->font_scale = 1.2;
      out->form_factor = flight::String("Phone");
      out->gpu_renderer = flight::String("Adreno 650");
      out->gpu_vendor = flight::String("Qualcomm");
      out->is_hdr = true;
      out->is_jailbroken = false;
      out->is_low_end_device = false;
      out->is_rooted = false;
      out->is_virtual = true;
      out->manufacturer = flight::String("Google");
      out->marketing_name = flight::String("Pixel 8 Pro");
      out->model = flight::String("Pixel");
      out->os_build = flight::String("TP1A.220624.014");
      out->os_name = flight::String("Android");
      out->os_version = flight::String("14");
      out->platform_string = flight::String("Linux armv8l");
      out->product_name = flight::String("husky");
      flight::Array<flight::String> abis;
      abis.push(flight::String("arm64-v8a"));
      abis.push(flight::String("armeabi-v7a"));
      out->supported_abis = abis;
      out->total_memory = 8000000000.0;
      out->web_view_version = flight::String("120.0.6099.230");
      return out;
    };
    capability->get_safe_area_insets = [](flight::Ref<Insets> out) {
      out->bottom = 16.0;
      out->left = 0.0;
      out->right = 0.0;
      out->top = 24.0;
      return out;
    };
    capability->refresh = include_refresh ? std::optional<std::function<void()>>([this]() { ++refresh_calls; })
                                          : std::nullopt;
  }

  HostRow row() {
    build();
    return flight::make_structural_ref<HostSchema>(
        flight::row_field<flight::RowKey<"getCapabilities">>(capability->get_capabilities),
        flight::row_field<flight::RowKey<"getDisplayMetrics">>(capability->get_display_metrics),
        flight::row_field<flight::RowKey<"getId">>(capability->get_id),
        flight::row_field<flight::RowKey<"getInfo">>(capability->get_info),
        flight::row_field<flight::RowKey<"getSafeAreaInsets">>(capability->get_safe_area_insets),
        flight::row_field<flight::RowKey<"refresh">>(capability->refresh));
  }
};

// --- The four descriptor constructors and their sentinels ------------------------------------------------------

void test_capabilities_start_all_false() {
  const auto caps = device_ns::create_device_capabilities();
  check(!caps->has_keyboard, "hasKeyboard starts false");
  check(!caps->has_mouse, "hasMouse starts false");
  check(!caps->has_stylus, "hasStylus starts false");
}

void test_display_metrics_start_at_the_not_reported_sentinel() {
  const auto metrics = device_ns::create_device_display_metrics();
  check_close(metrics->color_depth, -1.0, "colorDepth starts at -1, not 0");
  check_close(metrics->density_dpi, -1.0, "as does densityDpi");
  check_close(metrics->logical_height, -1.0, "logicalHeight");
  check_close(metrics->logical_width, -1.0, "logicalWidth");
  check_close(metrics->physical_height, -1.0, "physicalHeight");
  check_close(metrics->physical_width, -1.0, "physicalWidth");
  check_close(metrics->pixel_ratio, -1.0, "and pixelRatio");
}

void test_device_info_starts_at_its_three_kinds_of_sentinel() {
  const auto info = device_ns::create_device_info();

  check_text(info->arch, "", "arch starts empty");
  check_text(info->board_name, "", "as does boardName");
  check_text(info->color_gamut, "", "colorGamut");
  check_text(info->gpu_renderer, "", "gpuRenderer");
  check_text(info->gpu_vendor, "", "gpuVendor");
  check_text(info->manufacturer, "", "manufacturer");
  check_text(info->marketing_name, "", "marketingName");
  check_text(info->model, "", "model");
  check_text(info->os_build, "", "osBuild");
  check_text(info->os_name, "", "osName");
  check_text(info->os_version, "", "osVersion");
  check_text(info->platform_string, "", "platformString");
  check_text(info->product_name, "", "productName");
  check_text(info->web_view_version, "", "webViewVersion");

  check_close(info->available_memory, -1.0, "availableMemory starts at -1");
  check_close(info->cpu_cores, -1.0, "cpuCores");
  check_close(info->font_scale, -1.0, "fontScale");
  check_close(info->total_memory, -1.0, "totalMemory");

  check(!info->is_hdr, "isHdr starts false");
  check(!info->is_jailbroken, "as does isJailbroken");
  check(!info->is_low_end_device, "isLowEndDevice");
  check(!info->is_rooted, "isRooted");
  check(!info->is_virtual, "and isVirtual");

  check_text(info->form_factor, "Unknown", "formFactor starts at the Unknown form factor, not the empty string");
  check(abis_of(info->supported_abis).empty(), "and supportedAbis starts as an empty list");
}

void test_safe_area_insets_start_at_zero_not_minus_one() {
  const auto insets = device_ns::create_safe_area_insets();
  check_close(insets->top, 0.0, "the top inset starts at 0");
  check_close(insets->right, 0.0, "as do the right");
  check_close(insets->bottom, 0.0, "bottom");
  check_close(insets->left, 0.0, "and left edges");

  // The two sentinels must stay distinct kinds of "unset": a zero inset is a real measurement, so a descriptor that
  // used -1 here would report a device with no notch as having a negative inset.
  check_close(device_ns::create_device_display_metrics()->logical_width, -1.0,
              "while the display metrics use the -1 sentinel, so the two are not conflated");
}

// --- The five out-parameter reads ------------------------------------------------------------------------------

void test_capabilities_are_filled_and_the_caller_object_is_returned() {
  Provider provider;
  const auto out = device_ns::create_device_capabilities();

  const auto returned = device_ns::get_device_capabilities(provider.row(), out);

  check(returned.get() == out.get(), "the caller's own object is returned, not a copy");
  check(out->has_keyboard, "and the provider's writes are visible on it");
  check(out->has_mouse, "for every flag it set");
  check(!out->has_stylus, "including the one it cleared");
}

void test_display_metrics_are_filled_and_the_caller_object_is_returned() {
  Provider provider;
  const auto out = device_ns::create_device_display_metrics();

  const auto returned = device_ns::get_device_display_metrics(provider.row(), out);

  check(returned.get() == out.get(), "the caller's own object is returned");
  check_close(out->color_depth, 8.0, "colorDepth is overwritten with the reported value");
  check_close(out->density_dpi, 440.0, "densityDpi");
  check_close(out->logical_height, 800.0, "logicalHeight");
  check_close(out->logical_width, 360.0, "logicalWidth");
  check_close(out->physical_height, 1600.0, "physicalHeight");
  check_close(out->physical_width, 720.0, "physicalWidth");
  check_close(out->pixel_ratio, 2.0, "and pixelRatio, so no field keeps its -1 sentinel once reported");
}

void test_the_device_id_is_the_providers_string() {
  Provider provider;
  check_text(device_ns::get_device_id(provider.row()), "test-device-id",
             "the identifier is the provider's own string, unchanged");
}

void test_device_info_is_filled_and_the_caller_object_is_returned() {
  Provider provider;
  const auto out = device_ns::create_device_info();

  const auto returned = device_ns::get_device_info(provider.row(), out);

  check(returned.get() == out.get(), "the caller's own object is returned");

  check_text(out->arch, "arm64", "arch is reported");
  check_close(out->available_memory, 3000000000.0, "availableMemory is reported, replacing its -1 sentinel");
  check_text(out->board_name, "msm8998", "boardName");
  check_text(out->color_gamut, "display-p3", "colorGamut");
  check_close(out->cpu_cores, 8.0, "cpuCores");
  check_close(out->font_scale, 1.2, "fontScale carries a fraction unchanged");
  check_text(out->form_factor, "Phone", "formFactor is reported as one of the declared form factors");
  check_text(out->gpu_renderer, "Adreno 650", "gpuRenderer");
  check_text(out->gpu_vendor, "Qualcomm", "gpuVendor");
  check(out->is_hdr, "isHdr is set");
  check(!out->is_jailbroken, "while isJailbroken stays clear");
  check(!out->is_low_end_device, "and isLowEndDevice");
  check(!out->is_rooted, "and isRooted");
  check(out->is_virtual, "and isVirtual is set, so the four booleans are reported independently");
  check_text(out->manufacturer, "Google", "manufacturer");
  check_text(out->marketing_name, "Pixel 8 Pro", "marketingName");
  check_text(out->model, "Pixel", "model");
  check_text(out->os_build, "TP1A.220624.014", "osBuild");
  check_text(out->os_name, "Android", "osName");
  check_text(out->os_version, "14", "osVersion carries a numeric-looking string as a string");
  check_text(out->platform_string, "Linux armv8l", "platformString");
  check_text(out->product_name, "husky", "productName");
  check(abis_of(out->supported_abis) == std::vector<std::string>({"arm64-v8a", "armeabi-v7a"}),
        "supportedAbis arrives as a list, in order");
  check_close(out->total_memory, 8000000000.0, "totalMemory is reported");
  check_text(out->web_view_version, "120.0.6099.230", "webViewVersion");
}

void test_safe_area_insets_are_filled_and_the_caller_object_is_returned() {
  Provider provider;
  const auto out = device_ns::create_safe_area_insets();

  const auto returned = device_ns::get_safe_area_insets(provider.row(), out);

  check(returned.get() == out.get(), "the caller's own object is returned");
  check_close(out->top, 24.0, "the top inset is the provider's, not the zero it started at");
  check_close(out->bottom, 16.0, "as is the bottom");
  check_close(out->left, 0.0, "while an edge the provider leaves at zero keeps that value");
  check_close(out->right, 0.0, "on both sides");
}

// --- refresh: optional on the provider -------------------------------------------------------------------------

void test_refresh_does_not_throw_when_the_provider_omits_it() {
  Provider provider;
  check(!provider.include_refresh, "this provider does not expose refresh");

  device_ns::refresh_device_info(provider.row());   // must not throw

  check(provider.refresh_calls == 0, "and the hint does nothing rather than failing");
}

void test_refresh_is_called_when_the_provider_exposes_it() {
  Provider provider;
  provider.include_refresh = true;

  device_ns::refresh_device_info(provider.row());

  check(provider.refresh_calls == 1, "a provider exposing refresh is asked exactly once");
}

// --- The R3 boundary -------------------------------------------------------------------------------------------
//
// The upstream suite's last case is an absence: the module must export no ambient-state API (`setDeviceBackend`,
// `getDeviceBackend`, `installDeviceHostBackend`, `resetDeviceBackendForTest`, `createWebDeviceBackend`,
// `enableWebSafeAreaInsets`, `explainDeviceBackend`, `observeDeviceHostResult`). That is a property of what the
// module DOES NOT declare, and in C++ it cannot be asserted in-process: a qualified lookup of a name a namespace
// does not declare is a hard error rather than a substitution failure, measured -- `requires {
// flight::device::set_device_backend; }` fails to compile with "no member named 'set_device_backend' in namespace
// 'flight::device'" rather than evaluating false. So the half this file can assert positively is done above (every
// export the module does have is called, and the module's whole surface is those ten operations plus nothing), and
// the absence itself is recorded here as the measured compile error rather than dressed up as a runtime check.

} // namespace

int main() {
  test_capabilities_start_all_false();
  test_display_metrics_start_at_the_not_reported_sentinel();
  test_device_info_starts_at_its_three_kinds_of_sentinel();
  test_safe_area_insets_start_at_zero_not_minus_one();

  test_capabilities_are_filled_and_the_caller_object_is_returned();
  test_display_metrics_are_filled_and_the_caller_object_is_returned();
  test_the_device_id_is_the_providers_string();
  test_device_info_is_filled_and_the_caller_object_is_returned();
  test_safe_area_insets_are_filled_and_the_caller_object_is_returned();

  test_refresh_does_not_throw_when_the_provider_omits_it();
  test_refresh_is_called_when_the_provider_exposes_it();

  if (failures == 0) {
    std::cout << "device parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
