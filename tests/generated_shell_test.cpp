// Focused behavior tests for @flighthq/shell.
//
// The package emits whole at a627849 -- 3/3 modules, no refusals, no override -- so this is the
// emission's own behavior under test. Every case is ported from the module's upstream suite
// (packages/shell/src/shell.test.ts), which is the specification:
//
//   * the URL policy matches schemes case-insensitively from the caller's own policy, blocks a
//     malformed URL, and blocks everything under an empty policy;
//   * the batch trash operation starts every one-path operation before any of them settles and returns
//     the outcomes in the input's order;
//   * an external open blocks before dispatch when the scheme is not allowed, and two live hosts stay
//     isolated from each other;
//   * the path, reveal, shortcut-read and shortcut-write entry points return their own outcome types
//     unchanged, including a provider's failure message;
//   * beep dispatches synchronously, and spawn forwards the command, the argument vector and the
//     optional options unchanged while returning the provider's own process object.
#include <flight/executor.hpp>
#include <flight/shell/shell.hpp>

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
using Policy = flight::types::ShellExternalUrlPolicy;
using TrashOutcome = flight::types::ShellTrashOutcome;
using ExternalOutcome = flight::types::ShellExternalOutcome;
using RevealOutcome = flight::types::ShellPathRevealOutcome;
using WriteOutcome = flight::types::ShellShortcutLinkWriteOutcome;
using PathOpenOutcome = flight::types::ShellPathOpenOutcome;
using ShortcutReadOutcome = flight::types::ShellShortcutLinkReadOutcome;
using Process = flight::types::ShellProcess;
using ShortcutLink = flight::types::ShellShortcutLink;

flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Policy>>>> policy_row(
    const flight::Ref<Policy>& policy) {
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Policy>>>>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Policy>>>>(policy));
}

flight::Ref<Policy> policy_of(std::initializer_list<const char*> schemes) {
  auto policy = flight::make_ref<Policy>();
  for (const char* scheme : schemes) policy->allowed_schemes.push(String(scheme));
  return policy;
}

flight::Ref<TrashOutcome> trash_outcome(const char* reason) {
  return flight::make_ref<TrashOutcome>(TrashOutcome{.reason = String(reason)});
}

flight::Ref<ExternalOutcome> external_outcome(const char* reason) {
  return flight::make_ref<ExternalOutcome>(ExternalOutcome{.reason = String(reason)});
}

// A ShellProcess cannot be default-constructed: its `exit` member is a Task, which has no default
// constructor. The aggregate is built with every member spelled, which is how the emitted code builds
// its own records.
flight::Ref<Process> make_process() {
  auto exit_status = flight::make_ref<flight::types::ShellProcessExitStatus>();
  exit_status->code = 0.0;
  using ExitRow = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ShellProcessExitStatus>>>>;
  return flight::make_ref<Process>(Process{
      .entity_runtime_key = std::nullopt,
      .exit = flight::Task<ExitRow>::resolve(ExitRow(exit_status)),
      .stderr = flight::ReadableStream<flight::Uint8Array>{},
      .stdin = flight::WritableStream<flight::Uint8Array>{},
      .stdout = flight::ReadableStream<flight::Uint8Array>{},
      .terminate = []() {}});
}

}  // namespace

int main() {
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const flight::ExecutorScope scope(executor);

  // The URL policy: case-insensitive, explicit, and blocking by default.
  {
    auto https_policy = policy_of({"HTTPS"});
    const auto row = policy_row(https_policy);
    check(flight::shell::is_shell_url_allowed(String("https://example.test"), row),
          "an allowed scheme matches the policy case-insensitively");
    check(!flight::shell::is_shell_url_allowed(String("mailto:user@example.test"), row),
          "a scheme outside the policy is not allowed");
    check(!flight::shell::is_shell_url_allowed(String("not a URL"), row),
          "a malformed URL is not allowed rather than throwing");
    check(!flight::shell::is_shell_url_allowed(String("https://example.test"), policy_row(policy_of({}))),
          "an empty policy blocks every URL");
  }

  // The batch trash operation: every operation starts before any settles, and order is preserved.
  {
    flight::Array<String> seen = flight::Array<String>{};
    bool settled = false;
    auto host = flight::make_ref<flight::types::HostShellTrashCapability>();
    host->move_to_trash = [&seen](String path) {
      seen.push(path);
      return flight::Task<flight::Ref<TrashOutcome>>::resolve(
          trash_outcome(path == String("/first") ? "ok" : "operation-failed"));
    };

    flight::Array<String> paths = flight::Array<String>{};
    paths.push(String("/first"));
    paths.push(String("/second"));
    const auto pending = flight::shell::move_shell_items_to_trash(host, paths);
    // The map runs when the call is made, not when the aggregate is awaited: both one-path operations
    // are already started, in input order, before anything has settled.
    check(seen.size() == 2.0 && seen.element(0.0) == String("/first") &&
              seen.element(1.0) == String("/second"),
          "every one-path operation starts immediately, in input order");
    check(!settled, "and the aggregate is not settled until it is awaited");

    const auto outcomes = pending.get();
    settled = true;
    check(outcomes.size() == 2.0, "every outcome is present");
    check(outcomes.element(0.0)->reason == String("ok") &&
              outcomes.element(1.0)->reason == String("operation-failed"),
          "and the outcomes are in the input's order, not the settlement order");
  }

  // A single trash operation routes through the supplied provider.
  {
    int calls = 0;
    auto host = flight::make_ref<flight::types::HostShellTrashCapability>();
    host->move_to_trash = [&calls](String) {
      ++calls;
      return flight::Task<flight::Ref<TrashOutcome>>::resolve(trash_outcome("ok"));
    };
    check(flight::shell::move_shell_item_to_trash(host, String("/only")).get()->reason == String("ok"),
          "the single-path trash operation routes through the provider");
    check(calls == 1, "exactly once");
  }

  // The external open: blocked before dispatch, and two hosts stay isolated.
  {
    int opens = 0;
    flight::Array<String> calls = flight::Array<String>{};
    auto first = flight::make_ref<flight::types::HostShellExternalCapability>();
    first->open = [&calls, &opens](String url) {
      ++opens;
      calls.push(String("first:") + url);
      return flight::Task<flight::Ref<ExternalOutcome>>::resolve(external_outcome("ok"));
    };
    auto second = flight::make_ref<flight::types::HostShellExternalCapability>();
    second->open = [&calls, &opens](String url) {
      ++opens;
      calls.push(String("second:") + url);
      return flight::Task<flight::Ref<ExternalOutcome>>::resolve(external_outcome("operation-failed"));
    };
    auto https_only = policy_row(policy_of({"https"}));

    check(flight::shell::open_shell_external_url(first, String("file:///etc/passwd"), https_only)
                  .get()
                  ->reason == String("blocked-scheme"),
          "an unallowed scheme is blocked");
    check(opens == 0, "and never reaches the provider");

    check(flight::shell::open_shell_external_url(first, String("https://one.test"), https_only)
                  .get()
                  ->reason == String("ok"),
          "the first host answers for itself");
    check(flight::shell::open_shell_external_url(second, String("https://two.test"), https_only)
                  .get()
                  ->reason == String("operation-failed"),
          "and the second host's own answer is not shared");
    check(calls.size() == 2.0 && calls.element(0.0) == String("first:https://one.test") &&
              calls.element(1.0) == String("second:https://two.test"),
          "each host received exactly its own URL");
  }

  // The path entry points return their own outcome types unchanged, including a failure message.
  {
    auto open_host = flight::make_ref<flight::types::HostShellPathOpenCapability>();
    open_host->open = [](String) {
      auto failure = flight::make_ref<flight::types::message_reason_38ed09bf88f1d5eb>();
      failure->reason = String("operation-failed");
      failure->message = String("no handler for the path");
      return flight::Task<PathOpenOutcome>::resolve(
          PathOpenOutcome{std::in_place_index<0>, failure});
    };
    const auto opened = flight::shell::open_shell_path(open_host, String("/tmp/x")).get();
    check(opened.index() == 0, "a path-open failure keeps its own arm");
    check(opened.index() == 0 &&
              std::get<0>(opened)->message == String("no handler for the path"),
          "and its message survives");

    auto reveal_host = flight::make_ref<flight::types::HostShellPathRevealCapability>();
    reveal_host->reveal = [](String) {
      return flight::Task<flight::Ref<RevealOutcome>>::resolve(
          flight::make_ref<RevealOutcome>(RevealOutcome{.reason = String("ok")}));
    };
    check(flight::shell::reveal_shell_path(reveal_host, String("/tmp/x")).get()->reason == String("ok"),
          "reveal returns its own outcome");
  }

  // The shortcut-link entry points: read and an explicit write operation.
  {
    flight::Array<String> written = flight::Array<String>{};
    auto host = flight::make_ref<flight::types::HostShellShortcutLinkCapability>();
    host->read = [](String) {
      // The read outcome's arms are both object-backed: the success arm carries the parsed link.
      auto outcome = flight::make_ref<flight::types::link_reason_591377b92cfbfdf5>();
      outcome->reason = String("ok");
      auto link = flight::make_ref<ShortcutLink>();
      outcome->link = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<ShortcutLink>>>>(link);
      return flight::Task<ShortcutReadOutcome>::resolve(
          ShortcutReadOutcome{std::in_place_index<0>, outcome});
    };
    host->write = [&written](String shortcut_path, flight::StructuralRef<flight::RowReadonly<
                                                   flight::RowOf<flight::Ref<ShortcutLink>>>>,
                             flight::types::ShellShortcutWriteOperation operation) {
      written.push(shortcut_path);
      written.push(operation);
      return flight::Task<flight::Ref<WriteOutcome>>::resolve(
          flight::make_ref<WriteOutcome>(WriteOutcome{.reason = String("ok")}));
    };

    const auto read = flight::shell::read_shell_shortcut_link(host, String("/tmp/link")).get();
    check(read.index() == 0 && std::get<0>(read)->reason == String("ok"),
          "the shortcut read returns its own outcome");

    const auto link = flight::make_ref<ShortcutLink>();
    const auto written_outcome = flight::shell::write_shell_shortcut_link(
                                     host, String("/tmp/link"),
                                     flight::StructuralRef<flight::RowReadonly<
                                         flight::RowOf<flight::Ref<ShortcutLink>>>>(link),
                                     String("create"))
                                     .get();
    check(written_outcome->reason == String("ok"), "the shortcut write returns its own outcome");
    check(written.size() == 2.0 && written.element(0.0) == String("/tmp/link") &&
              written.element(1.0) == String("create"),
          "and the path and the explicit operation reach the provider unchanged");
  }

  // beep dispatches synchronously.
  {
    int beeps = 0;
    auto host = flight::make_ref<flight::types::HostShellBeepCapability>();
    host->beep = [&beeps]() { ++beeps; };
    flight::shell::shell_beep(host);
    check(beeps == 1, "beep dispatches synchronously, with no task to await");
  }

  // spawn forwards the command, the argument vector and the options, and returns the provider's process.
  {
    const auto process = make_process();
    flight::Array<String> seen_args = flight::Array<String>{};
    bool saw_options = false;
    auto host = flight::make_ref<flight::types::HostShellProcessCapability>();
    host->spawn = [&](String command, flight::Array<String> args,
                      std::optional<flight::StructuralRef<flight::RowReadonly<
                          flight::RowOf<flight::Ref<flight::types::ShellProcessOptions>>>>> options) {
      seen_args.push(command);
      for (const auto& arg : args) seen_args.push(arg);
      saw_options = options.has_value();
      return process;
    };

    auto options = flight::make_ref<flight::types::ShellProcessOptions>();
    const auto returned = flight::shell::spawn_shell_process(
        host, String("/bin/tool"),
        flight::Array<String>{String("--flag"), String("value")},
        flight::StructuralRef<flight::RowReadonly<
            flight::RowOf<flight::Ref<flight::types::ShellProcessOptions>>>>(options));

    check(returned == process, "spawn returns the provider's own process object");
    check(seen_args.size() == 3.0 && seen_args.element(0.0) == String("/bin/tool") &&
              seen_args.element(1.0) == String("--flag") && seen_args.element(2.0) == String("value"),
          "the command and every argument reach the provider unchanged");
    check(saw_options, "and so do the options when they are supplied");

    const auto without_options = flight::shell::spawn_shell_process(
        host, String("/bin/tool"), flight::Array<String>{String("--flag")});
    check(without_options == process, "spawn without options still returns the provider's process");
  }

  if (failures == 0) std::cout << "generated shell: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
