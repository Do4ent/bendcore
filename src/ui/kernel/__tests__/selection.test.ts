import {
  boxBodies,
  boxEdges,
  boxFaces,
  boxKind,
  boxSelect,
  boxVertices,
  combineSelection,
  defaultFilter,
  edgeAllowed,
  faceAllowed,
  facesOfBodies,
  normalizeRect,
  segmentTouchesRect,
  selectOp,
  setMode,
  toggleEdgeType,
  toggleFaceType,
  triangleTouchesRect,
} from '../selection';
import type { BoxMesh, Projector } from '../selection';

// Cube 10 mm: 8 vertices, 6 faces (two triangles each), 12 edges (one segment each).
const V: [number, number, number][] = [
  [0, 0, 0], [10, 0, 0], [10, 10, 0], [0, 10, 0],
  [0, 0, 10], [10, 0, 10], [10, 10, 10], [0, 10, 10],
];
// faces: 0 bottom, 1 top, 2 front (y=0), 3 back, 4 left, 5 right
const QUADS = [
  [0, 3, 2, 1],
  [4, 5, 6, 7],
  [0, 1, 5, 4],
  [3, 7, 6, 2],
  [0, 4, 7, 3],
  [1, 2, 6, 5],
];
const EDGE_PAIRS = [
  [0, 1], [1, 2], [2, 3], [3, 0],
  [4, 5], [5, 6], [6, 7], [7, 4],
  [0, 4], [1, 5], [2, 6], [3, 7],
];

function cube(): BoxMesh {
  const positions: number[] = V.flat();
  const indices: number[] = [];
  const spans: number[] = [];
  QUADS.forEach((q, f) => {
    spans.push(f, indices.length, 6);
    indices.push(q[0], q[1], q[2], q[0], q[2], q[3]);
  });
  return {
    positions,
    indices,
    faceSpans: spans,
    edges: EDGE_PAIRS.map(([a, b], i) => ({ edgeIndex: i, points: [...V[a], ...V[b]] })),
    vertices: positions,
    faceTypes: ['Plane', 'Plane', 'Plane', 'Cylinder', 'Plane', 'Cylinder'],
    edgeTypes: ['Line', 'Line', 'Line', 'Line', 'Circle', 'Circle', 'Line', 'Line', 'Line', 'Line', 'Line', 'Line'],
    faceBodies: [0, 0, 0, 0, 0, 0],
  };
}

// Oblique orthographic view: every face has a non-zero projected area. 1 mm = 10 px.
const project: Projector = (x, y, z) => ({ x: 10 * (x + 0.3 * z), y: 10 * (y + 0.5 * z), visible: true });

describe('filters', () => {
  test('empty type list lets every face and edge through', () => {
    const f = defaultFilter();
    expect(faceAllowed(f, 'Plane')).toBe(true);
    expect(faceAllowed(f, 'BSpline')).toBe(true);
    expect(edgeAllowed(f, 'Ellipse')).toBe(true);
  });

  test('a face type filter keeps only the named types', () => {
    const f = toggleFaceType(defaultFilter(), 'Plane');
    expect(faceAllowed(f, 'Plane')).toBe(true);
    expect(faceAllowed(f, 'Cylinder')).toBe(false);
    expect(faceAllowed(f, 'Cone')).toBe(false);
    expect(faceAllowed(f, undefined)).toBe(false);
    const both = toggleFaceType(f, 'Cylinder');
    expect(faceAllowed(both, 'Cylinder')).toBe(true);
    expect(faceAllowed(toggleFaceType(both, 'Plane'), 'Plane')).toBe(false);
    expect(toggleFaceType(toggleFaceType(f, 'Cylinder'), 'Cylinder').faceTypes).toEqual(['Plane']);
  });

  test('an edge type filter keeps only straight or circular edges', () => {
    const f = toggleEdgeType(setMode(defaultFilter(), 'edge'), 'Circle');
    expect(f.mode).toBe('edge');
    expect(edgeAllowed(f, 'Circle')).toBe(true);
    expect(edgeAllowed(f, 'Line')).toBe(false);
  });
});

describe('select operation', () => {
  test('modifiers', () => {
    expect(selectOp({})).toBe('replace');
    expect(selectOp({ shiftKey: true })).toBe('add');
    expect(selectOp({ ctrlKey: true })).toBe('remove');
    expect(selectOp({ metaKey: true })).toBe('remove');
    expect(selectOp({ shiftKey: true, ctrlKey: true })).toBe('remove');
  });

  test('combine', () => {
    expect(combineSelection([1, 2], [3, 3], 'replace')).toEqual([3]);
    expect(combineSelection([1, 2], [2, 3], 'add')).toEqual([1, 2, 3]);
    expect(combineSelection([1, 2, 3], [2, 9], 'remove')).toEqual([1, 3]);
  });
});

describe('rectangle geometry', () => {
  const r = normalizeRect({ x: 10, y: 10 }, { x: 0, y: 0 });
  test('direction and normalisation', () => {
    expect(r).toEqual({ x0: 0, y0: 0, x1: 10, y1: 10 });
    expect(boxKind({ x: 0 }, { x: 5 })).toBe('inside');
    expect(boxKind({ x: 5 }, { x: 0 })).toBe('cross');
  });
  test('segment and triangle against the rectangle', () => {
    expect(segmentTouchesRect({ x: -5, y: 5 }, { x: 15, y: 5 }, r)).toBe(true);
    expect(segmentTouchesRect({ x: -5, y: 20 }, { x: 15, y: 20 }, r)).toBe(false);
    expect(segmentTouchesRect({ x: -5, y: 5 }, { x: -1, y: 5 }, r)).toBe(false);
    // a big triangle that contains the rectangle without any corner or edge inside it
    expect(triangleTouchesRect({ x: -100, y: -100 }, { x: 200, y: -100 }, { x: 50, y: 300 }, r)).toBe(true);
    expect(triangleTouchesRect({ x: 20, y: 20 }, { x: 30, y: 20 }, { x: 25, y: 30 }, r)).toBe(false);
  });
});

describe('box selection of a cube', () => {
  const mesh = cube();

  test('left to right takes only faces entirely inside', () => {
    // the whole cube projects to x 0..130, y 0..150
    const all = normalizeRect({ x: -5, y: -5 }, { x: 140, y: 160 });
    expect(boxFaces(mesh, project, all, 'inside')).toEqual([0, 1, 2, 3, 4, 5]);
    // x 0..100: the bottom face (z=0) is entirely inside, the top (shifted by 30) is not
    const part = normalizeRect({ x: -5, y: -5 }, { x: 105, y: 105 });
    expect(boxFaces(mesh, project, part, 'inside')).toEqual([0]);
  });

  test('right to left takes every face the rectangle touches', () => {
    // a small rectangle at the projected corner (0, 0): only faces with that corner
    const corner = normalizeRect({ x: -2, y: -2 }, { x: 2, y: 2 });
    expect(boxFaces(mesh, project, corner, 'cross')).toEqual([0, 2, 4]);
    // a rectangle inside the projected top face touches the top (and what overlaps it on screen)
    const mid = normalizeRect({ x: 80, y: 90 }, { x: 82, y: 92 });
    expect(boxFaces(mesh, project, mid, 'cross').includes(1)).toBe(true);
    expect(boxFaces(mesh, project, mid, 'inside')).toEqual([]);
  });

  test('crossing takes a superset of inside', () => {
    const rect = normalizeRect({ x: -5, y: -5 }, { x: 105, y: 105 });
    const inside = boxFaces(mesh, project, rect, 'inside');
    const cross = boxFaces(mesh, project, rect, 'cross');
    for (const f of inside) expect(cross).toContain(f);
    expect(cross.length).toBeGreaterThan(inside.length);
  });

  test('the type filter applies to the box', () => {
    const all = normalizeRect({ x: -5, y: -5 }, { x: 140, y: 160 });
    const planes = toggleFaceType(defaultFilter(), 'Plane');
    expect(boxSelect(mesh, project, all, 'inside', planes).faces).toEqual([0, 1, 2, 4]);
    const cyl = toggleFaceType(defaultFilter(), 'Cylinder');
    expect(boxSelect(mesh, project, all, 'cross', cyl).faces).toEqual([3, 5]);
  });

  test('points behind the camera are never inside', () => {
    const hidden: Projector = (x, y, z) => ({ ...project(x, y, z), visible: z < 5 });
    const all = normalizeRect({ x: -5, y: -5 }, { x: 140, y: 160 });
    expect(boxFaces(mesh, hidden, all, 'inside')).toEqual([0]);
  });

  test('edges: inside needs the whole polyline, cross a touched segment', () => {
    const part = normalizeRect({ x: -5, y: -5 }, { x: 105, y: 105 });
    // the bottom face edges 0..3 and the edge 8 (vertex 4 projects to 30, 50) are inside
    expect(boxEdges(mesh, project, part, 'inside')).toEqual([0, 1, 2, 3, 8]);
    const corner = normalizeRect({ x: -2, y: -2 }, { x: 2, y: 2 });
    expect(boxEdges(mesh, project, corner, 'cross')).toEqual([0, 3, 8]);
    const circles = toggleEdgeType(setMode(defaultFilter(), 'edge'), 'Circle');
    const all = normalizeRect({ x: -5, y: -5 }, { x: 140, y: 160 });
    expect(boxSelect(mesh, project, all, 'inside', circles).edges).toEqual([4, 5]);
  });

  test('vertices in the rectangle, both directions alike', () => {
    const part = normalizeRect({ x: -5, y: -5 }, { x: 105, y: 105 });
    expect(boxVertices(mesh, project, part)).toEqual([0, 1, 2, 3, 4]);
    expect(boxSelect(mesh, project, part, 'cross', setMode(defaultFilter(), 'vertex')).vertices).toEqual([0, 1, 2, 3, 4]);
  });

  test('bodies: inside needs every face, cross any', () => {
    const part = normalizeRect({ x: -5, y: -5 }, { x: 105, y: 105 });
    expect(boxBodies(mesh, project, part, 'inside')).toEqual([]);
    expect(boxBodies(mesh, project, part, 'cross')).toEqual([0]);
    const all = normalizeRect({ x: -5, y: -5 }, { x: 140, y: 160 });
    expect(boxBodies(mesh, project, all, 'inside')).toEqual([0]);
    expect(facesOfBodies(mesh.faceBodies, [0])).toEqual([0, 1, 2, 3, 4, 5]);
  });
});
