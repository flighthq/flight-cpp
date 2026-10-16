#include <flight/app/app_loop.hpp>
#include <flight/app/app_render_view.hpp>
#include <flight/dialog/file_dialog.hpp>
#include <flight/filesystem/filesystem.hpp>
#include <flight/geometry/matrix.hpp>
#include <flight/node/viewport.hpp>
#include <flight/render/render_state.hpp>
#include <flight/signals/emitter.hpp>
#include <flight/signals/slot.hpp>

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

  int path_reads = 0;
  int path_writes = 0;
  int handle_reads = 0;
  int handle_writes = 0;
  const auto retained_bytes = flight::Uint8Array(flight::Array{7.0, 8.0, 9.0});
  const auto operations = flight::make_ref<FileDialogHandleOperations>(FileDialogHandleOperations{
      .read_binary = [&](std::optional<flight::AbortSignal>) {
        ++handle_reads;
        return flight::Task<std::optional<flight::Uint8Array>>::resolve(retained_bytes);
      },
      .read_text = [&](std::optional<flight::AbortSignal>) {
        ++handle_reads;
        return flight::Task<std::optional<String>>::resolve(std::optional<String>{String("retained")});
      },
      .write_binary = [&](flight::Uint8Array data, std::optional<flight::AbortSignal>) {
        ++handle_writes;
        return flight::Task<bool>::resolve(data == retained_bytes);
      },
      .write_text = [&](String data, std::optional<flight::AbortSignal>) {
        ++handle_writes;
        return flight::Task<bool>::resolve(data == String("retained"));
      },
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

  const auto host_file_system = flight::make_ref<HostFileSystemCapability>(
      HostFileSystemCapability{
          .read_binary_file = [&](String path, std::optional<flight::AbortSignal>) {
            ++path_reads;
            return flight::Task<std::optional<flight::Uint8Array>>::resolve(
                path == String("/tmp") ? std::optional<flight::Uint8Array>{retained_bytes}
                                       : std::nullopt);
          },
          .read_text_file = [&](String path, std::optional<flight::AbortSignal>) {
            ++path_reads;
            return flight::Task<std::optional<String>>::resolve(
                path == String("/tmp") ? std::optional<String>{String("native")} : std::nullopt);
          },
          .write_binary_file = [&](String path, flight::Uint8Array data,
                                   std::optional<flight::AbortSignal>) {
            ++path_writes;
            return flight::Task<bool>::resolve(path == String("/tmp") && data == retained_bytes);
          },
          .write_text_file = [&](String path, String data, std::optional<flight::AbortSignal>) {
            ++path_writes;
            return flight::Task<bool>::resolve(path == String("/tmp") && data == String("native"));
          },
      });
  const auto host = flight::filesystem::detail::HostFileSystem(host_file_system);
  if (!check(flight::filesystem::read_dialog_handle_binary_file(host, handle).get() ==
                 std::optional<flight::Uint8Array>{retained_bytes} &&
                 flight::filesystem::read_dialog_handle_text_file(host, handle).get() ==
                     std::optional<String>{String("retained")} &&
                 flight::filesystem::write_dialog_handle_binary_file(
                     host, handle, retained_bytes).get() &&
                 flight::filesystem::write_dialog_handle_text_file(
                     host, handle, String("retained")).get() &&
                 handle_reads == 3 && handle_writes == 2 && path_reads == 0 && path_writes == 0,
             "pathless dialog I/O did not retain provider operations") ||
      !check(flight::filesystem::read_dialog_handle_binary_file(host, native).get() ==
                 std::optional<flight::Uint8Array>{retained_bytes} &&
                 flight::filesystem::read_dialog_handle_text_file(host, native).get() ==
                     std::optional<String>{String("native")} &&
                 flight::filesystem::write_dialog_handle_binary_file(
                     host, native, retained_bytes).get() &&
                 flight::filesystem::write_dialog_handle_text_file(
                     host, native, String("native")).get() &&
                 path_reads == 2 && path_writes == 2 && handle_reads == 3 && handle_writes == 2,
             "path-backed dialog I/O did not delegate to the native filesystem")) {
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

  const auto window = flight::app::create_app_window();
  window->width = 10.4;
  window->height = 4.6;
  window->device_pixel_ratio = 2.0;
  const auto render_state = flight::render::create_render_state();
  render_state->render_transform_2d = flight::geometry::create_matrix();
  const auto render_target = flight::make_ref<RenderTargetDimensions>(
      RenderTargetDimensions{.height = 0.0, .width = 0.0});
  const auto viewport = flight::node::create_viewport();
  int resize_calls = 0;
  const auto view = flight::app::create_app_render_view(
      window, render_state, render_target, viewport,
      [&](flight::Ref<RenderState>, flight::Ref<RenderTargetDimensions> target,
          double width, double height) {
        ++resize_calls;
        target->width = width;
        target->height = height;
      });
  if (!check(resize_calls == 1 && render_target->width == 21.0 &&
                 render_target->height == 9.0 && viewport->width == 21.0 &&
                 viewport->height == 9.0 && viewport->device_pixel_ratio == 2.0 &&
                 render_state->pixel_ratio == 2.0 &&
                 render_state->render_transform_2d.value()->a == 2.0 &&
                 render_state->render_transform_2d.value()->d == 2.0,
             "AppRenderView creation did not perform the upstream synchronization")) {
    return 1;
  }
  flight::app::attach_app_render_view(view);
  window->width = 8.0;
  window->height = 3.0;
  window->device_pixel_ratio = 1.5;
  flight::signals::emit_signal<std::function<void()>>(window->on_resize);
  if (!check(resize_calls == 3 && render_target->width == 12.0 &&
                 render_target->height == 5.0 && viewport->width == 12.0 &&
                 viewport->height == 5.0 && render_state->pixel_ratio == 1.5,
             "attached AppRenderView did not follow its window")) {
    return 1;
  }
  flight::app::attach_app_render_view(view);
  flight::signals::emit_signal<std::function<void()>>(window->on_resize);
  if (!check(resize_calls == 5,
             "reattaching AppRenderView installed duplicate resize listeners")) {
    return 1;
  }
  flight::app::detach_app_render_view(view);
  flight::signals::emit_signal<std::function<void()>>(window->on_resize);
  if (!check(resize_calls == 5,
             "detached AppRenderView still received resize notifications")) {
    return 1;
  }

  const auto app_loop = flight::app::create_app_loop();
  flight::app::enable_app_loop_lifecycle_signals(app_loop);
  int fixed_updates = 0;
  int updates = 0;
  int renders = 0;
  flight::signals::connect_signal(
      app_loop->on_fixed_update.value(),
      std::function<void(double)>{[&](double) { ++fixed_updates; }}, std::nullopt);
  flight::signals::connect_signal(
      app_loop->on_update,
      std::function<void(double)>{[&](double) { ++updates; }}, std::nullopt);
  flight::signals::connect_signal(
      app_loop->on_render, std::function<void()>{[&]() { ++renders; }},
      std::nullopt);
  const auto step_options = flight::make_ref<AppStepOptions>(AppStepOptions{
      .max_delta_time = 25.0,
      .fixed_time_step = 10.0,
      .max_updates_per_frame = 3.0,
  });
  flight::app::step_app_loop(app_loop, 40.0, step_options);
  flight::app::step_app_loop(app_loop, 100.0, step_options);
  if (!check(app_loop->delta_time == 25.0 && app_loop->elapsed_time == 0.05 &&
                 app_loop->frame_count == 2.0 &&
                 app_loop->interpolation_alpha == 0.0 && fixed_updates == 5 &&
                 updates == 2 && renders == 2 &&
                 flight::app::get_app_loop_frame_rate(app_loop) == 40.0,
             "AppLoop fixed-step policy diverged from the upstream implementation")) {
    return 1;
  }

  int forwarded_errors = 0;
  bool retained_exception = false;
  flight::signals::connect_signal(
      app_loop->on_error.value(),
      std::function<void(flight::Any)>{[&](const flight::Any error) {
        ++forwarded_errors;
        retained_exception = error.external_if<std::exception_ptr>() != nullptr;
      }},
      std::nullopt);
  flight::app::invoke_with_app_loop_error_handling(
      app_loop, []() { throw std::runtime_error("app-loop probe"); });
  if (!check(forwarded_errors == 1 && retained_exception,
             "AppLoop did not forward a native callback failure as an opaque error")) {
    return 1;
  }

  int activations = 0;
  int deactivations = 0;
  flight::signals::connect_signal(
      app_loop->on_activate.value(),
      std::function<void()>{[&]() { ++activations; }}, std::nullopt);
  flight::signals::connect_signal(
      app_loop->on_deactivate.value(),
      std::function<void()>{[&]() { ++deactivations; }}, std::nullopt);
  app_loop->is_running = true;
  flight::app::attach_app_loop_lifecycle(app_loop, window);
  flight::signals::emit_signal(window->on_deactivate);
  flight::signals::emit_signal(window->on_activate);
  flight::app::attach_app_loop_lifecycle(app_loop, window);
  flight::signals::emit_signal(window->on_deactivate);
  if (!check(!app_loop->is_running && activations == 1 && deactivations == 2,
             "AppLoop lifecycle attachment was not idempotent")) {
    return 1;
  }

  flight::app::register_app_loop_window(app_loop, window);
  flight::app::register_app_loop_window(app_loop, window);
  const auto main_window = flight::app::get_app_loop_main_window(app_loop);
  if (!check(app_loop->windows.size() == 1 && main_window.has_value() &&
                 main_window.value() == window,
             "AppLoop window registration or default main-window selection diverged")) {
    return 1;
  }

  const auto driven_loop = flight::app::create_app_loop();
  int requested_frames = 0;
  int cancelled_frames = 0;
  std::function<void(double)> scheduled_frame;
  const auto host_loop = flight::make_ref<HostAppLoopCapability>(
      HostAppLoopCapability{
          .request_frame = [&](std::function<void(double)> callback) {
            ++requested_frames;
            scheduled_frame = std::move(callback);
            return flight::Any(requested_frames);
          },
          .cancel_frame = [&](flight::Any) { ++cancelled_frames; },
          .now = []() { return 0.0; },
      });
  const auto lifecycle = flight::make_ref<HostLifecycleCapability>(
      HostLifecycleCapability{
          .get_state = []() { return String("foreground"); },
          .subscribe = [](std::function<void()>) {
            return std::function<void()>{[]() {}};
          },
      });
  flight::app::start_app_loop(host_loop, lifecycle, driven_loop);
  auto first_frame = scheduled_frame;
  first_frame(100.0);
  auto second_frame = scheduled_frame;
  second_frame(116.0);
  flight::app::stop_app_loop(driven_loop);
  if (!check(requested_frames == 3 && cancelled_frames == 1 &&
                 driven_loop->frame_count == 2.0 &&
                 driven_loop->delta_time == 16.0 && !driven_loop->is_running,
             "host-driven AppLoop scheduling diverged from the upstream loop")) {
    return 1;
  }

  return 0;
}
