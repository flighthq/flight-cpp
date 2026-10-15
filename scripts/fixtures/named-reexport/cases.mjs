import assert from 'node:assert/strict';
import { pathToFileURL } from 'node:url';
const api = await import(pathToFileURL(process.env.FLIGHT_CPP_COMPILER_ENTRY));
let checks = 0;
const item = (pkg, file, code) => [pkg, `packages/${pkg.slice('@flighthq/'.length)}/src/${file}.ts`, code];
function emit(items, edges) {
  const resolution = { schema:'flight-compiler-module-resolution/1', edges: edges.map(([specifier, target]) => ({specifier,target:{packageName:target[0],source:target[1]}})) };
  const modules = api.lowerTypeScriptSources(items.map(([packageName, source, contents]) => ({packageName,sourceFile:api.parseTypeScriptSource(`/flight/${source}`,contents),upstreamDirectory:'/flight'})),resolution).map(x=>x.module);
  const session = api.createCppCompilerBackend().createEmissionSession({moduleResolution:resolution, modules, options:{runtimeProfile:'flight-cpp'}});
  return session.emitModule(modules.find(x=>x.packageName==='@flighthq/scene3d'))[0].contents;
}
function contains(items, edges, expected) {
  const text=emit(items,edges);for(const line of expected)assert.ok(text.includes(line),line);checks++;return text;
}
const owner=item('@flighthq/types','owner','export const BillboardKind = "Billboard";');
const barrel=item('@flighthq/types','contract','export * from "./owner.ts";');
const facade=item('@flighthq/scene3d','billboard','export { BillboardKind } from "@flighthq/types/contract";');
contains([owner,facade],[['@flighthq/types/contract',owner]],['using flighthq_types::billboard_kind;']);
const edges=[['./owner.ts',owner],['@flighthq/types/contract',barrel]];
contains([owner,barrel,facade],edges,['using flighthq_types::billboard_kind;']);
contains([facade,barrel,owner],edges,['using flighthq_types::billboard_kind;']);
const vendor=item('@flighthq/vendor','constants','export let SharedValue = 1;');
const forwarding=item('@flighthq/types','contract','export { SharedValue } from "@flighthq/vendor/constants";');
const foreign=item('@flighthq/scene3d','billboard','export { SharedValue } from "@flighthq/types/contract";');
contains([vendor,forwarding,foreign],[['@flighthq/vendor/constants',vendor],['@flighthq/types/contract',forwarding]],['using flighthq_vendor::shared_value;']);
const left=item('@flighthq/types','left','export * from "./owner.ts";');
const right=item('@flighthq/types','right','export * from "./owner.ts";');
const diamond=item('@flighthq/types','contract','export * from "./left.ts"; export * from "./right.ts";');
const diamondText=contains([owner,left,right,diamond,facade],[['./owner.ts',owner],['./left.ts',left],['./right.ts',right],['@flighthq/types/contract',diamond]],['using flighthq_types::billboard_kind;']);
assert.equal(diamondText.match(/using flighthq_types::billboard_kind;/gu).length,1);checks++;
const other=item('@flighthq/types','other','export const BillboardKind = "Other";');
const ambiguous=item('@flighthq/types','contract','export * from "./owner.ts"; export * from "./other.ts";');
assert.throws(()=>emit([owner,other,ambiguous,facade],[['./owner.ts',owner],['./other.ts',other],['@flighthq/types/contract',ambiguous]]),/distinct declarations/);checks++;
const cycleA=item('@flighthq/types','a','export * from "./b.ts";');
const cycleB=item('@flighthq/types','b','export * from "./a.ts"; export const BillboardKind = "Billboard";');
contains([cycleA,cycleB,facade],[['./a.ts',cycleA],['./b.ts',cycleB],['@flighthq/types/contract',cycleA]],['using flighthq_types::billboard_kind;']);
const renamed=item('@flighthq/scene3d','billboard','export { BillboardKind as Other } from "@flighthq/types/contract";');
assert.throws(()=>emit([owner,renamed],[['@flighthq/types/contract',owner]]),/renamed value reexport/);checks++;
const shape=item('@flighthq/vendor','shape','export interface Shape { x: number }');
const shapeBarrel=item('@flighthq/types','contract','export * from "@flighthq/vendor/shape";');
const shapeFacade=item('@flighthq/scene3d','billboard','export { Shape } from "@flighthq/types/contract";');
contains([shape,shapeBarrel,shapeFacade],[['@flighthq/vendor/shape',shape],['@flighthq/types/contract',shapeBarrel]],['using flighthq_vendor::Shape;']);
const typeOnly=item('@flighthq/scene3d','billboard','export type { Shape as OtherShape } from "@flighthq/vendor/shape";');
contains([shape,typeOnly],[['@flighthq/vendor/shape',shape]],['using OtherShape = flighthq_vendor::Shape;']);
const lanes=item('@flighthq/types','owner','export interface SharedName { x: number } export const SharedName = 1;');
const laneBarrel=item('@flighthq/types','contract','export * from "./owner.ts";');
const laneFacade=item('@flighthq/scene3d','billboard','export { SharedName } from "@flighthq/types/contract";');
contains([lanes,laneBarrel,laneFacade],[['./owner.ts',lanes],['@flighthq/types/contract',laneBarrel]],['using flighthq_types::SharedName;','using flighthq_types::shared_name;']);
const absent=item('@flighthq/types','contract','export const Other = 1;');
assert.throws(()=>emit([absent,facade],[['@flighthq/types/contract',absent]]),/no resolved module declares/);checks++;
const namespaceOther=item('@flighthq/types','namespaceOther','export const Separate = 1;');
const namespaceSibling=item('@flighthq/types','contract','export * from "./owner.ts"; export * as Unrelated from "./namespaceOther.ts";');
contains([owner,namespaceOther,namespaceSibling,facade],[['./owner.ts',owner],['./namespaceOther.ts',namespaceOther],['@flighthq/types/contract',namespaceSibling]],['using flighthq_types::billboard_kind;']);
const unrelated=item('@flighthq/types','other','export const Other = 1;');
const renamedSibling=item('@flighthq/types','contract','export * from "./owner.ts"; export { Other as Alias } from "./other.ts";');
contains([owner,unrelated,renamedSibling,facade],[['./owner.ts',owner],['./other.ts',unrelated],['@flighthq/types/contract',renamedSibling]],['using flighthq_types::billboard_kind;']);
const starFacade=item('@flighthq/scene3d','billboard','export * from "@flighthq/types/contract";');
assert.throws(()=>emit([owner,namespaceOther,namespaceSibling,starFacade],[['./owner.ts',owner],['./namespaceOther.ts',namespaceOther],['@flighthq/types/contract',namespaceSibling]]),/namespace Unrelated/);checks++;
assert.throws(()=>emit([owner,unrelated,renamedSibling,starFacade],[['./owner.ts',owner],['./other.ts',unrelated],['@flighthq/types/contract',renamedSibling]]),/under the name Alias/);checks++;
// An unrelated declaration with the same source spelling changes allocation in this namespace.
// Read the declaring header's actual name instead of assuming the unsuffixed spelling.
{
  const resolution={schema:'flight-compiler-module-resolution/1',edges:edges.map(([specifier,target])=>({specifier,target:{packageName:target[0],source:target[1]}}))};
  const modules=api.lowerTypeScriptSources([owner,other,barrel,facade].map(([packageName,source,contents])=>({packageName,sourceFile:api.parseTypeScriptSource(`/flight/${source}`,contents),upstreamDirectory:'/flight'})),resolution).map(x=>x.module);
  const session=api.createCppCompilerBackend().createEmissionSession({moduleResolution:resolution,modules,options:{runtimeProfile:'flight-cpp'}});
  const declared=session.emitModule(modules.find(x=>x.source===owner[1]))[0].contents;
  const allocated=declared.match(/inline const flight::String ([A-Za-z_][A-Za-z_0-9]*) =/u)?.[1];
  assert.ok(allocated);assert.ok(session.emitModule(modules.find(x=>x.packageName==='@flighthq/scene3d'))[0].contents.includes(`using flighthq_types::${allocated};`));checks++;
}
// A local import/export forwards the leaf binding, including an aliased local import.
const importedForward = item('@flighthq/types', 'contract', 'import { BillboardKind as Local } from "./owner.ts"; export { Local as BillboardKind };');
contains([owner, importedForward, facade], [['./owner.ts', owner], ['@flighthq/types/contract', importedForward]], ['using flighthq_types::billboard_kind;']);
const importedDiamond = item('@flighthq/types', 'contract', 'export * from "./left.ts"; export * from "./right.ts";');
const importedLeft = item('@flighthq/types', 'left', 'import { BillboardKind } from "./owner.ts"; export { BillboardKind };');
const importedRight = item('@flighthq/types', 'right', 'import { BillboardKind as Local } from "./owner.ts"; export { Local as BillboardKind };');
contains([owner, importedLeft, importedRight, importedDiamond, facade], [['./owner.ts', owner], ['./left.ts', importedLeft], ['./right.ts', importedRight], ['@flighthq/types/contract', importedDiamond]], ['using flighthq_types::billboard_kind;']);
// A type-only route to a merged name may forward the type, never its value lane.
const typeRoute = item('@flighthq/types', 'contract', 'export type { SharedName } from "./owner.ts";');
const typeRouteText = contains([lanes, typeRoute, laneFacade], [['./owner.ts', lanes], ['@flighthq/types/contract', typeRoute]], ['using flighthq_types::SharedName;']);
assert.ok(!typeRouteText.includes('using flighthq_types::shared_name;'));
// Existing direct named function/class/enum exports retain their original declarations too.
for (const [name, declaration, target] of [
  ['QueryValue', 'export function QueryValue(): number { return 7; }', 'query_value'],
  ['Widget', 'export class Widget {}', 'Widget'],
  ['Color', 'export enum Color { Red, Blue }', 'Color'],
]) {
  const declarationOwner = item('@flighthq/types', 'owner', declaration);
  const declarationFacade = item('@flighthq/scene3d', 'billboard', `export { ${name} } from "@flighthq/types/contract";`);
  const text = contains([declarationOwner, declarationFacade], [['@flighthq/types/contract', declarationOwner]], [`using flighthq_types::${target};`]);
  assert.equal(text.split(`using flighthq_types::${target};`).length - 1, 1);
}
console.log(`${checks} named re-export provenance controls passed`);
if (process.env.FLIGHT_CPP_REEXPORT_NATIVE_OUTPUT) {
  const { mkdirSync, writeFileSync } = await import('node:fs');
  const path = await import('node:path');
  const output = process.env.FLIGHT_CPP_REEXPORT_NATIVE_OUTPUT;
  const nativeOwner = item('@flighthq/types','owner','export const BillboardKind = "Billboard"; export let SharedCounter = 1;');
  const nativeFacade = item('@flighthq/scene3d','billboard','export { BillboardKind, SharedCounter } from "@flighthq/types/contract";');
  const resolution={schema:'flight-compiler-module-resolution/1',edges:[['./owner.ts',nativeOwner],['@flighthq/types/contract',barrel]].map(([specifier,target])=>({specifier,target:{packageName:target[0],source:target[1]}}))};
  const modules=api.lowerTypeScriptSources([nativeOwner,barrel,nativeFacade].map(([packageName,source,contents])=>({packageName,sourceFile:api.parseTypeScriptSource(`/flight/${source}`,contents),upstreamDirectory:'/flight'})),resolution).map(x=>x.module);
  const session=api.createCppCompilerBackend().createEmissionSession({moduleResolution:resolution,modules,options:{runtimeProfile:'flight-cpp'}});
  for (const module of modules) {
    const directory=path.join(output,module.packageName==='@flighthq/types'?'types':'scene3d');
    mkdirSync(directory,{recursive:true});
    for (const file of session.emitModule(module)) writeFileSync(path.join(directory,file.path),file.contents);
  }
  writeFileSync(path.join(output,'consumer.cpp'),`#include "scene3d/billboard.hpp"
int main() {
  if (&flighthq_types::billboard_kind != &flighthq_scene3d::billboard_kind) return 1;
  if (&flighthq_types::shared_counter != &flighthq_scene3d::shared_counter) return 2;
  flighthq_scene3d::shared_counter = 9;
  return flighthq_types::shared_counter == 9 ? 0 : 3;
}
`);
}
