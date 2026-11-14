// Writes tests/wasm/fixtures/threaded_block.part.json: the block 60 × 40 × 20 with a blind threaded
// hole M10×1,25LH (thread length 12, drill depth 18) at (20, 20) from the top face — the part of
// tests/drawing_v2/test_drawing_threads.cpp (ThreadPart), built through the bendcore_modeling_wasm
// JS API. The drawing editor test (tests/e2e-drawing/drawing_tools.spec.ts) puts a thread
// dimension on it.
//
//   node tests/wasm/fixtures/make_threaded_block_part.mjs --module <dir with bendcore_modeling.mjs>

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const i = process.argv.indexOf('--module');
const moduleDir = path.resolve(i >= 0 ? process.argv[i + 1] : path.join(here, '../../../build-wasm-modeling/wasm_modeling'));
const glue = await import(pathToFileURL(path.join(moduleDir, 'bendcore_modeling.mjs')).href);
const api = await import(pathToFileURL(path.join(moduleDir, 'bendcore_modeling_api.mjs')).href);
const modeling = await api.loadModeling(glue.default);
const near = (a, b) => Math.abs(a - b) <= 1e-3;

function faceWhere(mesh, pred) {
  const found = [];
  for (let s = 0; s < mesh.faceSpans.length; s += 3) {
    let all = true;
    for (let k = mesh.faceSpans[s + 1]; k < mesh.faceSpans[s + 1] + mesh.faceSpans[s + 2] && all; ++k) {
      const v = mesh.indices[k] * 3;
      all = pred(mesh.positions[v], mesh.positions[v + 1], mesh.positions[v + 2]);
    }
    if (all) found.push(mesh.faceSpans[s]);
  }
  if (found.length !== 1) throw new Error(`${found.length} faces match`);
  return found[0];
}
function rebuildOk(doc) {
  doc.rebuild();
  const bad = doc.features().filter((f) => f.state !== 'ok');
  if (bad.length) throw new Error(bad.map((f) => `${f.id}: ${f.messages.join('; ')}`).join(', '));
}

const doc = modeling.createDocument();
doc.addFeature({
  type: 'Sketch',
  id: 'Profile',
  options: { plane: 'XY' },
  sketch: modeling.sketchTemplate({ profiles: [{ kind: 'polygon', points: [[0, 0], [60, 0], [60, 40], [0, 40]] }] }),
});
doc.addFeature({ type: 'Extrude', id: 'Block', parameters: { distance: '20' }, links: { sketch: 'Profile' } });
rebuildOk(doc);
const mesh = doc.tessellate('final', { linearDeflection: 0.05, angularDeflection: 0.2 });
const top = doc.makeReference('final', 'face', faceWhere(mesh, (x, y, z) => near(z, 20)));
doc.addFeature({
  type: 'ThreadedHole',
  id: 'Tap',
  parameters: { nominal: '10', pitch: '1.25', threadLength: '12', drillDepth: '18', x: '20', y: '20', z: '20' },
  references: { face: [top] },
  options: { hand: 'left', drillPoint: 'false' },
});
rebuildOk(doc);
const v = doc.properties('final').volume;
// Volume: block − drill Ø(D − P) = 8,75 to the depth 18 (flat bottom).
const expected = 60 * 40 * 20 - (Math.PI * 8.75 * 8.75 / 4) * 18;
if (Math.abs(v - expected) / expected > 1e-6) throw new Error(`block volume ${v} vs ${expected}`);
fs.writeFileSync(path.join(here, 'threaded_block.part.json'), doc.save() + '\n');
doc.dispose();
console.log(`threaded_block.part.json written, volume ${v.toFixed(3)} mm³`);
