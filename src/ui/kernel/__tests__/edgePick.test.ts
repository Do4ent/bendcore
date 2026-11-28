import { packEdgePolylines } from '../edgeBuffers';
import { closestPointOnSegmentToRay, edgeCandidates } from '../edgePick';

// Orthographic projection onto the XY plane, 10 px per mm; z ignored.
const project = (x: number, y: number) => ({ x: x * 10, y: y * 10, visible: true });

describe('edgeCandidates', () => {
  const buffers = packEdgePolylines([
    { edgeIndex: 3, points: new Float32Array([0, 0, 0, 10, 0, 0]) }, // along X at y = 0
    { edgeIndex: 8, points: new Float32Array([0, 1, 0, 5, 1, 0, 10, 1, 5]) }, // y = 1, two segments
  ]);

  it('returns the nearest edge first with the closest 3D point', () => {
    const c = edgeCandidates(buffers, project, 40, 2, 6);
    expect(c.map((e) => e.edgeIndex)).toEqual([3]);
    expect(c[0].distance).toBeCloseTo(2, 12);
    expect(c[0].point[0]).toBeCloseTo(4, 6);
    const both = edgeCandidates(buffers, project, 40, 6, 6);
    expect(both.map((e) => e.edgeIndex)).toEqual([8, 3]);
    expect(both[0].distance).toBeCloseTo(4, 12);
  });

  it('keeps one candidate per edge (its nearest segment) and interpolates z', () => {
    const c = edgeCandidates(buffers, project, 75, 10, 3);
    expect(c).toHaveLength(1);
    expect(c[0].edgeIndex).toBe(8);
    expect(c[0].segment).toBe(2);
    expect(c[0].point[2]).toBeCloseTo(2.5, 6);
  });

  it('clamps to segment ends and rejects far or invisible points', () => {
    expect(edgeCandidates(buffers, project, 105, 0, 6).map((e) => e.edgeIndex)).toEqual([3]);
    expect(edgeCandidates(buffers, project, 120, 0, 6)).toEqual([]);
    const hidden = (x: number, y: number) => ({ x: x * 10, y: y * 10, visible: false });
    expect(edgeCandidates(buffers, hidden, 40, 0, 6)).toEqual([]);
  });
});

describe('closestPointOnSegmentToRay', () => {
  it('finds where a ray crosses the segment and the distance along the ray', () => {
    // Ray from (0, -10, 5) along +y crosses the vertical segment x = y = 0, z in [0, 20] at z = 5.
    const r = closestPointOnSegmentToRay([0, -10, 5], [0, 1, 0], [0, 0, 0], [0, 0, 20]);
    expect(r.point).toEqual([0, 0, 5]);
    expect(r.rayDistance).toBeCloseTo(10, 12);
    // Oblique ray (perspective): the 3D parameter, not the screen one.
    const d = Math.SQRT1_2;
    const o = closestPointOnSegmentToRay([-10, -10, 15], [d, d, 0], [0, 0, 0], [0, 0, 20]);
    expect(o.point[2]).toBeCloseTo(15, 12);
    expect(o.rayDistance).toBeCloseTo(Math.hypot(10, 10), 12);
  });

  it('clamps to the segment end and takes the start for a parallel ray', () => {
    expect(closestPointOnSegmentToRay([0, -10, 30], [0, 1, 0], [0, 0, 0], [0, 0, 20]).point).toEqual([0, 0, 20]);
    const p = closestPointOnSegmentToRay([1, 0, 0], [0, 0, 1], [0, 0, 0], [0, 0, 20]);
    expect(p.point).toEqual([0, 0, 0]);
    expect(p.rayDistance).toBe(0);
  });
});
