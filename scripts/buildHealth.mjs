import { existsSync, lstatSync, readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const cppRoot = root;
const readCpp = (relativePath) => readFileSync(path.join(cppRoot, relativePath), 'utf8');
const failures = [];

const bazelVersion = readCpp('.bazelversion').trim();
const bazelIgnore = readCpp('.bazelignore');
const bazelModule = readCpp('MODULE.bazel');
const bazelConfiguration = readCpp('.bazelrc');
const bazelDocumentation = readCpp('docs/bazel.md');
const bazelGraph = filesUnder(cppRoot, (name) => name === 'BUILD.bazel' || name.endsWith('.bzl'))
  .map((filename) => readFileSync(filename, 'utf8'))
  .join('\n');
const cmakeGraph = filesUnder(cppRoot, (name) => name === 'CMakeLists.txt')
  .map((filename) => readFileSync(filename, 'utf8'))
  .join('\n');
const ci = readFileSync(path.join(root, '.github', 'workflows', 'ci.yml'), 'utf8');
const presets = JSON.parse(readCpp('CMakePresets.json'));
const sdkManifest = JSON.parse(readCpp('generated/manifest.json'));

if (bazelVersion !== '9.2.0') failures.push(`.bazelversion selects ${bazelVersion || '<empty>'}, expected 9.2.0`);
requireText(bazelModule, 'name = "flight_cpp"', 'Bazel module identity');
requireText(bazelModule, 'bazel_compatibility = [">=9.2.0"]', 'Bazel compatibility floor');
requireText(bazelModule, 'rules_foreign_cc", version = "0.15.1"', 'Bazel foreign CMake rules pin');
requireText(bazelModule, 'SDL3-3.4.10.tar.gz', 'Bazel SDL source pin');
requireText(bazelConfiguration, 'build --incompatible_strict_action_env', 'strict Bazel action environments');
requireText(bazelConfiguration, 'build --nostamp', 'unstamped Bazel outputs');
requireText(bazelConfiguration, 'try-import %workspace%/.bazelrc.local', 'ignored local Bazel configuration hook');
requireText(bazelConfiguration, 'build:local-posix --cxxopt=-std=c++20', 'local POSIX C++20 configuration');
requireText(bazelConfiguration, 'build:local-msvc --cxxopt=/std:c++20', 'local MSVC C++20 configuration');
for (const directory of ['.dependencies', 'build', 'out']) {
  if (!bazelIgnore.split(/\r?\n/u).includes(directory)) {
    failures.push(`.bazelignore does not exclude ${directory}`);
  }
}
requireText(bazelDocumentation, '--platforms=', 'documented Bazel platform selection');
requireText(bazelDocumentation, '--extra_toolchains=', 'documented Bazel toolchain selection');

const moduleVersion = match(bazelModule, /\bversion = "(?<value>\d+\.\d+\.\d+)"/u, 'Bazel module version');
const cmakeVersion = match(cmakeGraph, /\bVERSION (?<value>\d+\.\d+\.\d+)/u, 'CMake project version');
if (moduleVersion !== cmakeVersion) {
  failures.push(`Bazel module version ${moduleVersion} differs from CMake version ${cmakeVersion}`);
}

const externalModuleGraph =
  /\b(?:archive_override|bazel_dep|git_override|local_path_override|use_extension)\s*\(/u.test(bazelModule);
if (externalModuleGraph && !existsSync(path.join(cppRoot, 'MODULE.bazel.lock'))) {
  failures.push('external Bazel dependencies require a committed MODULE.bazel.lock');
}
if (externalModuleGraph && !bazelConfiguration.includes('common --lockfile_mode=error')) {
  failures.push('external Bazel dependencies require lockfile enforcement in .bazelrc');
}

for (const target of ['cpp', 'c', 'flight_cpp', 'flight_cpp_c', 'tests']) {
  requireText(bazelGraph, `name = "${target}"`, `Bazel //:${target} target`);
}
requireText(cmakeGraph, 'Flight::SdkPreview', 'CMake generated SDK preview target');
requireText(bazelGraph, 'name = "sdk_preview"', 'Bazel generated SDK preview target');
const generatedSdkHeaders = filesUnder(path.join(cppRoot, 'generated', 'include'), (name) => name.endsWith('.hpp'))
  .filter((filename) => !filename.includes(`${path.sep}sdk${path.sep}`));
if (generatedSdkHeaders.length !== sdkManifest.summary.emittedModules) {
  failures.push(
    `generated SDK contains ${String(generatedSdkHeaders.length)} module headers, expected ${String(sdkManifest.summary.emittedModules)}`,
  );
}

const publicHeaderRoot = path.join(cppRoot, 'include', 'flight');
const publicHeaders = filesUnder(publicHeaderRoot, (name) => name.endsWith('.hpp'))
  .filter((filename) => path.dirname(filename) === publicHeaderRoot)
  .map((filename) => path.basename(filename, '.hpp'));
for (const header of publicHeaders) {
  requireText(cmakeGraph, `  ${header}\n`, `CMake public header ${header}.hpp`);
  requireText(bazelGraph, `include/flight/${header}.hpp`, `Bazel public header ${header}.hpp`);
  requireText(bazelGraph, `("${header}",`, `Bazel self-containment test for ${header}.hpp`);
}

const hostSdlSourceRoot = path.join(cppRoot, 'src', 'host_sdl');
const productionSources = filesUnder(path.join(cppRoot, 'src'), isNativeSource).filter(
  (filename) => !filename.startsWith(`${hostSdlSourceRoot}${path.sep}`),
);
for (const source of productionSources) requireSourceInBothBuilds(source, 'runtime source');
const hostSdlSources = filesUnder(hostSdlSourceRoot, isNativeSource);
for (const source of hostSdlSources) {
  const basename = path.basename(source);
  requireText(cmakeGraph, basename, `CMake SDL host source ${basename}`);
  if (basename !== 'vulkan.cpp') requireText(bazelGraph, basename, `Bazel SDL host source ${basename}`);
}
for (const target of [
  'Flight::HostSdl',
  'Flight::HostSdlGl',
  'Flight::HostSdlImage',
  'Flight::HostSdlSdkAudio',
  'Flight::HostSdlSdkClipboard',
  'Flight::HostSdlSdkCursor',
  'Flight::HostSdlSdkPlatform',
  'Flight::HostSdlSdkWindow',
  'Flight::HostSdlVulkan',
  'Flight::HostSdlWgpu',
]) {
  requireText(cmakeGraph, target, `CMake ${target} target`);
}
for (const target of [
  'host_sdl',
  'host_sdl_gl',
  'host_sdl_image',
  'host_sdl_sdk_audio',
  'host_sdl_sdk_clipboard',
  'host_sdl_sdk_cursor',
  'host_sdl_sdk_platform',
  'host_sdl_sdk_window',
  'host_sdl_wgpu',
]) {
  requireText(bazelGraph, `name = "${target}"`, `Bazel //:${target} target`);
}

const executableTests = filesUnder(path.join(cppRoot, 'tests'), isNativeSource).filter(
  (filename) =>
    path.basename(filename) !== 'header_self_containment_test.cpp' && path.basename(filename) !== 'host_sdl_test.cpp',
);
for (const source of executableTests) requireSourceInBothBuilds(source, 'test source');
requireText(cmakeGraph, 'host_sdl_test.cpp', 'CMake SDL host test source');
requireText(bazelGraph, 'host_sdl_test.cpp', 'Bazel SDL host test source');

const benchmarkSources = filesUnder(path.join(cppRoot, 'benchmarks'), isNativeSource);
for (const source of benchmarkSources) requireSourceInBothBuilds(source, 'benchmark source');

const exampleSources = filesUnder(path.join(cppRoot, 'examples'), isNativeSource);
const hostSdlExampleSources = exampleSources.filter(
  (filename) => path.basename(filename).includes('_sdl_'),
);
const portableExampleSources = exampleSources.filter((filename) => !hostSdlExampleSources.includes(filename));
for (const source of portableExampleSources) requireSourceInBothBuilds(source, 'example source');
for (const source of hostSdlExampleSources) {
  requireText(cmakeGraph, path.basename(source), `CMake SDL example source ${path.basename(source)}`);
  requireText(bazelGraph, path.basename(source), `Bazel SDL example source ${path.basename(source)}`);
}

if (presets.version !== 2 || presets.cmakeMinimumRequired?.major !== 3 || presets.cmakeMinimumRequired.minor !== 20) {
  failures.push('CMakePresets.json must remain usable with the declared CMake 3.20 floor');
}
for (const name of ['development', 'release']) {
  const configuration = name === 'development' ? 'Debug' : 'Release';
  if (!presets.configurePresets?.some((preset) => preset.name === name && preset.binaryDir?.includes('/out/cmake/'))) {
    failures.push(`CMake configure preset ${name} is absent or writes outside out/cmake`);
  }
  if (!presets.configurePresets?.some((preset) => preset.name === name && preset.generator === 'Ninja')) {
    failures.push(`CMake configure preset ${name} does not select Ninja`);
  }
  if (!presets.buildPresets?.some((preset) => preset.name === name && preset.configuration === configuration))
    failures.push(`CMake build preset ${name} does not select ${configuration}`);
  if (!presets.testPresets?.some((preset) => preset.name === name && preset.configuration === configuration))
    failures.push(`CMake test preset ${name} does not select ${configuration}`);
}

for (const os of ['ubuntu-latest', 'macos-latest', 'windows-latest']) {
  requireText(ci, os, `Bazel CI platform ${os}`);
}
requireText(ci, 'bazel-contrib/setup-bazel@c5acdfb288317d0b5c0bbd7a396a3dc868bb0f86', 'pinned Bazel CI bootstrap');
requireText(ci, 'bazel test --config=${{ matrix.toolchain_config }} --config=ci //...', 'Bazel platform toolchain CI run');
requireText(
  ci,
  'bazel test --config=${{ matrix.toolchain_config }} --config=release //benchmarks:runtime_benchmark',
  'Bazel performance smoke CI run',
);
requireText(ci, 'bazel test --config=local-posix //tests:host_sdl_test', 'Bazel SDL host CI run');
requireText(ci, 'bazel run --config=local-posix //examples:sound_sdl -- --smoke', 'Bazel SDL sound example CI run');
requireText(ci, 'bazel run --config=local-posix //examples:tween_sdl_gl -- --smoke', 'Bazel SDL example CI run');

if (failures.length > 0) {
  process.stderr.write(`C++ build metadata failed with ${String(failures.length)} error(s):\n`);
  for (const failure of failures) process.stderr.write(`- ${failure}\n`);
  process.exit(1);
}

process.stdout.write(
  `C++ build metadata agrees across CMake and Bazel ${bazelVersion}: ${String(publicHeaders.length)} public headers, ${String(productionSources.length)} runtime source(s), ${String(executableTests.length)} executable test source(s), ${String(benchmarkSources.length)} benchmark source(s), and ${String(portableExampleSources.length)} portable example source(s); both builds declare ${String(hostSdlSources.length - 1)} SDL/GL/WGPU host source(s) and ${String(hostSdlExampleSources.length)} SDL example source(s), while CMake additionally declares the Vulkan adapter.\n`,
);

function filesUnder(directory, include) {
  const files = [];
  for (const entry of readdirSync(directory).sort()) {
    const filename = path.join(directory, entry);
    const status = lstatSync(filename);
    if (status.isSymbolicLink()) continue;
    if (status.isDirectory()) {
      if (entry === 'build' || entry === 'out' || entry.startsWith('bazel-')) continue;
      files.push(...filesUnder(filename, include));
    } else if (include(entry)) files.push(filename);
  }
  return files;
}

function isNativeSource(name) {
  return /\.(?:c|cc|cpp)$/u.test(name);
}

function match(contents, pattern, label) {
  const value = pattern.exec(contents)?.groups?.value;
  if (value) return value;
  failures.push(`could not read ${label}`);
  return '<missing>';
}

function requireSourceInBothBuilds(filename, kind) {
  const basename = path.basename(filename);
  requireText(cmakeGraph, basename, `CMake ${kind} ${basename}`);
  requireText(bazelGraph, basename, `Bazel ${kind} ${basename}`);
}

function requireText(contents, expected, label) {
  if (!contents.includes(expected)) failures.push(`${label} is missing`);
}
