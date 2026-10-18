import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const args = Object.fromEntries(process.argv.slice(2).map(argument => {
  const equals = argument.indexOf('=');
  assert.ok(equals > 0, 'arguments use --name=value');
  return [argument.slice(0, equals), argument.slice(equals + 1)];
}));
const flight = path.resolve(args['--flight'] ?? path.join(root, '.dependencies/flight'));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'] ??
  path.join(root, '.dependencies/flight-compiler/node_modules/typescript/lib/typescript.js'))));
const digest = text => createHash('sha256').update(text).digest('hex');
const replacements = [
  ['parseInt(numStr, 10)', 'parseInt(numStr!, 10)'],
  ['groups.get(base)', 'groups.get(base!)'],
  ['groups.set(base,', 'groups.set(base!,'],
];
const reports = [];
let checks = 0;
for (const name of ['libgdxAtlasParse.ts', 'starlingParse.ts']) {
  const relative = 'packages/spritesheet-formats/src/' + name;
  const input = fs.readFileSync(path.join(flight, relative), 'utf8');
  let before = input, after = input;
  for (const [plain, asserted] of replacements) {
    assert.equal(input.includes(plain), !input.includes(asserted), relative + ': exact assertion site');
    before = before.replace(asserted, plain);
    after = after.replace(plain, asserted);
    ++checks;
  }
  const javascript = source => ts.transpileModule(source, {
    compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ES2022 },
  }).outputText;
  assert.equal(javascript(before), javascript(after), relative + ': complete module JavaScript is byte-identical');
  ++checks;
  assert.ok(before.includes("if (!match) continue;\n    const [, base, numStr] = match;"), 'original dominating guard and destructuring');
  const pattern = /const match = noExt\.match\((\/\^.*?\/)[)]/u.exec(before)?.[1];
  assert.equal(pattern, '/^(.*?)_?(\\d+)$/');
  checks += 2;
  const regex = new RegExp(pattern.slice(1, -1));
  for (const name of ['', '0', '00', '_1', 'run1', 'run_12', 'a b2', 'тест9', '🦊7', 'none', 'run_', '__2', '-1', 'x12.png', 'a\nb1']) {
    const match = name.replace(/\.\w+$/u, '').match(regex);
    if (match) {
      assert.equal(typeof match[1], 'string', 'mandatory base participates, including empty base');
      assert.match(match[2], /^\d+$/u, 'mandatory digits participate');
      checks += 2;
    } else {
      assert.equal(match, null, 'original guard skips a missing match');
      ++checks;
    }
  }
  reports.push({ path: relative, assertions: 3, beforeSha256: digest(before), afterSha256: digest(after),
    completeJavaScriptSha256: digest(javascript(after)), byteIdenticalJavaScript: true });
}
const report = { scope: 'source-equivalence preparation only; no native or normal-header success', checks, modules: reports };
if (args['--output']) fs.writeFileSync(path.resolve(args['--output']), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify(report, null, 2));
