// Exercise complete original Bezier, track construction and sampling modules without replacement bodies.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
  const at = arg.indexOf('='); assert.ok(at > 0); return [arg.slice(0, at), arg.slice(at + 1)];
}));
assert.ok(args['--typescript'] && args['--output']);
const { default: ts } = await import(pathToFileURL(path.resolve(args['--typescript'])));
const upstream = path.join(root, '.dependencies/flight');
const cache = new Map(), hashes = {};
const hash = text => createHash('sha256').update(text).digest('hex');
function load(input) {
  let file = path.resolve(input);
  if (!fs.existsSync(file) && fs.existsSync(file + '.ts')) file += '.ts';
  if (cache.has(file)) return cache.get(file).exports;
  const module = { exports: {} }; cache.set(file, module);
  const require = specifier => {
    if (specifier.startsWith('@flighthq/')) {
      const [name, ...sub] = specifier.slice('@flighthq/'.length).split('/');
      return load(path.join(upstream, 'packages', name, 'src', (sub.join('/') || 'index') + '.ts'));
    }
    return specifier.startsWith('.')
      ? load(path.resolve(path.dirname(file), specifier.replace(/\.js$/, '.ts')))
      : createRequire(file)(specifier);
  };
  const source = fs.readFileSync(file, 'utf8');
  hashes[path.relative(upstream, file)] = hash(source);
  const code = ts.transpileModule(source, {
    compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 },
  }).outputText;
  new Function('require', 'module', 'exports', '__filename', '__dirname', code)(require, module, module.exports, file, path.dirname(file));
  return module.exports;
}
const api = load(path.join(upstream, 'packages/scene2d-formats/src/lottieBezierPath.ts'));
const document = load(path.join(upstream, 'packages/scene2d-formats/src/lottieDocument.ts'));
const animation = load(path.join(upstream, 'packages/animation/src/animationTrack.ts'));
assert.equal(hashes['packages/scene2d-formats/src/lottieBezierPath.ts'], 'da3f167f812fc52463da4e6de9c9ae346373312f18c7c6f50f5f40c713115ce2');

const shape=(x,y)=>({v:[[x,y]],i:[],o:[],c:false});
const A=shape(1,2),B=shape(5,6),C=shape(9,10);
const context={frameOffset:0,frameScale:1,document:{ip:0,fr:1}};
const inputs=[
 {name:'empty-track',keyframes:[],components:2},
 {name:'one-keyframe',keyframes:[{t:0,s:A}],components:2},
 {name:'two-keyframe-linear',keyframes:[{t:0,s:A},{t:1,s:B}],components:2},
 {name:'hold-segment',keyframes:[{t:0,s:A,h:1},{t:1,s:B}],components:2},
 {name:'temporal-easing',keyframes:[{t:0,s:A,o:{x:0.42,y:0},i:{x:0.58,y:1}},{t:1,s:B}],components:2},
 {name:'previous-end-fallback',keyframes:[{t:0,s:A,e:B},{t:1}],components:2},
 {name:'source-index-after-skipped-time',keyframes:[{t:0,s:A},{t:0,s:C},{t:-1,s:C},{t:1,s:B}],components:2},
 {name:'frame-conversion',keyframes:[{t:0,s:A},{t:2,s:B}],components:2,context:{frameOffset:3,frameScale:2,document:{ip:1,fr:4}}},
 {name:'spatial-fallback-linear',keyframes:[{t:0,s:A},{t:1,s:B}],components:2,spatial:true},
 {name:'spatial-cubic',keyframes:[{t:0,s:A,to:[2,0],ti:[-2,0]},{t:1,s:B}],components:2,spatial:true},
];
for(const [name,input] of [
 ['empty-shape',{v:[],i:[],o:[],c:false}],['line',{v:[[1,2],[3,4]],i:[],o:[],c:false}],
 ['closed-cubic',{v:[[1,2],[3,4]],i:[[1,2],[4,5]],o:[[2,3],[6,7]],c:true}]
])inputs.push({name,keyframes:[{t:0,s:input}],components:api.flattenLottieShapePath(input).length});
const cases=inputs.map(input=>{
 const actualContext=input.context??context,callbackOrder=[];
 const callback=(value,index)=>{callbackOrder.push(index);return api.flattenLottieShapePath(api.toLottieShapePath(value));};
 const track=document.createLottieTrack(input.keyframes,input.components,actualContext,callback,0,input.spatial??false);
 const result={...input,context:actualContext,spatial:input.spatial??false,callbackOrder,times:Array.from(track.times),values:Array.from(track.values),interpolation:track.interpolation,segmentEasings:track.segmentEasings?.map(x=>x!==null)??null,paths:[],samples:[]};
 input.keyframes.forEach((key,index)=>{for(const field of ['s','e'])if(key[field]){const path=api.createLottieBezierPath(api.toLottieShapePath(key[field]));result.paths.push({key:index,field,data:Array.from(path.data),commands:Array.from(path.commands)});}});
 for(const time of [-1,0,0.25,0.5,0.75,1,1.5,2])for(const output of [[],[20],[20,30,40,50]])for(const typed of [false,true]){
  const out=typed?new Float32Array(output):[...output],owner=out;animation.sampleAnimationTrack(out,track,time);assert.equal(out,owner);
  result.samples.push({time,input:output,typed,output:Array.from(out)});
 }
 const alias=track.values;animation.sampleAnimationTrack(alias,track,0.5);
 assert.equal(alias,track.values);result.aliasedSample=Array.from(alias);
 return result;
});
const observations=[];
{
 const reads=[],sentinel=new RangeError('original later coordinate');
 const point={get 0(){reads.push('0');return 'x';},get 1(){reads.push('1');throw sentinel;}};
 const value={v:[point],i:[],o:[],c:false};
 assert.throws(()=>document.createLottieTrack([{t:0,s:value}],2,context,(v,index)=>{reads.push('callback:'+index);return api.flattenLottieShapePath(api.toLottieShapePath(v));},0),e=>e===sentinel);
 assert.deepEqual(reads,['callback:0','0','1']);observations.push({name:'track-callback-later-getter-before-escape',reads,sameError:true});
}
{
 const callbackReads=[];const samples=[[1,2],[3,4]];
 const track=document.createLottieTrack([{t:0,s:A},{t:1,s:B}],2,context,(_value,index)=>{callbackReads.push(index);return samples[index];},0);
 samples[0][0]=99;assert.deepEqual(track.values,[1,2,3,4]);observations.push({name:'flat-track-store-is-source-snapshot',callbackReads,values:track.values});
}
{
 const reads=[],vertices=[];
 vertices.push({get 0(){reads.push(0);vertices.push([3,4]);return 1;},get 1(){reads.push(1);return 2;}});
 const value={v:vertices,i:[],o:[],c:false};
 const track=document.createLottieTrack([{t:0,s:value}],4,context,v=>api.flattenLottieShapePath(api.toLottieShapePath(v)),0);
 const out=[];animation.sampleAnimationTrack(out,track,0);
 assert.deepEqual(out,[1,2,3,4]);assert.deepEqual(reads,[0,1]);
 observations.push({name:'live-points-through-track-callback',reads,values:Array.from(track.values),output:out});
}
{
 const track=document.createLottieTrack([{t:0,s:A}],2,context,v=>api.flattenLottieShapePath(api.toLottieShapePath(v)),0);
 const values=[1,2],reads=[],sentinel=new RangeError('later stored value');
 Object.defineProperty(values,'0',{get(){reads.push(0);return 1;}});
 Object.defineProperty(values,'1',{get(){reads.push(1);throw sentinel;}});
 track.values=values;const out=[];
 assert.throws(()=>animation.sampleAnimationTrack(out,track,0),e=>e===sentinel);
 assert.deepEqual(out,[1]);assert.deepEqual(reads,[0,1]);
 observations.push({name:'original-sampler-later-read-preserves-prefix',reads,output:out,sameError:true});
}
{
 let reads=0;const value={get v(){++reads;return reads===1?[[1,2]]:[[3,4]];},i:[],o:[],c:false};
 const track=document.createLottieTrack([{t:0,s:value},{t:1,s:value}],2,context,v=>{assert.equal(api.toLottieShapePath(v),value);return api.flattenLottieShapePath(v);},0);
 assert.equal(reads,2);assert.deepEqual(track.values,[1,2,3,4]);
 const out=[];animation.sampleAnimationTrack(out,track,0.5);assert.deepEqual(out,[2,3]);
 observations.push({name:'original-owner-getter-read-on-each-track-callback',reads,values:Array.from(track.values),output:out});
}
const sourcePaths=execFileSync('git',['-C',upstream,'ls-tree','-r','a62784923f3be814463286c9fb28edfe6ab15789'],{encoding:'utf8'});
const blobs=new Map(sourcePaths.trim().split('\n').map(line=>{const at=line.indexOf('\t');return [line.slice(at+1),line.slice(0,at).split(' ')[2]];}));
for(const relative of Object.keys(hashes)){
 const bytes=fs.readFileSync(path.join(upstream,relative));assert.equal(createHash('sha1').update(`blob ${bytes.length}\0`).update(bytes).digest('hex'),blobs.get(relative),relative+' changed from raw pin');
}
const result={schema:'flight-lottie-bezier-public-numeric-oracle/1',pin:'a62784923f3be814463286c9fb28edfe6ab15789',wholeOriginalModules:true,replacementBodies:false,sourceHashes:hashes,verifiedOriginalBlobs:Object.keys(hashes).length,cases,observations};
fs.writeFileSync(path.resolve(args['--output']),JSON.stringify(result,null,2)+'\n');
if (args['--native-expected']) {
  const cppString=value=>JSON.stringify(value);
  const cppNumber=value=>{assert.ok(Number.isFinite(value));return Object.is(value,-0)?'-0.0':String(value);};
  const cppNumbers=values=>'{'+values.map(cppNumber).join(',')+'}';
  const cppBool=value=>value?'true':'false';
  let header=`#pragma once
#include <vector>
namespace public_expected {
struct Sample {double time;bool typed;std::vector<double> input,output;};
struct Path {int key;const char* field;std::vector<double> data,commands;};
struct Case {const char* name;const char* keys;double components,offset,scale,ip,fr;bool spatial;const char* interpolation;std::vector<int> callbacks;std::vector<double> times,values;std::vector<bool> easings;std::vector<Path> paths;std::vector<Sample> samples;std::vector<double> alias;};
inline const std::vector<Case> cases={
`;
  for(const item of cases){
    const ctx=item.context;
    const paths='{'+item.paths.map(p=>'{'+[p.key,cppString(p.field),cppNumbers(p.data),cppNumbers(p.commands)].join(',')+'}').join(',')+'}';
    const samples='{'+item.samples.map(s=>'{'+[cppNumber(s.time),cppBool(s.typed),cppNumbers(s.input),cppNumbers(s.output)].join(',')+'}').join(',')+'}';
    const fields=[cppString(item.name),cppString(JSON.stringify(item.keyframes)),cppNumber(item.components),cppNumber(ctx.frameOffset),cppNumber(ctx.frameScale),cppNumber(ctx.document.ip),cppNumber(ctx.document.fr),cppBool(item.spatial),cppString(item.interpolation),cppNumbers(item.callbackOrder),cppNumbers(item.times),cppNumbers(item.values),'{'+item.segmentEasings.map(cppBool).join(',')+'}',paths,samples,cppNumbers(item.aliasedSample)];
    header+=' {'+fields.join(',')+'},\n';
  }
  header+='};\n}\n';
  fs.writeFileSync(path.resolve(args['--native-expected']),header);
}
console.log(JSON.stringify({publicCases:cases.length,behaviorCases:observations.length,sampleComparisons:cases.reduce((n,c)=>n+c.samples.length,0),verifiedOriginalBlobs:result.verifiedOriginalBlobs,failures:0}));
