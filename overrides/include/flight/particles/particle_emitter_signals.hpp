// Carried from the generated particleEmitterSignals module at a627849 with one body corrected:
// getParticleEmitterSignals. Everything else is the emission, unchanged. See overrides/manifest.json
// (particles-signals-missing-slot-reads-null).
#pragma once
#include <flight/erased_ref.hpp>
#include <flight/record.hpp>
#include <flight/structural_ref.hpp>
#include <flight/symbol.hpp>
#include <functional>
#include <optional>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::types { struct Entity; }
namespace flight::types { struct ParticleEmitterSignals; }
namespace flight::types { template <typename T> struct Signal; }

#include <flight/entity/entity.hpp>
#include <flight/signals/signal.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/particle_emitter_signals.hpp>
#include <flight/types/signal.hpp>

namespace flight::particles {

inline void initialize_particle_emitter_signals(flight::types::EntityConstruction<flight::Ref<flight::types::ParticleEmitterSignals>> out) {
  flight::row_set<flight::RowKey<"onEmitterComplete">>(out, flight::signals::create_signal<std::function<void()>>());
  flight::row_set<flight::RowKey<"onParticleDeath">>(out, flight::signals::create_signal<std::function<void(double, double, double)>>());
  flight::row_set<flight::RowKey<"onParticleSpawn">>(out, flight::signals::create_signal<std::function<void(double, double, double, double, double, double)>>());
}

inline flight::Ref<flight::types::ParticleEmitterSignals> create_particle_emitter_signals() {
  flight::types::EntityConstruction<flight::Ref<flight::types::ParticleEmitterSignals>> out = flight::entity::allocate_entity<flight::Ref<flight::types::ParticleEmitterSignals>>();
  initialize_particle_emitter_signals(out);
  return flight::entity::finish_entity<flight::Ref<flight::types::ParticleEmitterSignals>>(out);
}

inline const flight::Symbol signals_slot = flight::Symbol(flight::String("particleEmitterSignals"));

inline flight::Ref<flight::types::ParticleEmitterSignals> enable_particle_emitter_signals(flight::Ref<void> state) {
  flight::Record<flight::Symbol, std::optional<flight::Ref<flight::types::ParticleEmitterSignals>>> s = static_cast<flight::Record<flight::Symbol, std::optional<flight::Ref<flight::types::ParticleEmitterSignals>>>>(state);
  return ([&]() { auto&& assignment_receiver = s; const auto assignment_key = signals_slot; auto assignment_existing = assignment_receiver.get(assignment_key); if (assignment_existing.has_value() && assignment_existing.value().has_value()) return assignment_existing.value().value(); auto assignment_value = std::optional<flight::Ref<flight::types::ParticleEmitterSignals>>{create_particle_emitter_signals()}; assignment_receiver.set(assignment_key, assignment_value); return assignment_value.value(); }());
}

// OVERRIDE: getParticleEmitterSignals. The emission lowers `s[signalsSlot] ?? null` as `.get(slot).value()`,
// which throws std::bad_optional_access for a state whose signals were never enabled
// (docs/compiler-requests/record-missing-key-nullish-coalesce.md). The source reads the slot and answers null
// both when it was never written and when it holds undefined. Here both absences are tested, as the emitted
// enableParticleEmitterSignals already tests them for its `??=`.
inline std::optional<flight::Ref<flight::types::ParticleEmitterSignals>> get_particle_emitter_signals(flight::Ref<void> state) {
  const auto slot_value = static_cast<flight::Record<flight::Symbol, std::optional<flight::Ref<flight::types::ParticleEmitterSignals>>>>(state).get(signals_slot);
  if (slot_value.has_value() && slot_value.value().has_value()) {
    return slot_value.value();
  }
  return std::nullopt;
}

} // namespace flight::particles
