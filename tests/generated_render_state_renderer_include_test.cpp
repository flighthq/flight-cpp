// This translation unit intentionally enters the generated include cycle through Renderer. The
// executable's main translation unit enters through RenderProxy, so both formerly failing orders are
// compiled on every build.
#include <flight/types/renderer.hpp>
