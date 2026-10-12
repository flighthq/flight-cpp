// Focused behavior tests for @flighthq/filesystem.
//
// The package is `partial` at a627849 in the sense that its one implementation module is REFUSED whole:
// `cpp emission failed for … filesystem.ts: indexedAccess types require C++ type computation lowering:
// object is AbortSignalEventMap, index is unknown` — a signature-level refusal that withholds all
// forty-seven declarations, so the emitted header is a placeholder and the module is supplied by
// overrides/include/flight/filesystem/filesystem.hpp. This test exercises that supply, and every case is
// ported from the module's own upstream suite (packages/filesystem/src/filesystem.test.ts):
//
//   * path normalization, including the two asymmetries the source documents at length: an absolute path
//     clamps `..` at its root, and a relative path KEEPS a leading `..`;
//   * `joinFilePath` agreeing with `normalizeFilePath`, so one file has one spelling whichever entry
//     point produced it;
//   * the async batch: an aborted walk rejects before the provider is consulted at all, a provider that
//     was never supplied resolves its documented default, and the walk forwards its options only when
//     the caller supplied them;
//   * discovery order and matching: `findFiles` keeps the directory's own order and matches a pattern
//     against an entry's name or its path;
//   * host identity: each operation reads only its own slot on the capability it was given, and an
//     aborted signal stops the operation before the host is reached.
//
// NOT ported here: `writeBinaryFileChunks`' abort machinery (the abort listener's identity, the chunk
// copies, and the writer-abort suppression). Driving it needs a host-side stream fixture — an
// AsyncIterable of chunks plus a writer — which is a fixture this file would have to invent rather than
// port, and the override's own equivalence note is the record of what that lowering does.
#include <flight/executor.hpp>
#include <flight/filesystem/filesystem.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
using Host = flight::types::HostFileSystemCapability;
using Entry = flight::types::FileEntry;
using WalkOptions = flight::types::FileWalkOptions;

flight::Ref<Entry> entry(const String& name, const String& path) {
  auto value = flight::make_ref<Entry>();
  value->name = name;
  value->path = path;
  value->is_directory = false;
  return value;
}

flight::Array<flight::Ref<Entry>> entries_of(std::initializer_list<flight::Ref<Entry>> values) {
  flight::Array<flight::Ref<Entry>> out = flight::Array<flight::Ref<Entry>>{};
  for (const auto& value : values) out.push(value);
  return out;
}

std::string paths_of(const flight::Array<flight::Ref<Entry>>& entries) {
  std::string joined;
  for (const auto& item : entries) joined += item->path.to_utf8() + ";";
  return joined;
}

}  // namespace

int main() {
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const flight::ExecutorScope scope(executor);

  // normalizeFilePath: one spelling, with the two documented asymmetries about `..`.
  {
    check(flight::filesystem::normalize_file_path(String("/a/b/../c")) == String("/a/c"),
          "an interior `..` resolves against the segment before it");
    check(flight::filesystem::normalize_file_path(String("/a/../../b")) == String("/b"),
          "an absolute path clamps `..` at its root");
    check(flight::filesystem::normalize_file_path(String("a/../../b")) == String("../b"),
          "a relative path KEEPS a leading `..` rather than rewriting a path that escapes its base");
    check(flight::filesystem::normalize_file_path(String("/a/./b")) == String("/a/b"),
          "a `.` segment collapses");
    check(flight::filesystem::normalize_file_path(String("/a//./b")) == String("/a/b"),
          "empty and `.` segments collapse together");
    check(flight::filesystem::normalize_file_path(String("/..")) == String("/"),
          "the root is the only referent above an absolute path");
    check(flight::filesystem::normalize_file_path(String("..")) == String(".."),
          "a lone `..` is kept");
    check(flight::filesystem::normalize_file_path(String("a/..")) == String("."),
          "a relative path that resolves to its own base is `.`");
    check(flight::filesystem::normalize_file_path(String("a/../../..")) == String("../.."),
          "and repeated leading `..` segments are all kept");
  }

  // joinFilePath agrees with normalizeFilePath, whichever way a caller spells the same path.
  {
    flight::Array<String> absolute_segments = flight::Array<String>{};
    absolute_segments.push(String("/a"));
    absolute_segments.push(String("b"));
    absolute_segments.push(String("c"));
    check(flight::filesystem::join_file_path(absolute_segments) ==
              flight::filesystem::normalize_file_path(String("/a/b/c")),
          "joining absolute segments agrees with normalizing the joined string");
    flight::Array<String> relative_segments = flight::Array<String>{};
    relative_segments.push(String("a"));
    relative_segments.push(String(".."));
    relative_segments.push(String("b"));
    check(flight::filesystem::join_file_path(relative_segments) ==
              flight::filesystem::normalize_file_path(String("a/../b")),
          "and the same holds when the join contains a `..`");
  }

  // isAbsoluteFilePath, and the pathless defaults the source documents.
  {
    check(flight::filesystem::is_absolute_file_path(String("/x")), "a leading slash is absolute");
    check(flight::filesystem::is_absolute_file_path(String("C:/x")), "a drive letter is absolute");
    check(!flight::filesystem::is_absolute_file_path(String("x")), "a bare name is relative");
    check(!flight::filesystem::is_absolute_file_path(String("")), "and so is an empty path");
    check(flight::filesystem::get_file_system_path(String("anything")) == String(""),
          "getFileSystemPath has no native provider and answers with the empty path");
    // The supply's watchPath takes its own `detail::FileWatchEvent` listener and answers with a
    // documented no-op, so there is no behavior here to assert: the source returns `() => {}` and so
    // does the copy.
  }

  // The async batch: an aborted walk rejects before the provider is reached.
  {
    int calls = 0;
    auto host = flight::make_ref<Host>();
    host->read_directory_recursive = [&calls](String path, std::optional<flight::StructuralRef<
                                                                  flight::RowReadonly<flight::RowOf<
                                                                      flight::Ref<WalkOptions>>>>>) {
      ++calls;
      return flight::Task<flight::Array<flight::Ref<Entry>>>::resolve(entries_of({entry(String("a"), path + String("/a"))}));
    };

    auto options = flight::make_ref<WalkOptions>();
    flight::AbortController controller;
    controller.abort(String("stopped"));
    options->signal = controller.signal;

    bool rejected = false;
    try {
      static_cast<void>(flight::filesystem::read_directory_recursive(
                            host, String("/root"),
                            flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<WalkOptions>>>>(
                                options))
                            .get());
    } catch (...) {
      rejected = true;
    }
    check(rejected, "an already-aborted walk rejects");
    check(calls == 0, "and never consults the provider");
  }

  // A provider that was never supplied resolves the documented default, and the options are forwarded
  // only when the caller supplied them.
  {
    check(flight::filesystem::read_directory_recursive(flight::make_ref<Host>(), String("/root"))
                  .get()
                  .size() == 0.0,
          "a walk with no provider resolves an empty list");

    int with_options = 0;
    int without_options = 0;
    auto host = flight::make_ref<Host>();
    host->read_directory_recursive = [&with_options, &without_options](
                                         String path,
                                         std::optional<flight::StructuralRef<flight::RowReadonly<
                                             flight::RowOf<flight::Ref<WalkOptions>>>>> options) {
      if (options.has_value()) {
        ++with_options;
      } else {
        ++without_options;
      }
      return flight::Task<flight::Array<flight::Ref<Entry>>>::resolve(entries_of({entry(String("a"), path)}));
    };

    static_cast<void>(flight::filesystem::read_directory_recursive(host, String("/root")).get());
    const auto options = flight::make_ref<WalkOptions>();
    static_cast<void>(
        flight::filesystem::read_directory_recursive(
            host, String("/root"),
            flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<WalkOptions>>>>(options))
            .get());
    check(without_options == 1, "a walk with no options forwards absence");
    check(with_options == 1, "and a walk with options forwards them");
  }

  // findFiles: the directory's own order, matched by name or by path.
  {
    auto host = flight::make_ref<Host>();
    host->read_directory_recursive = [](String, std::optional<flight::StructuralRef<
                                                       flight::RowReadonly<flight::RowOf<
                                                           flight::Ref<WalkOptions>>>>>) {
      return flight::Task<flight::Array<flight::Ref<Entry>>>::resolve(
          entries_of({entry(String("second.txt"), String("second.txt")),
                      entry(String("first.txt"), String("first.txt")),
                      // Its NAME cannot match the pattern below; only its path can.
                      entry(String("data.cfg"), String("assets/pictures/logo.png"))}));
    };

    const auto all = flight::filesystem::find_files(host, String(""), String("**/*.txt")).get();
    check(paths_of(all) == "second.txt;first.txt;",
          "discovery keeps the directory's own order, not a sorted one");

    const auto by_path = flight::filesystem::find_files(host, String(""), String("**/*.png")).get();
    check(paths_of(by_path) == "assets/pictures/logo.png;",
          "a pattern matches an entry's PATH even when the entry's name cannot match it");

    const auto none = flight::filesystem::find_files(host, String(""), String("**/*.md")).get();
    check(none.size() == 0.0, "and a pattern nothing matches finds nothing");
  }

  // An empty directory short-circuits discovery.
  {
    int calls = 0;
    auto host = flight::make_ref<Host>();
    host->read_directory_recursive = [&calls](String, std::optional<flight::StructuralRef<
                                                            flight::RowReadonly<flight::RowOf<
                                                                flight::Ref<WalkOptions>>>>>) {
      ++calls;
      return flight::Task<flight::Array<flight::Ref<Entry>>>::resolve(flight::Array<flight::Ref<Entry>>{});
    };
    check(flight::filesystem::find_files(host, String("/empty"), String("*.txt")).get().size() == 0.0,
          "an empty directory discovers nothing");
    check(calls == 1, "after reading it once");
  }

  // writeBinaryFile: the same two host-identity rules as the walk.
  {
    int calls = 0;
    auto host = flight::make_ref<Host>();
    host->write_binary_file = [&calls](String, flight::Uint8Array, std::optional<flight::AbortSignal>) {
      ++calls;
      return flight::Task<bool>::resolve(true);
    };

    flight::AbortController controller;
    controller.abort(String("stopped"));
    bool rejected = false;
    try {
      static_cast<void>(
          flight::filesystem::write_binary_file(host, String("/tmp/x"),
                                                flight::Uint8Array(flight::Array{1.0, 2.0}), controller.signal)
              .get());
    } catch (...) {
      rejected = true;
    }
    check(rejected, "an already-aborted write rejects");
    check(calls == 0, "and never consults the provider");

    check(!flight::filesystem::write_binary_file(flight::make_ref<Host>(), String("/tmp/x"),
                                                 flight::Uint8Array(flight::Array{1.0}))
                   .get(),
          "a write with no provider resolves its documented default of false");

    static_cast<void>(flight::filesystem::write_binary_file(host, String("/tmp/x"),
                                                            flight::Uint8Array(flight::Array{1.0}))
                          .get());
    check(calls == 1, "and a write with no signal still reaches the provider");
  }

  if (failures == 0) std::cout << "generated filesystem: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
