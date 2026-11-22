import { columnHeadersAfter, groupSpecRows, headerChange, isGroupSpecLayout, lettersSummary } from '../groupDocuments';
import type { SpecificationTable } from '../../../wasm/bendcore_drawing_api.mjs';

describe('group documents (GOST 2.113)', () => {
  const column = { key: 'suppress:MountHole', header: 'MountHole', defaultHeader: 'MountHole' };

  test('header change: new text, back to default, no change', () => {
    expect(headerChange(column, 'Отверстие')).toEqual({ group: { headers: { 'suppress:MountHole': 'Отверстие' } } });
    expect(headerChange(column, 'MountHole')).toBeNull();
    expect(headerChange(column, '  ')).toBeNull();
    const edited = { ...column, header: 'Отверстие' };
    expect(headerChange(edited, '')).toEqual({ group: { headers: { 'suppress:MountHole': null } } });
    expect(headerChange(edited, 'MountHole')).toEqual({ group: { headers: { 'suppress:MountHole': null } } });
    expect(headerChange(edited, 'Отверстие')).toBeNull();
  });

  test('letters summary', () => {
    expect(lettersSummary([{ sheet: 'S1', id: 'L', letter: 'A' }, { sheet: 'S1', id: 'D', letter: 'B' }])).toBe('A — S1/L, B — S1/D');
    expect(lettersSummary([])).toBe('');
  });

  test('column headers of the group specification', () => {
    expect(columnHeadersAfter([], 2, 1, 'Усил.', ['–', '01'])).toEqual(['', 'Усил.']);
    expect(columnHeadersAfter(['', 'Усил.'], 2, 1, '01', ['–', '01'])).toEqual([]);
    expect(columnHeadersAfter([], 3, 0, 'Баз.', ['–', '01', '02'])).toEqual(['Баз.']);
  });

  test('layouts', () => {
    expect(isGroupSpecLayout('columns')).toBe(true);
    expect(isGroupSpecLayout('variableData')).toBe(true);
    expect(isGroupSpecLayout('rows')).toBe(false);
  });

  test('rows of the group specification: quantities per configuration, dash where absent', () => {
    const table: SpecificationTable = {
      designation: 'РЦ.200.00.000',
      name: 'Рамка',
      sections: [],
      group: {
        designation: 'РЦ.200.00.000',
        name: 'Рамка',
        layout: 'columns',
        configurations: ['РЦ.200.00.000', 'РЦ.200.00.000-01'],
        columnHeaders: ['–', '01'],
        sections: [
          { section: 'documentation', title: 'Документация', items: [{ format: 'А3', position: 0, designation: 'РЦ.200.00.000 СБ', name: 'Сборочный чертёж', quantities: [0, 0], constant: true, note: '' }] },
          { section: 'parts', title: 'Детали', items: [{ format: '', position: 2, designation: 'РЦ.080.00.010-01', name: 'Планка широкая', quantities: [0, 1], constant: false, note: '' }] },
        ],
      },
    };
    const rows = groupSpecRows(table);
    expect(rows.map((r) => r.kind)).toEqual(['section', 'item', 'section', 'item']);
    expect(rows[1].quantities).toEqual(['', '']);
    expect(rows[3].quantities).toEqual(['–', '1']);
    expect(rows[3].constant).toBe(false);
    expect(groupSpecRows(null)).toEqual([]);
    expect(groupSpecRows({ ...table, group: null })).toEqual([]);
  });
});
