// Qualifies the approved bounded result-only mechanism; never runs an SDK or edits generated inventory.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
const args=Object.fromEntries(process.argv.slice(2).map(arg=>{const at=arg.indexOf('=');assert.ok(at>0);return[arg.slice(0,at),arg.slice(at+1)];}));
for(const key of ['--compiler','--baseline','--typescript','--cxx','--context','--output'])assert.ok(args[key],key+' required');
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..'),url=pathToFileURL(path.resolve(args['--compiler']));
const candidate=await import(url),baseline=await import(pathToFileURL(path.resolve(args['--baseline'])));
const{lowerTypeScriptSource}=await import(new URL('../../compiler-semantic/src/index.js',url));
const{default:ts}=await import(pathToFileURL(path.resolve(args['--typescript'])));
const hash=input=>createHash('sha256').update(input).digest('hex');
const lower=(file,source,packageName)=>{const r=lowerTypeScriptSource(ts.createSourceFile('/flight/'+file,source,ts.ScriptTarget.Latest,true),{packageName,upstreamDirectory:'/flight'});assert.deepEqual(r.diagnostics,[]);return r.module;};
const scratch=fs.mkdtempSync(path.join(os.tmpdir(),'particle-named-result-'));
try {
 const fixture='scripts/fixtures/particleNamedResultView.ts',source=fs.readFileSync(path.join(root,fixture),'utf8');
 const program=ts.createProgram([path.join(root,fixture)],{strict:true,noEmit:true,types:[],skipLibCheck:true,target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ESNext});
 assert.deepEqual(ts.getPreEmitDiagnostics(program).map(d=>ts.flattenDiagnosticMessageText(d.messageText,'\n')),[]);
 const api={};new Function('exports',ts.transpileModule(source,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.CommonJS}}).outputText)(api);
 let sourceChecks=0;const check=value=>{++sourceChecks;assert.ok(value);};
 const owned={config:{size:7},diagnostics:[],document:{size:13}},small={config:{size:3},diagnostics:[]};
 let calls=0,optionReads=0;
 api.wideProvider=text=>{++calls;check(text==='input');return owned;};api.smallProvider=()=>small;
 const options={get size(){++optionReads;throw new Error('unused option getter');},get width(){++optionReads;throw new Error('unused option getter');}};
 const returned=api.wideCodec.parse('input',options);check(returned===owned&&calls===1&&optionReads===0);
 check(api.sameWide()&&!api.distinct()&&api.wideCodec.parse===api.parseWide);
 const config={size:19},diagnostics=[{code:1}];returned.config=config;returned.diagnostics=diagnostics;
 check(owned.config===config&&owned.diagnostics===diagnostics&&owned.document.size===13);
 api.writeConfig('input',config);check(owned.config===config&&owned.diagnostics.length===0&&optionReads===0);
 check(api.smallCodec.parse('small',options)===small&&api.smallCodec.parse===api.parseSmall&&optionReads===0);
 const sentinel=new Error('exact source error');api.wideProvider=()=>{++calls;throw sentinel;};
 const beforeCalls=calls;try{api.wideCodec.parse('throw',options);check(false);}catch(error){check(error===sentinel&&calls===beforeCalls+1&&optionReads===0);}
 const module=lower('packages/example/src/named-result.ts',source,'@flighthq/example');
 const emitted=candidate.emitIrModuleCpp(module,{runtimeProfile:'flight-cpp'}).contents;assert.ok(!emitted.includes('NOT GENERATED'));
 fs.writeFileSync(path.join(scratch,'named-result.hpp'),emitted);
 const binary=path.join(scratch,'native-control'),nativeFixture='scripts/fixtures/particleNamedResultView.cpp';
 const compileArguments=['-std=c++20','-ffp-contract=off','-I'+scratch,...['overrides/include','include','generated/include'].map(dir=>'-I'+path.join(root,dir)),path.join(root,nativeFixture),'-o',binary];
 const build=spawnSync(path.resolve(args['--cxx']),compileArguments,{encoding:'utf8',maxBuffer:16*1024*1024});
 fs.writeFileSync(path.resolve(args['--output'])+'.native-build.log',build.stderr+build.stdout);assert.equal(build.status,0,build.stderr||build.stdout);
 const run=spawnSync(binary,[],{encoding:'utf8'});fs.writeFileSync(path.resolve(args['--output'])+'.native-run.log',run.stdout+run.stderr);assert.equal(run.status,0,run.stderr||run.stdout);
 const input=fs.readFileSync(path.resolve(args['--context'])),context=JSON.parse(input);
 // Exactly the existing declared type-only result patch; all original runtime bodies remain intact.
 const codec='packages/types/src/ParticleFormatCodec.ts',typePatch='source-patches/particle-codec-document-result-owned-view.patch';
 fs.mkdirSync(path.dirname(path.join(scratch,codec)),{recursive:true});fs.copyFileSync(path.join(root,'.dependencies/flight',codec),path.join(scratch,codec));
 const applied=spawnSync('git',['apply',path.join(root,typePatch)],{cwd:scratch,encoding:'utf8'});assert.equal(applied.status,0,applied.stderr);
 context.modules=context.modules.map(m=>m.source===codec?lower(codec,fs.readFileSync(path.join(scratch,codec),'utf8'),'@flighthq/types'):m);
 const originalIr=JSON.stringify(context.modules),originalModules=[];
 const emit=(backend,module)=>backend.createCppCompilerBackend().createEmissionSession(context).emitModule(module).map(file=>file.contents).join('\n');
 for(const file of ['pixiParse.ts','spineParse.ts']){
  const original=context.modules.find(m=>m.source==='packages/particles-formats/src/'+file);assert.ok(original);
  const before=emit(baseline,original),after=emit(candidate,original);assert.equal(after,before);assert.ok(!after.includes('NOT GENERATED'));
  originalModules.push({file,sourceModuleIrSha256:hash(JSON.stringify(original)),emittedSha256:hash(after),byteIdentical:true});
 }
 const registration=context.modules.find(m=>m.source==='packages/particles-formats/src/registerBuiltInParticleFormats.ts');assert.ok(registration);
 const before=emit(baseline,registration),after=emit(candidate,registration);assert.ok(!after.includes('NOT GENERATED'));
 const names=['parse_pixi_particle_document','parse_spine_particle_document'];
 for(const name of names)assert.ok(after.includes('::from<&flight::particles_formats::'+name+'>'));
 const excludeSelected=contents=>contents.split('\n').filter(line=>!names.some(name=>line.includes(name+'(contextual_callable_argument'))).join('\n');
 assert.equal(excludeSelected(after),excludeSelected(before),'only the two original descriptor result wrappers may change');
 assert.equal(JSON.stringify(context.modules),originalIr,'original IR mutated');
 const patch='source-patches/compiler-named-required-result-view.patch';
 const report={active:fs.readFileSync(path.join(root,'source-patches/manifest.json'),'utf8').includes('compiler-named-required-result-view'),decisionRequired:false,fixtureStrict:true,sourceSyntheticChecks:sourceChecks,nativeSyntheticCompatibility:JSON.parse(run.stdout),sourceFixture:{file:fixture,sha256:hash(source),emittedSha256:hash(emitted)},nativeFixture:{file:nativeFixture,sha256:hash(fs.readFileSync(path.join(root,nativeFixture)))},originalSavedGraph:{sha256:hash(input),declaredTypeOnlyPatch:{file:typePatch,sha256:hash(fs.readFileSync(path.join(root,typePatch)))},moduleIrUnchanged:true,originalModules,registration:{sourceModuleIrSha256:hash(JSON.stringify(registration)),emittedSha256:hash(after),onlyTwoResultWrappersChanged:true,newNamedOrigins:names}},patch:{file:patch,sha256:fs.existsSync(path.join(root,patch))?hash(fs.readFileSync(path.join(root,patch))):null},normalSdk:false,scope:'Approved result-only same-owner source and synthetic native qualification. Actual original Pixi/Spine modules are byte-identical; only two existing descriptor result wrappers change. Installed normal output retains its separately reported failures; no SDK/header/package promotion.'};
 fs.writeFileSync(path.resolve(args['--output']),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
}finally{fs.rmSync(scratch,{recursive:true,force:true});}
