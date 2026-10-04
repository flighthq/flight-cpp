#include <flight/share/share.hpp>

#include <iostream>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

flight::types::ShareContent text_content(flight::String text) {
  return flight::types::ShareContent{
      flight::make_ref<flight::types::text_title_url_66ef499c1c8b6060>(
          flight::types::text_title_url_66ef499c1c8b6060{
              .text = std::move(text),
              .title = std::nullopt,
              .url = std::nullopt,
          })};
}

flight::types::ShareContent title_content(flight::String title) {
  return flight::types::ShareContent{
      flight::make_ref<flight::types::text_title_url_0920bc2a3ce65b06>(
          flight::types::text_title_url_0920bc2a3ce65b06{
              .text = std::nullopt,
              .title = std::move(title),
              .url = std::nullopt,
          })};
}

flight::types::ShareContent url_content(flight::String url) {
  return flight::types::ShareContent{
      flight::make_ref<flight::types::text_title_url_178ed2aab7bfa756>(
          flight::types::text_title_url_178ed2aab7bfa756{
              .text = std::nullopt,
              .title = std::nullopt,
              .url = std::move(url),
          })};
}

}  // namespace

int main() {
  using flight::String;
  using namespace flight::types;

  if (!check(!flight::share::has_share_content_fields(text_content(String(""))) &&
                 !flight::share::has_share_content_fields(title_content(String(""))) &&
                 !flight::share::has_share_content_fields(url_content(String(""))),
             "empty fields were treated as shareable") ||
      !check(flight::share::has_share_content_fields(text_content(String("text"))) &&
                 flight::share::has_share_content_fields(title_content(String("title"))) &&
                 flight::share::has_share_content_fields(url_content(String("url"))),
             "a populated variant arm was not recognized")) {
    return 1;
  }

  int can_content_calls = 0;
  int share_content_calls = 0;
  int share_result_calls = 0;
  String last_shared_kind;
  String last_shared_value;
  const auto provider_result = flight::make_ref<ShareResult>(ShareResult{
      .completed = true,
      .activity_type = String("activity"),
      .dismissed = false,
  });
  const auto content_provider = flight::make_ref<HostShareContentCapability>(
      HostShareContentCapability{
          .can_share_content = [&](ShareContent) {
            ++can_content_calls;
            return true;
          },
          .share_content = [&](ShareContent content) {
            ++share_content_calls;
            std::visit(
                [&](const auto& arm) {
                  if constexpr (requires { arm->text.has_value(); }) {
                    if (arm->text.has_value()) {
                      last_shared_kind = String("text");
                      last_shared_value = arm->text.value();
                    }
                  } else {
                    last_shared_kind = String("text");
                    last_shared_value = arm->text;
                  }
                  if constexpr (requires { arm->url.has_value(); }) {
                    if (arm->url.has_value()) {
                      last_shared_kind = String("url");
                      last_shared_value = arm->url.value();
                    }
                  } else {
                    last_shared_kind = String("url");
                    last_shared_value = arm->url;
                  }
                },
                content);
            return flight::Task<bool>::resolve(true);
          },
          .share_content_with_result = [&](ShareContent) {
            ++share_result_calls;
            return flight::Task<flight::Ref<ShareResult>>::resolve(provider_result);
          },
      });

  const auto empty = text_content(String(""));
  const auto populated = text_content(String("payload"));
  if (!check(!flight::share::can_share_content(content_provider, empty) &&
                 can_content_calls == 0,
             "invalid content reached canShareContent provider") ||
      !check(flight::share::can_share_content(content_provider, populated) &&
                 can_content_calls == 1,
             "valid content did not reach canShareContent provider") ||
      !check(!flight::share::share_content(content_provider, empty).get() &&
                 share_content_calls == 0,
             "invalid content did not resolve false locally") ||
      !check(flight::share::share_content(content_provider, populated).get() &&
                 share_content_calls == 1,
             "valid content did not preserve provider settlement")) {
    return 1;
  }

  if (!check(flight::share::share_text(content_provider, String("hello")).get() &&
                 last_shared_kind == String("text") &&
                 last_shared_value == String("hello"),
             "shareText constructed the wrong variant arm") ||
      !check(flight::share::share_url(content_provider, String("https://example.test")).get() &&
                 last_shared_kind == String("url") &&
                 last_shared_value == String("https://example.test"),
             "shareUrl constructed the wrong variant arm")) {
    return 1;
  }

  const auto signals_one = flight::share::enable_share_signals();
  const auto signals_two = flight::share::enable_share_signals();
  const auto signals_three = flight::share::enable_share_signals();
  int first_events = 0;
  int second_events = 0;
  int third_events = 0;
  signals_one->on_share_result->emit = [&](auto result) {
    ++first_events;
    if (result->completed) {
      flight::share::detach_share_signals(signals_two);
      flight::share::attach_share_signals(signals_three);
    }
  };
  signals_two->on_share_result->emit = [&](auto) { ++second_events; };
  signals_three->on_share_result->emit = [&](auto) { ++third_events; };
  flight::share::attach_share_signals(signals_one);
  flight::share::attach_share_signals(signals_one);
  flight::share::attach_share_signals(signals_two);

  const auto invalid_result =
      flight::share::share_content_with_result(content_provider, empty).get();
  if (!check(!invalid_result->completed && !invalid_result->activity_type.has_value() &&
                 !invalid_result->dismissed && share_result_calls == 0 &&
                 first_events == 0 && second_events == 0 && third_events == 0,
             "invalid result share did not resolve locally without publication")) {
    return 1;
  }
  const auto shared_result =
      flight::share::share_content_with_result(content_provider, populated).get();
  if (!check(shared_result == provider_result && share_result_calls == 1,
             "provider ShareResult identity or settlement changed") ||
      !check(first_events == 1 && second_events == 0 && third_events == 1,
             "attached ShareSignals did not use live Set iteration semantics")) {
    return 1;
  }

  const auto valid_one = flight::make_ref<ShareFile>(ShareFile{
      .name = String("one.txt"),
      .mime_type = String("text/plain"),
      .data_url = String("data:text/plain,one"),
  });
  const auto valid_two = flight::make_ref<ShareFile>(ShareFile{
      .name = String("two.txt"),
      .mime_type = String("text/plain"),
      .data_url = String("data:text/plain,two"),
  });
  const auto invalid_file = flight::make_ref<ShareFile>(ShareFile{
      .name = String("bad.txt"),
      .mime_type = String("text/plain"),
      .data_url = String("data:text/plain"),
  });
  if (!check(flight::share::is_share_file_valid(valid_one) &&
                 !flight::share::is_share_file_valid(invalid_file),
             "share file envelope validation changed")) {
    return 1;
  }

  int can_files_calls = 0;
  int share_files_calls = 0;
  bool files_tuple_preserved = false;
  const auto files_provider = flight::make_ref<HostShareFilesCapability>(
      HostShareFilesCapability{
          .can_share_content = [&](auto content) {
            ++can_files_calls;
            const auto files = content->files;
            files_tuple_preserved = std::get<0>(files) == valid_one &&
                                    std::get<1>(files).size() == 1 &&
                                    std::get<1>(files).element(0) == valid_two;
            return true;
          },
          .share_content = [&](auto content) {
            ++share_files_calls;
            const auto files = content->files;
            files_tuple_preserved = files_tuple_preserved &&
                                    std::get<0>(files) == valid_one &&
                                    std::get<1>(files).size() == 1 &&
                                    std::get<1>(files).element(0) == valid_two;
            return flight::Task<bool>::resolve(true);
          },
          .share_content_with_result = [](auto) {
            return flight::Task<flight::Ref<ShareResult>>::resolve(
                flight::make_ref<ShareResult>(ShareResult{}));
          },
      });
  const flight::Array<flight::Ref<ShareFile>> no_files;
  const flight::Array<flight::Ref<ShareFile>> bad_files{valid_one, invalid_file};
  const flight::Array<flight::Ref<ShareFile>> good_files{valid_one, valid_two};
  if (!check(!flight::share::can_share_files(files_provider, no_files) &&
                 !flight::share::can_share_files(files_provider, bad_files) &&
                 can_files_calls == 0,
             "invalid file arrays reached canShareContent provider") ||
      !check(flight::share::can_share_files(files_provider, good_files) &&
                 can_files_calls == 1 && files_tuple_preserved,
             "valid file tuple did not reach canShareContent provider") ||
      !check(!flight::share::share_files(files_provider, bad_files).get() &&
                 share_files_calls == 0,
             "invalid file share did not resolve false locally") ||
      !check(flight::share::share_files(files_provider, good_files).get() &&
                 share_files_calls == 1 && files_tuple_preserved,
             "valid file share changed tuple or provider settlement")) {
    return 1;
  }

  flight::share::dispose_share_signals(signals_one);
  flight::share::dispose_share_signals(signals_two);
  flight::share::dispose_share_signals(signals_three);
  signals_one->on_share_result->emit(provider_result);
  signals_two->on_share_result->emit(provider_result);
  signals_three->on_share_result->emit(provider_result);
  if (!check(first_events == 1 && second_events == 0 && third_events == 1,
             "dispose did not detach and clear ShareSignals")) {
    return 1;
  }

  return 0;
}
