// Annotation of sketch dimensions (docs/ui/SKETCH_EDITOR.md, "Размеры"): where the dimension
// line, the extension lines and the text of a dimension constraint go, from the solved points
// and curves of the kernel. The text position is the default anchor plus the user's offset
// (dragging the text). Also the choice of the dimension type from two picked points and the
// place of the text (smart dimension).

import type { SketchConstraintDef, SketchData, SketchSolution } from '../../../wasm/bendcore_modeling_api.mjs';

export interface DimensionGraphic {
  /** constraint number (1-based) */
  constraint: number;
  /** segments x1, y1, x2, y2 in sketch coordinates */
  lines: [number, number, number, number][];
  text: { x: number; y: number };
  label: string;
}

type V = { x: number; y: number };

export function formatValue(v: number): string {
  if (!Number.isFinite(v)) return String(v);
  return String(Number(v.toFixed(3)));
}

/** Text of a dimension: value with the type mark; reference dimensions in parentheses. */
export function dimensionLabel(c: SketchConstraintDef, value: number): string {
  const v = formatValue(c.type === 'distance-x' || c.type === 'distance-y' ? Math.abs(value) : value);
  const body =
    c.type === 'radius'
      ? `R${v}`
      : c.type === 'diameter'
        ? `⌀${v}`
        : c.type === 'angle' || c.type === 'rotation'
          ? `${v}°`
          : c.type === 'arc-length'
            ? `⌒${v}`
            : c.type === 'major-radius'
              ? `a ${v}`
              : c.type === 'minor-radius'
                ? `b ${v}`
                : v;
  return c.driven ? `(${body})` : body;
}

const mid = (a: V, b: V): V => ({ x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 });

/**
 * Graphic of dimension constraint number k. `gap` is the default distance of the text from the
 * measured geometry (sketch units), `offset` the user's displacement of the text.
 */
export function dimensionGraphic(
  data: SketchData,
  solution: SketchSolution,
  k: number,
  gap: number,
  offset: V = { x: 0, y: 0 },
): DimensionGraphic | null {
  const c = data.constraints[k - 1];
  const value = solution.constraintValues[k - 1];
  if (!c || value === undefined) return null;
  const P = (i: number): V | null => solution.points[i] ?? null;
  const curve = (id: number) => solution.curves[id - 1] ?? null;
  const label = dimensionLabel(c, value);
  const done = (lines: DimensionGraphic['lines'], text: V): DimensionGraphic => ({ constraint: k, lines, text, label });

  switch (c.type) {
    case 'distance': {
      const a = P(c.a);
      const b = P(c.b);
      if (!a || !b) return null;
      const d = { x: b.x - a.x, y: b.y - a.y };
      const l = Math.hypot(d.x, d.y) || 1;
      const n = { x: -d.y / l, y: d.x / l };
      const m = mid(a, b);
      const text = { x: m.x + n.x * gap + offset.x, y: m.y + n.y * gap + offset.y };
      // The dimension line runs through the text parallel to the measured segment.
      const h = (text.x - m.x) * n.x + (text.y - m.y) * n.y;
      const a2 = { x: a.x + n.x * h, y: a.y + n.y * h };
      const b2 = { x: b.x + n.x * h, y: b.y + n.y * h };
      return done(
        [
          [a.x, a.y, a2.x, a2.y],
          [b.x, b.y, b2.x, b2.y],
          [a2.x, a2.y, b2.x, b2.y],
        ],
        text,
      );
    }
    case 'distance-x': {
      const a = P(c.a);
      const b = P(c.b);
      if (!a || !b) return null;
      const y = Math.max(a.y, b.y) + gap + offset.y;
      const text = { x: (a.x + b.x) / 2 + offset.x, y };
      return done(
        [
          [a.x, a.y, a.x, y],
          [b.x, b.y, b.x, y],
          [a.x, y, b.x, y],
        ],
        text,
      );
    }
    case 'distance-y': {
      const a = P(c.a);
      const b = P(c.b);
      if (!a || !b) return null;
      const x = Math.max(a.x, b.x) + gap + offset.x;
      const text = { x, y: (a.y + b.y) / 2 + offset.y };
      return done(
        [
          [a.x, a.y, x, a.y],
          [b.x, b.y, x, b.y],
          [x, a.y, x, b.y],
        ],
        text,
      );
    }
    case 'radius':
    case 'diameter':
    case 'arc-length':
    case 'major-radius':
    case 'minor-radius': {
      const cv = curve(c.a);
      if (!cv) return null;
      // Leader from the centre (or the middle of the curve) to the text.
      const from: V | null = cv.center
        ? { x: cv.center[0], y: cv.center[1] }
        : cv.polyline.length >= 2
          ? { x: cv.polyline[0], y: cv.polyline[1] }
          : null;
      if (!from) return null;
      const r = cv.radius ?? gap;
      const anchor = cv.midpoint ? { x: cv.midpoint[0], y: cv.midpoint[1] } : { x: from.x + r * Math.SQRT1_2, y: from.y + r * Math.SQRT1_2 };
      const dir = { x: anchor.x - from.x, y: anchor.y - from.y };
      const l = Math.hypot(dir.x, dir.y) || 1;
      // Twice the gap: clear of the corner of a fillet (the virtual sharp lies at r·(√2 − 1) beyond).
      const text = { x: anchor.x + (dir.x / l) * 2 * gap + offset.x, y: anchor.y + (dir.y / l) * 2 * gap + offset.y };
      return done([[from.x, from.y, text.x, text.y]], text);
    }
    case 'angle': {
      const la = data.geometry[c.a - 1];
      const lb = data.geometry[c.b - 1];
      if (!la || !lb) return null;
      const pts = [P(la.start), P(la.end), P(lb.start), P(lb.end)];
      if (pts.some((p) => !p)) return null;
      const [a0, a1, b0, b1] = pts as V[];
      const ma = mid(a0, a1);
      const mb = mid(b0, b1);
      const m = mid(ma, mb);
      const text = { x: m.x + offset.x, y: m.y + gap + offset.y };
      return done(
        [
          [ma.x, ma.y, text.x, text.y],
          [mb.x, mb.y, text.x, text.y],
        ],
        text,
      );
    }
    case 'point-curve-distance': {
      const p = P(c.a);
      const cv = curve(c.b);
      if (!p || !cv) return null;
      // Foot of the point on the curve (line: on its infinite carrier; circle: on the circle).
      let foot: V | null = null;
      if (cv.center && cv.radius !== undefined) {
        const d = { x: p.x - cv.center[0], y: p.y - cv.center[1] };
        const l = Math.hypot(d.x, d.y) || 1;
        foot = { x: cv.center[0] + (d.x / l) * cv.radius, y: cv.center[1] + (d.y / l) * cv.radius };
      } else if (cv.polyline.length >= 4) {
        const ax = cv.polyline[0];
        const ay = cv.polyline[1];
        const dx = cv.polyline[cv.polyline.length - 2] - ax;
        const dy = cv.polyline[cv.polyline.length - 1] - ay;
        const t = ((p.x - ax) * dx + (p.y - ay) * dy) / (dx * dx + dy * dy || 1);
        foot = { x: ax + t * dx, y: ay + t * dy };
      }
      if (!foot) return null;
      const m = mid(p, foot);
      const text = { x: m.x + gap + offset.x, y: m.y + offset.y };
      return done([[p.x, p.y, foot.x, foot.y]], text);
    }
    default:
      return null;
  }
}

/**
 * Smart dimension between two points: the place of the text chooses the type, as in
 * SolidWorks/КОМПАС. Text beyond the points along Y (above or below both) with different X —
 * horizontal dimension; beyond them along X — vertical; otherwise the aligned distance.
 */
export function twoPointDimension(a: V, b: V, text: V, tolerance: number): 'distance' | 'distance-x' | 'distance-y' {
  const dx = Math.abs(b.x - a.x);
  const dy = Math.abs(b.y - a.y);
  if (dx <= tolerance || dy <= tolerance) return 'distance';
  const outsideY = text.y > Math.max(a.y, b.y) || text.y < Math.min(a.y, b.y);
  const outsideX = text.x > Math.max(a.x, b.x) || text.x < Math.min(a.x, b.x);
  if (outsideY && !outsideX) return 'distance-x';
  if (outsideX && !outsideY) return 'distance-y';
  return 'distance';
}

/** Offset that puts the text of graphic `g` (computed with zero offset) at `text`. */
export function offsetFor(g: DimensionGraphic, text: V): V {
  return { x: text.x - g.text.x, y: text.y - g.text.y };
}
