import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';

const root = fileURLToPath(new URL('..', import.meta.url));
const arg = (name, fallback) => process.argv.find(value => value.startsWith(`--${name}=`))?.slice(name.length + 3) ?? fallback;
const raw = path.resolve(arg('raw', path.join(root, '.dependencies/flight')));
const patched = path.resolve(arg('patched', raw));
const compiler = path.resolve(arg('compiler', path.join(root, '.dependencies/flight-compiler')));
const output = path.resolve(arg('output', path.join(root, 'out/lottie-bezier-source-oracle.json')));
const pin = 'a62784923f3be814463286c9fb28edfe6ab15789';
const relative = 'packages/scene2d-formats/src/lottieBezierPath.ts';
const original = spawnSync('git', ['show', `${pin}:${relative}`], { cwd: raw, encoding: 'utf8' });
if (original.status || original.stdout !== fs.readFileSync(path.join(raw, relative), 'utf8')) {
  throw new Error('Raw Bezier source must be the exact pinned original');
}
const ts = (await import(pathToFileURL(path.join(compiler, 'node_modules/typescript/lib/typescript.js')).href)).default;
function loader(directory) {
  const cache = new Map();
  const load = input => {
    let file = path.resolve(input);
    if (!fs.existsSync(file) && fs.existsSync(`${file}.ts`)) file += '.ts';
    if (cache.has(file)) return cache.get(file).exports;
    const module = { exports: {} };
    cache.set(file, module);
    const require = specifier => {
      if (specifier.startsWith('@flighthq/')) {
        const [name, ...sub] = specifier.slice('@flighthq/'.length).split('/');
        return load(path.join(directory, 'packages', name, 'src', `${sub.join('/') || 'index'}.ts`));
      }
      if (specifier.startsWith('.')) return load(path.resolve(path.dirname(file), specifier.replace(/\.js$/, '.ts')));
      return createRequire(file)(specifier);
    };
    const text = ts.transpileModule(fs.readFileSync(file, 'utf8'), {
      compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 },
    }).outputText;
    new Function('require', 'module', 'exports', '__filename', '__dirname', text)(require, module, module.exports, file, path.dirname(file));
    return module.exports;
  };
  return { api: load(path.join(directory, relative)), cache };
}
const sources = [loader(raw), loader(patched)];
const cases=[];
function measure(source, kind){
 let reads=0,writes=0,sourceObject;
 let input;
 switch(kind){
  case 'undefined':input=undefined;break;
  case 'null':input=null;break;
  case 'number':input=7;break;
  case 'string':input='v';break;
  case 'empty-array':input=[];break;
  case 'wrapper-hole':input=Array(1);break;
  case 'empty-object':input={};break;
  case 'wrong-number-v':sourceObject={v:7};input=sourceObject;break;
  case 'present-undefined-v':sourceObject={v:undefined};input=sourceObject;break;
  case 'wrapped-wrong-v':sourceObject={v:7};input=[sourceObject];break;
  case 'inherited-v':sourceObject=Object.create({v:7});input=sourceObject;break;
  case 'empty-v':sourceObject={v:[]};input=sourceObject;break;
  case 'one-point':sourceObject={v:[[1,2]],i:[],o:[],c:false};input=sourceObject;break;
  case 'live-owner':sourceObject={v:[]};input=sourceObject;break;
  case 'live-wrapped-owner':sourceObject={v:[]};input=[sourceObject];break;
  case 'getter-wrong-v':
  case 'getter-throws-v':
  case 'inherited-getter-v':{
    const host={get v(){++reads;if(kind==='getter-throws-v')throw new RangeError('original v getter');return 7;},set v(value){++writes;}};
    sourceObject=kind==='inherited-getter-v'?Object.create(host):host;input=sourceObject;break;
  }
  case 'changing-getter-v':{
    sourceObject={get v(){++reads;return reads===1?[[1,2]]:undefined;},set v(value){++writes;}};input=sourceObject;break;
  }
  default:throw Error(kind);
 }
 let selected,selectionError;
 try{selected=source.api.toLottieShapePath(input);}catch(error){selectionError={name:error.name,message:error.message};}
 const selection={present:selected!==undefined,sameOwner:selected!==undefined&&selected===sourceObject,reads,writes,error:selectionError??null};
 let later;
 if(selected!==undefined){
  if(kind==='live-owner'||kind==='live-wrapped-owner'){
   sourceObject.v=7;later={sameOwner:selected===sourceObject,live:selected.v===7};
  }else{
   try{const path=source.api.createLottieBezierPath(selected);later={returned:true,kind:path.kind,reads,writes};}
   catch(error){later={error:error.name,message:error.message,reads,writes};}
  }
 }
 return {selection,later:later??null};
}
for(const kind of ['undefined','null','number','string','empty-array','wrapper-hole','empty-object','wrong-number-v','present-undefined-v','wrapped-wrong-v','inherited-v','empty-v','one-point','live-owner','live-wrapped-owner','getter-wrong-v','getter-throws-v','inherited-getter-v','changing-getter-v']){
 const pair=sources.map(source=>measure(source,kind));cases.push({kind,raw:pair[0],patched:pair[1],equal:JSON.stringify(pair[0])===JSON.stringify(pair[1])});
}
const hash=directory=>createHash('sha256').update(fs.readFileSync(path.join(directory,relative))).digest('hex');
const result={schema:'flight-lottie-bezier-original-source-boundary/1',rawPin:pin,rawSourceSha256:hash(raw),patchedSourceSha256:hash(patched),wholeOriginalModule:true,replacementBodies:false,loadedModuleCounts:sources.map(source=>source.cache.size),summary:{cases:cases.length,pass:cases.filter(row=>row.equal).length,fail:cases.filter(row=>!row.equal).length},cases};
const get=kind=>cases.find(row=>row.kind===kind).raw;
for(const kind of ['wrong-number-v','present-undefined-v','wrapped-wrong-v','inherited-v','getter-wrong-v','getter-throws-v','inherited-getter-v','changing-getter-v']){
 const r=get(kind);if(!r.selection.present||!r.selection.sameOwner||r.selection.reads!==0||r.selection.writes!==0)throw Error('assertion read or copied owner: '+kind);
}
if(get('getter-wrong-v').later.reads!==2||get('getter-throws-v').later.reads!==1||get('empty-v').later.returned!==true||get('one-point').later.returned!==true||!get('live-owner').later.live||!get('live-wrapped-owner').later.live)throw Error('source boundary expectation drift');
fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result.summary));if(result.summary.fail)process.exitCode=1;
