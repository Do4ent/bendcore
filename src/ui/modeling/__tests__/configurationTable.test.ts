// Edits of the configuration table on the part page (docs/ui/PART_MODELING_UI.md, "Таблица
// параметров"): rows and columns added and removed, designations, available columns, labels.
import type { ConfigurationTable } from '../../../wasm/bendcore_modeling_api.mjs';
import {
  addRow,
  availableColumns,
  columnFromKey,
  columnKey,
  editRow,
  nextDesignation,
  removeColumn,
  removeRow,
  tableInput,
} from '../configurationTable';
import { translate } from '../../app/i18n';
import { partEn, partRu } from '../../app/messages/part';

const table: ConfigurationTable = {
  columns: [
    { kind: 'variable', target: 'W', title: 'W' },
    { kind: 'suppression', target: 'Hole', title: 'suppress:Hole' },
  ],
  rows: [
    { designation: 'РЦ.080.00.010', fullDesignation: 'РЦ.080.00.010', name: 'Брусок', cells: ['60', '0'] },
    { designation: '-01', fullDesignation: 'РЦ.080.00.010-01', name: 'Брусок', cells: ['80', ''] },
  ],
  active: '-01',
  material: '',
};

test('tableInput drops derived fields', () => {
  const t = tableInput(table);
  expect(t.columns).toEqual([
    { kind: 'variable', target: 'W' },
    { kind: 'suppression', target: 'Hole' },
  ]);
  expect(t.rows[1]).toEqual({ designation: '-01', name: 'Брусок', cells: ['80', ''] });
  expect(t.active).toBe('-01');
});

test('a new row copies the last one with the next free suffix', () => {
  expect(nextDesignation([])).toBe('-00');
  const t = addRow(tableInput(table));
  expect(t.rows).toHaveLength(3);
  expect(t.rows[2]).toEqual({ designation: '-02', name: 'Брусок', cells: ['80', ''] });
  expect(addRow(t, 0).rows[3]).toEqual({ designation: '-03', name: 'Брусок', cells: ['60', '0'] });
  const empty = addRow({ columns: [{ kind: 'variable', target: 'W' }], rows: [], active: '' });
  expect(empty.rows[0]).toEqual({ designation: '-00', name: '', cells: [''] });
});

test('removing the active row falls back to the base values; columns go with their cells', () => {
  const t = removeRow(tableInput(table), 1);
  expect(t.rows.map((r) => r.designation)).toEqual(['РЦ.080.00.010']);
  expect(t.active).toBe('');
  expect(removeRow(tableInput(table), 0).active).toBe('-01');
  const c = removeColumn(tableInput(table), 0);
  expect(c.columns).toEqual([{ kind: 'suppression', target: 'Hole' }]);
  expect(c.rows.map((r) => r.cells)).toEqual([['0'], ['']]);
});

test('renaming the active row keeps it active', () => {
  const t = editRow(tableInput(table), 1, 'designation', '-05');
  expect(t.rows[1].designation).toBe('-05');
  expect(t.active).toBe('-05');
  expect(editRow(tableInput(table), 0, 'name', 'Планка').rows[0].name).toBe('Планка');
});

test('available columns: variables and features without a column, sketches excluded, material once', () => {
  const cols = availableColumns(tableInput(table), ['W', 'D'], [
    { id: 'Outline', type: 'Sketch' },
    { id: 'Body', type: 'Extrude' },
    { id: 'Hole', type: 'Extrude' },
  ]);
  expect(cols.map(columnKey)).toEqual(['variable:D', 'suppression:Body', 'material:']);
  expect(columnFromKey('suppression:Body')).toEqual({ kind: 'suppression', target: 'Body' });
  expect(columnFromKey('colour:x')).toBeNull();
});

test('panel strings exist in both languages with the same parameters', () => {
  const keys = Object.keys(partRu).filter((k) => k.startsWith('part.config.'));
  expect(keys.length).toBeGreaterThan(15);
  const params = (s: string) => (s.match(/\{\w+\}/g) ?? []).sort();
  for (const k of keys) {
    expect(params(partEn[k as keyof typeof partRu])).toEqual(params(partRu[k as keyof typeof partRu]));
  }
  expect(translate('ru', 'part.config.suppress', { feature: 'Hole' })).toBe('Подавить: Hole');
  expect(translate('en', 'part.config.suppress', { feature: 'Hole' })).toBe('Suppress: Hole');
});
