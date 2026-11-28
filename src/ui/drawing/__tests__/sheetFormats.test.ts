/**
 * @jest-environment node
 */
// Sheet formats and hatch materials of the drawing editor (docs/ui/DRAWING_UI.md, «Формат листа»,
// «Зоны и штриховка по материалу»).
import { HATCH_MATERIALS, SHEET_FORMATS, isHatchMaterial, sheetFormatOf, sheetFormatValue } from '../documentSettings';
import { en, ru } from '../../app/i18n';

// GOST 2.301-68, table 1 and table 2 (short × long side, mm); the ISO 5457 elongated sizes are the same.
const table2: [string, number, number][] = [
  ['A0x2', 1189, 1682], ['A0x3', 1189, 2523], ['A1x3', 841, 1783], ['A1x4', 841, 2378],
  ['A2x3', 594, 1261], ['A2x4', 594, 1682], ['A2x5', 594, 2102], ['A3x3', 420, 891], ['A3x4', 420, 1189],
  ['A3x5', 420, 1486], ['A3x6', 420, 1783], ['A3x7', 420, 2080], ['A4x3', 297, 630], ['A4x4', 297, 841],
  ['A4x5', 297, 1051], ['A4x6', 297, 1261], ['A4x7', 297, 1471], ['A4x8', 297, 1682], ['A4x9', 297, 1892],
];

test('the list holds the five basic sizes and every multiple of table 2', () => {
  expect(SHEET_FORMATS).toHaveLength(5 + table2.length);
  expect(SHEET_FORMATS.filter((f) => f.multiplier === 1).map((f) => [f.value, ...f.size])).toEqual([
    ['A0', 841, 1189], ['A1', 594, 841], ['A2', 420, 594], ['A3', 297, 420], ['A4', 210, 297],
  ]);
  for (const [value, a, b] of table2) {
    const f = sheetFormatOf(value)!;
    expect(f).toBeDefined();
    expect(f.size).toEqual([a, b]);
    expect(sheetFormatValue(f.format, f.multiplier)).toBe(value);
  }
  // Each basic size is followed by its multiples.
  expect(SHEET_FORMATS.map((f) => f.value).slice(0, 4)).toEqual(['A0', 'A0x2', 'A0x3', 'A1']);
  expect(sheetFormatOf('A4x2')).toBeUndefined();
});

test('hatch materials have labels in both languages', () => {
  expect(HATCH_MATERIALS).toEqual(['metal', 'nonmetal', 'wood', 'glass', 'concrete', 'liquid']);
  expect(isHatchMaterial('wood')).toBe(true);
  expect(isHatchMaterial('steel')).toBe(false);
  for (const m of HATCH_MATERIALS) {
    expect(ru[`drawing.hatchMaterial.${m}`]).toBeTruthy();
    expect(en[`drawing.hatchMaterial.${m}`]).toBeTruthy();
  }
});
