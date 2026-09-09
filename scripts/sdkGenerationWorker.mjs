import { existsSync, lstatSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { parentPort, workerData } from 'node:worker_threads';
import { pathToFileURL } from 'node:url';

// One worker owns whole package output directories. Workers never share compiler state or output
// files; the parent sorts their results before writing the deterministic manifest and refusal ledger.

const { compilerEntry, flightDirectory, outputRoot, packageNames } = workerData;
const { compileCompilerCommandLineRequest } = await import(pathToFileURL(compilerEntry));
const packageResults = [];
const refusals = [];

for (const packageName of packageNames) {
  const packageBaseName = packageName.slice('@flighthq/'.length);
  const sourceDirectory = path.join(flightDirectory, 'packages', packageBaseName, 'src');
  const sources = existsSync(sourceDirectory) ? listSourceFiles(sourceDirectory) : [];
  const cppPackage = packageBaseName.replaceAll('-', '_');
  const outputDirectory = path.join(outputRoot, 'include', 'flight', cppPackage);
  let compilation = { emitted: 0, refusals: [] };
  if (sources.length > 0) {
    compilation = compileCompilerCommandLineRequest(
      {
        argv: [
          sourceDirectory,
          '--target',
          'cpp',
          '--out',
          outputDirectory,
          '--package',
          packageName,
          '--report',
        ],
      },
      {
        listSourceFiles: () => sources,
        write: () => {},
        writeError: () => {},
        writeOutputFile: (directory, relativePath, contents) => {
          const target = path.join(directory, relativePath);
          mkdirSync(path.dirname(target), { recursive: true });
          writeFileSync(target, contents);
        },
      },
    );
  }

  const packageRefusals = compilation.refusals.map((refusal) => ({
    module: refusal.module,
    package: packageName,
    reason: refusal.reason,
  }));
  if (compilation.emitted + packageRefusals.length !== sources.length) {
    throw new Error(`${packageName} did not report one outcome for each source module`);
  }
  const emittedFiles = existsSync(outputDirectory) ? filesUnder(outputDirectory).length : 0;
  if (emittedFiles !== compilation.emitted) {
    throw new Error(
      `${packageName} emitted ${String(compilation.emitted)} modules into ${String(emittedFiles)} files`,
    );
  }
  refusals.push(...packageRefusals);
  packageResults.push({
    cppIncludePrefix: `flight/${cppPackage}`,
    emittedModules: compilation.emitted,
    package: packageName,
    refusedModules: packageRefusals.length,
    sourceModules: sources.length,
  });
}

parentPort.postMessage({ packageResults, refusals });

function listSourceFiles(directory) {
  return filesUnder(directory)
    .filter((filename) => filename.endsWith('.ts') && !filename.endsWith('.d.ts') && !filename.endsWith('.test.ts'))
    .map((sourcePath) => ({
      contents: readFileSync(sourcePath, 'utf8'),
      moduleName: portable(path.relative(directory, sourcePath)),
      sourcePath,
    }));
}

function filesUnder(directory) {
  const files = [];
  for (const entry of readdirSync(directory).sort()) {
    const filename = path.join(directory, entry);
    const status = lstatSync(filename);
    if (status.isSymbolicLink()) continue;
    if (status.isDirectory()) files.push(...filesUnder(filename));
    else files.push(filename);
  }
  return files;
}

function portable(filename) {
  return filename.split(path.sep).join('/');
}
