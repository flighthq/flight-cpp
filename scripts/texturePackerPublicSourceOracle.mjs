import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// Run untouched public parser bodies and their actual dependency bodies. Contract imports are
// resolved lazily through their original re-exports, so unrelated browser modules are not executed.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const equals = arg.indexOf('=');
  assert.ok(equals > 0, 'arguments use --name=value');
  return [arg.slice(0, equals), arg.slice(equals + 1)];
}));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'] ??
  path.join(root, '.dependencies/flight-compiler/node_modules/typescript/lib/typescript.js'))));
const packages = path.join(root, '.dependencies/flight/packages');
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
const parserFile = path.join(packages, 'textureatlas-formats/src/texturePackerAtlasParse.ts');
const api = load(parserFile);
let checks = 0;
const sourceTests = [];
const check = (condition, message) => { assert.ok(condition, message); ++checks; };
function expect(actual) {
  return {
    toBe(expected) { assert.equal(actual, expected); ++checks; },
    toBeDefined() { assert.notEqual(actual, undefined); ++checks; },
    toBeNull() { assert.equal(actual, null); ++checks; },
    toHaveLength(length) { assert.equal(actual.length, length); ++checks; },
    toContain(expected) { assert.ok(actual.includes(expected)); ++checks; },
    toThrow() { assert.throws(actual); ++checks; },
    not: { toThrow() { assert.doesNotThrow(actual); ++checks; } },
  };
}
const testsFile = path.join(packages, 'textureatlas-formats/src/texturePackerAtlasParse.test.ts');
new Function('require', 'exports', 'describe', 'it', 'expect', ts.transpileModule(fs.readFileSync(testsFile, 'utf8'), {
  compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS },
}).outputText)(specifier => proxy(resolve(specifier, testsFile)), {}, (_name, run) => run(),
  (name, run) => { run(); sourceTests.push(name); }, expect);
const atlasApi = proxy(path.join(packages, 'textureatlas/src/contract.ts'));
const atlas = atlasApi.createTextureAtlas();
atlas.regions.push(atlasApi.createTextureAtlasRegion({ name: 'before' }));
const regions = atlas.regions;
atlas.imageName = 'before.png'; atlas.imageWidth = 8; atlas.imageHeight = 9; atlas.scale = 2;
let thrown;
try { api.parseTexturePackerAtlasJson('[]', atlas); } catch (error) { thrown = error; }
check(thrown instanceof TypeError, 'valid JSON array root throws at original Object.entries(frames) outside syntax catch');
check(atlas.regions === regions && regions.length === 0, 'original clear retains and empties region storage before shape error');
check(atlas.imageName === null && atlas.imageWidth === 0 && atlas.imageHeight === 0 && atlas.scale === 1,
  'original page reset occurs before shape error');
const report = { scope: 'complete original source modules and upstream tests; native proof separate',
  sourceTests, checks, nonObjectArrayRoot: { throws: 'TypeError', regionsSameOwner: true, regionsLength: 0,
    imageName: null, imageWidth: 0, imageHeight: 0, scale: 1 },
  sourceHashes: Object.fromEntries([...parsedFiles.keys()].map(file => [path.relative(packages, file),
    createHash('sha256').update(fs.readFileSync(file)).digest('hex')])) };
if (args['--output']) fs.writeFileSync(path.resolve(args['--output']), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify(report, null, 2));
