// Frozen installed-ABI counterexample only; intentionally asserts the missing readonly call edge.
// No public function declaration or body is replaced. Revisit after actual normal generation.
#include <flight/particles_formats/parse_particle_config.hpp>
#include <flight/structural_ref.hpp>
#include <optional>
#include <type_traits>
using Read = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ParseParticleConfigOptions>>>>;
static_assert(!std::is_invocable_v<decltype(&flight::particles_formats::parse_particle_config), flight::String, std::optional<Read>>);
static_assert(!std::is_invocable_v<decltype(&flight::particles_formats::parse_particle_config_document), flight::String, std::optional<Read>>);
static_assert(std::is_invocable_v<decltype(&flight::particles_formats::parse_registered_particle_format), flight::String, flight::String, std::optional<Read>>);
int main() {}
