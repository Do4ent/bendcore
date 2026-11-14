/**
 * @jest-environment node
 */
// Text font and dissolved sub-assemblies of the drawing editor (docs/ui/DRAWING_UI.md).
import type { SpecificationTable } from '../../../wasm/bendcore_drawing_api.mjs';
import { componentKey, isTextFont, subassemblyRows, toggleDissolved } from '../documentSettings';
import { en, ru } from '../../app/i18n';

const item = (position: number, designation: string, name: string) => ({ format: 'А4', zone: '', position, designation, name, quantity: 1, note: '' });

// The nested unit of tests/drawing_v2/test_drawing_document_subassembly.cpp: a shaft and the
// sub-assembly "bushing + pin"; a second sub-assembly without a designation.
const table: SpecificationTable = {
  designation: 'ВТ.03.000',
  name: 'Узел',
  sections: [
    { section: 'documentation', title: 'Документация', items: [{ ...item(0, 'ВТ.03.000 СБ', 'Сборочный чертёж'), quantity: null }] },
    { section: 'assemblyUnits', title: 'Сборочные единицы', items: [item(1, 'ВТ.03.100', 'Втулка в сборе'), item(2, '', 'Кронштейн в сборе')] },
    { section: 'parts', title: 'Детали', items: [item(3, 'ВТ.03.001', 'Вал')] },
  ],
};

describe('text font', () => {
  test('only the two fonts of the kernel are accepted', () => {
    expect(isTextFont('gost2304')).toBe(true);
    expect(isTextFont('truetype')).toBe(true);
    expect(isTextFont('Arial')).toBe(false);
    expect(isTextFont('')).toBe(false);
  });
  test('both fonts have captions in both languages', () => {
    for (const f of ['gost2304', 'truetype'] as const) {
      expect(ru[`drawing.textFont.${f}`]).toBeTruthy();
      expect(en[`drawing.textFont.${f}`]).toBeTruthy();
    }
    expect(ru['drawing.textFont.gost2304']).toMatch(/ГОСТ 2\.304/);
  });
});

describe('dissolved sub-assemblies', () => {
  test('the key is the designation, else the name (componentKey of the kernel)', () => {
    expect(componentKey({ designation: 'ВТ.03.100', name: 'Втулка в сборе' })).toBe('ВТ.03.100');
    expect(componentKey({ designation: '', name: 'Кронштейн в сборе' })).toBe('Кронштейн в сборе');
  });

  test('rows: the assembly units of the specification, parts and documents are not listed', () => {
    expect(subassemblyRows(table, [])).toEqual([
      { key: 'ВТ.03.100', label: 'ВТ.03.100 Втулка в сборе', dissolved: false, position: 1 },
      { key: 'Кронштейн в сборе', label: 'Кронштейн в сборе', dissolved: false, position: 2 },
    ]);
  });

  test('a dissolved sub-assembly is not in the table any more but stays in the list to be restored', () => {
    const after: SpecificationTable = { ...table, sections: [table.sections[0], { ...table.sections[1], items: [table.sections[1].items[1]] }, table.sections[2]] };
    expect(subassemblyRows(after, ['ВТ.03.100'])).toEqual([
      { key: 'Кронштейн в сборе', label: 'Кронштейн в сборе', dissolved: false, position: 2 },
      { key: 'ВТ.03.100', label: 'ВТ.03.100', dissolved: true, position: 0 },
    ]);
    expect(subassemblyRows(null, ['ВТ.03.100'])).toEqual([{ key: 'ВТ.03.100', label: 'ВТ.03.100', dissolved: true, position: 0 }]);
  });

  test('switching gives the next settings: sorted, no duplicates', () => {
    expect(toggleDissolved([], 'ВТ.03.100', true)).toEqual(['ВТ.03.100']);
    expect(toggleDissolved(['ВТ.03.100'], 'ВТ.03.100', true)).toEqual(['ВТ.03.100']);
    expect(toggleDissolved(['ВТ.03.200', 'ВТ.03.100'], 'Б', true)).toEqual(['Б', 'ВТ.03.100', 'ВТ.03.200'].sort());
    expect(toggleDissolved(['ВТ.03.100', 'ВТ.03.200'], 'ВТ.03.100', false)).toEqual(['ВТ.03.200']);
    expect(toggleDissolved([], 'ВТ.03.100', false)).toEqual([]);
  });
});
