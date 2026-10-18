import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// Execute original Sprite bounds callbacks and actual dependencies, without replacement bodies.
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
const types = proxy(path.join(packages, 'types/src/contract.ts'));
const key = types.EntityRuntimeKey;
const source = sprite.createSprite();
const runtime = source[key];
const compute = runtime.computeLocalBoundsRectangle;
const valid = runtime.isLocalBoundsRectangleValid;
const checks = [], cases = [];
function check(condition, label) { assert.ok(condition, label); checks.push(label); }
function record(label, out) { cases.push({label, x:out.x, y:out.y, width:out.width, height:out.height, version:runtime.localBoundsTextureVersion, valid:valid(source)}); }
check(compute === sprite.computeSpriteLocalBoundsRectangle, 'stored compute retains original function identity');
const out = {x:3, y:4, width:99, height:99};
runtime.localBoundsTextureVersion = 5;
check(!valid(source), 'original validity rejects stale null stamp');
compute(out, source);
check(out.width === 0 && out.height === 0 && out.x === 3 && out.y === 4, 'original null bounds preserve origin');
check(runtime.localBoundsTexture === null && runtime.localBoundsTextureVersion === -1 && valid(source), 'original null cache refresh');
record('null', out);
const texture = {dimension:'2d', source:{width:20, height:10}, uvScale:{x:-2,y:3}, uvRotation:0, version:7};
source.data.texture = texture;
check(!valid(source), 'original validity rejects new texture identity');
compute(out, source);
check(out.width === 40 && out.height === 30 && runtime.localBoundsTexture === texture && runtime.localBoundsTextureVersion === 7 && valid(source), 'original scaled bounds and exact texture cache');
record('scaled', out);
texture.version = 8;
texture.uvRotation = Math.PI / 2;
check(!valid(source), 'original validity sees live version change');
compute(out, source);
check(out.width === 20 && out.height === 60 && runtime.localBoundsTextureVersion === 8 && valid(source), 'original quarter-turn bounds');
record('quarter-turn', out);
const replacement = {...texture};
source.data.texture = replacement;
check(!valid(source), 'same version with distinct texture identity invalidates');
replacement.source = null;
compute(out, source);
check(out.width === 0 && out.height === 0 && runtime.localBoundsTexture === replacement && valid(source), 'original unbound texture gives zero bounds');
record('unbound', out);
source.data.texture = null;
compute(out, source);
check(runtime.localBoundsTexture === null && runtime.localBoundsTextureVersion === -1 && valid(source), 'original clear resets cache');
check(runtime.computeLocalBoundsRectangle === compute && runtime.isLocalBoundsRectangleValid === valid && source[key] === runtime, 'all calls preserve callback and physical runtime identity');
record('cleared', out);
console.log(JSON.stringify({schema:'flight-cpp-original-sprite-bounds-controls/1', scope:'Original JavaScript only; native factory/image qualification remains separate.', checks:checks.length, labels:checks, cases, modules:[...modules.keys()].map(file=>({file:path.relative(flight,file),sha256:createHash('sha256').update(sourceText(file)).digest('hex')}))}, null, 2));
