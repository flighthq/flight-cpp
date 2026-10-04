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
using DownloadedReason = flight::types::reason_update_938ed6a0d2da8e0c;
using DownloadedReasonRow = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<DownloadedReason>>>>;

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
