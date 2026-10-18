// Pending qualification against the coordinated normal GL/types floor.
// Uses original GL type owner helpers, the real base runtime initializer, EntityConstruction,
// and original public registry diagnostics. No GPU draw/frame or GL state factory claim.
#include <flight/render/render_registry_guards.hpp>
#include <flight/render/render_state.hpp>
#include <flight/types/gl_render_state.hpp>
#include <flight/nominal_or_structural_view.hpp>
#include <memory>
#include <optional>
#include <cassert>
#include <string>
#include <type_traits>
#include <vector>

int main() {
  namespace r = flight::render;
  namespace t = flight::types;
  using Table = t::RenderRegistryTable;
  using Emit = std::function<void(Table, flight::String)>;
  using Data = flight::Record<flight::String, flight::Any>;
  static_assert(std::is_base_of_v<t::RenderState, t::GlRenderState>);
  flight::log::clear_log_sinks();
  flight::log::clear_log_once_keys();
  flight::log::set_log_console_level(t::LogLevel::None);
  std::vector<flight::String> messages;
  flight::types::LogSink sink = [&](flight::Ref<t::LogEntry> entry) {
    assert(entry->level == t::LogLevel::Warn);
    assert(entry->channel.value() == flight::String("render"));
    auto data = entry->data;
    messages.push_back(std::get<Data>(data).get(flight::String("message")).value().as_string());
  };
  flight::log::add_log_sink(sink);
  static_assert(std::is_base_of_v<t::RenderStateRuntime, t::GlRenderStateRuntime>);
  static_assert(std::is_base_of_v<t::RenderRegistries, t::GlRenderRegistries>);
  using State = flight::Ref<t::GlRenderState>;
  using RuntimeView = flight::NominalOrStructuralView<t::GlRenderStateRuntime, t::RenderStateRuntime>;
  using RegistryView = flight::NominalOrStructuralView<t::GlRenderRegistries, t::RenderRegistries>;
  auto construction = r::create_derived_render_state_construction<State>(std::nullopt);
  auto registries = flight::make_ref<t::GlRenderRegistries>();
  auto originalRuntime = flight::make_ref<t::GlRenderStateRuntime>();
  originalRuntime->current_frame_id = 17;
  originalRuntime->renderer_map_id = 23;
  originalRuntime->temp_stack.push(t::NodeAny{});
  auto runtime = r::initialize_render_state_runtime(originalRuntime, registries);
  assert(runtime == originalRuntime && runtime->current_frame_id == 0 &&
      runtime->renderer_map_id == 0 && runtime->temp_stack.empty() &&
      !runtime->registry_miss && runtime->registries == registries);
  t::write_gl_render_state_runtime_owner(construction, runtime);
  auto gl = flight::entity::finish_entity<State>(construction);
  flight::Ref<t::RenderState> first = gl;
  auto second = r::create_render_state();
  assert(t::read_gl_render_state_runtime_owner(gl) == runtime);
  assert(t::read_render_state_runtime_owner(first) == runtime);
  assert(&first->entity_runtime_key == &gl->entity_runtime_key);
  assert(!first.owner_before(gl) && !gl.owner_before(first));
  auto recovered = flight::recover_owner<t::GlRenderState>(first);
  assert(recovered == gl && !recovered.owner_before(gl) && !gl.owner_before(recovered));
  flight::Ref<t::RenderStateRuntime> baseRuntime = runtime;
  assert(&runtime->registries == &baseRuntime->registries);
  auto replacement = flight::make_ref<t::GlRenderRegistries>();
  baseRuntime->registries = replacement;
  assert(RegistryView::read_cell(runtime->registries).native() == replacement);
  RegistryView::write_cell(runtime->registries, RegistryView(registries));
  assert(baseRuntime->registries == registries);
  auto wrongRegistries = flight::make_ref<t::RenderRegistries>();
  baseRuntime->registries = wrongRegistries;
  auto lazyRegistries = RegistryView::read_cell(runtime->registries);
  assert(lazyRegistries.object() == wrongRegistries);
  assert(!(lazyRegistries.read<flight::RowKey<"customEffectShaders">,
      flight::Map<flight::String, flight::String>, &t::GlRenderRegistries::custom_effect_shaders>()).has_value());
  bool dedicated = false;
  try { (void)lazyRegistries.native(); }
  catch (const flight::NativeScalarEscapeError&) { dedicated = true; }
  assert(dedicated);
  RegistryView::write_cell(runtime->registries, RegistryView(registries));
  auto otherRuntime = flight::make_ref<t::GlRenderStateRuntime>();
  first->entity_runtime_key = otherRuntime;
  assert(t::read_gl_render_state_runtime_owner(gl) == otherRuntime);
  t::write_gl_render_state_runtime_owner(construction, runtime);
  assert(first->entity_runtime_key == runtime);
  auto wrongRuntime = flight::make_ref<t::RenderStateRuntime>();
  first->entity_runtime_key = wrongRuntime;
  auto lazyRuntime = RuntimeView::read_cell(gl->entity_runtime_key);
  assert(lazyRuntime && lazyRuntime->object() == wrongRuntime);
  assert(!(lazyRuntime->read<flight::RowKey<"modifierSnippetRevision">, double,
      &t::GlRenderStateRuntime::modifier_snippet_revision>()).has_value());
  dedicated = false;
  try { (void)t::read_gl_render_state_runtime_owner(gl); }
  catch (const flight::NativeScalarEscapeError&) { dedicated = true; }
  assert(dedicated);
  RuntimeView::write_symbol_cell<&t::GlRenderState::entity_runtime_key>(
      gl, t::entity_runtime_key, std::optional<RuntimeView>{});
  flight::StructuralRef<flight::RowOf<flight::Ref<t::RenderState>>> row(first);
  assert(!first->entity_runtime_key && flight::row_has_own_property(row, t::entity_runtime_key));
  assert(!flight::row_has_own_property(row, flight::Symbol(flight::String("EntityRuntime"))));
  assert(row.shared_owner()->symbol_cell(t::entity_runtime_key)->storage_address() ==
      &first->entity_runtime_key);
  assert(!row.shared_owner()->has_dynamic_value(t::entity_runtime_key));
  t::write_gl_render_state_runtime_owner(construction, runtime);
  assert(!r::are_render_registries_guards_enabled(first));
  assert(r::explain_render_registries_misses(first)->status == flight::String("complete"));
  r::enable_render_registries_guards(first);
  r::enable_render_registries_guards(first);
  r::enable_render_registries_guards(second);
  assert(r::are_render_registries_guards_enabled(first));
  auto firstSignals = r::enable_render_registry_signals(first);
  assert(firstSignals == r::enable_render_registry_signals(first));
  const flight::String kind("native-λ");
  flight::signals::emit_signal<Emit>(firstSignals->on_registry_miss, Table::MaterialRenderer, kind);
  flight::signals::emit_signal<Emit>(firstSignals->on_registry_miss, Table::MaterialRenderer, kind);
  flight::signals::emit_signal<Emit>(firstSignals->on_registry_miss, Table::TextureResolver, kind);
  flight::signals::emit_signal<Emit>(firstSignals->on_registry_miss, Table::MaterialRenderer, flight::String("other"));
  flight::signals::emit_signal<Emit>(r::enable_render_registry_signals(second)->on_registry_miss, Table::MaterialRenderer, kind);
  assert(messages.size() == 4);
  assert(messages[0].to_utf8().find("resolveGlQuadMaterialRenderer") != std::string::npos);
  assert(messages[1].to_utf8().find("resolveGlTexture") != std::string::npos);
  auto explanation = r::explain_render_registries_misses(first);
  assert(explanation->status == flight::String("misses-recorded"));
  assert(explanation->misses.size() == 3);
  assert(flight::row_get<flight::RowKey<"registry">>(explanation->misses[0]) == Table::MaterialRenderer);
  assert(flight::row_get<flight::RowKey<"registry">>(explanation->misses[1]) == Table::TextureResolver);
  assert(flight::row_get<flight::RowKey<"kind">>(explanation->misses[2]) == flight::String("other"));
  assert(r::explain_render_registries_misses(second)->misses.size() == 1);
  // Exercise the installed public callback, retaining the prior explanation snapshot.
  auto firstEmitter = runtime->registry_miss.value();
  assert(firstEmitter->signals == firstSignals);
  firstEmitter->callable(Table::MaterialRenderer, flight::String("via-runtime"));
  assert(messages.size() == 5 && explanation->misses.size() == 3);
  auto afterCallback = r::explain_render_registries_misses(first);
  assert(afterCallback->misses.size() == 4);
  assert(explanation->misses.identity() != afterCallback->misses.identity());
  assert(flight::row_get<flight::RowKey<"kind">>(afterCallback->misses[3]) ==
      flight::String("via-runtime"));
  assert(r::explain_render_registries_misses(second)->misses.size() == 1);
  runtime->temp_stack.push(t::NodeAny{});
  r::destroy_render_state(first);
  assert(!runtime->registry_miss && runtime->temp_stack.empty());
  // A retained emitter must no longer reach the guard listener after public teardown.
  firstEmitter->callable(Table::MaterialRenderer, flight::String("after-destroy"));
  assert(messages.size() == 5);
  assert(r::explain_render_registries_misses(first)->misses.size() == 4);
  assert(explanation->misses.size() == 3);
  assert(t::read_gl_render_state_runtime_owner(gl) == runtime);
  r::enable_render_registry_signals(second);
  auto secondRuntime = r::get_render_state_runtime(second);
  secondRuntime->registry_miss.value()->callable(Table::MaterialRenderer,
      flight::String("second-still-live"));
  assert(messages.size() == 6);
  assert(r::explain_render_registries_misses(second)->misses.size() == 2);
  flight::log::clear_log_sinks();
  flight::log::clear_log_once_keys();
}
