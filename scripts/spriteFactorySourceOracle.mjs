import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// Execute the original Sprite factory and its actual entity, node and trait dependencies.
// Contract exports resolve lazily; no provider or factory body is replaced.
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
const overlay = args['--overlay'] ? path.resolve(args['--overlay']) : undefined;
function sourceText(file) {
  const relative = path.relative(flight, file);
  const patched = overlay && path.join(overlay, relative);
  const text = fs.readFileSync(patched && fs.existsSync(patched) ? patched : file, 'utf8');
  return text;
}
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
    ts.createSourceFile(file, sourceText(file), ts.ScriptTarget.Latest, true));
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
  const code = ts.transpileModule(sourceText(file), {
    compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS },
  }).outputText;
  new Function('require', 'module', 'exports', code)(specifier => proxy(resolve(specifier, file)), module, module.exports);
  return module.exports;
}

const sprite = load(path.join(packages, 'scene2d/src/sprite.ts'));
const display = load(path.join(packages, 'scene2d/src/displayObject.ts'));
const types = proxy(path.join(packages, 'types/src/contract.ts'));
const key = types.EntityRuntimeKey;
const checks = [];
function check(condition, label) { assert.ok(condition, label); checks.push(label); }
const plain = sprite.createSprite();
const second = sprite.createSprite(undefined);
check(plain !== second && plain.data !== second.data && plain[key] !== second[key],
  'each source factory call owns fresh Sprite, SpriteData and SpriteRuntime');
check(plain.kind === types.SpriteKind && plain.enabled && plain.name === null &&
  plain.data.texture === null && plain.alpha === 1 && plain.visible && plain.scaleX === 1 && plain.scaleY === 1,
  'source default Node, Sprite data and appearance values');
check(['pivotX', 'pivotY', 'rotation', 'skewX', 'skewY', 'x', 'y'].every(name => plain[name] === 0) &&
  ['blendMode', 'material', 'materialData', 'clip'].every(name => plain[name] === null),
  'source default transform and nullable trait values');
check(sprite.getSpriteRuntime(plain) === plain[key] && plain[key].traits === types.Node2DTraitsKey &&
  plain[key].scene2d === null && plain[key].localBoundsTexture === null && plain[key].localBoundsTextureVersion === -1,
  'Sprite runtime accessor retains the original exact runtime and Node2D traits');
check(plain[key].computeLocalBoundsRectangle === sprite.computeSpriteLocalBoundsRectangle &&
  typeof plain[key].isLocalBoundsRectangleValid === 'function' &&
  plain[key].canAddChild(plain, second) === true,
  'authored Sprite bounds callbacks and original Node default method remain installed');
check(plain[key].isLocalBoundsRectangleValid(plain) === true,
  'default Sprite bounds cache validity runs through the installed original callback');

const order = ['data', 'texture', 'name', 'enabled', 'pivotX', 'pivotY', 'rotation', 'scaleX', 'scaleY',
  'skewX', 'skewY', 'x', 'y', 'alpha', 'visible', 'blendMode', 'material', 'materialData', 'clip'];
const texture = { version: 9 }, material = {}, materialData = {}, clip = {};
const values = { texture, name: 'authored', enabled: false, pivotX: 2, pivotY: 3, rotation: 4,
  scaleX: 0, scaleY: -2, skewX: 5, skewY: 6, x: 7, y: 8, alpha: 0, visible: false,
  blendMode: 'multiply', material, materialData, clip };
function options(reads, throwing) {
  const nested = {};
  Object.defineProperty(nested, 'texture', { get() {
    reads.push('texture'); if (throwing?.name === 'texture') throw throwing.error; return values.texture;
  } });
  const input = {};
  for (const name of order.filter(name => name !== 'texture')) Object.defineProperty(input, name, { get() {
    reads.push(name); if (throwing?.name === name) throw throwing.error;
    return name === 'data' ? nested : values[name];
  } });
  Object.defineProperty(input, 'kind', { get() { throw new Error('source must ignore input kind'); } });
  Object.defineProperty(input, key, { get() { throw new Error('source must ignore input runtime'); } });
  return { input, nested };
}
const reads = [];
const { input, nested } = options(reads);
const result = sprite.createSprite(input);
check(JSON.stringify(reads) === JSON.stringify(order), 'source options getters run once in authored initializer order');
check(result !== input && result.data !== nested && result.data.texture === texture && result[key] !== plain[key],
  'source allocates fresh Sprite and data while retaining the supplied texture owner');
check(Object.keys(values).filter(name => name !== 'texture').every(name => result[name] === values[name]) &&
  result.kind === types.SpriteKind,
  'source preserves false, zero, negative transform values and all supplied trait owners');
check(sprite.getSpriteRuntime(result) === result[key] && result[key].traits === types.Node2DTraitsKey &&
  result[key].computeLocalBoundsRectangle === plain[key].computeLocalBoundsRectangle &&
  result[key].isLocalBoundsRectangleValid === plain[key].isLocalBoundsRectangleValid,
  'options construction retains the same runtime contract and callback identities as defaults');
check(Reflect.ownKeys(nested).length === 1 && Reflect.ownKeys(input).length === order.length + 1,
  'source never writes to options or nested input data');
const clone = sprite.cloneSprite(result);
check(clone !== result && clone.data !== result.data && clone[key] !== result[key] && clone.data.texture === texture,
  'original clone factory retains texture while allocating fresh Sprite, data and runtime');
check(clone.name === null && clone.alpha === 1 && clone.x === 0,
  'original clone copies only authored texture rather than unrelated source traits');
for (const data of [undefined, null, {}, { texture: undefined }, { texture: null }]) {
  const value = sprite.createSprite({ data });
  check(value.data.texture === null, 'source nullish data/texture defaults: ' + JSON.stringify(data));
}
for (const name of order) {
  const trace = [], error = { sourceError: name };
  let caught;
  try { sprite.createSprite(options(trace, { name, error }).input); } catch (value) { caught = value; }
  check(caught === error && JSON.stringify(trace) === JSON.stringify(order.slice(0, order.indexOf(name) + 1)),
    'source preserves thrown getter owner and stops at ' + name);
}
const runtime = display.createNode2DRuntime(), events = [];
const custom = display.createNode2D(types.SpriteKind, { get data() { events.push('data'); return nested; } },
  value => { events.push('dataFactory'); return sprite.createSpriteData(value); },
  () => { events.push('runtimeFactory'); return runtime; });
check(JSON.stringify(events) === JSON.stringify(['runtimeFactory', 'data', 'dataFactory']) &&
  custom[key] === runtime && custom.data.texture === texture,
  'original Node2D factory stores exact supplied runtime before reading data and calling its factory');
const error = {}, errorEvents = [];
let caught;
try {
  display.createNode2D(types.SpriteKind, { get data() { errorEvents.push('data'); return nested; } },
    sprite.createSpriteData, () => { errorEvents.push('runtimeFactory'); throw error; });
} catch (value) { caught = value; }
check(caught === error && JSON.stringify(errorEvents) === JSON.stringify(['runtimeFactory']),
  'original runtime factory error precedes every options read and retains thrown owner');

const report = {
  schema: 'flight-cpp-sprite-factory-source-controls/1',
  scope: 'Original TypeScript source controls only; no emitted/native Sprite or image consumer pass implied.',
  checks: checks.length, failures: 0, observed: checks,
  executedSources: [...modules.keys()].map(file => ({
    source: path.relative(flight, file), sha256: createHash('sha256').update(sourceText(file)).digest('hex'),
  })).sort((left, right) => left.source.localeCompare(right.source)),
};
if (args['--report']) fs.writeFileSync(args['--report'], JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify(report, null, 2));

