#pragma once

#include <optional>
#include <utility>
#include <variant>

#include <flight/error.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/updater.hpp>
#include <flight/weak_map.hpp>

namespace flight::updater {

using CheckReason = flight::types::reason_01523507f7b21613;
using CheckReasonRow = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<CheckReason>>>>;
// The downloaded-reason shape is an ANONYMOUS structural type, and the emitter names those by a hash
// of their members -- a hash that this override cannot track, because it is derived from the pin's own
// emission rather than from anything here. Naming the hash broke this override at the a627849 pin: the
// generated copy in flight/types moved to reason_update_457a6ef5d21e9081 while the hand-written copy
// still named the previous hash, and the whole module stopped compiling on an undeclared type.
// So the type is taken from the public variant instead. `AppUpdateCheckOutcome`'s first alternative IS
// the downloaded-reason row, and its element type IS the struct, whatever the current hash is: the
// override now tracks the emitter's naming instead of freezing one spelling of it.
using DownloadedReasonRow = std::variant_alternative_t<0, flight::types::AppUpdateCheckOutcome>;
using DownloadedReason = typename DownloadedReasonRow::object_type;

[[nodiscard]] inline flight::types::AppUpdateCheckOutcome make_check_reason(
    flight::String reason) {
  return flight::types::AppUpdateCheckOutcome{CheckReasonRow(
      flight::make_ref<CheckReason>(CheckReason{.reason = std::move(reason)}))};
}

inline const flight::types::AppUpdateCheckOutcome check_in_progress =
    make_check_reason(flight::String("check-in-progress"));
inline const flight::types::AppUpdateCheckOutcome not_available =
    make_check_reason(flight::String("not-available"));
inline const flight::types::AppUpdateCheckOutcome operation_failed =
    make_check_reason(flight::String("operation-failed"));
inline const flight::Ref<flight::types::AppUpdateInstallOutcome>
    install_operation_failed =
        flight::make_ref<flight::types::AppUpdateInstallOutcome>(
            flight::types::AppUpdateInstallOutcome{
                .reason = flight::String("operation-failed")});
inline const flight::Ref<flight::types::AppUpdateInstallOutcome> install_ok =
    flight::make_ref<flight::types::AppUpdateInstallOutcome>(
        flight::types::AppUpdateInstallOutcome{.reason = flight::String("ok")});

inline flight::WeakMap<
    flight::Ref<flight::types::DownloadedUpdate>,
    flight::Ref<flight::types::HostUpdaterCommandCapability>>
    download_owners;

[[nodiscard]] inline flight::String check_reason(
    const flight::types::AppUpdateCheckOutcome& outcome) {
  return std::visit([](const auto& arm) { return arm->reason; }, outcome);
}

[[nodiscard]] inline std::optional<flight::Ref<flight::types::DownloadedUpdate>>
downloaded_update(const flight::types::AppUpdateCheckOutcome& outcome) {
  return std::visit(
      [](const auto& arm)
          -> std::optional<flight::Ref<flight::types::DownloadedUpdate>> {
        if constexpr (requires { arm->update; }) {
          return arm->update;
        } else {
          return std::nullopt;
        }
      },
      outcome);
}

[[nodiscard]] inline flight::types::AppUpdateCheckOutcome downloaded_outcome(
    flight::Ref<flight::types::DownloadedUpdate> update) {
  return flight::types::AppUpdateCheckOutcome{DownloadedReasonRow(
      flight::make_ref<DownloadedReason>(DownloadedReason{
          .reason = flight::String("downloaded"),
          .update = std::move(update),
      }))};
}

[[nodiscard]] inline flight::Task<flight::types::AppUpdateCheckOutcome>
check_for_app_update(
    flight::Ref<flight::types::HostUpdaterCommandCapability>
        host_updater_command) {
  const auto provider = host_updater_command;
  try {
    auto outcome = co_await host_updater_command->check();
    const auto reason = check_reason(outcome);
    if (reason == flight::String("check-in-progress")) {
      co_return check_in_progress;
    }
    if (reason == flight::String("not-available")) {
      co_return not_available;
    }
    if (reason == flight::String("operation-failed")) {
      co_return operation_failed;
    }
    if (reason != flight::String("downloaded")) {
      co_return operation_failed;
    }
    auto update = downloaded_update(outcome);
    if (!update.has_value()) co_return operation_failed;
    auto existing_owner = download_owners.get(update.value());
    if (existing_owner.has_value() && existing_owner.value() != provider) {
      co_return operation_failed;
    }
    download_owners.set(update.value(), provider);
    co_return downloaded_outcome(update.value());
  } catch (...) {
    co_return operation_failed;
  }
}

[[nodiscard]] inline flight::Task<
    flight::Ref<flight::types::AppUpdateInstallOutcome>>
install_downloaded_update(
    flight::Ref<flight::types::HostUpdaterCommandCapability>
        host_updater_command,
    flight::Ref<flight::types::DownloadedUpdate> update) {
  auto origin = download_owners.get(update);
  if (!origin.has_value()) {
    throw flight::TypeError(flight::String(
        "Downloaded update did not originate from a completed check"));
  }
  try {
    auto outcome = co_await (host_updater_command == origin.value()
                                 ? host_updater_command->install(update)
                                 : origin.value()->install(update));
    if (outcome->reason != flight::String("ok")) {
      co_return install_operation_failed;
    }
    static_cast<void>(download_owners.erase(update));
    co_return install_ok;
  } catch (...) {
    co_return install_operation_failed;
  }
}

inline void destroy_updater(
    flight::Ref<flight::types::HostUpdaterCommandCapability>
        host_updater_command) {
  host_updater_command->destroy();
}

}  // namespace flight::updater
