/**
 * Document templates of the application shell (docs/ui/APP_SHELL.md, "Шаблоны"): starting
 * documents of a part, an assembly and a drawing with their settings.
 *
 * This file is the logic without the browser and without the kernel: the data types, the built-in
 * templates, reading and writing of the template file, and the settings taken from the files of
 * open documents ("Save as template"). The kernel calls that build a document from a template are
 * in templateService.ts; IndexedDB storage is in storage.ts.
 *
 * A template keeps settings, not a copy of a document: the part holds the material, the variables
 * and the datum planes, the drawing holds the sheet, the style, the projection method, the font
 * and the constant fields of the title block, the assembly holds the default material and
 * density and the parts to insert. Geometry never goes into a template.
 */

import type { DocKind } from './editorHost';
import type { MessageKey } from './i18n';

export const TEMPLATE_FORMAT = 'bendcore-template';
export const TEMPLATE_VERSION = 1;
export const TEMPLATE_EXTENSION = '.bctemplate.json';

/** The only unit of part documents: the kernel works in millimetres. */
export type TemplateUnits = 'mm';

export interface TemplateMaterial {
  /** Id of a kernel library material, or 'custom' for a density typed by hand. */
  id: string;
  /** kg/m^3 */
  density: number;
}

export interface TemplateVariable {
  name: string;
  expression: string;
}

export type BasePlane = 'XY' | 'XZ' | 'YZ';

export interface TemplatePlane {
  label: string;
  base: BasePlane;
  /** Offset from the base plane (expression: a number or a variable of the template), mm. */
  offset: string;
}

export interface PartTemplateSettings {
  units: TemplateUnits;
  material: TemplateMaterial | null;
  variables: TemplateVariable[];
  planes: TemplatePlane[];
}

export interface TemplateComponent {
  /** File name of the part (the assembly page lists it under this name). */
  name: string;
  /** Part JSON (bendcore-part). */
  text: string;
}

export interface AssemblyTemplateSettings {
  /** Material name and density given to inserted components that have none; null — none. */
  material: { name: string; density: number } | null;
  /** Parts inserted into the new assembly, one component each; the first one is fixed. */
  components: TemplateComponent[];
}

export type GostFormat = 'A0' | 'A1' | 'A2' | 'A3' | 'A4';
export type SheetOrientation = 'portrait' | 'landscape';
export type SheetStyleName = 'eskd' | 'iso7200';
export type ProjectionName = 'firstAngle' | 'thirdAngle';
export type TextFontName = 'gost2304' | 'truetype';

export interface TemplateSignature {
  surname: string;
  date: string;
}

export interface TemplateTitle {
  /** Organization (ESKD) or legal owner (ISO 7200). */
  organization: string;
  /** Developer (ESKD) or creator (ISO 7200): the surname; the date stays empty. */
  developer: string;
  department: string;
  language: string;
}

export interface DrawingTemplateSettings {
  format: GostFormat;
  orientation: SheetOrientation;
  style: SheetStyleName;
  projection: ProjectionName;
  textFont: TextFontName;
  title: TemplateTitle;
}

export interface TemplateSettingsByKind {
  part: PartTemplateSettings;
  assembly: AssemblyTemplateSettings;
  drawing: DrawingTemplateSettings;
}

export interface DocumentTemplate<K extends DocKind = DocKind> {
  format: typeof TEMPLATE_FORMAT;
  version: typeof TEMPLATE_VERSION;
  id: string;
  kind: K;
  /** Name of a user template; built-in templates are named by `nameKey` (RU/EN dictionaries). */
  name: string;
  nameKey?: MessageKey;
  descriptionKey?: MessageKey;
  builtin: boolean;
  /** ms since epoch; 0 for built-in templates */
  createdAt: number;
  settings: TemplateSettingsByKind[K];
}

export type AnyTemplate = { [K in DocKind]: DocumentTemplate<K> }[DocKind];

// ---------------------------------------------------------------------------------------------
// Built-in templates

export const EMPTY_TITLE: TemplateTitle = { organization: '', developer: '', department: '', language: '' };

const STEEL_45: TemplateMaterial = { id: 'steel_45', density: 7826 };

function drawingTemplate(
  id: string,
  nameKey: MessageKey,
  descriptionKey: MessageKey,
  settings: Omit<DrawingTemplateSettings, 'title'>,
): DocumentTemplate<'drawing'> {
  return {
    format: TEMPLATE_FORMAT,
    version: TEMPLATE_VERSION,
    id,
    kind: 'drawing',
    name: '',
    nameKey,
    descriptionKey,
    builtin: true,
    createdAt: 0,
    settings: { ...settings, title: { ...EMPTY_TITLE } },
  };
}

export const BUILTIN_TEMPLATES: readonly AnyTemplate[] = [
  {
    format: TEMPLATE_FORMAT,
    version: TEMPLATE_VERSION,
    id: 'builtin:part-eskd-mm',
    kind: 'part',
    name: '',
    nameKey: 'template.builtin.partEskd',
    descriptionKey: 'template.builtin.partEskd.hint',
    builtin: true,
    createdAt: 0,
    settings: { units: 'mm', material: STEEL_45, variables: [], planes: [] },
  },
  drawingTemplate('builtin:drawing-eskd-a3', 'template.builtin.drawingEskdA3', 'template.builtin.drawingEskd.hint', {
    format: 'A3',
    orientation: 'landscape',
    style: 'eskd',
    projection: 'firstAngle',
    textFont: 'gost2304',
  }),
  drawingTemplate('builtin:drawing-eskd-a4', 'template.builtin.drawingEskdA4', 'template.builtin.drawingEskd.hint', {
    format: 'A4',
    orientation: 'portrait',
    style: 'eskd',
    projection: 'firstAngle',
    textFont: 'gost2304',
  }),
  drawingTemplate('builtin:drawing-iso-a3-first', 'template.builtin.drawingIsoA3First', 'template.builtin.drawingIso.hint', {
    format: 'A3',
    orientation: 'landscape',
    style: 'iso7200',
    projection: 'firstAngle',
    textFont: 'truetype',
  }),
  drawingTemplate('builtin:drawing-iso-a3-third', 'template.builtin.drawingIsoA3Third', 'template.builtin.drawingIso.hint', {
    format: 'A3',
    orientation: 'landscape',
    style: 'iso7200',
    projection: 'thirdAngle',
    textFont: 'truetype',
  }),
  {
    format: TEMPLATE_FORMAT,
    version: TEMPLATE_VERSION,
    id: 'builtin:assembly',
    kind: 'assembly',
    name: '',
    nameKey: 'template.builtin.assembly',
    descriptionKey: 'template.builtin.assembly.hint',
    builtin: true,
    createdAt: 0,
    settings: { material: { name: 'Steel 45 (GOST 1050)', density: STEEL_45.density }, components: [] },
  },
];

/** Templates of a kind, built-in ones first, then the user's by name. */
export function templatesOfKind(builtin: readonly AnyTemplate[], user: readonly AnyTemplate[], kind: DocKind): AnyTemplate[] {
  const byName = (a: AnyTemplate, b: AnyTemplate) => a.name.localeCompare(b.name) || a.createdAt - b.createdAt;
  return [...builtin.filter((x) => x.kind === kind), ...[...user].filter((x) => x.kind === kind).sort(byName)];
}

// ---------------------------------------------------------------------------------------------
// Template file: reading with checks and writing

export type TemplateFileResult = { ok: true; template: AnyTemplate } | { ok: false; error: string };

const isObject = (v: unknown): v is Record<string, unknown> => typeof v === 'object' && v !== null && !Array.isArray(v);
const isString = (v: unknown): v is string => typeof v === 'string';
const oneOf = <T extends string>(v: unknown, list: readonly T[]): v is T => isString(v) && (list as readonly string[]).includes(v);

const KINDS: readonly DocKind[] = ['part', 'assembly', 'drawing'];
const FORMATS: readonly GostFormat[] = ['A0', 'A1', 'A2', 'A3', 'A4'];
const ORIENTATIONS: readonly SheetOrientation[] = ['portrait', 'landscape'];
const STYLES: readonly SheetStyleName[] = ['eskd', 'iso7200'];
const PROJECTIONS: readonly ProjectionName[] = ['firstAngle', 'thirdAngle'];
const FONTS: readonly TextFontName[] = ['gost2304', 'truetype'];
const BASES: readonly BasePlane[] = ['XY', 'XZ', 'YZ'];

function readMaterial(v: unknown, where: string): TemplateMaterial | string {
  if (!isObject(v) || !isString(v.id) || typeof v.density !== 'number' || !(v.density > 0) || !Number.isFinite(v.density)) {
    return `${where}: material needs a string id and a positive density`;
  }
  return { id: v.id, density: v.density };
}

function readPart(v: unknown): PartTemplateSettings | string {
  if (!isObject(v)) return 'settings of a part template must be an object';
  if (v.units !== 'mm') return 'units: only "mm" is supported';
  let material: TemplateMaterial | null = null;
  if (v.material !== null && v.material !== undefined) {
    const m = readMaterial(v.material, 'settings');
    if (typeof m === 'string') return m;
    material = m;
  }
  if (!Array.isArray(v.variables)) return 'variables must be an array';
  const names = new Set<string>();
  const variables: TemplateVariable[] = [];
  for (const x of v.variables) {
    if (!isObject(x) || !isString(x.name) || !isString(x.expression)) return 'a variable needs a name and an expression (strings)';
    if (!/^[\p{L}_][\p{L}\p{N}_]*$/u.test(x.name)) return `variable name "${x.name}" is not an identifier`;
    if (names.has(x.name)) return `variable "${x.name}" is listed twice`;
    names.add(x.name);
    variables.push({ name: x.name, expression: x.expression });
  }
  if (!Array.isArray(v.planes)) return 'planes must be an array';
  const planes: TemplatePlane[] = [];
  for (const x of v.planes) {
    if (!isObject(x) || !isString(x.label) || !oneOf(x.base, BASES) || !isString(x.offset)) {
      return 'a plane needs a label, a base plane (XY, XZ, YZ) and an offset (string)';
    }
    planes.push({ label: x.label, base: x.base, offset: x.offset });
  }
  return { units: 'mm', material, variables, planes };
}

function readAssembly(v: unknown): AssemblyTemplateSettings | string {
  if (!isObject(v)) return 'settings of an assembly template must be an object';
  let material: AssemblyTemplateSettings['material'] = null;
  if (v.material !== null && v.material !== undefined) {
    const m = v.material;
    if (!isObject(m) || !isString(m.name) || typeof m.density !== 'number' || !(m.density > 0) || !Number.isFinite(m.density)) {
      return 'material needs a name and a positive density';
    }
    material = { name: m.name, density: m.density };
  }
  if (!Array.isArray(v.components)) return 'components must be an array';
  const components: TemplateComponent[] = [];
  for (const c of v.components) {
    if (!isObject(c) || !isString(c.name) || !isString(c.text)) return 'a component needs a name and the part text (strings)';
    components.push({ name: c.name, text: c.text });
  }
  return { material, components };
}

function readDrawing(v: unknown): DrawingTemplateSettings | string {
  if (!isObject(v)) return 'settings of a drawing template must be an object';
  if (!oneOf(v.format, FORMATS)) return 'format must be A0…A4';
  if (!oneOf(v.orientation, ORIENTATIONS)) return 'orientation must be portrait or landscape';
  if (!oneOf(v.style, STYLES)) return 'style must be eskd or iso7200';
  if (!oneOf(v.projection, PROJECTIONS)) return 'projection must be firstAngle or thirdAngle';
  if (!oneOf(v.textFont, FONTS)) return 'textFont must be gost2304 or truetype';
  const t = isObject(v.title) ? v.title : {};
  const title: TemplateTitle = { ...EMPTY_TITLE };
  for (const k of Object.keys(EMPTY_TITLE) as (keyof TemplateTitle)[]) {
    const x = t[k];
    if (x !== undefined && !isString(x)) return `title.${k} must be a string`;
    if (isString(x)) title[k] = x;
  }
  return { format: v.format, orientation: v.orientation, style: v.style, projection: v.projection, textFont: v.textFont, title };
}

/** Checks the settings of a template of `kind`; the cleaned settings or the first problem. */
export function readSettings<K extends DocKind>(kind: K, v: unknown): TemplateSettingsByKind[K] | string {
  const r = kind === 'part' ? readPart(v) : kind === 'assembly' ? readAssembly(v) : readDrawing(v);
  return r as TemplateSettingsByKind[K] | string;
}

/** A template from its parsed JSON (user templates only: a file never makes a built-in one). */
export function readTemplate(value: unknown, fresh: { id: string; now: number }): TemplateFileResult {
  if (!isObject(value)) return { ok: false, error: 'the JSON value is not an object' };
  if (value.format !== TEMPLATE_FORMAT) return { ok: false, error: `format must be "${TEMPLATE_FORMAT}"` };
  if (typeof value.version !== 'number' || value.version > TEMPLATE_VERSION) {
    return { ok: false, error: `unsupported template version ${String(value.version)}` };
  }
  if (!oneOf(value.kind, KINDS)) return { ok: false, error: 'kind must be part, assembly or drawing' };
  const name = isString(value.name) ? value.name.trim() : '';
  if (name === '') return { ok: false, error: 'the template has no name' };
  const settings = readSettings(value.kind, value.settings);
  if (typeof settings === 'string') return { ok: false, error: settings };
  return {
    ok: true,
    template: {
      format: TEMPLATE_FORMAT,
      version: TEMPLATE_VERSION,
      id: fresh.id,
      kind: value.kind,
      name,
      builtin: false,
      createdAt: fresh.now,
      settings,
    } as AnyTemplate,
  };
}

export function parseTemplateFile(text: string, fresh: { id: string; now: number }): TemplateFileResult {
  let value: unknown;
  try {
    value = JSON.parse(text);
  } catch (e) {
    return { ok: false, error: `not JSON (${e instanceof Error ? e.message : String(e)})` };
  }
  return readTemplate(value, fresh);
}

/** Text of the template file; `name` is the file's template name (built-in ones need a localized one). */
export function serializeTemplate(template: AnyTemplate, name?: string): string {
  const { format, version, kind, settings } = template;
  return `${JSON.stringify({ format, version, kind, name: name ?? template.name, settings }, null, 2)}\n`;
}

/** File name for downloads: Latin letters and digits (Chromium replaces a Cyrillic name). */
export function templateFileName(name: string, id: string): string {
  const base = name
    .normalize('NFKD')
    .replace(/[^A-Za-z0-9._-]+/g, '-')
    .replace(/^-+|-+$/g, '');
  return `${base || `template-${id.replace(/[^A-Za-z0-9]/g, '').slice(-6) || 'file'}`}${TEMPLATE_EXTENSION}`;
}

let counter = 0;
/** Id of a new user template. */
export function newTemplateId(now = Date.now()): string {
  counter += 1;
  return `user:${now.toString(36)}-${counter.toString(36)}-${Math.random().toString(36).slice(2, 6)}`;
}

// ---------------------------------------------------------------------------------------------
// Settings taken from documents ("Save as template")

/** Settings of a part from the part file: material, variables and offset datum planes. */
export function partSettingsFromFile(text: string): PartTemplateSettings | string {
  let doc: unknown;
  try {
    doc = JSON.parse(text);
  } catch (e) {
    return `not JSON (${e instanceof Error ? e.message : String(e)})`;
  }
  if (!isObject(doc) || doc.format !== 'bendcore-part') return 'not a bendcore-part file';
  let material: TemplateMaterial | null = null;
  if (isObject(doc.material) && isString(doc.material.id) && typeof doc.material.density === 'number' && doc.material.density > 0) {
    material = { id: doc.material.id, density: doc.material.density };
  }
  const variables: TemplateVariable[] = [];
  for (const x of Array.isArray(doc.variables) ? doc.variables : []) {
    if (isObject(x) && isString(x.name) && isString(x.expression)) variables.push({ name: x.name, expression: x.expression });
  }
  const planes: TemplatePlane[] = [];
  for (const f of Array.isArray(doc.features) ? doc.features : []) {
    if (!isObject(f) || f.type !== 'DatumPlane' || !isObject(f.options) || !isObject(f.parameters)) continue;
    // Only planes made from a base plane by an offset: the others need geometry of the part.
    if (f.options.method !== 'offset' || !oneOf(f.options.base, BASES)) continue;
    const links = isObject(f.links) ? Object.keys(f.links).length : 0;
    if (links > 0) continue;
    const offset = f.parameters.offset;
    planes.push({
      label: isString(f.label) && f.label !== '' ? f.label : isString(f.id) ? f.id : 'Plane',
      base: f.options.base,
      offset: isString(offset) || typeof offset === 'number' ? String(offset) : '0',
    });
  }
  return { units: 'mm', material, variables, planes };
}

/** The pieces of a drawing description that a template keeps (DrawingDescription of the kernel). */
export interface DescribedDrawing {
  projection: ProjectionName;
  sheetStyle: SheetStyleName;
  textFont: TextFontName;
  sheets: {
    format: GostFormat;
    orientation: SheetOrientation;
    title: { organization: string; developer: { surname: string }; department: string; language: string };
  }[];
}

/** Settings of a drawing from its description: the first sheet gives the format and the title. */
export function drawingSettingsFromDescription(d: DescribedDrawing): DrawingTemplateSettings | string {
  const sheet = d.sheets[0];
  if (!sheet) return 'the drawing has no sheet';
  return {
    format: sheet.format,
    orientation: sheet.orientation,
    style: d.sheetStyle,
    projection: d.projection,
    textFont: d.textFont,
    title: {
      organization: sheet.title.organization,
      developer: sheet.title.developer.surname,
      department: sheet.title.department,
      language: sheet.title.language,
    },
  };
}

/** The definitions of an assembly state that a template keeps. */
export interface DescribedAssembly {
  definitions: { kind: 'part' | 'assembly'; properties: { material: string; density: number | null } }[];
}

/** The most common material and density over the part definitions that have a density. */
export function assemblyMaterialFromState(s: DescribedAssembly): AssemblyTemplateSettings['material'] {
  const counts = new Map<string, { name: string; density: number; n: number }>();
  for (const d of s.definitions) {
    if (d.kind !== 'part' || d.properties.density === null || !(d.properties.density > 0)) continue;
    const key = `${d.properties.material}\u0000${d.properties.density}`;
    const e = counts.get(key) ?? { name: d.properties.material, density: d.properties.density, n: 0 };
    e.n += 1;
    counts.set(key, e);
  }
  let best: { name: string; density: number; n: number } | null = null;
  for (const e of counts.values()) if (!best || e.n > best.n) best = e;
  return best ? { name: best.name, density: best.density } : null;
}

/** Parts of an assembly file for a template: one entry per definition inserted from a part. */
export function componentsFromParts(parts: readonly { definition: number; name: string; text: string }[] | undefined): TemplateComponent[] {
  const seen = new Set<string>();
  const out: TemplateComponent[] = [];
  for (const p of [...(parts ?? [])].filter((x) => x.definition >= 0).sort((a, b) => a.definition - b.definition)) {
    const key = `${p.name}\u0000${p.text}`;
    if (seen.has(key)) continue;
    seen.add(key);
    out.push({ name: p.name, text: p.text });
  }
  return out;
}

/** Properties for a component whose part file has no material of its own; null — keep the file's. */
export function defaultComponentProperties(
  partText: string,
  material: AssemblyTemplateSettings['material'],
): { material: string; density: number } | null {
  if (!material) return null;
  try {
    const doc = JSON.parse(partText) as unknown;
    if (isObject(doc) && isObject(doc.material) && typeof doc.material.density === 'number' && doc.material.density > 0) return null;
  } catch {
    return { material: material.name, density: material.density };
  }
  return { material: material.name, density: material.density };
}

/** The title fields of a drawing template for the kernel (CreateOptions.sheet.title). */
export function titleInput(title: TemplateTitle): {
  organization: string;
  department: string;
  language: string;
  developer: { surname: string };
} {
  return { organization: title.organization, department: title.department, language: title.language, developer: { surname: title.developer } };
}
