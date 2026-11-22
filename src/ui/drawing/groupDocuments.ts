/**
 * Group documents of GOST 2.113 in the drawing editor (docs/ui/DRAWING_UI.md, "Групповые
 * документы"): the group drawing of a part (letters at the dimensions that differ between the
 * configurations, table of configurations) and the group specification of an assembly (quantities
 * per configuration). The kernel computes both; these helpers only turn its description into the
 * rows of the interface and the settings changes it accepts.
 */

import type { GroupColumn, GroupSpecLayout, SpecificationTable } from '../../wasm/bendcore_drawing_api.mjs';

export const GROUP_SPEC_LAYOUTS: readonly GroupSpecLayout[] = ['columns', 'variableData'];
export function isGroupSpecLayout(v: string): v is GroupSpecLayout {
  return (GROUP_SPEC_LAYOUTS as readonly string[]).includes(v);
}

/**
 * setSettings change for a header edited in the table of configurations: the text, or null (back
 * to the default) when it is empty or equal to the default header. null — nothing changes.
 */
export function headerChange(column: GroupColumn, text: string): { group: { headers: Record<string, string | null> } } | null {
  const value = text.trim();
  const next = value === '' || value === column.defaultHeader ? null : value;
  const current = column.header === column.defaultHeader ? null : column.header;
  if (next === current) return null;
  return { group: { headers: { [column.key]: next } } };
}

/** Text of the lettered dimensions: «A — S1/L, B — S1/D». */
export function lettersSummary(dimensions: readonly { sheet: string; id: string; letter: string }[]): string {
  return dimensions.map((d) => `${d.letter} — ${d.sheet}/${d.id}`).join(', ');
}

/**
 * Column headers of the group specification after editing the header of configuration `index`:
 * the full list (empty strings — the default), trailing defaults dropped.
 */
export function columnHeadersAfter(current: readonly string[], count: number, index: number, text: string, defaults: readonly string[]): string[] {
  const out: string[] = [];
  for (let i = 0; i < count; ++i) out.push(current[i] ?? '');
  const value = text.trim();
  out[index] = value === defaults[index] ? '' : value;
  while (out.length > 0 && out[out.length - 1] === '') out.pop();
  return out;
}

export interface GroupSpecRow {
  kind: 'section' | 'item';
  key: string;
  title?: string;
  position?: number;
  designation?: string;
  name?: string;
  /** quantity per configuration, '–' — not in it; documents have none */
  quantities?: string[];
  constant?: boolean;
  note?: string;
}

/** Rows of the group specification table of the interface: section headings and items. */
export function groupSpecRows(table: SpecificationTable | null): GroupSpecRow[] {
  const g = table?.group;
  if (!g) return [];
  const rows: GroupSpecRow[] = [];
  for (const s of g.sections) {
    rows.push({ kind: 'section', key: `h-${s.section}`, title: s.title });
    s.items.forEach((it, i) => {
      const documentation = s.section === 'documentation';
      rows.push({
        kind: 'item',
        key: `${s.section}-${i}`,
        position: it.position,
        designation: it.designation,
        name: it.name,
        quantities: documentation ? g.configurations.map(() => '') : it.quantities.map((q) => (q > 0 ? String(q) : '–')),
        constant: it.constant,
        note: it.note,
      });
    });
  }
  return rows;
}
