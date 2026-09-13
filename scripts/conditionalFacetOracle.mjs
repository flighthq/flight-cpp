import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import { resolveDependency } from './dependencyLock.mjs';

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
  process.stdout.write('flight-compiler is not rehydrated (conditional facet oracle skipped).\n');
  process.exit(0);
}

const cppCompiler = process.env.CXX || 'c++';
if (spawnSync(cppCompiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`No C++ compiler available as ${cppCompiler} (conditional facet oracle skipped).\n`);
  process.exit(0);
}

if (!existsSync(compilerEntry)) {
  const build = spawnSync('npm', ['run', 'build', '--silent'], { cwd: compiler.directory, encoding: 'utf8' });
  if (build.status !== 0) {
    process.stderr.write(`${build.stdout}${build.stderr}`);
    process.exit(1);
  }
}

const api = await import(pathToFileURL(compilerEntry));
const source = api.parseTypeScriptSource(
  '/flight/packages/tray/src/conditional-facet.ts',
  `interface Entity { runtime: number }
   interface TrayIcon extends Entity {}
   declare const TrayImageFacetKey: unique symbol;
   export interface TrayWithImage extends TrayIcon { readonly [TrayImageFacetKey]: true }
   type TrayFacetFor<Host, Slot extends string, Facet> = Host extends {
     readonly tray: { readonly [Key in Slot]: unknown }
   } ? Facet : unknown;
   export type TrayIconForHost<Host> =
     TrayIcon & TrayFacetFor<Host, 'image', TrayWithImage>;
   export function assumeTrayFacets<Host>(icon: TrayIcon): TrayIconForHost<Host> {
     return icon as TrayIconForHost<Host>;
   }
   export function readRuntime(icon: TrayWithImage): number { return icon.runtime; }`,
);
const lowered = api.lowerTypeScriptSource(source, {
  packageName: '@flighthq/tray',
  upstreamDirectory: '/flight',
});
if (lowered.diagnostics.length > 0) {
  process.stderr.write(`${JSON.stringify(lowered.diagnostics, undefined, 2)}\n`);
  process.exit(1);
}
const emitted = api.emitIrModuleCpp(lowered.module, { runtimeProfile: 'flight-cpp' }).contents;
if (
  !emitted.includes('flight::ConditionalFacetRef<') ||
  !emitted.includes('flight::assume_conditional_facets<')
) {
  process.stderr.write('Pinned compiler did not emit the conditional facet ABI.\n');
  process.exit(1);
}

const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-conditional-facet-'));
try {
  writeFileSync(path.join(temporary, 'facet.hpp'), emitted);
  writeFileSync(
    path.join(temporary, 'consumer.cpp'),
    `#include "facet.hpp"

#include <concepts>
#include <optional>

struct TrayCapabilities { bool image; };
struct CapableHost { TrayCapabilities tray; };
struct IncapableTray {};
struct IncapableHost { IncapableTray tray; };
struct OptionalHost { std::optional<TrayCapabilities> tray; };

static_assert(std::convertible_to<
              flighthq_tray::TrayIconForHost<CapableHost>,
              flighthq_tray::TrayWithImage>);
static_assert(!std::convertible_to<
              flighthq_tray::TrayIconForHost<IncapableHost>,
              flighthq_tray::TrayWithImage>);
static_assert(!std::convertible_to<
              flighthq_tray::TrayIconForHost<OptionalHost>,
              flighthq_tray::TrayWithImage>);

int main() {
  auto icon = flight::make_ref<flighthq_tray::TrayIcon>();
  icon->runtime = 42.0;
  const auto conditional = flighthq_tray::assume_tray_facets<CapableHost>(icon);
  const flighthq_tray::TrayWithImage image = conditional;
  return flighthq_tray::read_runtime(image) == 42.0 &&
                 image.shared_reference() == icon && conditional.shared_reference() == icon
             ? 0
             : 1;
}
`,
  );
  const executable = path.join(temporary, process.platform === 'win32' ? 'oracle.exe' : 'oracle');
  const compilation = spawnSync(
    cppCompiler,
    [
      '-std=c++20',
      '-pthread',
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
    process.stderr.write(`${compilation.stdout}${compilation.stderr}`);
    process.exit(1);
  }
  const native = spawnSync(executable, [], { cwd: root, encoding: 'utf8' });
  if (native.status !== 0) {
    process.stderr.write(`${native.stdout}${native.stderr}`);
    process.exit(1);
  }
} finally {
  rmSync(temporary, { force: true, recursive: true });
}

process.stdout.write(
  `Compiler-emitted conditional facets compile and preserve static capability gates (${compiler.commit.slice(0, 7)}, ${cppCompiler}).\n`,
);
