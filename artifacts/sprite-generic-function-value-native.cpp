// Run unchanged against baseline/candidate headers emitted from the same source.
#include <probe/required_unknown.hpp>
#include <probe/optional_unknown.hpp>
#include <probe/constrained_exact.hpp>
#include <probe/default_generic.hpp>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool condition, const char* label) {
  if (!condition) throw std::runtime_error(label);
  ++checks;
}
}
int main() {
  try {
    using Required = double (*)(double);
    auto required = probe_required_unknown::select<double>();
    auto required_address = required.target<Required>();
    check(required_address && *required_address == &probe_required_unknown::identity<double>,
          "required fallback is original specialized function pointer");
    check(required(4.5) == 4.5, "required argument used");
    check(required(0) == 0, "zero argument preserved");
    check(required(-7) == -7, "negative argument preserved");
    auto second = probe_required_unknown::select<double>();
    check(second.target<Required>() && *second.target<Required>() == *required_address,
          "same signature retains same original function pointer");

    using Optional = std::optional<double> (*)(std::optional<double>);
    auto optional = probe_optional_unknown::select<double>();
    auto optional_address = optional.target<Optional>();
    check(optional_address && *optional_address == &probe_optional_unknown::identity<double>,
          "optional fallback is original specialized function pointer");
    check(optional(std::nullopt) == std::nullopt, "optional absence preserved");
    check(optional(0).value() == 0, "supplied optional zero used");
    check(optional(37).value() == 37, "supplied optional argument used");
    check(optional(-2).value() == -2, "supplied optional negative used");
    int supplied_calls = 0;
    probe_optional_unknown::Factory<double> supplied = [&](std::optional<double> value) {
      ++supplied_calls;
      return value ? std::optional<double>(*value + 1) : std::nullopt;
    };
    auto chosen = probe_optional_unknown::select<double>(supplied);
    check(supplied_calls == 0, "supplied callback is not invoked during selection");
    check(chosen(8).value() == 9 && supplied_calls == 1, "supplied optional callback receives value once");
    check(!chosen(std::nullopt) && supplied_calls == 2, "supplied callback receives absence once");

    using Owner = std::shared_ptr<int>;
    auto original = std::make_shared<int>(11);
    std::weak_ptr<int> weak = original;
    auto owners = probe_required_unknown::select<Owner>();
    auto retained = owners(original);
    check(retained == original, "original function returns same supplied allocation");
    *original = 12;
    check(*retained == 12, "live writes retain original allocation");
    original.reset();
    check(!weak.expired() && *retained == 12, "returned owner retains original lifetime");
    retained.reset();
    check(weak.expired(), "function pointer retains no input after final returned owner release");

    auto optional_original = std::make_shared<int>(21);
    std::weak_ptr<int> optional_weak = optional_original;
    auto optional_owners = probe_optional_unknown::select<Owner>();
    auto optional_retained = optional_owners(optional_original);
    check(optional_retained && *optional_retained == optional_original,
          "supplied optional owner is used without fabrication");
    optional_original.reset();
    check(!optional_weak.expired() && **optional_retained == 21, "optional returned owner retains allocation");
    optional_retained.reset();
    check(optional_weak.expired(), "optional original allocation releases with final result");
    check(!optional_owners(std::nullopt), "optional owner absence is not materialized");

    const auto error = std::make_shared<int>(42);
    int effects = 0;
    probe_optional_unknown::Factory<double> throwing = [error, &effects](std::optional<double>)
        -> std::optional<double> { ++effects; throw error; };
    auto throwing_chosen = probe_optional_unknown::select<double>(throwing);
    check(effects == 0, "selection preserves lazy error boundary");
    bool original_error = false;
    try { (void)throwing_chosen(5); }
    catch (const Owner& caught) { original_error = caught == error; }
    check(original_error && effects == 1, "original thrown owner and effect count preserved");

    auto constrained = probe_constrained_exact::select<double>();
    check(constrained.target<Required>() &&
          *constrained.target<Required>() == &probe_constrained_exact::identity<double>,
          "proven constraint retains original function pointer");
    check(constrained(13) == 13, "proven constraint argument used");
    auto authored_default = probe_default_generic::select<double>();
    check(authored_default.target<Required>() &&
          *authored_default.target<Required>() == &probe_default_generic::identity<double, double>,
          "authored unused generic default preserved");
    check(authored_default(19) == 19, "authored default has original behavior");
    std::cout << "PASS " << checks << " original generic function-value argument/identity/lifetime/error controls\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
