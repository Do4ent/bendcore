/**
 * Edits of the assembly configuration table (docs/ui/ASSEMBLY_UI.md, "Исполнения сборки"). The
 * page sends whole tables to the document (setConfigurationTable: one undo step, checked by the
 * kernel); these pure functions build the next table. Row and column edits are those of the part
 * configuration table (../modeling/configurationTable).
 */

import type {
  AssemblyConfigurationColumn,
  AssemblyConfigurationTable,
  AssemblyConfigurationTableInfo,
  AssemblyState,
} from '../../wasm/bendcore_assembly_api.mjs';
import type { RowTable } from '../modeling/configurationTable';

export { addRow, editRow, nextDesignation, removeColumn, removeRow } from '../modeling/configurationTable';

export type AssemblyTableInput = RowTable<AssemblyConfigurationColumn> & AssemblyConfigurationTable & { active: string };

/** Table as sent to setConfigurationTable (no derived fields). */
export function assemblyTableInput(t: AssemblyConfigurationTableInfo): AssemblyTableInput {
  return {
    columns: t.columns.map((c) => ({ kind: c.kind, target: c.target })),
    rows: t.rows.map((r) => ({ designation: r.designation, name: r.name, cells: [...r.cells] })),
    active: t.active,
  };
}

/** Key of a column in a <select>: "suppression:3", "configuration:2", "mateValue:7". */
export const assemblyColumnKey = (c: AssemblyConfigurationColumn): string => `${c.kind}:${c.target}`;

export function assemblyColumnFromKey(key: string): AssemblyConfigurationColumn | null {
  const i = key.indexOf(':');
  if (i < 0) return null;
  const kind = key.slice(0, i);
  const target = Number(key.slice(i + 1));
  if (kind !== 'suppression' && kind !== 'configuration' && kind !== 'mateValue') return null;
  if (!Number.isInteger(target) || target < 0) return null;
  return { kind, target };
}

/** Mate types with a value a configuration can set (distance mm, angle degrees, gear ratio). */
const VALUE_MATES = new Set(['distance', 'angle', 'gear']);

/**
 * Columns the user may still add: inclusion of every top-level component that is not a pattern
 * instance, the configuration of components whose part or subassembly document has
 * configurations, the value of distance/angle/gear mates.
 */
export function availableAssemblyColumns(t: AssemblyTableInput, state: AssemblyState): AssemblyConfigurationColumn[] {
  const has = (c: AssemblyConfigurationColumn) => t.columns.some((x) => x.kind === c.kind && x.target === c.target);
  const out: AssemblyConfigurationColumn[] = [];
  for (const o of state.occurrences) {
    if (o.pattern !== null) continue;
    const c: AssemblyConfigurationColumn = { kind: 'suppression', target: o.id };
    if (!has(c)) out.push(c);
  }
  for (const o of state.occurrences) {
    if (o.pattern !== null) continue;
    const d = state.definitions.find((x) => x.id === o.definition);
    if (!d || d.configurations.length === 0) continue;
    const c: AssemblyConfigurationColumn = { kind: 'configuration', target: o.id };
    if (!has(c)) out.push(c);
  }
  for (const m of state.mates) {
    if (!VALUE_MATES.has(m.type)) continue;
    const c: AssemblyConfigurationColumn = { kind: 'mateValue', target: m.id };
    if (!has(c)) out.push(c);
  }
  return out;
}

/** Adds a column; its cells are empty (base value) in every row. */
export function addColumn(t: AssemblyTableInput, column: AssemblyConfigurationColumn): AssemblyTableInput {
  return {
    ...t,
    columns: [...t.columns, { kind: column.kind, target: column.target }],
    rows: t.rows.map((r) => ({ ...r, cells: [...r.cells, ''] })),
  };
}

/** Sets one cell. */
export function setCell(t: AssemblyTableInput, row: number, column: number, value: string): AssemblyTableInput {
  return {
    ...t,
    rows: t.rows.map((r, i) => (i === row ? { ...r, cells: r.cells.map((c, k) => (k === column ? value : c)) } : { ...r, cells: [...r.cells] })),
  };
}
