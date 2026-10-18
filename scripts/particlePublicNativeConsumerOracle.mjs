// Actual generated public headers plus the compiler-emitted original source consumer. No SDK generation.
import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
const emitOnly = process.argv.includes('--emit-only');
const consumerOnly = process.argv.includes('--consumer-only');
const headersOnly = process.argv.includes('--headers-only');
assert.ok([emitOnly,consumerOnly,headersOnly].filter(Boolean).length<=1);
const args = Object.fromEntries(process.argv.slice(2).filter(arg=>!['--emit-only','--consumer-only','--headers-only'].includes(arg)).map(arg=>{const at=arg.indexOf('=');assert.ok(at>0);return[arg.slice(0,at),arg.slice(at+1)];}));
for(const key of ['--compiler','--typescript','--context','--inventory','--output']) assert.ok(args[key],key+' required');
if(!emitOnly) assert.ok(args['--cxx'],'--cxx required');
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..'),inventory=path.resolve(args['--inventory']);
const overlay=args['--overlay']?path.resolve(args['--overlay']):undefined;
const selected=file=>overlay&&fs.existsSync(path.join(overlay,'include',file))?path.join(overlay,'include',file):path.join(inventory,'include',file);
const url=pathToFileURL(path.resolve(args['--compiler']));
const {createCppCompilerBackend}=await import(url);
const {lowerTypeScriptSource}=await import(new URL('../../compiler-semantic/src/index.js',url));
const {default:ts}=await import(pathToFileURL(path.resolve(args['--typescript'])));
const hash=input=>createHash('sha256').update(input).digest('hex');
const lower=(file,source,packageName)=>{const result=lowerTypeScriptSource(ts.createSourceFile('/flight/'+file,source,ts.ScriptTarget.Latest,true),{packageName,upstreamDirectory:'/flight'});assert.deepEqual(result.diagnostics,[]);return result.module;};
const scratch=fs.mkdtempSync(path.join(os.tmpdir(),'particle-public-native-'));
const output=path.resolve(args['--output']);
fs.mkdirSync(path.dirname(output),{recursive:true});
const command=(commandArgs)=>new Promise(resolve=>{const child=spawn(path.resolve(args['--cxx']),commandArgs,{stdio:['ignore','pipe','pipe']});let stdout='',stderr='';child.stdout.on('data',data=>stdout+=data);child.stderr.on('data',data=>stderr+=data);child.on('error',error=>resolve({exit:null,stdout,stderr,error:error.message}));child.on('close',exit=>resolve({exit,stdout,stderr}));});
try {
 const input=fs.readFileSync(path.resolve(args['--context'])),context=JSON.parse(input);
 // Apply the EXACT declared type-only patch to its raw pinned module. No runtime source body is changed.
 const codec='packages/types/src/ParticleFormatCodec.ts',patch='source-patches/particle-codec-document-result-owned-view.patch';
 const codecFile=path.join(scratch,codec);fs.mkdirSync(path.dirname(codecFile),{recursive:true});
 fs.copyFileSync(path.join(root,'.dependencies/flight',codec),codecFile);
 const applied=spawnSync('git',['apply',path.join(root,patch)],{cwd:scratch,encoding:'utf8'});assert.equal(applied.status,0,applied.stderr);
 context.modules=context.modules.map(module=>module.source===codec?lower(codec,fs.readFileSync(codecFile,'utf8'),'@flighthq/types'):module);
 const fixture='scripts/fixtures/particlePublicConsumer.ts';
 const consumer=lower('packages/particles-formats/src/particlePublicConsumer.ts',fs.readFileSync(path.join(root,fixture),'utf8'),'@flighthq/particles-formats');context.modules.push(consumer);
 const files=createCppCompilerBackend().createEmissionSession(context).emitModule(consumer);assert.equal(files.length,1);const emitted=files[0].contents;assert.ok(!emitted.includes('NOT GENERATED'));
 const helperCalls=[...emitted.matchAll(/return flight::particles_formats::(parse_particle_config(?:_document)?_flight_readonly_options_[a-f0-9]+)\(text, options\);/g)].map(match=>match[1]);assert.equal(helperCalls.length,2);
 fs.writeFileSync(path.join(scratch,'particle-public-consumer.hpp'),emitted);
 const headers=['flight/particles_formats/unity_parse.hpp','flight/particles_formats/_internal_index.hpp','flight/particles_formats/contract.hpp','flight/particles_formats/register_built_in_particle_formats.hpp'];
 const inventoryFiles=[...headers,'flight/particles_formats/parse_particle_config.hpp','flight/types/particle_format_codec.hpp','flight/sdk/structural_members.hpp'];
 inventoryFiles.push('flight/types/import_diagnostic.hpp','flight/importdiagnostics/import_diagnostic_collector.hpp');
 const before=inventoryFiles.map(file=>({file,path:selected(file),sha256:hash(fs.readFileSync(selected(file)))}));
 const parserHeader=fs.readFileSync(selected('flight/particles_formats/parse_particle_config.hpp'),'utf8');
 const helpersInInstalledHeader=helperCalls.every(name=>parserHeader.includes(' '+name+'('));
 const report={normalSdk:false,scope:overlay?'Scratch original-module overlay on the selected frozen inventory; no normal output or header promotion.':'Actual selected generated inventory. Original public TypeScript consumer emitted with the exact declared type-only codec result patch. No local parser/detector/codec implementation or invented public declaration.',overlay,inputContextSha256:hash(input),declaredTypeOnlyPatch:{file:patch,sha256:hash(fs.readFileSync(path.join(root,patch)))},sourceConsumer:{file:fixture,sha256:hash(fs.readFileSync(path.join(root,fixture))),emittedSha256:hash(emitted),helperCalls,helpersInInstalledHeader},nativeFixture:{file:'scripts/fixtures/particlePublicConsumer.cpp',sha256:hash(fs.readFileSync(path.join(root,'scripts/fixtures/particlePublicConsumer.cpp')))},inventory:before,emitOnly,consumerOnly,headersOnly,headerChecks:consumerOnly?'omitted; consumer only':'four independent headers',consumerChecks:headersOnly?'omitted; header controls only':'compile/link/run',compiled:false,headers:[]};
 if(!emitOnly){
  const includes=['-I'+scratch,'-I'+path.join(root,'overrides/include'),'-I'+path.join(root,'include'),...(overlay?['-I'+path.join(overlay,'include')]:[]),'-I'+path.join(inventory,'include'),'-I'+path.join(root,'generated/include'),'-I'+path.join(root,'out/tooling/sdl/include'),'-I'+path.join(root,'out/tooling/gles/include')];
  const base=['-std=c++20','-ffp-contract=off','-g0',...includes];
  // At most two native compiler processes; all four requested headers are judged independently.
  if(!consumerOnly)for(let start=0;start<headers.length;start+=2){const rows=await Promise.all(headers.slice(start,start+2).map(async(file,index)=>{const number=start+index,source=path.join(scratch,'header-'+number+'.cpp');fs.writeFileSync(source,'#include <'+file+'>\n');const result=await command([...base,'-c',source,'-o',path.join(scratch,'header-'+number+'.o')]);const log=output+'.header-'+number+'.log';fs.writeFileSync(log,result.stderr+result.stdout);return{file,pass:result.exit===0,exit:result.exit,log,error:result.error};}));report.headers.push(...rows);}
  if(!headersOnly){const binary=path.join(scratch,'consumer');const built=await command([...base,path.join(root,'scripts/fixtures/particlePublicConsumer.cpp'),'-o',binary]);fs.writeFileSync(output+'.native-build.log',built.stderr+built.stdout);report.compiled=built.exit===0;report.compileExit=built.exit;report.compileError=built.error;
  if(report.compiled){const run=spawnSync(binary,[],{encoding:'utf8'});fs.writeFileSync(output+'.native-run.log',run.stdout+run.stderr);report.runExit=run.status;try{report.native=JSON.parse(run.stdout);}catch{report.native={output:run.stdout,error:run.stderr};}report.publicNativeQualified=run.status===0&&helpersInInstalledHeader;}else report.publicNativeQualified=false;
  }
 }
 for(const file of before)assert.equal(hash(fs.readFileSync(file.path)),file.sha256,'selected header changed during native control');
 report.inventoryUnchanged=true;fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
 if(!emitOnly)process.exitCode=(headersOnly||report.publicNativeQualified)&&report.headers.every(header=>header.pass)?0:1;
}finally{fs.rmSync(scratch,{recursive:true,force:true});}
