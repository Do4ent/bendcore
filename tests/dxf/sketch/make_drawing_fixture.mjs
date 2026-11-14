// Writes tests/dxf/sketch/plate_drawing.dxf: the sheet of a bendcore drawing (bendcore_drawing_v2
// DXF writer through bendcore_drawing_wasm) with the top view of a plate 60 x 40 x 10 with holes
// d = 16 at (20, 20) and d = 10 at (45, 20) (the part is built by bendcore_modeling_wasm), and
// plate_drawing.json with the sheet scale and the view placement the DXF import test needs
// (tests/wasm/dxf_sketch.test.mjs).
//
//   node tests/dxf/sketch/make_drawing_fixture.mjs --modeling <dir> --drawing <dir> [--out <dir>]

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
function argument(name, fallback) {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 && i + 1 < process.argv.length ? path.resolve(process.argv[i + 1]) : fallback;
}
const modelingDir = argument('modeling', null);
const drawingDir = argument('drawing', null);
const out = argument('out', here);
if (!modelingDir || !drawingDir) {
  console.error('usage: make_drawing_fixture.mjs --modeling <dir> --drawing <dir> [--out <dir>]');
  process.exit(2);
}

const mglue = await import(pathToFileURL(path.join(modelingDir, 'bendcore_modeling.mjs')).href);
const mapi = await import(pathToFileURL(path.join(modelingDir, 'bendcore_modeling_api.mjs')).href);
const modeling = await mapi.loadModeling(mglue.default);
const dglue = await import(pathToFileURL(path.join(drawingDir, 'bendcore_drawing.mjs')).href);
const dapi = await import(pathToFileURL(path.join(drawingDir, 'bendcore_drawing_api.mjs')).href);
const drawingKernel = await dapi.loadDrawingKernel(dglue.default);
drawingKernel.setFont(new Uint8Array(fs.readFileSync(path.join(drawingDir, 'DejaVuSans.ttf'))), 'DejaVuSans.ttf');

const doc = modeling.createDocument();
let partJson;
try {
  const sketch = modeling.sketchTemplate({
    profiles: [
      { kind: 'rectangle', x: 0, y: 0, width: 60, height: 40 },
      { kind: 'circle', cx: 20, cy: 20, radius: 8 },
      { kind: 'circle', cx: 45, cy: 20, radius: 5 },
    ],
  });
  doc.addFeature({ type: 'Sketch', id: 'Profile', options: { plane: 'XY' }, sketch });
  doc.addFeature({ type: 'Extrude', id: 'Plate', parameters: { distance: 10 }, links: { sketch: 'Profile' } });
  doc.rebuild();
  partJson = doc.save();
} finally {
  doc.dispose();
}

const drawing = drawingKernel.createFromPart(partJson, { path: 'plate.part.json' });
try {
  const view = drawing.addView('S1', { kind: 'base', standard: 'top' });
  const sheet = drawing.describe().sheets[0];
  const scale = sheet.computedScale.ratio[0] / sheet.computedScale.ratio[1];
  const bounds = drawing.sheetPicture('S1').views.find((v) => v.id === view).bounds;
  fs.writeFileSync(path.join(out, 'plate_drawing.dxf'), Buffer.from(drawing.exportSheet('S1', 'dxf')));
  fs.writeFileSync(
    path.join(out, 'plate_drawing.json'),
    JSON.stringify({ part: { width: 60, height: 40, holes: [{ x: 20, y: 20, d: 16 }, { x: 45, y: 20, d: 10 }] }, scale, bounds }, null, 2) + '\n',
  );
  console.log(`plate_drawing.dxf: scale ${sheet.computedScale.label}, view bounds ${JSON.stringify(bounds)}`);
} finally {
  drawing.dispose();
}
