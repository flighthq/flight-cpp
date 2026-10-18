// Execute complete original modules; measure raw values beyond the Bezier callback return.
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

const cases = [];
for (const [name, input] of [
  ['empty', {v:[],i:[],o:[],c:false}],
  ['one-point', {v:[[1,2]],i:[],o:[],c:false}],
  ['line', {v:[[1,2],[3,4]],i:[],o:[],c:false}],
  ['closed-line', {v:[[1,2],[3,4]],i:[],o:[],c:true}],
  ['cubic', {v:[[1,2],[3,4]],i:[[0,0],[4,5]],o:[[2,3],[0,0]],c:false}],
  ['closed-cubic', {v:[[1,2],[3,4]],i:[[1,2],[4,5]],o:[[2,3],[6,7]],c:true}],
  ['nullish-flatten-defaults', {v:[[null,null],[1]],i:[],o:[],c:false}],
]) {
  const selected = api.toLottieShapePath(input);
  assert.equal(selected, input);
  const flat=api.flattenLottieShapePath(selected);
  const path=name==='nullish-flatten-defaults'?null:api.createLottieBezierPath(selected);
  const restored=api.unflattenLottieShapePath(selected, flat);
  let sampled=null;
  if(flat.length) {
    const context={frameOffset:0,frameScale:1,document:{ip:0,fr:1}};
    const track=document.createLottieTrack([{t:0,s:input}],flat.length,context,
      value=>api.flattenLottieShapePath(api.toLottieShapePath(value)),0);
    sampled=[];animation.sampleAnimationTrack(sampled,track,0);assert.deepEqual(sampled,flat);
    const preallocated=Array(flat.length).fill(0);animation.sampleAnimationTrack(preallocated,track,0);assert.deepEqual(preallocated,flat);
  }
  cases.push({name,input,flat,path:path?{data:Array.from(path.data),commands:Array.from(path.commands)}:null,restored,sampled});
}
const ordered=[];
const sentinel=new RangeError('later coordinate');
const point={get 0(){ordered.push('0');return 'x';}, get 1(){ordered.push('1');throw sentinel;}};
const throwing={v:[point],i:[],o:[],c:false};
for (const operation of ['flattenLottieShapePath','createLottieBezierPath']) {
  ordered.length=0;
  assert.throws(()=>api[operation](throwing),error=>error===sentinel);
  cases.push({name:operation+'-later-getter-before-escape',order:[...ordered],sameError:true});
}
{
 let reads=0;const value={get v(){++reads;return reads===1?7:reads===2?[[9,2]]:[[3,4]];},i:[],o:[],c:false};
 const selected=api.toLottieShapePath(value);assert.equal(selected,value);assert.equal(reads,0);
 const result=api.createLottieBezierPath(selected);assert.deepEqual(result.data,[9,4]);assert.equal(reads,3);
 cases.push({name:'changing-v-getter-keeps-read-order',reads,data:Array.from(result.data)});
}
const sourcePaths=execFileSync('git',['-C',upstream,'ls-tree','-r','a62784923f3be814463286c9fb28edfe6ab15789'],{encoding:'utf8'});
const blobs=new Map(sourcePaths.trim().split('\n').map(line=>{const at=line.indexOf('\t');return [line.slice(at+1),line.slice(0,at).split(' ')[2]];}));
for(const relative of Object.keys(hashes)) {
 const bytes=fs.readFileSync(path.join(upstream,relative));
 const object=createHash('sha1').update(`blob ${bytes.length}\0`).update(bytes).digest('hex');
 assert.equal(object,blobs.get(relative),relative+' differs from the pinned original module');
}
const result={schema:'flight-lottie-bezier-numeric-oracle/1',pin:'a62784923f3be814463286c9fb28edfe6ab15789',sourceSha256:hashes['packages/scene2d-formats/src/lottieBezierPath.ts'],wholeOriginalModules:true,replacementBodies:false,loadedModules:cache.size,verifiedOriginalBlobs:Object.keys(hashes).length,cases,sourceHashes:hashes};
const output=path.resolve(args['--output']);fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify({cases:cases.length,verifiedOriginalBlobs:result.verifiedOriginalBlobs,failures:0}));
