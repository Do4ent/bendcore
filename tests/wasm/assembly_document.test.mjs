// Node check of bendcore_assembly_wasm: the assembly document API (docs/ui/ASSEMBLY_UI.md).
//
// Two reference assemblies built through the JS API the way the UI does it, with faces chosen
// by their geometry (element(): entity type, point, normal, radius), not by index:
//  1. a shaft Ø20 × 60 in the bore Ø20 of a housing 80 × 60 × 20 (tests/assembly-fixtures):
//     concentric + coincident → 1 DOF; position on the bore axis; drag rotates about the axis;
//     redundant and conflicting mates; touching, not interfering; linear pattern; BOM masses;
//  2. a plate 100 × 60 × 20 with two holes Ø11 and the stack bolt M10×40 ГОСТ 7798 + 2 washers
//     ГОСТ 11371 + nut ГОСТ 5915 in each hole: positions by the stack heights, 8 DOF, no
//     interference and 10 contacts, BOM quantities and washer mass by formula, exploded view,
//     subassembly levels, STEP and JSON round trips, undo/redo, errors.
//
//   node tests/wasm/assembly_document.test.mjs --module <dir with bendcore_assembly.mjs>
//
// Registered in ctest as wasm_assembly_document (cmake/wasm_assembly.cmake).

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { after, before, describe, test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../..');

function argument(name, fallback) {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 && i + 1 < process.argv.length ? path.resolve(process.argv[i + 1]) : fallback;
}

const moduleDir = argument('module', path.join(repo, 'build-wasm/wasm_assembly'));
const fixture = (name) => fs.readFileSync(path.join(repo, 'tests/assembly-fixtures', name), 'utf8');
const { PI } = Math;
const STEEL = 7850;

const near = (a, b, tol = 1e-6) => Math.abs(a - b) <= tol;
function close(actual, expected, tol, what) {
  assert.ok(Math.abs(actual - expected) <= tol, `${what}: ${actual} vs ${expected} (tolerance ${tol})`);
}
function relClose(actual, expected, rel, what) {
  const err = Math.abs(actual - expected) / Math.abs(expected);
  assert.ok(err <= rel, `${what}: ${actual} vs ${expected} (relative error ${err.toExponential(2)} > ${rel})`);
}
function vecClose(a, b, tol, what) {
  for (let i = 0; i < 3; ++i) close(a[i], b[i], tol, `${what}[${i}]`);
}

let api;
let assembly;

before(async () => {
  const glue = await import(pathToFileURL(path.join(moduleDir, 'bendcore_assembly.mjs')).href);
  api = await import(pathToFileURL(path.join(moduleDir, 'bendcore_assembly_api.mjs')).href);
  assembly = await api.loadAssembly(glue.default);
});

after(() => {
  assert.equal(assembly.info().liveDocuments, 0, 'every document created by the tests is disposed');
});

function withDoc(make, body) {
  const doc = make();
  try {
    return body(doc);
  } finally {
    doc.dispose();
  }
}

// Face of the part at `path` whose world entity satisfies pred; exactly one must match.
function faceWhere(doc, pathIds, pred, what) {
  const state = doc.state();
  let occ = null;
  let list = state.occurrences;
  for (const id of pathIds) {
    occ = list.find((o) => o.id === id);
    list = occ?.children ?? [];
  }
  const def = state.definitions.find((d) => d.id === occ.definition);
  const found = [];
  for (let i = 0; i < def.faces; ++i) {
    const e = doc.element({ path: pathIds, kind: 'face', index: i });
    if (e.entity && pred(e.entity)) found.push(i);
  }
  assert.equal(found.length, 1, `${what}: ${found.length} faces match`);
  return { path: pathIds, kind: 'face', index: found[0] };
}

const isCylinder = (r) => (e) => e.type === 'cylinder' && near(e.radius, r, 1e-6);
const isPlane = (z, nz) => (e) => e.type === 'plane' && near(e.point[2], z, 1e-6) && near(e.direction[2], nz, 1e-9);

function occurrence(doc, id) {
  const find = (list) => {
    for (const o of list) {
      if (o.id === id) return o;
      const c = find(o.children ?? []);
      if (c) return c;
    }
    return null;
  };
  return find(doc.state().occurrences);
}

// Angle of rotation about Z of a placement whose rotation axis is Z.
const zAngle = (p) => 2 * Math.atan2(p.rotation[2], p.rotation[3]);

describe('shaft in a bushing', () => {
  function build(doc) {
    const block = doc.insertPart({ name: 'Housing', format: 'part-json', data: fixture('block.json'), fixed: true });
    const shaft = doc.insertPart({
      name: 'Shaft',
      format: 'part-json',
      data: fixture('shaft.json'),
      placement: { position: [120, -40, 35], axis: [1, 1, 0], angle: 30 },
    });
    const bore = faceWhere(doc, [block.occurrence], isCylinder(10), 'bore Ø20');
    const bottom = faceWhere(doc, [block.occurrence], isPlane(0, -1), 'housing bottom');
    const shaftSide = faceWhere(doc, [shaft.occurrence], isCylinder(10), 'shaft side');
    // The shaft is tilted: find its end face of the part (z = 0 in part coordinates) by index
    // through its local geometry — the face whose world point is the shaft origin.
    const origin = occurrence(doc, shaft.occurrence).placement.position;
    const shaftEnd = faceWhere(
      doc,
      [shaft.occurrence],
      (e) => e.type === 'plane' && Math.hypot(e.point[0] - origin[0], e.point[1] - origin[1], e.point[2] - origin[2]) < 1e-6,
      'shaft end',
    );
    return { block, shaft, bore, bottom, shaftSide, shaftEnd };
  }

  test('concentric and coincident leave 1 DOF; the shaft sits on the bore axis', () =>
    withDoc(
      () => assembly.createDocument('Shaft unit'),
      (doc) => {
        const p = build(doc);
        assert.equal(occurrence(doc, p.shaft.occurrence).dof, 6, 'free shaft: 6 DOF');
        doc.addMate({ type: 'concentric', a: p.shaftSide, b: p.bore });
        assert.equal(occurrence(doc, p.shaft.occurrence).dof, 2, 'concentric: rotation and slide');
        doc.addMate({ type: 'coincident', a: p.shaftEnd, b: p.bottom, alignment: 'aligned' });
        const state = doc.state();
        assert.equal(state.solve.success, true);
        assert.equal(state.solve.totalDof, 1, 'one rotational DOF');
        const shaft = occurrence(doc, p.shaft.occurrence);
        assert.equal(shaft.dof, 1);
        vecClose(shaft.placement.position, [40, 30, 0], 1e-7, 'shaft origin on the bore axis at the bottom');
        // Axis of the shaft is world Z: the rotation maps local Z onto ±Z (aligned end faces → +Z).
        const side = doc.element(p.shaftSide).entity;
        close(Math.abs(side.direction[2]), 1, 1e-9, 'shaft axis along Z');
        assert.deepEqual(
          state.mates.map((m) => m.status),
          ['solved', 'solved'],
        );

        // Drag the rim point on +x round to +y: rotation by 90° about the axis, axis unchanged.
        const top = [shaft.placement.position[0], shaft.placement.position[1], 60];
        const rim = doc.element({ ...p.shaftSide }).entity;
        assert.ok(rim);
        // Grab a point on the top end at the shaft radius in the current world +x direction.
        const grab = [40 + 10, 30, 60];
        const before = zAngle(occurrence(doc, p.shaft.occurrence).placement);
        doc.dragBegin(p.shaft.occurrence, grab);
        let moved;
        for (let k = 1; k <= 6; ++k) {
          const t = ((PI / 2) * k) / 6;
          moved = doc.dragMove([40 + 10 * Math.cos(t), 30 + 10 * Math.sin(t), 60]);
        }
        assert.equal(moved.solve.success, true, 'mates hold during the drag');
        vecClose(moved.grabbed, [40, 40, 60], 1e-3, 'grabbed point follows the cursor on the circle');
        doc.dragEnd();
        const after = occurrence(doc, p.shaft.occurrence).placement;
        vecClose(after.position, [40, 30, 0], 1e-7, 'still on the axis after the drag');
        close(doc.element(p.shaftSide).entity.direction[2] ** 2, 1, 1e-9, 'axis still Z');
        let turned = zAngle(after) - before;
        turned = Math.atan2(Math.sin(turned), Math.cos(turned));
        close(Math.abs(turned), PI / 2, 1e-3, 'turned by 90°');
        assert.equal(doc.state().history.undoLabel, 'Drag component');
        doc.undo();
        close(zAngle(occurrence(doc, p.shaft.occurrence).placement), before, 1e-12, 'undo restores the angle');
        doc.redo();
        void top;

        // Touching, not interfering: Ø20 shaft in Ø20 bore.
        const report = doc.interference();
        assert.equal(report.totalPairs, 1);
        assert.equal(report.pairs.filter((x) => x.kind === 'interference').length, 0, 'no interference');
        assert.equal(report.pairs.length, 1, 'one contact');
        assert.equal(report.pairs[0].kind, 'touching');

        // BOM: masses of the housing and the shaft by formula.
        const housing = state.definitions.find((d) => d.name === 'Housing');
        const shaftDef = state.definitions.find((d) => d.name === 'Shaft');
        doc.setProperties(housing.id, { density: STEEL, material: 'Steel', designation: 'H-01' });
        doc.setProperties(shaftDef.id, { density: STEEL, material: 'Steel', designation: 'S-01' });
        const bom = doc.bom('flat');
        const vHousing = 80 * 60 * 20 - PI * (10 * 10 + 2 * 5.5 * 5.5) * 20;
        const vShaft = PI * 10 * 10 * 60;
        assert.deepEqual(
          bom.rows.map((r) => [r.designation, r.quantity]),
          [
            ['H-01', 1],
            ['S-01', 1],
          ],
        );
        relClose(bom.rows[0].unitVolume, vHousing, 1e-6, 'housing volume');
        relClose(bom.rows[0].unitMass, vHousing * 1e-9 * STEEL, 1e-6, 'housing mass');
        relClose(bom.rows[1].unitMass, vShaft * 1e-9 * STEEL, 1e-6, 'shaft mass');
        relClose(bom.totalMass, (vHousing + vShaft) * 1e-9 * STEEL, 1e-6, 'total mass');
        assert.equal(bom.totalParts, 2);
      },
    ));

  test('redundant and conflicting mates are reported; undo removes them', () =>
    withDoc(
      () => assembly.createDocument(),
      (doc) => {
        const p = build(doc);
        doc.addMate({ type: 'concentric', a: p.shaftSide, b: p.bore });
        doc.addMate({ type: 'coincident', a: p.shaftEnd, b: p.bottom, alignment: 'aligned' });
        const second = doc.addMate({ type: 'parallel', a: p.shaftSide, b: p.bore });
        let state = doc.state();
        assert.equal(state.mates.find((m) => m.id === second).status, 'redundant', 'parallel adds nothing to concentric');
        assert.equal(state.solve.totalDof, 1);
        // A circular edge of the shaft gives its axis: concentric with the bore is redundant too.
        const shaftDef = doc.state().definitions.find((d) => d.name === 'Shaft');
        const axes = [];
        for (let i = 0; i < shaftDef.edges; ++i) {
          const e = doc.element({ path: [p.shaft.occurrence], kind: 'edge', index: i });
          if (e.geometry === 'Circle') axes.push({ i, e });
        }
        assert.equal(axes.length, 2, 'two circular edges');
        axes.forEach(({ e }) => {
          assert.equal(e.entity.type, 'axis');
          close(Math.hypot(e.entity.point[0] - 40, e.entity.point[1] - 30), 0, 1e-7, 'edge axis through the centre');
        });
        const viaEdge = doc.addMate({ type: 'concentric', a: { path: [p.shaft.occurrence], kind: 'edge', index: axes[0].i }, b: p.bore });
        assert.equal(doc.state().mates.find((m) => m.id === viaEdge).status, 'redundant');
        doc.removeMate(viaEdge);
        const conflict = doc.addMate({ type: 'distance', a: p.shaftEnd, b: p.bottom, value: 10 });
        state = doc.state();
        assert.equal(state.solve.success, false);
        const sub = state.solve.subsystems.find((s) => s.mates.includes(conflict));
        assert.equal(sub.status, 'Conflicting');
        assert.ok(sub.conflictingMates.length > 0, 'conflicting mates listed');
        doc.setMateSuppressed(conflict, true);
        state = doc.state();
        assert.equal(state.solve.success, true, 'suppressing the conflict solves the rest');
        assert.equal(state.mates.find((m) => m.id === conflict).status, 'suppressed');
        vecClose(occurrence(doc, p.shaft.occurrence).placement.position, [40, 30, 0], 1e-7, 'back on the axis');
        doc.undo();
        doc.undo();
        assert.equal(doc.state().mates.length, 3, 'undo removed the distance mate');

        // Distance instead of coincidence: the shaft end 5 mm below the housing bottom plane.
        doc.removeMate(second);
        const coincident = doc.state().mates.find((m) => m.type === 'coincident').id;
        doc.editMate(coincident, { type: 'distance', a: p.shaftEnd, b: p.bottom, value: 5, alignment: 'aligned' });
        const z = occurrence(doc, p.shaft.occurrence).placement.position[2];
        close(Math.abs(z), 5, 1e-7, 'distance 5 mm from the bottom plane');
        close(z, -5, 1e-7, 'on the side of the bottom face normal (−Z)');

        // Linear pattern of the shaft along X: instances follow the seed.
        const pattern = doc.addPattern({ kind: 'linear', seed: p.shaft.occurrence, direction: [1, 0, 0], spacing: 100, count: 3 });
        const info = doc.state().patterns.find((x) => x.id === pattern);
        assert.equal(info.instances.length, 2);
        vecClose(occurrence(doc, info.instances[1]).placement.position, [240, 30, -5], 1e-7, 'second instance');
        assert.equal(doc.bom('flat').rows.find((r) => r.name === 'Shaft').quantity, 3);

        // Errors leave the document unchanged.
        const depth = doc.state().history.depth;
        assert.throws(() => doc.addMate({ type: 'concentric', a: p.shaftEnd, b: p.bore }), (e) => e.code === 'InvalidInput' || e.code === 'Unsupported');
        assert.throws(() => doc.addMate({ type: 'warp', a: p.shaftEnd, b: p.bore }), (e) => e.code === 'InvalidInput');
        assert.throws(() => doc.dragBegin(p.block.occurrence, [0, 0, 0]), (e) => e.code === 'InvalidInput', 'fixed part cannot be dragged');
        assert.throws(() => doc.insertPart({ format: 'part-json', data: '{' }), (e) => e.code === 'ParseError');
        assert.equal(doc.state().history.depth, depth);

        // A component inserted "beside": 20 mm to the +X of the assembly bounds, bottoms level.
        // Shafts span x up to 240 + 10 and z from −5; the housing x ∈ [0, 80].
        const spare = doc.insertPart({ name: 'Spare', format: 'part-json', data: fixture('shaft.json'), beside: true });
        const spareOrigin = doc.placements().find((x) => x.path[0] === spare.occurrence).world.position;
        close(spareOrigin[0] - 10, 250 + 20, 1e-6, 'gap of 20 mm along +X');
        close(spareOrigin[2], -5, 1e-6, 'bottom level with the lowest part');
        close(spareOrigin[1], 30, 1e-6, 'Y centres aligned');
      },
    ));
});

describe('plate with bolts', () => {
  const stack = {
    screw: { standard: 'gost-7798', size: 10, length: 40 },
    headWashers: [{ standard: 'gost-11371', size: 10 }],
    nutWashers: [{ standard: 'gost-11371', size: 10 }],
    nut: { standard: 'gost-5915', size: 10 },
  };

  function build(doc) {
    const plate = doc.insertPart({
      name: 'Plate',
      format: 'part-json',
      data: fixture('plate.json'),
      fixed: true,
      properties: { designation: 'P-01', density: STEEL, material: 'Steel' },
    });
    const placed = doc.insertFasteners({ part: [plate.occurrence], insertion: [0, 0, -1], radius: 5.5, ...stack });
    return { plate, placed };
  }

  test('stacks stand in both holes; 8 DOF; contacts without interference; BOM', () =>
    withDoc(
      () => assembly.createDocument('Plate'),
      (doc) => {
        const holes = (() => {
          const plate = doc.insertPart({ name: 'Probe', format: 'part-json', data: fixture('plate.json'), fixed: true });
          const sites = doc.findHoles({ part: [plate.occurrence], insertion: [0, 0, -1], radius: 5.5 });
          doc.undo();
          return sites;
        })();
        assert.equal(holes.length, 2);
        holes.forEach((h) => close(h.depth, 20, 1e-9, 'hole depth'));

        const { placed } = build(doc);
        assert.equal(placed.length, 2);
        const centres = [
          [25, 30],
          [75, 30],
        ];
        // Stack heights: washer h = 2, plate 20, nut m = 8: grip 32, bolt l = 40 → protrusion 8.
        placed.forEach((s, i) => {
          close(s.grip, 32, 1e-9, 'grip');
          close(s.protrusion, 8, 1e-9, 'protrusion');
          const z = (id) => occurrence(doc, id).placement.position;
          const [cx, cy] = centres[i];
          vecClose(z(s.screw), [cx, cy, 22], 1e-7, 'bolt bearing on the head washer');
          vecClose(z(s.headWashers[0]), [cx, cy, 20], 1e-7, 'head washer on the plate');
          vecClose(z(s.nutWashers[0]), [cx, cy, 0], 1e-7, 'nut washer under the plate');
          vecClose(z(s.nut), [cx, cy, -2], 1e-7, 'nut under its washer');
        });
        const state = doc.state();
        assert.equal(state.solve.success, true);
        assert.equal(state.solve.totalDof, 8, 'each of 8 fasteners turns about its axis');
        // Mates chain head washer → bolt and nut washer → nut: subsystems of two parts with two
        // rotations each, four per assembly.
        const fasteners = state.occurrences.filter((o) => o.name !== 'Plate');
        assert.ok(fasteners.every((o) => o.dof === 2 && o.dofShared), 'pair subsystems with 2 DOF');
        assert.equal(state.solve.subsystems.filter((s) => s.occurrences.length === 2 && s.dof === 2).length, 4);

        const report = doc.interference();
        assert.equal(report.parts.length, 9);
        assert.equal(report.pairs.filter((p) => p.kind === 'interference').length, 0, 'no interference');
        // Per stack: head–washer, washer–plate, plate–washer, washer–nut, shank–nut bore.
        assert.equal(report.pairs.filter((p) => p.kind === 'touching').length, 10);

        const bom = doc.bom('flat');
        const byName = Object.fromEntries(bom.rows.map((r) => [r.name, r]));
        const bolt = assembly.describeFastener(stack.screw);
        const washer = assembly.describeFastener(stack.headWashers[0]);
        const nut = assembly.describeFastener(stack.nut);
        assert.equal(byName.Plate.quantity, 1);
        assert.equal(byName[bolt.designation].quantity, 2);
        assert.equal(byName[washer.designation].quantity, 4);
        assert.equal(byName[nut.designation].quantity, 2);
        assert.equal(bom.totalParts, 9);
        // Washer ГОСТ 11371 М10: d1 = 10.5, d2 = 21, h = 2 — a plain ring.
        const vWasher = (PI / 4) * (21 * 21 - 10.5 * 10.5) * 2;
        relClose(byName[washer.designation].unitVolume, vWasher, 1e-6, 'washer volume');
        relClose(byName[washer.designation].unitMass, vWasher * 1e-9 * STEEL, 1e-6, 'washer mass');
        relClose(byName[washer.designation].totalMass, 4 * vWasher * 1e-9 * STEEL, 1e-6, 'washer total mass');
        // Plate: 100 × 60 × 20 minus two holes Ø11.
        const vPlate = 100 * 60 * 20 - 2 * PI * 5.5 * 5.5 * 20;
        relClose(byName.Plate.unitMass, vPlate * 1e-9 * STEEL, 1e-6, 'plate mass');
        // Bolt: between the bare shank + hexagon prism (s = 17, k = 7) minus chamfers, within 6 %.
        const vBolt = PI * 25 * 40 + ((3 * Math.sqrt(3)) / 2) * (17 / Math.sqrt(3)) ** 2 * 7;
        relClose(byName[bolt.designation].unitVolume, vBolt, 0.06, 'bolt volume near shank + prism');
        relClose(
          bom.totalMass,
          (vPlate + 4 * vWasher + 2 * byName[bolt.designation].unitVolume + 2 * byName[nut.designation].unitVolume) * 1e-9 * STEEL,
          1e-9,
          'total mass is the sum of the rows',
        );

        // Exploded view: bolts up by 50, nuts down by 30 (two steps).
        const bolts = placed.map((s) => s.screw);
        const nuts = placed.map((s) => s.nut);
        const view = doc.addExplodedView({
          name: 'Bolts out',
          steps: [
            { occurrences: bolts, direction: [0, 0, 1], distance: 50 },
            { occurrences: nuts, direction: [0, 0, -1], distance: 30 },
          ],
        });
        const zOf = (list, id) => list.find((x) => x.path[0] === id).world.position[2];
        let pl = doc.placements({ view, progress: 0.5 });
        close(zOf(pl, bolts[0]), 22 + 25, 1e-9, 'half of step 1');
        close(zOf(pl, nuts[0]), -2, 1e-9, 'step 2 not started');
        pl = doc.placements({ view, progress: 2 });
        close(zOf(pl, bolts[0]), 72, 1e-9, 'step 1 done');
        close(zOf(pl, nuts[0]), -32, 1e-9, 'step 2 done');
        close(occurrence(doc, bolts[0]).placement.position[2], 22, 1e-12, 'the document keeps the assembled position');

        // Tessellation: one mesh per definition, one placed occurrence per part, face spans valid.
        const mesh = doc.tessellate();
        assert.equal(mesh.occurrences.length, 9);
        assert.equal(Object.keys(mesh.meshes).length, 4, 'plate, bolt, washer, nut');
        for (const m of Object.values(mesh.meshes)) {
          assert.equal(m.indices.length % 3, 0);
          let covered = 0;
          for (let s = 0; s < m.faceSpans.length; s += 3) covered += m.faceSpans[s + 2];
          assert.equal(covered, m.indices.length, 'face spans cover all triangles');
        }
        const again = doc.tessellate({ skip: Object.keys(mesh.meshes) });
        assert.equal(Object.keys(again.meshes).length, 0, 'known meshes are not sent again');
      },
    ));

  test('subassembly levels, STEP and JSON round trips', () => {
    const subText = withDoc(
      () => assembly.createDocument('Bolted plate'),
      (doc) => {
        build(doc);
        return doc.save();
      },
    );
    assembly.registerSource('bolted_plate.json', subText);
    withDoc(
      () => assembly.createDocument('Frame'),
      (doc) => {
        const a = doc.insertSubassembly({ source: 'bolted_plate.json', fixed: true });
        doc.insertOccurrence({ definition: a.definition, placement: { position: [0, 100, 0] }, name: 'Plate 2' });
        const flat = doc.bom('flat');
        const bolt = assembly.describeFastener(stack.screw);
        assert.equal(flat.rows.find((r) => r.name === bolt.designation).quantity, 4);
        assert.equal(flat.totalParts, 18);
        const levels = doc.bom('structured');
        assert.equal(levels.rows.length, 1);
        assert.equal(levels.rows[0].kind, 'assembly');
        assert.equal(levels.rows[0].quantity, 2);
        assert.equal(levels.rows[0].children.find((r) => r.name === bolt.designation).quantity, 2);
        assert.equal(levels.rows[0].children[0].positionPath, '1.1');
        relClose(levels.totalMass, flat.totalMass, 1e-12, 'same total mass');

        // JSON round trip: the subassembly is read again from the registered source.
        const saved = doc.save();
        const { document: copy, report } = assembly.loadDocument(saved);
        try {
          assert.ok(report.sources.some((s) => s.status === 'AssemblyLoaded'));
          assert.equal(copy.placements().length, 18);
          relClose(copy.bom('flat').totalMass, flat.totalMass, 1e-12, 'mass after reload');
        } finally {
          copy.dispose();
        }

        // STEP round trip: structure and placements.
        const step = doc.exportStep();
        assert.match(step, /^ISO-10303-21;/);
        const imported = assembly.importStep(step);
        try {
          const a1 = doc.placements().map((p) => p.world.position).sort((x, y) => x[0] - y[0] || x[1] - y[1] || x[2] - y[2]);
          const a2 = imported.placements().map((p) => p.world.position).sort((x, y) => x[0] - y[0] || x[1] - y[1] || x[2] - y[2]);
          assert.equal(a2.length, a1.length);
          a1.forEach((p, i) => vecClose(a2[i], p, 1e-6, `placement ${i}`));
          assert.equal(imported.bom('flat').totalParts, 18);
        } finally {
          imported.dispose();
        }
      },
    );
  });

  test('errors and the fastener catalog', () =>
    withDoc(
      () => assembly.createDocument(),
      (doc) => {
        const catalog = assembly.fastenerCatalog();
        const iso4017 = catalog.find((c) => c.id === 'iso-4017');
        assert.equal(iso4017.kind, 'hex-bolt');
        assert.ok(iso4017.sizes.includes(10));
        assert.equal(catalog.find((c) => c.id === 'gost-7798').counterpart, 'iso-4014');
        const plate = doc.insertPart({ name: 'Plate', format: 'part-json', data: fixture('plate.json'), fixed: true });
        const depth = doc.state().history.depth;
        assert.throws(
          () => doc.insertFasteners({ part: [plate.occurrence], insertion: [0, 0, -1], radius: 5.5, ...stack, screw: { ...stack.screw, length: 25 } }),
          (e) => e.code === 'InvalidInput',
          'short bolt',
        );
        assert.throws(
          () => doc.insertFasteners({ part: [plate.occurrence], insertion: [0, 0, -1], radius: 7, ...stack }),
          (e) => e.code === 'InvalidInput',
          'no hole of that radius',
        );
        assert.throws(() => doc.insertFasteners({ ...stack, part: [plate.occurrence], insertion: [0, 0, -1], radius: 5.5, screw: { standard: 'din-1', size: 10 } }), (e) => e.code === 'InvalidInput');
        assert.equal(doc.state().history.depth, depth, 'failed insertions are not recorded');
        assert.equal(doc.state().occurrences.length, 1);
        // Only the selected hole.
        const sites = doc.findHoles({ part: [plate.occurrence], insertion: [0, 0, -1], radius: 5.5 });
        doc.insertFasteners({ part: [plate.occurrence], insertion: [0, 0, -1], radius: 5.5, holes: [sites[1].hole.index], ...stack });
        assert.equal(doc.state().occurrences.length, 5);
        doc.undo();
        assert.equal(doc.state().occurrences.length, 1);
        doc.undo();
        assert.equal(doc.state().occurrences.length, 0);
        assert.throws(() => doc.undo(), (e) => e.code === 'InvalidInput', 'nothing to undo');
        doc.redo();
        doc.redo();
        assert.equal(doc.state().occurrences.length, 5);
        const disposed = assembly.createDocument();
        disposed.dispose();
        assert.throws(() => disposed.state(), (e) => e.code === 'Disposed');
      },
    ));
});
