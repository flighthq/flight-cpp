import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const lock = JSON.parse(readFileSync(path.join(root, 'dependencies.lock.json'), 'utf8'));
const checkout = path.join(root, lock.directory, 'flight');
const parser = path.join(root, lock.directory, 'flight-compiler', 'node_modules/typescript/lib/typescript.js');
if (!existsSync(path.join(checkout, '.git')) || !existsSync(parser)) {
  process.stdout.write('Video helper oracle skipped: pinned checkout/compiler parser is absent.\n');
  process.exit(0);
}
const revision = lock.dependencies.find((entry) => entry.name === 'flight').commit;
const ts = (await import(pathToFileURL(parser).href)).default;
function pinned(file) {
  const result = spawnSync('git', ['show', `${revision}:${file}`], { cwd: checkout, encoding: 'utf8' });
  assert.equal(result.status, 0, result.stderr);
  return result.stdout;
}
const hostDeclaration = pinned('packages/types/src/HostVideo.ts');
for (const name of ['getWidth', 'getHeight']) assert.match(hostDeclaration, new RegExp(`${name}\\?\\(element: HostImageSource\\): number;`));
assert.match(pinned('packages/types/src/HostImageSource.ts'), /export type HostImageSource = CanvasImageSource;/u);
const declarations = [];
for (const [file, names] of [
  ['packages/entity/src/entity.ts', ['allocateEntity', 'finishEntity']],
  ['packages/geometry/src/vector2.ts', ['cloneVector2', 'copyVector2', 'createVector2', 'initializeVector2']],
  ['packages/texture/src/sampler.ts', ['cloneSampler', 'copySampler', 'createSampler', 'initializeSampler']],
  ['packages/geometry/src/matrix3.ts', ['inverseMatrix3', 'isAffineMatrix3']],
  ['packages/texture/src/texture.ts', ['cloneTexture', 'copyTexture', 'getTextureUvMatrix', 'createTexture', 'createTexture2D', 'initializeTexture2D', 'createCommonTextureFields', 'applyCommonTextureFields', 'attachTextureToResource']],
  ['packages/texture/src/videoTexture.ts', ['advanceVideoTexture', 'cloneVideoTexture', 'copyVideoTexture', 'getVideoTextureUvMatrix', 'getVideoTextureInverseUvMatrix', 'getVideoTextureWidth', 'getVideoTextureHeight', 'isVideoTextureFrameReady', 'setVideoTextureSource', 'getVideoImage', 'getVideoSource', 'createVideoTexture', 'createVideoImageResource', 'initializeVideoImageResource', 'updateVideoImageSize']],
]) {
  const text = pinned(file);
  const parsed = ts.createSourceFile(file, text, ts.ScriptTarget.Latest, true);
  for (const name of names) {
    const implementations = parsed.statements.filter((node) => ts.isFunctionDeclaration(node) && node.name?.text === name && node.body);
    assert.equal(implementations.length, 1, `re-derive oracle when ${name} implementation changes`);
    declarations.push(implementations[0].getText(parsed).replace(/^export /u, ''));
  }
}
const key = pinned('packages/types/src/Entity.ts').match(/^export const EntityRuntimeKey = .+;$/mu)?.[0];
const version = pinned('packages/texture/src/videoTexture.ts').match(/^const INITIAL_VIDEO_VERSION = .+;$/mu)?.[0];
const kind = pinned('packages/types/src/TextureSourceKind.ts').match(/^export const ImageTextureSourceKind = .+;$/mu)?.[0];
assert.ok(key && version && kind);
const halfPi = pinned('packages/texture/src/texture.ts').match(/^const HALF_PI = .+;$/mu)?.[0];
assert.ok(halfPi);
const source = `${halfPi}\n${key.replace(/^export /u, '')}\n${kind.replace(/^export /u, '')}\n${version}\n${declarations.join('\n')}`;
const js = ts.transpileModule(source, { compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.None } }).outputText;
const api = new Function(`${js}\nreturn {createVideoTexture,createTexture,createSampler,createVector2,EntityRuntimeKey,advanceVideoTexture,cloneVideoTexture,copyVideoTexture,getVideoTextureUvMatrix,getVideoTextureInverseUvMatrix,getVideoTextureWidth,getVideoTextureHeight,isVideoTextureFrameReady,setVideoTextureSource};`)();
// Focused callable proof can run without repeating the accepted helper suite.
if (!process.argv.includes('--callable-proxies-only')) {
const bitmap = {kind: 'bitmap', width: 2, height: 3, version: 17};
const texture = api.createTexture({source: bitmap, version: 17});
const trace = [];
const advanceRevision = api.advanceVideoTexture({
  getWidth(source) {trace.push(['width', source === undefined]);return 23;},
  getHeight(source) {trace.push(['height', source === undefined]);return 11;},
}, texture);
assert.deepEqual(trace, [['width', true], ['height', true]]);
assert.equal(advanceRevision, 18);assert.equal(bitmap.version, 18);
assert.equal(bitmap.width, 23);assert.equal(bitmap.height, 11);
assert.equal(texture.source, bitmap);
process.stdout.write('Pinned advanceVideoTexture assertion witness: a valid Texture2D with Bitmap source invokes width then height with undefined, mutates the same Bitmap dimensions/version, and mirrors version18. HostImageSource callback carrier cannot represent undefined; no fabricated host source is faithful.\n');
{
  const bitmap = {kind:'bitmap',width:64,height:32,version:0};const voxel = {kind:'voxel',width:4,height:4,version:0};
  for (const dimension of ['2d','cube','2d-array','3d']) {
    const source = api.createTexture({dimension,source:dimension==='3d'?voxel:bitmap,sources:[bitmap,null]});
    source.colorSpace='linear';source.flipX=true;source.uvOffset.x=0.25;source.uvScale.y=3;
    source.sampler.anisotropy=7;source.version=0x100000001;
    const output = api.cloneVideoTexture(source);
    assert.notEqual(output,source);assert.equal(output.version,1);assert.equal(output.colorSpace,source.colorSpace);
    for (const key of ['sampler','uvOffset','uvScale']) assert.notEqual(output[key],source[key]);
    if ('sources' in source) {assert.notEqual(output.sources,source.sources);assert.equal(output.sources[0],bitmap);}
    else assert.equal(output.source,source.source);
    const refs=[output.sampler,output.uvOffset,output.uvScale];api.copyVideoTexture(output,source);
    assert.deepEqual([output.sampler,output.uvOffset,output.uvScale],refs);
    if ('sources' in source) {const list=source.sources;api.copyVideoTexture(source,source);assert.notEqual(source.sources,list);assert.equal(source.sources[0],bitmap);}
    output.uvOffset=source.uvScale;const x=source.uvOffset.x;api.copyVideoTexture(output,source);
    assert.equal(source.uvScale.x,x);assert.equal(output.uvScale.x,x);
  }
}
{
  const reads=[];let readError=false;let dimension='cube';let rotation=Math.PI/2;
  const scale=api.createVector2(2,3);const values={colorSpace:'linear',flipX:true,flipY:true,sampler:api.createSampler(),uvOffset:api.createVector2(0.25,0.5),uvRotation:rotation,uvScale:scale,version:0x100000001,dimension,sources:[null]};
  const source={};for (const [key,value] of Object.entries(values)) Object.defineProperty(source,key,{get(){
    reads.push(key);if(key==='uvOffset'&&readError)throw new Error('read failed');
    return key==='dimension'?dimension:key==='uvRotation'?rotation:value;
  }});
  api.cloneVideoTexture(source);
  assert.deepEqual(reads,['colorSpace','flipX','flipY','sampler','uvOffset','uvRotation','uvScale','version','dimension','sources']);
  reads.length=0;const output=api.createTexture({dimension:'cube'});api.copyVideoTexture(output,source);
  assert.deepEqual(reads,['dimension','colorSpace','flipX','flipY','uvRotation','version','sampler','uvOffset','uvScale','dimension','sources']);
  reads.length=0;readError=true;assert.throws(()=>api.cloneVideoTexture(source),{message:'read failed'});
  assert.deepEqual(reads,['colorSpace','flipX','flipY','sampler','uvOffset']);readError=false;
  dimension='2d';reads.length=0;assert.throws(()=>api.copyVideoTexture(output,source),{message:'copyTexture requires matching dimensions'});
  assert.deepEqual(reads,['dimension']);dimension='cube';
  const matrix={m:new Float32Array(9)};reads.length=0;api.getVideoTextureUvMatrix(matrix,source);
  assert.deepEqual(reads,['uvRotation','flipX','flipY','uvScale','uvScale','flipX','uvScale','flipY','uvScale','uvOffset','uvOffset']);
  assert.equal(matrix.m[1],-2);assert.equal(matrix.m[3],3);assert.equal(matrix.m[6],-2.75);assert.equal(matrix.m[7],2.5);
  api.getVideoTextureInverseUvMatrix(matrix,source);assert.ok(Math.abs(matrix.m[1]-1/3)<1e-6);assert.equal(matrix.m[3],-0.5);
  scale.x=0;api.getVideoTextureInverseUvMatrix(matrix,source);assert.ok([...matrix.m].every(Number.isNaN));
}
{
  const reads=[];const out=api.createTexture({dimension:'cube'});const original=out.sampler;const replacement=api.createSampler();
  const source=api.createTexture({dimension:'cube',sampler:api.createSampler({anisotropy:7})});
  const sampler=source.sampler;Object.defineProperty(source,'sampler',{get(){reads.push('source-sampler');out.sampler=replacement;return sampler;}});
  api.copyVideoTexture(out,source);assert.equal(original.anisotropy,7);assert.equal(out.sampler,replacement);assert.equal(replacement.anisotropy,1);
  assert.deepEqual(reads,['source-sampler']);
}
{
  const element={borrowed:true},resource={element};const video=api.createVideoTexture({},resource);
  const log=[];const host={
    isReady(value){assert.equal(value,element);log.push('ready');return true;},
    getWidth(value){assert.equal(value,element);log.push('width');host.getHeight=()=>{log.push('height');return 11;};return 23;},
    getHeight(){log.push('old-height');return -1;},
  };
  assert.equal(api.isVideoTextureFrameReady(host,video),true);assert.deepEqual(log,['ready','width','height']);
  host.isReady=()=>{log.push('ready');return false;};log.length=0;
  assert.equal(api.isVideoTextureFrameReady(host,video),false);assert.deepEqual(log,['ready']);
  host.isReady=()=>{log.push('ready');return true;};host.getWidth=()=>{log.push('width');return 0;};log.length=0;
  assert.equal(api.isVideoTextureFrameReady(host,video),false);assert.deepEqual(log,['ready','width']);
  assert.equal(api.getVideoTextureWidth(host,video),-1);assert.equal(api.getVideoTextureHeight(host,video),11);
  host.getWidth=()=>NaN;assert.equal(api.getVideoTextureWidth(host,video),-1);assert.equal(api.getVideoTextureWidth({},video),-1);
  log.length=0;assert.equal(api.isVideoTextureFrameReady(host,api.createTexture({source:{kind:'bitmap'}})),false);
  assert.equal(api.getVideoTextureWidth(host,api.createTexture({dimension:'cube'})),-1);assert.deepEqual(log,[]);
  host.getWidth=()=>{log.push('width');return 7;};log.length=0;const old=video.source;
  api.setVideoTextureSource(host,video,resource);assert.notEqual(video.source,old);assert.equal(video.source.source,element);
  assert.equal(video.version,0xffffffff);assert.equal(video.source.width,7);assert.deepEqual(log,['width','height']);
  api.setVideoTextureSource(host,video,{element:null});assert.equal(video.source,null);
  log.length=0;assert.throws(()=>api.setVideoTextureSource(host,api.createTexture({dimension:'cube'}),resource),{message:'setVideoTextureSource requires a Texture2D'});assert.deepEqual(log,[]);
  api.setVideoTextureSource({},video,resource);const prior=video.source;video.version=9;host.getHeight=()=>{throw new Error('height failed');};
  assert.throws(()=>api.setVideoTextureSource(host,video,resource),{message:'height failed'});assert.equal(video.source,prior);assert.equal(video.version,9);
}
process.stdout.write('Pinned eight-helper source oracle: all four readonly arms; clone/copy ownership and source-list identity; self/cross-alias copies and read order; repeated UV getters and singular inverse; host readiness/size short circuits; late callback replacement; set-source identity, guards and exception order PASS.\n');

}
{
  const log=[];let writes=0;
  function readonly(name,target) {
    return new Proxy(target,{
      get(object,key,receiver) {if(typeof key==='string')log.push(`${name}.${key}`);return Reflect.get(object,key,receiver);},
      set() {++writes;throw new Error('readonly write');},
      defineProperty() {++writes;throw new Error('readonly define');},
      deleteProperty() {++writes;throw new Error('readonly delete');},
    });
  }
  const element={borrowed:true};const resource={element};
  const target=api.createVideoTexture({},resource);
  target.colorSpace='linear';target.flipX=true;target.flipY=true;
  target.uvOffset=api.createVector2(0.25,0.5);target.uvScale=api.createVector2(2,3);
  target.uvRotation=Math.PI/2;target.version=7;
  const source=readonly('texture',target);
  const hostTarget={
    isReady(value){assert.equal(value,element);log.push('ready');return true;},
    getWidth(value){assert.equal(value,element);log.push('width');return 23;},
    getHeight(value){assert.equal(value,element);log.push('height');return 11;},
  };
  const host=readonly('host',hostTarget);
  const input=readonly('resource',resource);
  const {cloneVideoTexture:clone,copyVideoTexture:copy,getVideoTextureUvMatrix:uv,
    getVideoTextureInverseUvMatrix:inverse,getVideoTextureWidth:width,
    getVideoTextureHeight:height,isVideoTextureFrameReady:ready,setVideoTextureSource:setSource}=api;
  const seen=[];
  function expect(name,call,keys) {
    log.length=0;const result=call();assert.deepEqual(log,keys,name);seen.push(name);return result;
  }
  const common=['colorSpace','flipX','flipY','sampler','uvOffset','uvRotation','uvScale','version'];
  const keys=values=>values.map(name=>`texture.${name}`);
  const cloned=expect('clone',()=>clone(source),keys([...common,'dimension','source']));
  assert.equal(cloned.source,target.source);assert.notEqual(cloned.sampler,target.sampler);
  const output=api.createTexture();
  expect('copy',()=>copy(output,source),keys(['dimension','colorSpace','flipX','flipY','uvRotation','version','sampler','uvOffset','uvScale','dimension','source']));
  const matrix={m:new Float32Array(9)};
  const uvReads=keys(['uvRotation','flipX','flipY','uvScale','uvScale','flipX','uvScale','flipY','uvScale','uvOffset','uvOffset']);
  expect('uv',()=>uv(matrix,source),uvReads);
  expect('inverse',()=>inverse(matrix,source),uvReads);
  assert.equal(matrix.m[3],-0.5);
  assert.equal(expect('width',()=>width(host,source),[...keys(['dimension','source']),'host.getWidth','width']),23);
  assert.equal(expect('height',()=>height(host,source),[...keys(['dimension','source']),'host.getHeight','height']),11);
  assert.equal(expect('ready',()=>ready(host,source),[...keys(['dimension','source']),'host.isReady','ready','host.getWidth','width','host.getHeight','height']),true);
  const old=output.source;
  expect('setSource',()=>setSource(host,output,input),['resource.element','resource.element','host.getWidth','width','host.getHeight','height']);
  assert.notEqual(output.source,old);assert.equal(output.source.source,element);
  assert.equal(writes,0);assert.equal(resource.element,element);assert.equal(target.version,7);
  assert.deepEqual(seen,['clone','copy','uv','inverse','width','height','ready','setSource']);
  process.stdout.write('Focused pinned callable proxy proof PASS8/8: write-trapped readonly texture/host/resource, canonical function values, exact repeated getter order, zero input writes.\n');
}
