#include <flight/surface/contract.hpp>

#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>

// Native port of every case in packages/surface/src/*.test.ts at Flight a627849. Host capability
// doubles retain the exact row owners and opaque values they receive, making runtime storage,
// dimensions, options, acquisition/release order, copy identity, and all null/rejection paths
// observable. NativeSurfaceHandle is deliberately opaque; the byte-backed fixture below verifies
// exact storage without inventing an image decoder or pretending @flighthq/image's refused browser
// and erased-owner modules exist.
// Mutation check (2026-10-12): dropping the stored handle, swapping resize dimensions, dropping GL
// acquisition options, and propagating WGPU rejection each made this executable fail.

namespace {

using flight::Any;
using flight::Ref;
using flight::String;
using flight::StructuralRef;
using flight::Uint8Array;
using flight::types::AppWindow;
using flight::types::CanvasSurface;
using flight::types::EntityRuntime;
using flight::types::GlContextOptions;
using flight::types::GlSurface;
using flight::types::HostCanvasCapability;
using flight::types::HostGlCapability;
using flight::types::HostSurfaceDisplayCapability;
using flight::types::HostSurfaceResizeCapability;
using flight::types::HostWgpuCapability;
using flight::types::Surface;
using flight::types::WgpuHostAcquisition;
using flight::types::WgpuHostAcquisitionOptions;
using flight::types::WgpuSurface;
namespace surface = flight::surface;

using ReadonlySurface = surface::ReadonlySurface;
using ResizeCapabilityView = StructuralRef<flight::RowReadonly<
    flight::RowOf<Ref<HostSurfaceResizeCapability>>>>;
using DisplayCapabilityView = StructuralRef<flight::RowReadonly<
    flight::RowOf<Ref<HostSurfaceDisplayCapability>>>>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void run_test(const char* name, void (*test)()) {
  try {
    test();
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << name << " threw: " << error.what() << '\n';
    ++failures;
  }
}

bool bytes_equal(const Uint8Array& actual, const Uint8Array& expected) {
  if (actual.size() != expected.size()) return false;
  for (std::size_t i = 0; i < actual.size(); ++i) {
    if (actual[i] != expected[i]) return false;
  }
  return true;
}

Ref<Surface> make_surface(Any handle) {
  auto construction = surface::allocate_surface<Ref<Surface>>(std::move(handle));
  return flight::entity::finish_entity<Ref<Surface>>(construction);
}

ReadonlySurface readonly_surface(const Ref<Surface>& value) {
  return ReadonlySurface(value);
}

ReadonlySurface readonly_canvas(const Ref<CanvasSurface>& value) {
  return surface::readonly_surface_view(surface::ReadonlyCanvasSurface(value));
}

ReadonlySurface readonly_gl(const Ref<GlSurface>& value) {
  return surface::readonly_surface_view(surface::ReadonlyGlSurface(value));
}

ReadonlySurface readonly_wgpu(const Ref<WgpuSurface>& value) {
  return surface::readonly_surface_view(surface::ReadonlyWgpuSurface(value));
}

void test_surface_runtime_storage() {
  const auto drawable = flight::make_ref<Surface>();
  const Any object_handle = Any::object(drawable);
  const auto first = make_surface(object_handle);
  const auto first_view = readonly_surface(first);
  const auto first_runtime = surface::get_surface_runtime(first_view);
  const auto recovered_object = surface::get_surface_handle(first_view).object_if<Surface>();

  check(recovered_object == drawable,
        "allocateSurface stores the exact drawable object on its runtime");
  check(first->entity_runtime_key.has_value() &&
            first->entity_runtime_key->get() == static_cast<EntityRuntime*>(first_runtime.get()),
        "getSurfaceRuntime returns the exact owner retained in EntityRuntimeKey");
  check(!first_runtime->binding.has_value() && !first_runtime->uid.has_value(),
        "allocateSurface initializes only the base runtime's empty cells and opaque handle");

  const auto second = make_surface(Any::external<int>(2));
  const auto second_runtime = surface::get_surface_runtime(readonly_surface(second));
  const auto recovered_second =
      surface::get_surface_handle(readonly_surface(second)).external_if<int>();
  check(first_runtime != second_runtime,
        "each allocated surface has its own runtime rather than shared side storage");
  check(recovered_second != nullptr && *recovered_second == 2,
        "the second surface retains its own native handle");

  const auto numeric = make_surface(Any(7));
  check(surface::get_surface_handle(readonly_surface(numeric)).as_number() == 7.0,
        "NativeSurfaceHandle accepts a non-object numeric host representation");

  const auto copied_view = first_view;
  check(surface::get_surface_runtime(copied_view) == first_runtime &&
            surface::get_surface_handle(copied_view) == object_handle,
        "copying a readonly Surface view preserves runtime and handle identity");

  const Uint8Array bytes{0x00, 0x7f, 0x80, 0xff};
  const auto byte_surface = make_surface(Any::external<Uint8Array>(bytes));
  const auto recovered_bytes =
      surface::get_surface_handle(readonly_surface(byte_surface)).external_if<Uint8Array>();
  check(recovered_bytes != nullptr && recovered_bytes->identity() == bytes.identity(),
        "opaque byte-backed handles retain the exact Uint8Array storage identity");
  check(recovered_bytes != nullptr &&
            bytes_equal(*recovered_bytes, Uint8Array{0x00, 0x7f, 0x80, 0xff}),
        "opaque byte-backed handles preserve every byte including boundary values");
}

void test_surface_dimensions() {
  const auto target = make_surface(Any::external<int>(9));
  const auto target_view = readonly_surface(target);
  int resize_calls = 0;
  std::shared_ptr<flight::RowOwner> resized_owner;
  double backing_width = 0;
  double backing_height = 0;
  auto resize = flight::make_ref<HostSurfaceResizeCapability>();
  resize->resize = [&](ReadonlySurface received, double width, double height) {
    ++resize_calls;
    resized_owner = received.shared_owner();
    backing_width = width;
    backing_height = height;
  };
  surface::resize_surface(ResizeCapabilityView(resize), target_view, 1600, 1000);
  check(resize_calls == 1 && resized_owner == target_view.shared_owner() &&
            backing_width == 1600 && backing_height == 1000,
        "resizeSurface routes the exact surface owner and device-pixel dimensions once");

  int display_calls = 0;
  std::shared_ptr<flight::RowOwner> displayed_owner;
  double display_width = 0;
  double display_height = 0;
  auto display = flight::make_ref<HostSurfaceDisplayCapability>();
  display->set_display_size = [&](ReadonlySurface received, double width, double height) {
    ++display_calls;
    displayed_owner = received.shared_owner();
    display_width = width;
    display_height = height;
  };
  surface::set_surface_display_size(DisplayCapabilityView(display), target_view, 800, 500);
  check(display_calls == 1 && displayed_owner == target_view.shared_owner() &&
            display_width == 800 && display_height == 500,
        "setSurfaceDisplaySize routes the exact surface owner and logical dimensions once");
  check(backing_width == 1600 && backing_height == 1000 &&
            display_width == 800 && display_height == 500,
        "logical dimensions remain unscaled and independent of backing-store dimensions");

  const double largest = std::numeric_limits<double>::max();
  surface::resize_surface(ResizeCapabilityView(resize), target_view, -1, largest);
  check(resize_calls == 2 && backing_width == -1 && backing_height == largest,
        "the surface boundary forwards numeric bounds exactly without hidden clamping");
}

struct CanvasCapture {
  int create_calls = 0;
  int acquire_calls = 0;
  int release_calls = 0;
  double width = 0;
  double height = 0;
  std::shared_ptr<flight::RowOwner> window_owner;
  std::shared_ptr<flight::RowOwner> acquired_owner;
  std::shared_ptr<flight::RowOwner> released_owner;
  std::optional<flight::CanvasRenderingContext2DSettings> create_options;
  std::optional<flight::CanvasRenderingContext2DSettings> acquire_options;
};

Ref<HostCanvasCapability> canvas_capability(
    CanvasCapture& capture,
    std::optional<Any> create_result,
    std::optional<flight::CanvasRenderingContext2D> acquire_result) {
  auto capability = flight::make_ref<HostCanvasCapability>();
  capability->create = [&capture, create_result](
                           surface::ReadonlyAppWindow window,
                           double width,
                           double height,
                           std::optional<flight::CanvasRenderingContext2DSettings> options) {
    ++capture.create_calls;
    capture.window_owner = window.shared_owner();
    capture.width = width;
    capture.height = height;
    capture.create_options = options;
    return create_result;
  };
  capability->acquire = [&capture, acquire_result](
                            ReadonlySurface received,
                            std::optional<flight::CanvasRenderingContext2DSettings> options) {
    ++capture.acquire_calls;
    capture.acquired_owner = received.shared_owner();
    capture.acquire_options = options;
    return acquire_result;
  };
  capability->release = [&capture](ReadonlySurface received) {
    ++capture.release_calls;
    capture.released_owner = received.shared_owner();
  };
  return capability;
}

void test_canvas_surface() {
  const auto window = flight::make_ref<AppWindow>();
  const auto context = flight::CanvasRenderingContext2D::create(
      std::shared_ptr<flight::Canvas2DRasterizer>{},
      flight::Canvas2DSurface::create(320, 240));
  CanvasCapture capture;
  const auto capability = canvas_capability(capture, Any(String("canvas")), context);
  const auto created = surface::create_canvas_surface(
      surface::ReadonlyCanvasCapability(capability), surface::ReadonlyAppWindow(window), 320, 240);
  check(created.has_value() && (*created)->brand == String("CanvasSurface") &&
            (*created)->context == context,
        "createCanvasSurface brands the result and stores the exact acquired 2D context");
  check(created.has_value() &&
            surface::get_surface_handle(readonly_canvas(*created)).as_string() == String("canvas"),
        "createCanvasSurface stores the exact drawable handle");
  check(capture.create_calls == 1 && capture.acquire_calls == 1 && capture.width == 320 &&
            capture.height == 240 && capture.window_owner ==
                surface::ReadonlyAppWindow(window).shared_owner() &&
            !capture.create_options.has_value() && !capture.acquire_options.has_value(),
        "canvas creation forwards window, device-pixel size, and omitted settings exactly once");
  check(created.has_value() &&
            capture.acquired_owner == surface::ReadonlyCanvasSurface(*created).shared_owner(),
        "canvas acquire receives the same row owner returned to the caller");

  CanvasCapture no_drawable_capture;
  const auto no_drawable = canvas_capability(no_drawable_capture, std::nullopt, context);
  check(!surface::create_canvas_surface(
             surface::ReadonlyCanvasCapability(no_drawable), surface::ReadonlyAppWindow(window), 8, 8)
             .has_value() &&
            no_drawable_capture.acquire_calls == 0,
        "canvas creation returns null without acquiring when no drawable exists");

  CanvasCapture no_context_capture;
  const auto no_context = canvas_capability(no_context_capture, Any(String("canvas")), std::nullopt);
  check(!surface::create_canvas_surface(
             surface::ReadonlyCanvasCapability(no_context), surface::ReadonlyAppWindow(window), 8, 8)
             .has_value() &&
            no_context_capture.acquire_calls == 1,
        "canvas creation returns null when the drawable cannot rasterize 2D");

  CanvasCapture adopted_capture;
  const auto adopted_capability = canvas_capability(adopted_capture, Any(String("unused")), context);
  const auto adopted = surface::create_canvas_surface_from_native_handle(
      surface::ReadonlyCanvasCapability(adopted_capability), Any(String("adopted")));
  check(adopted.has_value() && adopted_capture.create_calls == 0 &&
            surface::get_surface_handle(readonly_canvas(*adopted)).as_string() == String("adopted"),
        "canvas native-handle creation adopts the caller's drawable without allocating");

  check(created.has_value(), "canvas release fixture was created");
  if (created.has_value()) {
    surface::destroy_canvas_surface(
        surface::ReadonlyCanvasCapability(capability), surface::ReadonlyCanvasSurface(*created));
    check(capture.release_calls == 1 &&
              capture.released_owner == surface::ReadonlyCanvasSurface(*created).shared_owner(),
          "destroyCanvasSurface releases the exact returned surface once");
  }

  const flight::CanvasRenderingContext2DSettings options{
      .alpha = false,
      .color_space = String("display-p3"),
      .desynchronized = true,
      .will_read_frequently = true,
  };
  CanvasCapture options_capture;
  const auto options_capability = canvas_capability(options_capture, Any(String("canvas")), context);
  static_cast<void>(surface::create_canvas_surface(
      surface::ReadonlyCanvasCapability(options_capability), surface::ReadonlyAppWindow(window),
      4, 3, options));
  check(options_capture.create_options.has_value() &&
            options_capture.acquire_options.has_value() &&
            options_capture.create_options->alpha == std::optional<bool>{false} &&
            options_capture.acquire_options->color_space ==
                std::optional<String>{String("display-p3")} &&
            options_capture.acquire_options->desynchronized == std::optional<bool>{true} &&
            options_capture.acquire_options->will_read_frequently == std::optional<bool>{true},
        "canvas settings are forwarded byte-for-byte to create and acquire");
}

struct GlCapture {
  int create_calls = 0;
  int acquire_calls = 0;
  int release_calls = 0;
  double width = 0;
  double height = 0;
  std::shared_ptr<flight::RowOwner> window_owner;
  std::shared_ptr<flight::RowOwner> acquired_owner;
  std::shared_ptr<flight::RowOwner> released_owner;
  std::optional<surface::ReadonlyGlOptions> create_options;
  std::optional<surface::ReadonlyGlOptions> acquire_options;
};

Ref<HostGlCapability> gl_capability(
    GlCapture& capture,
    std::optional<Any> create_result,
    std::optional<flight::host_sdl::WebGl2Context> acquire_result) {
  auto capability = flight::make_ref<HostGlCapability>();
  capability->create = [&capture, create_result](surface::ReadonlyGlAppWindow window,
                                                 double width,
                                                 double height,
                                                 std::optional<surface::ReadonlyGlOptions> options) {
    ++capture.create_calls;
    capture.window_owner = window.shared_owner();
    capture.width = width;
    capture.height = height;
    capture.create_options = options;
    return create_result;
  };
  capability->acquire = [&capture, acquire_result](
                            ReadonlySurface received,
                            std::optional<surface::ReadonlyGlOptions> options) {
    ++capture.acquire_calls;
    capture.acquired_owner = received.shared_owner();
    capture.acquire_options = options;
    return acquire_result;
  };
  capability->release = [&capture](ReadonlySurface received) {
    ++capture.release_calls;
    capture.released_owner = received.shared_owner();
  };
  return capability;
}

void test_gl_surface() {
  const auto window = flight::make_ref<AppWindow>();
  const flight::host_sdl::WebGl2Context context;
  GlCapture capture;
  const auto capability = gl_capability(capture, Any(String("canvas")), context);
  const auto created = surface::create_gl_surface(
      surface::ReadonlyGlCapability(capability), surface::ReadonlyGlAppWindow(window), 800, 500);
  check(created.has_value() && (*created)->brand == String("GlSurface") &&
            (*created)->context == context &&
            surface::get_surface_handle(readonly_gl(*created)).as_string() == String("canvas"),
        "createGlSurface stores the brand, exact context value, and drawable handle");
  check(capture.create_calls == 1 && capture.acquire_calls == 1 && capture.width == 800 &&
            capture.height == 500 && capture.window_owner ==
                surface::ReadonlyGlAppWindow(window).shared_owner() &&
            !capture.create_options.has_value() && !capture.acquire_options.has_value(),
        "GL creation forwards window, dimensions, and omitted options exactly once");
  check(created.has_value() &&
            capture.acquired_owner == surface::ReadonlyGlSurface(*created).shared_owner(),
        "GL acquire receives the same surface owner returned to the caller");

  const auto options = flight::make_ref<GlContextOptions>();
  options->antialias = true;
  GlCapture options_capture;
  const auto options_capability = gl_capability(options_capture, Any(String("canvas")), context);
  const surface::ReadonlyGlOptions options_view(options);
  static_cast<void>(surface::create_gl_surface(
      surface::ReadonlyGlCapability(options_capability), surface::ReadonlyGlAppWindow(window),
      800, 500, options_view));
  check(options_capture.create_options.has_value() &&
            options_capture.acquire_options.has_value() &&
            options_capture.create_options->identity() == options_view.identity() &&
            options_capture.acquire_options->identity() == options_view.identity() &&
            flight::row_get<flight::RowKey<"antialias">>(*options_capture.acquire_options) ==
                std::optional<bool>{true},
        "GL create and acquire receive the exact same options owner");

  GlCapture no_drawable_capture;
  const auto no_drawable = gl_capability(no_drawable_capture, std::nullopt, context);
  check(!surface::create_gl_surface(
             surface::ReadonlyGlCapability(no_drawable), surface::ReadonlyGlAppWindow(window), 8, 8)
             .has_value() &&
            no_drawable_capture.acquire_calls == 0,
        "GL creation returns null without acquiring when no drawable exists");

  GlCapture no_context_capture;
  const auto no_context = gl_capability(no_context_capture, Any(String("canvas")), std::nullopt);
  check(!surface::create_gl_surface(
             surface::ReadonlyGlCapability(no_context), surface::ReadonlyGlAppWindow(window), 8, 8)
             .has_value(),
        "GL creation returns null when context acquisition fails");

  GlCapture adopted_capture;
  const auto adopted_capability = gl_capability(adopted_capture, Any(String("unused")), context);
  const auto adopted = surface::create_gl_surface_from_native_handle(
      surface::ReadonlyGlCapability(adopted_capability), Any(String("adopted")));
  check(adopted.has_value() && adopted_capture.create_calls == 0 &&
            surface::get_surface_handle(readonly_gl(*adopted)).as_string() == String("adopted"),
        "GL native-handle creation adopts the drawable without allocating");

  GlCapture rejected_adopt_capture;
  const auto rejected_adopt = gl_capability(rejected_adopt_capture, Any(String("unused")), std::nullopt);
  check(!surface::create_gl_surface_from_native_handle(
             surface::ReadonlyGlCapability(rejected_adopt), Any(String("adopted")))
             .has_value(),
        "GL native-handle creation returns null when the adopted drawable has no context");

  check(created.has_value(), "GL release fixture was created");
  if (created.has_value()) {
    surface::destroy_gl_surface(
        surface::ReadonlyGlCapability(capability), surface::ReadonlyGlSurface(*created));
    check(capture.release_calls == 1 &&
              capture.released_owner == surface::ReadonlyGlSurface(*created).shared_owner(),
          "destroyGlSurface releases the exact returned surface once");
  }
}

struct WgpuCapture {
  int create_calls = 0;
  int acquire_calls = 0;
  int release_calls = 0;
  double width = 0;
  double height = 0;
  std::shared_ptr<flight::RowOwner> window_owner;
  std::shared_ptr<flight::RowOwner> acquired_owner;
  std::shared_ptr<flight::RowOwner> released_owner;
  std::optional<surface::ReadonlyWgpuOptions> acquire_options;
};

Ref<HostWgpuCapability> wgpu_capability(
    WgpuCapture& capture,
    std::optional<Any> create_result,
    const Ref<WgpuHostAcquisition>& acquisition,
    bool reject_acquisition = false) {
  auto capability = flight::make_ref<HostWgpuCapability>();
  capability->create = [&capture, create_result](
                           surface::ReadonlyWgpuAppWindow window, double width, double height) {
    ++capture.create_calls;
    capture.window_owner = window.shared_owner();
    capture.width = width;
    capture.height = height;
    return create_result;
  };
  capability->acquire = [&capture, acquisition, reject_acquisition](
                            ReadonlySurface received, surface::ReadonlyWgpuOptions options) {
    ++capture.acquire_calls;
    capture.acquired_owner = received.shared_owner();
    capture.acquire_options = options;
    if (reject_acquisition) {
      return flight::Task<Ref<WgpuHostAcquisition>>::reject(String("no adapter"));
    }
    return flight::Task<Ref<WgpuHostAcquisition>>::resolve(acquisition);
  };
  capability->release = [&capture](surface::ReadonlyWgpuAcquisition received) {
    ++capture.release_calls;
    capture.released_owner = received.shared_owner();
  };
  return capability;
}

void test_wgpu_surface() {
  const auto window = flight::make_ref<AppWindow>();
  const auto acquisition = flight::make_ref<WgpuHostAcquisition>();
  WgpuCapture capture;
  const auto capability = wgpu_capability(capture, Any(String("canvas")), acquisition);
  const auto created = surface::create_wgpu_surface(
      surface::ReadonlyWgpuCapability(capability), surface::ReadonlyWgpuAppWindow(window), 800, 500)
                           .get();
  check(created.has_value() && (*created)->brand == String("WgpuSurface") &&
            (*created)->acquisition == acquisition &&
            surface::get_surface_handle(readonly_wgpu(*created)).as_string() == String("canvas"),
        "createWgpuSurface stores the brand, exact acquisition, and drawable handle");
  check(capture.create_calls == 1 && capture.acquire_calls == 1 && capture.width == 800 &&
            capture.height == 500 && capture.window_owner ==
                surface::ReadonlyWgpuAppWindow(window).shared_owner(),
        "WGPU creation forwards the exact window and dimensions once");
  check(created.has_value() &&
            capture.acquired_owner == surface::ReadonlyWgpuSurface(*created).shared_owner(),
        "WGPU acquire receives the same surface owner returned to the caller");
  check(capture.acquire_options.has_value() &&
            flight::row_get<flight::RowKey<"format">>(*capture.acquire_options) == std::nullopt,
        "WGPU acquisition receives an empty options object when options are omitted");

  WgpuCapture no_drawable_capture;
  const auto no_drawable = wgpu_capability(no_drawable_capture, std::nullopt, acquisition);
  check(!surface::create_wgpu_surface(
             surface::ReadonlyWgpuCapability(no_drawable), surface::ReadonlyWgpuAppWindow(window), 8, 8)
             .get()
             .has_value() &&
            no_drawable_capture.acquire_calls == 0,
        "WGPU creation returns null without acquiring when no drawable exists");

  WgpuCapture rejected_capture;
  const auto rejected = wgpu_capability(rejected_capture, Any(String("canvas")), acquisition, true);
  check(!surface::create_wgpu_surface(
             surface::ReadonlyWgpuCapability(rejected), surface::ReadonlyWgpuAppWindow(window), 8, 8)
             .get()
             .has_value() &&
            rejected_capture.acquire_calls == 1,
        "WGPU acquisition rejection is converted to a null result");

  WgpuCapture adopted_capture;
  const auto adopted_capability = wgpu_capability(adopted_capture, Any(String("unused")), acquisition);
  const auto adopted = surface::create_wgpu_surface_from_native_handle(
      surface::ReadonlyWgpuCapability(adopted_capability), Any(String("adopted"))).get();
  check(adopted.has_value() && adopted_capture.create_calls == 0 &&
            surface::get_surface_handle(readonly_wgpu(*adopted)).as_string() == String("adopted"),
        "WGPU native-handle creation adopts the drawable without allocating");

  check(created.has_value(), "WGPU release fixture was created");
  if (created.has_value()) {
    surface::destroy_wgpu_surface(
        surface::ReadonlyWgpuCapability(capability), surface::ReadonlyWgpuSurface(*created));
    check(capture.release_calls == 1 &&
              capture.released_owner == surface::ReadonlyWgpuAcquisition(acquisition).shared_owner(),
          "destroyWgpuSurface releases the exact acquisition once");
  }

  const auto options = flight::make_ref<WgpuHostAcquisitionOptions>();
  options->format = String("rgba8unorm");
  options->power_preference = String("high-performance");
  const surface::ReadonlyWgpuOptions options_view(options);
  WgpuCapture options_capture;
  const auto options_capability = wgpu_capability(options_capture, Any(String("canvas")), acquisition);
  static_cast<void>(surface::create_wgpu_surface(
      surface::ReadonlyWgpuCapability(options_capability), surface::ReadonlyWgpuAppWindow(window),
      4, 3, options_view).get());
  check(options_capture.acquire_options.has_value(),
        "WGPU acquisition forwards explicit options");
  if (options_capture.acquire_options.has_value()) {
    const auto received_options =
        options_capture.acquire_options->shared_exact_object<WgpuHostAcquisitionOptions>();
    check(options_capture.acquire_options->identity() == options_view.identity() &&
              flight::row_get<flight::RowKey<"format">>(*options_capture.acquire_options) ==
                  std::optional<String>{String("rgba8unorm")} &&
              received_options == options &&
              received_options->power_preference == String("high-performance"),
          "WGPU acquisition preserves the exact explicit options owner and metadata");
  }
}

}  // namespace

int main() {
  run_test("surface runtime storage", test_surface_runtime_storage);
  run_test("surface dimensions", test_surface_dimensions);
  run_test("canvas surface", test_canvas_surface);
  run_test("GL surface", test_gl_surface);
  run_test("WGPU surface", test_wgpu_surface);
  if (failures != 0) {
    std::cerr << failures << " surface assertion(s) failed\n";
    return 1;
  }
  std::cout << "surface generated semantics passed\n";
  return 0;
}
