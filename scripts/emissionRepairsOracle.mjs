import assert from 'node:assert/strict';

import { applyEmissionRepairs } from './emissionRepairs.mjs';

const repair = {
  appliesTo: 'flight/bitmapfont_formats/bitmap_font_json.hpp',
  from: 'is_object',
  id: 'overlapping-exact-respelling',
  kind: 'respell-reference-member-access',
  to: 'bitmap_font_json_is_object',
};
const file = {
  contents: [
    'inline bool is_object(flight::Any value) {',
    '  return is_object(value);',
    '}',
    '',
  ].join('\n'),
  path: 'flight/bitmapfont_formats/bitmap_font_json.hpp',
};

const applied = applyEmissionRepairs([repair], [file]);
assert.deepEqual(applied[0].files, [file.path]);
assert.equal(file.contents.match(/bitmap_font_json_is_object/gu)?.length, 2);
assert.equal(file.contents.includes('bitmap_font_json_bitmap_font_json_is_object'), false);

const reapplied = applyEmissionRepairs([repair], [file]);
assert.deepEqual(reapplied[0].files, []);

process.stdout.write('Emission repair fixed-point oracle passed.\n');
