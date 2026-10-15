import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const option = (name, fallback) => process.argv.find((value) => value.startsWith(`${name}=`))?.slice(name.length + 1) ?? fallback;
const header = path.resolve(option('--header', path.join(root, 'generated/include/flight/scene3d/instanced_mesh.hpp')));
const generated = path.resolve(option('--generated', path.join(root, 'generated')));
const baseline = option('--baseline-header', '');
const compiler = process.env.CXX || 'c++';
if (spawnSync(compiler, ['--version'], { encoding: 'utf8' }).status !== 0) {
  process.stdout.write(`Instanced native oracle skipped: no compiler at ${compiler}.\n`);
  process.exit(0);
}
function functions(filename) {
  const source = readFileSync(filename, 'utf8');
  const start = source.indexOf('inline std::optional<flight::Ref<flight::types::InstancedMeshSignals>> get_instanced_mesh_signals(');
  const end = source.indexOf('inline void invalidate_instanced_mesh(', start);
  assert.ok(start >= 0 && end > start, 're-derive focused extraction when emitted declaration boundaries change');
  const text = source.slice(start, end);
  for (const name of ['get', 'initialize', 'create', 'enable']) assert.ok(text.includes(`${name}_instanced_mesh_signals(`));
  return text;
}
const emitted = functions(header);
const factory = 'inline flight::Ref<flight::types::InstancedMeshSignals> create_instanced_mesh_signals() {';
assert.equal(emitted.split(factory).length, 2);
// Observe entry to the actual emitted factory. Its allocation, field initialization,
// return body and real input/output types stay intact; no supplier is substituted.
const instrument = (text) => text.replace(factory, `${factory}\n  ++factory_calls;\n  if (fail_factory) throw std::runtime_error("factory-probe");`);
const fixture = readFileSync(path.join(root, 'scripts/fixtures/instanced-signals/native.cpp'), 'utf8');
const temporary = mkdtempSync(path.join(tmpdir(), 'flight-cpp-instanced-native-'));
const report = { header, generated, compiler, extractedSha256: createHash('sha256').update(emitted).digest('hex'),
  scope: 'Four actual emitted functions over real generated InstancedMesh/runtime/group/Signal owners; excludes mesh construction and whole-module parity', controls: [] };
function run(label, text) {
  const input = path.join(temporary, `${label}.cpp`);
  const output = path.join(temporary, label);
  writeFileSync(input, fixture.replace('// @INSTANCED_FUNCTIONS@', instrument(text)));
  const built = spawnSync(compiler, ['-std=c++20', '-ffp-contract=off', ...['include', 'overrides/include'].map((name) => `-I${path.join(root, name)}`),
    `-I${path.join(generated, 'include')}`, input, '-o', output], { encoding: 'utf8' });
  if (built.status !== 0) return { label, build: built.status, run: null, diagnostic: `${built.stdout}${built.stderr}` };
  const ran = spawnSync(output, [], { encoding: 'utf8' });
  return { label, build: 0, run: ran.status, signal: ran.signal, diagnostic: `${ran.stdout}${ran.stderr}` };
}
function replaceOnce(text, before, after) {
  assert.equal(text.split(before).length, 2, `mutation requires exactly one ${before}`);
  return text.replace(before, after);
}
try {
  const candidate = run('candidate', emitted);
  report.controls.push(candidate);
  assert.equal(candidate.build, 0, candidate.diagnostic);
  assert.equal(candidate.run, 0, candidate.diagnostic);
  process.stdout.write(candidate.diagnostic);
  if (baseline) {
    const control = run('baseline', functions(path.resolve(baseline)));
    report.controls.push(control);
    assert.notEqual(control.build, 0, 'original nullish assignment must fail its variant/optional control');
    assert.match(control.diagnostic, /has_value|no member named 'value'/u);
    process.stdout.write('Original emitted nullish-assignment control fails at variant optional methods.\n');
  }
  if (process.argv.includes('--mutations')) {
    const absence = '(std::holds_alternative<flight::Null>(signals) || std::holds_alternative<flight::Undefined>(signals))';
    const stored = '(runtime->instanced_mesh_signals = std::variant<flight::Ref<flight::types::InstancedMeshSignals>, flight::Null, flight::Undefined>{std::in_place_type<flight::Ref<flight::types::InstancedMeshSignals>>, std::get<0>(signals)});';
    const mutations = [
      ['null-only', replaceOnce(emitted, absence, 'std::holds_alternative<flight::Null>(signals)')],
      ['undefined-only', replaceOnce(emitted, absence, 'std::holds_alternative<flight::Undefined>(signals)')],
      ['eager-factory', replaceOnce(emitted, '  auto runtime = target->entity_runtime_key.value();', '  (void)create_instanced_mesh_signals();\n  auto runtime = target->entity_runtime_key.value();')],
      ['missing-store', replaceOnce(emitted, stored, '(void)runtime;')],
      ['copied-group', replaceOnce(emitted, stored, stored.replace('std::get<0>(signals)', 'flight::make_ref<flight::types::InstancedMeshSignals>(*std::get<0>(signals))'))],
      ['copied-runtime', replaceOnce(emitted, '  auto runtime = target->entity_runtime_key.value();', '  auto runtime = flight::make_ref<flight::types::InstancedMeshRuntime>(*target->entity_runtime_key.value());')],
    ];
    for (const [name, text] of mutations) {
      const result = run(name, text);
      report.controls.push(result);
      assert.equal(result.build, 0, result.diagnostic);
      assert.ok(result.run !== 0, `native oracle missed ${name}`);
      assert.match(result.diagnostic, /FAIL:/u, `${name} must fail a semantic assertion`);
      process.stdout.write(`Detected Instanced native mutation: ${name}\n`);
    }
  }
  report.result = 'pass';
} finally {
  const reportFile = option('--report', '');
  if (reportFile) writeFileSync(reportFile, `${JSON.stringify(report, null, 2)}\n`);
  rmSync(temporary, { recursive: true, force: true });
}
