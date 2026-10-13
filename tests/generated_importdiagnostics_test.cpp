#include <flight/importdiagnostics/contract.hpp>

#include <iostream>
#include <string>
#include <variant>

// Native port of packages/importdiagnostics/src/importDiagnosticCollector.test.ts and
// importDiagnosticText.test.ts at a627849, against the generated headers.

namespace {

using flight::String;
using flight::types::ImportDiagnostic;
using Detail = flight::Record<String, std::variant<bool, double, String>>;
using Sink = flight::Array<flight::Ref<ImportDiagnostic>>;
namespace diagnostics = flight::importdiagnostics;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

Detail detail_of(std::initializer_list<std::pair<const char*, std::variant<bool, double, String>>> entries) {
  Detail detail;
  for (const auto& [key, value] : entries) detail.set(String(key), value);
  return detail;
}

const auto& severity() { return flight::types::import_diagnostic_severity; }

}  // namespace

int main() {
  // collectImportDiagnostics
  const Sink collected = diagnostics::collect_import_diagnostics([](Sink sink) {
    diagnostics::report_import_diagnostic(sink, severity()->skip, String("awd2.method-material"), String("resolveAwdMaterial"),
                                          detail_of({{"count", 2.0}}));
    diagnostics::report_import_diagnostic(sink, severity()->reject, String("awd2.unsupported-version"), String("parseAwd2"),
                                          detail_of({{"version", 3.0}}));
  });
  check(collected.size() == 2, "returns the crumbs the run callback emits into its engaged collector");
  check(collected.size() == 2 && collected[0]->kind == String("awd2.method-material") &&
            collected[1]->kind == String("awd2.unsupported-version"),
        "crumbs keep the order they were reported in");
  check(diagnostics::collect_import_diagnostics([](Sink) {}).size() == 0,
        "returns an empty array when the parse emits nothing");

  // reportImportDiagnostic
  Sink sink;
  diagnostics::report_import_diagnostic(sink, severity()->drop, String("awd2.skin-vertex-mismatch"), String("buildAwdSkin"),
                                        detail_of({{"actual", 3.0}, {"expected", 4.0}}));
  check(sink.size() == 1, "pushes onto the caller's own array: the sink is shared, not copied");
  if (sink.size() == 1) {
    const auto& crumb = sink[0];
    check(crumb->kind == String("awd2.skin-vertex-mismatch") && crumb->origin == String("buildAwdSkin") &&
              crumb->severity == String("Drop"),
          "pushes a structured crumb with kind, origin and severity");
    check(crumb->detail.has_value() && crumb->detail->size() == 2 &&
              std::get<double>(crumb->detail->get(String("actual")).value()) == 3.0 &&
              std::get<double>(crumb->detail->get(String("expected")).value()) == 4.0,
          "pushes the detail record as given");
  }
  Sink without_detail;
  diagnostics::report_import_diagnostic(without_detail, severity()->reject, String("md2.bad-magic"), String("parseMd2"));
  check(without_detail.size() == 1 && !without_detail[0]->detail.has_value() &&
            without_detail[0]->severity == String("Reject"),
        "carries the detail through as absent when none is supplied");
  bool threw = false;
  try {
    diagnostics::report_import_diagnostic(std::nullopt, severity()->drop, String("gltf.accessor-out-of-bounds"), String("readAccessor"));
  } catch (...) {
    threw = true;
  }
  check(!threw, "does nothing when the collector is absent -- the default silent path");

  // formatImportDiagnostic
  const auto mismatch = flight::make_ref<ImportDiagnostic>(ImportDiagnostic{
      .detail = detail_of({{"expected", 4.0}, {"actual", 3.0}}),
      .kind = String("awd2.skin-vertex-mismatch"),
      .origin = String("buildAwdSkin"),
      .severity = severity()->drop,
  });
  check(diagnostics::format_import_diagnostic(mismatch) == String("Drop buildAwdSkin: awd2.skin-vertex-mismatch actual=3 expected=4"),
        "renders severity, origin and kind with detail keys sorted");
  const auto bad_magic = flight::make_ref<ImportDiagnostic>(ImportDiagnostic{
      .detail = std::nullopt,
      .kind = String("md2.bad-magic"),
      .origin = String("parseMd2"),
      .severity = severity()->reject,
  });
  check(diagnostics::format_import_diagnostic(bad_magic) == String("Reject parseMd2: md2.bad-magic"),
        "omits the detail section when there is no detail");


  // Added by the builder7 unit that regenerated, compiled and mutation-checked this port: three properties the
  // upstream suite leaves untested. The two halves meeting (a crumb the collector recorded is what the text side
  // renders), each detail value domain rendering as its own text rather than one catch-all, and the collector
  // being an append-only log rather than a set.
  const Sink recorded = diagnostics::collect_import_diagnostics([](Sink sink) {
    diagnostics::report_import_diagnostic(sink, severity()->skip, String("awd2.method-material"),
                                          String("resolveAwdMaterial"), detail_of({{"count", 2.0}}));
  });
  check(recorded.size() == 1 &&
            diagnostics::format_import_diagnostic(recorded[0]) ==
                String("Skip resolveAwdMaterial: awd2.method-material count=2"),
        "a crumb the collector recorded renders as the line its fields describe");

  Detail domains;
  domains.set(String("flag"), true);
  domains.set(String("count"), 2.0);
  domains.set(String("name"), String("Skin"));
  const auto aggregate = flight::make_ref<ImportDiagnostic>(ImportDiagnostic{
      .detail = domains,
      .kind = String("awd2.aggregate"),
      .origin = String("build"),
      .severity = severity()->drop,
  });
  check(diagnostics::format_import_diagnostic(aggregate) == String("Drop build: awd2.aggregate count=2 flag=true name=Skin"),
        "each detail value domain renders as its own text, in sorted key order");

  Sink repeated;
  diagnostics::report_import_diagnostic(repeated, severity()->drop, String("awd2.skin-vertex-mismatch"), String("buildAwdSkin"));
  diagnostics::report_import_diagnostic(repeated, severity()->drop, String("awd2.skin-vertex-mismatch"), String("buildAwdSkin"));
  check(repeated.size() == 2, "the same crumb reported twice is two crumbs: the collector is an append-only log");

  return failures == 0 ? 0 : 1;
}
