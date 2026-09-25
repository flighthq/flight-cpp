#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include <flight/string.hpp>

namespace flight {

class Date {
 public:
  Date() noexcept : milliseconds_(current_milliseconds()) {}
  explicit Date(double milliseconds) noexcept : milliseconds_(time_clip(milliseconds)) {}

  [[nodiscard]] static double now() noexcept { return current_milliseconds(); }

  [[nodiscard]] double get_date() const noexcept {
    if (!valid()) return invalid_number();
    return static_cast<double>(calendar_projection().day);
  }

  [[nodiscard]] double get_day() const noexcept {
    if (!valid()) return invalid_number();
    return static_cast<double>(calendar_projection().weekday);
  }

  [[nodiscard]] double get_full_year() const noexcept {
    if (!valid()) return invalid_number();
    return static_cast<double>(calendar_projection().year);
  }

  [[nodiscard]] double get_hours() const noexcept {
    return valid() ? static_cast<double>(calendar_projection().hours) : invalid_number();
  }

  [[nodiscard]] double get_milliseconds() const noexcept {
    return valid() ? static_cast<double>(calendar_projection().milliseconds) : invalid_number();
  }

  [[nodiscard]] double get_minutes() const noexcept {
    return valid() ? static_cast<double>(calendar_projection().minutes) : invalid_number();
  }

  [[nodiscard]] double get_month() const noexcept {
    if (!valid()) return invalid_number();
    return static_cast<double>(calendar_projection().month - 1);
  }

  [[nodiscard]] double get_seconds() const noexcept {
    return valid() ? static_cast<double>(calendar_projection().seconds) : invalid_number();
  }

  [[nodiscard]] double get_time() const noexcept { return milliseconds_; }

  [[nodiscard]] String to_isostring() const {
    if (!valid()) throw std::range_error("flight::Date is outside the supported calendar range");

    const auto date = calendar_projection();
    std::ostringstream output;
    output << std::setfill('0');
    if (date.year >= 0 && date.year <= 9999) {
      output << std::setw(4) << date.year;
    } else {
      output << (date.year < 0 ? '-' : '+') << std::setw(6)
             << (date.year < 0 ? -date.year : date.year);
    }
    output << '-' << std::setw(2) << date.month << '-' << std::setw(2) << date.day << 'T'
           << std::setw(2) << date.hours << ':' << std::setw(2) << date.minutes << ':'
           << std::setw(2) << date.seconds << '.' << std::setw(3) << date.milliseconds << 'Z';
    return String::from_utf8(output.str());
  }

  [[nodiscard]] String to_string() const { return to_isostring(); }

  [[nodiscard]] bool valid() const noexcept { return std::isfinite(milliseconds_); }

 private:
  static constexpr std::int64_t milliseconds_per_second = 1000;
  static constexpr std::int64_t milliseconds_per_minute = 60 * milliseconds_per_second;
  static constexpr std::int64_t milliseconds_per_hour = 60 * milliseconds_per_minute;
  static constexpr std::int64_t milliseconds_per_day = 24 * milliseconds_per_hour;

  struct CalendarProjection {
    std::int64_t year;
    unsigned month;
    unsigned day;
    unsigned weekday;
    unsigned hours;
    unsigned minutes;
    unsigned seconds;
    unsigned milliseconds;
  };

  // Converts a day count relative to 1970-01-01 into the proleptic Gregorian calendar. Unlike
  // std::chrono::year_month_day, this remains defined across JavaScript's complete TimeClip range
  // of plus or minus 100,000,000 days (years -271821 through +275760).
  [[nodiscard]] static CalendarProjection calendar_date(std::int64_t epoch_days) noexcept {
    const auto shifted_days = epoch_days + 719468;
    const auto era =
        (shifted_days >= 0 ? shifted_days : shifted_days - 146096) / 146097;
    const auto day_of_era = static_cast<unsigned>(shifted_days - era * 146097);
    const auto year_of_era = static_cast<unsigned>(
        (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) /
        365);
    auto year = static_cast<std::int64_t>(year_of_era) + era * 400;
    const auto day_of_year =
        day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    const auto month_prime = static_cast<unsigned>((5 * day_of_year + 2) / 153);
    const auto day = static_cast<unsigned>(day_of_year - (153 * month_prime + 2) / 5 + 1);
    const auto month =
        static_cast<unsigned>(static_cast<int>(month_prime) + (month_prime < 10 ? 3 : -9));
    year += month <= 2 ? 1 : 0;

    auto weekday = (epoch_days + 4) % 7;
    if (weekday < 0) weekday += 7;
    return {year, month, day, static_cast<unsigned>(weekday), 0, 0, 0, 0};
  }

  [[nodiscard]] CalendarProjection calendar_projection() const noexcept {
    const auto total_milliseconds = static_cast<std::int64_t>(milliseconds_);
    auto epoch_days = total_milliseconds / milliseconds_per_day;
    auto day_milliseconds = total_milliseconds % milliseconds_per_day;
    if (day_milliseconds < 0) {
      --epoch_days;
      day_milliseconds += milliseconds_per_day;
    }

    auto result = calendar_date(epoch_days);
    result.hours = static_cast<unsigned>(day_milliseconds / milliseconds_per_hour);
    day_milliseconds %= milliseconds_per_hour;
    result.minutes = static_cast<unsigned>(day_milliseconds / milliseconds_per_minute);
    day_milliseconds %= milliseconds_per_minute;
    result.seconds = static_cast<unsigned>(day_milliseconds / milliseconds_per_second);
    result.milliseconds = static_cast<unsigned>(day_milliseconds % milliseconds_per_second);
    return result;
  }

  [[nodiscard]] static double current_milliseconds() noexcept {
    const auto elapsed = std::chrono::system_clock::now().time_since_epoch();
    return time_clip(std::chrono::duration<double, std::milli>(elapsed).count());
  }

  [[nodiscard]] static double invalid_number() noexcept {
    return std::numeric_limits<double>::quiet_NaN();
  }

  [[nodiscard]] static double time_clip(double milliseconds) noexcept {
    constexpr double limit = 8.64e15;
    if (!std::isfinite(milliseconds) || std::abs(milliseconds) > limit) {
      return std::numeric_limits<double>::quiet_NaN();
    }
    return std::trunc(milliseconds);
  }

  double milliseconds_;
};

} // namespace flight
