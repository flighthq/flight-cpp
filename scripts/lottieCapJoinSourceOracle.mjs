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
const output = path.resolve(arg('output', path.join(root, 'out/lottie-cap-join-source-oracle.json')));
const pin = 'a62784923f3be814463286c9fb28edfe6ab15789';
const relative = 'packages/scene2d-formats/src/lottieStrokeShapeItem.ts';
const original = spawnSync('git', ['show', `${pin}:${relative}`], { cwd: raw, encoding: 'utf8' });
if (original.status || original.stdout !== fs.readFileSync(path.join(raw, relative), 'utf8')) {
  throw new Error('Raw stroke source must be the exact pinned original');
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
const values=[['undefined',undefined,'std::nullopt'],['negative-infinity',-Infinity,'-std::numeric_limits<double>::infinity()'],['negative-one',-1,'-1.0'],['negative-zero',-0,'-0.0'],['zero',0,'0.0'],['one',1,'1.0'],['two',2,'2.0'],['three',3,'3.0'],['four',4,'4.0'],['infinity',Infinity,'std::numeric_limits<double>::infinity()'],['nan',NaN,'std::numeric_limits<double>::quiet_NaN()']];
const cases=values.map(([label,value,cpp])=>{const pair=sources.map(s=>({caps:s.api.mapLottieLineCap(value),joints:s.api.mapLottieLineJoin(value)}));return{label,cpp,raw:pair[0],patched:pair[1],equal:JSON.stringify(pair[0])===JSON.stringify(pair[1])};});
const hash=d=>createHash('sha256').update(fs.readFileSync(path.join(d,relative))).digest('hex');
const result={schema:'flight-original-lottie-cap-join-source/1',rawPin:pin,rawSourceSha256:hash(raw),patchedSourceSha256:hash(patched),wholeOriginalModule:true,replacementBodies:false,loadedModuleCounts:sources.map(s=>s.cache.size),scope:'Two original exported cap/join maps, complete original Stroke module loaded. No native whole-handler execution claimed.',summary:{cases:cases.length,functionChecks:cases.length*2,pass:cases.filter(c=>c.equal).length,fail:cases.filter(c=>!c.equal).length},cases};
fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(result,null,2)+'\n');
const fixture=path.resolve(arg('native-cases',path.join(path.dirname(output),'lottie-cap-join-source-cases.inc')));
fs.writeFileSync(fixture,'// Source-oracle data from complete original JavaScript module; no native function body.\nstatic const SourceCase source_cases[] = {\n'+cases.map(c=>`  {${c.cpp}, ${JSON.stringify(c.raw.caps)}, ${JSON.stringify(c.raw.joints)}},`).join('\n')+'\n};\n');
console.log(JSON.stringify(result.summary));if(result.summary.fail)process.exitCode=1;
