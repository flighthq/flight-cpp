import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// Exercise the public original image handler and real Sprite/Texture/hierarchy dependencies.
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


const image = load(path.join(packages, 'scene2d-formats/src/lottieImageLayer.ts'));
const display = load(path.join(packages, 'scene2d/src/displayObject.ts'));
const sprite = load(path.join(packages, 'scene2d/src/sprite.ts'));
const types = proxy(path.join(packages, 'types/src/contract.ts'));
const key = types.EntityRuntimeKey;
const checks = [];
function check(condition, label) { assert.ok(condition, label); checks.push(label); }
function scenario({ refId = 'image', asset, resolver } = {}) {
  const parent = display.createDisplayObject();
  const diagnostics = [];
  const context = { assets: new Map(asset ? [['image', asset]] : []), diagnostics,
    resolveImageResource: resolver };
  const layer = { refId };
  const input = { container: parent, layer, import: context };
  return { parent, diagnostics, context, layer, input,
    children: () => parent[key].children ?? [] };
}
const asset = { id: 'image', p: 'asset.png', w: 20, h: 10 };
const resource = { kind: 'image', width: 20, height: 10 };
let calls = 0, received;
const resolver = value => { calls++; received = value; return resource; };
const valid = scenario({ asset, resolver });
const handler = image.lottieImageLayerHandler;
handler(valid.input);
const child = valid.children()[0];
check(calls === 1 && received === asset, 'public image handler invokes original resolver once with the exact asset');
check(valid.children().length === 1 && valid.diagnostics.length === 0,
  'resolved original public image handler appends one child without diagnostics');
check(child.kind === types.SpriteKind && child !== valid.parent && child.data !== resource,
  'original factories allocate a distinct Sprite and SpriteData');
check(child.data.texture.dimension === '2d' && child.data.texture.source === resource,
  'original nested Texture2D allocation retains exact resolver ImageResource');
check(child[key].parent === valid.parent && valid.parent[key].children[0] === child,
  'original hierarchy binds actual child and parent in both directions');
check(sprite.getSpriteRuntime(child) === child[key] &&
  child[key].computeLocalBoundsRectangle === sprite.computeSpriteLocalBoundsRectangle,
  'constructed Sprite retains original runtime accessor and authored bounds callback');
check(child.alpha === 1 && child.visible === true && child.x === 0 && child.scaleX === 1,
  'public image options preserve original Sprite defaults');
handler(valid.input);
const second = valid.children()[1];
check(valid.children().length === 2 && child !== second && child.data !== second.data &&
  child[key] !== second[key] && child.data.texture !== second.data.texture &&
  second.data.texture.source === resource,
  'repeated public calls allocate fresh Sprite data runtime and texture but retain image identity');
check(handler === image.lottieImageLayerHandler && valid.context.resolveImageResource === resolver,
  'fixture calls original public handler and resolver identities without replacement');
for (const [label, values, kind, severity] of [
  ['missing asset', {}, 'lottie.unresolved-asset', 'Drop'],
  ['non-image asset', {asset: {id:'image', layers:[]}}, 'lottie.unresolved-asset', 'Drop'],
  ['missing resolver', {asset}, 'lottie.unresolved-image', 'Skip'],
  ['null resolver result', {asset, resolver: () => null}, 'lottie.unresolved-image', 'Skip'],
]) {
  const value = scenario(values);
  handler(value.input);
  check(value.children().length === 0 && value.diagnostics.length === 1 &&
    value.diagnostics[0].kind === kind && value.diagnostics[0].severity === severity &&
    value.diagnostics[0].origin === 'appendLottieImage',
    'public image ' + label + ' preserves original diagnostic and no child');
}
const thrown = { originalResolverError: true };
const failing = scenario({asset, resolver: () => { throw thrown; }});
let caught;
try { handler(failing.input); } catch (value) { caught = value; }
check(caught === thrown && failing.children().length === 0 && failing.diagnostics.length === 0,
  'public resolver exception keeps exact thrown owner before construction or diagnostics');
const getterEvents = [];
const getterAsset = { id: 'image', get p() { throw new Error('in must not invoke p getter'); } };
const access = scenario({asset: getterAsset, resolver: value => {
  getterEvents.push('resolve'); assert.equal(value,getterAsset); return resource;
}});
Object.defineProperty(access.layer, 'refId', { get() { getterEvents.push('refId'); return 'image'; } });
handler(access.input);
check(JSON.stringify(getterEvents) === JSON.stringify(['refId','refId','resolve']) &&
  access.children().length === 1,
  'original image branch preserves refId reads and in-presence without invoking asset p getter');
const getterThrown = { originalRefIdError: true };
Object.defineProperty(failing.layer, 'refId', { get() { throw getterThrown; } });
caught = undefined;
try { handler(failing.input); } catch (value) { caught = value; }
check(caught === getterThrown && failing.children().length === 0 && failing.diagnostics.length === 0,
  'original layer getter exception keeps exact thrown owner and stops before resolver');
const report = { schema: 'sprite-original-public-image-source-oracle/1',
  scope: 'Original TypeScript public image consumer only; no native factory/getter reach or rendered-frame claim.',
  sourceInputs: [...new Set([...parsedFiles.keys(), ...modules.keys()])].map(file => ({
    source:path.relative(flight,file), sha256:createHash('sha256').update(sourceText(file)).digest('hex')
  })).sort((a,b)=>a.source.localeCompare(b.source)),
  checks: checks.length, failures: 0, observed: checks,
  executedSources: [...modules.keys()].map(file => ({source:path.relative(flight,file),
    sha256:createHash('sha256').update(sourceText(file)).digest('hex')}))
    .sort((a,b)=>a.source.localeCompare(b.source)) };
if(args['--report']) fs.writeFileSync(args['--report'],JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify(report,null,2));
