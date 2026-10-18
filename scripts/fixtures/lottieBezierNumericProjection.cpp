// Consumer of the complete emitted original module. No replacement Bezier implementation.
#include <flight/scene2d_formats/lottie_bezier_path.hpp>
#include <flight/animation/animation_track.hpp>
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <type_traits>
#include <vector>
#include "bezier_numeric_expected.hpp"

namespace {
using flight::Any;
namespace api = flight::scene2d_formats;
int checks = 0, failures = 0, partial = 0, sampler_gaps = 0;
void check(bool good) { ++checks; if (!good) { ++failures; std::cerr << "FAIL " << checks << '\n'; } }
void numbers(const flight::Array<double>& actual, const std::vector<double>& expected) {
  check(actual.size() == expected.size());
  if (actual.size() != expected.size()) return;
  for (std::size_t index=0; index<expected.size(); ++index) {
    const double value=actual.element(static_cast<double>(index)), want=expected[index];
    check(std::isnan(want) ? std::isnan(value) : value == want && (want != 0 || std::signbit(want) == std::signbit(value)));
  }
}
void rows(const flight::Array<flight::Array<double>>& actual, const std::vector<std::vector<double>>& expected) {
  check(actual.size() == expected.size());
  for(std::size_t index=0;index<actual.size() && index<expected.size();++index) numbers(actual.element(static_cast<double>(index)),expected[index]);
}
template<typename Error, typename F> void throws(F run) {
  bool caught=false;
  try { run(); } catch(const Error&) { caught=true; }
  check(caught);
}
struct Accessors : flight::ReferenceEnabled {
  std::function<Any()> read_v, read_i, read_o, read_c;
};
const bool prototype=flight::register_class_prototype<Accessors>(flight::ClassPrototype{{},{"v","i","o","c"},std::nullopt,false});
const bool fields=flight::register_class_fields<Accessors>({});
const bool readers=flight::register_class_property_readers<Accessors>({
 {"v",[](const Any& raw)->Any{return raw.object_reference_if<Accessors>()->read_v();}},
 {"i",[](const Any& raw)->Any{return raw.object_reference_if<Accessors>()->read_i();}},
 {"o",[](const Any& raw)->Any{return raw.object_reference_if<Accessors>()->read_o();}},
 {"c",[](const Any& raw)->Any{return raw.object_reference_if<Accessors>()->read_c();}},
});
struct Descriptor : flight::ReferenceEnabled {};
auto descriptor(){return flight::make_ref<Descriptor>();}
auto defaults(){
 const auto owner=flight::make_ref<Accessors>();
 owner->read_v=[] {return Any(flight::Array<Any>{});};
 owner->read_i=[] {return Any(flight::Array<Any>{});};
 owner->read_o=[] {return Any(flight::Array<Any>{});};
 owner->read_c=[] {return Any(false);};
 return owner;
}
}

int main() {
 static_assert(std::is_same_v<decltype(flight::types::Path::data),flight::Array<double>>);
 static_assert(std::is_same_v<decltype(flight::types::AnimationTrack::values),flight::SequenceView<double>>);
 check(prototype && fields && readers);
 for(const auto& source:numeric_cases) {
   const Any original(flight::Json::parse(flight::String(source.input)));
   const auto selected=api::to_lottie_shape_path(original);
   check(selected.has_value());
   check(flight::json_row_raw(*selected)==original);
   const auto flat=api::flatten_lottie_shape_path(*selected);
   numbers(flat,source.flat);
   if(source.path) {
     const auto path=api::create_lottie_bezier_path(*selected);
     numbers(path->data,source.data);numbers(path->commands,source.commands);
   }
   const auto restored=api::unflatten_lottie_shape_path(*selected,flat);
   rows(restored->v,source.v);rows(restored->i,source.i);rows(restored->o,source.o);
   check(restored->c==source.closed);
   if(!source.flat.empty()) {
     // Use the existing public native numeric consumer; this is not a replacement createLottieTrack.
     using Options=flight::animation::times_values_components_interpolation_quaternion_easing_segment_easings_9b0eadfed5e06042;
     const auto options=flight::make_ref<Options>();
     options->times=flight::SequenceView<double>(flight::Array<double>{0.0});
     options->values=flight::SequenceView<double>(flat);
     options->components=static_cast<double>(flat.size());
     const auto track=flight::animation::create_animation_track(options);
     const flight::Array<double> sampled;
     // Retain the observed independent empty-output consumer gap, without calling it source parity.
     try { flight::animation::sample_animation_track(sampled,track,0.0); }
     catch(const std::range_error&) { ++sampler_gaps; }
     sampled.resize(flat.size());
     flight::animation::sample_animation_track(sampled,track,0.0);
     numbers(sampled,source.flat);
     check(track->values.size()==flat.size());
   }
 }
 {
   int reads=0;const auto frame=defaults();
   frame->read_v=[&] {++reads;if(reads==1)return Any(7.0);return Any(flight::Array<Any>{Any(flight::Array<Any>{Any(reads==2?9.0:3.0),Any(reads==2?2.0:4.0)})});};
   const auto selected=api::to_lottie_shape_path(Any::object(frame));
   check(selected && reads==0 && selected->shared_native_object()==frame);
   numbers(api::create_lottie_bezier_path(*selected)->data,{9,4});
   check(reads==3);
 }
 {
   const Any wrapper(flight::Json::parse("[{\"v\":[[1,2]],\"i\":[],\"o\":[],\"c\":false}]"));
   const auto selected=api::to_lottie_shape_path(wrapper);
   check(selected.has_value());check(flight::json_row_raw(*selected)==*wrapper.array_get(0));
   numbers(api::create_lottie_bezier_path(*selected)->data,{1,2});
 }
 for(const char* input:{"null","7","\"text\"","[]","{}"}) {
   check(!api::to_lottie_shape_path(Any(flight::Json::parse(input))).has_value());
 }
 for(const char* input:{"{\"v\":7,\"i\":[],\"o\":[]}","{\"v\":null,\"i\":[],\"o\":[]}"}) {
   const auto selected=api::to_lottie_shape_path(Any(flight::Json::parse(input)));
   check(selected.has_value());
   throws<flight::TypeError>([&]{(void)api::flatten_lottie_shape_path(*selected);});
 }
 for(const char* coordinate:{"\"x\"","false","{}"}) {
   const flight::String source=flight::String("{\"v\":[[")+flight::String(coordinate)+flight::String(",2]],\"i\":[],\"o\":[],\"c\":false}");
   const auto selected=api::to_lottie_shape_path(Any(flight::Json::parse(source)));
   check(selected.has_value());
   throws<flight::NativeScalarEscapeError>([&]{(void)api::flatten_lottie_shape_path(*selected);});
   throws<flight::NativeScalarEscapeError>([&]{(void)api::create_lottie_bezier_path(*selected);});
   partial+=2; // These six cases explicitly refuse the source's broader numeric-storage domain.
 }
 {
   std::vector<int> order;
   const auto frame=defaults();
   const auto sentinel=std::make_exception_ptr(flight::RangeError("original i getter"));
   frame->read_v=[&]{order.push_back(1);return Any(7.0);};
   frame->read_i=[&]()->Any{order.push_back(2);std::rethrow_exception(sentinel);};
   frame->read_o=[&]{order.push_back(3);return Any(flight::Array<Any>{});};
   const Any original=Any::object(frame);
   const auto selected=api::to_lottie_shape_path(original);
   check(selected.has_value() && order.empty());
   check(selected->shared_native_object()==frame && flight::json_row_raw(*selected)==original);
   bool same=false;try{(void)api::flatten_lottie_shape_path(*selected);}catch(const flight::RangeError&){same=std::current_exception()==sentinel;}
   check(same && order==std::vector<int>{1,2});
 }
 for(bool flatten:{true,false}) {
   const auto point=descriptor();const auto cells=flight::detail::owner_for(point);
   std::vector<int> order;
   const auto sentinel=std::make_exception_ptr(flight::RangeError("later coordinate"));
   cells->set_named_cell("0",std::make_shared<flight::detail::ReadonlyNamedCell>([&]{order.push_back(0);return Any("x");}));
   cells->set_named_cell("1",std::make_shared<flight::detail::ReadonlyNamedCell>([&]()->Any{order.push_back(1);std::rethrow_exception(sentinel);}));
   const auto frame=defaults();frame->read_v=[&]{return Any(flight::Array<Any>{Any::object(point)});};
   const auto selected=api::to_lottie_shape_path(Any::object(frame));
   bool same=false;try{if(flatten)(void)api::flatten_lottie_shape_path(*selected);else(void)api::create_lottie_bezier_path(*selected);}catch(const flight::RangeError&){same=std::current_exception()==sentinel;}
   catch(const flight::NativeScalarEscapeError& error){std::cerr<<"later-coordinate-before-escape: premature native conversion after "<<order.size()<<" coordinate read(s): "<<error.what()<<'\n';}
   if(!same || order!=std::vector<int>{0,1}) std::cerr<<"later-coordinate-before-escape failed, flatten="<<flatten<<"\n";
   check(same && order==std::vector<int>{0,1});
 }
 {
   const auto point=descriptor();const auto cells=flight::detail::owner_for(point);
   const flight::Array<Any> points{Any::object(point)};
   cells->set_named_cell("0",std::make_shared<flight::detail::ReadonlyNamedCell>([&]{points.push(Any(flight::Array<Any>{Any(3.0),Any(4.0)}));return Any(1.0);}));
   cells->set_named_value("1",Any(2.0));
   const auto frame=defaults();frame->read_v=[&]{return Any(points);};
   numbers(api::flatten_lottie_shape_path(*api::to_lottie_shape_path(Any::object(frame))),{1,2,3,4});
 }
 {
   const auto selected=api::to_lottie_shape_path(Any(flight::Json::parse("{\"v\":\"🙂\",\"i\":[],\"o\":[]}")));
   check(selected.has_value());
   throws<flight::NativeScalarEscapeError>([&]{(void)api::flatten_lottie_shape_path(*selected);});++partial;
 }
 std::cout<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"partialNumericEscapes\":"<<partial<<",\"independentEmptySamplerOutputGaps\":"<<sampler_gaps<<"}\n";
 return failures?1:0;
}
