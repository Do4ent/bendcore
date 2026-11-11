// Node check of the sketch editor API of bendcore_modeling_wasm (docs/ui/SKETCH_EDITOR.md):
// definition diagnostics and reference dimensions of the sketch node (sketchResult) and the
// editor calls solveSketch / editSketch on reference sketches with known answers:
//   - rectangle 60 x 40 with a fixed corner: fully defined, dof 0, corners at the analytic positions;
//   - the same without the width: dof 1, exactly the entities that depend on the width are free;
//   - a second width 50: both widths conflicting; a second width 60: redundant;
//   - a reference diagonal: measured sqrt(60^2 + 40^2), the sketch stays fully defined;
//   - rectangle with a circle inside: two regions, areas, material selection, selectors;
//   - dragging a free corner, writing the solution back into the data;
//   - fillet, chamfer, trim, mirror, linear pattern of the editing tools, against analytic areas
//     and the volume of the extruded document;
//   - the editor solve and the document agree (definition, dof, dimension values, areas).
//
//   node tests/wasm/sketch_editor.test.mjs --module <dir with bendcore_modeling.mjs>
//
// Registered in ctest as wasm_sketch_editor (cmake/wasm_modeling.cmake).

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
const { PI } = Math;

const close = (a, b, tol, what) => assert.ok(Math.abs(a - b) <= tol, `${what}: ${a} vs ${b}`);

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

/** Rectangle 60 x 40 (corner (0,0) fixed): points 0..3, lines 1..4 (bottom, right, top, left),
 *  constraints 1..4 horizontal/vertical, 5 width (p0-p1), 6 height (p1-p2). */
const rectangle = () => modeling.sketchTemplate({ profiles: [{ kind: 'rectangle', x: 0, y: 0, width: 60, height: 40 }] });

function withSketch(sketch, body, options = { plane: 'XY' }) {
  const doc = modeling.createDocument();
  try {
    doc.addFeature({ type: 'Sketch', id: 'S', options, sketch });
    doc.rebuild();
    body(doc);
  } finally {
    doc.dispose();
  }
}

describe('sketch node diagnostics (sketchResult)', () => {
  test('fully defined rectangle: definition, point and entity definition, dimension values', () => {
    withSketch(rectangle(), (doc) => {
      const r = doc.feature('S').sketchResult;
      assert.equal(r.definition, 'fully-defined');
      assert.equal(r.dof, 0);
      assert.deepEqual(r.redundant, []);
      assert.deepEqual(r.pointDefined, [true, true, true, true]);
      assert.deepEqual(r.entityDefined, [true, true, true, true]);
      assert.deepEqual(r.constraintValues.slice(4), [60, 40]);
      assert.deepEqual(r.placement.normal, [0, 0, 1]);
    });
  });

  test('without the width: under-defined, the width-dependent entities are free', () => {
    const s = rectangle();
    s.constraints.splice(4, 1);
    withSketch(s, (doc) => {
      const r = doc.feature('S').sketchResult;
      assert.equal(r.definition, 'under-defined');
      assert.equal(r.dof, 1);
      // p0 fixed, p3 = (0, 40) by vertical + horizontal + height; p1, p2 slide along x.
      assert.deepEqual(r.pointDefined, [true, false, false, true]);
      assert.deepEqual(r.entityDefined, [false, false, false, true]);
    });
  });

  test('redundant width (same value): over-defined with the redundant numbers, node warning', () => {
    const s = rectangle();
    s.constraints.push({ type: 'distance', a: 0, b: 1, c: -1, value: '60' });
    withSketch(s, (doc) => {
      const f = doc.feature('S');
      assert.equal(f.state, 'warning');
      assert.equal(f.sketchResult.definition, 'over-defined');
      assert.ok(f.sketchResult.redundant.length >= 1);
      assert.ok(f.sketchResult.redundant.every((k) => [5, 7].includes(k)), `redundant ${f.sketchResult.redundant}`);
    });
  });

  test('reference diagonal: measured, not driving', () => {
    const s = rectangle();
    s.constraints.push({ type: 'distance', a: 0, b: 2, c: -1, value: '0', driven: true });
    withSketch(s, (doc) => {
      const r = doc.feature('S').sketchResult;
      assert.equal(r.definition, 'fully-defined');
      assert.equal(r.dof, 0);
      close(r.constraintValues[6], Math.hypot(60, 40), 1e-9, 'reference diagonal');
      assert.equal(doc.feature('S').sketch.constraints[6].driven, true);
    });
  });
});

describe('solveSketch', () => {
  test('agrees with the sketch node: positions, definition, dimension values', () => {
    withSketch(rectangle(), (doc) => {
      const sol = doc.solveSketch('S', doc.feature('S').sketch);
      const node = doc.feature('S').sketchResult;
      assert.equal(sol.converged, true);
      assert.equal(sol.definition, node.definition);
      assert.equal(sol.dof, node.dof);
      assert.deepEqual(sol.constraintValues, node.constraintValues);
      const expected = [[0, 0], [60, 0], [60, 40], [0, 40]];
      sol.points.forEach((p, k) => {
        close(p.x, expected[k][0], 1e-9, `x of point ${k}`);
        close(p.y, expected[k][1], 1e-9, `y of point ${k}`);
        assert.equal(p.defined, true);
      });
      assert.equal(sol.points[0].fixed, true);
      assert.equal(sol.curves.length, 4);
      const bottom = sol.curves[0];
      assert.equal(bottom.type, 'line');
      assert.deepEqual(Array.from(bottom.polyline).map((v) => Math.round(v * 1e9) / 1e9), [0, 0, 60, 0]);
      assert.deepEqual(bottom.midpoint.map((v) => Math.round(v * 1e9) / 1e9), [30, 0]);
      // One region, selected (material), area 2400 = the face area of the document.
      assert.equal(sol.profile.regions.length, 1);
      close(sol.profile.regions[0].area, 2400, 1e-6, 'region area');
      close(doc.properties('sketch:S').area, sol.profile.regions[0].area, 1e-6, 'document region area');
      assert.equal(sol.profile.regions[0].selected, true);
      assert.ok(sol.profile.regions[0].indices.length >= 6, 'region is triangulated');
      // The free corners of the template start 1 mm off; the solved data has them in place.
      assert.equal(Number(sol.sketch.points[2].x), 60);
      assert.equal(Number(sol.sketch.points[2].y), 40);
      assert.equal(sol.sketch.points[0].x, '0');
    });
  });

  test('conflicting width: both numbers, the node fails with the same numbers', () => {
    const s = rectangle();
    s.constraints.push({ type: 'distance', a: 0, b: 1, c: -1, value: '50' });
    withSketch(s, (doc) => {
      const sol = doc.solveSketch('S', s);
      assert.equal(sol.definition, 'over-defined');
      assert.deepEqual([...sol.conflicting].sort(), [5, 7]);
      assert.equal(sol.profile, null);
      const f = doc.feature('S');
      assert.equal(f.state, 'error');
      assert.match(f.messages.join(' '), /conflict.*#5.*#7|conflict.*#7.*#5/);
    });
  });

  test('dragging a free corner moves the width, a fixed corner cannot be dragged', () => {
    const s = rectangle();
    s.constraints.splice(4, 1);
    withSketch(s, (doc) => {
      const sol = doc.solveSketch('S', s, { drag: { point: 1, x: 75, y: 3 } });
      assert.equal(sol.converged, true);
      close(sol.points[1].x, 75, 1e-6, 'dragged corner x');
      close(sol.points[1].y, 0, 1e-9, 'dragged corner stays on the horizontal bottom');
      close(sol.points[2].x, 75, 1e-6, 'opposite corner follows');
      close(sol.points[0].x, 0, 1e-12, 'fixed corner');
      assert.equal(sol.dof, 1, 'diagnostics without the drag constraint');
      close(Number(sol.sketch.points[1].x), 75, 1e-6, 'written back');
      close(sol.profile.regions[0].area, 75 * 40, 1e-4, 'area after the drag');
      assert.throws(
        () => doc.solveSketch('S', s, { drag: { point: 0, x: 5, y: 5 } }),
        (e) => e.code === 'InvalidInput' && /fixed/.test(e.detail),
      );
    });
  });

  test('rectangle with a circle inside: two regions, hole, selection by selector entity', () => {
    const s = modeling.sketchTemplate({
      profiles: [
        { kind: 'rectangle', x: 0, y: 0, width: 60, height: 40 },
        { kind: 'circle', cx: 30, cy: 20, radius: 8 },
      ],
    });
    withSketch(s, (doc) => {
      const sol = doc.solveSketch('S', s);
      assert.equal(sol.definition, 'fully-defined');
      const regions = sol.profile.regions;
      assert.equal(regions.length, 2);
      const outer = regions.find((r) => r.depth === 0);
      const inner = regions.find((r) => r.depth === 1);
      close(outer.area, 2400 - PI * 64, 1e-6, 'plate with a hole');
      close(inner.area, PI * 64, 1e-6, 'disc');
      assert.equal(outer.selected, true);
      assert.equal(inner.selected, false);
      assert.deepEqual(inner.entities, [5]);
      assert.equal(inner.selector, 5);
      assert.equal(outer.holes.length, 1);
      // The circle curve: centre and radius from the kernel, polyline on the circle.
      const circle = sol.curves[4];
      assert.deepEqual(circle.center, [30, 20]);
      close(circle.radius, 8, 1e-9, 'circle radius');
      assert.equal(circle.midpoint, null);
      for (let i = 0; i < circle.polyline.length; i += 2) {
        close(Math.hypot(circle.polyline[i] - 30, circle.polyline[i + 1] - 20), 8, 1e-9, 'polyline on circle');
      }
      // Selecting the disc only.
      const picked = { ...s, regions: [inner.selector] };
      const sol2 = doc.solveSketch('S', picked);
      assert.deepEqual(sol2.profile.regions.map((r) => r.selected), regions.map((r) => r === inner));
    });
  });

  test('open profile and intersections', () => {
    const s = {
      points: [
        { x: '0', y: '0', fixed: false },
        { x: '10', y: '10', fixed: false },
        { x: '0', y: '10', fixed: false },
        { x: '10', y: '0', fixed: false },
      ],
      geometry: [
        { type: 'line', start: 0, end: 1 },
        { type: 'line', start: 2, end: 3 },
      ],
      constraints: [],
      regions: [],
    };
    withSketch(s, (doc) => {
      assert.equal(doc.feature('S').state, 'error', 'the node cannot build an open profile');
      const sol = doc.solveSketch('S', s);
      assert.equal(sol.definition, 'under-defined');
      assert.equal(sol.dof, 8);
      assert.equal(sol.intersections.length, 1);
      close(sol.intersections[0].x, 5, 1e-9, 'intersection x');
      close(sol.intersections[0].y, 5, 1e-9, 'intersection y');
      assert.ok(sol.profile.issues.some((i) => i.kind === 'open-contour' || /open/i.test(i.kind)), JSON.stringify(sol.profile.issues));
      assert.deepEqual(sol.profile.regions, []);
    });
  });

  test('placement on XZ with an offset, before and after the node builds', () => {
    const plain = (v) => v.map((c) => c + 0);  // -0 -> 0
    const open = { points: [{ x: '0', y: '0' }, { x: '5', y: '0' }], geometry: [{ type: 'line', start: 0, end: 1 }], constraints: [], regions: [] };
    const doc = modeling.createDocument();
    try {
      doc.addFeature({ type: 'Sketch', id: 'S', options: { plane: 'XZ' }, parameters: { offset: 7 }, sketch: open });
      doc.rebuild();
      // Open profile: the node is an error, the editor places the base plane itself.
      assert.equal(doc.feature('S').state, 'error');
      const sol = doc.solveSketch('S', open);
      assert.deepEqual(plain(sol.placement.normal), [0, -1, 0]);
      assert.deepEqual(plain(sol.placement.xDirection), [1, 0, 0]);
      assert.deepEqual(plain(sol.placement.yDirection), [0, 0, 1]);
      assert.deepEqual(plain(sol.placement.origin), [0, -7, 0], 'offset along the normal');
      // Built node: the same frame from its result.
      doc.editFeature('S', { sketch: rectangle() });
      doc.rebuild();
      assert.equal(doc.feature('S').state, 'ok');
      assert.deepEqual(doc.feature('S').sketchResult.placement, sol.placement);
      assert.deepEqual(doc.solveSketch('S', rectangle()).placement, sol.placement);
    } finally {
      doc.dispose();
    }
  });

  test('errors: bad operand, unknown feature, not a sketch', () => {
    withSketch(rectangle(), (doc) => {
      const bad = rectangle();
      bad.constraints.push({ type: 'radius', a: 1, b: -1, c: -1, value: '3' });
      assert.throws(() => doc.solveSketch('S', bad), (e) => e.code === 'InvalidInput' && /constraint 7.*not a circle/.test(e.detail));
      const negative = rectangle();
      negative.constraints[4].value = '-5';
      assert.throws(() => doc.solveSketch('S', negative), (e) => e.code === 'InvalidInput' && /positive/.test(e.detail));
      assert.throws(() => doc.solveSketch('Nope', bad), (e) => e.code === 'InvalidInput' && /Nope/.test(e.detail));
      doc.addFeature({ type: 'Extrude', id: 'E', parameters: { distance: 5 }, links: { sketch: 'S' } });
      doc.rebuild();
      assert.throws(() => doc.solveSketch('E', bad), (e) => e.code === 'InvalidInput' && /not a sketch/.test(e.detail));
      assert.throws(() => doc.editSketch('S', rectangle(), { name: 'warp' }), (e) => e.code === 'InvalidInput' && /unknown tool/.test(e.detail));
    });
  });

  test('variables in sketch expressions are evaluated in the document', () => {
    const s = modeling.sketchTemplate({ profiles: [{ kind: 'rectangle', x: 0, y: 0, width: 'W', height: 'W / 2' }] });
    const doc = modeling.createDocument();
    try {
      doc.setVariable('W', '50');
      doc.addFeature({ type: 'Sketch', id: 'S', options: { plane: 'XY' }, sketch: s });
      doc.rebuild();
      const sol = doc.solveSketch('S', s);
      close(sol.points[2].x, 50, 1e-9, 'W');
      close(sol.points[2].y, 25, 1e-9, 'W / 2');
      assert.equal(sol.sketch.constraints[4].value, 'W', 'expressions are kept');
    } finally {
      doc.dispose();
    }
  });
});

describe('editSketch', () => {
  test('fillet of four corners: fully defined, area, extruded volume follows the width', () => {
    const r = 5;
    withSketch(rectangle(), (doc) => {
      let s = doc.solveSketch('S', rectangle()).sketch;
      // Corners (bottom, right), (right, top), (top, left), (left, bottom); entity ids stay 1..4.
      for (const [a, b] of [[1, 2], [2, 3], [3, 4], [4, 1]]) {
        const out = doc.editSketch('S', s, { name: 'fillet', a, b, radius: r });
        assert.equal(out.created.length, 1, 'one arc per corner');
        s = out.sketch;
      }
      assert.equal(s.geometry.filter((g) => g.type === 'arc').length, 4);
      const sol = doc.solveSketch('S', s);
      assert.equal(sol.definition, 'fully-defined');
      assert.equal(sol.dof, 0);
      const area = 60 * 40 - (4 - PI) * r * r;
      close(sol.profile.regions[0].area, area, 1e-6, 'rounded rectangle area');
      doc.editFeature('S', { sketch: sol.sketch });
      doc.addFeature({ type: 'Extrude', id: 'E', parameters: { distance: 10 }, links: { sketch: 'S' } });
      doc.rebuild();
      close(doc.properties('final').volume, area * 10, 1e-6 * area * 10, 'extruded volume');
      // Width 60 -> 80: the fillets follow (the corner stays a virtual sharp on both lines).
      const wider = structuredClone(sol.sketch);
      wider.constraints[4].value = '80';
      doc.editFeature('S', { sketch: wider });
      doc.rebuild();
      const area80 = 80 * 40 - (4 - PI) * r * r;
      close(doc.properties('final').volume, area80 * 10, 1e-6 * area80 * 10, 'volume after the width change');
    });
  });

  test('chamfer, trim, mirror, linear pattern', () => {
    withSketch(rectangle(), (doc) => {
      const s0 = doc.solveSketch('S', rectangle()).sketch;
      // Chamfer 6 x 4 at the corner (bottom, right): area loses 12.
      const ch = doc.editSketch('S', s0, { name: 'chamfer', a: 1, b: 2, d1: 6, d2: 4 });
      const solCh = doc.solveSketch('S', ch.sketch);
      close(solCh.profile.regions[0].area, 2400 - 12, 1e-6, 'chamfered area');
      assert.equal(solCh.definition, 'fully-defined');

      // Trim: a vertical construction-free line x = 20 across the rectangle, trimmed above y = 40.
      const t = structuredClone(s0);
      t.points.push({ x: '20', y: '-10', fixed: false }, { x: '20', y: '50', fixed: false });
      t.geometry.push({ type: 'line', start: 4, end: 5 });
      const trimmed = doc.editSketch('S', t, { name: 'trim', entity: 5, pick: [20, 48] });
      const line = trimmed.sketch.geometry[4];
      const solT = doc.solveSketch('S', trimmed.sketch);
      const ends = [solT.points[line.start], solT.points[line.end]].map((p) => p.y).sort((a, b) => a - b);
      close(ends[0], -10, 1e-9, 'kept end');
      close(ends[1], 40, 1e-9, 'trimmed at the top side');
      assert.ok(trimmed.constraints.length >= 1, 'point-on constraint at the cut');

      // Mirror the bottom line about the left side (x = 0): copy from (0,0) to (-60,0).
      const mir = doc.editSketch('S', s0, { name: 'mirror', entities: [1], axis: 4 });
      assert.equal(mir.created.length, 1);
      const solM = doc.solveSketch('S', mir.sketch);
      const copy = solM.curves[mir.created[0] - 1].polyline;
      const xs = [copy[0], copy[2]].sort((a, b) => a - b);
      close(xs[0], -60, 1e-9, 'mirrored end');
      close(xs[1], 0, 1e-9, 'end on the axis');

      // Linear pattern of a circle: 3 copies in total, step (20, 0), equal radii.
      const c = structuredClone(s0);
      c.points.push({ x: '10', y: '10', fixed: true });
      c.geometry.push({ type: 'circle', center: 4, radius: '3' });
      const pat = doc.editSketch('S', c, { name: 'linear-pattern', entities: [5], step: [20, 0], count: 3 });
      assert.equal(pat.created.length, 2);
      const solP = doc.solveSketch('S', pat.sketch);
      const centers = pat.created.map((id) => solP.curves[id - 1].center[0]).sort((a, b) => a - b);
      close(centers[0], 30, 1e-9, 'second copy');
      close(centers[1], 50, 1e-9, 'third copy');
    });
  });
});
