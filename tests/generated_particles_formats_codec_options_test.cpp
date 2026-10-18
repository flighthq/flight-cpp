// Independent boundary controls. This supplies no built-in codec adapter or parser implementation.
#include <flight/particles_formats/starling_pex_parse.hpp>
#include <flight/particles_formats/libgdx_parse.hpp>
#include <flight/types/particle_format_codec.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <type_traits>

namespace {
// The installed key table has no textureSize entry until a real lowered read requests it.
// Use its existing size key for this independent row protocol; do not fabricate SDK metadata.
struct Options : flight::ReferenceEnabled { std::optional<double> size; };
using Read = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Options>>>>;
using Write = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Options>>>>;
using Size = flight::RowKey<"size">;
using NativeOptions = flight::types::StarlingPexParseOptions;
using Config = flight::Ref<flight::types::ParticleEmitterConfig>;
using NativeParse = Config (*)(flight::String, std::optional<flight::Ref<NativeOptions>>);
using Codec = decltype(flight::types::ParticleFormatCodec::parse_to_config);
static_assert(std::is_same_v<decltype(&flight::particles_formats::parse_starling_pex), NativeParse>);
// A read view must never implicitly manufacture a nominal allocation, including at this callback.
using ActualRead = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ParseParticleConfigOptions>>>>;
static_assert(!std::is_convertible_v<ActualRead, flight::Ref<NativeOptions>>);
static_assert(!std::is_convertible_v<Read, flight::Ref<NativeOptions>>);
static_assert(!std::is_constructible_v<Codec, NativeParse>);

// The same control follows the public slot when its equivalent Required row is generated.
// Before that normal generation the slot owns a Ref; afterwards it already is a writable row.
template<typename Carrier> struct ResultView { using type = Carrier; };
template<typename Object> struct ResultView<std::shared_ptr<Object>> {
  using type = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Object>>>>;
};

int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
// Synthetic one-member protocol, deliberately separate from the real parser's body.
double read_size(std::optional<Read> options) {
  if (!options) return 1;
  return flight::row_get<Size>(*options).value_or(1);
}
struct Sentinel {};
struct ObservableCell final : flight::TypedRowCell<std::optional<double>> {
  std::optional<double> value{2};
  int reads = 0;
  bool throws = false;
  std::shared_ptr<Sentinel> sentinel = std::make_shared<Sentinel>();
  std::optional<double>& get() override {
    ++reads;
    if (throws) throw sentinel;
    return value;
  }
  void set(std::optional<double> replacement) override { value = replacement; }
};
double plain(double value) { return value; }
}

int main() {
  auto native = flight::make_ref<Options>();
  native->size = 2;
  const Write writable(native);
  const Read readonly(writable);
  check(writable.identity() == readonly.identity(), "readonly projection keeps owner");
  check(read_size(readonly) == 2, "native field reads through readonly row");
  native->size = 4;
  check(read_size(readonly) == 4, "native writes remain live");
  flight::row_set<Size>(writable, std::optional<double>{8});
  check(native->size == 8 && read_size(readonly) == 8, "alias writes use physical cell");

  const auto literal = flight::make_structural_ref<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<Options>>>>>(
      flight::row_field<Size>(std::optional<double>{3}));
  check(!literal.shared_exact_object<Options>() && !literal.shared_exact_object<NativeOptions>(),
        "source-valid structural options need no nominal allocation");
  check(read_size(literal) == 3, "structural options keep their original field");
  const auto absent = flight::make_structural_ref<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<Options>>>>>();
  check(read_size(std::nullopt) == 1 && read_size(absent) == 1, "absent options and omitted field default");

  auto cell = std::make_shared<ObservableCell>();
  auto owner = std::make_shared<flight::RowOwner>();
  owner->set_named_cell("size", cell);
  const Read observable = Read::from_owner(owner);
  check(observable.identity() == owner.get(), "custom accessor retains original owner");
  check(read_size(observable) == 2 && cell->reads == 1, "one property read at use");
  cell->value = 5;
  check(read_size(observable) == 5 && cell->reads == 2, "later read observes changed accessor value");
  cell->value.reset();
  check(read_size(observable) == 1 && cell->reads == 3, "present undefined field defaults at use");
  cell->throws = true;
  bool same_error = false;
  try { (void)read_size(observable); }
  catch (const std::shared_ptr<Sentinel>& error) { same_error = error == cell->sentinel; }
  check(same_error && cell->reads == 4, "accessor exception keeps exact thrown owner");
  int evaluations = 0;
  auto producer = [&]() -> std::optional<Read> { ++evaluations; return literal; };
  check(read_size(producer()) == 3 && evaluations == 1, "argument producer evaluated once");

  std::function<double(double)> pointer = plain;
  check(flight::callable_identity_equal(pointer, pointer), "plain function identity remains available");
  std::function<double(double)> adapter = [](double value) { return plain(value); };
  bool identity_unavailable = false;
  try { (void)flight::callable_identity_equal(adapter, adapter); }
  catch (const std::logic_error&) { identity_unavailable = true; }
  check(identity_unavailable, "plain wrapper cannot pretend to preserve source function identity");
  flight::Function<double(double)> identified = [](double value) { return value; };
  const auto identified_copy = identified;
  check(identified == identified_copy, "existing same-signature Function copies preserve identity");
  check(!(identified == flight::Function<double(double)>{[](double value) { return value; }}),
        "fresh callable is a distinct source identity");

  const auto options = flight::make_ref<NativeOptions>();
  options->texture_size = 2;
  const auto config = flight::particles_formats::parse_starling_pex("<particleEmitterConfig/>", options);
  const auto result = flight::particles_formats::parse_starling_pex_document("<particleEmitterConfig/>", options);
  check(config->scale_min == 16 && result->config->scale_min == 16,
        "original public native Ref entrypoints remain callable");
  check(result->document->start_particle_size == 32 && result->config != config,
        "original result retains extra document owner and authored fresh config");
  // This projection starts with the exact public result Ref, so it does not depend on the withdrawn
  // allocated-type recovery change. The row keeps that same NativeRowOwner through the wider schema.
  using DocumentSlot = decltype(flight::types::ParticleFormatCodec::parse_to_document);
  using SlotResult = std::invoke_result_t<DocumentSlot, flight::String, std::optional<ActualRead>>;
  using Result = flight::types::StarlingPexParseResult;
  using FullRow = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Result>>>>;
  using BaseRow = typename ResultView<SlotResult>::type;
  std::weak_ptr<Result> lifetime;
  {
    auto original = flight::particles_formats::parse_starling_pex_document("<particleEmitterConfig/>", options);
    lifetime = original;
    const FullRow full(original);
    const BaseRow base(full);
    check(base.identity() == full.identity(), "base result row keeps original result identity");
    flight::row_set<flight::RowKey<"config">>(base, config);
    check(original->config == config && original->document->start_particle_size == 32,
          "base result writes reach original result and retain extra document");
    original.reset();
    check(!lifetime.expired() && base.shared_exact_object<Result>() == lifetime.lock(),
          "base result view retains and recovers the original exact owner");
  }
  check(lifetime.expired(), "result view releases original owner after last reference");
  // Conditional exact-shape follow-on: original libGDX public functions and original result owner.
  // The retained full descriptor is refused earlier at its optional trim detector, so this supplies
  // no installed Libgdx codec adapter or built-in registry claim.
  using LibOptions = flight::types::LibgdxParseOptions;
  using LibResult = flight::types::LibgdxParseResult;
  using LibNative = Config (*)(flight::String, std::optional<flight::Ref<LibOptions>>);
  using LibNativeDocument = flight::Ref<LibResult> (*)(flight::String, std::optional<flight::Ref<LibOptions>>);
  static_assert(std::is_same_v<decltype(&flight::particles_formats::parse_libgdx_particle), LibNative>);
  static_assert(std::is_same_v<decltype(&flight::particles_formats::parse_libgdx_particle_document), LibNativeDocument>);
  const flight::String lib_input = "Particle Effect\nScale\nlowMin: 32\nlowMax: 32\nhighMin: 32\nhighMax: 32\n";
  auto lib_options = flight::make_ref<LibOptions>();
  lib_options->texture_size = 2;
  auto lib_result = flight::particles_formats::parse_libgdx_particle_document(lib_input, lib_options);
  check(flight::particles_formats::parse_libgdx_particle(lib_input, lib_options)->scale_min == 16 &&
        lib_result->config->scale_min == 16 && lib_result->document->scale->high_min == 32,
        "original libGDX public native functions preserve options and extra document");
  lib_options->texture_size = 4;
  check(flight::particles_formats::parse_libgdx_particle(lib_input, lib_options)->scale_min == 8,
        "native libGDX alias update remains live");
  using LibFull = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<LibResult>>>>;
  const LibFull lib_full(lib_result);
  const BaseRow lib_base(lib_full);
  flight::row_set<flight::RowKey<"config">>(lib_base, config);
  check(lib_base.identity() == lib_full.identity() && lib_result->config == config &&
        lib_base.shared_exact_object<LibResult>() == lib_result && lib_result->document->scale->high_min == 32,
        "libGDX base result keeps original owner, writable cells and extra document");
  const std::function<Config(flight::String, std::optional<flight::Ref<LibOptions>>)> lib_named =
      flight::NamedFunction<Config(flight::String, std::optional<flight::Ref<LibOptions>>)>::from<
          &flight::particles_formats::parse_libgdx_particle>(flight::particles_formats::parse_libgdx_particle);
  check(flight::callable_identity_equal_named<&flight::particles_formats::parse_libgdx_particle>(lib_named),
        "original libGDX named declaration identity is represented");
  const std::function<flight::Ref<LibResult>(flight::String, std::optional<flight::Ref<LibOptions>>)> lib_document_named =
      flight::NamedFunction<flight::Ref<LibResult>(flight::String, std::optional<flight::Ref<LibOptions>>)>::from<
          &flight::particles_formats::parse_libgdx_particle_document>(flight::particles_formats::parse_libgdx_particle_document);
  check(flight::callable_identity_equal_named<&flight::particles_formats::parse_libgdx_particle_document>(lib_document_named) &&
        !flight::callable_identity_equal(lib_named, lib_document_named) &&
        lib_document_named(lib_input, lib_options)->config->scale_min == 8,
        "libGDX document origin is distinct from config and invokes the original live-options body");
  return failures == 0 ? 0 : 1;
}
