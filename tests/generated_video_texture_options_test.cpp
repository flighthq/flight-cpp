#include <flight/texture/video_texture.hpp>
#include <flight/texture/cube_texture.hpp>
#include <flight/types/bitmap.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
int failures = 0;
void check(bool value, const char* message) {
  if (value) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Value>
class RecordedCell final : public flight::TypedRowCell<Value> {
 public:
  RecordedCell(Value initial, std::string name, std::vector<std::string>& trace)
      : value(std::move(initial)), name_(std::move(name)), trace_(trace) {}
  Value& get() override {
    trace_.push_back(name_);
    if (throws) throw flight::Error(flight::String("option read failed"));
    return value;
  }
  void set(Value next) override { value = std::move(next); }
  Value value;
  bool throws = false;
 private:
  std::string name_;
  std::vector<std::string>& trace_;
};

class RecordedOptions final : public flight::RowOwner {
 public:
  explicit RecordedOptions(std::vector<std::string>& trace) : trace_(trace) {}
  template <typename Value>
  auto add(const std::string& name, Value value) {
    auto cell = std::make_shared<RecordedCell<Value>>(std::move(value), name, trace_);
    set_named_cell(name, cell);
    return cell;
  }
  void add_runtime() {
    runtime_ = std::make_shared<RecordedCell<std::optional<flight::Ref<flight::types::EntityRuntime>>>>(
        std::nullopt, "entityRuntime", trace_);
  }
  std::shared_ptr<flight::RowCell> symbol_cell(const flight::Symbol& key) const override {
    if (key == flight::types::entity_runtime_key) return runtime_;
    return flight::RowOwner::symbol_cell(key);
  }
 private:
  std::vector<std::string>& trace_;
  std::shared_ptr<flight::RowCell> runtime_;
};
}

int main() {
  namespace tex = flight::texture;
  namespace types = flight::types;
  using CubeRow = tex::ReadonlyPartialVideoOptions<types::TextureCube>;
  using Signature = flight::Ref<types::Texture2D> (*)(
      tex::ReadonlyVideoHost, flight::Ref<types::VideoResource>,
      std::optional<tex::VideoTextureOptions>);
  const Signature construct = &tex::create_video_texture;
  const auto element = flight::host_sdl::ImageSource::rgba8(2, 1,
      flight::Uint8ClampedArray{255, 0, 0, 255, 0, 255, 0, 255},
      flight::host_sdl::ImageSourceKind::video_element);
  const auto source = flight::make_ref<types::VideoResource>();
  source->element = element;
  const auto host = flight::make_ref<types::HostVideoCapability>();
  host->can_play_type = [](flight::String) { return true; };
  int width_calls = 0, height_calls = 0;
  bool same_element = true;
  host->get_width = [&](types::HostImageSource received) {
    ++width_calls; same_element = same_element && received == element; return 23.0;
  };
  host->get_height = [&](types::HostImageSource received) {
    ++height_calls; same_element = same_element && received == element; return 11.0;
  };
  int readonly_writes = 0;
  const auto host_view = flight::make_structural_write_proxy(tex::ReadonlyVideoHost(host),
      std::string("getWidth"), [&] { ++readonly_writes; });
  const auto image_of = [](const auto& texture) {
    return tex::get_video_image(types::TextureLike{
        std::in_place_type<flight::Ref<types::Texture2D>>, texture}).value();
  };
  std::vector<types::TextureLike> input_arms{
      types::TextureLike{std::in_place_type<flight::Ref<types::Texture2D>>, tex::create_texture2_d()},
      types::TextureLike{std::in_place_type<flight::Ref<types::TextureCube>>, tex::create_cube_texture()},
      types::TextureLike{std::in_place_type<flight::Ref<types::Texture2DArray>>,
          tex::create_texture(flight::make_ref<types::CreateTexture2DArrayOptions>())},
      types::TextureLike{std::in_place_type<flight::Ref<types::Texture3D>>,
          tex::create_texture(flight::make_ref<types::CreateTexture3DOptions>())}};
  for (const auto& arm : input_arms) {
    std::visit([&](const auto& input) {
      using Owner = typename std::decay_t<decltype(input)>::element_type;
      using Row = tex::ReadonlyPartialVideoOptions<Owner>;
      input->color_space = flight::String("linear");
      input->flip_x = true; input->flip_y = false; input->version = 17.0;
      input->sampler->anisotropy = 7.0;
      input->uv_offset->x = 0.25; input->uv_offset->y = 0.5;
      input->uv_scale->x = 2.0; input->uv_scale->y = 3.0; input->uv_rotation = 0.75;
      const auto original_dimension = input->dimension;
      const auto output = construct(host_view, source,
          tex::VideoTextureOptions{std::in_place_type<Row>, Row(input)});
      const auto image = image_of(output);
      check(output->dimension == flight::String("2d") && output->version == 4294967295.0 &&
                image->version == 4294967295.0 && image->source == element &&
                image->width == 23.0 && image->height == 11.0,
            "each readonly partial arm receives forced 2D video identity/version and host dimensions");
      check(output->color_space == input->color_space && output->flip_x && !output->flip_y &&
                output->uv_rotation == 0.75 && output->sampler->anisotropy == 7.0 &&
                output->sampler != input->sampler && output->uv_offset != input->uv_offset &&
                output->uv_scale != input->uv_scale && output->uv_offset->x == 0.25 &&
                output->uv_scale->y == 3.0,
            "all four option arms retain values and allocate independent sampling/UV owners");
      check(input->dimension == original_dimension && input->version == 17.0 &&
                input->sampler->anisotropy == 7.0,
            "forced video writes cannot mutate the readonly input arm");
    }, arm);
  }
  check(width_calls == 4 && height_calls == 4 && same_element && readonly_writes == 0,
        "readonly host rows route each borrowed source once without writes or owner recovery");
  const auto partial = flight::make_structural_ref<typename CubeRow::schema_type>(
      flight::row_field<flight::RowKey<"colorSpace">>(std::optional<flight::String>{flight::String("")}),
      flight::row_field<flight::RowKey<"flipX">>(std::optional<bool>{false}),
      flight::row_field<flight::RowKey<"uvRotation">>(std::optional<double>{0.0}),
      flight::row_field<flight::RowKey<"version">>(std::optional<double>{123.0}));
  const auto sparse_options = tex::create_video_texture(host_view, source, partial);
  check(sparse_options->color_space == flight::String("") && !sparse_options->flip_x &&
            sparse_options->uv_rotation == 0.0 && sparse_options->version == 4294967295.0 &&
            sparse_options->uv_offset->x == 0.0 && sparse_options->uv_scale->x == 1.0,
        "partial rows preserve supplied empty/false/zero values and default omitted members");
  const auto empty_host = flight::make_ref<types::HostVideoCapability>();
  empty_host->can_play_type = [](flight::String) { return false; };
  const auto defaults = construct(tex::ReadonlyVideoHost(empty_host), source, std::nullopt);
  check(image_of(defaults)->width == 0.0 && image_of(defaults)->height == 0.0 &&
            defaults->color_space == flight::String("srgb") && defaults->sampler->mipmaps,
        "omitted options and missing host dimension callbacks retain source defaults");
  const auto absent_source = flight::make_ref<types::VideoResource>();
  const int previous_width = width_calls, previous_height = height_calls;
  const auto absent = construct(host_view, absent_source, tex::VideoTextureOptions{partial});
  check(!absent->source && absent->version == 4294967295.0 &&
            absent->color_space == flight::String("") && width_calls == previous_width &&
            height_calls == previous_height,
        "null borrowed element suppresses callbacks while still spreading options");

  std::vector<std::string> trace;
  auto options = std::make_shared<RecordedOptions>(trace);
  options->add("version", std::optional<double>{123.0});
  options->add("sources", std::optional<types::TextureSourceCubeFaces>{types::TextureSourceCubeFaces{std::nullopt}});
  options->add("dimension", std::optional<flight::String>{flight::String("cube")});
  auto uv = flight::geometry::create_vector2(0.5, 0.75);
  auto uv_cell = options->add("uvOffset", std::optional<flight::Ref<types::Vector2>>{uv});
  auto color_cell = options->add("colorSpace", std::optional<flight::String>{flight::String("before")});
  options->add("sampler", std::optional<flight::Ref<types::Sampler>>{tex::create_sampler()});
  options->add("uvScale", std::optional<flight::Ref<types::Vector2>>{uv});
  options->add("uvRotation", std::optional<double>{0.0});
  options->add("flipX", std::optional<bool>{false});
  options->add("flipY", std::optional<bool>{true});
  options->add_runtime();
  const auto recorded = CubeRow::from_owner(options);
  host->get_width = [&](types::HostImageSource) {
    trace.push_back("width"); color_cell->value = flight::String("after");
    host->get_height = [&](types::HostImageSource) { trace.push_back("height"); return 19.0; };
    return 17.0;
  };
  host->get_height = [&](types::HostImageSource) { trace.push_back("old-height"); return -1.0; };
  const auto ordered = tex::create_video_texture(host_view, source, recorded);
  const std::vector<std::string> expected{"width", "height", "version", "sources", "dimension",
      "uvOffset", "colorSpace", "sampler", "uvScale", "uvRotation", "flipX", "flipY", "entityRuntime"};
  check(trace == expected && ordered->color_space == flight::String("after") &&
            image_of(ordered)->height == 19.0,
        "image callbacks precede ordered string/symbol spread and callback replacement is observed");
  check(ordered->uv_offset != uv && ordered->uv_scale != uv && ordered->uv_offset != ordered->uv_scale &&
            ordered->uv_offset->x == 0.5 && ordered->uv_scale->y == 0.75,
        "a shared input UV owner is independently cloned for both destination fields");
  trace.clear(); uv_cell->throws = true;
  bool read_error = false;
  try { (void)tex::create_video_texture(host_view, source, recorded); }
  catch (const flight::Error& error) { read_error = error.message() == flight::String("option read failed"); }
  check(read_error && trace == std::vector<std::string>{"width", "height", "version", "sources", "dimension", "uvOffset"},
        "option exceptions preserve completed callbacks and stop later property reads");
  uv_cell->throws = false; trace.clear();
  host->get_width = [&](types::HostImageSource) -> double {
    trace.push_back("width"); throw flight::Error(flight::String("host width failed"));
  };
  bool host_error = false;
  try { (void)tex::create_video_texture(host_view, source, recorded); }
  catch (const flight::Error& error) { host_error = error.message() == flight::String("host width failed"); }
  check(host_error && trace == std::vector<std::string>{"width"},
        "host exceptions occur before height and any option spread");
  return failures == 0 ? 0 : 1;
}
