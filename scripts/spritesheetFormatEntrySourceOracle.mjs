import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

// Run untouched public parser bodies and their actual dependency bodies. Contract imports are
// resolved lazily through their original re-exports, so unrelated browser modules are not executed.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const args = Object.fromEntries(
  process.argv.slice(2).map((arg) => {
    const equals = arg.indexOf("=");
    assert.ok(equals > 0, "arguments use --name=value");
    return [arg.slice(0, equals), arg.slice(equals + 1)];
  }),
);
const { default: ts } = await import(
  pathToFileURL(
    path.resolve(
      args["--typescript"] ??
        path.join(root, ".dependencies/flight-compiler/node_modules/typescript/lib/typescript.js"),
    ),
  )
);
const flight = path.resolve(args["--flight"] ?? path.join(root, ".dependencies/flight"));
const packages = path.join(flight, "packages");
const parsedFiles = new Map(),
  modules = new Map(),
  origins = new Map();
function sourceFile(file) {
  if (!fs.existsSync(file)) file = file.replace(/\.js$/u, ".ts");
  assert.ok(fs.existsSync(file), "missing original source: " + file);
  return file;
}
function resolve(specifier, from) {
  return sourceFile(
    specifier.startsWith("@flighthq/")
      ? path.join(
          packages,
          specifier.slice("@flighthq/".length).replace(/\/contract$/u, "/src/contract.ts"),
        )
      : path.resolve(path.dirname(from), specifier),
  );
}
function ast(file) {
  if (!parsedFiles.has(file))
    parsedFiles.set(
      file,
      ts.createSourceFile(file, sourceText(file), ts.ScriptTarget.Latest, true),
    );
  return parsedFiles.get(file);
}
function origin(file, name, seen = new Set()) {
  const key = file + "#" + name;
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
            ? origin(resolve(statement.moduleSpecifier.text, file), local, seen)
            : { file, name };
          if (result) {
            origins.set(key, result);
            return result;
          }
        }
      } else if (!statement.exportClause && statement.moduleSpecifier) {
        const result = origin(resolve(statement.moduleSpecifier.text, file), name, seen);
        if (result) {
          origins.set(key, result);
          return result;
        }
      }
    }
    if (!statement.modifiers?.some((modifier) => modifier.kind === ts.SyntaxKind.ExportKeyword))
      continue;
    const names = ts.isVariableStatement(statement)
      ? statement.declarationList.declarations.map((declaration) =>
          declaration.name.getText(ast(file)),
        )
      : statement.name
        ? [statement.name.text]
        : [];
    if (
      names.includes(name) &&
      !ts.isInterfaceDeclaration(statement) &&
      !ts.isTypeAliasDeclaration(statement)
    ) {
      const result = { file, name };
      origins.set(key, result);
      return result;
    }
  }
  return undefined;
}
function proxy(file) {
  return new Proxy(
    {},
    {
      get(_target, name) {
        if (name === "__esModule") return true;
        const target = origin(file, name);
        assert.ok(target, `missing original export ${String(name)} from ${file}`);
        return load(target.file)[target.name];
      },
    },
  );
}
function load(file) {
  if (modules.has(file)) return modules.get(file).exports;
  const module = { exports: {} };
  modules.set(file, module);
  const code = ts.transpileModule(sourceText(file), {
    compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS },
  }).outputText;
  new Function("require", "module", "exports", code)(
    (specifier) => proxy(resolve(specifier, file)),
    module,
    module.exports,
  );
  return module.exports;
}
const entryFile = path.join(packages, "spritesheet-formats/src/spritesheetDetect.ts");
const publicFile = path.join(packages, "types/src/SpritesheetFormatDescriptor.ts");
const input = fs.readFileSync(entryFile, "utf8");
const privateBlock = `interface FormatEntry {
  detect: (text: string) => boolean;
  parse: (text: string, options: SpritesheetParseOptions) => SpritesheetData;
}`;
const publicImport = "  SpritesheetFormatEntry,\n";
const registerBlock = `  entry: {
    detect: (text: string) => boolean;
    parse: (text: string, options: SpritesheetParseOptions) => SpritesheetData;
  },`;
const resultBlock = `Readonly<{
  detect: (text: string) => boolean;
  parse: (text: string, options: SpritesheetParseOptions) => SpritesheetData;
}>`;
const before = input
  .replace(publicImport, "")
  .replace("type FormatEntry = SpritesheetFormatEntry;", privateBlock)
  .replace("  entry: FormatEntry,", registerBlock)
  .replace("Readonly<FormatEntry>", resultBlock);
assert.ok(
  before.includes(privateBlock) && before.includes(registerBlock) && before.includes(resultBlock),
  "all three exact original entry declarations",
);
const after = before
  .replace("  SpritesheetFormatDescriptor,\n", "  SpritesheetFormatDescriptor,\n" + publicImport)
  .replace(privateBlock, "type FormatEntry = SpritesheetFormatEntry;")
  .replace(registerBlock, "  entry: FormatEntry,")
  .replace(resultBlock, "Readonly<FormatEntry>");
const parsedBefore = ts.createSourceFile(entryFile, before, ts.ScriptTarget.Latest, true);
const publicAst = ts.createSourceFile(
  publicFile,
  fs.readFileSync(publicFile, "utf8"),
  ts.ScriptTarget.Latest,
  true,
);
const privateType = parsedBefore.statements.find(
  (node) => ts.isInterfaceDeclaration(node) && node.name.text === "FormatEntry",
);
const publicType = publicAst.statements.find(
  (node) => ts.isInterfaceDeclaration(node) && node.name.text === "SpritesheetFormatEntry",
);
assert.ok(privateType && publicType && !privateType.heritageClauses && !publicType.heritageClauses);
const members = (node, file) =>
  node.members.map((member) => member.getText(file).replace(/\s+/gu, ""));
assert.deepEqual(
  members(privateType, parsedBefore),
  members(publicType, publicAst),
  "exact same declared fields/callback types",
);
const registerFunction = parsedBefore.statements.find(
  (node) => ts.isFunctionDeclaration(node) && node.name?.text === "registerSpritesheetFormat",
);
const getFunction = parsedBefore.statements.find(
  (node) => ts.isFunctionDeclaration(node) && node.name?.text === "getSpritesheetFormat",
);
const registerType = registerFunction?.parameters.find(
  (node) => node.name.getText(parsedBefore) === "entry",
)?.type;
const resultType = getFunction?.type?.types?.find(
  (node) => ts.isTypeReferenceNode(node) && node.typeName.getText(parsedBefore) === "Readonly",
)?.typeArguments?.[0];
assert.ok(
  registerType &&
    ts.isTypeLiteralNode(registerType) &&
    resultType &&
    ts.isTypeLiteralNode(resultType),
  "original public entry parameter and readonly result shapes",
);
assert.deepEqual(
  members(registerType, parsedBefore),
  members(publicType, publicAst),
  "public registration has identical fields and callback types",
);
assert.deepEqual(
  members(resultType, parsedBefore),
  members(publicType, publicAst),
  "public readonly result has identical fields and callback types",
);
const javascript = (source) =>
  ts.transpileModule(source, {
    compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022 },
  }).outputText;
assert.equal(
  javascript(before),
  javascript(after),
  "complete original module JavaScript byte identity",
);
function sourceText(file) {
  return file === entryFile ? after : fs.readFileSync(file, "utf8");
}
let checks = 4;
const sourceTests = [];
function expect(actual) {
  return {
    toBe(value) {
      assert.equal(actual, value);
      ++checks;
    },
    toBeNull() {
      assert.equal(actual, null);
      ++checks;
    },
    toEqual(value) {
      assert.deepEqual(actual, value);
      ++checks;
    },
    toHaveLength(value) {
      assert.equal(actual.length, value);
      ++checks;
    },
    toContain(value) {
      assert.ok(actual.includes(value));
      ++checks;
    },
    toBeLessThan(value) {
      assert.ok(actual < value);
      ++checks;
    },
    not: {
      toBeNull() {
        assert.notEqual(actual, null);
        ++checks;
      },
      toContain(value) {
        assert.ok(!actual.includes(value));
        ++checks;
      },
    },
  };
}
const run = (name, callback) => {
  callback();
  sourceTests.push(name);
};
run.each = (cases) => (name, callback) =>
  cases.forEach((args, index) =>
    run(name + " #" + index, () => callback(...(Array.isArray(args) ? args : [args]))),
  );
const testsFile = path.join(packages, "spritesheet-formats/src/spritesheetDetect.test.ts");
const testCode = ts.transpileModule(fs.readFileSync(testsFile, "utf8"), {
  compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS },
}).outputText;
const output = {};
new Function(
  "require",
  "exports",
  "describe",
  "it",
  "expect",
  testCode +
    "\nexports.fixtures={TEXTURE_PACKER_JSON,ASEPRITE_JSON,STARLING_XML,LIBGDX_ATLAS,COCOS_PLIST,PARTICLE_DESIGNER_PLIST,COCOS_PLIST_FORMAT_0};",
)(
  (specifier) => proxy(resolve(specifier, testsFile)),
  output,
  (_name, callback) => callback(),
  run,
  expect,
);
const digest = (value) => createHash("sha256").update(value).digest("hex");
const report = {
  scope:
    "source erased type-alias preparation; complete original spritesheetDetect public tests and actual dependency bodies; native proof separate",
  sourceTests,
  checks,
  canonicalEntry: {
    sameFieldsAndCallbacks: true,
    publicParameterAndReadonlyResultIdentical: true,
    beforeSha256: digest(before),
    afterSha256: digest(after),
    completeJavaScriptSha256: digest(javascript(before)),
    byteIdenticalJavaScript: true,
  },
  fixtures: output.fixtures,
  sourceHashes: Object.fromEntries(
    [...parsedFiles.keys()].map((file) => [
      path.relative(packages, file),
      digest(fs.readFileSync(file)),
    ]),
  ),
};
if (args["--output"])
  fs.writeFileSync(path.resolve(args["--output"]), JSON.stringify(report, null, 2) + "\n");
console.log(JSON.stringify(report, null, 2));
