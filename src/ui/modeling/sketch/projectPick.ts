// Picking of model edges and faces in the sketch editor view (docs/ui/SKETCH_EDITOR.md,
// "Проекция рёбер модели"). The view looks at the sketch plane along -normal, so the body mesh
// (in model coordinates) is moved into the sketch frame: local = Rᵀ (world − origin). An edge
// polyline within the tolerance of the click wins (the nearest); otherwise the face whose
// triangle under the click is closest to the viewer (largest local z).

import type { KernelMesh } from '../../../wasm/bendcore_kernel_api.mjs';
import type { SketchPlacementInfo } from '../../../wasm/bendcore_modeling_api.mjs';

export interface BodyPick {
  kind: 'edge' | 'face';
  /** edge or face index of the body (target before:<sketch>) */
  index: number;
}

type V3 = [number, number, number];

/** Model point -> sketch frame (x, y in the plane, z along the normal). */
export function toSketchFrame(p: SketchPlacementInfo): (x: number, y: number, z: number) => V3 {
  const [ox, oy, oz] = p.origin;
  const dot = (d: V3, x: number, y: number, z: number) => d[0] * (x - ox) + d[1] * (y - oy) + d[2] * (z - oz);
  return (x, y, z) => [dot(p.xDirection, x, y, z), dot(p.yDirection, x, y, z), dot(p.normal, x, y, z)];
}

function segmentDistance(px: number, py: number, ax: number, ay: number, bx: number, by: number): number {
  const dx = bx - ax;
  const dy = by - ay;
  const l2 = dx * dx + dy * dy;
  const t = l2 > 0 ? Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / l2)) : 0;
  return Math.hypot(px - (ax + t * dx), py - (ay + t * dy));
}

/** Nearest edge within `tolerance` (sketch mm) of (x, y), else the front face under (x, y). */
export function pickBodyElement(body: KernelMesh, placement: SketchPlacementInfo, x: number, y: number, tolerance: number): BodyPick | null {
  const local = toSketchFrame(placement);
  let bestEdge: { index: number; d: number } | null = null;
  for (const e of body.edges) {
    const p = e.points;
    let prev: V3 | null = null;
    for (let i = 0; i + 2 < p.length; i += 3) {
      const q = local(p[i], p[i + 1], p[i + 2]);
      const d = prev ? segmentDistance(x, y, prev[0], prev[1], q[0], q[1]) : Math.hypot(x - q[0], y - q[1]);
      if (d <= tolerance && (!bestEdge || d < bestEdge.d)) bestEdge = { index: e.edgeIndex, d };
      prev = q;
    }
  }
  if (bestEdge) return { kind: 'edge', index: bestEdge.index };

  let bestFace: { index: number; z: number } | null = null;
  const pos = body.positions;
  const spans = body.faceSpans;
  for (let s = 0; s + 2 < spans.length; s += 3) {
    const face = spans[s];
    const first = spans[s + 1];
    const count = spans[s + 2];
    for (let t = first; t + 2 < first + count; t += 3) {
      const v = [0, 1, 2].map((k) => {
        const i = body.indices[t + k] * 3;
        return local(pos[i], pos[i + 1], pos[i + 2]);
      });
      const [a, b, c] = v;
      const den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]);
      if (Math.abs(den) < 1e-12) continue; // triangle seen edge-on
      const l1 = ((b[1] - c[1]) * (x - c[0]) + (c[0] - b[0]) * (y - c[1])) / den;
      const l2 = ((c[1] - a[1]) * (x - c[0]) + (a[0] - c[0]) * (y - c[1])) / den;
      const l3 = 1 - l1 - l2;
      if (l1 < -1e-9 || l2 < -1e-9 || l3 < -1e-9) continue;
      const z = l1 * a[2] + l2 * b[2] + l3 * c[2];
      if (!bestFace || z > bestFace.z) bestFace = { index: face, z };
    }
  }
  return bestFace ? { kind: 'face', index: bestFace.index } : null;
}
