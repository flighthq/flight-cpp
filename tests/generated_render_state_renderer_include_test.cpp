// This translation unit intentionally enters the generated types include graph through NodeRenderer, the
// a627849 replacement for the removed types::Renderer. The executable's main translation unit enters
// through flight/render/render_state.hpp, so both include orders are compiled on every build.
#include <flight/types/node_renderer.hpp>
