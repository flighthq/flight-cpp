import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// A synthetic semantic control emitted by the production compiler, not a replacement parser.
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const at = arg.indexOf('=');
  assert.ok(at > 0, 'expected --compiler=backend.js --typescript=typescript.js --cxx=compiler --output=report.json');
  return [arg.slice(0, at), arg.slice(at + 1)];
}));
assert.ok(args['--compiler'] && args['--typescript'] && args['--cxx'], 'explicit compiler, TypeScript and C++ tool paths required');
const backendUrl = pathToFileURL(path.resolve(args['--compiler']));
const { emitIrModuleCpp } = await import(backendUrl);
const { lowerTypeScriptSource } = await import(new URL('../../compiler-semantic/src/index.js', backendUrl));
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'])));
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const source = String.raw`interface Config{size:number} interface Doc{size:number} interface Result{config:Config;diagnostics:number[];document:Doc} interface Options{size?:number} interface ParseOptions extends Options{width?:number} interface Codec{parse(text:string,options?:Readonly<ParseOptions>):Required<{config:Config;diagnostics:number[]}>} export function parse(text:string,options?:Options):Result{if(text==='bad')throw new Error('bad');return {config:{size:options?.size??1},diagnostics:[],document:{size:2}}} export function other(text:string,options?:Options):Result{if(text==='bad')throw new Error('bad');return {config:{size:options?.size??1},diagnostics:[],document:{size:2}}} export const codec:Codec={parse};export const second:Codec={parse:other};export const alias=parse;export function same():boolean{return codec.parse===parse}export function different():boolean{return codec.parse!==other}export function aliasSame():boolean{return codec.parse===alias}export function copiesSame():boolean{return codec.parse===second.parse}`;
const native = String.raw`#include "emitted.hpp"
#include <iostream>
#include <type_traits>
namespace api = flighthq_example;
using Read = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<api::ParseOptions>>>>;
using Size = flight::RowKey<"size">;
int failures = 0;
void check(bool value,const char* message){if(!value){++failures;std::cerr<<message<<'\n';}}
struct Cell final:flight::TypedRowCell<std::optional<double>>{
 std::optional<double> value{3};int reads=0;bool throws=false;const std::shared_ptr<int> sentinel=std::make_shared<int>(2);
 std::optional<double>& get()override{++reads;if(throws)throw sentinel;return value;}
 void set(std::optional<double> v)override{value=v;}
};
int main(){
 using Native=flight::Ref<api::Result>(*)(flight::String,std::optional<flight::Ref<api::Options>>);
 static_assert(std::is_same_v<decltype(&api::parse),Native>);
 auto native=flight::make_ref<api::Options>();native->size=4;
 check(api::parse("ok",native)->config->size==4,"native public ABI");
 auto contextual_native=flight::make_ref<api::ParseOptions>();contextual_native->size=5;
 const Read native_view(contextual_native);
 check(api::codec->parse("ok",native_view).shared_exact_object<api::Result>()->config->size==5,"native options owner through contextual view");
 contextual_native->size=7;
 check(api::codec->parse("ok",native_view).shared_exact_object<api::Result>()->config->size==7,"native alias updates stay live");
 const auto bag=flight::make_structural_ref<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<api::ParseOptions>>>>>(flight::row_field<Size>(std::optional<double>{2}));
 check(!bag.shared_exact_object<api::ParseOptions>(),"actual schema-only options");
 auto result=api::codec->parse("ok",bag);auto original=result.shared_exact_object<api::Result>();
 check(original&&original->config->size==2&&original->document->size==2,"same original result and extra document");
 auto replacement=flight::make_ref<api::Config>();replacement->size=44;
 flight::row_set<flight::RowKey<"config">>(result,replacement);
 check(original->config==replacement&&original->document->size==2,"result writes reach original owner and retain document");
 check(api::codec->parse("ok",std::nullopt).shared_exact_object<api::Result>()->config->size==1,"absent options");
 const auto empty=flight::make_structural_ref<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<api::ParseOptions>>>>>();
 const auto undefined=flight::make_structural_ref<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<api::ParseOptions>>>>>(flight::row_field<Size>(std::optional<double>{}));
 check(api::codec->parse("ok",empty).shared_exact_object<api::Result>()->config->size==1&&api::codec->parse("ok",undefined).shared_exact_object<api::Result>()->config->size==1,"absent field and present undefined defaults");
 auto cell=std::make_shared<Cell>();auto owner=std::make_shared<flight::RowOwner>();owner->set_named_cell("size",cell);const auto options=Read::from_owner(owner);
 check(api::codec->parse("ok",options).shared_exact_object<api::Result>()->config->size==3&&cell->reads==1,"accessor at original read time exactly once");
 cell->value=6;check(api::codec->parse("ok",options).shared_exact_object<api::Result>()->config->size==6&&cell->reads==2,"live accessor value");
 bool invalid=false;try{(void)api::codec->parse("bad",options);}catch(const flight::Error&){invalid=true;}
 check(invalid&&cell->reads==2,"original validation before option read");
 cell->throws=true;bool same=false;try{(void)api::codec->parse("ok",options);}catch(const std::shared_ptr<int>&e){same=e==cell->sentinel;}
 check(same&&cell->reads==3,"exact source accessor exception");
 check(api::same()&&api::different()&&api::alias_same()&&!api::copies_same(),"emitted named/bare/alias/different-function identity");
 return failures?1:0;
}
`;
const scratch = fs.mkdtempSync(path.join(os.tmpdir(), 'particle-codec-options-native-'));
try {
  const file = ts.createSourceFile('/flight/packages/example/src/options.ts', source, ts.ScriptTarget.Latest, true);
  const lowered = lowerTypeScriptSource(file, { packageName: '@flighthq/example', upstreamDirectory: '/flight' });
  assert.deepEqual(lowered.diagnostics, []);
  const emitted = emitIrModuleCpp(lowered.module, { runtimeProfile: 'flight-cpp' }).contents;
  fs.writeFileSync(path.join(scratch, 'emitted.hpp'), emitted);
  fs.writeFileSync(path.join(scratch, 'probe.cpp'), native);
  const binary = path.join(scratch, 'probe');
  const compile = spawnSync(path.resolve(args['--cxx']), ['-std=c++20', '-ffp-contract=off',
    '-I' + path.join(root, 'overrides/include'), '-I' + path.join(root, 'include'),
    '-I' + path.join(root, 'generated/include'), path.join(scratch, 'probe.cpp'), '-o', binary], { encoding: 'utf8' });
  assert.equal(compile.status, 0, compile.stderr || compile.stdout);
  const run = spawnSync(binary, [], { encoding: 'utf8' });
  assert.equal(run.status, 0, run.stderr || run.stdout);
  const report = { result: 'pass', compilerEmittedSyntheticControl: true, nativePublicAbi: true,
    sameOptionsOwnerAndReadTime: true, schemaOnlyLiteral: true, originalResultAndExtraDocument: true,
    originalNamedFunctionIdentity: true, exactThrownOwnerAndEffects: true,
    emittedSha256: createHash('sha256').update(emitted).digest('hex'),
    scope: 'Synthetic compiler-emitted native control only; no installed particle callback or registry claim.' };
  const output = path.resolve(args['--output'] ?? 'out/particle-codec-options-native.json');
  fs.mkdirSync(path.dirname(output), { recursive: true });
  fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n');
  console.log(JSON.stringify(report));
} finally { fs.rmSync(scratch, { recursive: true, force: true }); }
