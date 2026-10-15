#include <flight/texture/video_texture.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace t=flight::texture;namespace y=flight::types;
namespace {
int failures=0,writes=0;
std::vector<std::string> reads;
void check(bool condition,const char* name){if(!condition){++failures;std::cerr<<"FAIL: "<<name<<'\n';}}
template<class Value>class Cell final:public flight::TypedRowCell<Value>{
 public:
  Cell(Value value,std::string name):value(std::move(value)),name(std::move(name)){}
  Value& get()override{reads.push_back(name);return value;}
  void set(Value next)override{++writes;value=std::move(next);}
 private:Value value;std::string name;
};
class Owner final:public flight::RowOwner{
 public:
  explicit Owner(std::string prefix):prefix(std::move(prefix)){}
  template<class Value>void add(const std::string& key,Value value){
    set_named_cell(key,std::make_shared<Cell<Value>>(std::move(value),prefix+"."+key));
  }
 private:std::string prefix;
};
template<class Row>Row guard(Row source,std::initializer_list<const char*> keys){
  for(const auto* key:keys)source=flight::make_structural_write_proxy(source,std::string(key),[]{++writes;throw flight::Error(flight::String("readonly write"));});
  return source;
}
void expect(std::initializer_list<const char*> expected,const char* name){
  std::vector<std::string> values;for(auto key:expected)values.emplace_back(key);
  check(reads==values,name);reads.clear();
}
}
int main(){
  using R=t::ReadonlyVideoTexture;using H=t::ReadonlyVideoHost;
  y::Texture(*clone)(R)=&t::clone_video_texture;
  void(*copy)(y::TextureLike,R)=&t::copy_video_texture;
  void(*uv)(y::Matrix3Like,R)=&t::get_video_texture_uv_matrix;
  void(*inverse)(y::Matrix3Like,R)=&t::get_video_texture_inverse_uv_matrix;
  double(*width)(H,R)=&t::get_video_texture_width;
  double(*height)(H,R)=&t::get_video_texture_height;
  bool(*ready)(H,R)=&t::is_video_texture_frame_ready;
  void(*set_source)(H,y::TextureLike,flight::Ref<y::VideoResource>)=&t::set_video_texture_source;
  const auto element=flight::host_sdl::ImageSource::rgba8(1,1,flight::Uint8ClampedArray{1,2,3,255},flight::host_sdl::ImageSourceKind::video_element);
  const auto image=flight::make_ref<y::ImageResource>();image->source=element;image->version=17;
  const auto fields=std::make_shared<Owner>("texture");
  fields->add("colorSpace",flight::String("linear"));fields->add("flipX",true);fields->add("flipY",true);
  const auto sampler=t::create_sampler();fields->add("sampler",sampler);
  fields->add("uvOffset",flight::geometry::create_vector2(0.25,0.5));fields->add("uvRotation",flight::pi/2);
  fields->add("uvScale",flight::geometry::create_vector2(2,3));fields->add("version",7.0);
  fields->add("dimension",flight::String("2d"));
  fields->add("source",std::optional<y::TextureSourceValue>{y::TextureSourceValue{std::in_place_type<flight::Ref<y::ImageResource>>,image}});
  using Row=t::ReadonlyVideoTextureArm<y::Texture2D>;
  const auto row=guard(Row::from_owner(fields),{"colorSpace","flipX","flipY","sampler","uvOffset","uvRotation","uvScale","version","dimension","source"});
  const R source{std::in_place_type<Row>,row};
  check(row.shared_owner()->native_type()==std::type_index(typeid(void)),"readonly source is a pure row owner behind distinct write proxies");
  auto host_fields=std::make_shared<Owner>("host");
  using Size=std::optional<std::function<double(y::HostImageSource)>>;
  using Ready=std::optional<std::function<bool(y::HostImageSource)>>;
  bool same_source=true;
  host_fields->add("getWidth",Size{[&](y::HostImageSource value){reads.push_back("width");same_source=same_source&&value==element;return 23.;}});
  host_fields->add("getHeight",Size{[&](y::HostImageSource value){reads.push_back("height");same_source=same_source&&value==element;return 11.;}});
  host_fields->add("isReady",Ready{[&](y::HostImageSource value){reads.push_back("ready");same_source=same_source&&value==element;return true;}});
  const auto host=guard(H::from_owner(host_fields),{"getWidth","getHeight","isReady"});
  reads.clear();const auto cloned=std::get<flight::Ref<y::Texture2D>>(clone(source));
  expect({"texture.colorSpace","texture.flipX","texture.flipY","texture.sampler","texture.uvOffset","texture.uvRotation","texture.uvScale","texture.version","texture.dimension","texture.source"},"clone canonical function value reads through pure readonly proxy in source order");
  check(cloned->sampler!=sampler&&std::get<flight::Ref<y::ImageResource>>(*cloned->source)==image,"proxy clone keeps source identity and independent sampler");
  const auto output=t::create_texture2_d();reads.clear();copy(y::TextureLike{output},source);
  expect({"texture.dimension","texture.colorSpace","texture.flipX","texture.flipY","texture.uvRotation","texture.version","texture.sampler","texture.uvOffset","texture.uvScale","texture.dimension","texture.source"},"copy canonical function value preserves repeated dimension reads through readonly proxy");
  const auto matrix=flight::make_ref<y::Matrix3>();matrix->m=flight::Float32Array(9);
  uv(matrix,source);
  expect({"texture.uvRotation","texture.flipX","texture.flipY","texture.uvScale","texture.uvScale","texture.flipX","texture.uvScale","texture.flipY","texture.uvScale","texture.uvOffset","texture.uvOffset"},"UV canonical value keeps repeated getter sequence through readonly proxy");
  inverse(matrix,source);
  expect({"texture.uvRotation","texture.flipX","texture.flipY","texture.uvScale","texture.uvScale","texture.flipX","texture.uvScale","texture.flipY","texture.uvScale","texture.uvOffset","texture.uvOffset"},"inverse canonical value keeps its own complete repeated getter sequence");
  check(matrix->m[3]==-0.5,"proxy inverse returns pinned matrix value");
  check(width(host,source)==23,"proxy width result");
  expect({"texture.dimension","texture.source","host.getWidth","width"},"width canonical value reads texture and callback once through readonly proxies");
  check(height(host,source)==11,"proxy height result");
  expect({"texture.dimension","texture.source","host.getHeight","height"},"height canonical value reads texture and callback once through readonly proxies");
  check(ready(host,source),"proxy readiness result");
  expect({"texture.dimension","texture.source","host.isReady","ready","host.getWidth","width","host.getHeight","height"},"ready canonical value reads both readonly proxies in source order");
  const auto resource=flight::make_ref<y::VideoResource>();resource->element=element;
  const auto resource_view=t::ReadonlyVideoResource(resource);
  resource_view.shared_owner()->set_named_cell("element",std::make_shared<Cell<std::optional<y::HostImageSource>>>(std::optional<y::HostImageSource>{element},"resource.element"));
  const auto previous=output->source;reads.clear();set_source(host,y::TextureLike{output},resource);
  expect({"resource.element","resource.element","host.getWidth","width","host.getHeight","height"},"setSource canonical value preserves repeated resource reads and readonly host proxy reads");
  check(output->source!=previous&&output->version==4294967295.0&&resource->element==element,"setSource replaces output while retaining input resource");
  check(writes==0&&same_source,"all8canonical callables leave readonly source and host proxies unwritten");
  if(!failures)std::cout<<"Focused native canonical callable proxy proof PASS8/8; exact getter sequences and zero readonly writes.\n";
  return failures?1:0;
}
