#include <flight/scene2d_formats/lottie_bezier_path.hpp>
#include <flight/scene2d_formats/lottie_document.hpp>
#include "bezier_public_numeric_expected.hpp"
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
namespace api=flight::scene2d_formats;
using flight::Any;
using Key=flight::types::LottieKeyframe<Any>;
using KeyRow=flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Key>>>>;
using TrackRow=flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::AnimationTrack>>>>;
static_assert(std::is_same_v<decltype(std::declval<flight::types::Path>().data),flight::Array<double>>);
static_assert(std::is_same_v<decltype(std::declval<flight::types::AnimationTrack>().values),flight::SequenceView<double>>);
namespace {
int checks=0,failures=0,samples=0,partial=0;
const char* current="setup";
void check(bool good,const char* label){++checks;if(!good){++failures;std::cerr<<current<<": "<<label<<'\n';}}
Any member(const Any& value,const char* key){return flight::read_json_scalar_property(value,key).raw();}
double number(const Any& value){return flight::JsonScalarCell(value).as_native_number();}
std::vector<Any> elements(const Any& value){std::vector<Any> out;auto it=flight::json_scalar_values(value);while(true){auto step=it.next();if(step.done)break;out.push_back(*step.value);}return out;}
flight::Array<double> numeric_array(const Any& value){flight::Array<double> out;for(const auto& x:elements(value))out.push(number(x));return out;}
template<class Values>void numbers(const Values& actual,const std::vector<double>& expected,bool exact=false){check(actual.size()==expected.size(),"numeric length");for(std::size_t i=0;i<std::min(actual.size(),expected.size());++i){const double x=actual[i],y=expected[i];check(y==0?x==0&&std::signbit(x)==std::signbit(y):exact?x==y:std::abs(x-y)<=1e-9*std::max(1.0,std::abs(y)),"numeric value");}}
auto context(double offset=0,double scale=1,double ip=0,double fr=1){const auto doc=flight::make_ref<flight::types::LottieDocument>();doc->ip=ip;doc->fr=fr;const auto out=flight::make_ref<flight::types::LottieImportContext>();out->document=decltype(out->document)(doc);out->frame_offset=offset;out->frame_scale=scale;return out;}
std::variant<flight::Array<double>,double> handle_component(const Any& value){if(value.is_array())return numeric_array(value);return number(value);}
auto handle(const Any& raw){const auto out=flight::make_ref<flight::types::LottieBezierHandle>();out->x=handle_component(member(raw,"x"));out->y=handle_component(member(raw,"y"));return out;}
// Fixture construction mirrors the source test's typed keyframe literals. Shape values stay raw on
// their original Any/JSON owners; no source function body or owner projection is replaced here.
flight::Array<KeyRow> fixture_keys(const Any& raw){flight::Array<KeyRow> out;for(const auto& input:elements(raw)){const auto key=flight::make_ref<Key>();key->t=number(member(input,"t"));
 for(const char* field:{"s","e"}){const auto value=member(input,field);if(!value.is_undefined()){if(field[0]=='s')key->s=value;else key->e=value;}}
 const auto hold=member(input,"h");if(!hold.is_undefined())key->h=number(hold);
 const auto incoming=member(input,"i"),outgoing=member(input,"o");if(!incoming.is_undefined())key->i=handle(incoming);if(!outgoing.is_undefined())key->o=handle(outgoing);
 const auto to=member(input,"to"),ti=member(input,"ti");if(!to.is_undefined())key->to=numeric_array(to);if(!ti.is_undefined())key->ti=numeric_array(ti);
 out.push(KeyRow(key));}return out;}
auto flatten_callback(std::vector<int>* order=nullptr){return std::function<flight::Array<double>(std::optional<Any>,double)>{[order](std::optional<Any> value,double index){if(order)order->push_back(static_cast<int>(index));const auto raw=value.value_or(Any(flight::undefined));const auto selected=api::to_lottie_shape_path(raw);check(selected.has_value()&&flight::json_row_raw(*selected)==raw,"original shape owner selected");return api::flatten_lottie_shape_path(selected.value());}};}
auto track(const flight::Array<KeyRow>& keys,double components,std::vector<int>* order=nullptr){return api::create_lottie_track<Any>(keys,components,context(),flatten_callback(order),0);}
auto one_key(const Any& value){const auto key=flight::make_ref<Key>();key->t=0;key->s=value;return flight::Array<KeyRow>{KeyRow(key)};}
struct ShapeOwner:flight::ReferenceEnabled {std::function<Any()> vertices;Any get_v(){return vertices();}Any get_i(){return Any(flight::Array<Any>{});}Any get_o(){return Any(flight::Array<Any>{});}};
struct PointOwner:flight::ReferenceEnabled {};
const bool prototype=flight::register_class_prototype<ShapeOwner>(flight::ClassPrototype{{},{"v","i","o"},std::nullopt,false});
const bool fields=flight::register_class_fields<ShapeOwner>({});
auto shape_owner(const Any& value){const auto owner=value.object_reference_if<ShapeOwner>();if(!owner)throw flight::OutOfProfileError("fixture getter requires its exact owner");return owner;}
const bool getters=flight::register_class_property_readers<ShapeOwner>({
 {"v",[](const Any& value){return shape_owner(value)->get_v();}},
 {"i",[](const Any& value){return shape_owner(value)->get_i();}},
 {"o",[](const Any& value){return shape_owner(value)->get_o();}},
});
}
int main(){
 check(prototype&&fields&&getters,"getter metadata");
 for(const auto& source:public_expected::cases){current=source.name;try{
  const Any raw(flight::Json::parse(flight::String(source.keys)));const auto keys=fixture_keys(raw);std::vector<int> callbacks;
  const auto actual=api::create_lottie_track<Any>(keys,source.components,context(source.offset,source.scale,source.ip,source.fr),flatten_callback(&callbacks),0,source.spatial);
  check(callbacks==source.callbacks,"callback source indices");numbers(actual->times,source.times);numbers(actual->values,source.values);
  check(actual->interpolation==flight::String(source.interpolation),"interpolation");check(actual->segment_easings.has_value(),"segment easing array exists");
  check(actual->segment_easings->size()==source.easings.size(),"segment easing length");for(std::size_t i=0;i<source.easings.size();++i)check(actual->segment_easings->element(static_cast<double>(i)).has_value()==source.easings[i],"segment easing presence");
  for(const auto& expected:source.paths){const auto key=member(raw,std::to_string(expected.key).c_str());const auto selected=api::to_lottie_shape_path(member(key,expected.field));const auto path=api::create_lottie_bezier_path(selected.value());numbers(path->data,expected.data);numbers(path->commands,expected.commands);}
  for(const auto& expected:source.samples){++samples;if(expected.typed){const flight::Float32Array output(static_cast<double>(expected.input.size()));const auto owner=output.identity();for(std::size_t i=0;i<expected.input.size();++i)output.set_index(static_cast<double>(i),expected.input[i]);flight::animation::sample_animation_track(output,TrackRow(actual),expected.time);numbers(output,expected.output,true);check(output.identity()==owner,"typed output owner");}
   else {const flight::Array<double> output;for(double value:expected.input)output.push(value);const auto owner=output.identity();flight::animation::sample_animation_track(output,TrackRow(actual),expected.time);numbers(output,expected.output);check(output.identity()==owner,"output owner");}}
  const auto alias=actual->values.array_owner_if();check(alias.has_value()&&alias->identity()==actual->values.identity(),"exact original track values owner");
  flight::animation::sample_animation_track(alias.value(),TrackRow(actual),0.5);numbers(alias.value(),source.alias);numbers(actual->values,source.alias);check(alias->identity()==actual->values.identity(),"original track store/output alias");
 }catch(const std::exception& error){check(false,error.what());}}
 current="track-callback-later-getter-before-escape";
 {const auto point=flight::make_ref<PointOwner>();const auto owner=flight::detail::owner_for(point);std::vector<int> reads,callbacks;const auto sentinel=std::make_exception_ptr(flight::RangeError("original later coordinate"));
  owner->set_named_cell("0",std::make_shared<flight::detail::ReadonlyNamedCell>([&](){reads.push_back(0);return Any("x");}));owner->set_named_cell("1",std::make_shared<flight::detail::ReadonlyNamedCell>([&]()->Any{reads.push_back(1);std::rethrow_exception(sentinel);}));
  const auto shape=flight::make_ref<ShapeOwner>();shape->vertices=[point](){return Any(flight::Array<Any>{Any::object(point)});};bool same=false;try{track(one_key(Any::object(shape)),2,&callbacks);}catch(const flight::RangeError&){same=std::current_exception()==sentinel;}catch(const std::exception& error){check(false,error.what());}
  check(same&&reads==std::vector<int>{0,1}&&callbacks==std::vector<int>{0},"original later getter error before numeric escape");}
 current="flat-track-store-is-source-snapshot";
 {const flight::Array<double> first{1,2},second{3,4};const auto keys=fixture_keys(Any(flight::Json::parse("[{\"t\":0},{\"t\":1}]")));const auto actual=api::create_lottie_track<Any>(keys,2,context(),std::function<flight::Array<double>(std::optional<Any>,double)>{[&](std::optional<Any>,double index){return index==0?first:second;}},0);first[0]=99;numbers(actual->values,{1,2,3,4});const auto stored=actual->values.array_owner_if();check(stored&&stored->identity()!=first.identity()&&stored->identity()!=second.identity(),"source flat allocates its own store");}
 current="live-points-through-track-callback";
 {const auto point=flight::make_ref<PointOwner>();const auto owner=flight::detail::owner_for(point);const flight::Array<Any> vertices{Any::object(point)};std::vector<int> reads;
  owner->set_named_cell("0",std::make_shared<flight::detail::ReadonlyNamedCell>([&](){reads.push_back(0);vertices.push(Any(flight::Array<Any>{Any(3.0),Any(4.0)}));return Any(1.0);}));owner->set_named_cell("1",std::make_shared<flight::detail::ReadonlyNamedCell>([&](){reads.push_back(1);return Any(2.0);}));
  const auto shape=flight::make_ref<ShapeOwner>();shape->vertices=[vertices](){return Any(vertices);};const auto actual=track(one_key(Any::object(shape)),4);numbers(actual->values,{1,2,3,4});const flight::Array<double> output;flight::animation::sample_animation_track(output,TrackRow(actual),0);numbers(output,{1,2,3,4});check(reads==std::vector<int>{0,1},"live point iterator reads");}
 current="original-sampler-later-read-preserves-prefix";
 {const auto actual=track(fixture_keys(Any(flight::Json::parse("[{\"t\":0,\"s\":{\"v\":[[1,2]],\"i\":[],\"o\":[]}}]"))),2);std::vector<int> reads;const auto sentinel=std::make_exception_ptr(flight::RangeError("later stored value"));actual->values=flight::SequenceView<double>::from_erased_array(Any(flight::Array<Any>{Any(1.0),Any(2.0)}),[&](const Any& raw){const double value=raw.as_number();reads.push_back(value==1?0:1);if(value==2)std::rethrow_exception(sentinel);return value;});const flight::Array<double> output;bool same=false;
  try{flight::animation::sample_animation_track(output,TrackRow(actual),0);}catch(const flight::RangeError&){same=std::current_exception()==sentinel;}catch(const std::exception& error){check(false,error.what());}
  check(same&&reads==std::vector<int>{0,1},"original sampler read error identity/order");numbers(output,{1});}
 current="original-owner-getter-read-on-each-track-callback";
 {const auto shape=flight::make_ref<ShapeOwner>();int reads=0;shape->vertices=[&](){++reads;return Any(flight::Array<Any>{Any(flight::Array<Any>{Any(reads==1?1.0:3.0),Any(reads==1?2.0:4.0)})});};auto keys=one_key(Any::object(shape));const auto second=flight::make_ref<Key>();second->t=1;second->s=Any::object(shape);keys.push(KeyRow(second));const auto actual=track(keys,2);check(reads==2,"original owner getter read count");numbers(actual->values,{1,2,3,4});const flight::Array<double> output;flight::animation::sample_animation_track(output,TrackRow(actual),0.5);numbers(output,{2,3});}
 current="explicit-partial-nonnumeric-storage";
 for(const char* coordinate:{"\"x\"","false","{}"}){const auto raw=Any(flight::Json::parse(flight::String("{\"v\":[[")+flight::String(coordinate)+flight::String(",2]],\"i\":[],\"o\":[]}")));bool path_refused=false,track_refused=false;try{api::create_lottie_bezier_path(api::to_lottie_shape_path(raw).value());}catch(const flight::NativeScalarEscapeError&){path_refused=true;}try{track(one_key(raw),2);}catch(const flight::NativeScalarEscapeError&){track_refused=true;}check(path_refused&&track_refused,"shared explicit native escape family");partial+=2;}
 {bool refused=false;try{track(one_key(Any(flight::Json::parse("{\"v\":\"🙂\",\"i\":[],\"o\":[]}"))),2);}catch(const flight::NativeScalarEscapeError&){refused=true;}check(refused,"string iteration reaches numeric escape");++partial;}
 check(samples==624,"all source sample comparisons executed");check(partial==7,"seven policy escape rows remain PARTIAL");
 std::cout<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"sampleComparisons\":"<<samples<<",\"partialNumericEscapes\":"<<partial<<",\"behaviorCases\":5}\n";return failures?1:0;
}
