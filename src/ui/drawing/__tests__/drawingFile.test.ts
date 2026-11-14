// Drawing file of the application (src/ui/drawing/drawingFile.ts, docs/ui/DRAWING_UI.md):
// packing, unpacking, the earlier two-file format and drafts of the earlier shell.

import {
  DRAWING_FILE_FORMAT,
  drawingFileName,
  migrateDrawingDocument,
  packDrawingFile,
  readDrawingFiles,
  unpackDrawingFile,
} from '../drawingFile';
import { detectKind } from '../../app/storage';

// Kernel JSON texts: exact bytes, including a number a JavaScript double cannot hold (2^63 + 1)
// and the kernel's own indentation; they must come back unchanged.
const drawing = '{\n  "format": "bendcore-drawing",\n  "version": 2,\n  "model": {"kind": "part", "path": "a.bcpart.json"},\n  "big": 9223372036854775809\n}';
const part = '{"format":"bendcore-part","version":3,"features":[{"id":"S","x":0.1000000000000000055511151231257827}]}';
const assembly = '{"format":"bendcore-assembly","version":1}';

describe('drawing file', () => {
  test('pack and unpack keep both documents byte for byte', () => {
    const text = packDrawingFile({ drawing, model: part, modelName: 'кронштейн.bcpart.json' });
    expect(JSON.parse(text).format).toBe(DRAWING_FILE_FORMAT);
    const r = unpackDrawingFile(text);
    expect(r).toEqual({ ok: true, migrated: false, content: { drawing, model: part, modelName: 'кронштейн.bcpart.json' } });
  });

  test('the shell detects the drawing file and the earlier drawing document as drawings', () => {
    expect(detectKind(packDrawingFile({ drawing, model: assembly, modelName: 'a' }))).toEqual({ kind: 'drawing' });
    expect(detectKind(drawing)).toEqual({ kind: 'drawing' });
  });

  test('errors: not JSON, other format, wrong version, fields of the wrong kind', () => {
    expect(unpackDrawingFile('{').ok).toBe(false);
    const other = unpackDrawingFile(part);
    expect(other.ok === false && other.error.code).toBe('notDrawing');
    const legacy = unpackDrawingFile(drawing);
    expect(legacy.ok === false && legacy.error.code).toBe('needsModel');
    const v2 = unpackDrawingFile(JSON.stringify({ format: DRAWING_FILE_FORMAT, version: 2, drawing, model: part }));
    expect(v2.ok === false && v2.error.code).toBe('invalid');
    const objects = unpackDrawingFile(JSON.stringify({ format: DRAWING_FILE_FORMAT, version: 1, drawing: JSON.parse(part), model: part }));
    expect(objects.ok === false && objects.error.code).toBe('invalid');
    const swapped = unpackDrawingFile(JSON.stringify({ format: DRAWING_FILE_FORMAT, version: 1, drawing: part, model: drawing }));
    expect(swapped.ok === false && swapped.error.detail).toMatch(/"drawing" is not a bendcore-drawing/);
  });

  test('the earlier pair .drawing.json + .part.json is read as one drawing file', () => {
    const r = readDrawingFiles([
      { name: 'RD_10_001.part.json', text: part },
      { name: 'RD_10_001.drawing.json', text: drawing },
    ]);
    expect(r).toEqual({
      ok: true,
      migrated: true,
      name: 'RD_10_001.bcdrw.json',
      content: { drawing, model: part, modelName: 'RD_10_001.part.json' },
    });
    // Without the model file: needsModel.
    const alone = readDrawingFiles([{ name: 'x.drawing.json', text: drawing }]);
    expect(alone.ok === false && alone.error.code).toBe('needsModel');
    // A drawing file among other files is taken as is.
    const pack = packDrawingFile({ drawing, model: assembly, modelName: 'sb.bcasm.json' });
    const p = readDrawingFiles([{ name: 'a.bcpart.json', text: part }, { name: 'sb.bcdrw.json', text: pack }]);
    expect(p.ok && p.name).toBe('sb.bcdrw.json');
    expect(p.ok && p.migrated).toBe(false);
    expect(p.ok && p.content.model).toBe(assembly);
    const none = readDrawingFiles([{ name: 'a.bcpart.json', text: part }]);
    expect(none.ok === false && none.error.code).toBe('notDrawing');
  });

  test('file names', () => {
    expect(drawingFileName('кронштейн.bcpart.json')).toBe('кронштейн.bcdrw.json');
    expect(drawingFileName('RD.drawing.json')).toBe('RD.bcdrw.json');
    expect(drawingFileName('sb.bcasm.json')).toBe('sb.bcdrw.json');
    expect(drawingFileName('plate.json')).toBe('plate.bcdrw.json');
    expect(drawingFileName('x.bcdrw.json')).toBe('x.bcdrw.json');
    expect(drawingFileName('')).toBe('drawing.bcdrw.json');
  });

  test('drafts of the earlier shell are packed, other documents pass unchanged', () => {
    const old = { kind: 'drawing', name: 'a.drawing.json', text: drawing, model: part };
    const migrated = migrateDrawingDocument(old);
    expect(migrated.model).toBeUndefined();
    expect(migrated.name).toBe('a.bcdrw.json');
    const r = unpackDrawingFile(migrated.text);
    expect(r.ok && r.content.drawing).toBe(drawing);
    expect(r.ok && r.content.model).toBe(part);
    const partFile = { kind: 'part', name: 'a.bcpart.json', text: part };
    expect(migrateDrawingDocument(partFile)).toBe(partFile);
    const current = { kind: 'drawing', name: 'b.bcdrw.json', text: migrated.text };
    expect(migrateDrawingDocument(current)).toBe(current);
  });
});
