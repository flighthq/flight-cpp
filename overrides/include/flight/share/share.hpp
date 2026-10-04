#pragma once

#include <functional>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

#include <flight/entity/entity.hpp>
#include <flight/runtime.hpp>
#include <flight/set.hpp>
#include <flight/signals/emitter.hpp>
#include <flight/signals/signal.hpp>
#include <flight/signals/slot.hpp>
#include <flight/types/share.hpp>
#include <flight/types/share_file.hpp>
#include <flight/types/share_signals.hpp>

namespace flight::share {

using ShareResultSignal =
    std::function<void(flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::ShareResult>>>>)>;

inline flight::Set<flight::Ref<flight::types::ShareSignals>> attached_signals;

[[nodiscard]] inline bool non_empty_share_field(const flight::String& field) {
  return field != flight::String("");
}

[[nodiscard]] inline bool non_empty_share_field(
    const std::optional<flight::String>& field) {
  return field.has_value() && field.value() != flight::String("");
}

[[nodiscard]] inline bool has_share_content_fields(
    flight::types::ShareContent content) {
  return std::visit(
      [](const auto& arm) {
        return non_empty_share_field(arm->title) ||
               non_empty_share_field(arm->text) ||
               non_empty_share_field(arm->url);
      },
      content);
}

inline void initialize_share_signals(
    flight::types::EntityConstruction<flight::Ref<flight::types::ShareSignals>> out) {
  out->on_share_result = flight::signals::create_signal<ShareResultSignal>();
}

[[nodiscard]] inline flight::Ref<flight::types::ShareSignals>
enable_share_signals() {
  auto out =
      flight::entity::allocate_entity<flight::Ref<flight::types::ShareSignals>>();
  initialize_share_signals(out);
  return flight::entity::finish_entity(out);
}

inline void attach_share_signals(
    flight::Ref<flight::types::ShareSignals> signals) {
  attached_signals.add(std::move(signals));
}

inline void detach_share_signals(
    const flight::Ref<flight::types::ShareSignals>& signals) {
  static_cast<void>(attached_signals.erase(signals));
}

inline void dispose_share_signals(
    flight::Ref<flight::types::ShareSignals> signals) {
  detach_share_signals(signals);
  flight::signals::clear_signal(signals->on_share_result);
}

[[nodiscard]] inline bool can_share_content(
    flight::Ref<flight::types::HostShareContentCapability> host_share_content,
    flight::types::ShareContent content) {
  return has_share_content_fields(content) &&
         host_share_content->can_share_content(std::move(content));
}

[[nodiscard]] inline bool is_share_file_valid(
    flight::Ref<flight::types::ShareFile> file) {
  return file->name != flight::String("") &&
         file->mime_type != flight::String("") &&
         file->data_url.starts_with(flight::String("data:")) &&
         file->data_url.includes(flight::String(","));
}

[[nodiscard]] inline flight::Task<bool> share_content(
    flight::Ref<flight::types::HostShareContentCapability> host_share_content,
    flight::types::ShareContent content) {
  if (!has_share_content_fields(content)) {
    return flight::Task<bool>::resolve(false);
  }
  return host_share_content->share_content(std::move(content));
}

[[nodiscard]] inline flight::Task<bool> share_text(
    flight::Ref<flight::types::HostShareContentCapability> host_share_content,
    flight::String text) {
  auto value = flight::make_ref<flight::types::text_title_url_66ef499c1c8b6060>(
      flight::types::text_title_url_66ef499c1c8b6060{
          .text = std::move(text),
          .title = std::nullopt,
          .url = std::nullopt,
      });
  return share_content(std::move(host_share_content),
                       flight::types::ShareContent{std::move(value)});
}

[[nodiscard]] inline flight::Task<bool> share_url(
    flight::Ref<flight::types::HostShareContentCapability> host_share_content,
    flight::String url) {
  auto value = flight::make_ref<flight::types::text_title_url_178ed2aab7bfa756>(
      flight::types::text_title_url_178ed2aab7bfa756{
          .text = std::nullopt,
          .title = std::nullopt,
          .url = std::move(url),
      });
  return share_content(std::move(host_share_content),
                       flight::types::ShareContent{std::move(value)});
}

[[nodiscard]] inline flight::Task<flight::Ref<flight::types::ShareResult>>
share_content_with_result(
    flight::Ref<flight::types::HostShareContentCapability> host_share_content,
    flight::types::ShareContent content) {
  if (!has_share_content_fields(content)) {
    co_return flight::make_ref<flight::types::ShareResult>(
        flight::types::ShareResult{
            .completed = false,
            .activity_type = std::nullopt,
            .dismissed = false,
        });
  }
  auto result =
      co_await host_share_content->share_content_with_result(std::move(content));
  attached_signals.for_each([&](const auto& signals) {
    flight::signals::emit_signal(signals->on_share_result, result);
  });
  co_return result;
}

[[nodiscard]] inline std::optional<flight::Ref<flight::types::ShareFilesContent>>
files_content(flight::Array<flight::Ref<flight::types::ShareFile>> files) {
  auto first = files.get(0.0);
  if (!first.has_value() ||
      !files.every([](const auto& file) { return is_share_file_valid(file); })) {
    return std::nullopt;
  }
  auto content = flight::make_ref<flight::types::ShareFilesContent>(
      flight::types::ShareFilesContent{
          .files = std::tuple{first.value(), files.slice(1.0)},
          .text = std::nullopt,
          .title = std::nullopt,
          .url = std::nullopt,
      });
  return content;
}

[[nodiscard]] inline bool can_share_files(
    flight::Ref<flight::types::HostShareFilesCapability> host_share_files,
    flight::Array<flight::Ref<flight::types::ShareFile>> files) {
  auto content = files_content(std::move(files));
  return content.has_value() &&
         host_share_files->can_share_content(content.value());
}

[[nodiscard]] inline flight::Task<bool> share_files(
    flight::Ref<flight::types::HostShareFilesCapability> host_share_files,
    flight::Array<flight::Ref<flight::types::ShareFile>> files) {
  auto content = files_content(std::move(files));
  if (!content.has_value()) return flight::Task<bool>::resolve(false);
  return host_share_files->share_content(content.value());
}

}  // namespace flight::share
