// Hand-written guard boundary preserving JavaScript callback identity and log-record construction.
#pragma once

#include <optional>
#include <utility>

#include <flight/runtime.hpp>
#include <flight/assets/asset_library.hpp>
#include <flight/log/log.hpp>
#include <flight/types/assets.hpp>
#include <flight/types/log.hpp>

namespace flight::assets {

using flight::types::AssetAcquireGuard;
using flight::types::AssetLibrary;
using flight::types::AssetLoadExplanation;

using AssetLibraryView =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<AssetLibrary>>>>;
using AssetLoadExplanationView =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<AssetLoadExplanation>>>>;

inline void warn_on_asset_acquire_failure(
    AssetLibraryView, AssetLoadExplanationView explanation) {
  const auto status = explanation->status;
  if (status != flight::String("missing-descriptor") &&
      status != flight::String("missing-loader")) {
    return;
  }
  const auto id = explanation->id;
  const auto type = explanation->type;
  const flight::String message = status == flight::String("missing-descriptor")
      ? flight::String("acquireAsset: no descriptor is registered for id \"") + id +
            flight::String("\"; call registerAssetDescriptor or registerAssetManifest before acquiring it.")
      : flight::String("acquireAsset: no loader is registered for type \"") +
            (type.has_value() ? type.value() : flight::String("null")) +
            flight::String("\"; call registerAssetLoader before acquiring \"") + id +
            flight::String("\".");
  flight::Record<flight::String, flight::Any> data{
      {flight::String("id"), flight::Any(id)},
      {flight::String("refCount"), flight::Any(explanation->ref_count)},
      {flight::String("status"), flight::Any(status)},
      {flight::String("type"), flight::Any(type)},
      {flight::String("message"), flight::Any(message)},
  };
  flight::log::log_once(
      flight::String("assets:acquire:") + status + flight::String(":") +
          type.value_or(flight::String("")) + flight::String(":") + id,
      flight::types::LogLevel::Warn, std::move(data),
      std::optional<std::optional<flight::String>>{flight::String("assets")});
}

inline const AssetAcquireGuard asset_acquire_failure_guard{
    warn_on_asset_acquire_failure};

inline bool are_asset_guards_enabled(AssetLibraryView library) {
  return flight::row_get<flight::RowKey<"runtime">>(library)->acquire_guard ==
      asset_acquire_failure_guard;
}

inline void disable_asset_guards(AssetLibraryView library) {
  set_asset_acquire_guard(std::move(library), std::nullopt);
}

inline void enable_asset_guards(AssetLibraryView library) {
  set_asset_acquire_guard(
      std::move(library), std::optional<AssetAcquireGuard>{asset_acquire_failure_guard});
}

}  // namespace flight::assets
