// Snapping of the sketch editor (docs/ui/SKETCH_EDITOR.md, "Привязки"). Snap targets are taken
// from the kernel solve: solved points (curve ends, centres, standalone points), midpoints and
// intersections computed by the kernel, the sampled curves (nearest point on a curve), plus the
// sketch origin, horizontal/vertical alignment with a reference point and the grid.
// Priority: point > intersection > midpoint > origin > on-curve > horizontal/vertical > grid.

import type { SketchData, SketchSolution } from '../../../wasm/bendcore_modeling_api.mjs';

export type SnapKind =
  | 'endpoint'
  | 'center'
  | 'point'
  | 'intersection'
  | 'midpoint'
  | 'origin'
  | 'on-curve'
  | 'horizontal'
  | 'vertical'
  | 'grid'
  | 'free';

export interface Snap {
  x: number;
  y: number;
  kind: SnapKind;
  /** endpoint, center, point: index of the sketch point */
  point?: number;
  /** midpoint, on-curve: entity id */
  entity?: number;
  /** intersection: the two entity ids */
  entities?: [number, number];
}

export interface SnapOptions {
  /** capture radius in sketch units (pixels / scale) */
  tolerance: number;
  /** previous point of the tool: horizontal / vertical alignment with it */
  reference?: { x: number; y: number } | null;
  /** grid step in sketch units; 0 or undefined: no grid snap */
  grid?: number;
  /** point indices and entity ids that must not be snapped to (elements being dragged) */
  excludePoints?: ReadonlySet<number>;
  excludeEntities?: ReadonlySet<number>;
}

/** Role of every sketch point for snapping: curve end, centre, or other point. */
export function pointRoles(data: SketchData): ('endpoint' | 'center' | 'point')[] {
  const roles: ('endpoint' | 'center' | 'point')[] = data.points.map(() => 'point');
  for (const g of data.geometry) {
    if (g.center >= 0 && roles[g.center] === 'point') roles[g.center] = 'center';
  }
  for (const g of data.geometry) {
    if (g.start >= 0) roles[g.start] = 'endpoint';
    if (g.end >= 0) roles[g.end] = 'endpoint';
  }
  return roles;
}

/** Closest point of a polyline (x0, y0, x1, y1, ...) to (x, y). */
export function nearestOnPolyline(
  polyline: ArrayLike<number>,
  x: number,
  y: number,
): { x: number; y: number; distance: number } | null {
  let best: { x: number; y: number; distance: number } | null = null;
  for (let i = 0; i + 3 < polyline.length; i += 2) {
    const ax = polyline[i];
    const ay = polyline[i + 1];
    const dx = polyline[i + 2] - ax;
    const dy = polyline[i + 3] - ay;
    const len2 = dx * dx + dy * dy;
    const t = len2 > 0 ? Math.min(1, Math.max(0, ((x - ax) * dx + (y - ay) * dy) / len2)) : 0;
    const px = ax + t * dx;
    const py = ay + t * dy;
    const d = Math.hypot(x - px, y - py);
    if (!best || d < best.distance) best = { x: px, y: py, distance: d };
  }
  return best;
}

/** Entity whose curve passes nearest to (x, y) within the tolerance. */
export function nearestEntity(
  solution: SketchSolution,
  x: number,
  y: number,
  tolerance: number,
  exclude?: ReadonlySet<number>,
): { entity: number; x: number; y: number; distance: number } | null {
  let best: { entity: number; x: number; y: number; distance: number } | null = null;
  for (const c of solution.curves) {
    if (exclude?.has(c.entity)) continue;
    const n = nearestOnPolyline(c.polyline, x, y);
    if (n && n.distance <= tolerance && (!best || n.distance < best.distance)) best = { entity: c.entity, ...n };
  }
  return best;
}

/** Sketch point nearest to (x, y) within the tolerance. */
export function nearestPoint(
  solution: SketchSolution,
  x: number,
  y: number,
  tolerance: number,
  exclude?: ReadonlySet<number>,
): { point: number; distance: number } | null {
  let best: { point: number; distance: number } | null = null;
  solution.points.forEach((p, i) => {
    if (exclude?.has(i)) return;
    const d = Math.hypot(p.x - x, p.y - y);
    if (d <= tolerance && (!best || d < best.distance)) best = { point: i, distance: d };
  });
  return best;
}

export function findSnap(
  cursor: { x: number; y: number },
  data: SketchData,
  solution: SketchSolution | null,
  options: SnapOptions,
): Snap {
  const { x, y } = cursor;
  const tol = options.tolerance;
  const within = (px: number, py: number) => Math.hypot(px - x, py - y) <= tol;

  if (solution) {
    const p = nearestPoint(solution, x, y, tol, options.excludePoints);
    if (p) {
      const s = solution.points[p.point];
      return { x: s.x, y: s.y, kind: pointRoles(data)[p.point] ?? 'point', point: p.point };
    }
    let bestI: Snap | null = null;
    let bestD = Infinity;
    for (const i of solution.intersections) {
      if (options.excludeEntities?.has(i.a) || options.excludeEntities?.has(i.b)) continue;
      const d = Math.hypot(i.x - x, i.y - y);
      if (d <= tol && d < bestD) {
        bestD = d;
        bestI = { x: i.x, y: i.y, kind: 'intersection', entities: [i.a, i.b] };
      }
    }
    if (bestI) return bestI;
    for (const c of solution.curves) {
      if (options.excludeEntities?.has(c.entity) || !c.midpoint) continue;
      if (within(c.midpoint[0], c.midpoint[1])) return { x: c.midpoint[0], y: c.midpoint[1], kind: 'midpoint', entity: c.entity };
    }
  }
  if (within(0, 0)) return { x: 0, y: 0, kind: 'origin' };
  if (solution) {
    const e = nearestEntity(solution, x, y, tol, options.excludeEntities);
    if (e) return { x: e.x, y: e.y, kind: 'on-curve', entity: e.entity };
  }
  const ref = options.reference;
  if (ref) {
    const dx = Math.abs(x - ref.x);
    const dy = Math.abs(y - ref.y);
    // Alignment only away from the reference point itself, the nearer axis wins.
    if (dy <= tol && dx > tol && dy <= dx) return { x, y: ref.y, kind: 'horizontal' };
    if (dx <= tol && dy > tol) return { x: ref.x, y, kind: 'vertical' };
  }
  if (options.grid && options.grid > 0) {
    const g = options.grid;
    return { x: Math.round(x / g) * g, y: Math.round(y / g) * g, kind: 'grid' };
  }
  return { x, y, kind: 'free' };
}

/** Grid step (1, 2, 5 · 10^n mm) giving lines at least `minPixels` apart at `scale` px/mm. */
export function gridStep(scale: number, minPixels = 12): number {
  const raw = minPixels / scale;
  const pow = Math.pow(10, Math.floor(Math.log10(raw)));
  for (const m of [1, 2, 5, 10]) {
    if (m * pow >= raw) return m * pow;
  }
  return 10 * pow;
}
