// Shaft unit for the assembly drawing tests (docs/ui/DRAWING_UI.md, "Позиции"): made with the
// assembly API of bendcore_assembly_wasm, the way the assembly editor makes it.
//
//   Втулка (block.json, 80 × 60 × 20, bore Ø20 at (40, 30), holes Ø11 at (15, 30), (65, 30)) — fixed;
//   Вал (shaft.json, Ø20 × 60) — concentric with the bore, its end on the bottom of the bushing;
//   in both holes Ø11: bolt ГОСТ 7798 M10 × 40, washers ГОСТ 11371 under the head and the nut,
//   nut ГОСТ 5915 M10.
//
// Specification by construction: two parts (bushing, shaft) and three kinds of standard items
// (bolt × 2, washer × 4, nut × 2): five positions.
//
//   import { buildShaftUnit } from './shaft_unit.mjs';
//   const json = buildShaftUnit(assemblyKernel);           // bendcore-assembly JSON
//
//   node tests/assembly-fixtures/shaft_unit.mjs --module <dir with bendcore_assembly.mjs>   (prints the JSON)
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const fixture = (name) => fs.readFileSync(path.join(here, name), 'utf8');

const near = (a, b, tol) => Math.abs(a - b) <= tol;
const isCylinder = (r) => (e) => e.type === 'cylinder' && near(e.radius, r, 1e-6);
const isPlane = (z, nz) => (e) => e.type === 'plane' && near(e.point[2], z, 1e-6) && near(e.direction[2], nz, 1e-9);

// The one face of the occurrence `id` (top level) whose world entity satisfies pred.
function faceWhere(doc, id, pred, what) {
  const state = doc.state();
  const occ = state.occurrences.find((o) => o.id === id);
  const def = state.definitions.find((d) => d.id === occ.definition);
  const found = [];
  for (let i = 0; i < def.faces; ++i) {
    const e = doc.element({ path: [id], kind: 'face', index: i });
    if (e.entity && pred(e.entity)) found.push(i);
  }
  if (found.length !== 1) throw new Error(`${what}: ${found.length} faces match`);
  return { path: [id], kind: 'face', index: found[0] };
}

export function buildShaftUnit(assembly) {
  const doc = assembly.createDocument('Узел вала');
  try {
    const STEEL = 7850;
    const bushing = doc.insertPart({
      name: 'Втулка',
      format: 'part-json',
      data: fixture('block.json'),
      fixed: true,
      properties: { designation: 'УВ.01.001', density: STEEL, material: 'Сталь 45' },
    });
    const shaft = doc.insertPart({
      name: 'Вал',
      format: 'part-json',
      data: fixture('shaft.json'),
      placement: { position: [120, -40, 35] },
      properties: { designation: 'УВ.01.002', density: STEEL, material: 'Сталь 45' },
    });
    const bore = faceWhere(doc, bushing.occurrence, isCylinder(10), 'bore Ø20');
    const bottom = faceWhere(doc, bushing.occurrence, isPlane(0, -1), 'bushing bottom');
    const side = faceWhere(doc, shaft.occurrence, isCylinder(10), 'shaft side');
    // The end of the shaft at its origin (z = 0 of the part, world z = 35).
    const end = faceWhere(doc, shaft.occurrence, isPlane(35, -1), 'shaft end');
    doc.addMate({ type: 'concentric', a: side, b: bore });
    doc.addMate({ type: 'coincident', a: end, b: bottom, alignment: 'aligned' });
    const placed = doc.insertFasteners({
      part: [bushing.occurrence],
      insertion: [0, 0, -1],
      radius: 5.5,
      screw: { standard: 'gost-7798', size: 10, length: 40 },
      headWashers: [{ standard: 'gost-11371', size: 10 }],
      nutWashers: [{ standard: 'gost-11371', size: 10 }],
      nut: { standard: 'gost-5915', size: 10 },
    });
    if (placed.length !== 2) throw new Error(`fasteners in ${placed.length} holes, expected 2`);
    const state = doc.state();
    if (!state.solve.success) throw new Error('the shaft unit does not solve');
    // The assembly drawing entry of the specification needs the designation of the assembly.
    const root = state.definitions.find((d) => d.kind === 'assembly');
    if (!root) throw new Error('no root assembly definition in the state');
    doc.setProperties(root.id, { designation: 'УВ.01.000', name: 'Узел вала' });
    return doc.save({ embedGeometry: true });
  } finally {
    doc.dispose();
  }
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const i = process.argv.indexOf('--module');
  const dir = path.resolve(i >= 0 ? process.argv[i + 1] : path.join(here, '../../build-wasm-all/wasm_assembly'));
  const glue = await import(pathToFileURL(path.join(dir, 'bendcore_assembly.mjs')).href);
  const api = await import(pathToFileURL(path.join(dir, 'bendcore_assembly_api.mjs')).href);
  process.stdout.write(buildShaftUnit(await api.loadAssembly(glue.default)));
}
