import assert from 'node:assert/strict';
import { test } from 'node:test';

import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  loadStructuralValueCarriers,
  structuralValueCarrierOptions,
  unmatchedStructuralValueCarrierSites,
} from './structuralValueCarriers.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function withDeclaration(declaration, run) {
  const directory = mkdtempSync(path.join(tmpdir(), 'flight-cpp-carriers-'));
  try {
    writeFileSync(path.join(directory, 'structural-value-carriers.json'), JSON.stringify(declaration));
    return run(directory);
  } finally {
    rmSync(directory, { force: true, recursive: true });
  }
}

const carrier = {
  base: 'Target',
  callbackParameters: [{ field: 'encodeCapture', owner: 'Screen', package: '@flighthq/types', position: 0 }],
  fields: [{ field: 'frame', owner: 'Runtime', package: '@flighthq/types' }],
  id: 'screen',
  nominal: 'Screen',
  parameters: [{ function: 'bindScreen', package: '@flighthq/render', parameter: 'target' }],
  reason: 'a narrowed look-alike reaches these sites',
  request: 'docs/compiler-requests/narrowed-value-escape.md',
  target: 'flight::NominalOrStructuralView<Screen, Target>',
};
const declaration = { carriers: [carrier], schema: 'flight-cpp-structural-value-carriers/1' };

test('the committed declaration loads', () => {
  assert.ok(loadStructuralValueCarriers(root).length > 0);
});

test('an absent declaration carries nothing', () => {
  const directory = mkdtempSync(path.join(tmpdir(), 'flight-cpp-carriers-'));
  try {
    assert.deepEqual(loadStructuralValueCarriers(directory), []);
  } finally {
    rmSync(directory, { force: true, recursive: true });
  }
});

test('a declaration with a missing reason, a non-carrier target, or no sites is refused', () => {
  for (const broken of [
    { ...carrier, reason: '' },
    { ...carrier, target: 'flight::Ref<Screen>' },
    { ...carrier, callbackParameters: [], fields: [], parameters: [] },
    { ...carrier, callbackParameters: [{ ...carrier.callbackParameters[0], position: -1 }] },
  ]) {
    assert.throws(() => withDeclaration({ ...declaration, carriers: [broken] }, loadStructuralValueCarriers));
  }
});

test('the compiler option omits bookkeeping', () => {
  const [option] = structuralValueCarrierOptions([carrier]);
  assert.deepEqual(Object.keys(option).sort(), ['base', 'callbackParameters', 'fields', 'nominal', 'parameters', 'target']);
});

test('a site the pinned source no longer declares is reported, and packages outside the run are not judged', () => {
  const sources = new Map([
    ['@flighthq/render', ['export function bindScreen(state: State, target: Screen): void {}']],
    ['@flighthq/types', ['export interface Screen { encodeCapture: null }\nexport interface Runtime { frame: Screen | null }']],
  ]);
  assert.deepEqual(unmatchedStructuralValueCarrierSites([carrier], sources), []);
  const renamed = { ...carrier, parameters: [{ ...carrier.parameters[0], function: 'bindScreenGone' }] };
  assert.deepEqual(unmatchedStructuralValueCarrierSites([renamed], sources), [
    'screen: @flighthq/render function bindScreenGone parameter target',
  ]);
  const retyped = new Map([...sources, ['@flighthq/render', ['export function bindScreen(target: Other): void {}']]]);
  assert.equal(unmatchedStructuralValueCarrierSites([carrier], retyped).length, 1);
  assert.deepEqual(unmatchedStructuralValueCarrierSites([renamed], new Map()), []);
});
