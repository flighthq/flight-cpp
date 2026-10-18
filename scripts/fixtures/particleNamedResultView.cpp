// Synthetic same-owner/native ABI controls; not installed parser qualification.
#include "named-result.hpp"
#include <iostream>
#include <type_traits>
#include <vector>
namespace api = flighthq_example;
using NativeWide = flight::Ref<api::WideResult> (*)(flight::String);
using NativeSmall = flight::Ref<api::SmallResult> (*)(flight::String);
using ReadOptions = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<api::Options>>>>;
using WideRow = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<api::WideResult>>>>;
using ReadWide = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<api::WideResult>>>>;
static_assert(std::is_same_v<decltype(&api::parse_wide), NativeWide>);
static_assert(std::is_same_v<decltype(&api::parse_small), NativeSmall>);
static_assert(!std::is_constructible_v<WideRow, ReadWide>);
static_assert(!std::is_invocable_v<NativeWide, flight::String, ReadOptions>);
struct Sentinel {};
struct OptionsCell final : flight::TypedRowCell<std::optional<double>> {
  std::optional<double> value{2}; int reads = 0;
  std::shared_ptr<Sentinel> error = std::make_shared<Sentinel>();
  std::optional<double>& get() override { ++reads; throw error; }
  void set(std::optional<double> next) override { value = next; }
};
struct ConfigCell final : flight::TypedRowCell<flight::Ref<api::Config>> {
  api::WideResult* original; int reads = 0, writes = 0; bool throws = false;
  std::shared_ptr<Sentinel> error = std::make_shared<Sentinel>();
  explicit ConfigCell(api::WideResult* owner) : original(owner) {}
  flight::Ref<api::Config>& get() override { ++reads; if (throws) throw error; return original->config; }
  void set(flight::Ref<api::Config> next) override { ++writes; original->config = std::move(next); }
};
int checks = 0;
void check(bool value) { ++checks; if (!value) throw checks; }
int main() {
 try {
  auto full = flight::make_ref<api::WideResult>();
  full->config = flight::make_ref<api::Config>(); full->config->size = 7;
  full->document = flight::make_ref<api::Document>(); full->document->size = 13;
  auto originalDocument = full->document;
  auto sourceRow = WideRow(full);
  auto sourceOwner = sourceRow.shared_owner();
  auto cell = std::make_shared<ConfigCell>(full.get());
  sourceOwner->set_named_cell("config", cell);
  auto optionCell = std::make_shared<OptionsCell>();
  auto optionOwner = std::make_shared<flight::RowOwner>();
  optionOwner->set_named_cell("size", optionCell);
  optionOwner->set_named_cell("width", optionCell);
  auto options = ReadOptions::from_owner(optionOwner);
  int calls = 0; std::vector<int> events;
  api::wide_provider = [&](flight::String text) { ++calls; events.push_back(1); check(text == "input"); return full; };
  const NativeWide native = &api::parse_wide;
  check(native("input") == full && calls == 1);
  check(native == &api::parse_wide && cell->reads == 0);
  auto view = api::wide_codec->parse("input", std::nullopt);
  check(calls == 2 && cell->reads == 0 && cell->writes == 0);
  check(view.shared_owner() == sourceOwner && view.shared_native_object() == full);
  check(view.identity() == sourceRow.identity());
  auto alias = view;
  check(alias.shared_owner() == view.shared_owner());
  auto withOptions = api::wide_codec->parse("input", options);
  check(calls == 3 && optionCell->reads == 0 && cell->reads == 0);
  check(withOptions.shared_owner() == sourceOwner);
  check(flight::callable_identity_equal_named<&api::parse_wide>(api::wide_codec->parse));
  check(!flight::callable_identity_equal_named<&api::parse_small>(api::wide_codec->parse));
  auto copiedCallback = api::wide_codec->parse;
  check(flight::callable_identity_equal(copiedCallback, api::wide_codec->parse));
  check(api::same_wide() && !api::distinct());
  check(flight::row_get<flight::RowKey<"config">>(view) == full->config && cell->reads == 1);
  auto replacement = flight::make_ref<api::Config>(); replacement->size = 19;
  flight::row_set<flight::RowKey<"config">>(alias, replacement);
  check(full->config == replacement && cell->reads == 2 && cell->writes == 0);
  check(flight::row_get<flight::RowKey<"config">>(view) == replacement && cell->reads == 3);
  flight::Array<flight::Ref<api::Diagnostic>> diagnostics;
  diagnostics.push(flight::make_ref<api::Diagnostic>());
  flight::row_set<flight::RowKey<"diagnostics">>(view, diagnostics);
  check(full->diagnostics.identity() == diagnostics.identity());
  full->diagnostics.push(flight::make_ref<api::Diagnostic>());
  check(flight::row_get<flight::RowKey<"diagnostics">>(alias).size() == 2);
  check(full->document == originalDocument && full->document->size == 13);
  full->document->size = 23;
  check(view.shared_native_object() == full && originalDocument->size == 23);
  api::write_config("input", replacement);
  check(full->config == replacement && full->diagnostics.empty() && cell->reads == 4 && cell->writes == 0 && optionCell->reads == 0);
  cell->throws = true;
  auto beforeRead = api::wide_codec->parse("input", options);
  check(cell->reads == 4 && optionCell->reads == 0);
  try { (void)flight::row_get<flight::RowKey<"config">>(beforeRead); check(false); }
  catch (const std::shared_ptr<Sentinel>& error) { check(error == cell->error && cell->reads == 5); }
  cell->throws = false;
  auto error = std::make_shared<Sentinel>();
  api::wide_provider = [&](flight::String) -> flight::Ref<api::WideResult> { ++calls; events.push_back(2); throw error; };
  const int beforeCalls = calls;
  try { api::wide_codec->parse("throw", options); check(false); }
  catch (const std::shared_ptr<Sentinel>& caught) { check(caught == error && calls == beforeCalls + 1 && optionCell->reads == 0); }
  check(events.back() == 2 && cell->reads == 5);
  auto small = flight::make_ref<api::SmallResult>(); small->config = replacement;
  api::small_provider = [&](flight::String text) { check(text == "small"); return small; };
  const NativeSmall smallNative = &api::parse_small;
  check(smallNative("small") == small);
  auto smallView = api::small_codec->parse("small", options);
  check(smallView.shared_native_object() == small && optionCell->reads == 0);
  check(flight::callable_identity_equal_named<&api::parse_small>(api::small_codec->parse));
  check(!flight::callable_identity_equal(api::small_codec->parse, api::wide_codec->parse));
  std::weak_ptr<api::WideResult> lifetime = full;
  std::weak_ptr<api::Document> documentLifetime = originalDocument;
  beforeRead = {}; withOptions = {}; alias = {}; sourceRow = {}; sourceOwner.reset();
  originalDocument.reset(); full.reset();
  check(!lifetime.expired() && !documentLifetime.expired());
  check(flight::row_get<flight::RowKey<"config">>(view) == replacement);
  view = {};
  check(lifetime.expired() && documentLifetime.expired());
  std::cout << "{\"checks\":" << checks << ",\"failures\":0}\n";
 } catch (int failure) { std::cerr << "failed check " << failure << '\n'; return 1; }
}
