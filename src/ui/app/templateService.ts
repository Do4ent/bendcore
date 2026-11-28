/**
 * Kernel side of the document templates (docs/ui/APP_SHELL.md, "Шаблоны"): a new document is
 * built by the kernel module of its kind (variables, datum planes, material, sheet settings,
 * components are real kernel calls, nothing is written into the file by hand), and the settings of
 * an open document are read back for "Save as template". Each call starts the module in its own
 * Web Worker and stops it at the end; the editor tabs have workers of their own.
 */

import { defaultAssemblyBaseUrl, loadAssemblyFrom } from '../assembly/assemblyLoader';
import { defaultDrawingBaseUrl, loadDrawingFrom } from '../drawing/drawingLoader';
import { defaultModelingBaseUrl, loadModelingFrom } from '../modeling/modelingLoader';
import { DrawingFileContent, drawingFileName, packDrawingFile, unpackDrawingFile } from '../drawing/drawingFile';
import type { DocumentFile } from './editorHost';
import {
  assemblyMaterialFromState,
  componentsFromParts,
  defaultComponentProperties,
  drawingSettingsFromDescription,
  partSettingsFromFile,
  titleInput,
} from './templates';
import type { AssemblyTemplateSettings, DocumentTemplate, DrawingTemplateSettings, PartTemplateSettings } from './templates';
import type { WasmClient } from '../worker/wasmClient';

/** Runs `body` with a started module and stops the module afterwards, also after a failure. */
async function withModule<M, R>(start: () => Promise<WasmClient<M>>, body: (root: WasmClient<M>['root']) => Promise<R>): Promise<R> {
  const client = await start();
  try {
    return await body(client.root);
  } finally {
    client.dispose();
  }
}

/** Library materials a part template may name are checked by the part editor itself; here the file only keeps id and density. */
export function buildPart(settings: PartTemplateSettings, fileName: string): Promise<DocumentFile> {
  return withModule(
    () => loadModelingFrom(defaultModelingBaseUrl()),
    async (kernel) => {
      const doc = await kernel.createDocument();
      try {
        for (const v of settings.variables) await doc.setVariable(v.name, v.expression);
        for (const p of settings.planes) {
          await doc.addFeature({ type: 'DatumPlane', label: p.label, options: { method: 'offset', base: p.base }, parameters: { offset: p.offset } });
        }
        const text = await doc.save(settings.material ? { material: settings.material } : {});
        return { kind: 'part' as const, name: fileName, text };
      } finally {
        await doc.dispose();
      }
    },
  );
}

export function buildAssembly(settings: AssemblyTemplateSettings, name: string, fileName: string): Promise<DocumentFile> {
  return withModule(
    () => loadAssemblyFrom(defaultAssemblyBaseUrl()),
    async (kernel) => {
      const doc = await kernel.createDocument(name);
      try {
        const parts: NonNullable<DocumentFile['parts']> = [];
        for (const c of settings.components) {
          const properties = defaultComponentProperties(c.text, settings.material);
          const first = parts.length === 0;
          const baseName = c.name.replace(/\.bcpart\.json$/i, '.json').replace(/\.[^.]+$/, '');
          const r = await doc.insertPart({
            name: baseName,
            format: 'part-json',
            data: c.text,
            fixed: first,
            beside: !first,
            ...(properties ? { properties } : {}),
          });
          parts.push({ definition: r.definition, name: c.name, text: c.text });
        }
        const text = await doc.save();
        return { kind: 'assembly' as const, name: fileName, text, parts };
      } finally {
        await doc.dispose();
      }
    },
  );
}

/** Base name of a model file: without .json and the part / assembly suffixes. */
const modelBase = (fileName: string) => fileName.replace(/(\.part|\.assembly|\.bcpart|\.bcasm)?\.json$/i, '');

/** A drawing of `model` (a part or an assembly file) with the settings of the template. */
export function buildDrawing(settings: DrawingTemplateSettings, model: DocumentFile): Promise<DocumentFile> {
  return withModule(
    () => loadDrawingFrom(defaultDrawingBaseUrl()),
    async (kernel) => {
      const options = {
        path: model.name,
        sheet: { format: settings.format, orientation: settings.orientation, title: titleInput(settings.title) },
      };
      const drawing =
        model.kind === 'assembly'
          ? await kernel.createFromAssembly(model.text, options)
          : await kernel.createFromPart(model.text, { ...options, properties: { name: modelBase(model.name) } });
      try {
        await drawing.setSettings({ sheetStyle: settings.style, projection: settings.projection, textFont: settings.textFont });
        const content: DrawingFileContent = { drawing: await drawing.save(), model: await drawing.saveModel(), modelName: model.name };
        return { kind: 'drawing' as const, name: drawingFileName(model.name), text: packDrawingFile(content) };
      } finally {
        await drawing.dispose();
      }
    },
  );
}

/** Settings of an open document for "Save as template"; a message string when it cannot be read. */
export async function settingsOfDocument(file: DocumentFile): Promise<
  | { kind: 'part'; settings: PartTemplateSettings }
  | { kind: 'assembly'; settings: AssemblyTemplateSettings }
  | { kind: 'drawing'; settings: DrawingTemplateSettings }
  | string
> {
  if (file.kind === 'part') {
    const s = partSettingsFromFile(file.text);
    return typeof s === 'string' ? s : { kind: 'part', settings: s };
  }
  if (file.kind === 'assembly') {
    const state = await withModule(
      () => loadAssemblyFrom(defaultAssemblyBaseUrl()),
      async (kernel) => {
        const { document } = await kernel.loadDocument(file.text);
        try {
          return await document.state();
        } finally {
          await document.dispose();
        }
      },
    );
    return { kind: 'assembly', settings: { material: assemblyMaterialFromState(state), components: componentsFromParts(file.parts) } };
  }
  const unpacked = unpackDrawingFile(file.text);
  if (!unpacked.ok) return unpacked.error.detail;
  const described = await withModule(
    () => loadDrawingFrom(defaultDrawingBaseUrl()),
    async (kernel) => {
      const { drawing } = await kernel.load(unpacked.content.drawing, unpacked.content.model);
      try {
        return await drawing.describe();
      } finally {
        await drawing.dispose();
      }
    },
  );
  const s = drawingSettingsFromDescription(described);
  return typeof s === 'string' ? s : { kind: 'drawing', settings: s };
}

export type { DocumentTemplate };
