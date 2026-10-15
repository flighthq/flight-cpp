import assert from 'node:assert/strict';
import { realpathSync } from 'node:fs';
import { createRequire, register } from 'node:module';
import path from 'node:path';
import { test } from 'node:test';
import { pathToFileURL } from 'node:url';

const flightDirectory = realpathSync(process.env.FLIGHT_SCENE_MATERIAL_SOURCE);
const typescriptPath = createRequire(new URL('../../../.dependencies/flight-compiler/package.json', import.meta.url)).resolve('typescript');
register('./loader.mjs', import.meta.url, { data: { flightDirectory, typescriptPath, mutation: process.env.FLIGHT_SCENE_MATERIAL_MUTATION } });
const source = (name, file) => pathToFileURL(path.join(flightDirectory, 'packages', name, 'src', `${file}.ts`));
const { createMesh } = await import(source('scene3d', 'mesh'));
const { createNode3D } = await import(source('scene3d', 'sceneNode'));
const { findScene3DMaterialByName: find, getScene3DMaterials: collect } = await import(source('scene3d', 'sceneMaterial'));
const { createBlinnPhongMaterial, createStandardPbrMaterial } = await import(source('materials', 'contract'));
const { createMaterial3D } = await import(source('materials', 'material3d'));
const { createBoxMeshGeometry } = await import(source('mesh', 'meshGeometryBuilders'));
const { addNodeChild } = await import(source('node', 'contract'));
const mesh = (materials) => createMesh(createBoxMeshGeometry(), materials);
const named = (value, name) => Object.assign(value, { name });
const blinn = (name) => named(createBlinnPhongMaterial(), name);
const pbr = (name) => named(createStandardPbrMaterial(), name);

// The first nine cases port sceneMaterial.test.ts at the lock's exact Flight pin.
test('finds a named descendant material', () => {
  const value = blinn('canopy'), root = createNode3D();
  addNodeChild(root, mesh([value]));
  assert.equal(find(root, 'canopy'), value);
});
test('returns null for an absent name', () => {
  const root = createNode3D(); addNodeChild(root, mesh([blinn('canopy')]));
  assert.equal(find(root, 'fuselage'), null);
});
test('ignores anonymous materials', () => {
  const root = createNode3D(); addNodeChild(root, mesh([createBlinnPhongMaterial()]));
  assert.equal(find(root, 'canopy'), null);
});
test('searches nested descendants', () => {
  const value = pbr('deep'), root = createNode3D(), group = createNode3D();
  addNodeChild(root, group); addNodeChild(group, mesh([value]));
  assert.equal(find(root, 'deep'), value);
});
test('finds the root material itself', () => {
  const value = blinn('root-mat'); assert.equal(find(mesh([value]), 'root-mat'), value);
});
test('finds the first matching mesh in preorder', () => {
  const first = blinn('shared'), second = blinn('shared'), root = createNode3D();
  addNodeChild(root, mesh([first])); addNodeChild(root, mesh([second]));
  assert.equal(find(root, 'shared'), first);
});
test('lookup skips null material slots', () => {
  const value = blinn('canopy'); assert.equal(find(mesh([null, value]), 'canopy'), value);
});
test('collects distinct shared materials across the subtree', () => {
  const shared = blinn(null), other = pbr(null), root = createNode3D(), group = createNode3D();
  addNodeChild(root, mesh([shared])); addNodeChild(group, mesh([shared, other])); addNodeChild(root, group);
  const out = []; collect(root, out);
  assert.deepEqual(out, [shared, other]);
  assert.equal(out[0], shared); assert.equal(out[1], other);
});
test('collects no material from null slots or a mesh-free group', () => {
  const root = createNode3D(); addNodeChild(root, mesh([null]));
  const out = []; collect(root, out); assert.deepEqual(out, []);
});

test('mixed built-ins and the open vendor arm retain caller-owned arrays and exact owners', () => {
  const first = blinn('first'), second = pbr('second'), vendor = named(createMaterial3D('VendorMaterial'), 'vendor');
  const slots = [first, null, second, vendor, first], root = mesh(slots);
  assert.equal(root.materials, slots);
  const out = []; collect(root, out);
  assert.equal(root.materials, slots); assert.equal(slots.length, 5);
  assert.deepEqual(out, [first, second, vendor]);
  out.forEach((value, i) => assert.equal(value, [first, second, vendor][i]));
  assert.equal(find(root, 'vendor'), vendor);
  vendor.name = 'renamed';
  assert.equal(find(root, 'vendor'), null); assert.equal(find(root, 'renamed'), vendor);
  slots[0] = vendor;
  const changed = []; collect(root, changed); assert.deepEqual(changed, [vendor, second, first]);
});
test('enumeration appends, deduplicates by identity, and preserves exact depth-first first-appearance order', () => {
  const seed = blinn('seed'), rootMat = blinn('same'), equalLooking = blinn('same');
  const deepMat = pbr('deep'), lastMat = pbr('last'), root = mesh([rootMat, null]);
  const firstChild = mesh([equalLooking, rootMat]);
  addNodeChild(firstChild, mesh([deepMat, seed])); addNodeChild(root, firstChild); addNodeChild(root, mesh([lastMat]));
  const out = [seed], alias = out; collect(root, out);
  assert.equal(out, alias); assert.deepEqual(out, [seed, rootMat, equalLooking, deepMat, lastMat]);
  assert.notEqual(out[1], out[2]);
  collect(root, out); assert.deepEqual(out, [seed, rootMat, equalLooking, deepMat, lastMat]);
});
test('lookup prioritizes root and slot order, then visits a branch before its next sibling', () => {
  const rootMat = blinn('hit'), laterSlot = pbr('hit'), descendant = pbr('hit');
  const root = mesh([null, rootMat, laterSlot]); addNodeChild(root, mesh([descendant]));
  assert.equal(find(root, 'hit'), rootMat);
  const group = createNode3D(), branch = createNode3D(), deep = blinn('hit'), sibling = pbr('hit');
  addNodeChild(group, branch); addNodeChild(branch, mesh([deep])); addNodeChild(group, mesh([sibling]));
  assert.equal(find(group, 'hit'), deep);
});
test('a sparse slot preserves undefined separately from a null material', () => {
  const slots = new Array(2); slots[1] = null;
  const root = mesh(slots), out = [];
  collect(root, out);
  assert.equal(out.length, 1); assert.equal(out[0], undefined);
  assert.throws(() => find(root, 'missing'), TypeError);
  assert.equal(root.materials, slots); assert.equal(0 in slots, false);
});
