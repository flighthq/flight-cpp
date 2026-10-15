import { existsSync, readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

let flightDirectory;
let mutation;
let typescript;
export function initialize(data) {
  ({ flightDirectory, mutation } = data);
  typescript = createRequire(import.meta.url)(data.typescriptPath);
}
export function resolve(specifier, context, nextResolve) {
  if (specifier.startsWith('@flighthq/')) {
    const [name, ...suffix] = specifier.slice('@flighthq/'.length).split('/');
    const source = suffix.length === 0 ? 'index' : suffix.join('/');
    return { shortCircuit: true, url: pathToFileURL(path.join(flightDirectory, 'packages', name, 'src', `${source}.ts`)).href };
  }
  // Flight source also contains extensionless and emitted .js relative imports.
  if (specifier.startsWith('.') && context.parentURL?.startsWith('file:')) {
    const filename = fileURLToPath(new URL(specifier, context.parentURL));
    if (filename.startsWith(`${flightDirectory}${path.sep}`)) {
      for (const candidate of [`${filename}.ts`, filename.replace(/\.js$/u, '.ts')]) {
        if (candidate.endsWith('.ts') && existsSync(candidate)) return { shortCircuit: true, url: pathToFileURL(candidate).href };
      }
    }
  }
  return nextResolve(specifier, context);
}
export function load(url, context, nextLoad) {
  if (!url.startsWith('file:') || !url.endsWith('.ts')) return nextLoad(url, context);
  const filename = fileURLToPath(url);
  if (!filename.startsWith(`${flightDirectory}${path.sep}`)) return nextLoad(url, context);
  let source = readFileSync(filename, 'utf8');
  if (filename.endsWith(`${path.sep}sceneMaterial.ts`) && mutation) {
    const replacements = {
      dedup: ['!out.includes(material)', 'true'],
      root: ['collectNodeMaterials(root, out);', ''],
      order: ['for (let i = 0; i < materials.length; i++)', 'for (let i = materials.length - 1; i >= 0; i--)'],
      identity: ['out.push(material)', 'out.push({ ...material })'],
    };
    const [from, to] = replacements[mutation] ?? [];
    if (!from || !source.includes(from)) throw new Error(`Material mutation ${mutation} no longer matches`);
    source = source.replaceAll(from, to);
  }
  return { format: 'module', shortCircuit: true, source: typescript.transpileModule(source, { fileName: filename, compilerOptions: { module: typescript.ModuleKind.ESNext, target: typescript.ScriptTarget.ES2022 } }).outputText };
}
