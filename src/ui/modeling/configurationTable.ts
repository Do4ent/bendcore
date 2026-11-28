/**
 * Edits of the configuration table on the page (docs/ui/PART_MODELING_UI.md, "Таблица параметров").
 * The page sends whole tables to the document (setConfigurations: one undo entry, checked by the
 * kernel); these pure functions build the next table from the current one. Row and column edits
 * are generic over the column type: the assembly configuration table (docs/ui/ASSEMBLY_UI.md,
 * "Исполнения сборки") uses them with its own columns.
 */

import type { ConfigurationColumn, ConfigurationRow, ConfigurationTable } from '../../wasm/bendcore_modeling_api.mjs';

export interface TableInput {
  columns: ConfigurationColumn[];
  rows: ConfigurationRow[];
  active: string;
}

/** Any table of configurations: rows of cells, one cell per column. */
export interface RowTable<C> {
  columns: C[];
  rows: ConfigurationRow[];
  active: string;
}

const plainColumn = <C extends { kind: string; target: string | number }>(c: C): C => ({ kind: c.kind, target: c.target }) as C;
const plainRow = (r: ConfigurationRow): ConfigurationRow => ({ designation: r.designation, name: r.name, cells: [...r.cells] });

/** Table as sent to setConfigurations (no derived fields). */
export function tableInput(t: ConfigurationTable): TableInput {
  return { columns: t.columns.map(plainColumn), rows: t.rows.map(plainRow), active: t.active };
}

/**
 * Next free designation suffix: "-01", "-02", … after the existing rows (ГОСТ 2.113: the first row
 * is the base designation, the others are suffixes).
 */
export function nextDesignation(rows: readonly ConfigurationRow[]): string {
  if (rows.length === 0) return '-00';
  let n = 1;
  const taken = new Set(rows.map((r) => r.designation));
  for (;;) {
    const d = `-${String(n).padStart(2, '0')}`;
    if (!taken.has(d)) return d;
    ++n;
  }
}

/** New row: a copy of the row `from` (default the last one) with the next designation. */
export function addRow<T extends RowTable<unknown>>(t: T, from?: number): T {
  const src = from !== undefined ? t.rows[from] : t.rows[t.rows.length - 1];
  const row: ConfigurationRow = {
    designation: nextDesignation(t.rows),
    name: src ? src.name : '',
    cells: src ? [...src.cells] : t.columns.map(() => ''),
  };
  return { ...t, rows: [...t.rows.map(plainRow), row] };
}

/** Removes a row; the active configuration falls back to the base values when it is removed. */
export function removeRow<T extends RowTable<unknown>>(t: T, index: number): T {
  const removed = t.rows[index];
  return {
    ...t,
    rows: t.rows.filter((_, i) => i !== index).map(plainRow),
    active: removed && removed.designation === t.active ? '' : t.active,
  };
}

/** Removes a column with its cells. */
export function removeColumn<T extends RowTable<{ kind: string; target: string | number }>>(t: T, index: number): T {
  return {
    ...t,
    columns: t.columns.filter((_, i) => i !== index).map((c) => plainColumn(c)),
    rows: t.rows.map((r) => ({ ...plainRow(r), cells: r.cells.filter((_, i) => i !== index) })),
  };
}

/** Renames a row (designation or name); the active designation follows a renamed row. */
export function editRow<T extends RowTable<unknown>>(t: T, index: number, field: 'designation' | 'name', value: string): T {
  const rows = t.rows.map(plainRow);
  const old = rows[index]?.designation;
  if (!rows[index]) return t;
  rows[index] = { ...rows[index], [field]: value };
  return { ...t, rows, active: field === 'designation' && old === t.active ? value : t.active };
}

/** Columns the user may still add: variables and features without a column, the material once. */
export function availableColumns(
  t: TableInput,
  variables: readonly string[],
  features: readonly { id: string; type: string }[],
): ConfigurationColumn[] {
  const has = (c: ConfigurationColumn) => t.columns.some((x) => x.kind === c.kind && x.target === c.target);
  const out: ConfigurationColumn[] = [];
  for (const v of variables) if (!has({ kind: 'variable', target: v })) out.push({ kind: 'variable', target: v });
  for (const f of features) {
    if (f.type === 'Sketch') continue;
    if (!has({ kind: 'suppression', target: f.id })) out.push({ kind: 'suppression', target: f.id });
  }
  if (!has({ kind: 'material', target: '' })) out.push({ kind: 'material', target: '' });
  return out;
}

/** Key of a column in a <select>: "variable:W", "suppression:Hole", "material:". */
export const columnKey = (c: ConfigurationColumn): string => `${c.kind}:${c.target}`;

export function columnFromKey(key: string): ConfigurationColumn | null {
  const i = key.indexOf(':');
  if (i < 0) return null;
  const kind = key.slice(0, i);
  if (kind !== 'variable' && kind !== 'suppression' && kind !== 'material') return null;
  return { kind, target: key.slice(i + 1) };
}
