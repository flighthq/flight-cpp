#include <flight/texture/texture.hpp>

#include <iostream>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace {
int failures = 0;
int checks = 0;
void check(bool value, const char* message) {
  ++checks;
  if (value) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Value>
class GetterCell final : public flight::TypedRowCell<Value> {
 public:
  GetterCell(std::function<Value&()> read, int& writes) : read_(std::move(read)), writes_(writes) {}
  Value& get() override { return read_(); }
  void set(Value) override { ++writes_; throw flight::TypeError("readonly probe write"); }
 private:
  std::function<Value&()> read_;
  int& writes_;
};

void qualify_readonly_2d_entry() {
  namespace texture = flight::texture;
  namespace types = flight::types;
  using Row = texture::CreateTexture2DOptionsRow;
  using Writable = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<types::CreateTexture2DOptions>>>>;
  using Factory = flight::Ref<types::Texture2D> (*)(std::optional<Row>);
  const Factory factory = static_cast<Factory>(&texture::create_texture);
  struct Unrelated { double value; };
  using WrongRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Unrelated>>>>;
  static_assert(!std::is_convertible_v<Row, Writable>);
  static_assert(!std::is_invocable_v<Factory, std::optional<WrongRow>>);

  const auto defaults = factory(std::nullopt);
  check(defaults->dimension == flight::String("2d") && defaults->color_space == flight::String("srgb") &&
            !defaults->source && defaults->sampler && defaults->uv_scale->x == 1,
        "absent readonly options preserve original 2D defaults");
  const auto legacy = texture::create_texture();
  check(legacy->dimension == defaults->dimension && legacy != defaults,
        "the legacy default overload remains callable and creates its own texture");

  using Source = decltype(types::CreateTexture2DOptions::source);
  auto image = flight::make_ref<types::ImageResource>();
  image->width = 32;
  image->height = 16;
  const auto options = flight::make_ref<types::CreateTexture2DOptions>();
  options->dimension = flight::String("2d");
  options->source = Source{std::in_place_type<flight::Ref<types::ImageResource>>, image};
  const Row row(options);
  check(row.shared_native_object().get() == options.get(), "readonly options retain their exact owner");
  auto first = factory(row);
  check(first->source && std::get<flight::Ref<types::ImageResource>>(*first->source) == image,
        "the original ImageResource union arm and owner survive the wrapper");
  image->width = 47;
  check(texture::get_texture_width(types::TextureLike{first}) == 47,
        "writes to the original source remain live through the texture");
  const auto replacement = flight::make_ref<types::ImageResource>();
  options->source = Source{std::in_place_type<flight::Ref<types::ImageResource>>, replacement};
  check(std::get<flight::Ref<types::ImageResource>>(*factory(row)->source) == replacement &&
            std::get<flight::Ref<types::ImageResource>>(*first->source) == image,
        "later calls read current options while earlier textures retain their original source");
  options->source = Source{std::in_place_type<flight::Null>, flight::null};
  check(!factory(row)->source, "a present null source stays absent in the native nullable slot");
  options->source = Source{std::in_place_type<flight::Undefined>, flight::undefined};
  check(!factory(row)->source, "an undefined source retains the default absence");

  std::weak_ptr<types::CreateTexture2DOptions> options_lifetime;
  std::weak_ptr<types::ImageResource> image_lifetime;
  flight::Ref<types::Texture2D> escaped;
  {
    const auto local = flight::make_ref<types::CreateTexture2DOptions>();
    const auto source = flight::make_ref<types::ImageResource>();
    options_lifetime = local;
    image_lifetime = source;
    local->source = Source{std::in_place_type<flight::Ref<types::ImageResource>>, source};
    escaped = factory(Row(local));
  }
  check(options_lifetime.expired() && !image_lifetime.expired(),
        "the returned texture retains the source without retaining the input options");
  escaped.reset();
  check(image_lifetime.expired(), "releasing the texture releases its last source owner");

  types::CreateTexture2DOptions storage;
  storage.dimension = flight::String("2d");
  storage.color_space = flight::String("linear");
  storage.flip_x = true;
  storage.flip_y = false;
  auto sampler_first = texture::create_sampler(std::nullopt);
  auto sampler_second = texture::create_sampler(std::nullopt);
  sampler_first->anisotropy = 3;
  sampler_second->anisotropy = 7;
  storage.sampler = sampler_second;
  std::optional<flight::Ref<types::Sampler>> first_sampler{sampler_first};
  storage.uv_offset = flight::geometry::create_vector2(.25, .5);
  storage.uv_rotation = .75;
  storage.uv_scale = flight::geometry::create_vector2(2, 3);
  storage.version = 4294967297.0;
  storage.source = Source{std::in_place_type<flight::Ref<types::ImageResource>>, image};
  storage.resource = decltype(storage.resource){std::in_place_type<flight::Null>, flight::null};
  const auto owner = std::make_shared<flight::RowOwner>();
  std::vector<std::string> reads;
  int writes = 0, sampler_reads = 0;
  bool fail_second_sampler = false;
  const auto bind = [&]<typename Value>(const char* key, Value& value) {
    owner->set_named_cell(key, std::make_shared<GetterCell<Value>>(
        [&, key, pointer = &value]() -> Value& { reads.emplace_back(key); return *pointer; }, writes));
  };
  bind("dimension", storage.dimension);
  bind("colorSpace", storage.color_space);
  bind("flipX", storage.flip_x);
  bind("flipY", storage.flip_y);
  owner->set_named_cell("sampler", std::make_shared<GetterCell<decltype(storage.sampler)>>([&]() -> auto& {
    reads.emplace_back("sampler");
    ++sampler_reads;
    if (fail_second_sampler && sampler_reads == 2) storage.sampler.reset();
    return sampler_reads == 1 ? first_sampler : storage.sampler;
  }, writes));
  owner->set_named_cell("uvOffset", std::make_shared<GetterCell<decltype(storage.uv_offset)>>([&]() -> auto& {
    reads.emplace_back("uvOffset"); sampler_second->anisotropy = 99; return storage.uv_offset;
  }, writes));
  owner->set_named_cell("uvRotation", std::make_shared<GetterCell<decltype(storage.uv_rotation)>>([&]() -> auto& {
    reads.emplace_back("uvRotation"); (*storage.uv_offset)->x = 9; return storage.uv_rotation;
  }, writes));
  bind("uvScale", storage.uv_scale);
  bind("version", storage.version);
  bind("source", storage.source);
  bind("resource", storage.resource);
  const auto getter_row = Row::from_owner(owner);
  check(reads.empty(), "forming the readonly row evaluates no getter");
  const auto observed = factory(getter_row);
  // Derived by running the complete original pinned texture.ts and its real dependencies.
  const std::vector<std::string> expected{
      "colorSpace", "flipX", "flipY", "sampler", "sampler", "uvOffset", "uvOffset", "uvRotation",
      "uvScale", "uvScale", "version", "dimension", "colorSpace", "flipX", "flipY", "sampler",
      "sampler", "uvOffset", "uvOffset", "uvRotation", "uvScale", "uvScale", "version", "source", "resource"};
  check(reads == expected, "common prelude, switch and leaf preserve the original getter order and counts");
  check(observed->sampler->anisotropy == 99 && observed->uv_offset->x == 9 && observed->version == 1,
        "changing getters and clone timing match the complete original source");
  check(writes == 0 && std::get<flight::Ref<types::ImageResource>>(*observed->source) == image,
        "the wrapper writes no option cell and retains the ImageResource alternative");
  reads.clear(); sampler_reads = 0; fail_second_sampler = true;
  bool threw = false;
  try { (void)factory(getter_row); }
  catch (const flight::TypeError& error) {
    threw = std::string(error.what()) == "Cannot read properties of undefined (reading 'anisotropy')";
  }
  check(threw && reads == std::vector<std::string>{"colorSpace", "flipX", "flipY", "sampler", "sampler"},
        "a nullish second getter throws the original TypeError before later getters");

  const auto bad_owner = std::make_shared<flight::RowOwner>();
  bad_owner->set_named_value("source", flight::String("wrong-cell-domain"));
  bool rejected = false;
  try { (void)factory(Row::from_owner(bad_owner)); } catch (const std::bad_cast&) { rejected = true; }
  check(rejected, "a wrong source cell domain is rejected rather than converted or fabricated");
}
}

int main() {
  namespace texture = flight::texture;
  namespace types = flight::types;
  using CubeRow = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<types::CreateTextureCubeOptions>>>>;
  using WritableCubeRow = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<types::CreateTextureCubeOptions>>>>;
  static_assert(!std::is_convertible_v<CubeRow, WritableCubeRow>);

  const auto sampler = texture::create_sampler(std::nullopt);
  sampler->anisotropy = 4.0;
  const auto face = flight::make_ref<types::Bitmap>();
  face->width = 32.0;
  types::TextureSourceCubeFaces sources{
      std::nullopt, types::TextureSourceValue{
          std::in_place_type<flight::Ref<types::Bitmap>>, face},
      std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  const auto opts = flight::make_ref<types::CreateTextureCubeOptions>();
  opts->dimension = flight::String("cube");
  opts->sources = sources;
  opts->sampler = sampler;
  opts->color_space = flight::String("linear");
  opts->flip_x = true;
  opts->version = 4294967297.0;
  const auto offset = flight::geometry::create_vector2(0.25, 0.5);
  const auto scale = flight::geometry::create_vector2(0.5, 0.75);
  opts->uv_offset = offset;
  opts->uv_scale = scale;
  opts->uv_rotation = 0.75;
  const auto resource = flight::make_ref<types::ExternalImageResourceReference>();
  opts->resource = resource;
  const CubeRow row(opts);
  const auto cube = texture::create_texture(row);
  check(cube->dimension == flight::String("cube") &&
            cube->color_space == flight::String("linear") && cube->flip_x,
        "readonly cube options retain dimension, color and flip state");
  check(cube->version == 1.0 && cube->uv_rotation == 0.75,
        "readonly cube options preserve numeric fields and u32 version wrapping");
  check(opts->sampler == sampler && cube->sampler != sampler &&
            cube->sampler->anisotropy == 4.0,
        "construction clones sampling state without writing its options");
  check(cube->uv_offset != offset && cube->uv_offset->x == 0.25 &&
            cube->uv_scale != scale && cube->uv_scale->y == 0.75,
        "construction deep-clones supplied UV owners");
  check(cube->sources.size() == 6 && !cube->sources.element(0.0),
        "construction retains six face slots and empty elements");
  check(&cube->sources.element(0.0) != &sources.element(0.0) &&
            std::get<flight::Ref<types::Bitmap>>(*cube->sources.element(1.0)) == face,
        "construction creates fresh face-list storage with the same source owner");
  sources.element(1.0) = std::nullopt;
  check(cube->sources.element(1.0).has_value(),
        "reassigning an input slot cannot change the returned cube");
  check(resource->textures && resource->textures->size() == 1 &&
            std::get<flight::Ref<types::TextureCube>>(resource->textures->element(0.0)) == cube,
        "resource attachment retains the returned cube exactly once");

  const auto literal = flight::make_structural_ref<CubeRow::schema_type>(
      flight::row_field<flight::RowKey<"dimension">>(flight::String("cube")));
  const auto defaults = texture::create_texture(literal);
  check(defaults->sources.size() == 6 && defaults->color_space == flight::String("srgb") &&
            defaults->sampler && defaults->uv_scale->x == 1.0,
        "the generated structural literal boundary supplies the cube defaults");
  for (const auto& value : defaults->sources) check(!value, "default cube faces are empty");

  const auto empty_opts = flight::make_ref<types::CreateTextureCubeOptions>();
  empty_opts->dimension = flight::String("cube");
  empty_opts->sources = types::TextureSourceCubeFaces{};
  empty_opts->color_space = flight::String("");
  const auto empty_cube = texture::create_texture(CubeRow(empty_opts));
  check(empty_cube->sources.empty() && empty_cube->color_space == flight::String(""),
        "present empty arrays and empty strings retain their supplied values");

  // The shared row helper must retain the existing 2D constructor boundary.
  const auto two_d_row = flight::make_structural_ref<texture::CreateTexture2DOptionsRow::schema_type>(
      flight::row_field<flight::RowKey<"colorSpace">>(
          std::optional<flight::String>{flight::String("linear")}));
  const auto two_d = texture::create_texture_2d(two_d_row);
  check(two_d->dimension == flight::String("2d") &&
            two_d->color_space == flight::String("linear") && !two_d->source,
        "the shared readonly helper retains 2D construction and absence");
  qualify_readonly_2d_entry();
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures == 0 ? 0 : 1;
}
