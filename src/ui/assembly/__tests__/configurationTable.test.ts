// Edits of the assembly configuration table (docs/ui/ASSEMBLY_UI.md, "Исполнения сборки"):
// candidate columns from the document state, added columns with base cells, cells, rows and
// columns through the shared functions of the part table, column keys.
import type { AssemblyConfigurationTableInfo, AssemblyState } from '../../../wasm/bendcore_assembly_api.mjs';
import {
  addColumn,
  addRow,
  assemblyColumnFromKey,
  assemblyColumnKey,
  assemblyTableInput,
  availableAssemblyColumns,
  editRow,
  removeColumn,
  removeRow,
  setCell,
} from '../configurationTable';

const occurrence = (id: number, definition: number, pattern: number | null = null) =>
  ({ id, name: `C${id}`, definition, pattern, kind: 'part', fixed: false, suppressed: false, designation: '', color: null }) as never;

const state = {
  definitions: [
    { id: 1, configurations: [] },
    { id: 2, configurations: [{ designation: '-01', fullDesignation: 'РЦ-01', name: 'x' }] },
  ],
  occurrences: [occurrence(1, 1), occurrence(2, 2), occurrence(3, 1, 7)],
  mates: [
    { id: 4, type: 'distance' },
    { id: 5, type: 'concentric' },
    { id: 6, type: 'angle' },
  ],
} as unknown as AssemblyState;

const info: AssemblyConfigurationTableInfo = {
  columns: [{ kind: 'suppression', target: 1, title: 'suppress:C1', label: 'C1', unit: '', options: [] }],
  rows: [
    { designation: 'РЦ.100.00.000', fullDesignation: 'РЦ.100.00.000', name: 'Узел', cells: ['0'] },
    { designation: '-01', fullDesignation: 'РЦ.100.00.000-01', name: 'Узел', cells: ['1'] },
  ],
  active: '-01',
  base: ['0'],
  designation: 'РЦ.100.00.000-01',
};

describe('assembly configuration table', () => {
  it('strips derived fields', () => {
    expect(assemblyTableInput(info)).toEqual({
      columns: [{ kind: 'suppression', target: 1 }],
      rows: [
        { designation: 'РЦ.100.00.000', name: 'Узел', cells: ['0'] },
        { designation: '-01', name: 'Узел', cells: ['1'] },
      ],
      active: '-01',
    });
  });

  it('offers inclusion of components, configurations of components with them, values of value mates', () => {
    const t = assemblyTableInput(info);
    expect(availableAssemblyColumns(t, state)).toEqual([
      { kind: 'suppression', target: 2 },
      { kind: 'configuration', target: 2 },
      { kind: 'mateValue', target: 4 },
      { kind: 'mateValue', target: 6 },
    ]);
  });

  it('adds columns with base cells, sets cells, adds and removes rows and columns', () => {
    let t = addColumn(assemblyTableInput(info), { kind: 'mateValue', target: 4 });
    expect(t.columns).toEqual([
      { kind: 'suppression', target: 1 },
      { kind: 'mateValue', target: 4 },
    ]);
    expect(t.rows.map((r) => r.cells)).toEqual([
      ['0', ''],
      ['1', ''],
    ]);
    t = setCell(t, 1, 1, '15');
    expect(t.rows[1].cells).toEqual(['1', '15']);
    expect(t.rows[0].cells).toEqual(['0', '']);
    t = addRow(t);
    expect(t.rows[2]).toEqual({ designation: '-02', name: 'Узел', cells: ['1', '15'] });
    t = editRow(t, 1, 'designation', '-05');
    expect(t.active).toBe('-05');
    t = removeColumn(t, 0);
    expect(t.columns).toEqual([{ kind: 'mateValue', target: 4 }]);
    expect(t.rows.map((r) => r.cells)).toEqual([[''], ['15'], ['15']]);
    t = removeRow(t, 1);
    expect(t.active).toBe('');
    expect(t.rows.map((r) => r.designation)).toEqual(['РЦ.100.00.000', '-02']);
  });

  it('round-trips column keys', () => {
    for (const c of [
      { kind: 'suppression', target: 3 },
      { kind: 'configuration', target: 12 },
      { kind: 'mateValue', target: 0 },
    ] as const) {
      expect(assemblyColumnFromKey(assemblyColumnKey(c))).toEqual(c);
    }
    expect(assemblyColumnFromKey('material:')).toBeNull();
    expect(assemblyColumnFromKey('suppression:x')).toBeNull();
    expect(assemblyColumnFromKey('nothing')).toBeNull();
  });
});
