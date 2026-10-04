#include <flight/dialog/file_dialog.hpp>

#include <iostream>
#include <optional>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

}  // namespace

int main() {
  using flight::String;
  using namespace flight::types;

  const auto operations = flight::make_ref<FileDialogHandleOperations>(FileDialogHandleOperations{
      .read_binary = std::nullopt,
      .read_text = [](std::optional<flight::AbortSignal>) {
        return flight::Task<std::optional<String>>::resolve(std::optional<String>{String("retained")});
      },
      .write_binary = std::nullopt,
      .write_text = std::nullopt,
  });
  const auto handle = flight::dialog::create_file_dialog_handle(
      String("File"), String("picked.txt"), std::nullopt,
      std::optional<std::optional<flight::Ref<FileDialogHandleOperations>>>{operations});
  const auto recovered = flight::dialog::get_file_dialog_handle_operations(handle);
  if (!check(handle->kind == String("File") && handle->name == String("picked.txt") &&
                 !handle->path.has_value() && handle->entity_runtime_key.has_value(),
             "file dialog handle fields or entity marker changed") ||
      !check(recovered.has_value() && recovered.value() == operations,
             "file dialog operations did not retain identity") ||
      !check(recovered.value()->read_text.value()(std::nullopt).get() ==
                 std::optional<String>{String("retained")},
             "file dialog operation did not cross the handle boundary")) {
    return 1;
  }

  const auto native = flight::dialog::create_file_dialog_handle(
      String("Directory"), String("tmp"), std::optional<String>{String("/tmp")});
  if (!check(!flight::dialog::get_file_dialog_handle_operations(native).has_value(),
             "native path handle gained runtime authority")) {
    return 1;
  }

  bool saw_directory_options = false;
  const auto cancelled = flight::make_ref<outcome_bd967408cf2a4c5c>(
      outcome_bd967408cf2a4c5c{.outcome = String("cancelled")});
  const auto directory_host = flight::make_ref<HostDirectoryOpenDialogCapability>(
      HostDirectoryOpenDialogCapability{.open = [&](auto options) {
        saw_directory_options = options.has_value() &&
                                flight::row_get<flight::RowKey<"signal">>(options.value()).has_value();
        return flight::Task<DirectoryOpenDialogResult>::resolve(
            DirectoryOpenDialogResult{std::in_place_type<flight::Ref<outcome_bd967408cf2a4c5c>>, cancelled});
      }});
  const auto directory_options = flight::make_ref<OpenDirectoryDialogOptions>(
      OpenDirectoryDialogOptions{.signal = flight::AbortSignal{}});
  const auto directory_result = flight::dialog::show_open_directory_dialog(
                                    directory_host, directory_options)
                                    .get();
  if (!check(saw_directory_options, "directory options were not forwarded") ||
      !check(std::get<flight::Ref<outcome_bd967408cf2a4c5c>>(directory_result)->outcome ==
                 String("cancelled"),
             "directory outcome changed")) {
    return 1;
  }

  return 0;
}
