// Node check of bendcore_modeling_wasm: the part document API (docs/ui/PART_MODELING_UI.md).
//
// Three reference parts of docs/modeling/REFERENCE_PARTS.md (cover with rounded corners, bushing
// with chamfers, cast boss with draft) are built through the JS API the way the UI does it:
// sketches from templates, features with expression parameters, edge and face references made
// from element indices of the tessellated result. The edges and faces are chosen by the mesh
// geometry (polyline points, triangle vertices), not by names. Volumes are compared with the
// analytic formulas of REFERENCE_PARTS.md; then dimension changes, undo/redo, save/load and errors.
//
//   node tests/wasm/modeling_document.test.mjs --module <dir with bendcore_modeling.mjs>
//
// Registered in ctest as wasm_modeling_document (cmake/wasm_modeling.cmake).

import assert from 'node:assert/strict';
import path from 'node:path';
import { after, before, describe, test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../..');

function argument(name, fallback) {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 && i + 1 < process.argv.length ? path.resolve(process.argv[i + 1]) : fallback;
}

const moduleDir = argument('module', path.join(repo, 'build-wasm-modeling/wasm_modeling'));
const MESH = { linearDeflection: 0.05, angularDeflection: 0.2 };
const { PI } = Math;
const filletLoss = (r, L) => (1 - PI / 4) * r * r * L;
const ringChamfer = (d, R, side) => PI * d * d * (R - (side * d) / 3);

function relClose(actual, expected, rel, what) {
  const err = Math.abs(actual - expected) / Math.abs(expected);
  assert.ok(err <= rel, `${what}: ${actual} vs ${expected} (relative error ${err.toExponential(2)} > ${rel})`);
}

const near = (a, b, tol = 1e-3) => Math.abs(a - b) <= tol;

// Edge whose every polyline point satisfies pred(x, y, z); exactly one must match.
function edgeWhere(mesh, pred, what) {
  const found = mesh.edges.filter((e) => {
    for (let i = 0; i < e.points.length; i += 3) {
      if (!pred(e.points[i], e.points[i + 1], e.points[i + 2])) return false;
    }
    return true;
  });
  assert.equal(found.length, 1, `${what}: ${found.length} edges match`);
  return found[0].edgeIndex;
}

// Face whose every triangle vertex satisfies pred(x, y, z); exactly one must match.
function faceWhere(mesh, pred, what) {
  const found = [];
  for (let s = 0; s < mesh.faceSpans.length; s += 3) {
    const first = mesh.faceSpans[s + 1];
    const count = mesh.faceSpans[s + 2];
    let all = true;
    for (let k = first; k < first + count && all; ++k) {
      const v = mesh.indices[k] * 3;
      all = pred(mesh.positions[v], mesh.positions[v + 1], mesh.positions[v + 2]);
    }
    if (all) found.push(mesh.faceSpans[s]);
  }
  assert.equal(found.length, 1, `${what}: ${found.length} faces match`);
  return found[0];
}

let api;
let modeling;

before(async () => {
  const glue = await import(pathToFileURL(path.join(moduleDir, 'bendcore_modeling.mjs')).href);
  api = await import(pathToFileURL(path.join(moduleDir, 'bendcore_modeling_api.mjs')).href);
  modeling = await api.loadModeling(glue.default);
});

after(() => {
  assert.equal(modeling.info().liveDocuments, 0, 'every document created by the tests is disposed');
});

function setVariables(doc, values) {
  for (const [name, v] of Object.entries(values)) doc.setVariable(name, String(v));
}

function rebuildOk(doc) {
  const report = doc.rebuild();
  const bad = doc.features().filter((f) => f.state !== 'ok');
  assert.deepEqual(
    bad.map((f) => `${f.id}: ${f.state} ${f.messages.join('; ')}`),
    [],
    'all features ok',
  );
  return report;
}

const volume = (doc) => doc.properties('final').volume;

const parts = [
  {
    name: 'cover with rounded corners',
    base: { L: 100, W: 60, H: 12, r: 8, e: 10, dh: 6 },
    volume: (v) => v.L * v.W * v.H - 4 * filletLoss(v.r, v.H) - (4 * PI * v.dh * v.dh * v.H) / 4,
    build(doc, v) {
      doc.addFeature({
        type: 'Sketch',
        id: 'Outline',
        options: { plane: 'XY' },
        sketch: modeling.sketchTemplate({ profiles: [{ kind: 'rectangle', x: 0, y: 0, width: 'L', height: 'W' }] }),
      });
      doc.addFeature({ type: 'Extrude', id: 'Body', parameters: { distance: 'H' }, links: { sketch: 'Outline' } });
      rebuildOk(doc);
      let mesh = doc.tessellate('final', MESH);
      const corners = [
        [0, 0],
        [v.L, 0],
        [v.L, v.W],
        [0, v.W],
      ].map(([cx, cy]) =>
        doc.makeReference('final', 'edge', edgeWhere(mesh, (x, y) => near(x, cx) && near(y, cy), `corner ${cx},${cy}`)),
      );
      doc.addFeature({ type: 'Fillet', id: 'Corners', parameters: { radius: 'r' }, references: { edges: corners } });
      rebuildOk(doc);
      mesh = doc.tessellate('final', MESH);
      const top = doc.makeReference('final', 'face', faceWhere(mesh, (x, y, z) => near(z, v.H), 'top'));
      doc.addFeature({
        type: 'Sketch',
        id: 'HoleSketch',
        options: { plane: 'face' },
        parameters: { originX: 0, originY: 0, originZ: 'H' },
        references: { face: [top] },
        sketch: modeling.sketchTemplate({ profiles: [{ kind: 'point', x: 'e', y: 'e' }] }),
      });
      doc.addFeature({
        type: 'Hole',
        id: 'Screw',
        parameters: { diameter: 'dh' },
        options: { end: 'through' },
        references: { face: [top] },
        links: { sketch: 'HoleSketch' },
      });
      doc.addFeature({
        type: 'LinearPattern',
        id: 'ScrewGrid',
        parameters: { count1: 2, spacing1: 'L - 2*e', count2: 2, spacing2: 'W - 2*e' },
        options: { direction1: 'X', direction2: 'Y' },
        links: { feature: 'Screw' },
      });
    },
    scenarios: [{ L: 120 }, { W: 70 }, { H: 16 }, { r: 6 }, { e: 12 }, { dh: 5 }],
  },
  {
    name: 'bushing with chamfers',
    base: { R: 25, r: 15, h: 40, c1: 2, c2: 1.5 },
    volume: (v) => PI * (v.R * v.R - v.r * v.r) * v.h - ringChamfer(v.c1, v.R, 1) - ringChamfer(v.c2, v.r, -1),
    build(doc, v) {
      doc.addFeature({
        type: 'Sketch',
        id: 'Rings',
        options: { plane: 'XY' },
        sketch: modeling.sketchTemplate({
          profiles: [
            { kind: 'circle', cx: 0, cy: 0, radius: 'R' },
            { kind: 'circle', cx: 0, cy: 0, radius: 'r' },
          ],
        }),
      });
      doc.addFeature({ type: 'Extrude', id: 'Tube', parameters: { distance: 'h' }, links: { sketch: 'Rings' } });
      rebuildOk(doc);
      const mesh = doc.tessellate('final', MESH);
      const rim = (radius) =>
        doc.makeReference(
          'final',
          'edge',
          edgeWhere(mesh, (x, y, z) => near(z, v.h) && near(Math.hypot(x, y), radius, 1e-2), `rim ${radius}`),
        );
      doc.addFeature({ type: 'Chamfer', id: 'OuterChamfer', parameters: { distance: 'c1' }, references: { edges: [rim(v.R)] } });
      doc.addFeature({ type: 'Chamfer', id: 'InnerChamfer', parameters: { distance: 'c2' }, references: { edges: [rim(v.r)] } });
    },
    scenarios: [{ R: 30 }, { r: 12 }, { h: 50 }, { c1: 3 }, { c2: 2 }],
  },
  {
    name: 'cast boss with draft',
    base: { a: 40, h: 20, al: 5 },
    volume: (v) => {
      const a2 = v.a - 2 * v.h * Math.tan((v.al * PI) / 180);
      return (v.h / 3) * (v.a * v.a + a2 * a2 + v.a * a2);
    },
    build(doc, v) {
      doc.addFeature({
        type: 'Sketch',
        id: 'Outline',
        options: { plane: 'XY' },
        sketch: modeling.sketchTemplate({ profiles: [{ kind: 'rectangle', x: 0, y: 0, width: 'a', height: 'a' }] }),
      });
      doc.addFeature({ type: 'Extrude', id: 'Block', parameters: { distance: 'h' }, links: { sketch: 'Outline' } });
      rebuildOk(doc);
      const mesh = doc.tessellate('final', MESH);
      const face = (pred, what) => doc.makeReference('final', 'face', faceWhere(mesh, pred, what));
      const sides = [
        face((x) => near(x, 0), 'x = 0'),
        face((x) => near(x, v.a), 'x = a'),
        face((x, y) => near(y, 0), 'y = 0'),
        face((x, y) => near(y, v.a), 'y = a'),
      ];
      const bottom = face((x, y, z) => near(z, 0), 'bottom');
      doc.addFeature({
        type: 'Draft',
        id: 'Taper',
        parameters: { angle: 'al' },
        options: { neutral: 'face' },
        references: { faces: sides, neutralFace: [bottom] },
      });
    },
    scenarios: [{ a: 50 }, { h: 25 }, { al: 3 }, { al: 8 }],
  },
];

describe('reference parts through the WASM API', () => {
  for (const part of parts) {
    test(part.name, () => {
      const doc = modeling.createDocument();
      let loaded = null;
      try {
        setVariables(doc, part.base);
        part.build(doc, part.base);
        rebuildOk(doc);
        const baseVolume = volume(doc);
        relClose(baseVolume, part.volume(part.base), 1e-6, `${part.name}: volume`);

        // Dimension changes rebuild the model; every reference survives (all features ok).
        for (const change of part.scenarios) {
          const values = { ...part.base, ...change };
          setVariables(doc, change);
          rebuildOk(doc);
          relClose(volume(doc), part.volume(values), 1e-6, `${part.name} ${JSON.stringify(change)}`);
          setVariables(doc, part.base);
          rebuildOk(doc);
          assert.equal(volume(doc), baseVolume, 'return to the base values reproduces the volume exactly');
        }

        // Undo / redo of a transaction that changes two dimensions.
        const [k1, k2] = Object.keys(part.scenarios[0]).concat(Object.keys(part.scenarios[1]));
        const changed = { ...part.base, [k1]: part.base[k1] * 1.1, [k2]: part.base[k2] * 1.1 };
        doc.beginTransaction('resize');
        doc.setVariable(k1, String(changed[k1]));
        doc.setVariable(k2, String(changed[k2]));
        doc.commitTransaction();
        rebuildOk(doc);
        const changedVolume = volume(doc);
        relClose(changedVolume, part.volume(changed), 1e-6, `${part.name}: resized`);
        assert.equal(doc.state().undoHistory.at(-1).name, 'resize');
        const undo = doc.undo();
        assert.deepEqual(undo.recomputed, [], 'undo takes results from the pool');
        assert.equal(volume(doc), baseVolume, 'undo restores the volume exactly');
        assert.ok(doc.state().canRedo);
        doc.redo();
        assert.equal(volume(doc), changedVolume, 'redo restores the resized volume');
        doc.undo();

        // Save -> load: same feature states, references and volume; still parametric.
        const json = doc.save();
        assert.equal(JSON.parse(json).version, 3);
        ({ document: loaded } = modeling.loadDocument(json));
        assert.deepEqual(
          loaded.features().map((f) => [f.id, f.type, f.state, JSON.stringify(f.references)]),
          doc.features().map((f) => [f.id, f.type, f.state, JSON.stringify(f.references)]),
        );
        relClose(volume(loaded), baseVolume, 1e-9, `${part.name}: loaded volume`);
        setVariables(loaded, changed);
        rebuildOk(loaded);
        relClose(volume(loaded), part.volume(changed), 1e-6, `${part.name}: loaded and resized`);
        // With the BRep cache, the loaded results come from the cache.
        const cached = modeling.loadDocument(doc.save({ includeCache: true }));
        try {
          assert.ok(cached.report.fromCache.length > 0, 'bodies from the cache');
          relClose(volume(cached.document), baseVolume, 1e-9, 'cached volume');
        } finally {
          cached.document.dispose();
        }
      } finally {
        doc.dispose();
        loaded?.dispose();
      }
    });
  }
});

describe('tree editing', () => {
  function plate(doc) {
    setVariables(doc, { W: 40, D: 25, T: 10 });
    doc.addFeature({
      type: 'Sketch',
      id: 'Profile',
      options: { plane: 'XY' },
      sketch: modeling.sketchTemplate({ profiles: [{ kind: 'rectangle', x: 0, y: 0, width: 'W', height: 'D' }] }),
    });
    doc.addFeature({ type: 'Extrude', id: 'Body', parameters: { distance: 'T' }, links: { sketch: 'Profile' } });
    rebuildOk(doc);
  }

  test('fillet by a picked edge follows a sketch dimension edit; suppress, rollback, move, remove', () => {
    const doc = modeling.createDocument();
    try {
      plate(doc);
      const mesh = doc.tessellate('final', MESH);
      const edge = edgeWhere(mesh, (x, y) => near(x, 40) && near(y, 0), 'edge (40, 0)');
      const record = doc.element('final', 'edge', edge);
      assert.equal(record.name, 'G[Body](Profile:V1_2)', 'vertical edge named after the sketch vertex');
      doc.addFeature({
        type: 'Fillet',
        id: 'Round',
        parameters: { radius: 3 },
        references: { edges: [doc.makeReference('final', 'edge', edge)] },
      });
      rebuildOk(doc);
      relClose(volume(doc), 40 * 25 * 10 - filletLoss(3, 10), 1e-9, 'fillet');
      assert.equal(doc.feature('Round').referenceReports[0].method, 'exact');

      // Edit the sketch dimension itself (sketch data with a new width), as the UI does.
      const sketch = doc.feature('Profile').sketch;
      const width = sketch.constraints.findIndex((c) => c.type === 'distance' && c.value === 'W');
      sketch.constraints[width].value = '55';
      doc.editFeature('Profile', { sketch });
      rebuildOk(doc);
      relClose(volume(doc), 55 * 25 * 10 - filletLoss(3, 10), 1e-9, 'sketch width 55');
      assert.equal(doc.feature('Round').referenceReports[0].method, 'exact');
      assert.equal(doc.state().undoHistory.at(-1).name, 'Edit Profile');
      doc.undo();
      relClose(volume(doc), 40 * 25 * 10 - filletLoss(3, 10), 1e-9, 'undo sketch edit');

      doc.setSuppressed('Round', true);
      doc.rebuild();
      assert.equal(doc.feature('Round').state, 'suppressed');
      relClose(volume(doc), 40 * 25 * 10, 1e-9, 'suppressed fillet');
      doc.setSuppressed('Round', false);

      doc.setRollback(2);
      doc.rebuild();
      assert.equal(doc.feature('Round').state, 'rolled-back');
      assert.equal(doc.state().rollback, 2);
      relClose(volume(doc), 40 * 25 * 10, 1e-9, 'rolled back');
      doc.setRollback(null);
      rebuildOk(doc);

      // Moving the fillet above its body is an error state, not an exception.
      doc.moveFeature('Round', 0);
      doc.rebuild();
      assert.equal(doc.feature('Round').state, 'error');
      assert.ok(doc.feature('Round').messages.length > 0);
      doc.undo();
      rebuildOk(doc);
      doc.removeFeature('Round');
      doc.rebuild();
      relClose(volume(doc), 40 * 25 * 10, 1e-9, 'fillet removed');
      doc.undo();
      rebuildOk(doc);
      relClose(volume(doc), 40 * 25 * 10 - filletLoss(3, 10), 1e-9, 'removal undone');
    } finally {
      doc.dispose();
    }
  });

  test('preview inside a transaction, abort restores; mass from density', () => {
    const doc = modeling.createDocument();
    try {
      plate(doc);
      const undoDepth = doc.state().undoHistory.length;
      doc.beginTransaction('Add Cut');
      doc.addFeature({
        type: 'Sketch',
        id: 'Pocket',
        options: { plane: 'XY' },
        parameters: { offset: 'T' },
        sketch: modeling.sketchTemplate({ profiles: [{ kind: 'circle', cx: 20, cy: 12.5, radius: 5 }] }),
      });
      doc.addFeature({
        type: 'Extrude',
        id: 'Cut',
        parameters: { distance: 4 },
        options: { mode: 'cut', reversed: 'true' },
        links: { sketch: 'Pocket' },
      });
      doc.rebuild();
      assert.equal(doc.feature('Cut').state, 'ok', doc.feature('Cut').messages.join('; '));
      relClose(volume(doc), 40 * 25 * 10 - PI * 25 * 4, 1e-9, 'preview of the pocket');
      assert.ok(doc.state().inTransaction);
      doc.abortTransaction();
      doc.rebuild();
      assert.equal(doc.features().length, 2);
      assert.equal(doc.state().undoHistory.length, undoDepth);
      const p = doc.properties('final', 7850);
      relClose(p.mass, 40 * 25 * 10 * 1e-9 * 7850, 1e-12, 'mass, kg');
      assert.deepEqual(p.counts, { solids: 1, faces: 6, edges: 12, vertices: 8 });
      relClose(p.area, 2 * (40 * 25 + 40 * 10 + 25 * 10), 1e-9, 'area');
      const sketchMesh = doc.tessellate('sketch:Profile', MESH);
      assert.equal(sketchMesh.faceSpans.length / 3, 1, 'one sketch region');
    } finally {
      doc.dispose();
    }
  });

  test('errors become KernelError with code and message', () => {
    const doc = modeling.createDocument();
    try {
      const expectError = (fn, code, pattern) =>
        assert.throws(fn, (e) => e instanceof api.KernelError && e.code === code && pattern.test(e.detail));
      expectError(() => doc.addFeature({ type: 'Nope' }), 'Unsupported', /Nope/);
      expectError(() => doc.setVariable('A', '2 +'), 'InvalidInput', /./);
      expectError(() => doc.removeFeature('Missing'), 'InvalidInput', /Missing/);
      expectError(() => doc.undo(), 'InvalidInput', /nothing to undo/);
      expectError(() => doc.makeReference('final', 'edge', 0), 'InvalidInput', /out of range/);
      expectError(() => doc.tessellate('somewhere'), 'InvalidInput', /unknown target/);
      expectError(() => doc.addFeature({ type: 'Sketch', sketch: { points: 'x' } }), 'InvalidInput', /sketch.points/);
      expectError(() => modeling.loadDocument('{"format":"other"}'), 'ParseError', /./);
      expectError(() => modeling.sketchTemplate({ profiles: [{ kind: 'spline' }] }), 'InvalidInput', /spline/);
      // A sketch dimension referring to an unknown variable: the node is an error, not an exception.
      doc.addFeature({
        type: 'Sketch',
        id: 'S',
        options: { plane: 'XY' },
        sketch: modeling.sketchTemplate({ profiles: [{ kind: 'circle', cx: 0, cy: 0, radius: 'Q' }] }),
      });
      doc.rebuild();
      assert.equal(doc.feature('S').state, 'error');
      assert.match(doc.feature('S').messages.join(' '), /Q/);
      // The empty tree tessellates to an empty mesh.
      assert.equal(doc.tessellate('final').indices.length, 0);
    } finally {
      doc.dispose();
    }
    assert.throws(() => doc.features(), (e) => e.code === 'Disposed');
    const catalog = modeling.catalog();
    for (const type of ['Sketch', 'Extrude', 'Revolve', 'Hole', 'Fillet', 'Chamfer', 'LinearPattern', 'CircularPattern',
      'Mirror', 'Shell', 'Draft', 'DatumPlane', 'DatumAxis']) {
      assert.ok(catalog.some((c) => c.type === type), `catalog has ${type}`);
    }
    assert.ok(catalog.find((c) => c.type === 'Sketch').hasSketch);
  });
});
