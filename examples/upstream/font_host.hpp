#pragma once
#include "scene2d_host.hpp"
#include <flight/types/text_label.hpp>
#include <span>
namespace flight::examples {
void install_text_renderer(flight::Ref<flight::types::GlRenderState> state);
void verify_text_labels(const Scene2DRenderFrame& frame,
    std::span<const flight::Ref<flight::types::TextLabel>> labels);
}
