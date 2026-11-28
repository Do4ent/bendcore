/**
 * Document settings of the drawing editor (docs/ui/DRAWING_UI.md, "Шрифт текста и разборные
 * подсборки"): the text font of the drawing and specification sheets and the sub-assemblies listed
 * by their parts. Both are applied by the kernel call setSettings; these helpers only turn the
 * kernel's description and specification into the rows of the interface and the next settings.
 */

import type { GostFormat, HatchMaterial, ProjectionMethod, SheetStyle, SpecificationTable, TextFont, ViewInfo } from '../../wasm/bendcore_drawing_api.mjs';

export const TEXT_FONTS: readonly TextFont[] = ['gost2304', 'truetype'];

export function isTextFont(v: string): v is TextFont {
  return (TEXT_FONTS as readonly string[]).includes(v);
}

/** Sheet styles: ESKD (GOST 2.104) and ISO (ISO 5457 frame, ISO 7200 title block). */
export const SHEET_STYLES: readonly SheetStyle[] = ['eskd', 'iso7200'];
export function isSheetStyle(v: string): v is SheetStyle {
  return (SHEET_STYLES as readonly string[]).includes(v);
}

/**
 * Sheet formats of the format list (docs/ui/DRAWING_UI.md, "Формат листа"): the basic sizes of
 * GOST 2.301 table 1 and the additional multiples of table 2 (the same sizes are the ISO 5457
 * elongated sizes, tables 2 and 3), short × long side in mm. The kernel checks the combination.
 */
export interface SheetFormatOption {
  /** option value: "A4" or "A4x3" */
  value: string;
  format: GostFormat;
  multiplier: number;
  /** short and long side, mm */
  size: readonly [number, number];
}
const BASIC: readonly (readonly [GostFormat, number, number])[] = [
  ['A0', 841, 1189], ['A1', 594, 841], ['A2', 420, 594], ['A3', 297, 420], ['A4', 210, 297],
];
const MULTIPLES: readonly (readonly [GostFormat, number, number, number])[] = [
  ['A0', 2, 1189, 1682], ['A0', 3, 1189, 2523],
  ['A1', 3, 841, 1783], ['A1', 4, 841, 2378],
  ['A2', 3, 594, 1261], ['A2', 4, 594, 1682], ['A2', 5, 594, 2102],
  ['A3', 3, 420, 891], ['A3', 4, 420, 1189], ['A3', 5, 420, 1486], ['A3', 6, 420, 1783], ['A3', 7, 420, 2080],
  ['A4', 3, 297, 630], ['A4', 4, 297, 841], ['A4', 5, 297, 1051], ['A4', 6, 297, 1261], ['A4', 7, 297, 1471],
  ['A4', 8, 297, 1682], ['A4', 9, 297, 1892],
];
/** Every format: each basic size followed by its multiples, A0 first. */
export const SHEET_FORMATS: readonly SheetFormatOption[] = BASIC.flatMap(([format, a, b]) => [
  { value: format, format, multiplier: 1, size: [a, b] as const },
  ...MULTIPLES.filter((m) => m[0] === format).map(([f, n, s, l]) => ({ value: `${f}x${n}`, format: f, multiplier: n, size: [s, l] as const })),
]);
export const sheetFormatValue = (format: GostFormat, multiplier: number): string => (multiplier === 1 ? format : `${format}x${multiplier}`);
export function sheetFormatOf(value: string): SheetFormatOption | undefined {
  return SHEET_FORMATS.find((f) => f.value === value);
}

/** Material symbols of sections, GOST 2.306 table 1 (the kernel's views::HatchMaterial). */
export const HATCH_MATERIALS: readonly HatchMaterial[] = ['metal', 'nonmetal', 'wood', 'glass', 'concrete', 'liquid'];
export function isHatchMaterial(v: string): v is HatchMaterial {
  return (HATCH_MATERIALS as readonly string[]).includes(v);
}

/** Projection methods of the document: first angle (ESKD, ISO E) and third angle (ISO A). */
export const PROJECTION_METHODS: readonly ProjectionMethod[] = ['firstAngle', 'thirdAngle'];
export function isProjectionMethod(v: string): v is ProjectionMethod {
  return (PROJECTION_METHODS as readonly string[]).includes(v);
}

/**
 * Parent of a new auxiliary view (GOST 2.305, 5.4): the selected view when it is an orthogonal
 * view the arrow can be put on (base, projected, section, auxiliary), else the base view; null —
 * the sheet has no base view.
 */
export function auxiliaryParent(views: readonly Pick<ViewInfo, 'id' | 'kind'>[], selected: string | null): string | null {
  const orthogonal = ['base', 'projected', 'section', 'auxiliary'];
  const sel = views.find((v) => v.id === selected);
  if (sel && orthogonal.includes(sel.kind)) return sel.id;
  return views.find((v) => v.kind === 'base')?.id ?? null;
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
