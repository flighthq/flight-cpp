import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// Run untouched public GL registry/runtime bodies and their actual dependency bodies. Contract imports are
// resolved lazily through their original re-exports, so unrelated browser modules are not executed.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const equals = arg.indexOf('=');
  assert.ok(equals > 0, 'arguments use --name=value');
  return [arg.slice(0, equals), arg.slice(equals + 1)];
}));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'] ??
  path.join(root, '.dependencies/flight-compiler/node_modules/typescript/lib/typescript.js'))));
const flight = path.resolve(args['--flight'] ?? path.join(root, '.dependencies/flight'));
const packages = path.join(flight, 'packages');
const parsedFiles = new Map(), modules = new Map(), origins = new Map();
function sourceFile(file) {
  if (!fs.existsSync(file)) file = file.replace(/\.js$/u, '.ts');
  assert.ok(fs.existsSync(file), 'missing original source: ' + file);
  return file;
}
function resolve(specifier, from) {
  return sourceFile(specifier.startsWith('@flighthq/')
    ? path.join(packages, specifier.slice('@flighthq/'.length).replace(/\/contract$/u, '/src/contract.ts'))
    : path.resolve(path.dirname(from), specifier));
}
function ast(file) {
  if (!parsedFiles.has(file)) parsedFiles.set(file,
    ts.createSourceFile(file, fs.readFileSync(file, 'utf8'), ts.ScriptTarget.Latest, true));
  return parsedFiles.get(file);
}
function origin(file, name, seen = new Set()) {
  const key = file + '#' + name;
  if (origins.has(key)) return origins.get(key);
  if (seen.has(key)) return undefined;
  seen = new Set(seen).add(key);
  for (const statement of ast(file).statements) {
    if (ts.isExportDeclaration(statement) && !statement.isTypeOnly) {
      if (statement.exportClause && ts.isNamedExports(statement.exportClause)) {
        for (const entry of statement.exportClause.elements) {
          if (entry.isTypeOnly || entry.name.text !== name) continue;
          const local = (entry.propertyName ?? entry.name).text;
          const result = statement.moduleSpecifier
            ? origin(resolve(statement.moduleSpecifier.text, file), local, seen) : { file, name };
          if (result) { origins.set(key, result); return result; }
        }
      } else if (!statement.exportClause && statement.moduleSpecifier) {
        const result = origin(resolve(statement.moduleSpecifier.text, file), name, seen);
        if (result) { origins.set(key, result); return result; }
      }
    }
    if (!statement.modifiers?.some(modifier => modifier.kind === ts.SyntaxKind.ExportKeyword)) continue;
    const names = ts.isVariableStatement(statement)
      ? statement.declarationList.declarations.map(declaration => declaration.name.getText(ast(file)))
      : statement.name ? [statement.name.text] : [];
    if (names.includes(name) && !ts.isInterfaceDeclaration(statement) && !ts.isTypeAliasDeclaration(statement)) {
      const result = { file, name }; origins.set(key, result); return result;
    }
  }
  return undefined;
}
function proxy(file) {
  return new Proxy({}, { get(_target, name) {
    if (name === '__esModule') return true;
    const target = origin(file, name);
    assert.ok(target, `missing original export ${String(name)} from ${file}`);
    return load(target.file)[target.name];
  } });
}
function load(file) {
  if (modules.has(file)) return modules.get(file).exports;
  const module = { exports: {} }; modules.set(file, module);
  const code = ts.transpileModule(fs.readFileSync(file, 'utf8'), {
    compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS },
  }).outputText;
  new Function('require', 'module', 'exports', code)(specifier => proxy(resolve(specifier, file)), module, module.exports);
  return module.exports;
}

// The state fixtures use the existing non-GPU construction boundary. Every called implementation,
// including entity allocation, logging, signal dispatch and persistent registration, is original source.
const api = (pkg, file) => load(path.join(packages, pkg, 'src', file + '.ts'));
const stateApi = api('render', 'renderState');
const glApi = api('render-gl', 'glRenderState');
const guards = api('render', 'renderRegistryGuards');
const signals = api('render', 'renderRegistrySignals');
const materials = api('render-gl', 'glQuadMaterialRegistry');
const entity = proxy(path.join(packages, 'entity/src/contract.ts'));
const types = proxy(path.join(packages, 'types/src/contract.ts'));
const logging = proxy(path.join(packages, 'log/src/contract.ts'));
const key = types.EntityRuntimeKey, table = types.RenderRegistryTable;
const observed = [];
function check(condition, name) { assert.ok(condition, name); observed.push(name); }
function makeState() {
  const state = stateApi.createRenderState();
  const context = { references: 1, teardowns: [] };
  const contextState = entity.allocateEntity();
  contextState[key] = context;
  contextState.gl = Object.freeze({});
  const runtime = glApi.createGlRenderStateRuntime(entity.finishEntity(contextState), {});
  state.gl = contextState.gl;
  state.contextState = contextState;
  state[key] = runtime;
  return { state, runtime, context };
}
logging.clearLogSinks();
logging.clearLogOnceKeys();
logging.setLogConsoleLevel(types.LogLevel.None);
const logs = [];
logging.addLogSink(entry => logs.push(entry));
const first = makeState(), second = makeState();
check(first.context.references === 2 && first.runtime.context === first.context,
  'runtime factory retains exact context and increments shared reference');
guards.enableRenderRegistriesGuards(first.state);
guards.enableRenderRegistriesGuards(first.state);
guards.enableRenderRegistriesGuards(second.state);
const firstSignals = signals.enableRenderRegistrySignals(first.state);
const emitter = first.runtime.registryMiss;
check(emitter.signals === firstSignals && signals.enableRenderRegistrySignals(first.state) === firstSignals,
  'installed callback and repeated public opt-in share one signal owner');
emitter(table.MaterialRenderer, 'native-λ');
emitter(table.MaterialRenderer, 'native-λ');
emitter(table.TextureResolver, 'native-λ');
emitter(table.MaterialRenderer, 'other');
second.runtime.registryMiss(table.MaterialRenderer, 'native-λ');
check(logs.length === 4, 'installed callback deduplicates by state/table/kind');
check(logs.every(entry => entry.level === types.LogLevel.Warn && entry.channel === 'render'),
  'registry warning level and channel');
check(logs[0].data.message.includes('resolveGlQuadMaterialRenderer'), 'GL material diagnostic route');
check(logs[1].data.message.includes('resolveGlTexture'), 'GL texture diagnostic route');
const before = guards.explainRenderRegistriesMisses(first.state);
const repeat = guards.explainRenderRegistriesMisses(first.state);
check(before.misses !== repeat.misses && before.misses[0] !== repeat.misses[0],
  'each public explanation owns fresh array and miss records');
emitter(table.MaterialRenderer, 'via-runtime');
check(before.misses.length === 3 && guards.explainRenderRegistriesMisses(first.state).misses.length === 4,
  'retained explanation snapshot does not grow with later callback');
check(guards.explainRenderRegistriesMisses(second.state).misses.length === 1,
  'first callback leaves second state explanation untouched');
first.runtime.tempStack.push({});
stateApi.destroyRenderState(first.state);
check(first.runtime.registryMiss === null && first.runtime.tempStack.length === 0,
  'base public teardown clears callback and traversal stack on same runtime');
emitter(table.MaterialRenderer, 'after-destroy');
check(logs.length === 5 && guards.explainRenderRegistriesMisses(first.state).misses.length === 4,
  'retained callback cannot reach cleared guard listener after teardown');
check(first.state[key] === first.runtime && before.misses.length === 3,
  'public teardown retains runtime owner and earlier explanation');
second.runtime.registryMiss(table.MaterialRenderer, 'second-still-live');
check(logs.length === 6 && guards.explainRenderRegistriesMisses(second.state).misses.length === 2,
  'first teardown leaves second callback live');

const teardown = makeState(), teardownOrder = [];
glApi.registerGlRenderStateTeardown(teardown.state, received => {
  check(received === teardown.state, 'first teardown receives original state owner');
  teardownOrder.push('first');
  glApi.registerGlRenderStateTeardown(received, () => teardownOrder.push('late'));
});
glApi.registerGlRenderStateTeardown(teardown.state, received => {
  check(received === teardown.state, 'second teardown receives original state owner');
  teardownOrder.push('second');
});
glApi.destroyGlRenderState(teardown.state);
check(teardownOrder.join(',') === 'first,second' && teardown.runtime.teardowns.length === 0,
  'GL teardown iterates snapshot in order and clears callback added during teardown');
check(teardown.context.references === 1 && teardown.state[key] === teardown.runtime,
  'shared-context teardown decrements once and retains original runtime owner');
glApi.destroyGlRenderState(teardown.state);
check(teardown.context.references === 1 && teardownOrder.length === 2,
  'GL teardown is idempotent with no repeated callbacks');

const materialState = makeState();
const initial = materialState.runtime.registries.materialRenderers;
const bind = () => {};
const renderer = { instanceFloatCount: 0, bind }, replacement = { instanceFloatCount: 0, bind };
const material = { kind: 'source-material', extra: {} };
check(materials.resolveGlQuadMaterialRenderer(materialState.state, material) === null &&
  materials.resolveGlQuadMaterialRenderer(materialState.state, null) === null,
  'unregistered material and default remain null without built-in fallback');
materials.registerGlQuadMaterialRenderer(materialState.state, material.kind, renderer);
const retained = materialState.runtime.registries.materialRenderers;
check(initial !== retained && initial.size === 0 && retained.get(material.kind) === renderer,
  'material registration owns fresh table while retaining exact renderer');
check(materials.resolveGlQuadMaterialRenderer(materialState.state, material) === renderer &&
  materials.getGlQuadMaterialRenderer(materialState.state, material.kind) === renderer && renderer.bind === bind,
  'public get/resolve preserve descriptor and callback identity');
materials.registerGlQuadMaterialRenderer(materialState.state, material.kind, replacement);
check(retained.get(material.kind) === renderer &&
  materials.resolveGlQuadMaterialRenderer(materialState.state, material) === replacement,
  'last registration wins without mutating retained material table');
material.kind = 'unregistered-live-kind';
check(materials.resolveGlQuadMaterialRenderer(materialState.state, material) === null,
  'material resolution reads same material live kind');
materials.registerGlQuadMaterialRenderer(materialState.state, types.StandardMaterialKind, renderer);
check(materials.resolveGlQuadMaterialRenderer(materialState.state, material) === renderer &&
  materials.resolveGlQuadMaterialRenderer(materialState.state, null) === renderer,
  'explicit standard registration supplies the source fallback');
logging.clearLogSinks();
logging.clearLogOnceKeys();
const report = {
  schema: 'flight-cpp-gl-public-source-observable-controls/1',
  scope: 'Original TypeScript bodies and original lazy-resolved dependency bodies ONLY; no generated C++, native consumer, SDK or GPU draw qualification.',
  checks: observed.length, observed,
  sourceFiles: [...modules.keys()].sort().map(file => ({
    path: path.relative(flight, file),
    sha256: createHash('sha256').update(fs.readFileSync(file)).digest('hex'),
  })),
  nativeConsumer: { path: 'artifacts/gl-public-registry-consumer.cpp', state: 'NOT RUN: actual builder2 registry helper/body plus B inheritance/coherent output pending' },
  limits: ['Shared-context teardown branch only; no last-context GPU resource destruction.',
    'Material get/register/resolve identity is checked; no GPU draw dispatch, program or frame claim.',
    'Existing generated_scene2d_gl_owners_test.cpp already checks concrete Material2D owner/live kind and proxy teardown; not rerun or changed.',
    'No generated/provider/header/override/pin changes, target-copy scope or SDK runs.'],
};
if (args['--output']) fs.writeFileSync(args['--output'], JSON.stringify(report, null, 2) + '\n');
console.log('Original public GL source controls PASS: ' + observed.length + '; native/GPU pending.');
