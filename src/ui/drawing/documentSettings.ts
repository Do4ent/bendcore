/**
 * Document settings of the drawing editor (docs/ui/DRAWING_UI.md, "Шрифт текста и разборные
 * подсборки"): the text font of the drawing and specification sheets and the sub-assemblies listed
 * by their parts. Both are applied by the kernel call setSettings; these helpers only turn the
 * kernel's description and specification into the rows of the interface and the next settings.
 */

import type { SpecificationTable, TextFont } from '../../wasm/bendcore_drawing_api.mjs';

export const TEXT_FONTS: readonly TextFont[] = ['gost2304', 'truetype'];

export function isTextFont(v: string): v is TextFont {
  return (TEXT_FONTS as readonly string[]).includes(v);
}

export interface SubassemblyRow {
  /** component key of the kernel (SpecificationSettings::dissolved): designation, else name */
  key: string;
  /** text shown: designation and name */
  label: string;
  /** listed by its parts */
  dissolved: boolean;
  /** position in the specification (0 for a dissolved one: it has none) */
  position: number;
}

/** Key of a component the kernel uses (componentKey of specification_document.cpp). */
export const componentKey = (item: { designation: string; name: string }): string => item.designation || item.name;

/**
 * Sub-assemblies the user can dissolve or restore: the entries of "Сборочные единицы" of the
 * specification (not dissolved) and the keys already dissolved (no longer in the table: listed
 * by their parts), in the order of the table, then of the keys.
 */
export function subassemblyRows(table: SpecificationTable | null, dissolved: readonly string[]): SubassemblyRow[] {
  const rows: SubassemblyRow[] = [];
  const seen = new Set<string>();
  for (const s of table?.sections ?? []) {
    if (s.section !== 'assemblyUnits') continue;
    for (const it of s.items) {
      const key = componentKey(it);
      if (!key || seen.has(key)) continue;
      seen.add(key);
      const label = it.designation && it.name ? `${it.designation} ${it.name}` : key;
      rows.push({ key, label, dissolved: dissolved.includes(key), position: it.position });
    }
  }
  for (const key of dissolved) {
    if (seen.has(key)) continue;
    seen.add(key);
    rows.push({ key, label: key, dissolved: true, position: 0 });
  }
  return rows;
}

/** The dissolved keys after switching `key`: sorted, without duplicates. */
export function toggleDissolved(dissolved: readonly string[], key: string, on: boolean): string[] {
  const next = new Set(dissolved);
  if (on) next.add(key);
  else next.delete(key);
  return [...next].sort();
}
