// Screen-space edge picking for KernelViewport (docs/ui/PART_MODELING_UI.md, "Выбор рёбер").
//
// The edge buffers are the kernel's polylines (edgeBuffers.ts). Each segment is projected to
// client pixels by the caller's projection; the nearest segment within `tolerance` pixels wins.
// The caller rejects candidates hidden behind faces (depth test against a face ray hit).

import type { EdgeBuffers } from './edgeBuffers';

export interface ProjectedPoint {
  /** Client pixels. */
  x: number;
  y: number;
  /** false when the point is behind the camera. */
  visible: boolean;
}

export interface EdgeCandidate {
  edgeIndex: number;
  segment: number;
  /** Distance from the cursor to the projected segment, px. */
  distance: number;
  /** Parameter (0..1) of the closest point along the segment. */
  t: number;
  /** Closest point on the 3D segment (linear interpolation at t), model coordinates. */
  point: [number, number, number];
}

/**
 * Candidates closer than `tolerance` px to (px, py), nearest first; at most one per kernel edge.
 */
export function edgeCandidates(
  buffers: EdgeBuffers,
  project: (x: number, y: number, z: number) => ProjectedPoint,
  px: number,
  py: number,
  tolerance: number,
): EdgeCandidate[] {
  const { positions, indices, segmentEdges } = buffers;
  const projected = new Array<ProjectedPoint>(positions.length / 3);
  const at = (v: number): ProjectedPoint => {
    let p = projected[v];
    if (!p) {
      p = project(positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]);
      projected[v] = p;
    }
    return p;
  };
  const best = new Map<number, EdgeCandidate>();
  for (let s = 0; s < segmentEdges.length; ++s) {
    const ia = indices[s * 2];
    const ib = indices[s * 2 + 1];
    const a = at(ia);
    const b = at(ib);
    if (!a.visible || !b.visible) continue;
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    const len2 = dx * dx + dy * dy;
    let t = len2 > 0 ? ((px - a.x) * dx + (py - a.y) * dy) / len2 : 0;
    t = Math.min(1, Math.max(0, t));
    const distance = Math.hypot(a.x + t * dx - px, a.y + t * dy - py);
    if (distance > tolerance) continue;
    const edgeIndex = segmentEdges[s];
    const prev = best.get(edgeIndex);
    if (prev && prev.distance <= distance) continue;
    const point: [number, number, number] = [0, 0, 0];
    for (let c = 0; c < 3; ++c) {
      point[c] = positions[ia * 3 + c] + t * (positions[ib * 3 + c] - positions[ia * 3 + c]);
    }
    best.set(edgeIndex, { edgeIndex, segment: s, distance, t, point });
  }
  return [...best.values()].sort((x, y) => x.distance - y.distance);
}

type Vec3 = [number, number, number];

/**
 * Point of segment a-b closest to the ray origin + s·dir (s >= 0), and its ray parameter s
 * (the distance along the ray when dir is a unit vector). Used for the depth test of a picked
 * edge: under perspective the screen-space parameter of edgeCandidates is not the 3D one.
 */
export function closestPointOnSegmentToRay(
  origin: Vec3,
  dir: Vec3,
  a: Vec3,
  b: Vec3,
): { point: Vec3; rayDistance: number } {
  const u = [b[0] - a[0], b[1] - a[1], b[2] - a[2]];
  const w = [a[0] - origin[0], a[1] - origin[1], a[2] - origin[2]];
  const dot = (p: number[], q: number[]) => p[0] * q[0] + p[1] * q[1] + p[2] * q[2];
  const aa = dot(dir, dir);
  const bb = dot(dir, u);
  const cc = dot(u, u);
  const dd = dot(dir, w);
  const ee = dot(u, w);
  const denom = aa * cc - bb * bb;
  // Segment parameter of the closest pair of the two lines; a parallel ray takes the start.
  let t = denom > 1e-12 * aa * cc ? (bb * dd - aa * ee) / denom : 0;
  t = Math.min(1, Math.max(0, t));
  const point: Vec3 = [a[0] + t * u[0], a[1] + t * u[1], a[2] + t * u[2]];
  const s = Math.max(0, dot(dir, [point[0] - origin[0], point[1] - origin[1], point[2] - origin[2]]) / aa);
  return { point, rayDistance: s };
}
