/**
 * Pure logic of the DXF import panel of the sketch editor (docs/ui/SKETCH_EDITOR.md, "Импорт
 * DXF"): the options passed to ModelingKernel.dxfSketch and the texts of its answer. The file is
 * read, scaled, filtered and turned into sketch data by the kernel; nothing here computes
 * geometry.
 */

import type { DxfSketchImport, DxfSketchOptions } from '../../../wasm/bendcore_modeling_api.mjs';
import { caption } from '../../app/i18n';
import type { Translate } from '../../app/i18n';
import { localizeKernelMessage } from '../../app/kernelMessages';
import { formatValue } from './dimensionLayout';

/** $INSUNITS codes offered when the file has none or the user overrides it. */
export const DXF_UNITS = [4, 5, 14, 6, 13, 1, 2, 9] as const;

export interface DxfImportForm {
  /** chosen layer names; null: the kernel default (layers neither frozen nor off) */
  layers: string[] | null;
  /** $INSUNITS code chosen by the user; null: the file's */
  units: number | null;
  tolerance: string;
  fixed: boolean;
}

export const initialDxfForm = (): DxfImportForm => ({ layers: null, units: null, tolerance: '0.001', fixed: false });

/** Tolerance of the form as a number, or null when it is not a positive number. */
export function dxfTolerance(form: DxfImportForm): number | null {
  const v = Number(form.tolerance.replace(',', '.'));
  return Number.isFinite(v) && v > 0 ? v : null;
}

/**
 * Options of dxfSketch for the form. A file without $INSUNITS is read in millimetres until the
 * user chooses other units (defaultUnits; the kernel refuses such a file without units).
 */
export function dxfOptions(form: DxfImportForm): DxfSketchOptions {
  const out: DxfSketchOptions = { fixed: form.fixed, defaultUnits: 4 };
  const tol = dxfTolerance(form);
  if (tol !== null) out.tolerance = tol;
  if (form.layers !== null) out.layers = [...form.layers];
  if (form.units !== null) out.units = form.units;
  return out;
}

/** Layer names selected in the answer (the kernel reports its default choice). */
export const selectedLayers = (r: DxfSketchImport): string[] => r.layers.filter((l) => l.selected).map((l) => l.name);

/** Form with one layer switched on or off (starting from the kernel's selection). */
export function toggleLayer(form: DxfImportForm, r: DxfSketchImport, name: string, on: boolean): DxfImportForm {
  const current = form.layers ?? selectedLayers(r);
  const layers = on ? [...current.filter((n) => n !== name), name] : current.filter((n) => n !== name);
  return { ...form, layers };
}

export const unitCaption = (t: Translate, code: number): string => caption(t, 'sketch.dxf.unit.', String(code));

/** Summary lines of an answer: counts, size, open ends, what was left out. */
export function dxfSummary(r: DxfSketchImport, t: Translate): string[] {
  const lines: string[] = [];
  const total = r.entities.length;
  if (total === 0) lines.push(t('sketch.dxf.noCurves'));
  lines.push(
    t('sketch.dxf.summary', {
      lines: r.imported.lines,
      arcs: r.imported.arcs,
      circles: r.imported.circles,
      polylines: r.imported.polylines,
      segments: r.imported.polylineSegments,
    }),
  );
  lines.push(t('sketch.dxf.points', { points: r.points, merged: r.merged }));
  if (r.bounds) {
    lines.push(t('sketch.dxf.size', { w: formatValue(r.bounds.max[0] - r.bounds.min[0]), h: formatValue(r.bounds.max[1] - r.bounds.min[1]) }));
  }
  if (total > 0) lines.push(r.openEnds.length > 0 ? t('sketch.dxf.openEnds', { n: r.openEnds.length }) : t('sketch.dxf.closed'));
  if (r.imported.degenerate > 0) lines.push(t('sketch.dxf.degenerate', { n: r.imported.degenerate }));
  const skipped = Object.entries(r.skipped);
  if (skipped.length > 0) lines.push(t('sketch.dxf.skipped', { list: skipped.map(([k, n]) => `${k} × ${n}`).join(', ') }));
  if (r.ignored.texts + r.ignored.paperSpace + r.ignored.otherLayers > 0) {
    lines.push(t('sketch.dxf.ignored', { texts: r.ignored.texts, paper: r.ignored.paperSpace, other: r.ignored.otherLayers }));
  }
  if (r.warnings.length > 0) lines.push(t('sketch.dxf.warnings', { list: r.warnings.map((w) => localizeKernelMessage(t, 'message', w).text ?? w).join('; ') }));
  return lines;
}
