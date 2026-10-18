import assert from 'node:assert/strict';
import { test } from 'node:test';

import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  classInterfaceAdoptionOptions,
  loadClassInterfaceAdoptions,
  unmatchedClassInterfaceAdoptionSites,
} from './classInterfaceAdoptions.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function withDeclaration(declaration, run) {
  const directory = mkdtempSync(path.join(tmpdir(), 'flight-cpp-adoptions-'));
  try {
    writeFileSync(path.join(directory, 'class-interface-adoptions.json'), JSON.stringify(declaration));
    return run(directory);
  } finally {
    rmSync(directory, { force: true, recursive: true });
  }
}

const adoption = {
  class: 'ByteReader',
  classPackage: '@flighthq/example',
  derive: true,
  id: 'reader',
  interface: 'Reader',
  interfacePackage: '@flighthq/types',
  parameters: [{ function: 'consume', package: '@flighthq/example', parameter: 'reader' }],
  reason: 'the class is the interface the caller passes',
  request: 'docs/compiler-requests/class-instance-as-implemented-interface.md',
};
const declaration = { adoptions: [adoption], schema: 'flight-cpp-class-interface-adoptions/1' };

test('the committed declaration loads', () => {
  assert.ok(loadClassInterfaceAdoptions(root).length > 0);
});

test('an absent declaration carries nothing', () => {
  const directory = mkdtempSync(path.join(tmpdir(), 'flight-cpp-adoptions-'));
  try {
    assert.deepEqual(loadClassInterfaceAdoptions(directory), []);
  } finally {
    rmSync(directory, { force: true, recursive: true });
  }
});

test('the compiler option drops the bookkeeping and keeps the pair and sites', () => {
  assert.deepEqual(classInterfaceAdoptionOptions([adoption]), [
    {
      class: 'ByteReader',
      derive: true,
      interface: 'Reader',
      parameters: [{ function: 'consume', package: '@flighthq/example', parameter: 'reader' }],
    },
  ]);
});

test('a declaration with a missing reason, a missing package, a bad name or a non-boolean derive is refused', () => {
  for (const broken of [
    { ...adoption, reason: '' },
    { ...adoption, classPackage: '' },
    { ...adoption, class: 'Byte Reader' },
    { ...adoption, derive: 'yes' },
    { ...adoption, parameters: [{ function: 'consume', package: '', parameter: 'reader' }] },
  ]) {
    assert.throws(() => withDeclaration({ ...declaration, adoptions: [broken] }, loadClassInterfaceAdoptions));
  }
});

test('a site is reported only for a package the run read, and only when its source lost it', () => {
  const sources = new Map([
    ['@flighthq/example', ['export class ByteReader { }\nexport function consume(reader: ByteReader): void { }']],
    ['@flighthq/types', ['export interface Reader { pos: number }']],
  ]);
  assert.deepEqual(unmatchedClassInterfaceAdoptionSites([adoption], sources), []);
  const withoutClass = new Map([...sources].map(([name, texts]) => [name, texts.map((text) => text.replace('class ByteReader', 'class OtherReader'))]));
  assert.deepEqual(unmatchedClassInterfaceAdoptionSites([adoption], withoutClass), [
    'reader: @flighthq/example class ByteReader',
  ]);
  assert.deepEqual(unmatchedClassInterfaceAdoptionSites([adoption], new Map([['@flighthq/other', ['nothing']]])), []);
});

test('a class that declares its own base cannot adopt a native interface base', () => {
  const sources = new Map([
    [
      '@flighthq/example',
      ['export class Base { }\nexport class ByteReader extends Base { }\nexport function consume(reader: ByteReader): void { }'],
    ],
    ['@flighthq/types', ['export interface Reader { pos: number }']],
  ]);
  assert.deepEqual(unmatchedClassInterfaceAdoptionSites([adoption], sources), [
    'reader: ByteReader declares a base, so a native interface base is not the bounded shape',
  ]);
});
