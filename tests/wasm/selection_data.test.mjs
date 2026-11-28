// Node check of the selection data and the assembly section of the WebAssembly modules
// (docs/ui/KERNEL_VIEWPORT.md, "Фильтры выбора и выбор рамкой"; docs/ui/ASSEMBLY_UI.md, "Сечение"):
// surface and curve types of every face and edge, the solid of every face, vertex positions and the
// section of an assembly by a plane. Values are compared with the analytic ones of a
// 100 x 60 x 20 box (6 planes, 12 straight edges, 8 vertices, one solid) and of a cylinder.
//
//   node tests/wasm/selection_data.test.mjs --modeling <dir> --kernel <dir> --assembly <dir>
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
const corpus = path.join(repo, 'tests/corpus/generated');
const MESH = { linearDeflection: 0.05, angularDeflection: 0.2 };
const NO_BODY = 0xffffffff;

const near = (a, b, tol = 1e-3) => Math.abs(a - b) <= tol;

async function load(dir, glueName, apiName, loader) {
  const glue = await import(pathToFileURL(path.join(dir, glueName)).href);
  const api = await import(pathToFileURL(path.join(dir, apiName)).href);
  return { api, instance: await api[loader](glue.default) };
}

const haveKernel = fs.existsSync(path.join(kernelDir, 'bendcore_kernel.mjs'));
const haveAssembly = fs.existsSync(path.join(assemblyDir, 'bendcore_assembly.mjs'));

let modeling;
let kernel = null;
let assembly = null;

before(async () => {
  modeling = (await load(modelingDir, 'bendcore_modeling.mjs', 'bendcore_modeling_api.mjs', 'loadModeling')).instance;
  if (haveKernel) kernel = (await load(kernelDir, 'bendcore_kernel.mjs', 'bendcore_kernel_api.mjs', 'loadKernel')).instance;
  if (haveAssembly) assembly = (await load(assemblyDir, 'bendcore_assembly.mjs', 'bendcore_assembly_api.mjs', 'loadAssembly')).instance;
});

after(() => {
  assert.equal(modeling.info().liveDocuments, 0, 'every document created by the tests is disposed');
});

function box(doc) {
  doc.addFeature({ type: 'Box', id: 'Box', parameters: { length: '100', width: '60', height: '20' } });
  doc.rebuild();
  return doc.tessellate('final', MESH);
}

const count = (list, value) => list.filter((x) => x === value).length;

describe('part mesh selection data', () => {
  test('box: 6 planes, 12 lines, 8 vertices, one solid', () => {
    const doc = modeling.createDocument();
    try {
      const mesh = box(doc);
      assert.equal(mesh.faceTypes.length, 6);
      assert.equal(count(mesh.faceTypes, 'Plane'), 6);
      assert.equal(mesh.edgeTypes.length, 12);
      assert.equal(count(mesh.edgeTypes, 'Line'), 12);
      assert.equal(mesh.vertices.length, 8 * 3);
      assert.equal(mesh.faceBodies.length, 6);
      assert.ok(Array.from(mesh.faceBodies).every((b) => b === 0), 'every face belongs to solid 0');
      // The type list is indexed like faceSpans: the face of the first span is entry faceSpans[0].
      assert.ok(mesh.faceSpans[0] < mesh.faceTypes.length);
    } finally {
      doc.dispose();
    }
  });
});

describe('kernel mesh selection data', { skip: !haveKernel }, () => {
  test('cylinder: planes, a cylinder, circles and a seam line', () => {
    const shape = kernel.readStep(fs.readFileSync(path.join(corpus, 'cylinder_r20_h50.step')));
    try {
      const mesh = shape.tessellate(MESH);
      assert.deepEqual(mesh.faceTypes, shape.faceTypes());
      assert.deepEqual(mesh.edgeTypes, shape.edgeTypes());
      assert.equal(count(mesh.faceTypes, 'Plane'), 2);
      assert.equal(count(mesh.faceTypes, 'Cylinder'), 1);
      assert.equal(count(mesh.edgeTypes, 'Circle'), 2);
      assert.ok(count(mesh.edgeTypes, 'Line') >= 1, 'the seam of the side face is a line');
      assert.equal(mesh.vertices.length, shape.counts().vertices * 3);
      assert.equal(mesh.faceBodies.length, shape.counts().faces);
    } finally {
      shape.dispose();
    }
  });

  test('two boxes are two solids, 6 faces each', () => {
    const shape = kernel.readStep(fs.readFileSync(path.join(corpus, 'multibody_two_boxes.step')));
    try {
      const mesh = shape.tessellate(MESH);
      assert.equal(shape.counts().solids, 2);
      assert.equal(mesh.faceBodies.length, 12);
      assert.deepEqual([...new Set(mesh.faceBodies)].sort(), [0, 1]);
      assert.equal(count(Array.from(mesh.faceBodies), 0), 6);
      assert.equal(count(Array.from(mesh.faceBodies), 1), 6);
    } finally {
      shape.dispose();
    }
  });

  test('box: 8 vertices at the corners', () => {
    const shape = kernel.readStep(fs.readFileSync(path.join(corpus, 'box_100x60x20.step')));
    try {
      const mesh = shape.tessellate(MESH);
      assert.equal(mesh.vertices.length, 24);
      for (let i = 0; i < 8; ++i) {
        const [x, y, z] = [mesh.vertices[i * 3], mesh.vertices[i * 3 + 1], mesh.vertices[i * 3 + 2]];
        assert.ok([0, 100].some((v) => near(x, v)) && [0, 60].some((v) => near(y, v)) && [0, 20].some((v) => near(z, v)), `corner ${i}`);
      }
    } finally {
      shape.dispose();
    }
  });
});

describe('assembly section and mesh types', { skip: !haveAssembly }, () => {
  // Two copies of the 100 x 60 x 20 box part; the second one is placed by `placement`.
  function build(placement) {
    const part = modeling.createDocument();
    let text;
    try {
      box(part);
      text = part.save({ includeCache: true });
    } finally {
      part.dispose();
    }
    const doc = assembly.createDocument('Pair');
    doc.insertPart({ name: 'Lower', format: 'part-json', data: text, fixed: true });
    doc.insertPart({ name: 'Upper', format: 'part-json', data: text, placement, fixed: true });
    return doc;
  }

  test('tessellation carries the face and edge types of every part mesh', () => {
    const doc = build({ position: [0, 0, 50] });
    try {
      const { meshes } = doc.tessellate();
      for (const m of Object.values(meshes)) {
        assert.equal(count(m.faceTypes, 'Plane'), 6);
        assert.equal(count(m.edgeTypes, 'Line'), 12);
      }
    } finally {
      doc.dispose();
    }
  });

  test('section of both components: 2 x 100 x 60 = 12000 mm2', () => {
    const doc = build({ position: [0, 0, 10] });
    try {
      // The upper box spans z 10..30, the lower one 0..20: the plane z = 15 cuts both.
      const both = doc.section({ origin: [50, 30, 15], normal: [0, 0, 1] });
      assert.ok(near(both.area, 12000, 1e-3), `area ${both.area}`);
      assert.equal(both.parts, 2);
      assert.ok(both.polylines.length >= 2, 'a contour per part');
      // z = 25 cuts only the upper box.
      const upper = doc.section({ origin: [50, 30, 25], normal: [0, 0, 1] });
      assert.ok(near(upper.area, 6000, 1e-3), `area ${upper.area}`);
      assert.equal(upper.parts, 1);
      // Above both: nothing.
      const none = doc.section({ origin: [50, 30, 100], normal: [0, 0, 1] });
      assert.equal(none.area, 0);
      assert.equal(none.parts, 0);
      assert.equal(none.polylines.length, 0);
      // A plane along X cuts both boxes: 60 x 20 each at x = 50 (the upper one is shifted in z only).
      const side = doc.section({ origin: [50, 30, 10], normal: [1, 0, 0] });
      assert.ok(near(side.area, 2 * 60 * 20, 1e-3), `area ${side.area}`);
    } finally {
      doc.dispose();
    }
  });

  test('a bad plane is InvalidInput', () => {
    const doc = build({ position: [0, 0, 10] });
    try {
      assert.throws(() => doc.section({ origin: [0, 0, 0], normal: [0, 0, 0] }), (e) => e.code === 'InvalidInput');
      assert.throws(() => doc.section({ origin: [0, 0, 0] }), (e) => e.code === 'InvalidInput');
    } finally {
      doc.dispose();
    }
  });
});
