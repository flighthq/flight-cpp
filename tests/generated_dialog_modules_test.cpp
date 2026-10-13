// Focused behavior tests for the @flighthq/dialog modules, independent of the application and render
// packages.
//
// `@flighthq/dialog` is `partial` at a627849: four of five modules emit and `fileDialog` is supplied by
// the hand-written override in `overrides/include/flight/dialog/file_dialog.hpp`, which this file
// exercises along with the two modules that emit whole (`dialog`, `mediaDialog`).
//
// What is checked, against the pinned TypeScript:
//
//   * a file-dialog handle's operations are authority tied to that handle's identity: present only for
//     the handle they were created with, absent for a handle created without them, absent when the
//     caller passes an explicit null, and never recovered from a second handle carrying the same
//     descriptor;
//   * every `show*Dialog` function forwards the caller's options row to the provider — the directory,
//     image, photo and video entry points pass `nullopt` when there are no options, and each one hands
//     back the provider's own result object;
//   * the message family stamps its kind. `showErrorBox` builds the options row from its own arguments
//     with `kind: 'error'`, and the spread-based entry points re-emit the caller's row with the kind
//     replaced (`error`, `info`, `warning`) and every other field carried through.
//
// The `generated_dialog_test.cpp` in this directory covers the same package's application- and
// render-side integration, and does NOT build in this tree for a reason outside the package: it
// reaches the render lane, where `generated/include/flight/render/render_state.hpp` includes
// `flight/types/renderer.hpp` and that header is absent. (This header previously pointed at "the note
// at the end of this file"; no such note existed, so the pointer was replaced with the measured cause.)
// The dialog-package semantics are therefore covered here instead.
#include <flight/dialog/dialog.hpp>
#include <flight/dialog/file_dialog.hpp>
#include <flight/dialog/media_dialog.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <variant>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

using flight::String;
using Handle = flight::types::FileDialogHandle;
using Operations = flight::types::FileDialogHandleOperations;
// Every dialog result union carries this cancelled arm as its second alternative.
using Cancelled = flight::types::outcome_bd967408cf2a4c5c;

using DirectoryOptions = flight::types::OpenDirectoryDialogOptions;
using FileOpenOptions = flight::types::OpenFileDialogOptions;
using FileSaveOptions = flight::types::SaveFileDialogOptions;
using ImageOptions = flight::types::OpenImageDialogOptions;
using PhotoOptions = flight::types::CapturePhotoDialogOptions;
using VideoOptions = flight::types::CaptureVideoDialogOptions;
using MessageOptions = flight::types::MessageDialogOptions;

// `flight::Ref<T>` is an alias that hides a dependent type, so it cannot be deduced from; the
// parameter is spelled as the shared pointer it resolves to for a reference-enabled type.
template <typename Options>
flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Options>>>> readonly_row(
    const std::shared_ptr<Options>& options) {
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Options>>>>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Options>>>>(options));
}

template <typename Row>
bool carries_signal(const Row& row) {
  return flight::row_get<flight::RowKey<"signal">>(row).has_value();
}

flight::Ref<Cancelled> cancelled(const char* outcome) {
  auto value = flight::make_ref<Cancelled>();
  value->outcome = String(outcome);
  return value;
}

// The handle's operations parameter is a dual sentinel -- `undefined` and `null` are distinct -- which
// the emitter carries as an optional of an optional.
using OperationsArgument = std::optional<std::optional<flight::Ref<Operations>>>;

}  // namespace

int main() {
  // A handle's operations are its own, and only its own.
  {
    auto operations = flight::make_ref<Operations>();
    const auto handle = flight::dialog::create_file_dialog_handle(
        String("open-file"), String("Open"), std::optional<flight::String>{String("/tmp/a.txt")},
        OperationsArgument{std::optional<flight::Ref<Operations>>{operations}});

    check(handle->kind == String("open-file"), "the handle keeps the kind it was created with");
    check(handle->name == String("Open"), "and its name");
    check(handle->path.has_value() && handle->path.value() == String("/tmp/a.txt"), "and its path");

    const auto recovered = flight::dialog::get_file_dialog_handle_operations(handle);
    check(recovered.has_value(), "a handle created with operations has them");
    check(recovered.has_value() && recovered.value() == operations,
          "and they are the very object the caller supplied, not a copy");

    // A second handle with the same descriptor carries no authority of its own: the source keeps
    // operations beside the handle's identity so a copied descriptor cannot recover them.
    const auto copied = flight::dialog::create_file_dialog_handle(
        String("open-file"), String("Open"), std::optional<flight::String>{String("/tmp/a.txt")});
    check(!flight::dialog::get_file_dialog_handle_operations(copied).has_value(),
          "a handle created without operations has none, however identical its descriptor");
    check(flight::dialog::get_file_dialog_handle_operations(handle).has_value(),
          "and the original still has its own");

    // An explicit null is a distinct input from an absent one, and neither grants authority.
    const auto explicit_null = flight::dialog::create_file_dialog_handle(
        String("open-file"), String("Open"), std::nullopt,
        OperationsArgument{std::optional<flight::Ref<Operations>>{}});
    check(!flight::dialog::get_file_dialog_handle_operations(explicit_null).has_value(),
          "an explicit null operations argument grants no authority");

    // Two handles with operations keep them apart.
    auto other_operations = flight::make_ref<Operations>();
    const auto second = flight::dialog::create_file_dialog_handle(
        String("save-file"), String("Save"), std::nullopt,
        OperationsArgument{std::optional<flight::Ref<Operations>>{other_operations}});
    const auto second_recovered = flight::dialog::get_file_dialog_handle_operations(second);
    check(second_recovered.has_value() && second_recovered.value() == other_operations,
          "a second handle recovers its own operations");
    check(second_recovered.has_value() && second_recovered.value() != operations,
          "which are not the first handle's");
  }

  // Directory open: no options are forwarded as absent, and the provider's own result comes back.
  {
    std::optional<bool> saw_options;
    const auto provider_result = cancelled("cancelled");
    auto host = flight::make_ref<flight::types::HostDirectoryOpenDialogCapability>();
    host->open = [&saw_options, provider_result](
                     std::optional<flight::StructuralRef<
                         flight::RowReadonly<flight::RowOf<flight::Ref<DirectoryOptions>>>>> options) {
      saw_options = options.has_value();
      return flight::Task<flight::types::DirectoryOpenDialogResult>::resolve(
          flight::types::DirectoryOpenDialogResult{
              std::in_place_type<flight::Ref<Cancelled>>, provider_result});
    };

    const auto without = flight::dialog::show_open_directory_dialog(host).get();
    check(saw_options.has_value() && !saw_options.value(),
          "a directory dialog with no options forwards absence, not an empty row");
    check(std::get<flight::Ref<Cancelled>>(without) == provider_result,
          "and the caller receives the provider's own result object");

    auto options = flight::make_ref<DirectoryOptions>();
    options->signal = flight::AbortSignal{};
    const auto with = flight::dialog::show_open_directory_dialog(host, options).get();
    check(saw_options.has_value() && saw_options.value(),
          "a directory dialog with options forwards them");
    check(std::get<flight::Ref<Cancelled>>(with) == provider_result, "with the provider's result again");
  }

  // File open and file save forward the caller's row.
  {
    bool saw_open_options = false;
    const auto open_result = cancelled("cancelled");
    auto open_host = flight::make_ref<flight::types::HostFileOpenDialogCapability>();
    open_host->open = [&saw_open_options, open_result](
                          flight::StructuralRef<
                              flight::RowReadonly<flight::RowOf<flight::Ref<FileOpenOptions>>>> options) {
      saw_open_options = carries_signal(options);
      return flight::Task<flight::types::FileOpenDialogResult>::resolve(
          flight::types::FileOpenDialogResult{std::in_place_type<flight::Ref<Cancelled>>, open_result});
    };
    auto open_options = flight::make_ref<FileOpenOptions>();
    open_options->signal = flight::AbortSignal{};
    const auto opened = flight::dialog::show_open_file_dialog(open_host, readonly_row(open_options)).get();
    check(saw_open_options, "the open dialog's options reach the provider, signal and all");
    check(std::get<flight::Ref<Cancelled>>(opened) == open_result, "and its result is the provider's");

    bool saw_save_options = false;
    const auto save_result = cancelled("cancelled");
    auto save_host = flight::make_ref<flight::types::HostFileSaveDialogCapability>();
    save_host->save = [&saw_save_options, save_result](
                          flight::StructuralRef<
                              flight::RowReadonly<flight::RowOf<flight::Ref<FileSaveOptions>>>> options) {
      saw_save_options = carries_signal(options);
      return flight::Task<flight::types::FileSaveDialogResult>::resolve(
          flight::types::FileSaveDialogResult{std::in_place_type<flight::Ref<Cancelled>>, save_result});
    };
    auto save_options = flight::make_ref<FileSaveOptions>();
    save_options->signal = flight::AbortSignal{};
    const auto saved = flight::dialog::show_save_file_dialog(save_host, readonly_row(save_options)).get();
    check(saw_save_options, "the save dialog's options reach the provider");
    check(std::get<flight::Ref<Cancelled>>(saved) == save_result, "and its result is the provider's");
  }

  // The media entry points: absence and presence on each of the three.
  {
    std::optional<bool> image_options;
    std::optional<bool> photo_options;
    std::optional<bool> video_options;
    const auto image_result = cancelled("cancelled");
    const auto photo_result = cancelled("cancelled");
    const auto video_result = cancelled("cancelled");

    auto image_host = flight::make_ref<flight::types::HostImageOpenDialogCapability>();
    image_host->open = [&image_options, image_result](
                           std::optional<flight::StructuralRef<
                               flight::RowReadonly<flight::RowOf<flight::Ref<ImageOptions>>>>> options) {
      image_options = options.has_value();
      return flight::Task<flight::types::ImageOpenDialogResult>::resolve(
          flight::types::ImageOpenDialogResult{std::in_place_type<flight::Ref<Cancelled>>, image_result});
    };
    auto photo_host = flight::make_ref<flight::types::HostPhotoCaptureDialogCapability>();
    photo_host->capture = [&photo_options, photo_result](
                              std::optional<flight::StructuralRef<
                                  flight::RowReadonly<flight::RowOf<flight::Ref<PhotoOptions>>>>> options) {
      photo_options = options.has_value();
      return flight::Task<flight::types::PhotoCaptureDialogResult>::resolve(
          flight::types::PhotoCaptureDialogResult{std::in_place_type<flight::Ref<Cancelled>>, photo_result});
    };
    auto video_host = flight::make_ref<flight::types::HostVideoCaptureDialogCapability>();
    video_host->capture = [&video_options, video_result](
                              std::optional<flight::StructuralRef<
                                  flight::RowReadonly<flight::RowOf<flight::Ref<VideoOptions>>>>> options) {
      video_options = options.has_value();
      return flight::Task<flight::types::VideoCaptureDialogResult>::resolve(
          flight::types::VideoCaptureDialogResult{std::in_place_type<flight::Ref<Cancelled>>, video_result});
    };

    check(std::get<flight::Ref<Cancelled>>(flight::dialog::show_open_image_dialog(image_host).get()) ==
              image_result,
          "the image dialog answers with the provider's result when no options are given");
    check(image_options.has_value() && !image_options.value(), "with absence forwarded");

    auto photo_options_value = flight::make_ref<PhotoOptions>();
    photo_options_value->signal = flight::AbortSignal{};
    check(std::get<flight::Ref<Cancelled>>(
              flight::dialog::show_capture_photo_dialog(photo_host, photo_options_value).get()) ==
              photo_result,
          "the photo capture dialog answers with the provider's result");
    check(photo_options.has_value() && photo_options.value(), "with the caller's options forwarded");

    check(std::get<flight::Ref<Cancelled>>(flight::dialog::show_capture_video_dialog(video_host).get()) ==
              video_result,
          "the video capture dialog answers with the provider's result");
    check(video_options.has_value() && !video_options.value(), "with absence forwarded");

    auto image_options_value = flight::make_ref<ImageOptions>();
    image_options_value->signal = flight::AbortSignal{};
    static_cast<void>(flight::dialog::show_open_image_dialog(image_host, image_options_value).get());
    check(image_options.has_value() && image_options.value(), "and presence, when options are given");
  }

  // The message family: the kind is the module's to stamp, and everything else passes through.
  {
    std::optional<String> kind;
    std::optional<bool> signal;
    std::optional<String> title;
    std::optional<String> message;
    const auto box_result = flight::make_ref<flight::types::MessageDialogResult>();
    auto host = flight::make_ref<flight::types::HostMessageDialogCapability>();
    host->message = [&](flight::StructuralRef<
                        flight::RowReadonly<flight::RowOf<flight::Ref<MessageOptions>>>> options) {
      kind = flight::row_get<flight::RowKey<"kind">>(options);
      signal = carries_signal(options);
      title = flight::row_get<flight::RowKey<"title">>(options);
      message = flight::row_get<flight::RowKey<"message">>(options);
      return flight::Task<flight::Ref<flight::types::MessageDialogResult>>::resolve(box_result);
    };

    // showErrorBox builds its own row: title, message, and the kind the function is for.
    const auto boxed = flight::dialog::show_error_box(host, String("Broken"), String("It failed")).get();
    check(boxed == box_result, "showErrorBox returns the provider's result object");
    check(kind.has_value() && kind.value() == String("error"), "showErrorBox stamps kind 'error'");
    check(title.has_value() && title.value() == String("Broken"), "and forwards the title");
    check(message.has_value() && message.value() == String("It failed"), "and the message");
    check(signal.has_value() && !signal.value(), "and omits the signal when none was given");

    // With a signal supplied, the row carries it.
    const auto with_signal = flight::dialog::show_error_box(
                                 host, String("Broken"), String("It failed"),
                                 std::optional<std::optional<flight::AbortSignal>>{
                                     std::optional<flight::AbortSignal>{flight::AbortSignal{}}})
                                 .get();
    check(with_signal == box_result, "showErrorBox with a signal still returns the provider's result");
    check(signal.has_value() && signal.value(), "and stamps the signal onto the row");

    // The spread-based entry points keep the caller's fields and replace only the kind.
    auto options = flight::make_ref<MessageOptions>();
    options->message = String("Body");
    options->title = String("Heading");
    options->signal = flight::AbortSignal{};

    static_cast<void>(flight::dialog::show_info_dialog(host, readonly_row(options)).get());
    check(kind.has_value() && kind.value() == String("info"), "showInfoDialog stamps kind 'info'");
    check(title.has_value() && title.value() == String("Heading"), "and carries the caller's title");
    check(signal.has_value() && signal.value(), "and the caller's signal");

    static_cast<void>(flight::dialog::show_warning_dialog(host, readonly_row(options)).get());
    check(kind.has_value() && kind.value() == String("warning"), "showWarningDialog stamps kind 'warning'");

    static_cast<void>(flight::dialog::show_error_dialog(host, readonly_row(options)).get());
    check(kind.has_value() && kind.value() == String("error"), "showErrorDialog stamps kind 'error'");
    check(message.has_value() && message.value() == String("Body"), "and carries the caller's message");

    // showMessageDialog is the unstamped entry point: the caller's row goes through untouched.
    auto plain = flight::make_ref<MessageOptions>();
    plain->message = String("Plain");
    plain->kind = String("plain");
    static_cast<void>(flight::dialog::show_message_dialog(host, readonly_row(plain)).get());
    check(kind.has_value() && kind.value() == String("plain"),
          "showMessageDialog forwards the caller's own kind rather than stamping one");

    bool confirmed = false;
    host->confirm = [&](flight::StructuralRef<
                        flight::RowReadonly<flight::RowOf<flight::Ref<MessageOptions>>>> options) {
      confirmed = flight::row_get<flight::RowKey<"message">>(options) == String("Plain");
      return flight::Task<bool>::resolve(true);
    };
    check(flight::dialog::show_confirm_dialog(host, readonly_row(plain)).get(),
          "the confirm dialog returns the provider's answer");
    check(confirmed, "and the caller's row reached it");
  }

  // The prompt entry point forwards its row and returns the provider's value.
  {
    std::optional<String> message;
    auto host = flight::make_ref<flight::types::HostPromptDialogCapability>();
    host->prompt = [&message](flight::StructuralRef<flight::RowReadonly<flight::RowOf<
                                  flight::Ref<flight::types::PromptDialogOptions>>>> options) {
      message = flight::row_get<flight::RowKey<"message">>(options);
      return flight::Task<std::optional<String>>::resolve(std::optional<String>{String("typed")});
    };
    auto options = flight::make_ref<flight::types::PromptDialogOptions>();
    options->message = String("Your name?");
    const auto answer = flight::dialog::show_prompt_dialog(host, readonly_row(options)).get();
    check(message.has_value() && message.value() == String("Your name?"),
          "the prompt's options reach the provider");
    check(answer.has_value() && answer.value() == String("typed"),
          "and the provider's answer is what comes back");
  }

  // EACH OPERATION'S OWN OUTCOME DOMAIN. The blocks above always use the outcome string
  // "cancelled", which proves the provider's result round-trips but not that the operation keeps ITS
  // domain: every result union here shares the same cancelled arm, so a single value cannot distinguish
  // them. The upstream fixtures give each entry point a failure outcome of its own, and these assert
  // exactly those strings -- a module that collapsed or remapped them would pass the blocks above and
  // fail here.
  {
    using Selected = flight::types::handle_outcome_194a6af8011a3c94;

    // Every failure outcome the three fixtures name, one per operation that has one.
    struct Case {
      const char* outcome;
      const char* message;
    };

    {
      // directory open -- the directory-specific runtime-unavailable outcome.
      auto host = flight::make_ref<flight::types::HostDirectoryOpenDialogCapability>();
      host->open = [](auto) {
        return flight::Task<flight::types::DirectoryOpenDialogResult>::resolve(
            flight::types::DirectoryOpenDialogResult{
                std::in_place_type<flight::Ref<Cancelled>>, cancelled("runtime-unavailable")});
      };
      check(std::get<flight::Ref<Cancelled>>(flight::dialog::show_open_directory_dialog(host).get())->outcome ==
                String("runtime-unavailable"),
            "the directory dialog keeps its runtime-unavailable outcome");
    }

    {
      // file open -- the file-open-specific security outcome.
      auto host = flight::make_ref<flight::types::HostFileOpenDialogCapability>();
      host->open = [](auto) {
        return flight::Task<flight::types::FileOpenDialogResult>::resolve(
            flight::types::FileOpenDialogResult{std::in_place_type<flight::Ref<Cancelled>>,
                                                cancelled("security-denied")});
      };
      check(std::get<flight::Ref<Cancelled>>(flight::dialog::show_open_file_dialog(host, {}).get())->outcome ==
                String("security-denied"),
            "the file-open dialog keeps its security-denied outcome");
    }

    {
      // file save -- the file-save-specific failure outcome, and the SELECTED arm carrying the handle.
      auto host = flight::make_ref<flight::types::HostFileSaveDialogCapability>();
      host->save = [](auto) {
        return flight::Task<flight::types::FileSaveDialogResult>::resolve(
            flight::types::FileSaveDialogResult{std::in_place_type<flight::Ref<Cancelled>>,
                                                cancelled("file-save-failed")});
      };
      check(std::get<flight::Ref<Cancelled>>(flight::dialog::show_save_file_dialog(host, {}).get())->outcome ==
                String("file-save-failed"),
            "the file-save dialog keeps its file-save-failed outcome");

      // The success arm is a DIFFERENT alternative, not a differently-spelled cancellation: it carries
      // the handle the caller asked for. Building it here pins that the two arms stay distinguishable.
      const auto handle = flight::dialog::create_file_dialog_handle(
          String("save-file"), String("out.txt"), std::optional<flight::String>{String("/tmp/out.txt")});
      const auto selected = flight::make_ref<Selected>(Selected{.handle = handle, .outcome = String("selected")});
      auto selected_host = flight::make_ref<flight::types::HostFileSaveDialogCapability>();
      selected_host->save = [selected](auto) {
        return flight::Task<flight::types::FileSaveDialogResult>::resolve(
            flight::types::FileSaveDialogResult{std::in_place_type<flight::Ref<Selected>>, selected});
      };
      const auto selected_result = flight::dialog::show_save_file_dialog(selected_host, {}).get();
      check(selected_result.index() == 0 &&
                std::get<flight::Ref<Selected>>(selected_result)->outcome == String("selected"),
            "a selected save is the handle arm, not a cancellation");
      check(std::get<flight::Ref<Selected>>(selected_result)->handle->name == String("out.txt"),
            "and the selected arm carries the handle the provider chose");
    }

    {
      // image open -- the image-open-specific failure outcome.
      auto host = flight::make_ref<flight::types::HostImageOpenDialogCapability>();
      host->open = [](auto) {
        return flight::Task<flight::types::ImageOpenDialogResult>::resolve(
            flight::types::ImageOpenDialogResult{std::in_place_type<flight::Ref<Cancelled>>,
                                                 cancelled("image-open-failed")});
      };
      check(std::get<flight::Ref<Cancelled>>(flight::dialog::show_open_image_dialog(host).get())->outcome ==
                String("image-open-failed"),
            "the image dialog keeps its image-open-failed outcome");
    }

    {
      // photo capture -- the photo-capture-specific failure outcome.
      auto host = flight::make_ref<flight::types::HostPhotoCaptureDialogCapability>();
      host->capture = [](auto) {
        return flight::Task<flight::types::PhotoCaptureDialogResult>::resolve(
            flight::types::PhotoCaptureDialogResult{std::in_place_type<flight::Ref<Cancelled>>,
                                                    cancelled("photo-capture-failed")});
      };
      check(std::get<flight::Ref<Cancelled>>(flight::dialog::show_capture_photo_dialog(host).get())->outcome ==
                String("photo-capture-failed"),
            "the photo capture dialog keeps its photo-capture-failed outcome");
    }
  }

  if (failures == 0) std::cout << "generated dialog modules: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
