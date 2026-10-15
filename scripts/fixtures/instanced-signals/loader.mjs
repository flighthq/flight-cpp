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
  if (filename === path.join(flightDirectory, 'packages/scene3d/src/instancedMesh.ts')) {
    if (mutation) {
      const replacements = {
        'null-only': ['if (signals == null)', 'if (signals === null)'],
        'undefined-only': ['if (signals == null)', 'if (signals === undefined)'],
        'eager-factory': ['  let signals = runtime.instancedMeshSignals;', '  const unused = createInstancedMeshSignals();\n  let signals = runtime.instancedMeshSignals;'],
        'double-read': ['  let signals = runtime.instancedMeshSignals;', '  let signals = runtime.instancedMeshSignals;\n  void runtime.instancedMeshSignals;'],
        'read-back': ['  return signals;\n}', '  return runtime.instancedMeshSignals!;\n}'],
      };
      const [from, to] = replacements[mutation] ?? [];
      if (!from || !source.includes(from)) throw new Error(`Instanced signals mutation ${mutation} no longer matches`);
      source = source.replace(from, to);
    }
    // Count the real factory at entry; retain its allocation/initialization/return body.
    const factory = 'export function createInstancedMeshSignals(): InstancedMeshSignals {';
    if (!source.includes(factory)) throw new Error('Pinned signal factory no longer matches');
    source = source.replace(factory, factory + "\n  (globalThis as any)[Symbol.for('flight.instanced-signals.factory-probe')]?.();");
  }
  return { format: 'module', shortCircuit: true, source: typescript.transpileModule(source, { fileName: filename, compilerOptions: { module: typescript.ModuleKind.ESNext, target: typescript.ScriptTarget.ES2022 } }).outputText };
}
