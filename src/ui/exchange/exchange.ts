/**
 * Data exchange in the interface (docs/ui/DATA_EXCHANGE.md): export formats and their options,
 * file names and media types, import format from a file name, and validation of the export
 * form. The files themselves are written by the kernel modules (exportFile of a part, an
 * assembly or an imported shape); nothing here produces geometry.
 */

import type { ExportFormat, ExportOptions, MassProperties } from '../../wasm/bendcore_kernel_api.mjs';

export type { ExportFormat, ExportOptions, MassProperties };

export type ImportFormat = 'step' | 'iges';

/** Every format is offered for parts, assemblies and imported models. */
export const EXPORT_FORMATS: ExportFormat[] = ['step', 'iges', 'stl', '3mf'];

export const EXTENSIONS: Record<ExportFormat, string> = { step: 'step', iges: 'igs', stl: 'stl', '3mf': '3mf' };

export const MEDIA_TYPES: Record<ExportFormat, string> = {
  step: 'model/step',
  iges: 'model/iges',
  stl: 'model/stl',
  '3mf': 'model/3mf',
};

/** Extensions accepted by "Import STEP/IGES" inputs. */
export const IMPORT_ACCEPT = '.step,.stp,.igs,.iges,.STEP,.STP,.IGS,.IGES';

export function importFormatOf(fileName: string): ImportFormat | null {
  if (/\.(step|stp)$/i.test(fileName)) return 'step';
  if (/\.(iges|igs)$/i.test(fileName)) return 'iges';
  return null;
}

/** Name of a document without the bendcore extensions and the path. */
export function baseNameOf(fileName: string): string {
  const leaf = fileName.split(/[\\/]/).pop() ?? fileName;
  const stem = leaf.replace(/(\.bcpart|\.bcasm|\.bcdrw)?\.json$/i, '').replace(/\.(step|stp|iges|igs|stl|3mf)$/i, '');
  return stem.trim() || 'model';
}

export function exportFileName(baseName: string, format: ExportFormat): string {
  return `${baseNameOf(baseName)}.${EXTENSIONS[format]}`;
}

/** Values of the export form as typed (text fields stay strings until validated). */
export interface ExportForm {
  format: ExportFormat;
  schema: 'AP214' | 'AP242';
  unit: 'mm' | 'm' | 'inch';
  igesMode: 'brep' | 'faces';
  stlFormat: 'binary' | 'ascii';
  linearDeflection: string;
  angularDeflectionDeg: string;
}

export const DEFAULT_EXPORT_FORM: ExportForm = {
  format: 'step',
  schema: 'AP214',
  unit: 'mm',
  igesMode: 'brep',
  stlFormat: 'binary',
  linearDeflection: '0.05',
  angularDeflectionDeg: '20',
};

/** Ranges the kernel accepts (src/wasm/exchange_common.hpp, meshOptions). */
export const DEFLECTION_RANGE = { min: 0.0001, max: 100 };
export const ANGLE_RANGE = { min: 0.5, max: 90 };

export type ExportFormError = { field: 'linearDeflection' | 'angularDeflectionDeg'; min: number; max: number };

export function isMeshFormat(format: ExportFormat): boolean {
  return format === 'stl' || format === '3mf';
}

/** Options for exportFile, or the field that is out of range. */
export function exportOptions(form: ExportForm, name: string): { options: ExportOptions } | { error: ExportFormError } {
  const options: ExportOptions = { name: baseNameOf(name) };
  if (form.format === 'step') {
    options.schema = form.schema;
    options.unit = form.unit;
  } else if (form.format === 'iges') {
    options.igesMode = form.igesMode;
    options.unit = form.unit;
  } else {
    const deflection = Number(form.linearDeflection);
    if (!(deflection >= DEFLECTION_RANGE.min && deflection <= DEFLECTION_RANGE.max)) {
      return { error: { field: 'linearDeflection', ...DEFLECTION_RANGE } };
    }
    const angle = Number(form.angularDeflectionDeg);
    if (!(angle >= ANGLE_RANGE.min && angle <= ANGLE_RANGE.max)) {
      return { error: { field: 'angularDeflectionDeg', ...ANGLE_RANGE } };
    }
    options.linearDeflection = deflection;
    options.angularDeflectionDeg = angle;
    if (form.format === 'stl') options.stlFormat = form.stlFormat;
  }
  return { options };
}

/** Density typed by the user, kg/m³, or null when it is not a usable number. */
export function parseDensity(text: string): number | null {
  const d = Number(text.replace(',', '.'));
  return Number.isFinite(d) && d > 0 && d <= 30000 ? d : null;
}

/** Diagonal of the inertia tensor at the centre of mass (Ixx, Iyy, Izz), kg·mm². */
export function inertiaDiagonal(p: MassProperties): [number, number, number] {
  return [p.inertiaAtCenter[0][0], p.inertiaAtCenter[1][1], p.inertiaAtCenter[2][2]];
}

/** Products of inertia (Ixy, Ixz, Iyz) in the tensor convention of the kernel (−∫xy dm). */
export function inertiaProducts(p: MassProperties): [number, number, number] {
  return [p.inertiaAtCenter[0][1], p.inertiaAtCenter[0][2], p.inertiaAtCenter[1][2]];
}
