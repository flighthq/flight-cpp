#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace flight::upstream_test {

class AssertionFailure final : public std::runtime_error {
public:
  explicit AssertionFailure(std::string message) : std::runtime_error(std::move(message)) {}
};

template <typename Value>
std::string printable(const Value& value) {
  std::ostringstream stream;
  if constexpr (std::is_same_v<std::remove_cvref_t<Value>, bool>) {
    stream << (value ? "true" : "false");
  } else if constexpr (requires { stream << value; }) {
    stream << value;
  } else {
    stream << "<unprintable>";
  }
  return stream.str();
}

template <typename Actual>
class Expectation {
public:
  explicit Expectation(Actual actual) : actual_(std::move(actual)) {}

  template <typename Expected>
  void to_be(const Expected& expected) const {
    if (!(actual_ == expected)) {
      fail("to be", printable(expected));
    }
  }

  template <std::ranges::input_range Expected>
    requires std::ranges::input_range<Actual>
  void to_equal(const Expected& expected) const {
    if (!std::ranges::equal(actual_, expected)) {
      throw AssertionFailure("expected ranges to contain equal values");
    }
  }

  void to_be_close_to(double expected, int digits = 2) const
    requires std::is_convertible_v<Actual, double>
  {
    const double actual = static_cast<double>(actual_);
    const double tolerance = std::pow(10.0, -digits) / 2.0;
    if (std::isnan(actual) || std::isnan(expected) || std::abs(actual - expected) >= tolerance) {
      fail("to be close to", printable(expected));
    }
  }

  void to_be_nan() const
    requires std::is_convertible_v<Actual, double>
  {
    if (!std::isnan(static_cast<double>(actual_))) {
      fail("to be NaN", "NaN");
    }
  }

  template <typename Expected>
  void to_be_greater_than(const Expected& expected) const {
    if (!(actual_ > expected)) {
      fail("to be greater than", printable(expected));
    }
  }

  template <typename Expected>
  void to_be_greater_than_or_equal(const Expected& expected) const {
    if (!(actual_ >= expected)) {
      fail("to be greater than or equal to", printable(expected));
    }
  }

  template <typename Expected>
  void to_be_less_than(const Expected& expected) const {
    if (!(actual_ < expected)) {
      fail("to be less than", printable(expected));
    }
  }

  template <typename Expected>
  void to_be_less_than_or_equal(const Expected& expected) const {
    if (!(actual_ <= expected)) {
      fail("to be less than or equal to", printable(expected));
    }
  }

private:
  [[noreturn]] void fail(std::string_view relation, const std::string& expected) const {
    throw AssertionFailure("expected " + printable(actual_) + " " + std::string(relation) + " " + expected);
  }

  Actual actual_;
};

template <typename Actual>
auto expect(Actual&& actual) {
  return Expectation<std::decay_t<Actual>>(std::forward<Actual>(actual));
}

class Runner {
public:
  template <typename Body>
  void describe(std::string name, Body&& body) {
    descriptions_.push_back(std::move(name));
    std::forward<Body>(body)();
    descriptions_.pop_back();
  }

  template <typename Body>
  void it(std::string name, Body&& body) {
    std::string qualified;
    for (const auto& description : descriptions_) {
      if (!qualified.empty()) qualified += " > ";
      qualified += description;
    }
    if (!qualified.empty()) qualified += " > ";
    qualified += name;
    tests_.push_back(Test{std::move(qualified), std::forward<Body>(body)});
  }

  int run() const {
    std::size_t failed = 0;
    for (const auto& test : tests_) {
      try {
        test.body();
      } catch (const std::exception& error) {
        ++failed;
        std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
      } catch (...) {
        ++failed;
        std::cerr << "FAIL " << test.name << ": non-standard exception\n";
      }
    }
    std::cout << (tests_.size() - failed) << "/" << tests_.size() << " upstream parity tests passed\n";
    return failed == 0 ? 0 : 1;
  }

private:
  struct Test {
    std::string name;
    std::function<void()> body;
  };

  std::vector<std::string> descriptions_;
  std::vector<Test> tests_;
};

} // namespace flight::upstream_test
