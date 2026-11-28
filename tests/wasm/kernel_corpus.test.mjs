// Node check of bendcore_kernel_wasm against the generated STEP corpus
// (docs/ui/KERNEL_VIEWPORT.md). Expected values come from tests/corpus/generated/manifest.json,
// where they are computed by formulas from the model dimensions, not measured by OCCT.
//
//   node tests/wasm/kernel_corpus.test.mjs --module <dir with bendcore_kernel.mjs> --corpus <dir>
//
// Registered in ctest as wasm_kernel_corpus (cmake/wasm_kernel.cmake).

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import { after, before, describe, test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../..');

function argument(name, fallback) {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 && i + 1 < process.argv.length ? path.resolve(process.argv[i + 1]) : fallback;
}

const moduleDir = argument('module', path.join(repo, 'build-wasm-kernel/wasm_kernel'));
const corpusDir = argument('corpus', path.join(repo, 'tests/corpus/generated'));
const manifest = JSON.parse(readFileSync(path.join(corpusDir, 'manifest.json'), 'utf8'));

// Surface types of the faces by construction of each model (tests/corpus/generate_corpus.cpp):
// counts per type, independent of the kernel's face order.
const expectedSurfaceTypes = {
  box_100x60x20: { Plane: 6 },
  cylinder_r20_h50: { Plane: 2, Cylinder: 1 },
  tube_R30_r10_h40: { Plane: 2, Cylinder: 2 },
  plate_120x80x10_hole_d20: { Plane: 6, Cylinder: 1 },
  torus_R50_r10: { Torus: 1 },
  sphere_R25: { Sphere: 1 },
  frustum_R30_R10_h40: { Plane: 2, Cone: 1 },
  l_bracket_60x40x8_w30: { Plane: 8 },
  multibody_two_boxes: { Plane: 12 },
};

// Radii of the curved faces by construction.
const expectedRadii = {
  cylinder_r20_h50: { Cylinder: [20] },
  tube_R30_r10_h40: { Cylinder: [10, 30] },
  plate_120x80x10_hole_d20: { Cylinder: [10] },
  sphere_R25: { Sphere: [25] },
};

function relClose(actual, expected, rel, what) {
  const err = Math.abs(actual - expected) / Math.abs(expected);
  assert.ok(err <= rel, `${what}: ${actual} vs ${expected} (relative error ${err.toExponential(2)} > ${rel})`);
}

function histogram(values) {
  const out = {};
  for (const v of values) out[v] = (out[v] ?? 0) + 1;
  return out;
}

// Signed volume enclosed by the triangles (divergence theorem); positive and equal to the
// solid volume only if the mesh is closed and every triangle is wound outwards.
function meshVolume(mesh) {
  const p = mesh.positions;
  const ix = mesh.indices;
  let v = 0;
  for (let t = 0; t < ix.length; t += 3) {
    const a = ix[t] * 3;
    const b = ix[t + 1] * 3;
    const c = ix[t + 2] * 3;
    v +=
      p[a] * (p[b + 1] * p[c + 2] - p[b + 2] * p[c + 1]) -
      p[a + 1] * (p[b] * p[c + 2] - p[b + 2] * p[c]) +
      p[a + 2] * (p[b] * p[c + 1] - p[b + 1] * p[c]);
  }
  return v / 6;
}

let api;
let kernel;

before(async () => {
  const glue = await import(pathToFileURL(path.join(moduleDir, 'bendcore_kernel.mjs')).href);
  api = await import(pathToFileURL(path.join(moduleDir, 'bendcore_kernel_api.mjs')).href);
  kernel = await api.loadKernel(glue.default);
});

after(() => {
  assert.equal(kernel.info().liveShapes, 0, 'every shape created by the tests is disposed');
});

describe('STEP corpus', () => {
  for (const model of manifest.models) {
    test(model.name, () => {
      const bytes = new Uint8Array(readFileSync(path.join(corpusDir, model.file)));
      const shape = kernel.readStep(bytes);
      try {
        const counts = shape.counts();
        assert.equal(counts.faces, model.faces, 'face count');
        assert.equal(counts.solids, model.solids, 'solid count');

        relClose(shape.volumeProperties().volume, model.volume, model.volume_rel_tol, 'volume');
        relClose(shape.surfaceProperties().area, model.area, model.area_rel_tol, 'area');

        const box = shape.boundingBox();
        for (let i = 0; i < 3; ++i) {
          assert.ok(Math.abs(box.min[i] - model.bbox_min[i]) <= model.bbox_abs_tol, `bbox min[${i}] ${box.min[i]}`);
          assert.ok(Math.abs(box.max[i] - model.bbox_max[i]) <= model.bbox_abs_tol, `bbox max[${i}] ${box.max[i]}`);
        }

        const types = shape.faceTypes();
        assert.equal(types.length, model.faces);
        if (expectedSurfaceTypes[model.name]) {
          assert.deepEqual(histogram(types), expectedSurfaceTypes[model.name], 'surface types');
        }
        assert.equal(shape.edgeTypes().length, counts.edges);

        // Per-face areas add up to the analytic total area.
        let faceAreaSum = 0;
        for (let i = 0; i < model.faces; ++i) {
          const face = shape.face(i);
          assert.equal(face.index, i);
          assert.equal(face.surfaceType, types[i]);
          faceAreaSum += face.area;
        }
        relClose(faceAreaSum, model.area, 1e-6, 'sum of face areas');

        for (const [type, radii] of Object.entries(expectedRadii[model.name] ?? {})) {
          const found = [];
          for (let i = 0; i < model.faces; ++i) {
            if (types[i] === type) found.push(shape.face(i).parameters.radius);
          }
          found.sort((a, b) => a - b);
          assert.equal(found.length, radii.length, `${type} faces`);
          found.forEach((r, k) => relClose(r, radii[k], 1e-9, `${type} radius`));
        }

        const mesh = shape.tessellate({ linearDeflection: 0.01, angularDeflection: 0.1 });
        assert.ok(mesh.positions instanceof Float32Array && mesh.normals instanceof Float32Array);
        assert.ok(mesh.indices instanceof Uint32Array && mesh.faceSpans instanceof Uint32Array);
        assert.equal(mesh.normals.length, mesh.positions.length);
        assert.equal(mesh.faceSpans.length / 3, model.faces, 'one span per face');
        let next = 0;
        for (let s = 0; s < mesh.faceSpans.length; s += 3) {
          assert.equal(mesh.faceSpans[s], s / 3, 'spans in face order');
          assert.equal(mesh.faceSpans[s + 1], next, 'spans are contiguous');
          assert.ok(mesh.faceSpans[s + 2] > 0 && mesh.faceSpans[s + 2] % 3 === 0);
          next += mesh.faceSpans[s + 2];
        }
        assert.equal(next, mesh.indices.length, 'spans cover all indices');
        const vertexCount = mesh.positions.length / 3;
        assert.ok(mesh.indices.every((i) => i < vertexCount), 'indices within vertex count');
        assert.equal(mesh.missingFaces.length, 0);
        assert.ok(mesh.edges.length > 0 && mesh.edges.every((e) => e.points.length >= 6 && e.points.length % 3 === 0));
        // Chordal error of the mesh is bounded by the deflection: 0.5 % of the analytic volume.
        relClose(meshVolume(mesh), model.volume, 5e-3, 'mesh volume');

        // faceOfTriangle agrees with the spans for the first and last triangle of every face.
        for (let s = 0; s < mesh.faceSpans.length; s += 3) {
          const first = mesh.faceSpans[s + 1] / 3;
          const last = first + mesh.faceSpans[s + 2] / 3 - 1;
          assert.equal(api.faceOfTriangle(mesh.faceSpans, first), mesh.faceSpans[s]);
          assert.equal(api.faceOfTriangle(mesh.faceSpans, last), mesh.faceSpans[s]);
        }
        assert.equal(api.faceOfTriangle(mesh.faceSpans, mesh.indices.length / 3), -1);
      } finally {
        shape.dispose();
      }
    });
  }
});

describe('face parameters', () => {
  test('box_100x60x20: plane normals point out of the material', () => {
    const shape = kernel.readStep(new Uint8Array(readFileSync(path.join(corpusDir, 'box_100x60x20.step'))));
    try {
      const center = [50, 30, 10];
      const areas = {};
      for (let i = 0; i < 6; ++i) {
        const face = shape.face(i);
        const n = face.parameters.normal;
        const axis = n.findIndex((c) => Math.abs(Math.abs(c) - 1) < 1e-12);
        assert.ok(axis >= 0, `normal ${n} is axis-aligned`);
        const outward = (face.centroid[axis] - center[axis]) * n[axis];
        assert.ok(outward > 0, `face ${i} normal ${n} points outwards`);
        areas[`${n[axis] > 0 ? '+' : '-'}${'xyz'[axis]}`] = face.area;
      }
      const expected = { '+x': 1200, '-x': 1200, '+y': 2000, '-y': 2000, '+z': 6000, '-z': 6000 };
      for (const [k, v] of Object.entries(expected)) relClose(areas[k], v, 1e-9, `face ${k} area`);
    } finally {
      shape.dispose();
    }
  });

  test('frustum_R30_R10_h40: cone semi-angle atan((R-r)/h)', () => {
    const shape = kernel.readStep(new Uint8Array(readFileSync(path.join(corpusDir, 'frustum_R30_R10_h40.step'))));
    try {
      const i = shape.faceTypes().indexOf('Cone');
      const p = shape.face(i).parameters;
      relClose(Math.abs(p.semiAngle), Math.atan((30 - 10) / 40), 1e-9, 'semi-angle');
    } finally {
      shape.dispose();
    }
  });
});

describe('errors and lifetime', () => {
  test('garbage input is a ParseError', () => {
    const bytes = new TextEncoder().encode('this is not a STEP file');
    assert.throws(() => kernel.readStep(bytes), (e) => e instanceof api.KernelError && e.code === 'ParseError');
  });

  test('empty input is a ParseError', () => {
    assert.throws(() => kernel.readStep(new Uint8Array(0)), (e) => e.code === 'ParseError');
  });

  test('non-Uint8Array input is InvalidInput', () => {
    assert.throws(() => kernel.readStep('ISO-10303-21;'), (e) => e.code === 'InvalidInput');
  });

  test('calls after dispose throw Disposed; dispose is idempotent', () => {
    const shape = kernel.readStep(new Uint8Array(readFileSync(path.join(corpusDir, 'sphere_R25.step'))));
    const before = kernel.info().liveShapes;
    shape.dispose();
    assert.equal(kernel.info().liveShapes, before - 1);
    assert.equal(shape.disposed, true);
    assert.throws(() => shape.counts(), (e) => e.code === 'Disposed');
    shape.dispose();
  });

  test('face index out of range is InvalidInput', () => {
    const shape = kernel.readStep(new Uint8Array(readFileSync(path.join(corpusDir, 'cylinder_r20_h50.step'))));
    try {
      assert.throws(() => shape.face(3), (e) => e.code === 'InvalidInput' && /out of range/.test(e.message));
      assert.throws(() => shape.face(-1), (e) => e.code === 'InvalidInput');
    } finally {
      shape.dispose();
    }
  });

  test('non-positive deflection is InvalidInput from the kernel', () => {
    const shape = kernel.readStep(new Uint8Array(readFileSync(path.join(corpusDir, 'box_100x60x20.step'))));
    try {
      assert.throws(() => shape.tessellate({ linearDeflection: 0 }), (e) => e.code === 'InvalidInput');
    } finally {
      shape.dispose();
    }
  });
});
