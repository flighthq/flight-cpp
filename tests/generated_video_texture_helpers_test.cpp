#include <flight/texture/video_texture.hpp>
#include <flight/texture/cube_texture.hpp>
#include <flight/types/bitmap.hpp>

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace tex = flight::texture;
namespace types = flight::types;
namespace {
int failures = 0;
void check(bool value, const char* name) {if (!value) {std::cerr << "FAIL: " << name << '\n'; ++failures;}}
template <typename Value> class Cell final : public flight::TypedRowCell<Value> {
 public:
  Cell(Value initial, std::string name, std::vector<std::string>& trace)
      : value(std::move(initial)), name(std::move(name)), trace(trace) {}
  Value& get() override {
    trace.push_back(name);
    if (action) action();
    if (throws) throw flight::Error(flight::String("read failed"));
    return value;
  }
  void set(Value next) override {value = std::move(next);}
  Value value;
  bool throws = false;
  std::function<void()> action;
 private:
  std::string name;
  std::vector<std::string>& trace;
};
class Owner final : public flight::RowOwner {
 public:
  explicit Owner(std::vector<std::string>& trace) : trace(trace) {}
  template <typename Value> auto add(const std::string& name, Value value) {
    auto cell = std::make_shared<Cell<Value>>(std::move(value), name, trace);
    set_named_cell(name, cell);return cell;
  }
 private:
  std::vector<std::string>& trace;
};
template <typename Ref> tex::ReadonlyVideoTexture readonly_texture(Ref source) {
  using Row = tex::ReadonlyVideoTextureArm<typename Ref::element_type>;
  return tex::ReadonlyVideoTexture{std::in_place_type<Row>, Row(source)};
}
}

int main() {
  using R = std::variant<tex::ReadonlyVideoTextureArm<types::Texture2DArray>,
      tex::ReadonlyVideoTextureArm<types::TextureCube>,tex::ReadonlyVideoTextureArm<types::Texture2D>,
      tex::ReadonlyVideoTextureArm<types::Texture3D>>;
  using H = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::HostVideoCapability>>>>;
  types::Texture (*clone)(R) = &tex::clone_video_texture;
  void (*copy)(types::TextureLike,R) = &tex::copy_video_texture;
  void (*uv)(types::Matrix3Like,R) = &tex::get_video_texture_uv_matrix;
  void (*inverse)(types::Matrix3Like,R) = &tex::get_video_texture_inverse_uv_matrix;
  double (*width)(H,R) = &tex::get_video_texture_width;
  double (*height)(H,R) = &tex::get_video_texture_height;
  bool (*ready)(H,R) = &tex::is_video_texture_frame_ready;
  void (*set_source)(H,types::TextureLike,flight::Ref<types::VideoResource>) = &tex::set_video_texture_source;
  double (*emitted_width)(H,types::TextureLike) = &tex::get_video_texture_width;
  double (*emitted_height)(H,types::TextureLike) = &tex::get_video_texture_height;
  bool (*emitted_ready)(H,types::TextureLike) = &tex::is_video_texture_frame_ready;
  check(clone && copy && uv && inverse && width && height && ready && set_source,
      "all eight readonly source signatures are independently assignable function values");
  const auto bitmap = flight::make_ref<types::Bitmap>();
  const auto voxel = flight::make_ref<types::VoxelGrid>();
  const types::TextureSourceValue face{std::in_place_type<flight::Ref<types::Bitmap>>,bitmap};
  const auto two_d = tex::create_texture_2d(); two_d->source = face;
  const auto cube = tex::create_cube_texture();cube->sources.element(0) = face;
  const auto array = tex::create_texture(flight::make_ref<types::CreateTexture2DArrayOptions>());
  array->sources = flight::Array<std::optional<types::TextureSourceValue>>{face,std::nullopt};
  const auto three_d = tex::create_texture(flight::make_ref<types::CreateTexture3DOptions>());three_d->source = voxel;
  const std::vector<types::TextureLike> arms{types::TextureLike{two_d},types::TextureLike{cube},types::TextureLike{array},types::TextureLike{three_d}};
  for (const auto& arm : arms) std::visit([&](const auto& input) {
    using Ref = std::decay_t<decltype(input)>;
    input->color_space = flight::String("linear");input->flip_x = true;
    input->uv_offset->x = 0.25;input->uv_scale->y = 3;input->uv_rotation = 0.75;
    input->sampler->anisotropy = 7;input->version = 4294967297.0;
    const auto result = clone(readonly_texture(input));const auto output = std::get<Ref>(result);
    check(output != input && output->sampler != input->sampler && output->uv_offset != input->uv_offset &&
        output->uv_scale != input->uv_scale && output->sampler->anisotropy == 7 && output->version == 1 &&
        output->color_space == input->color_space && output->uv_offset->x == 0.25 && output->uv_scale->y == 3,
        "each arm clone has independent nested owners and u32 version with retained values");
    if constexpr (requires {input->sources;}) {
      check(output->sources != input->sources && output->sources.element(0) == input->sources.element(0),
          "clone source lists get fresh identity while their source owners remain shared");
    } else check(output->source == input->source, "clone scalar source identity is shared");
    const auto old_sampler = output->sampler;const auto old_offset = output->uv_offset;const auto old_scale = output->uv_scale;
    copy(result, readonly_texture(input));
    check(output->sampler == old_sampler && output->uv_offset == old_offset && output->uv_scale == old_scale,
        "copy keeps destination sampler and vector identities");
    if constexpr (requires {input->sources;}) {
      const auto old_list = input->sources;copy(arm, readonly_texture(input));
      check(input->sources != old_list && input->sources.element(0) == old_list.element(0),
          "self-copy still replaces source-list identity without losing source owners");
    }
    output->uv_offset = input->uv_scale;const auto x = input->uv_offset->x;
    copy(result, readonly_texture(input));
    check(input->uv_scale->x == x && output->uv_scale->x == x, "cross-alias UV writes retain copy order");
  }, arm);
  std::vector<std::string> trace;
  auto fields = std::make_shared<Owner>(trace);
  fields->add("colorSpace", flight::String("linear"));fields->add("flipX",true);fields->add("flipY",true);
  auto source_sampler_cell=fields->add("sampler", tex::create_sampler());
  auto offset = fields->add("uvOffset",flight::geometry::create_vector2(0.25,0.5));
  auto rotation = fields->add("uvRotation",flight::pi/2);
  auto scale = fields->add("uvScale",flight::geometry::create_vector2(2,3));
  fields->add("version",4294967297.0);auto dimension = fields->add("dimension",flight::String("cube"));
  fields->add("sources",types::TextureSourceCubeFaces{face,std::nullopt});
  using CubeRow = tex::ReadonlyVideoTextureArm<types::TextureCube>;
  const R observed{CubeRow::from_owner(fields)};
  (void)clone(observed);
  check(trace == std::vector<std::string>{"colorSpace","flipX","flipY","sampler","uvOffset","uvRotation","uvScale","version","dimension","sources"},
      "clone preserves source object-literal read order before discriminant/content");
  trace.clear();copy(types::TextureLike{cube},observed);
  check(trace == std::vector<std::string>{"dimension","colorSpace","flipX","flipY","uvRotation","version","sampler","uvOffset","uvScale","dimension","sources"},
      "copy preserves snapshot/nested-write/content recheck order");
  const auto sampler_before=cube->sampler;const auto replacement_sampler=tex::create_sampler();
  source_sampler_cell->value->anisotropy=7;
  source_sampler_cell->action=[&] {cube->sampler=replacement_sampler;};
  copy(types::TextureLike{cube},observed);
  check(sampler_before->anisotropy==7&&cube->sampler==replacement_sampler&&replacement_sampler->anisotropy==1,
      "destination reference is captured before source getter replaces its owner");
  source_sampler_cell->action={};cube->sampler=sampler_before;
  trace.clear();offset->throws = true;bool error = false;
  try {(void)clone(observed);} catch (const flight::Error&) {error=true;}
  check(error && trace == std::vector<std::string>{"colorSpace","flipX","flipY","sampler","uvOffset"},
      "clone read errors stop before later source fields");offset->throws=false;
  trace.clear();dimension->value=flight::String("2d");const auto old_color=cube->color_space;error=false;
  try {copy(types::TextureLike{cube},observed);} catch(const flight::Error& e) {error=e.message()==flight::String("copyTexture requires matching dimensions");}
  check(error && trace == std::vector<std::string>{"dimension"} && cube->color_space==old_color,
      "dimension mismatch precedes destination mutation");dimension->value=flight::String("cube");
  trace.clear();auto matrix=flight::make_ref<types::Matrix3>();matrix->m=flight::Float32Array(9);
  uv(matrix,observed);
  check(trace == std::vector<std::string>{"uvRotation","flipX","flipY","uvScale","uvScale","flipX","uvScale","flipY","uvScale","uvOffset","uvOffset"},
      "UV path retains repeated getter reads and branch order");
  check(matrix->m[1]==-2 && matrix->m[3]==3 && matrix->m[6]==-2.75 && matrix->m[7]==2.5 && matrix->m[8]==1,
      "quarter-turn flips/scales/translation match pinned matrix");
  inverse(matrix,observed);check(std::abs(matrix->m[1]-1.0/3)<1e-6 && matrix->m[3]==-0.5,
      "inverse operates in place on completed forward matrix");
  scale->value->x=0;inverse(matrix,observed);bool all_nan=true;
  for(int i=0;i<9;++i)all_nan=all_nan&&std::isnan(static_cast<double>(matrix->m[i]));
  check(all_nan,"singular scale fills all inverse matrix entries with NaN");scale->value->x=2;
  trace.clear();rotation->throws=true;matrix->m.fill(7);error=false;
  try {uv(matrix,observed);}catch(const flight::Error&){error=true;}
  check(error&&trace==std::vector<std::string>{"uvRotation"}&&matrix->m[0]==7,
      "UV read error precedes all matrix writes");rotation->throws=false;

  const auto element=flight::host_sdl::ImageSource::rgba8(1,1,flight::Uint8ClampedArray{1,2,3,255},flight::host_sdl::ImageSourceKind::video_element);
  const auto resource=flight::make_ref<types::VideoResource>();resource->element=element;
  const auto empty_host=flight::make_ref<types::HostVideoCapability>();empty_host->can_play_type=[](flight::String){return true;};
  const auto video=tex::create_video_texture(H(empty_host),resource);
  const auto image=tex::get_video_image(types::TextureLike{video}).value();
  check(tex::get_video_texture_width(H(empty_host),video)==-1 &&
      tex::get_video_texture_height(H(empty_host),video)==-1 &&
      !tex::is_video_texture_frame_ready(H(empty_host),video),
      "readonly host with a native concrete arm has an unambiguous call surface");
  check(emitted_width(H(empty_host),types::TextureLike{video})==-1 &&
      emitted_height(H(empty_host),types::TextureLike{video})==-1 &&
      !emitted_ready(H(empty_host),types::TextureLike{video}),
      "pinned emitter native-union host-row function signatures bridge to readonly cells");
  auto host_fields=std::make_shared<Owner>(trace);
  using Ready=std::optional<std::function<bool(types::HostImageSource)>>;
  using Size=std::optional<std::function<double(types::HostImageSource)>>;
  bool same_source=true;
  auto is_ready=host_fields->add("isReady",Ready{[&](types::HostImageSource source){trace.push_back("ready-call");same_source=same_source&&source==element;return true;}});
  auto get_height=host_fields->add("getHeight",Size{[&](types::HostImageSource){trace.push_back("old-height-call");return -1.;}});
  auto get_width=host_fields->add("getWidth",Size{[&](types::HostImageSource source){
    trace.push_back("width-call");same_source=same_source&&source==element;
    get_height->value=[&](types::HostImageSource source){trace.push_back("height-call");same_source=same_source&&source==element;return 11.;};return 23.;}});
  const auto host=H::from_owner(host_fields);const auto view=readonly_texture(video);
  trace.clear();check(ready(host,view)&&same_source&&trace==std::vector<std::string>{"isReady","ready-call","getWidth","width-call","getHeight","height-call"},
      "ready checks short circuit in order and read replaced height callback late");
  trace.clear();is_ready->value=[&](types::HostImageSource){trace.push_back("ready-call");return false;};
  check(!ready(host,view)&&trace==std::vector<std::string>{"isReady","ready-call"},"false readiness suppresses dimension callbacks");
  is_ready->value=[&](types::HostImageSource){trace.push_back("ready-call");return true;};
  get_width->value=[&](types::HostImageSource){trace.push_back("width-call");return 0.;};trace.clear();
  check(!ready(host,view)&&trace==std::vector<std::string>{"isReady","ready-call","getWidth","width-call"},"zero width suppresses height");
  check(width(host,view)==-1&&height(host,view)==11,"size queries preserve positive values and use -1 for zero");
  get_width->value=[](types::HostImageSource){return std::numeric_limits<double>::quiet_NaN();};
  check(width(host,view)==-1&&width(H(empty_host),view)==-1,"NaN and absent callbacks yield -1");
  trace.clear();get_width->throws=true;error=false;
  try {(void)ready(host,view);}catch(const flight::Error&){error=true;}
  check(error&&trace==std::vector<std::string>{"isReady","ready-call","getWidth"},"host getter exceptions stop readiness before later callbacks");get_width->throws=false;
  trace.clear();check(!ready(host,readonly_texture(two_d))&&width(host,readonly_texture(cube))==-1&&trace.empty(),
      "non-image closed source and non2D texture suppress host callbacks");
  get_width->value=[&](types::HostImageSource){trace.push_back("width-call");return 7.;};
  trace.clear();set_source(host,types::TextureLike{video},resource);
  const auto replacement=tex::get_video_image(types::TextureLike{video}).value();
  check(replacement!=image&&replacement->source==element&&replacement->width==7&&video->version==4294967295.0&&
      trace==std::vector<std::string>{"getWidth","width-call","getHeight","height-call"},
      "set source creates fresh image identity then assigns/reset version after callbacks");
  const auto absent=flight::make_ref<types::VideoResource>();trace.clear();set_source(host,types::TextureLike{video},absent);
  check(!video->source&&trace.empty(),"missing element detaches source without host reads");
  trace.clear();error=false;try {set_source(host,types::TextureLike{cube},resource);}catch(const flight::Error& e){error=e.message()==flight::String("setVideoTextureSource requires a Texture2D");}
  check(error&&trace.empty(),"set source dimension error precedes image callbacks");
  video->source=types::TextureSourceValue{std::in_place_type<flight::Ref<types::ImageResource>>,replacement};video->version=9;
  get_height->throws=true;error=false;try {set_source(host,types::TextureLike{video},resource);}catch(const flight::Error&){error=true;}
  check(error&&std::get<flight::Ref<types::ImageResource>>(*video->source)==replacement&&video->version==9,
      "failed replacement callback preserves previous source and version");
  return failures==0?0:1;
}
