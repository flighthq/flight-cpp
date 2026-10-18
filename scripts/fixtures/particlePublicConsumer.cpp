// Pending installed-output consumer. Build only after the real shared Types and particles run.
// Includes the actual public package; supplies no parser, codec adapter, detector or generated body.
#include <flight/particles_formats/contract.hpp>
#include <flight/callable.hpp>
#include <flight/structural_ref.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

namespace api = flight::particles_formats;
namespace types = flight::types;
namespace {
using Config = flight::Ref<types::ParticleEmitterConfig>;
using Read = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::ParseParticleConfigOptions>>>>;
using Bag = flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<types::ParseParticleConfigOptions>>>>;
int failures = 0;
void check(bool value, const char* label) { if (!value) { ++failures; std::cerr << label << '\n'; } }
void clear() { for (const auto& kind : api::get_registered_particle_formats()) api::unregister_particle_format(kind); }
struct Sentinel {};
struct Cell final : flight::TypedRowCell<std::optional<double>> {
  std::optional<double> value{2};
  int reads = 0;
  int error = 0;
  std::shared_ptr<Sentinel> sentinel = std::make_shared<Sentinel>();
  std::optional<double>& get() override {
    ++reads;
    if (error == 1) throw sentinel;
    if (error == 2) throw flight::Error("options sentinel");
    return value;
  }
  void set(std::optional<double> next) override { value = next; }
};

template<auto ConfigFunction, auto DocumentFunction, typename Options, typename Result,
         auto NativeMember, typename Key, typename Descriptor, typename Measure, typename Expected>
void family(const Descriptor& descriptor, flight::String input, Measure measure, Expected expected, double fallback) {
  using Native = Config (*)(flight::String, std::optional<flight::Ref<Options>>);
  using NativeDocument = flight::Ref<Result> (*)(flight::String, std::optional<flight::Ref<Options>>);
  static_assert(std::is_same_v<decltype(ConfigFunction), Native>);
  static_assert(std::is_same_v<decltype(DocumentFunction), NativeDocument>);
  const auto codec = flight::row_get<flight::RowKey<"codec">>(descriptor);
  const auto kind = flight::row_get<flight::RowKey<"kind">>(descriptor);
  check(api::get_particle_format_codec(kind).value() == codec, "registry retains original descriptor codec");
  check(flight::callable_identity_equal_named<ConfigFunction>(codec->parse_to_config), "actual config slot retains original named function");
  check(flight::callable_identity_equal_named<DocumentFunction>(codec->parse_to_document), "actual document slot retains original named function");
  check(!flight::callable_identity_equal(codec->parse_to_config, codec->parse_to_document), "different original declarations stay unequal across signatures");

  auto native_options = flight::make_ref<Options>();
  native_options.get()->*NativeMember = 2;
  check(measure(ConfigFunction(input, native_options)) == expected(2), "unchanged original public nominal native ABI");
  const auto literal = flight::make_structural_ref<Bag>(flight::row_field<Key>(std::optional<double>{2}));
  const auto omitted = flight::make_structural_ref<Bag>();
  const auto undefined = flight::make_structural_ref<Bag>(flight::row_field<Key>(std::optional<double>{}));
  check(!literal.template shared_exact_object<types::ParseParticleConfigOptions>() && !literal.template shared_exact_object<Options>(), "schema-only literal does not manufacture a nominal options owner");
  check(measure(codec->parse_to_config(input, literal)) == expected(2), "actual callback consumes source-valid structural options");
  check(measure(codec->parse_to_config(input, std::nullopt)) == expected(fallback), "omitted options default");
  check(measure(codec->parse_to_config(input, omitted)) == expected(fallback), "omitted field defaults");
  check(measure(codec->parse_to_config(input, undefined)) == expected(fallback), "present undefined field defaults");

  for (const auto& missing : {std::optional<Read>{}, std::optional<Read>{Read(omitted)}, std::optional<Read>{Read(undefined)}}) {
    const auto result = codec->parse_to_document(input, missing);
    check(measure(flight::row_get<flight::RowKey<"config">>(result)) == expected(fallback), "actual document slot preserves omitted/undefined defaults");
  }
  const auto copied_config = codec->parse_to_config;
  const auto copied_document = codec->parse_to_document;
  check(flight::callable_identity_equal(copied_config, codec->parse_to_config) && flight::callable_identity_equal(copied_document, codec->parse_to_document), "copies of actual callback slots preserve original function identity");

  auto cell = std::make_shared<Cell>();
  auto owner = std::make_shared<flight::RowOwner>();
  owner->set_named_cell(Key::name.view(), cell);
  const Read observable = Read::from_owner(owner);
  int evaluations = 0;
  auto produce = [&] { ++evaluations; return observable; };
  check(measure(codec->parse_to_config(input, produce())) == expected(2) && cell->reads == 1 && evaluations == 1, "actual config slot reads original accessor/argument once");
  cell->value = 4;
  const auto document = codec->parse_to_document(input, observable);
  const auto exact = document.template shared_exact_object<Result>();
  check(exact && measure(exact->config) == expected(4) && cell->reads == 2, "actual document slot observes live options and retains original full result");
  check(exact && document.identity() == exact.get(), "callback view shares original native result identity");
  const int before = cell->reads;
  const auto registered = api::parse_registered_particle_format(input, kind, observable);
  check(measure(registered->config) == expected(4) && cell->reads == before + 1 && registered->format == kind, "public registered route forwards the same readonly options owner");

  std::weak_ptr<Result> lifetime;
  {
    auto result = codec->parse_to_document(input, literal);
    auto original = result.template shared_exact_object<Result>();
    check(original != nullptr, "actual callback result recovers its original full schema");
    if (original) {
      lifetime = original;
      const auto extra_document = original->document;
      auto alias = result;
      const Config replacement = ConfigFunction(input, std::nullopt);
      flight::Array<flight::Ref<types::ImportDiagnostic>> diagnostics;
      flight::row_set<flight::RowKey<"config">>(alias, replacement);
      flight::row_set<flight::RowKey<"diagnostics">>(alias, diagnostics);
      check(original->config == replacement && original->diagnostics.identity() == diagnostics.identity() && original->document == extra_document, "writable base cells reach same original result and retain extra document");
      original.reset();
      check(!lifetime.expired() && alias.template shared_exact_object<Result>() == lifetime.lock(), "view retains original full result after native local release");
    }
  }
  check(lifetime.expired(), "last actual result view releases original owner");
  cell->error = 1;
  bool original_error = false;
  try { (void)codec->parse_to_config(input, observable); }
  catch (const std::shared_ptr<Sentinel>& error) { original_error = error == cell->sentinel; }
  check(original_error, "stored original config callback propagates exact accessor exception owner");
  original_error = false;
  try { (void)codec->parse_to_document(input, observable); }
  catch (const std::shared_ptr<Sentinel>& error) { original_error = error == cell->sentinel; }
  check(original_error, "stored original document callback propagates exact accessor exception owner");

  cell->error = 2;
  const auto rejected = api::parse_registered_particle_format(input, kind, observable);
  bool matching_error = false;
  for (const auto& crumb : rejected->diagnostics) {
    if (crumb->kind != "particles.parse-error" || crumb->origin != "parseRegisteredParticleFormat" || !crumb->detail) continue;
    const auto message = crumb->detail->get("message");
    const auto* text = message ? std::get_if<flight::String>(&*message) : nullptr;
    matching_error = crumb->severity == "Reject" && text && *text == "options sentinel";
  }
  check(matching_error, "actual public registered route catches option error with original diagnostic origin/message");

  // Public mutable-options entrypoints keep their original nominal ABI. This separate control
  // does not qualify schema-only bags through that ABI; the TypeScript fixture records that edge.
  auto public_options = flight::make_ref<types::ParseParticleConfigOptions>();
  public_options->texture_size = 2;
  public_options->pixels_per_unit = 2;
  check(measure(api::parse_particle_config(input, public_options)) == expected(2), "public automatic config dispatch with original nominal options");
  check(measure(api::parse_particle_config_document(input, public_options)->config) == expected(2), "public automatic document dispatch with original nominal options");
}
}

int main() {
  check(api::get_registered_particle_formats().empty(), "importing actual public package installs nothing");
  api::register_built_in_particle_formats();
  api::register_built_in_particle_formats();
  check(api::get_registered_particle_formats().size() == 6, "public installation is idempotent");
  auto scale = [](const Config& value) { return value->scale_min; };
  auto speed = [](const Config& value) { return value->speed_min; };
  auto size = [](double value) { return 32 / value; };
  auto units = [](double value) { return 5 * value; };
  family<&api::parse_starling_pex, &api::parse_starling_pex_document, types::StarlingPexParseOptions, types::StarlingPexParseResult, &types::StarlingPexParseOptions::texture_size, flight::RowKey<"textureSize">>(api::starling_pex_particle_format, "<particleEmitterConfig/>", scale, size, 1);
  family<&api::parse_unity_particle, &api::parse_unity_particle_document, types::UnityParseOptions, types::UnityParseResult, &types::UnityParseOptions::pixels_per_unit, flight::RowKey<"pixelsPerUnit">>(api::unity_particle_format, "{\"gravityModifier\":0}", speed, units, 100);
  family<&api::parse_particle_designer_plist, &api::parse_particle_designer_plist_document, types::ParticleDesignerParseOptions, types::ParticleDesignerParseResult, &types::ParticleDesignerParseOptions::texture_size, flight::RowKey<"textureSize">>(api::particle_designer_particle_format, "<plist><dict><key>maxParticles</key><integer>100</integer></dict></plist>", scale, size, 1);
  family<&api::parse_libgdx_particle, &api::parse_libgdx_particle_document, types::LibgdxParseOptions, types::LibgdxParseResult, &types::LibgdxParseOptions::texture_size, flight::RowKey<"textureSize">>(api::libgdx_particle_format, "Particle Effect\nScale\nlowMin: 32\nlowMax: 32\nhighMin: 32\nhighMax: 32\n", scale, size, 1);
  const auto unity = flight::row_get<flight::RowKey<"codec">>(api::unity_particle_format);
  const auto spine = flight::row_get<flight::RowKey<"codec">>(api::spine_particle_format);
  check(unity->detect("{\"startLifetime\":{\"mode\":\"constant\"}}"), "original Unity nested JSON mode detector");
  check(spine->detect("{\"emission\":{\"low\":1,\"high\":2}}"), "original Spine nested JSON range detector");
  check(!unity->detect("null") && !spine->detect("[]"), "original nullable JSON helper rejects roots");
  const auto libgdx = flight::row_get<flight::RowKey<"codec">>(api::libgdx_particle_format);
  check(libgdx->detect(" \tParticle Effect \r\nmore") && !libgdx->detect("x\nParticle Effect"), "original indexed optional trim detector");
  clear();
  return failures ? 1 : 0;
}
