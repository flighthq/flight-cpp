// Preparation fixture for the ORIGINAL whole-source image/Sprite closure.
// Include the frozen emitted image + original factory headers before repo overrides.
// The installed default-only Sprite override is not an admissible replacement.
#include <flight/scene2d_formats/lottie_image_layer.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/node/native_node2d_access.hpp>
#include <flight/types/lottie_registry.hpp>
#include <flight/types/image_resource.hpp>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool value, const char* label) {
  if (!value) throw std::runtime_error(label);
  ++checks;
}
using ImageAssetSource = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::LottieImageAsset>>>>;
auto children(flight::types::Node2D node) {
  return flight::row_get<flight::RowKey<"children">>(
      flight::node::native_node_2d_runtime_view(node));
}
} // namespace

int main() {
  try {
    // These are genuine source factory calls. Do not replace a failed call with
    // make_ref<Sprite/NodeRuntime> or a manually copied factory body.
    auto parent = flight::scene2d::create_display_object();
    auto context = flight::make_ref<flight::types::LottieImportContext>();
    context->diagnostics = flight::Array<flight::Ref<flight::types::ImportDiagnostic>>{};
    auto layer = flight::make_ref<flight::types::LottieLayer>();
    layer->ref_id = flight::String("image");
    auto asset = flight::make_ref<flight::types::LottieImageAsset>();
    asset->id = flight::String("image");
    asset->p = flight::String("asset.png");
    context->assets.set(flight::String("image"), flight::types::LottieAsset{asset});
    auto input = flight::make_ref<flight::types::LottieLayerContext>();
    input->container = parent;
    input->import = context;
    input->layer = layer;
    const auto handler = &flight::scene2d_formats::lottie_image_layer_handler;

    // No resolver: the original image handler reports a skip without constructing
    // a Sprite. Calling the same public entry also compiles the resolved branch.
    handler(input);
    check(context->diagnostics->size() == 1, "missing resolver reports exactly once");
    check(context->diagnostics->element(0)->kind == flight::String("lottie.unresolved-image"),
          "missing resolver diagnostic kind");
    check(!children(parent) || children(parent)->size() == 0, "unresolved image adds no child");

    auto resource = flight::make_ref<flight::types::ImageResource>();
    resource->width = 20;
    resource->height = 10;
    int resolver_calls = 0;
    context->resolve_image_resource = [&, asset, resource](ImageAssetSource value)
        -> std::optional<flight::Ref<flight::types::ImageResource>> {
      ++resolver_calls;
      check(value.shared_native_object().get() == asset.get(), "resolver receives original asset");
      return resource;
    };
    handler(input);
    check(resolver_calls == 1, "resolved public call invokes resolver once");
    const auto first_children = children(parent);
    check(first_children && first_children->size() == 1, "resolved image appends one child");
    check(context->diagnostics->size() == 1, "successful call adds no diagnostic");

    const auto first = first_children->element(0);
    const auto first_native = first.shared_exact_object<flight::types::Sprite>();
    check(bool(first_native), "public image result has the exact required native Sprite owner");
    check(first_native->data && first_native->data->texture, "authored data.texture survives factory");
    const auto texture = std::get<flight::Ref<flight::types::Texture2D>>(*first_native->data->texture);
    check(std::get<flight::Ref<flight::types::ImageResource>>(*texture->source) == resource,
          "Texture2D retains exact ImageResource owner");
    handler(input);
    const auto second_children = children(parent);
    check(resolver_calls == 2 && second_children->size() == 2, "repeat public call appends once");
    check(first.identity() != second_children->element(1).identity(), "repeat factory allocates fresh child");

    const auto error = flight::make_ref<flight::types::ImageResource>();
    context->resolve_image_resource = [error](ImageAssetSource)
        -> std::optional<flight::Ref<flight::types::ImageResource>> { throw error; };
    bool original_error = false;
    try { handler(input); }
    catch (const flight::Ref<flight::types::ImageResource>& caught) { original_error = caught == error; }
    check(original_error, "resolver exception preserves original thrown owner");
    check(children(parent)->size() == 2 && context->diagnostics->size() == 1,
          "resolver exception precedes child allocation and reporting");
    std::cout << "PASS " << checks << " original public image consumer controls\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
