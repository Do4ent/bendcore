// Writes tests/wasm/fixtures/bracket.part.json: reference part 1 of docs/modeling/REFERENCE_PARTS.md
// (bracket: L-profile A × B × T extruded symmetrically by Wd, inner fillet r, hole d at hx), built
// through the bendcore_modeling_wasm JS API exactly as tests/drawing_v2 builds it in C++
// (makeBracket). The drawing tests (tests/wasm/drawing_document.test.mjs, tests/e2e-drawing)
// open this file as the model of the drawing.
//
//   node tests/wasm/fixtures/make_bracket_part.mjs --module <dir with bendcore_modeling.mjs>

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

function edgeWhere(mesh, pred) {
  const found = mesh.edges.filter((e) => {
    for (let k = 0; k < e.points.length; k += 3) if (!pred(e.points[k], e.points[k + 1], e.points[k + 2])) return false;
    return true;
  });
  if (found.length !== 1) throw new Error(`${found.length} edges match`);
  return found[0].edgeIndex;
}
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
for (const [k, v] of Object.entries({ A: 60, B: 50, T: 8, Wd: 30, r: 4, hx: 40, d: 8 })) doc.setVariable(k, String(v));
doc.addFeature({
  type: 'Sketch',
  id: 'Profile',
  options: { plane: 'XZ' },
  sketch: modeling.sketchTemplate({
    profiles: [{ kind: 'polygon', points: [[0, 0], ['A', 0], ['A', 'T'], ['T', 'T'], ['T', 'B'], [0, 'B']] }],
  }),
});
doc.addFeature({ type: 'Extrude', id: 'Body', options: { extent: 'symmetric' }, parameters: { distance: 'Wd' }, links: { sketch: 'Profile' } });
rebuildOk(doc);
const MESH = { linearDeflection: 0.05, angularDeflection: 0.2 };
let mesh = doc.tessellate('final', MESH);
const inner = doc.makeReference('final', 'edge', edgeWhere(mesh, (x, y, z) => near(x, 8) && near(z, 8)));
doc.addFeature({ type: 'Fillet', id: 'InnerRound', parameters: { radius: 'r' }, references: { edges: [inner] } });
rebuildOk(doc);
mesh = doc.tessellate('final', MESH);
const top = doc.makeReference('final', 'face', faceWhere(mesh, (x, y, z) => near(z, 8)));
doc.addFeature({
  type: 'Sketch',
  id: 'HoleSketch',
  options: { plane: 'face' },
  parameters: { originX: 0, originY: 0, originZ: 'T' },
  references: { face: [top] },
  sketch: modeling.sketchTemplate({ profiles: [{ kind: 'point', x: 'hx', y: 0 }] }),
});
doc.addFeature({
  type: 'Hole',
  id: 'MountHole',
  parameters: { diameter: 'd' },
  options: { end: 'through' },
  references: { face: [top] },
  links: { sketch: 'HoleSketch' },
});
rebuildOk(doc);
const v = doc.properties('final').volume;
// Analytic volume: L-profile area × Wd + fillet (1 − π/4)·r²·Wd − hole π d²/4 · T.
const expected = (60 * 8 + 8 * 42) * 30 + (1 - Math.PI / 4) * 16 * 30 - (Math.PI * 64 / 4) * 8;
if (Math.abs(v - expected) / expected > 1e-6) throw new Error(`bracket volume ${v} vs ${expected}`);
fs.writeFileSync(path.join(here, 'bracket.part.json'), doc.save() + '\n');
doc.dispose();
console.log(`bracket.part.json written, volume ${v.toFixed(3)} mm³`);
