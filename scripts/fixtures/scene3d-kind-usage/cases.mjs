import assert from 'node:assert/strict';
import { realpathSync } from 'node:fs';
import { register } from 'node:module';
import path from 'node:path';
import { test } from 'node:test';
import { pathToFileURL } from 'node:url';
const flightDirectory=realpathSync(process.env.FLIGHT_KIND_SOURCE);
register(pathToFileURL(process.env.FLIGHT_KIND_LOADER),import.meta.url,{data:{flightDirectory,typescriptPath:process.env.FLIGHT_KIND_TYPESCRIPT,mutation:process.env.FLIGHT_KIND_MUTATION}});
const source=(pkg,file)=>pathToFileURL(path.join(flightDirectory,'packages',pkg,'src',`${file}.ts`));
const {createScene3DKindUsage:create,getScene3DKindUsage:collect,initializeScene3DKindUsage:initialize}=await import(source('scene3d','sceneKindUsage'));
const {createScene3D}=await import(source('scene3d','scene'));
const {createMesh}=await import(source('scene3d','mesh'));
const {createNode3D}=await import(source('scene3d','sceneNode'));
const {createBoxMeshGeometry}=await import(source('mesh','meshGeometryBuilders'));
const {addNodeChild}=await import(source('node','contract'));
const {createBlinnPhongMaterial,createUnlitMaterial}=await import(source('materials','contract'));
const {createMaterial3D}=await import(source('materials','material3d'));
const {createRimModifier,createShadedMaterial}=await import(source('shading','contract'));
const {createTexture}=await import(source('texture','texture'));
const {EntityRuntimeKey,ImageResourceReferenceKind,MeshKind,Node3DKind,ResourceResolutionState}=await import(source('types','contract'));
const mesh=(slots)=>createMesh(createBoxMeshGeometry(),slots);
function sceneWith(material) { const scene=createScene3D();addNodeChild(scene.root,mesh([material]));return scene; }
function kinds(material) {const usage=create();collect(usage,sceneWith(material));return usage;}
const ref=(mimeType='image/png')=>({[EntityRuntimeKey]:undefined,alphaType:'straight',bytes:new Uint8Array([1,2,3]),failure:null,kind:ImageResourceReferenceKind.Embedded,mimeType,state:ResourceResolutionState.Unresolved});
const lists=['materialKinds','modifierKinds','nodeKinds','resourceMimeTypes','textureSourceKinds'];
// Eleven cases port sceneKindUsage.test.ts at the locked Flight revision.
test('starts with five empty lists and the declared runtime cell',()=>{const usage=create();assert.equal(Object.hasOwn(usage,EntityRuntimeKey),true);for(const key of lists)assert.deepEqual(usage[key],[]);});
test('material kinds sort and deduplicate across meshes',()=>{const scene=createScene3D();for(const value of [createUnlitMaterial(),createUnlitMaterial(),createBlinnPhongMaterial()])addNodeChild(scene.root,mesh([value]));const usage=create();collect(usage,scene);assert.deepEqual(usage.materialKinds,['BlinnPhongMaterial','UnlitMaterial']);});
test('walk includes non-mesh groups',()=>{const scene=createScene3D(),group=createNode3D(Node3DKind);addNodeChild(group,mesh([createUnlitMaterial()]));addNodeChild(scene.root,group);const usage=create();collect(usage,scene);assert.deepEqual(usage.nodeKinds,[MeshKind,Node3DKind]);});
test('reads a shaded modifier stack structurally',()=>{const rim=createRimModifier({color:0x49d8ffff,intensity:.72,power:2.4}),material=createShadedMaterial();material.modifiers=[rim];const usage=kinds(material);assert.deepEqual(usage.modifierKinds,[rim.kind]);assert.deepEqual(usage.materialKinds,['ShadedMaterial']);});
test('material without a stack has no modifier kinds',()=>assert.deepEqual(kinds(createUnlitMaterial()).modifierKinds,[]));
test('null material slots are skipped',()=>{const scene=createScene3D();addNodeChild(scene.root,mesh([null,createUnlitMaterial()]));const usage=create();collect(usage,scene);assert.deepEqual(usage.materialKinds,['UnlitMaterial']);});
test('consumed resource contributes image kind and MIME',()=>{const resource=ref(),map=createTexture({resource}),scene=createScene3D();scene.resources.push(resource);addNodeChild(scene.root,mesh([createUnlitMaterial({baseColorMap:map})]));const usage=create();collect(usage,scene);assert.deepEqual(usage.textureSourceKinds,['image']);assert.deepEqual(usage.resourceMimeTypes,['image/png']);});
test('unconsumed resource is inert',()=>{const scene=createScene3D();scene.resources.push(ref());const usage=create();collect(usage,scene);assert.deepEqual(usage.textureSourceKinds,[]);assert.deepEqual(usage.resourceMimeTypes,[]);});
test('null MIME still records a consumed image source',()=>{const resource=ref(null);createTexture({resource});const scene=createScene3D();scene.resources.push(resource);const usage=create();collect(usage,scene);assert.deepEqual(usage.resourceMimeTypes,[]);assert.deepEqual(usage.textureSourceKinds,['image']);});
test('reused usage does not accumulate kinds',()=>{const scene=sceneWith(createUnlitMaterial()),usage=create();collect(usage,scene);collect(usage,scene);assert.deepEqual(usage.materialKinds,['UnlitMaterial']);assert.deepEqual(usage.nodeKinds,[MeshKind,Node3DKind]);});
test('initializer retains function-value surface',()=>assert.equal(typeof initialize,'function'));
// Presence differs from reading the value once; retain the source's exact domain and exceptions.
test('present undefined stack is skipped',()=>{const material=createMaterial3D('Vendor');material.modifiers=undefined;assert.deepEqual(kinds(material).modifierKinds,[]);});
test('open vendor material and modifiers are structurally accepted',()=>{const material=createMaterial3D('Vendor'),first={kind:'Zulu'},second={kind:'Alpha'};material.modifiers=[first,second,first];const usage=kinds(material);assert.deepEqual(usage.materialKinds,['Vendor']);assert.deepEqual(usage.modifierKinds,['Alpha','Zulu']);assert.equal(material.modifiers[0],first);});
test('inherited stack is read without an own-property restriction',()=>{const material=createMaterial3D('Vendor');Object.setPrototypeOf(material,Object.create(Object.getPrototypeOf(material),{modifiers:{value:[{kind:'Inherited'}]}}));assert.equal(Object.hasOwn(material,'modifiers'),false);assert.deepEqual(kinds(material).modifierKinds,['Inherited']);});
test('getter stack is read exactly once',()=>{let reads=0;const material=createMaterial3D('Vendor');Object.defineProperty(material,'modifiers',{get(){return ++reads===1?[{kind:'First'}]:[{kind:'Second'}];}});assert.deepEqual(kinds(material).modifierKinds,['First']);assert.equal(reads,1);});
test('undefined getter is read once and skipped',()=>{let reads=0;const material=createMaterial3D('Vendor');Object.defineProperty(material,'modifiers',{get(){return ++reads===1?undefined:[{kind:'Later'}];}});assert.deepEqual(kinds(material).modifierKinds,[]);assert.equal(reads,1);});
test('null stack keeps the original TypeError rather than becoming absent',()=>{const material=createMaterial3D('Vendor');material.modifiers=null;assert.throws(()=>kinds(material),TypeError);});
test('reusing output preserves each list identity and clears all prior values',()=>{const usage=create(),identities=lists.map(key=>usage[key]);for(const key of lists)usage[key].push('stale');collect(usage,createScene3D());lists.forEach((key,i)=>assert.equal(usage[key],identities[i]));assert.deepEqual(usage.nodeKinds,[Node3DKind]);for(const key of lists.filter(x=>x!=='nodeKinds'))assert.deepEqual(usage[key],[]);});
