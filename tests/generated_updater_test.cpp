#include <flight/updater/updater.hpp>

#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

flight::Ref<flight::types::DownloadedUpdate> make_update(flight::String version) {
  using namespace flight::types;
  const auto info = flight::make_ref<UpdateInfo>(UpdateInfo{
      .download_size_bytes = std::nullopt,
      .is_mandatory = std::nullopt,
      .minimum_os_version = std::nullopt,
      .notes = std::nullopt,
      .release_date = std::nullopt,
      .sha512 = std::nullopt,
      .version = std::move(version),
  });
  return flight::make_ref<DownloadedUpdate>(DownloadedUpdate{
      .entity_runtime_key = std::nullopt,
      .info = flight::StructuralRef<flight::RowReadonly<
          flight::RowOf<flight::Ref<UpdateInfo>>>>(info),
  });
}

}  // namespace

int main() {
  using flight::String;
  using namespace flight::types;

  int check_calls = 0;
  int destroy_calls = 0;
  int install_calls = 0;
  auto next_check = flight::updater::not_available;
  auto next_install = flight::make_ref<AppUpdateInstallOutcome>(
      AppUpdateInstallOutcome{.reason = String("ok")});
  bool reject_check = false;
  bool reject_install = false;
  flight::Ref<DownloadedUpdate> installed_update;
  const auto origin = flight::make_ref<HostUpdaterCommandCapability>(
      HostUpdaterCommandCapability{
          .check = [&]() {
            ++check_calls;
            if (reject_check) {
              return flight::Task<AppUpdateCheckOutcome>::reject(
                  std::runtime_error("private native detail"));
            }
            return flight::Task<AppUpdateCheckOutcome>::resolve(next_check);
          },
          .destroy = [&]() { ++destroy_calls; },
          .install = [&](flight::Ref<DownloadedUpdate> update) {
            ++install_calls;
            installed_update = std::move(update);
            if (reject_install) {
              return flight::Task<flight::Ref<AppUpdateInstallOutcome>>::reject(
                  std::runtime_error("private install detail"));
            }
            return flight::Task<flight::Ref<AppUpdateInstallOutcome>>::resolve(
                next_install);
          },
      });

  const auto first_not_available =
      flight::updater::check_for_app_update(origin).get();
  const auto second_not_available =
      flight::updater::check_for_app_update(origin).get();
  if (!check(flight::updater::check_reason(first_not_available) ==
                 String("not-available") &&
                 check_calls == 2,
             "check did not await exactly one provider operation") ||
      !check(std::get<flight::updater::CheckReasonRow>(first_not_available) ==
                 std::get<flight::updater::CheckReasonRow>(second_not_available),
             "check sentinel identity was not stable")) {
    return 1;
  }

  reject_check = true;
  const auto rejected_check = flight::updater::check_for_app_update(origin).get();
  reject_check = false;
  next_check = flight::updater::make_check_reason(String("unexpected"));
  const auto unknown_check = flight::updater::check_for_app_update(origin).get();
  if (!check(flight::updater::check_reason(rejected_check) ==
                 String("operation-failed") &&
                 flight::updater::check_reason(unknown_check) ==
                     String("operation-failed"),
             "provider rejection or unknown reason leaked through check")) {
    return 1;
  }

  const auto update = make_update(String("1.2.3"));
  next_check = flight::updater::downloaded_outcome(update);
  const auto downloaded = flight::updater::check_for_app_update(origin).get();
  if (!check(flight::updater::check_reason(downloaded) == String("downloaded") &&
                 flight::updater::downloaded_update(downloaded) == update,
             "downloaded check did not retain the update identity")) {
    return 1;
  }

  int replacement_installs = 0;
  const auto replacement = flight::make_ref<HostUpdaterCommandCapability>(
      HostUpdaterCommandCapability{
          .check = []() {
            return flight::Task<AppUpdateCheckOutcome>::resolve(
                flight::updater::not_available);
          },
          .destroy = []() {},
          .install = [&](flight::Ref<DownloadedUpdate>) {
            ++replacement_installs;
            return flight::Task<flight::Ref<AppUpdateInstallOutcome>>::resolve(
                flight::make_ref<AppUpdateInstallOutcome>(
                    AppUpdateInstallOutcome{.reason = String("ok")}));
          },
      });
  const auto installed =
      flight::updater::install_downloaded_update(replacement, update).get();
  if (!check(installed->reason == String("ok") && install_calls == 1 &&
                 replacement_installs == 0 && installed_update == update,
             "provider replacement observed or rerouted the downloaded handle")) {
    return 1;
  }

  bool rejected_unowned = false;
  try {
    static_cast<void>(
        flight::updater::install_downloaded_update(origin, update).get());
  } catch (const flight::TypeError&) {
    rejected_unowned = true;
  }
  if (!check(rejected_unowned, "consumed or foreign update did not reject")) return 1;

  const auto retry_update = make_update(String("2.0.0"));
  next_check = flight::updater::downloaded_outcome(retry_update);
  static_cast<void>(flight::updater::check_for_app_update(origin).get());
  reject_install = true;
  const auto failed_install =
      flight::updater::install_downloaded_update(origin, retry_update).get();
  reject_install = false;
  const auto retried_install =
      flight::updater::install_downloaded_update(origin, retry_update).get();
  if (!check(failed_install->reason == String("operation-failed") &&
                 retried_install->reason == String("ok"),
             "install failure leaked detail or consumed retry ownership")) {
    return 1;
  }

  const auto conflict_update = make_update(String("3.0.0"));
  next_check = flight::updater::downloaded_outcome(conflict_update);
  static_cast<void>(flight::updater::check_for_app_update(origin).get());
  const auto conflict_provider = flight::make_ref<HostUpdaterCommandCapability>(
      HostUpdaterCommandCapability{
          .check = [conflict_update]() {
            return flight::Task<AppUpdateCheckOutcome>::resolve(
                flight::updater::downloaded_outcome(conflict_update));
          },
          .destroy = []() {},
          .install = [](flight::Ref<DownloadedUpdate>) {
            return flight::Task<flight::Ref<AppUpdateInstallOutcome>>::resolve(
                flight::updater::install_ok);
          },
      });
  const auto conflict =
      flight::updater::check_for_app_update(conflict_provider).get();
  if (!check(flight::updater::check_reason(conflict) ==
                 String("operation-failed"),
             "a second provider stole an owned downloaded handle")) {
    return 1;
  }

  flight::updater::destroy_updater(origin);
  if (!check(destroy_calls == 1, "destroy was not delegated synchronously")) return 1;

  return 0;
}
