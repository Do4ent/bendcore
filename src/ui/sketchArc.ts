/**
 * Arc of a 2D drawing sketch given SVG-style: endpoints, radius, large-arc and sweep flags
 * (the same parametrisation as drawing::make_sketch_arc in src/drawing.cpp).
 *
 * Centre and angles follow the endpoint-to-centre conversion of the SVG 1.1 specification,
 * appendix F.6.5 (rotation 0); a radius too small for the chord is scaled up (F.6.6).
 * Angles are in the sketch's screen coordinates (y down), as used by CanvasRenderingContext2D.
 */
export interface ArcGeometry {
  cx: number;
  cy: number;
  r: number;
  /** Angle of the first endpoint, radians. */
  start: number;
  /** Signed sweep from start to the second endpoint, radians (|delta| <= 2*pi). */
  delta: number;
}

const TWO_PI = 2 * Math.PI;

function vectorAngle(ux: number, uy: number, vx: number, vy: number): number {
  return Math.atan2(ux * vy - uy * vx, ux * vx + uy * vy);
}

export function arcFromEndpoints(
  x1: number,
  y1: number,
  x2: number,
  y2: number,
  radius: number,
  large: boolean,
  sweep: boolean,
): ArcGeometry | null {
  const hx = (x1 - x2) / 2;
  const hy = (y1 - y2) / 2;
  if (hx === 0 && hy === 0) return null; // coincident endpoints: no arc (SVG F.6.2)
  let r = Math.abs(radius);
  if (r === 0) return null;
  const lambda = (hx * hx + hy * hy) / (r * r);
  if (lambda > 1) r *= Math.sqrt(lambda);
  const num = r * r - hx * hx - hy * hy;
  const coef = (large !== sweep ? 1 : -1) * Math.sqrt(Math.max(0, num / (hx * hx + hy * hy)));
  const cxp = coef * hy;
  const cyp = -coef * hx;
  const cx = cxp + (x1 + x2) / 2;
  const cy = cyp + (y1 + y2) / 2;
  const ux = (hx - cxp) / r;
  const uy = (hy - cyp) / r;
  const vx = (-hx - cxp) / r;
  const vy = (-hy - cyp) / r;
  const start = Math.atan2(uy, ux);
  let delta = vectorAngle(ux, uy, vx, vy);
  if (!sweep && delta > 0) delta -= TWO_PI;
  if (sweep && delta < 0) delta += TWO_PI;
  return { cx, cy, r, start, delta };
}

/** True if (px, py) lies within `tolerance` of the arc (not of the full circle). */
export function hitTestArc(px: number, py: number, arc: ArcGeometry, tolerance: number): boolean {
  const d = Math.hypot(px - arc.cx, py - arc.cy);
  if (Math.abs(d - arc.r) >= tolerance) return false;
  const a = Math.atan2(py - arc.cy, px - arc.cx);
  const along = arc.delta >= 0 ? a - arc.start : arc.start - a;
  const t = ((along % TWO_PI) + TWO_PI) % TWO_PI;
  // Allow the tolerance band around both endpoints as well.
  const slack = tolerance / arc.r;
  return t <= Math.abs(arc.delta) + slack || t >= TWO_PI - slack;
}
