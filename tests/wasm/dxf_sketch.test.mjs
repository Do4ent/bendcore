// Node check of the DXF import into a sketch (ModelingKernel.dxfSketch, docs/ui/SKETCH_EDITOR.md,
// "Импорт DXF") on files written by other programs:
//   - tests/dxf/sketch/{plate_inch,slot_mm,outline_r12}.dxf written by ezdxf
//     (make_sketch_fixtures.py): inches, separate LINE entities with gaps below the tolerance,
//     LWPOLYLINE stadiums with bulges in both directions, LINE + ARC rounded rectangle, R12
//     POLYLINE with a bulge and a Cyrillic layer name in ANSI_1251, layers (frozen, paper space),
//     SPLINE/ELLIPSE reported as skipped, missing $INSUNITS;
//   - tests/dxf/sketch/plate_drawing.dxf written by the bendcore_drawing_v2 DXF writer
//     (make_drawing_fixture.mjs): the top view of a plate with two holes on its sheet.
// Every imported profile is extruded in a part document; the region area and the volume are
// compared with the formulas of the drawn shapes.
//
//   node tests/wasm/dxf_sketch.test.mjs --module <dir with bendcore_modeling.mjs>
//
// Registered in ctest as wasm_dxf_sketch (cmake/wasm_modeling.cmake).

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { after, before, describe, test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../..');
const fixtures = path.join(repo, 'tests/dxf/sketch');

function argument(name, fallback) {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 && i + 1 < process.argv.length ? path.resolve(process.argv[i + 1]) : fallback;
}

const moduleDir = argument('module', path.join(repo, 'build-wasm-modeling/wasm_modeling'));
const { PI } = Math;
const IN = 25.4;

const close = (a, b, tol, what) => assert.ok(Math.abs(a - b) <= tol, `${what}: ${a} vs ${b}`);
const bytes = (name) => new Uint8Array(fs.readFileSync(path.join(fixtures, name)));

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

function expectKernelError(fn, code, pattern) {
  assert.throws(fn, (e) => e instanceof api.KernelError && e.code === code && pattern.test(e.detail), `${code} ${pattern}`);
}

/** Extrudes the sketch by `depth`; returns { area of the sketch faces, volume, regions, state }. */
function extrude(sketch, depth = 10) {
  const doc = modeling.createDocument();
  try {
    doc.addFeature({ type: 'Sketch', id: 'S', options: { plane: 'XY' }, sketch });
    doc.addFeature({ type: 'Extrude', id: 'E', parameters: { distance: depth }, links: { sketch: 'S' } });
    doc.rebuild();
    const states = doc.features().map((f) => `${f.id}:${f.state}:${f.messages.join('|')}`);
    const solution = doc.solveSketch('S', sketch);
    return {
      states,
      area: doc.properties('sketch:S').area,
      volume: doc.properties('final').volume,
      solids: doc.properties('final').counts.solids,
      regions: solution.profile.regions,
      issues: solution.profile.issues,
    };
  } finally {
    doc.dispose();
  }
}

const layer = (r, name) => r.layers.find((l) => l.name === name);

describe('ezdxf files', () => {
  test('plate in inches: separate lines merged within the tolerance, layers, units', () => {
    const r = modeling.dxfSketch(bytes('plate_inch.dxf'));
    assert.equal(r.fileUnits, 1);
    assert.equal(r.units, 1);
    assert.equal(r.mmPerUnit, IN);
    // Default layers: every layer neither frozen nor off.
    assert.equal(layer(r, 'FROZEN').frozen, true);
    assert.equal(layer(r, 'FROZEN').selected, false);
    for (const n of ['CONTOUR', 'HOLES', 'DIMS']) assert.equal(layer(r, n).selected, true, n);
    assert.equal(layer(r, 'CONTOUR').curves, 4, 'paper space line not counted');
    assert.equal(r.ignored.paperSpace, 1);
    assert.equal(r.ignored.texts, 1);
    assert.equal(r.ignored.otherLayers, 1, 'the frozen line');

    const only = modeling.dxfSketch(bytes('plate_inch.dxf'), { layers: ['contour', 'HOLES'] });
    assert.deepEqual(only.layers.filter((l) => l.selected).map((l) => l.name).sort(), ['CONTOUR', 'HOLES']);
    assert.equal(only.imported.lines, 4);
    assert.equal(only.imported.circles, 1);
    assert.equal(only.entities.length, 5);
    // Four corners (each end point within 2e-5 in = 0.000508 mm of its neighbour) + the centre.
    assert.equal(only.sketch.points.length, 5);
    assert.equal(only.points, 5);
    assert.equal(only.merged, 4);
    assert.equal(only.openEnds.length, 0, 'closed contour');
    assert.deepEqual(only.bounds.min.map((v) => Number(v.toFixed(6))), [0, 0]);
    close(only.bounds.max[0], 4 * IN, 1e-3, 'width');
    close(only.bounds.max[1], 2 * IN, 1e-3, 'height');
    const corners = only.sketch.points.map((p) => [Number(p.x), Number(p.y)]);
    for (const [x, y] of [[0, 0], [4 * IN, 0], [4 * IN, 2 * IN], [0, 2 * IN]]) {
      assert.ok(corners.some(([px, py]) => Math.hypot(px - x, py - y) < 1e-3), `corner (${x}, ${y})`);
    }
    const e = extrude(only.sketch, 10);
    assert.ok(e.states.every((s) => /:ok:/.test(s)), e.states.join(', '));
    // A merged point keeps the position of the first end point, so the outline moves by at most
    // the gap g = 2e-5 in: |area error| <= perimeter * g.
    const area = (8 - PI * 0.25 * 0.25) * IN * IN;
    const bound = 12 * IN * 2e-5 * IN;
    close(e.area, area, bound, 'profile area');
    close(e.volume, area * 10, bound * 10, 'volume');

    // A tolerance below the gaps leaves the contour open: only the exactly shared corner (0, 2)
    // is merged, the other three corners are six open ends.
    const tight = modeling.dxfSketch(bytes('plate_inch.dxf'), { layers: ['CONTOUR'], tolerance: 1e-4 });
    assert.equal(tight.merged, 1);
    assert.equal(tight.openEnds.length, 6);

    // DIMS: a line outside the contour is an open chain of its own.
    const dims = modeling.dxfSketch(bytes('plate_inch.dxf'), { layers: ['DIMS'] });
    assert.equal(dims.imported.lines, 1);
    assert.equal(dims.openEnds.length, 2);

    // Units chosen by the user override the file: millimetres.
    const mm = modeling.dxfSketch(bytes('plate_inch.dxf'), { layers: ['CONTOUR'], units: 4 });
    close(mm.bounds.max[0], 4, 1e-12, 'width in mm units (bounds of the merged points)');
    // Paper space on request.
    const paper = modeling.dxfSketch(bytes('plate_inch.dxf'), { layers: ['CONTOUR'], paperSpace: true });
    assert.equal(paper.imported.lines, 5);
    // Fixed points.
    const fixed = modeling.dxfSketch(bytes('plate_inch.dxf'), { layers: ['CONTOUR'], fixed: true });
    assert.ok(fixed.sketch.points.every((p) => p.fixed));
  });

  test('stadiums from LWPOLYLINE bulges (both directions), rounded rectangle from LINE + ARC, SPLINE skipped', () => {
    const r = modeling.dxfSketch(bytes('slot_mm.dxf'));
    assert.equal(r.fileUnits, 4);
    assert.equal(r.skipped.SPLINE, 1);
    assert.equal(r.skipped.ELLIPSE, 1);
    assert.equal(layer(r, 'CURVES').curves, 0);
    const slot = 50 * 20 + PI * 10 * 10;
    for (const name of ['SLOT', 'SLOT_CW']) {
      const s = modeling.dxfSketch(bytes('slot_mm.dxf'), { layers: [name] });
      assert.equal(s.imported.polylines, 1);
      assert.equal(s.imported.polylineSegments, 4);
      assert.equal(s.imported.lines, 0, 'polyline segments are not counted as LINE entities');
      assert.equal(s.sketch.geometry.filter((g) => g.type === 'arc').length, 2, name);
      assert.ok(s.sketch.geometry.every((g) => g.type !== 'arc' || g.counterClockwise));
      assert.equal(s.openEnds.length, 0);
      // Bounds include the extremes of the semicircles.
      close(s.bounds.min[0], -10, 1e-9, `${name} min x`);
      close(s.bounds.max[0], 60, 1e-9, `${name} max x`);
      const e = extrude(s.sketch, 5);
      assert.ok(e.states.every((x) => /:ok:/.test(x)), e.states.join(', '));
      assert.equal(e.regions.length, 1);
      close(e.area, slot, 1e-6, `${name} area`);
      close(e.volume, slot * 5, 1e-5, `${name} volume`);
    }
    const rounded = modeling.dxfSketch(bytes('slot_mm.dxf'), { layers: ['ROUNDED'] });
    assert.equal(rounded.imported.lines, 4);
    assert.equal(rounded.imported.arcs, 4);
    assert.equal(rounded.merged, 8, 'each arc end merged with a line end');
    assert.equal(rounded.openEnds.length, 0);
    const e = extrude(rounded.sketch, 20);
    const area = 60 * 40 - (4 - PI) * 25;
    close(e.area, area, 1e-6, 'rounded area');
    close(e.volume, area * 20, 1e-5, 'rounded volume');
    // All three closed contours together: three regions, one solid each.
    const three = modeling.dxfSketch(bytes('slot_mm.dxf'), { layers: ['SLOT', 'SLOT_CW', 'ROUNDED'] });
    const all = extrude(three.sketch, 1);
    assert.equal(all.regions.length, 3);
    close(all.area, 2 * slot + area, 1e-5, 'three regions');
    assert.equal(all.solids, 3);
  });

  test('R12 POLYLINE with a bulge, ANSI_1251 layer name, no $INSUNITS', () => {
    expectKernelError(() => modeling.dxfSketch(bytes('outline_r12.dxf')), 'InvalidInput', /no \$INSUNITS/);
    const r = modeling.dxfSketch(bytes('outline_r12.dxf'), { units: 4 });
    assert.equal(r.fileUnits, 0);
    assert.deepEqual(r.layers.filter((l) => l.curves > 0).map((l) => l.name), ['КОНТУР']);
    assert.equal(r.imported.polylineSegments, 4);
    // Square 30 x 30 plus the circular segment of the bulged side: theta = 4 atan b,
    // radius = c (1 + b^2) / (4 b), segment = r^2 / 2 (theta - sin theta).
    const b = 0.5;
    const theta = 4 * Math.atan(b);
    const rad = (30 * (1 + b * b)) / (4 * b);
    const area = 900 + ((rad * rad) / 2) * (theta - Math.sin(theta));
    const e = extrude(r.sketch, 3);
    close(e.area, area, 1e-6, 'outline area');
    close(e.volume, area * 3, 1e-5, 'outline volume');
    // Bulge outward: the arc reaches x = -(r - sagitta offset).
    const sagitta = rad - Math.sqrt(rad * rad - 15 * 15);
    close(r.bounds.min[0], -sagitta, 1e-9, 'arc extreme');
    // Centimetres: everything ten times larger.
    const cm = modeling.dxfSketch(bytes('outline_r12.dxf'), { units: 5 });
    close(cm.bounds.max[0], 300, 1e-9, 'cm');
  });

  test('appending to existing sketch data and errors', () => {
    const base = modeling.sketchTemplate({ profiles: [{ kind: 'circle', cx: 500, cy: 500, radius: 10 }] });
    const r = modeling.dxfSketch(bytes('slot_mm.dxf'), { layers: ['SLOT'], into: base });
    assert.equal(r.sketch.points.length, base.points.length + r.points);
    assert.deepEqual(r.sketch.constraints, base.constraints, 'existing constraints kept');
    assert.deepEqual(Array.from(r.entities), [2, 3, 4, 5]);
    const e = extrude(r.sketch, 1);
    close(e.area, 50 * 20 + PI * 100 + PI * 100, 1e-5, 'slot + base circle');

    expectKernelError(() => modeling.dxfSketch(bytes('slot_mm.dxf'), { layers: ['NOPE'] }), 'InvalidInput', /no layer 'NOPE'/);
    expectKernelError(() => modeling.dxfSketch(bytes('slot_mm.dxf'), { tolerance: 0 }), 'InvalidInput', /tolerance/);
    expectKernelError(() => modeling.dxfSketch(bytes('slot_mm.dxf'), { units: 99 }), 'InvalidInput', /unknown DXF units code 99/);
    expectKernelError(() => modeling.dxfSketch(new TextEncoder().encode('0\nSECTION\n2\nENTITIES\n0\nLINE\n10\nabc\n')), 'ParseError', /./);
    expectKernelError(() => modeling.dxfSketch(42), 'InvalidInput', /Uint8Array/);
  });
});

describe('bendcore_drawing_v2 DXF', () => {
  test('top view of a plate on its sheet: the visible-line layer is the profile, area = part area x scale^2', () => {
    const meta = JSON.parse(fs.readFileSync(path.join(fixtures, 'plate_drawing.json'), 'utf8'));
    const all = modeling.dxfSketch(bytes('plate_drawing.dxf'));
    assert.equal(all.version, 'AC1015');
    assert.equal(all.fileUnits, 4);
    // The layer whose curves lie inside the view: the outline and the two holes.
    const inside = (l) => {
      const r = modeling.dxfSketch(bytes('plate_drawing.dxf'), { layers: [l.name] });
      if (!r.bounds) return false;
      const b = meta.bounds;
      const eps = 1e-6;
      return r.bounds.min[0] >= b.minX - eps && r.bounds.max[0] <= b.maxX + eps && r.bounds.min[1] >= b.minY - eps && r.bounds.max[1] <= b.maxY + eps
        && r.imported.circles === 2;
    };
    const visible = all.layers.filter((l) => l.curves > 0 && inside(l));
    assert.equal(visible.length, 1, `one layer with the view outline: ${all.layers.map((l) => `${l.name}:${l.curves}`).join(', ')}`);
    const r = modeling.dxfSketch(bytes('plate_drawing.dxf'), { layers: [visible[0].name] });
    assert.equal(r.openEnds.length, 0, 'closed outline');
    const k = meta.scale;
    const { width, height, holes } = meta.part;
    close(r.bounds.max[0] - r.bounds.min[0], width * k, 1e-6, 'view width');
    close(r.bounds.max[1] - r.bounds.min[1], height * k, 1e-6, 'view height');
    const diameters = r.sketch.geometry.filter((g) => g.type === 'circle').map((g) => (2 * Number(g.radius)) / k).sort((a, b) => a - b);
    assert.deepEqual(diameters.map((d) => Number(d.toFixed(6))), holes.map((h) => h.d).sort((a, b) => a - b));
    const e = extrude(r.sketch, 10);
    assert.ok(e.states.every((s) => /:ok:/.test(s)), e.states.join(', '));
    const area = (width * height - holes.reduce((s, h) => s + (PI * h.d * h.d) / 4, 0)) * k * k;
    close(e.area, area, 1e-5, 'sheet profile area');
    close(e.volume, area * 10, 1e-4, 'volume');
    // Whole sheet: frame and title block lines are open chains or rectangles of their own.
    assert.ok(all.imported.lines > r.imported.lines);
    assert.ok(all.ignored.texts > 0, 'title block texts are not geometry');
  });
});
