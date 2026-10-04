// Derived from @flighthq/dialog/packages/dialog/src/fileDialog.ts.
#pragma once

#include <optional>

#include <flight/entity/entity.hpp>
#include <flight/runtime.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/dialog.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/host_file_dialog.hpp>
#include <flight/weak_map.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1,
              "Flight C++ runtime ABI mismatch");

namespace flight::dialog {

using flight::types::DirectoryOpenDialogResult;
using flight::types::FileDialogHandle;
using flight::types::FileDialogHandleOperations;
using flight::types::FileOpenDialogResult;
using flight::types::FileSaveDialogResult;
using flight::types::HostDirectoryOpenDialogCapability;
using flight::types::HostFileOpenDialogCapability;
using flight::types::HostFileSaveDialogCapability;
using flight::types::OpenDirectoryDialogOptions;
using flight::types::OpenFileDialogOptions;
using flight::types::SaveFileDialogOptions;

inline flight::WeakMap<flight::Ref<FileDialogHandle>, flight::Ref<FileDialogHandleOperations>>
    file_dialog_handle_operations;

inline std::optional<flight::Ref<FileDialogHandleOperations>> get_file_dialog_handle_operations(
    flight::Ref<FileDialogHandle> handle) {
  return file_dialog_handle_operations.get(handle);
}

inline void initialize_file_dialog_handle(
    flight::types::EntityConstruction<flight::Ref<FileDialogHandle>> handle,
    flight::String kind,
    flight::String name,
    std::optional<flight::String> path,
    std::optional<flight::Ref<FileDialogHandleOperations>> operations = std::nullopt) {
  flight::row_set<flight::RowKey<"kind">>(handle, std::move(kind));
  flight::row_set<flight::RowKey<"name">>(handle, std::move(name));
  flight::row_set<flight::RowKey<"path">>(handle, std::move(path));
  const auto runtime = flight::make_ref<flight::types::EntityRuntime>(
      flight::types::EntityRuntime{.binding = std::nullopt, .uid = std::nullopt});
  flight::row_set(handle, flight::types::entity_runtime_key,
                  std::optional<flight::Ref<flight::types::EntityRuntime>>{runtime});
  const auto owner = flight::entity::finish_entity<flight::Ref<FileDialogHandle>>(handle);
  static_cast<void>(file_dialog_handle_operations.erase(owner));
  if (operations.has_value()) file_dialog_handle_operations.set(owner, operations.value());
}

inline flight::Ref<FileDialogHandle> create_file_dialog_handle(
    flight::String kind,
    flight::String name,
    std::optional<flight::String> path,
    std::optional<std::optional<flight::Ref<FileDialogHandleOperations>>> operations = std::nullopt) {
  const auto handle = flight::entity::allocate_entity<flight::Ref<FileDialogHandle>>();
  initialize_file_dialog_handle(handle, std::move(kind), std::move(name), std::move(path),
                                operations.value_or(std::nullopt));
  return flight::entity::finish_entity<flight::Ref<FileDialogHandle>>(handle);
}

inline flight::Task<flight::Ref<DirectoryOpenDialogResult>> show_open_directory_dialog(
    flight::Ref<HostDirectoryOpenDialogCapability> host_directory_open_dialog,
    std::optional<flight::Ref<OpenDirectoryDialogOptions>> options = std::nullopt) {
  using Options = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<OpenDirectoryDialogOptions>>>>;
  if (!options.has_value()) return host_directory_open_dialog->open(std::nullopt);
  const auto writable = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<OpenDirectoryDialogOptions>>>>(options.value());
  return host_directory_open_dialog->open(std::optional<Options>{flight::structural_ref_cast<Options>(writable)});
}

inline flight::Task<flight::Ref<FileOpenDialogResult>> show_open_file_dialog(
    flight::Ref<HostFileOpenDialogCapability> host_file_open_dialog,
    flight::Ref<OpenFileDialogOptions> options) {
  using Options = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<OpenFileDialogOptions>>>>;
  const auto writable = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<OpenFileDialogOptions>>>>(options);
  return host_file_open_dialog->open(flight::structural_ref_cast<Options>(writable));
}

inline flight::Task<flight::Ref<FileSaveDialogResult>> show_save_file_dialog(
    flight::Ref<HostFileSaveDialogCapability> host_file_save_dialog,
    flight::Ref<SaveFileDialogOptions> options) {
  using Options = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<SaveFileDialogOptions>>>>;
  const auto writable = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<SaveFileDialogOptions>>>>(options);
  return host_file_save_dialog->save(flight::structural_ref_cast<Options>(writable));
}

}  // namespace flight::dialog
