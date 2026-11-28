/**
 * Drawing file of the application (docs/ui/DRAWING_UI.md, "Файл чертежа"): one file
 * `<name>.bcdrw.json` that holds the drawing document of bendcore_drawing_wasm together with the
 * document of its model (part or assembly).
 *
 *   {
 *     "format": "bendcore-drawing-file", "version": 1,
 *     "modelName": "кронштейн.bcpart.json",
 *     "drawing": "<bendcore-drawing JSON, as DrawingHandle.save() wrote it>",
 *     "model":   "<bendcore-part or bendcore-assembly JSON, as DrawingHandle.saveModel() wrote it>",
 *     "specification": { "dissolvedAssemblies": ["УВ.02.000"],      (optional, assembly drawings)
 *                        "groupSpecification": { "enabled": true, "layout": "columns", "columnHeaders": [] } }
 *   }
 *
 * "specification" keeps the settings of the specification document of the session that the
 * drawing document does not hold: the sub-assemblies listed by their parts (setSettings
 * {dissolvedAssemblies}) and the group specification of GOST 2.113 (setSettings
 * {groupSpecification}); the editor applies them again after DrawingKernel.load. Each is written
 * only when it is not the default, so files without them stay as before.
 *
 * Why the model is embedded and not referenced by a relative path: a page in the browser cannot
 * open a file by path (only files the user picks), drafts, recent files and the "part → drawing"
 * transfer between tabs keep one text per document, and a drawing must open exactly on the model
 * its views and dimension references were computed from (DrawingKernel.load checks the model hash
 * and reports the changes). A newer model is brought in by the editor ("Заменить модель…") and the
 * update report then lists what changed. Both documents are kept as JSON strings, not as nested
 * objects, so they reach the kernel byte for byte (no number passes through a JavaScript double).
 *
 * Older files: the editor used to save two files, `<name>.drawing.json` (format
 * "bendcore-drawing") and `<name>.part.json` / `.assembly.json`. readDrawingFiles() accepts that
 * pair (chosen together) and returns the same content with migrated = true; the next save writes
 * one `.bcdrw.json`. Drafts of the earlier shell kept { text: drawing, model } and are migrated by
 * migrateDrawingDocument().
 */

export const DRAWING_FILE_FORMAT = 'bendcore-drawing-file';
export const DRAWING_FILE_VERSION = 1;
export const DRAWING_FILE_EXTENSION = '.bcdrw.json';

export interface DrawingFileContent {
  /** Drawing document JSON (format "bendcore-drawing"). */
  drawing: string;
  /** Model document JSON (format "bendcore-part" or "bendcore-assembly"). */
  model: string;
  /** File name of the model when the drawing was made (shown in the editor). */
  modelName: string;
  /** Sub-assemblies listed by their parts in the specification (component keys); none if absent. */
  dissolvedAssemblies?: string[];
  /** Group specification of GOST 2.113 (assembly configurations); absent — off. */
  groupSpecification?: GroupSpecificationSettings;
}

export interface GroupSpecificationSettings {
  enabled: boolean;
  layout: 'columns' | 'variableData';
  columnHeaders: string[];
}

/** The group specification settings worth keeping in the file: null when they are the defaults. */
export function storedGroupSpecification(g: GroupSpecificationSettings | null | undefined): GroupSpecificationSettings | null {
  if (!g) return null;
  if (!g.enabled && g.layout === 'columns' && g.columnHeaders.length === 0) return null;
  return { enabled: g.enabled, layout: g.layout, columnHeaders: [...g.columnHeaders] };
}

export type DrawingFileError =
  /** The text is not JSON or not a drawing file. */
  | { code: 'notDrawing'; detail: string }
  /** An older drawing document chosen without its model file. */
  | { code: 'needsModel'; detail: string }
  /** A drawing file of a newer version, or with a field of the wrong type. */
  | { code: 'invalid'; detail: string };

export type DrawingFileResult = { ok: true; content: DrawingFileContent; migrated: boolean } | { ok: false; error: DrawingFileError };

function formatOf(text: string): { format: unknown; value: Record<string, unknown> } | { error: string } {
  try {
    const value = JSON.parse(text) as unknown;
    if (typeof value !== 'object' || value === null || Array.isArray(value)) return { error: 'the JSON value is not an object' };
    return { format: (value as { format?: unknown }).format, value: value as Record<string, unknown> };
  } catch (e) {
    return { error: `not JSON (${e instanceof Error ? e.message : String(e)})` };
  }
}

const MODEL_FORMATS = new Set(['bendcore-part', 'bendcore-assembly']);

/** Text of the drawing file. */
export function packDrawingFile(content: DrawingFileContent): string {
  return JSON.stringify(
    {
      format: DRAWING_FILE_FORMAT,
      version: DRAWING_FILE_VERSION,
      modelName: content.modelName,
      drawing: content.drawing,
      model: content.model,
      ...specificationField(content),
    },
    null,
    1,
  );
}

function specificationField(content: DrawingFileContent): { specification?: Record<string, unknown> } {
  const spec: Record<string, unknown> = {};
  if (content.dissolvedAssemblies && content.dissolvedAssemblies.length > 0) spec.dissolvedAssemblies = [...content.dissolvedAssemblies];
  const group = storedGroupSpecification(content.groupSpecification);
  if (group) spec.groupSpecification = group;
  return Object.keys(spec).length > 0 ? { specification: spec } : {};
}

/** Content of a drawing file text (format "bendcore-drawing-file"). */
export function unpackDrawingFile(text: string): DrawingFileResult {
  const f = formatOf(text);
  if ('error' in f) return { ok: false, error: { code: 'notDrawing', detail: f.error } };
  if (f.format === 'bendcore-drawing') {
    return { ok: false, error: { code: 'needsModel', detail: 'a drawing document of the earlier format: choose it together with its model file' } };
  }
  if (f.format !== DRAWING_FILE_FORMAT) {
    return { ok: false, error: { code: 'notDrawing', detail: `format ${JSON.stringify(f.format ?? null)} is not a drawing file` } };
  }
  const { version, drawing, model, modelName, specification } = f.value;
  if (version !== DRAWING_FILE_VERSION) {
    return { ok: false, error: { code: 'invalid', detail: `drawing file version ${JSON.stringify(version ?? null)} is not supported (expected ${DRAWING_FILE_VERSION})` } };
  }
  if (typeof drawing !== 'string' || typeof model !== 'string') {
    return { ok: false, error: { code: 'invalid', detail: 'the fields "drawing" and "model" must be JSON texts (strings)' } };
  }
  const d = formatOf(drawing);
  if ('error' in d || d.format !== 'bendcore-drawing') {
    return { ok: false, error: { code: 'invalid', detail: 'the field "drawing" is not a bendcore-drawing document' } };
  }
  const m = formatOf(model);
  if ('error' in m || !MODEL_FORMATS.has(String(m.format))) {
    return { ok: false, error: { code: 'invalid', detail: 'the field "model" is not a bendcore-part or bendcore-assembly document' } };
  }
  const dissolved = dissolvedOf(specification);
  if (dissolved === null) {
    return { ok: false, error: { code: 'invalid', detail: 'the field "specification.dissolvedAssemblies" must be a list of texts' } };
  }
  const group = groupSpecificationOf(specification);
  if (group === undefined) {
    return { ok: false, error: { code: 'invalid', detail: 'the field "specification.groupSpecification" must be {enabled, layout, columnHeaders}' } };
  }
  const content: DrawingFileContent = { drawing, model, modelName: typeof modelName === 'string' ? modelName : '' };
  if (dissolved.length > 0) content.dissolvedAssemblies = dissolved;
  if (group) content.groupSpecification = group;
  return { ok: true, content, migrated: false };
}

/** Dissolved sub-assemblies of the optional "specification" field; null when malformed. */
function dissolvedOf(specification: unknown): string[] | null {
  if (specification === undefined) return [];
  if (typeof specification !== 'object' || specification === null || Array.isArray(specification)) return null;
  const list = (specification as { dissolvedAssemblies?: unknown }).dissolvedAssemblies;
  if (list === undefined) return [];
  if (!Array.isArray(list) || !list.every((k) => typeof k === 'string')) return null;
  return list as string[];
}

/** Group specification of the optional "specification" field: null — absent, undefined — malformed. */
function groupSpecificationOf(specification: unknown): GroupSpecificationSettings | null | undefined {
  if (typeof specification !== 'object' || specification === null) return null;
  const g = (specification as { groupSpecification?: unknown }).groupSpecification;
  if (g === undefined) return null;
  if (typeof g !== 'object' || g === null || Array.isArray(g)) return undefined;
  const { enabled, layout, columnHeaders } = g as Record<string, unknown>;
  if (typeof enabled !== 'boolean' || (layout !== 'columns' && layout !== 'variableData')) return undefined;
  if (!Array.isArray(columnHeaders) || !columnHeaders.every((h) => typeof h === 'string')) return undefined;
  return { enabled, layout, columnHeaders: columnHeaders as string[] };
}

export interface NamedText {
  name: string;
  text: string;
}

/**
 * The drawing among files the user chose together: a drawing file, or an older drawing document
 * with its model file (migrated). `name` is the name to save the drawing under.
 */
export function readDrawingFiles(files: NamedText[]): ({ name: string } & DrawingFileResult) | { ok: false; error: DrawingFileError; name: string } {
  const typed = files.map((f) => ({ ...f, parsed: formatOf(f.text) }));
  const fileOf = (fmt: string) => typed.find((x) => !('error' in x.parsed) && x.parsed.format === fmt);
  const pack = fileOf(DRAWING_FILE_FORMAT);
  if (pack) return { name: pack.name, ...unpackDrawingFile(pack.text) };
  const legacy = fileOf('bendcore-drawing');
  if (!legacy) {
    const name = files[0]?.name ?? '';
    return { ok: false, name, error: { code: 'notDrawing', detail: 'none of the files is a drawing (format bendcore-drawing-file or bendcore-drawing)' } };
  }
  const model = typed.find((x) => x !== legacy && !('error' in x.parsed) && MODEL_FORMATS.has(String(x.parsed.format)));
  if (!model) {
    return { ok: false, name: legacy.name, error: { code: 'needsModel', detail: 'a drawing document of the earlier format opens together with its model file' } };
  }
  return {
    ok: true,
    name: drawingFileName(legacy.name),
    content: { drawing: legacy.text, model: model.text, modelName: model.name },
    migrated: true,
  };
}

/** `<base>.bcdrw.json` for a model or older drawing file name, or a designation. */
export function drawingFileName(name: string): string {
  const base = name
    .trim()
    .replace(/(\.bcdrw|\.drawing|\.part|\.assembly|\.bcpart|\.bcasm)?\.json$/i, '')
    .trim();
  return `${base || 'drawing'}${DRAWING_FILE_EXTENSION}`;
}

/**
 * A drawing document of the shell (draft, recent file, tab) in the current form: `text` is the
 * drawing file. Earlier drafts kept the drawing document in `text` and the model in `model`;
 * they are packed here. Other kinds pass unchanged.
 */
export function migrateDrawingDocument<T extends { kind: string; name: string; text: string; model?: string }>(file: T): T {
  if (file.kind !== 'drawing' || file.model === undefined) return file;
  const f = formatOf(file.text);
  if ('error' in f || f.format !== 'bendcore-drawing') return file;
  const { model, ...rest } = file;
  return { ...rest, name: drawingFileName(file.name), text: packDrawingFile({ drawing: file.text, model, modelName: '' }) } as T;
}
