import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import { resolveDependency } from './dependencyLock.mjs';

// Compiles the Canvas 2D surface the pinned compiler emits for the portable web-types profile and
// runs it against a recording rasterizer. The runtime owns the drawing-state stack, the current
// transformation matrix and path construction; this proves the compiler's spelling of those calls
// lands on them. Drawing sources are deliberately absent: CanvasImageSource is bound by the host
// image profile, not the portable one, and the native tests cover the erased-source carrier.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const compiler = resolveDependency(root, 'flight-compiler');
const compilerEntry = path.join(
  compiler.directory,
  'packages',
  'tool-compiler',
  'dist',
  'packages',
  'tool-compiler',
  'src',
  'index.js',
);
if (!existsSync(compiler.directory)) {
  process.stdout.write('flight-compiler is not rehydrated (Canvas 2D profile oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (Canvas 2D profile oracle skipped).\n`);
  process.exit(0);
}

if (!existsSync(compilerEntry)) {
  const build = spawnSync('npm', ['run', 'build', '--silent'], {
    cwd: compiler.directory,
    encoding: 'utf8',
  });
  if (build.status !== 0) {
    process.stderr.write(`${build.stdout}${build.stderr}`);
    process.exit(1);
  }
}

const api = await import(pathToFileURL(compilerEntry));
const profiles = ['runtime', 'web-types'].map((name) =>
  JSON.parse(readFileSync(path.join(root, 'bindings', `${name}.json`), 'utf8')),
);
const bindings = {
  schema: 'flight-cpp-external-bindings/1',
  bindings: profiles.flatMap((profile) => profile.bindings),
};
const source = api.parseTypeScriptSource(
  '/flight/packages/runtime-test/src/canvas2d.ts',
  `export function describeState(ctx: CanvasRenderingContext2D): number[] {
     ctx.save();
     ctx.globalAlpha = 0.25;
     ctx.lineWidth = 4;
     ctx.fillStyle = '#112233';
     ctx.save();
     ctx.globalAlpha = 0.75;
     ctx.lineWidth = 9;
     ctx.restore();
     return [ctx.globalAlpha, ctx.lineWidth];
   }
   export function buildPath(ctx: CanvasRenderingContext2D): void {
     ctx.beginPath();
     ctx.moveTo(0, 0);
     ctx.lineTo(4, 0);
     ctx.quadraticCurveTo(6, 2, 4, 4);
     ctx.arc(2, 2, 1, 0, 3);
     ctx.rect(0, 0, 2, 2);
     ctx.roundRect(0, 0, 8, 8, 2);
     ctx.closePath();
   }
   export function transformPoint(ctx: CanvasRenderingContext2D): number[] {
     ctx.resetTransform();
     ctx.translate(10, 20);
     ctx.scale(2, 4);
     const matrix = ctx.getTransform();
     return [matrix.a, matrix.d, matrix.e, matrix.f];
   }
   export function paint(ctx: CanvasRenderingContext2D, text: string): number {
     ctx.font = '16px serif';
     ctx.textAlign = 'center';
     ctx.globalCompositeOperation = 'source-over';
     ctx.imageSmoothingQuality = 'high';
     ctx.imageSmoothingEnabled = false;
     ctx.setTransform(1, 0, 0, 1, 0, 0);
     ctx.clearRect(0, 0, 1, 2);
     ctx.fillRect(1, 2, 3, 4);
     ctx.strokeRect(0, 0, 1, 1);
     ctx.fillText(text, 2, 3);
     ctx.strokeText(text, 0, 0, 12);
     ctx.beginPath();
     ctx.rect(0, 0, 1, 1);
     ctx.fill();
     ctx.stroke();
     ctx.clip('evenodd');
     return ctx.measureText(text).width;
   }
   export function gradientStops(ctx: CanvasRenderingContext2D): CanvasGradient {
     const gradient = ctx.createLinearGradient(0, 0, 1, 1);
     gradient.addColorStop(1, '#ffffff');
     gradient.addColorStop(0, '#000000');
     ctx.fillStyle = gradient;
     return gradient;
   }
   export function imageDataRoundTrip(ctx: CanvasRenderingContext2D): ImageData {
     const created = ctx.createImageData(2, 2);
     ctx.putImageData(created, 0, 0);
     ctx.putImageData(created, 0, 0, 0, 0, 1, 1);
     return ctx.getImageData(0, 0, 2, 3);
   }
   export function dashes(ctx: CanvasRenderingContext2D): number {
     ctx.setLineDash([1, 2, 3]);
     ctx.lineDashOffset = 2;
     return ctx.getLineDash().length;
   }
   export function surfaceSize(ctx: CanvasRenderingContext2D): number[] {
     return [ctx.canvas.width, ctx.canvas.height];
   }
   export function attributes(ctx: CanvasRenderingContext2D): boolean {
     return ctx.getContextAttributes().alpha ?? true;
   }`,
);
const lowered = api.lowerTypeScriptSource(source, {
  packageName: '@flighthq/runtime-test',
  upstreamDirectory: '/flight',
});
if (lowered.diagnostics.length > 0) {
  process.stderr.write(`${JSON.stringify(lowered.diagnostics, undefined, 2)}\n`);
  process.exit(1);
}
const emitted = api.emitIrModuleCpp(lowered.module, {
  externalBindings: bindings,
  runtimeProfile: 'flight-cpp',
}).contents;
for (const expected of [
  '#include <flight/canvas_2d.hpp>',
  'flight::CanvasRenderingContext2D ctx',
  'ctx.save()',
  'ctx.restore()',
  '(ctx.global_alpha = 0.25)',
  '(ctx.fill_style = flight::String("#112233"))',
  'ctx.quadratic_curve_to(',
  'ctx.round_rect(',
  'ctx.reset_transform()',
  'ctx.get_transform()',
  'ctx.set_transform(1.0, 0.0, 0.0, 1.0, 0.0, 0.0)',
  'ctx.clear_rect(',
  'ctx.fill_rect(',
  'ctx.stroke_rect(',
  'ctx.fill_text(',
  'ctx.stroke_text(',
  'ctx.clip(flight::String("evenodd"))',
  'ctx.measure_text(text)',
  'flight::CanvasGradient gradient_stops(',
  'gradient.add_color_stop(',
  'ctx.create_image_data(2.0, 2.0)',
  'ctx.put_image_data(',
  'ctx.get_image_data(0.0, 0.0, 2.0, 3.0)',
  'ctx.set_line_dash(',
  'ctx.get_line_dash()',
  'ctx.canvas.width',
  'ctx.get_context_attributes()',
]) {
  if (!emitted.includes(expected)) {
    process.stderr.write(`Canvas 2D binding fixture did not emit ${expected}.\n${emitted}\n`);
    process.exit(1);
  }
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-canvas-2d-'));
try {
  writeFileSync(path.join(temporary, 'canvas_2d_fixture.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "canvas_2d_fixture.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace {

class Recorder final : public flight::Canvas2DRasterizer {
 public:
  void clear_rect(const flight::Canvas2DRect& rect, const flight::Canvas2DDrawingState&) override {
    cleared.push_back(rect);
  }

  void fill_path(const flight::Canvas2DPath& path, const flight::String& fill_rule,
                 const flight::Canvas2DDrawingState& state) override {
    fill_rules.push_back(fill_rule);
    fill_segments.push_back(path.segments().size());
    fill_alpha.push_back(state.global_alpha);
  }

  void stroke_path(const flight::Canvas2DPath&, const flight::Canvas2DDrawingState& state) override {
    stroke_widths.push_back(state.line_width);
  }

  void draw_image(const flight::Canvas2DImageSource&, const flight::Canvas2DRect&,
                  const flight::Canvas2DRect&, const flight::Canvas2DDrawingState&) override {}

  void draw_text(const flight::String& text, double, double, std::optional<double> max_width, bool stroke,
                 const flight::Canvas2DDrawingState& state) override {
    texts.push_back(text);
    stroked.push_back(stroke);
    max_widths.push_back(max_width);
    fonts.push_back(state.font);
    alignments.push_back(state.text_align);
  }

  [[nodiscard]] flight::WebTextMetrics measure_text(const flight::String& text,
                                                    const flight::Canvas2DDrawingState&) override {
    flight::WebTextMetrics metrics;
    metrics.width = static_cast<double>(text.length()) * 7.0;
    return metrics;
  }

  [[nodiscard]] flight::ImageData read_image_data(const flight::Canvas2DRect& rect,
                                                  const flight::ImageDataSettings& settings) override {
    return flight::ImageData(rect.width, rect.height, settings);
  }

  void write_image_data(const flight::ImageData&, double, double,
                        const std::optional<flight::Canvas2DRect>& dirty) override {
    dirty_rects.push_back(dirty);
  }

  [[nodiscard]] bool is_point_in_path(const flight::Canvas2DPath&, double, double, const flight::String&,
                                      const flight::Canvas2DDrawingState&) override {
    return true;
  }

  [[nodiscard]] bool is_point_in_stroke(const flight::Canvas2DPath&, double, double,
                                        const flight::Canvas2DDrawingState&) override {
    return false;
  }

  std::vector<flight::String> alignments;
  std::vector<flight::Canvas2DRect> cleared;
  std::vector<std::optional<flight::Canvas2DRect>> dirty_rects;
  std::vector<double> fill_alpha;
  std::vector<flight::String> fill_rules;
  std::vector<std::size_t> fill_segments;
  std::vector<flight::String> fonts;
  std::vector<std::optional<double>> max_widths;
  std::vector<bool> stroked;
  std::vector<double> stroke_widths;
  std::vector<flight::String> texts;
};

} // namespace

int main() {
  auto recorder = std::make_shared<Recorder>();
  auto context = flight::CanvasRenderingContext2D::create(
      recorder, flight::Canvas2DSurface::create(64.0, 32.0),
      flight::CanvasRenderingContext2DSettings{std::optional<bool>(false), std::nullopt, std::nullopt,
                                               std::nullopt});

  const auto restored = flighthq_runtime_test::describe_state(context);
  if (restored.size() != 2 || restored[0] != 0.25 || restored[1] != 4.0) return 1;

  flighthq_runtime_test::build_path(context);
  if (context.current_path().segments().empty()) return 2;

  const auto transform = flighthq_runtime_test::transform_point(context);
  if (transform.size() != 4 || transform[0] != 2.0 || transform[1] != 4.0 || transform[2] != 10.0 ||
      transform[3] != 20.0) {
    return 3;
  }

  if (flighthq_runtime_test::paint(context, flight::String("hi")) != 14.0) return 4;
  if (recorder->cleared.size() != 1 || recorder->cleared[0].width != 1.0) return 5;
  if (recorder->texts.size() != 2 || recorder->stroked[0] || !recorder->stroked[1]) return 6;
  if (recorder->max_widths[0].has_value() || recorder->max_widths[1] != std::optional<double>(12.0)) {
    return 7;
  }
  if (recorder->fonts[0] != flight::String("16px serif") ||
      recorder->alignments[0] != flight::String("center")) {
    return 8;
  }
  if (recorder->fill_rules.empty() || recorder->stroke_widths.empty()) return 9;
  if (context.drawing_state().clip_regions.size() != 1 ||
      context.drawing_state().clip_regions[0].fill_rule != flight::String("evenodd")) {
    return 10;
  }

  const auto gradient = flighthq_runtime_test::gradient_stops(context);
  const auto stops = gradient.color_stops();
  if (stops.size() != 2 || stops[0].offset != 0.0 || stops[1].offset != 1.0) return 11;

  const auto read = flighthq_runtime_test::image_data_round_trip(context);
  if (read.width != 2.0 || read.height != 3.0) return 12;
  if (recorder->dirty_rects.size() != 2 || recorder->dirty_rects[0].has_value() ||
      !recorder->dirty_rects[1].has_value()) {
    return 13;
  }

  if (flighthq_runtime_test::dashes(context) != 6.0) return 14;

  const auto size = flighthq_runtime_test::surface_size(context);
  if (size.size() != 2 || size[0] != 64.0 || size[1] != 32.0) return 15;

  if (flighthq_runtime_test::attributes(context)) return 16;
  return 0;
}
`,
  );
  const executable = path.join(temporary, process.platform === 'win32' ? 'oracle.exe' : 'oracle');
  const compilation = spawnSync(
    cppCompiler,
    [
      '-std=c++20',
      '-I',
      path.join(root, 'include'),
      '-I',
      temporary,
      path.join(temporary, 'consumer.cpp'),
      '-o',
      executable,
    ],
    { cwd: root, encoding: 'utf8' },
  );
  if (compilation.status !== 0) {
    process.stderr.write(`${compilation.stdout}${compilation.stderr}\nEmitted Canvas 2D header:\n${emitted}\n`);
    process.exit(1);
  }
  const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
  if (native.status !== 0) {
    process.stderr.write(
      `${native.stdout}${native.stderr}Compiler-emitted Canvas 2D fixture failed with status ${String(native.status)}.\n`,
    );
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Compiler-emitted Canvas 2D state, transform, and path calls reach the runtime (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
