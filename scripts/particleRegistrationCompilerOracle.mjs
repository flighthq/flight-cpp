import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// Direct original-source qualification of the bounded compiler extensions. Does not generate an SDK.
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const at = arg.indexOf('='); assert.ok(at > 0, 'expected explicit --compiler --typescript --cxx --output paths');
  return [arg.slice(0, at), arg.slice(at + 1)];
}));
assert.ok(args['--compiler'] && args['--typescript'] && args['--cxx'] && args['--output']);
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const backendUrl = pathToFileURL(path.resolve(args['--compiler']));
const { emitIrModuleCpp } = await import(backendUrl);
const { lowerTypeScriptSource } = await import(new URL('../../compiler-semantic/src/index.js', backendUrl));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'])));
const original = ts.createSourceFile('register.ts', fs.readFileSync(path.join(root, '.dependencies/flight/packages/particles-formats/src/registerBuiltInParticleFormats.ts'), 'utf8'), ts.ScriptTarget.Latest, true);
const helper = original.statements.find(node => ts.isFunctionDeclaration(node) && node.name?.text === 'parseJsonObject');
assert.ok(helper);
const boundary = fs.readFileSync(path.join(root, 'scripts/fixtures/particleRegistrationJsonBoundary.ts'), 'utf8');
assert.ok(boundary.includes(helper.getText(original)), 'must execute the exact untouched private helper');
const detector = fs.readFileSync(path.join(root, 'scripts/fixtures/particleRegistrationLibgdxDetector.ts'), 'utf8');
assert.ok(original.text.includes("text.trimStart().split('\\n')[0]?.trim() === 'Particle Effect'"));
const reads = `
export function marker(text:string):unknown{const obj=parseJsonObject(text);return obj!==null?obj.marker:undefined;}
export function indexedMarker(text:string):unknown{const obj=parseJsonObject(text);return obj===null?undefined:obj['marker'];}
export function hasMarker(text:string):boolean{const obj=parseJsonObject(text);return obj!==null&&obj.marker!==undefined;}
export function trimAt(values:()=>string[],index:()=>number):string|undefined{return values()[index()]?.trim();}
`;
const source = boundary + '\n' + detector + reads;
const scratch = fs.mkdtempSync(path.join(os.tmpdir(), 'particle-registration-compiler-'));
try {
  const filename = path.join(scratch, 'registration.ts'); fs.writeFileSync(filename, source);
  const program = ts.createProgram([filename], { strict: true, noEmit: true, types: [], target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.ESNext, skipLibCheck: true });
  assert.deepEqual(ts.getPreEmitDiagnostics(program).map(d => ts.flattenDiagnosticMessageText(d.messageText, '\n')), []);
  const exports = {};
  new Function('exports', ts.transpileModule(source, { compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 } }).outputText)(exports);
  const roots = [['null', false], ['0', false], ['true', false], ['"text"', false], ['[]', false], ['{}', true], ['{"marker":3}', true], ['{"marker":null}', true], ['bad', false]];
  for (const [input, expected] of roots) assert.equal(exports.acceptsObject(input), expected);
  const detectors = [['', false], ['Particle Effect', true], [' \tParticle Effect \r\nmore', true], ['x\nParticle Effect', false], ['#comment\nParticle Effect', false], ['Particle EffectX', false], ['\r\nParticle Effect', true]];
  for (const [input, expected] of detectors) assert.equal(exports.detect(input), expected);
  const values = [['null', undefined], ['bad', undefined], ['[]', undefined], ['{}', undefined], ['{"marker":null}', null], ['{"marker":3}', 3], ['{"marker":false}', false], ['{"marker":"x"}', 'x']];
  for (const [input, expected] of values) { assert.deepEqual(exports.marker(input), expected); assert.deepEqual(exports.indexedMarker(input), expected); assert.equal(exports.hasMarker(input), expected !== undefined); }
  for (const index of [-1, 0, 1, 0.5, NaN]) {
    const events = []; const expected = index === 0 ? 'x' : undefined;
    assert.equal(exports.trimAt(() => { events.push('receiver'); return [' x ']; }, () => { events.push('index'); return index; }), expected);
    assert.deepEqual(events, ['receiver', 'index']);
  }
  const sentinel = {}; let calls = 0;
  assert.throws(() => exports.trimAt(() => { throw sentinel; }, () => { ++calls; return 0; }), error => error === sentinel); assert.equal(calls, 0);
  assert.throws(() => exports.trimAt(() => [], () => { throw sentinel; }), error => error === sentinel);
  const lowered = lowerTypeScriptSource(ts.createSourceFile('/flight/packages/example/src/registration.ts', source, ts.ScriptTarget.Latest, true), { packageName: '@flighthq/example', upstreamDirectory: '/flight' });
  assert.deepEqual(lowered.diagnostics, []);
  const emitted = emitIrModuleCpp(lowered.module, { runtimeProfile: 'flight-cpp' }).contents;
  assert.ok(emitted.includes('std::optional<flight::NamedProperties> parse_json_object'));
  assert.equal((emitted.match(/flight::Json::parse\(text\)/g) ?? []).length, 1);
  assert.equal((emitted.match(/flight::named_properties\(value\)/g) ?? []).length, 1);
  assert.ok(emitted.includes('obj.value().get('));
  assert.ok(emitted.includes('values().get(index())'));
  fs.writeFileSync(path.join(scratch, 'registration.hpp'), emitted);
  const cpp = `#include "registration.hpp"
#include <cassert>
#include <limits>
#include <vector>
int main(){using namespace flighthq_example;
  for(const char* text:{"null","0","true","\\\"text\\\"","[]","bad"}){assert(!accepts_object(text));assert(marker(text).is_undefined());}
  for(const char* text:{"{}","{\\\"marker\\\":3}","{\\\"marker\\\":null}"})assert(accepts_object(text));
  assert(marker("{}").is_undefined());assert(marker("{\\\"marker\\\":null}").is_null());assert(marker("{\\\"marker\\\":3}").as_number()==3);
  assert(indexed_marker("{\\\"marker\\\":3}").as_number()==3);assert(has_marker("{\\\"marker\\\":null}"));assert(!has_marker("{}"));
  assert(!detect(""));assert(detect("Particle Effect"));assert(detect(" \\tParticle Effect \\r\\nmore"));assert(!detect("x\\nParticle Effect"));assert(!detect("#comment\\nParticle Effect"));assert(!detect("Particle EffectX"));assert(detect("\\r\\nParticle Effect"));
  for(double index:{-1.0,0.0,1.0,0.5,std::numeric_limits<double>::quiet_NaN()}){std::vector<int> events;auto value=trim_at([&]{events.push_back(1);return flight::Array<flight::String>{" x "};},[&]{events.push_back(2);return index;});assert((events==std::vector<int>{1,2}));assert(value.has_value()==(index==0));if(value)assert(*value=="x");}
  int calls=0;try{trim_at([]()->flight::Array<flight::String>{throw 17;},[&]{++calls;return 0;});assert(false);}catch(int error){assert(error==17&&calls==0);}
  try{trim_at([]{return flight::Array<flight::String>{};},[]()->double{throw 23;});assert(false);}catch(int error){assert(error==23);}
  std::weak_ptr<flight::JsonObject> lifetime;
  {auto result=parse_json_object("{\\\"marker\\\":3,\\\"present\\\":null}");assert(result);auto owner=std::static_pointer_cast<flight::JsonObject>(result->shared_owner()->native_object());lifetime=owner;assert(result->shared_owner()->native_type()==typeid(flight::JsonObject));owner->set("marker",flight::JsonValue(7));assert(result->get("marker").as_number()==7);assert(result->get("present").is_null()&&result->get("missing").is_undefined());auto alias=*result;assert(alias.shared_owner()==result->shared_owner());result.reset();owner.reset();assert(!lifetime.expired()&&alias.get("marker").as_number()==7);}
  assert(lifetime.expired());
}
`;
  fs.writeFileSync(path.join(scratch, 'registration.cpp'), cpp);
  const binary = path.join(scratch, 'registration');
  const build = spawnSync(path.resolve(args['--cxx']), ['-std=c++20', '-ffp-contract=off', '-I' + path.join(root, 'overrides/include'), '-I' + path.join(root, 'include'), '-I' + path.join(root, 'generated/include'), path.join(scratch, 'registration.cpp'), '-o', binary], { encoding: 'utf8' });
  assert.equal(build.status, 0, build.stderr || build.stdout);
  const run = spawnSync(binary, [], { encoding: 'utf8' }); assert.equal(run.status, 0, run.stderr || run.stdout);
  const report = { originalHelperSha256: createHash('sha256').update(helper.getText(original)).digest('hex'), sourceChecks: { roots: roots.length, detector: detectors.length, reads: values.length * 3, indexedEvaluation: 7 }, strictTypecheck: 'pass', productionEmission: 'pass', nativeBuild: 'pass', nativeRun: 'pass', originalOwnerMutationAbsenceLifetime: 'pass', emittedSha256: createHash('sha256').update(emitted).digest('hex'), normalSdk: false, scope: 'Original private helper and detector plus bounded read/evaluation controls only. No full registration, installed callbacks or header promotion.' };
  const output = path.resolve(args['--output']); fs.mkdirSync(path.dirname(output), { recursive: true }); fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n');
  console.log(JSON.stringify(report));
} finally { fs.rmSync(scratch, { recursive: true, force: true }); }
