// Native parity test for @flighthq/haptics, ported from its own haptics.test.ts (19 cases).
//
// The package routes haptic requests to a host capability and returns what the provider reports. It has no tasks
// and no state of its own, so the portable semantics are: which provider call each function makes, with WHICH
// ARGUMENTS; what the defaults are where an argument is omitted; two decisions the package makes before reaching
// the provider at all (an empty pattern is false without a call); and the one fallback -- a waveform-requesting
// call to a provider with no waveform support becomes a plain timing pattern rather than a silent no-op.
//
// The provider here is a recording stand-in, which is what the upstream fixture is too: upstream's `makeHost`
// builds a `HapticsCapability` of `record(name, result)` stubs and asserts against the recorded call list. Nothing
// is invented for a device -- every member answers whatever the test sets, and there is no ambient slot, no stub
// host and no SDK backend.
//
// WHAT THIS PINS, in the order the package decides it
//
//   * Routing and the return value: every function returns the provider's own boolean, so a provider that reports
//     failure is reported as failing rather than assumed to succeed.
//   * Argument defaults: an omitted impact intensity is forwarded as the literal 1 (not as absence), and an omitted
//     waveform repeat is forwarded as -1.
//   * Ordering: each style and each notification type is forwarded as itself, in the order given.
//   * The two pre-provider decisions: an empty pattern and empty timings are false WITHOUT the provider being
//     called.
//   * The waveform fallback: with a waveform provider the amplitudes and repeat are forwarded; without one the
//     timings alone go to `vibratePattern`, so a provider lacking amplitude support still buzzes.
//   * `prepare` is optional on the provider (the one optional-chain in the package), and its absence is silent.
//   * The out-parameter is caller-owned and returned by identity, not copied.
#include <flight/haptics/haptics.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace haptics_ns = flight::haptics;
namespace types = flight::types;

using Capabilities = types::HapticsCapabilities;
using Style = types::HapticImpactStyle;
using Notification = types::HapticNotificationType;

using HostSchema = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostHapticsCapability>>>;
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

std::string utf8(const flight::String& value) { return value.to_utf8(); }

std::vector<double> to_vector(const flight::Array<double>& values) {
  std::vector<double> out;
  for (double i = 0.0; i < static_cast<double>(values.size()); i += 1.0) out.push_back(values.element(i));
  return out;
}

flight::Array<double> array_of(const std::vector<double>& values) {
  flight::Array<double> out;
  for (double value : values) out.push(value);
  return out;
}

// The caller-owned capabilities object upstream's `makeCapabilities` builds: every flag written explicitly, because
// the struct declares plain bools and a default-constructed one would leave them indeterminate.
flight::Ref<Capabilities> empty_capabilities() {
  auto out = flight::make_ref<Capabilities>();
  out->amplitude_control = false;
  out->custom_events = false;
  out->intensity = false;
  out->patterns = false;
  out->supported = false;
  return out;
}

// A provider that records what it was asked to do. `answer` is what every boolean operation reports, so a test can
// show that failure is propagated rather than swallowed.
struct Provider {
  std::vector<std::string> order;
  bool answer = true;

  // The arguments each operation was handed, kept separately so a test reads one thing at a time.
  std::string impact_style;
  std::optional<double> impact_intensity;
  std::string notification_type;
  double vibrate_duration = -1.0;
  std::vector<double> pattern;
  std::vector<double> waveform_timings;
  std::vector<double> waveform_amplitudes;
  std::optional<double> waveform_repeat;

  int cancel_calls = 0;
  int prepare_calls = 0;
  int selection_calls = 0;
  int is_supported_calls = 0;

  bool include_prepare = false;
  bool include_waveform = false;

  flight::Ref<types::HostHapticsCapability> capability = flight::make_ref<types::HostHapticsCapability>();

  // Built on each `row()` rather than at construction, so a test can set `include_prepare` or
  // `include_waveform` AFTER creating the provider and have the row reflect it -- the flags select which optional
  // members the provider implements, and reading them once in a constructor would make those cases untestable.
  void build() {
    capability->cancel = [this]() {
      order.push_back("cancel");
      ++cancel_calls;
      return answer;
    };
    capability->capabilities = [this](flight::Ref<Capabilities> out) {
      order.push_back("capabilities");
      out->patterns = true;
      out->supported = true;
      return out;
    };
    capability->is_supported = [this]() {
      order.push_back("isSupported");
      ++is_supported_calls;
      return answer;
    };
    capability->impact = [this](Style style, std::optional<double> intensity) {
      order.push_back("impact");
      impact_style = utf8(style);
      impact_intensity = intensity;
      return answer;
    };
    capability->notification = [this](Notification type) {
      order.push_back("notification");
      notification_type = utf8(type);
      return answer;
    };
    capability->selection = [this]() {
      order.push_back("selection");
      ++selection_calls;
      return answer;
    };
    capability->vibrate = [this](double duration_ms) {
      order.push_back("vibrate");
      vibrate_duration = duration_ms;
      return answer;
    };
    capability->vibrate_pattern = [this](flight::Array<double> timings) {
      order.push_back("vibratePattern");
      pattern = to_vector(timings);
      return answer;
    };
    capability->prepare = include_prepare
                              ? std::optional<std::function<void()>>([this]() {
                                  order.push_back("prepare");
                                  ++prepare_calls;
                                })
                              : std::nullopt;
    capability->vibrate_waveform =
        include_waveform
            ? std::optional<std::function<bool(flight::Array<double>, flight::Array<double>, std::optional<double>)>>(
                  [this](flight::Array<double> timings, flight::Array<double> amplitudes,
                         std::optional<double> repeat) {
                    order.push_back("vibrateWaveform");
                    waveform_timings = to_vector(timings);
                    waveform_amplitudes = to_vector(amplitudes);
                    waveform_repeat = repeat;
                    return answer;
                  })
            : std::nullopt;
  }

  HostRow row() {
    build();
    return flight::make_structural_ref<HostSchema>(
        flight::row_field<flight::RowKey<"cancel">>(capability->cancel),
        flight::row_field<flight::RowKey<"capabilities">>(capability->capabilities),
        flight::row_field<flight::RowKey<"impact">>(capability->impact),
        flight::row_field<flight::RowKey<"isSupported">>(capability->is_supported),
        flight::row_field<flight::RowKey<"notification">>(capability->notification),
        flight::row_field<flight::RowKey<"prepare">>(capability->prepare),
        flight::row_field<flight::RowKey<"selection">>(capability->selection),
        flight::row_field<flight::RowKey<"vibrate">>(capability->vibrate),
        flight::row_field<flight::RowKey<"vibratePattern">>(capability->vibrate_pattern),
        flight::row_field<flight::RowKey<"vibrateWaveform">>(capability->vibrate_waveform));
  }
};

std::vector<std::string> only(const Provider& provider, const std::string& name) {
  std::vector<std::string> found;
  for (const std::string& call : provider.order) {
    if (call == name) found.push_back(call);
  }
  return found;
}

// --- cancel --------------------------------------------------------------------------------------------------

void test_cancel_forwards_to_the_provider() {
  Provider provider;
  check(haptics_ns::cancel_device_vibration(provider.row()), "cancelling reports the provider's answer");
  check(provider.order == std::vector<std::string>({"cancel"}), "and makes exactly the one cancel call");
}

void test_failure_is_reported_rather_than_assumed_away() {
  Provider provider;
  provider.answer = false;
  check(!haptics_ns::cancel_device_vibration(provider.row()),
        "a provider that reports failure is reported as failing, not assumed to have succeeded");
  check(!haptics_ns::vibrate_device(provider.row(), 10.0), "as is a refused vibration");
  check(!haptics_ns::trigger_haptic_selection(provider.row()), "and a refused selection");
}

// --- capabilities ---------------------------------------------------------------------------------------------

void test_the_capabilities_call_fills_and_returns_the_caller_object() {
  Provider provider;
  const auto out = empty_capabilities();

  const auto returned = haptics_ns::get_haptics_capabilities(provider.row(), out);

  check(returned.get() == out.get(), "the same object the caller handed in is returned, not a copy");
  check(out->supported, "and the provider's write to it is visible on the caller's object");
  check(out->patterns, "for every flag it set, not only the first");
  check(!out->intensity, "while a flag the provider left alone keeps the caller's value");
}

void test_two_providers_answer_independently() {
  // Under an ambient model the second provider would overwrite the first; here each row is its own provider, so the
  // two answers must coexist.
  Provider supported;
  Provider unsupported;
  unsupported.answer = false;

  check(haptics_ns::get_haptics_capabilities(supported.row(), empty_capabilities())->supported,
        "the first provider reports itself supported");
  check(!haptics_ns::is_haptics_supported(unsupported.row()), "and the second reports itself unsupported");
  check(haptics_ns::is_haptics_supported(supported.row()), "without the second having disturbed the first");
}

void test_support_is_the_providers_own_answer() {
  Provider provider;
  check(haptics_ns::is_haptics_supported(provider.row()), "a provider answering true is reported supported");

  Provider refusing;
  refusing.answer = false;
  check(!haptics_ns::is_haptics_supported(refusing.row()), "and one answering false is reported unsupported");
}

// --- prepare is optional ---------------------------------------------------------------------------------------

void test_prepare_is_called_when_the_provider_implements_it() {
  Provider provider;
  provider.include_prepare = true;

  haptics_ns::prepare_haptics(provider.row());

  check(provider.prepare_calls == 1, "a provider that implements prepare is warmed up exactly once");
}

void test_an_absent_prepare_is_silent() {
  Provider provider;
  check(!provider.include_prepare, "this provider does not implement prepare");

  haptics_ns::prepare_haptics(provider.row());   // must not throw

  check(provider.prepare_calls == 0, "and the hint does nothing rather than failing");
  check(provider.order.empty(), "reaching no provider call at all");
}

// --- impact ---------------------------------------------------------------------------------------------------

void test_an_omitted_intensity_is_forwarded_as_one() {
  Provider provider;
  haptics_ns::trigger_haptic_impact(provider.row(), flight::String("heavy"));

  check(provider.order == std::vector<std::string>({"impact"}), "the impact operation is the one that runs");
  check(provider.impact_style == "heavy", "carrying the style it was given");
  check(provider.impact_intensity.has_value(), "and a present intensity rather than an absent one");
  check_close(provider.impact_intensity.value_or(-1.0), 1.0, "whose value is the default 1");
}

void test_an_explicit_intensity_is_forwarded_unchanged() {
  Provider provider;
  haptics_ns::trigger_haptic_impact(provider.row(), flight::String("light"), 0.25);

  check(provider.impact_intensity.has_value(), "an explicit intensity is present");
  check_close(provider.impact_intensity.value_or(-1.0), 0.25,
              "and forwarded unchanged, not replaced by the default");
}

void test_every_impact_style_is_forwarded_in_order() {
  Provider provider;
  std::vector<std::string> seen;
  for (const char* style : {"heavy", "light", "medium", "rigid", "soft"}) {
    haptics_ns::trigger_haptic_impact(provider.row(), flight::String(style));
    seen.push_back(provider.impact_style);
  }

  check(seen == std::vector<std::string>({"heavy", "light", "medium", "rigid", "soft"}),
        "each style reaches the provider as itself, in the order it was given");
}

void test_every_notification_type_is_forwarded_in_order() {
  Provider provider;
  std::vector<std::string> seen;
  for (const char* type : {"error", "success", "warning"}) {
    haptics_ns::trigger_haptic_notification(provider.row(), flight::String(type));
    seen.push_back(provider.notification_type);
  }

  check(seen == std::vector<std::string>({"error", "success", "warning"}),
        "each notification type reaches the provider as itself, in order");
  check(provider.order == std::vector<std::string>({"notification", "notification", "notification"}),
        "through the notification operation, once each");
}

void test_selection_forwards_to_the_provider() {
  Provider provider;
  check(haptics_ns::trigger_haptic_selection(provider.row()), "selection reports the provider's answer");
  check(provider.selection_calls == 1, "having made exactly one selection call");
}

// --- vibration ------------------------------------------------------------------------------------------------

void test_the_duration_is_forwarded() {
  Provider provider;
  haptics_ns::vibrate_device(provider.row(), 42.0);

  check(provider.order == std::vector<std::string>({"vibrate"}), "the vibrate operation is the one that runs");
  check_close(provider.vibrate_duration, 42.0, "carrying the duration it was given");
}

void test_a_nonempty_pattern_reaches_the_provider() {
  Provider provider;
  haptics_ns::vibrate_device_pattern(provider.row(), array_of({10.0, 20.0}));

  check(provider.pattern == std::vector<double>({10.0, 20.0}), "the pattern arrives with its values and order");
  check(provider.order == std::vector<std::string>({"vibratePattern"}), "through the pattern operation");
}

void test_an_empty_pattern_is_refused_before_the_provider_is_reached() {
  Provider provider;
  check(!haptics_ns::vibrate_device_pattern(provider.row(), array_of({})),
        "an empty pattern is refused");
  check(provider.order.empty(), "and the provider is never called, so no empty pattern reaches a device");
}

// --- the waveform fallback ------------------------------------------------------------------------------------

void test_a_waveform_provider_receives_timings_amplitudes_and_repeat() {
  Provider provider;
  provider.include_waveform = true;

  check(haptics_ns::vibrate_device_waveform(provider.row(), array_of({10.0, 20.0}), array_of({255.0, 128.0}), 1.0),
        "a provider that implements the waveform is used");
  check(provider.waveform_timings == std::vector<double>({10.0, 20.0}), "with the timings it was given");
  check(provider.waveform_amplitudes == std::vector<double>({255.0, 128.0}), "and the amplitudes");
  check(provider.waveform_repeat.has_value(), "and a present repeat");
  check_close(provider.waveform_repeat.value_or(-2.0), 1.0, "carrying the explicit value");
  check(provider.order == std::vector<std::string>({"vibrateWaveform"}), "through the waveform operation alone");
}

void test_an_omitted_repeat_is_forwarded_as_minus_one() {
  Provider provider;
  provider.include_waveform = true;

  haptics_ns::vibrate_device_waveform(provider.row(), array_of({10.0}), array_of({255.0}));

  check(provider.waveform_repeat.has_value(), "the repeat is still present when omitted");
  check_close(provider.waveform_repeat.value_or(0.0), -1.0, "with the default -1, which means loop forever");
}

void test_a_provider_without_waveform_falls_back_to_the_timings() {
  Provider provider;
  check(!provider.include_waveform, "this provider does not implement the waveform");

  check(haptics_ns::vibrate_device_waveform(provider.row(), array_of({10.0, 20.0}), array_of({255.0, 128.0})),
        "a waveform request to it still succeeds");
  check(provider.order == std::vector<std::string>({"vibratePattern"}),
        "by falling back to the plain pattern operation rather than doing nothing");
  check(provider.pattern == std::vector<double>({10.0, 20.0}),
        "carrying the timings, since every provider can express those");
}

void test_empty_timings_are_refused_before_either_path() {
  Provider provider;
  check(!haptics_ns::vibrate_device_waveform(provider.row(), array_of({}), array_of({})),
        "empty timings are refused");
  check(provider.order.empty(), "before either the waveform or the fallback is reached");

  Provider waveform_provider;
  waveform_provider.include_waveform = true;
  check(!haptics_ns::vibrate_device_waveform(waveform_provider.row(), array_of({}), array_of({255.0})),
        "and a waveform provider is refused the same way, so the guard is not the fallback's");
  check(waveform_provider.order.empty(), "reaching no provider call either");
}

} // namespace

int main() {
  test_cancel_forwards_to_the_provider();
  test_failure_is_reported_rather_than_assumed_away();

  test_the_capabilities_call_fills_and_returns_the_caller_object();
  test_two_providers_answer_independently();
  test_support_is_the_providers_own_answer();

  test_prepare_is_called_when_the_provider_implements_it();
  test_an_absent_prepare_is_silent();

  test_an_omitted_intensity_is_forwarded_as_one();
  test_an_explicit_intensity_is_forwarded_unchanged();
  test_every_impact_style_is_forwarded_in_order();
  test_every_notification_type_is_forwarded_in_order();
  test_selection_forwards_to_the_provider();

  test_the_duration_is_forwarded();
  test_a_nonempty_pattern_reaches_the_provider();
  test_an_empty_pattern_is_refused_before_the_provider_is_reached();

  test_a_waveform_provider_receives_timings_amplitudes_and_repeat();
  test_an_omitted_repeat_is_forwarded_as_minus_one();
  test_a_provider_without_waveform_falls_back_to_the_timings();
  test_empty_timings_are_refused_before_either_path();

  if (failures == 0) {
    std::cout << "haptics parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
