// Node check of the reference meshes of the part and assembly WebAssembly modules
// (docs/modeling/PART_DOCUMENT.md and docs/assembly/ASSEMBLY_DOCUMENT.md, "Справочная сетка";
// docs/kernel/ANALYSIS.md, section 7). A binary and an ASCII STL of a 10 mm cube are written here
// byte by byte; the modules report 12 triangles, area 600, volume 1000, the section at z = 5 is a
// 10 x 10 square, inches scale the cube to 254 mm, the mesh is saved inside the document and comes
// back, the distance between two snapped corners is 10·√3 and from a mesh corner to a body face
// the gap; a truncated file is refused.
//
//   node tests/wasm/reference_mesh.test.mjs --modeling <dir with bendcore_modeling.mjs> \
//        --assembly <dir with bendcore_assembly.mjs>
//   (also --module <dir>: the modeling directory)

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

const modelingDir = argument('modeling', argument('module', path.join(repo, 'build-wasm-all/wasm_modeling')));
const assemblyDir = argument('assembly', path.join(repo, 'build-wasm-all/wasm_assembly'));
const near = (a, b, tol = 1e-6) => Math.abs(a - b) <= tol;

// Cube [0, a]^3: 12 triangles, counter-clockwise from outside.
function cubeTriangles(a) {
  const P = (x, y, z) => [x * a, y * a, z * a];
  const quads = [
    [[0, 0, 0], [0, 1, 0], [1, 1, 0], [1, 0, 0]],
    [[0, 0, 1], [1, 0, 1], [1, 1, 1], [0, 1, 1]],
    [[0, 0, 0], [1, 0, 0], [1, 0, 1], [0, 0, 1]],
    [[0, 1, 0], [0, 1, 1], [1, 1, 1], [1, 1, 0]],
    [[0, 0, 0], [0, 0, 1], [0, 1, 1], [0, 1, 0]],
    [[1, 0, 0], [1, 1, 0], [1, 1, 1], [1, 0, 1]],
  ];
  const out = [];
  for (const q of quads) {
    const c = q.map((v) => P(...v));
    out.push([c[0], c[1], c[2]], [c[0], c[2], c[3]]);
  }
  return out;
}

function binaryStl(tris) {
  const buf = Buffer.alloc(84 + 50 * tris.length);
  buf.write('test cube', 0, 'ascii');
  buf.writeUInt32LE(tris.length, 80);
  tris.forEach((t, i) => {
    const at = 84 + 50 * i + 12;
    t.forEach((v, k) => v.forEach((c, j) => buf.writeFloatLE(c, at + 12 * k + 4 * j)));
  });
  return new Uint8Array(buf);
}

function asciiStl(tris) {
  let s = 'solid cube\n';
  for (const t of tris) {
    s += ' facet normal 0 0 0\n  outer loop\n';
    for (const v of t) s += `   vertex ${v.join(' ')}\n`;
    s += '  endloop\n endfacet\n';
  }
  return new TextEncoder().encode(`${s}endsolid cube\n`);
}

// A triangle of the display mesh with a corner at p.
function triangleAt(geometry, p) {
  for (let t = 0; t * 3 < geometry.indices.length; ++t) {
    for (let k = 0; k < 3; ++k) {
      const v = geometry.indices[t * 3 + k] * 3;
      if (near(geometry.positions[v], p[0]) && near(geometry.positions[v + 1], p[1]) && near(geometry.positions[v + 2], p[2])) return t;
    }
  }
  throw new Error(`no triangle at ${p}`);
}

let modeling;
let assembly = null;

before(async () => {
  const glue = await import(pathToFileURL(path.join(modelingDir, 'bendcore_modeling.mjs')).href);
  const api = await import(pathToFileURL(path.join(modelingDir, 'bendcore_modeling_api.mjs')).href);
  modeling = await api.loadModeling(glue.default);
  if (fs.existsSync(path.join(assemblyDir, 'bendcore_assembly.mjs'))) {
    const ag = await import(pathToFileURL(path.join(assemblyDir, 'bendcore_assembly.mjs')).href);
    const aa = await import(pathToFileURL(path.join(assemblyDir, 'bendcore_assembly_api.mjs')).href);
    assembly = await aa.loadAssembly(ag.default);
  }
});

after(() => {
  assert.equal(modeling.info().liveDocuments, 0, 'every part document is disposed');
});

describe('part reference mesh', () => {
  test('binary and ASCII STL of a cube against the analytic figures', () => {
    for (const data of [binaryStl(cubeTriangles(10)), asciiStl(cubeTriangles(10))]) {
      const doc = modeling.createDocument();
      try {
        const id = doc.addMesh({ fileName: 'cube.stl', data });
        const [m] = doc.meshes();
        assert.equal(m.id, id);
        assert.equal(m.properties.triangles, 12);
        assert.equal(m.properties.closed, true);
        assert.ok(near(m.properties.area, 600, 1e-9));
        assert.ok(near(m.properties.volume, 1000, 1e-9));
        assert.deepEqual(m.properties.size, [10, 10, 10]);
        const g = doc.meshGeometry(id);
        assert.equal(g.indices.length, 36);
        assert.equal(g.positions.length, 24);
        const s = doc.meshSection(id, { origin: [0, 0, 5], normal: [0, 0, 1] });
        assert.equal(s.polylines.length, 1);
        assert.equal(s.closed[0], true);
        assert.ok(near(s.area, 100, 1e-9));
        assert.ok(near(s.length, 40, 1e-9));
        assert.equal(s.polylines[0].length, 15); // 4 corners + the first again
      } finally {
        doc.dispose();
      }
    }
  });

  test('placement, undo, save and load with the embedded file', () => {
    const doc = modeling.createDocument();
    let loaded = null;
    try {
      doc.addFeature({ type: 'Box', id: 'Box', parameters: { length: '20', width: '20', height: '20' } });
      doc.rebuild();
      const volume = doc.properties('final').volume;
      const id = doc.addMesh({ fileName: 'cube.stl', data: binaryStl(cubeTriangles(10)) });
      doc.editMesh(id, { unit: 'in', translation: [100, 0, 0], opacity: 0.3 });
      let [m] = doc.meshes();
      assert.ok(near(m.properties.min[0], 100));
      assert.ok(near(m.properties.size[0], 254, 1e-9));
      assert.equal(m.opacity, 0.3);
      assert.equal(doc.properties('final').volume, volume, 'the mesh is no body');
      doc.undo(); // the edit is one step
      [m] = doc.meshes();
      assert.ok(near(m.properties.size[0], 10));
      doc.redo();
      const text = doc.save();
      assert.equal(JSON.parse(text).version, 5);
      loaded = modeling.loadDocument(text).document;
      const [b] = loaded.meshes();
      assert.equal(b.unit, 'in');
      assert.deepEqual(b.translation, [100, 0, 0]);
      assert.equal(b.bytes, 84 + 50 * 12);
      assert.ok(near(b.properties.volume, 254 ** 3, 1e-3));
      assert.equal(loaded.properties('final').volume, volume);
    } finally {
      doc.dispose();
      loaded?.dispose();
    }
  });

  test('distance between snapped mesh corners and to a body face', () => {
    const doc = modeling.createDocument();
    try {
      doc.addFeature({ type: 'Box', id: 'Box', parameters: { length: '20', width: '20', height: '20' } });
      doc.rebuild();
      const id = doc.addMesh({ fileName: 'cube.stl', data: binaryStl(cubeTriangles(10)), translation: [50, 0, 0] });
      const g = doc.meshGeometry(id);
      const a = { kind: 'mesh', mesh: id, triangle: triangleAt(g, [50, 0, 0]), point: [50.2, 0.1, 0.1], snap: 0.5 };
      const b = { kind: 'mesh', mesh: id, triangle: triangleAt(g, [60, 10, 10]), point: [59.9, 9.8, 9.9], snap: 0.5 };
      const d = doc.measure({ type: 'distance', a, b });
      assert.ok(near(d.value, 10 * Math.sqrt(3), 1e-9), `distance ${d.value}`);
      assert.equal(d.vertexA, true);
      assert.equal(d.vertexB, true);
      // Mesh corner (50, 0, 0) to the box face x = 20: 30 mm.
      const mesh = doc.tessellate('final', { linearDeflection: 0.05, angularDeflection: 0.2 });
      let faceX = -1;
      for (let s = 0; s < mesh.faceSpans.length; s += 3) {
        const v = mesh.indices[mesh.faceSpans[s + 1]] * 3;
        let all = true;
        for (let k = mesh.faceSpans[s + 1]; k < mesh.faceSpans[s + 1] + mesh.faceSpans[s + 2]; ++k) all &&= near(mesh.positions[mesh.indices[k] * 3], 20, 1e-6);
        if (all && near(mesh.positions[v], 20)) faceX = mesh.faceSpans[s];
      }
      assert.ok(faceX >= 0);
      const e = doc.measure({ type: 'distance', a, b: { kind: 'face', index: faceX } });
      assert.ok(near(e.value, 30, 1e-7), `distance ${e.value}`);
      assert.throws(() => doc.measure({ type: 'angle', a, b }), (x) => x.code === 'InvalidInput');
      assert.throws(() => doc.measure({ type: 'distance', a: { ...a, triangle: 99 }, b }), (x) => x.code === 'InvalidInput');
    } finally {
      doc.dispose();
    }
  });

  test('section to a sketch of construction lines', () => {
    const doc = modeling.createDocument();
    try {
      const id = doc.addMesh({ fileName: 'cube.stl', data: binaryStl(cubeTriangles(10)) });
      const sketch = doc.meshSectionSketch(id, 'XY', 4);
      doc.rebuild();
      const f = doc.features().find((x) => x.id === sketch);
      assert.ok(f, 'the sketch is in the tree');
      assert.equal(f.type, 'Sketch');
      assert.notEqual(f.state, 'error');
      assert.throws(() => doc.meshSectionSketch(id, 'XY', 40), (x) => x.code === 'InvalidInput');
    } finally {
      doc.dispose();
    }
  });

  test('a truncated file is refused with a clear message', () => {
    const doc = modeling.createDocument();
    try {
      const data = binaryStl(cubeTriangles(10)).slice(0, 500);
      assert.throws(
        () => doc.addMesh({ fileName: 'cut.stl', data }),
        (e) => e.code === 'ParseError' && /mesh 'cut\.stl': binary STL: the header declares 12 triangles/.test(e.detail),
      );
      assert.throws(() => doc.addMesh({ fileName: 'scan.obj', data }), (e) => e.code === 'InvalidInput');
      assert.equal(doc.meshes().length, 0);
    } finally {
      doc.dispose();
    }
  });
});

describe('assembly reference mesh', () => {
  test('not a part: no BOM rows; moved, undone, saved and loaded', { skip: !fs.existsSync(path.join(assemblyDir, 'bendcore_assembly.mjs')) }, () => {
    const doc = assembly.createDocument('Scan');
    let loaded = null;
    try {
      const id = doc.insertMesh({ fileName: 'cube.stl', data: binaryStl(cubeTriangles(10)) });
      let st = doc.state();
      assert.equal(st.referenceMeshes.length, 1);
      assert.equal(st.occurrences.length, 0);
      assert.equal(doc.bom('flat').rows.length, 0);
      doc.setMesh(id, { placement: { position: [0, 0, 100], axis: [0, 0, 1], angle: 90 } });
      st = doc.state();
      const p = st.referenceMeshes[0].properties;
      assert.ok(near(p.min[0], -10, 1e-9) && near(p.max[0], 0, 1e-9));
      assert.ok(near(p.min[2], 100, 1e-9));
      const s = doc.meshSection(id, { origin: [0, 0, 105], normal: [0, 0, 1] });
      assert.ok(near(s.area, 100, 1e-9));
      doc.undo();
      assert.ok(near(doc.state().referenceMeshes[0].properties.min[2], 0, 1e-9));
      doc.redo();
      const text = doc.save();
      assert.equal(JSON.parse(text).version, 4);
      loaded = assembly.loadDocument(text).document;
      const m = loaded.state().referenceMeshes[0];
      assert.equal(m.fileName, 'cube.stl');
      assert.ok(near(m.properties.min[2], 100, 1e-9));
      assert.ok(near(m.properties.volume, 1000, 1e-9));
      assert.throws(
        () => doc.insertMesh({ fileName: 'bad.stl', data: new TextEncoder().encode('solid x\nfacet normal 0 0 0\n') }),
        (e) => e.code === 'ParseError',
      );
    } finally {
      doc.dispose();
      loaded?.dispose();
    }
  });
});
