// Node check of the viewport tools of the WebAssembly modules (docs/ui/KERNEL_VIEWPORT.md,
// "Измерения и сечение"; docs/ui/PART_MODELING_UI.md, "Материал"): the material library, the
// measurements between faces and edges, the section by a plane, and the material saved in the
// part file. Values are compared with the analytic ones of a 100 x 60 x 20 box and a cylinder.
//
//   node tests/wasm/viewport_tools.test.mjs --modeling <dir with bendcore_modeling.mjs> \
//        --kernel <dir with bendcore_kernel.mjs> --assembly <dir with bendcore_assembly.mjs>
//
// The kernel and assembly directories are optional (their checks are skipped without them).

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

const modelingDir = argument('modeling', path.join(repo, 'build-wasm-all/wasm_modeling'));
const kernelDir = argument('kernel', path.join(repo, 'build-wasm-all/wasm_kernel'));
const assemblyDir = argument('assembly', path.join(repo, 'build-wasm-all/wasm_assembly'));
const MESH = { linearDeflection: 0.05, angularDeflection: 0.2 };

const near = (a, b, tol = 1e-3) => Math.abs(a - b) <= tol;

async function load(dir, glueName, apiName, loader) {
  const glue = await import(pathToFileURL(path.join(dir, glueName)).href);
  const api = await import(pathToFileURL(path.join(dir, apiName)).href);
  return { api, instance: await api[loader](glue.default) };
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

const face = (index) => ({ kind: 'face', index });
const edge = (index) => ({ kind: 'edge', index });

let modeling;
let modelingApi;
let kernel = null;
let assembly = null;

before(async () => {
  ({ api: modelingApi, instance: modeling } = await load(modelingDir, 'bendcore_modeling.mjs', 'bendcore_modeling_api.mjs', 'loadModeling'));
  if (fs.existsSync(path.join(kernelDir, 'bendcore_kernel.mjs'))) {
    kernel = (await load(kernelDir, 'bendcore_kernel.mjs', 'bendcore_kernel_api.mjs', 'loadKernel')).instance;
  }
  if (fs.existsSync(path.join(assemblyDir, 'bendcore_assembly.mjs'))) {
    assembly = await load(assemblyDir, 'bendcore_assembly.mjs', 'bendcore_assembly_api.mjs', 'loadAssembly');
  }
});

after(() => {
  assert.equal(modeling.info().liveDocuments, 0, 'every document created by the tests is disposed');
});

function box(doc) {
  doc.addFeature({ type: 'Box', id: 'Box', parameters: { length: '100', width: '60', height: '20' } });
  doc.rebuild();
  return doc.tessellate('final', MESH);
}

describe('material library', () => {
  test('lists the 21 built-in materials with densities from the kernel', () => {
    const lib = modeling.materials();
    assert.equal(lib.length, 21);
    const ids = new Set(lib.map((m) => m.id));
    assert.equal(ids.size, lib.length, 'ids are unique');
    for (const m of lib) {
      assert.ok(m.density > 500 && m.density < 20000, `${m.id}: density ${m.density}`);
      assert.equal(m.color.length, 3);
      assert.equal(typeof m.verified, 'boolean');
      assert.ok(m.name.length > 0 && m.category.length > 0 && m.source.length > 0);
    }
    const s235 = lib.find((m) => m.id === 'steel_s235');
    assert.ok(s235, 'steel_s235 is in the library');
    assert.equal(s235.density, 7850);
    assert.equal(lib.find((m) => m.category === 'aluminum').density < 3000, true);
  });

  test('the kernel module has the same library', { skip: !fs.existsSync(path.join(kernelDir, 'bendcore_kernel.mjs')) }, () => {
    assert.deepEqual(kernel.materials().map((m) => [m.id, m.density]), modeling.materials().map((m) => [m.id, m.density]));
  });

  test('the density of the material gives the mass: V * 1e-9 * density', () => {
    const doc = modeling.createDocument();
    try {
      box(doc);
      const volume = doc.properties('final').volume;
      assert.ok(near(volume, 120000, 1e-6));
      for (const id of ['steel_s235', modeling.materials().find((m) => m.category === 'aluminum').id]) {
        const m = modeling.materials().find((x) => x.id === id);
        const mass = doc.properties('final', m.density).mass;
        assert.ok(Math.abs(mass - volume * 1e-9 * m.density) < 1e-9, `${id}: ${mass}`);
      }
    } finally {
      doc.dispose();
    }
  });

  test('the material is saved in the part file and read back; the file stays a v3 document', () => {
    const doc = modeling.createDocument();
    let reloaded;
    try {
      box(doc);
      const text = doc.save({ material: { id: 'steel_s235', density: 7850 } });
      const parsed = JSON.parse(text);
      assert.equal(parsed.format, 'bendcore-part');
      assert.equal(parsed.version, 3);
      assert.deepEqual(parsed.material, { id: 'steel_s235', density: 7850 });
      reloaded = modeling.loadDocument(text);
      assert.deepEqual(reloaded.material, { id: 'steel_s235', density: 7850 });
      assert.ok(near(reloaded.document.properties('final').volume, 120000, 1e-6), 'the model loads unchanged');
      // A file without a material, and a damaged field.
      assert.equal(modeling.loadDocument(doc.save()).document.dispose() ?? null, null);
      const damaged = JSON.parse(text);
      damaged.material = { id: 7, density: -1 };
      const d = modeling.loadDocument(JSON.stringify(damaged));
      assert.equal(d.material, null);
      d.document.dispose();
      // The text without a material read through the API has none.
      const plain = modeling.loadDocument(doc.save());
      assert.equal(plain.material, null);
      plain.document.dispose();
      assert.throws(() => doc.save({ material: { id: 'x', density: 0 } }), /material/);
      // Saving again from the reloaded document keeps the data the caller passes.
      assert.deepEqual(JSON.parse(reloaded.document.save({ material: { id: 'custom', density: 2700 } })).material, { id: 'custom', density: 2700 });
    } finally {
      doc.dispose();
      reloaded?.document.dispose();
    }
  });
});

describe('measurements (part document)', () => {
  let doc;
  let mesh;
  before(() => {
    doc = modeling.createDocument();
    mesh = box(doc);
  });
  after(() => doc.dispose());

  test('distance between the bottom and the top face is 20 mm, with the nearest points', () => {
    const bottom = faceWhere(mesh, (x, y, z) => near(z, 0), 'bottom');
    const top = faceWhere(mesh, (x, y, z) => near(z, 20), 'top');
    const m = doc.measure({ type: 'distance', a: face(bottom), b: face(top) });
    assert.equal(m.type, 'distance');
    assert.equal(m.unit, 'mm');
    assert.ok(near(m.value, 20, 1e-6), `distance ${m.value}`);
    assert.equal(m.points.length, 2);
    assert.ok(near(Math.abs(m.points[0][2] - m.points[1][2]), 20, 1e-6), 'the points are 20 apart along Z');
  });

  test('distance between a face and a parallel edge', () => {
    const bottom = faceWhere(mesh, (x, y, z) => near(z, 0), 'bottom');
    const topEdge = edgeWhere(mesh, (x, y, z) => near(z, 20) && near(y, 0), 'top front edge');
    const m = doc.measure({ type: 'distance', a: face(bottom), b: edge(topEdge) });
    assert.ok(near(m.value, 20, 1e-6));
  });

  test('angle between the top and a side face is 90 degrees, between top and bottom 180', () => {
    const top = faceWhere(mesh, (x, y, z) => near(z, 20), 'top');
    const bottom = faceWhere(mesh, (x, y, z) => near(z, 0), 'bottom');
    const side = faceWhere(mesh, (x) => near(x, 0), 'left side');
    const m = doc.measure({ type: 'angle', a: face(top), b: face(side) });
    assert.equal(m.unit, 'deg');
    assert.ok(near(m.value, 90, 1e-6), `angle ${m.value}`);
    assert.equal(m.points.length, 3, 'vertex and a point on each face');
    assert.ok(near(doc.measure({ type: 'angle', a: face(top), b: face(bottom) }).value, 180, 1e-6));
  });

  test('angle between two edges: along X and along Y are 90 degrees', () => {
    const alongX = edgeWhere(mesh, (x, y, z) => near(y, 0) && near(z, 0), 'edge along X');
    const alongY = edgeWhere(mesh, (x, y, z) => near(x, 0) && near(z, 0), 'edge along Y');
    assert.ok(near(doc.measure({ type: 'angle', a: edge(alongX), b: edge(alongY) }).value, 90, 1e-6));
  });

  test('length of an edge and area of a face', () => {
    const alongX = edgeWhere(mesh, (x, y, z) => near(y, 0) && near(z, 0), 'edge along X');
    const l = doc.measure({ type: 'length', a: edge(alongX) });
    assert.ok(near(l.value, 100, 1e-6));
    assert.equal(l.points.length, 3, 'start, end and middle');
    const top = faceWhere(mesh, (x, y, z) => near(z, 20), 'top');
    const a = doc.measure({ type: 'area', a: face(top) });
    assert.equal(a.unit, 'mm2');
    assert.ok(near(a.value, 6000, 1e-6));
  });

  test('radius of a cylinder face and of a circular edge', () => {
    const cyl = modeling.createDocument();
    try {
      cyl.addFeature({ type: 'Cylinder', id: 'Cyl', parameters: { radius: '5', height: '12' } });
      cyl.rebuild();
      const m = cyl.tessellate('final', MESH);
      let r = null;
      for (let i = 0; i < 3 && !r; ++i) {
        try {
          r = cyl.measure({ type: 'radius', a: face(i) });
        } catch (e) {
          assert.equal(e.code, 'Unsupported');
        }
        if (r && !near(r.value, 5, 1e-6)) r = null;
      }
      assert.ok(r, 'the cylinder face has radius 5');
      assert.ok(near(r.diameter, 10, 1e-6));
      const rim = cyl.measure({ type: 'radius', a: edge(m.edges.find((e) => e.points.length > 12).edgeIndex) });
      assert.ok(near(rim.value, 5, 1e-6));
    } finally {
      cyl.dispose();
    }
  });

  test('wrong requests are InvalidInput errors, not crashes', () => {
    const top = faceWhere(mesh, (x, y, z) => near(z, 20), 'top');
    const anEdge = edgeWhere(mesh, (x, y, z) => near(y, 0) && near(z, 0), 'edge');
    assert.throws(() => doc.measure({ type: 'angle', a: face(top), b: edge(anEdge) }), (e) => e.code === 'InvalidInput');
    assert.throws(() => doc.measure({ type: 'distance', a: face(top) }), (e) => e.code === 'InvalidInput');
    assert.throws(() => doc.measure({ type: 'distance', a: face(999), b: face(top) }), (e) => e.code === 'InvalidInput');
    assert.throws(() => doc.measure({ type: 'length', a: face(top) }), (e) => e.code === 'InvalidInput');
    assert.throws(() => doc.measure({ type: 'volume', a: face(top) }), (e) => e.code === 'InvalidInput');
    assert.throws(() => doc.measure({ type: 'area', a: { kind: 'solid', index: 0 } }), (e) => e.code === 'InvalidInput');
  });
});

describe('section (part document)', () => {
  let doc;
  before(() => {
    doc = modeling.createDocument();
    box(doc);
  });
  after(() => doc.dispose());

  test('plane z = 10: the section is the 100 x 60 rectangle', () => {
    const s = doc.section({ origin: [50, 30, 10], normal: [0, 0, 1] });
    assert.ok(near(s.area, 6000, 1e-6), `area ${s.area}`);
    assert.ok(s.polylines.length >= 1);
    for (const line of s.polylines) {
      assert.ok(line instanceof Float32Array && line.length >= 6 && line.length % 3 === 0);
      for (let i = 2; i < line.length; i += 3) assert.ok(near(line[i], 10, 1e-4), 'the contour lies in the plane');
    }
  });

  test('plane x = 25 gives 60 x 20; an inclined plane gives the analytic area', () => {
    assert.ok(near(doc.section({ origin: [25, 0, 0], normal: [1, 0, 0] }).area, 1200, 1e-6));
    // Plane through the middle of the X extent tilted about Y by 45 degrees: the section of the
    // box x in [0, 100] cut at z in [0, 20] is a parallelogram, 60 wide and 20*sqrt(2) long.
    const c = Math.SQRT1_2;
    const s = doc.section({ origin: [50, 30, 10], normal: [c, 0, c] });
    assert.ok(near(s.area, 60 * 20 * Math.SQRT2, 1e-4), `area ${s.area}`);
  });

  test('a plane that misses the body has no area; a zero normal is an error', () => {
    let missed;
    try {
      missed = doc.section({ origin: [0, 0, 50], normal: [0, 0, 1] });
    } catch (e) {
      assert.equal(e.code === 'OperationFailed' || e.code === 'InvalidInput', true);
      return;
    } finally {
      assert.throws(() => doc.section({ origin: [0, 0, 0], normal: [0, 0, 0] }), (e) => e.code === 'InvalidInput');
    }
    assert.equal(missed.area, 0);
  });
});

const haveKernel = fs.existsSync(path.join(kernelDir, 'bendcore_kernel.mjs'));
const haveAssembly = fs.existsSync(path.join(assemblyDir, 'bendcore_assembly.mjs'));

describe('kernel module', { skip: !haveKernel }, () => {
  test('measures and cuts a shape read back from STEP', () => {
    const doc = modeling.createDocument();
    let shape;
    try {
      const mesh = box(doc);
      shape = kernel.readStep(doc.exportFile('step'));
      const kmesh = shape.tessellate(MESH);
      const bottom = faceWhere(kmesh, (x, y, z) => near(z, 0), 'bottom');
      const top = faceWhere(kmesh, (x, y, z) => near(z, 20), 'top');
      assert.ok(near(shape.measure({ type: 'distance', a: face(bottom), b: face(top) }).value, 20, 1e-6));
      assert.ok(near(shape.measure({ type: 'area', a: face(top) }).value, 6000, 1e-6));
      assert.ok(near(shape.section({ origin: [50, 30, 10], normal: [0, 0, 1] }).area, 6000, 1e-6));
      assert.ok(mesh.faceSpans.length === kmesh.faceSpans.length);
      assert.throws(() => shape.measure({ type: 'angle', a: face(top), b: edge(0) }), (e) => e.code === 'InvalidInput');
    } finally {
      shape?.dispose();
      doc.dispose();
    }
  });
});

describe('assembly module', { skip: !haveAssembly }, () => {
  // Two copies of the 100 x 60 x 20 box part; the second one is placed by `placement`.
  function build(asm, placement) {
    const part = modeling.createDocument();
    let text;
    try {
      box(part);
      text = part.save({ includeCache: true });
    } finally {
      part.dispose();
    }
    const doc = asm.createDocument('Pair');
    const a = doc.insertPart({ name: 'Lower', format: 'part-json', data: text, fixed: true });
    const b = doc.insertPart({ name: 'Upper', format: 'part-json', data: text, placement, fixed: true });
    return { doc, a: [a.occurrence], b: [b.occurrence] };
  }

  // Face of the occurrence whose world entity is the plane through height z with normal nz.
  function planeFace(doc, path, z, nz) {
    for (let i = 0; i < 6; ++i) {
      const e = doc.element({ path, kind: 'face', index: i }).entity;
      if (e && e.type === 'plane' && near(e.point[2], z, 1e-6) && near(e.direction[2], nz, 1e-9)) return { path, kind: 'face', index: i };
    }
    assert.fail(`no face at z = ${z}`);
  }

  test('distance between faces of two components in world coordinates', () => {
    const { doc, a, b } = build(assembly.instance, { position: [0, 0, 50] });
    try {
      const top = planeFace(doc, a, 20, 1);
      const bottom = planeFace(doc, b, 50, -1);
      const m = doc.measure({ type: 'distance', a: top, b: bottom });
      assert.equal(m.unit, 'mm');
      assert.ok(near(m.value, 30, 1e-6), `distance ${m.value}`);
      assert.equal(m.points.length, 2);
      assert.ok(near(m.points[0][2], 20, 1e-6) && near(m.points[1][2], 50, 1e-6), 'the nearest points are in world coordinates');
      assert.ok(near(doc.measure({ type: 'area', a: top }).value, 6000, 1e-6));
    } finally {
      doc.dispose();
    }
  });

  test('angle between faces follows the placement of the component', () => {
    const { doc, a, b } = build(assembly.instance, { position: [0, 0, 60], axis: [0, 1, 0], angle: 90 });
    try {
      const top = planeFace(doc, a, 20, 1);
      let best = null;
      for (let i = 0; i < 6; ++i) {
        const e = doc.element({ path: b, kind: 'face', index: i }).entity;
        const dot = e.direction[2];
        if (e.type === 'plane' && near(Math.abs(dot), 0, 1e-9) && !best) best = { i, e };
      }
      assert.ok(best, 'a face of the rotated block is vertical');
      const m = doc.measure({ type: 'angle', a: top, b: { path: b, kind: 'face', index: best.i } });
      assert.ok(near(m.value, 90, 1e-6), `angle ${m.value}`);
      assert.equal(m.unit, 'deg');
    } finally {
      doc.dispose();
    }
  });

  test('wrong requests are InvalidInput errors', () => {
    const { doc, a, b } = build(assembly.instance, { position: [0, 0, 50] });
    try {
      const top = planeFace(doc, a, 20, 1);
      assert.throws(() => doc.measure({ type: 'distance', a: top }), (e) => e.code === 'InvalidInput');
      assert.throws(() => doc.measure({ type: 'distance', a: top, b: { path: [9999], kind: 'face', index: 0 } }), (e) => e.code === 'InvalidInput');
      assert.throws(() => doc.measure({ type: 'distance', a: top, b: { path: b, kind: 'face', index: 99 } }), (e) => e.code === 'InvalidInput');
      assert.throws(() => doc.measure({ type: 'volume', a: top }), (e) => e.code === 'InvalidInput');
    } finally {
      doc.dispose();
    }
  });
});
