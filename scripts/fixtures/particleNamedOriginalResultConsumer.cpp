// Two actual original parsers through their actual public slot; no replacement parser or declaration.
#include "original-result-consumer.hpp"
#include <iostream>
#include <type_traits>
namespace api = flight::particles_formats;
namespace types = flight::types;
using ReadOptions = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::ParseParticleConfigOptions>>>>;
using PixiNative = flight::Ref<types::PixiParseResult> (*)(flight::String);
using SpineNative = flight::Ref<types::SpineParseResult> (*)(flight::String);
static_assert(std::is_same_v<decltype(&api::parse_pixi_particle_document), PixiNative>);
static_assert(std::is_same_v<decltype(&api::parse_spine_particle_document), SpineNative>);
struct OptionsCell final : flight::TypedRowCell<std::optional<double>> {
 std::optional<double> value{2}; int reads=0;
 std::optional<double>& get() override { ++reads; throw std::runtime_error("unused options getter"); }
 void set(std::optional<double> next) override { value=next; }
};
int checks=0;
void check(bool value) { ++checks; if(!value) throw checks; }
template<auto Source, typename Slot>
void original(Slot slot, flight::String input, bool hasDocument) {
 using Full=typename decltype(Source(input))::element_type;
 auto optionCell=std::make_shared<OptionsCell>();
 auto optionOwner=std::make_shared<flight::RowOwner>();
 optionOwner->set_named_cell("textureSize",optionCell);optionOwner->set_named_cell("pixelsPerUnit",optionCell);
 auto options=ReadOptions::from_owner(optionOwner);
 int evaluations=0;auto produce=[&] { ++evaluations; return options; };
 auto view=slot->parse_to_document(input,produce());
 check(evaluations==1&&optionCell->reads==0);
 check(view.shared_owner()->native_type()==typeid(Full)&&view.shared_native_object()!=nullptr);
 check(flight::callable_identity_equal_named<Source>(slot->parse_to_document));
 auto copy=slot->parse_to_document;check(flight::callable_identity_equal(copy,slot->parse_to_document));
 auto native=Source(input);
 check(native->config->max_particles==7);
 check(native->config->max_particles==flight::row_get<flight::RowKey<"config">>(view)->max_particles);
 auto* configSlot=view.shared_owner()->template named_value<flight::Ref<types::ParticleEmitterConfig>>("config");
 check(configSlot&&*configSlot==flight::row_get<flight::RowKey<"config">>(view));
 auto alias=view;auto replacement=flight::make_ref<types::ParticleEmitterConfig>();replacement->max_particles=29;
 flight::row_set<flight::RowKey<"config">>(alias,replacement);
 check(*configSlot==replacement&&flight::row_get<flight::RowKey<"config">>(view)==replacement);
 flight::Array<flight::Ref<types::ImportDiagnostic>> diagnostics;
 flight::row_set<flight::RowKey<"diagnostics">>(view,diagnostics);
 auto* diagnosticSlot=view.shared_owner()->template named_value<flight::Array<flight::Ref<types::ImportDiagnostic>>>("diagnostics");
 check(diagnosticSlot&&diagnosticSlot->identity()==diagnostics.identity());
 diagnosticSlot->push(flight::make_ref<types::ImportDiagnostic>());
 check(flight::row_get<flight::RowKey<"diagnostics">>(alias).size()==1);
 std::weak_ptr<types::SpineParticleDocument> documentLifetime;
 if(hasDocument) {
  auto* doc=view.shared_owner()->template named_value<flight::Ref<types::SpineParticleDocument>>("document");
  check(doc&&*doc&&(*doc)->name=="retained");documentLifetime=*doc;
  (*doc)->name="still original";
  check(view.shared_owner()==alias.shared_owner()&&(*doc)->name=="still original");
 }
 for(const auto& invalid:{flight::String("bad"),flight::String("null"),flight::String("[]")}) {
  flight::String expected;bool directThrew=false,wrappedThrew=false;
  try{(void)Source(invalid);}catch(const std::exception& error){directThrew=true;expected=error.what();}
  try{(void)slot->parse_to_document(invalid,options);}catch(const std::exception& error){wrappedThrew=true;check(flight::String(error.what())==expected);}
  check(directThrew&&wrappedThrew&&optionCell->reads==0);
 }
 std::weak_ptr<void> lifetime=view.shared_native_object();
 native.reset();view={};check(!lifetime.expired());
 if(hasDocument)check(!documentLifetime.expired());
 alias={};check(lifetime.expired());
 if(hasDocument)check(documentLifetime.expired());
}
int main() {
 try {
  check(api::original_named_identity());
  original<&api::parse_pixi_particle_document>(api::named_pixi_document,"{\"maxParticles\":7,\"frequency\":0.25}",false);
  original<&api::parse_spine_particle_document>(api::named_spine_document,"{\"name\":\"retained\",\"maxParticles\":7,\"continuous\":true}",true);
  check(!flight::callable_identity_equal(api::named_pixi_document->parse_to_document,api::named_spine_document->parse_to_document));
  std::cout<<"{\"checks\":"<<checks<<",\"failures\":0,\"originalCallbacks\":2}\n";
 }catch(int failure){std::cerr<<"failed check "<<failure<<'\n';return 1;}
}
