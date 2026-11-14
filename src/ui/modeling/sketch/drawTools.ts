// Drawing tools of the sketch editor (docs/ui/SKETCH_EDITOR.md, "Инструменты рисования"): each
// tool turns a sequence of snapped picks into additions to the SketchData — points at their
// initial positions, curve definitions and the automatic constraints (coincidence through the
// snap, horizontal/vertical, tangency of the tangent arc). The curves themselves are computed
// by the kernel when the data is solved; nothing here is drawn.

import type { SketchData } from '../../../wasm/bendcore_modeling_api.mjs';
import { addConstraint, addGeometry, addPoint, cloneSketch, originPoint } from './sketchModel';
import type { Snap } from './snap';
import { translate } from '../../app/i18n';
import type { MessageKey } from '../../app/i18n';

export type DrawTool =
  | 'line'
  | 'rectangle'
  | 'rectangle-center'
  | 'circle'
  | 'circle-3'
  | 'arc'
  | 'arc-3'
  | 'arc-tangent'
  | 'ellipse'
  | 'spline'
  | 'point';

/** Drawing tools in toolbar order; captions are messages sketch.tool.<tool>. */
export const DRAW_TOOLS: DrawTool[] = [
  'line',
  'rectangle',
  'rectangle-center',
  'circle',
  'circle-3',
  'arc',
  'arc-3',
  'arc-tangent',
  'ellipse',
  'spline',
  'point',
];

/** Picks a tool needs; the spline takes any number >= 2 and is finished explicitly. */
export const PICK_COUNT: Record<DrawTool, number> = {
  line: 2,
  rectangle: 2,
  'rectangle-center': 2,
  circle: 2,
  'circle-3': 3,
  arc: 3,
  'arc-3': 3,
  'arc-tangent': 2,
  ellipse: 3,
  spline: Infinity,
  point: 1,
};

/** Prompt (message key) for pick number `n` (0-based); the last one repeats. */
export const PICK_HINTS: Record<DrawTool, MessageKey[]> = {
  line: ['sketch.pick.lineStart', 'sketch.pick.lineEnd'],
  rectangle: ['sketch.pick.firstCorner', 'sketch.pick.oppositeCorner'],
  'rectangle-center': ['sketch.pick.center', 'sketch.pick.corner'],
  circle: ['sketch.pick.center', 'sketch.pick.onCircle'],
  'circle-3': ['sketch.pick.first', 'sketch.pick.second', 'sketch.pick.third'],
  arc: ['sketch.pick.center', 'sketch.pick.arcStart', 'sketch.pick.arcEnd'],
  'arc-3': ['sketch.pick.arcStart', 'sketch.pick.arcEnd', 'sketch.pick.onArc'],
  'arc-tangent': ['sketch.pick.tangentFrom', 'sketch.pick.tangentEnd'],
  ellipse: ['sketch.pick.center', 'sketch.pick.majorAxis', 'sketch.pick.onEllipse'],
  spline: ['sketch.pick.pole', 'sketch.pick.nextPole'],
  point: ['sketch.pick.point'],
};

export interface DrawOptions {
  construction?: boolean;
  /** Angle (degrees) within which a drawn line gets a horizontal / vertical constraint. */
  axisAngle?: number;
}

export interface DrawResult {
  data: SketchData;
  /** entity ids created */
  created: number[];
  /** the line tool continues its chain from this pick */
  next?: Snap;
}

/** A shape that cannot be drawn from the picks: `key` is the message for the user. */
export class DrawError extends Error {
  constructor(
    readonly key: MessageKey,
    readonly params: Record<string, string | number> = {},
  ) {
    super(translate('ru', key, params));
  }
}

type V = { x: number; y: number };
const sub = (a: V, b: V): V => ({ x: a.x - b.x, y: a.y - b.y });
const len = (a: V) => Math.hypot(a.x, a.y);
const cross = (a: V, b: V) => a.x * b.y - a.y * b.x;
const TWO_PI = 2 * Math.PI;
const norm = (a: number) => ((a % TWO_PI) + TWO_PI) % TWO_PI;

/** Centre of the circle through three points; null when they are collinear. */
export function circumcenter(a: V, b: V, c: V): V | null {
  const d = 2 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
  const scale = Math.max(len(sub(b, a)), len(sub(c, a)), len(sub(c, b)));
  if (Math.abs(d) <= 1e-9 * scale * scale) return null;
  const a2 = a.x * a.x + a.y * a.y;
  const b2 = b.x * b.x + b.y * b.y;
  const c2 = c.x * c.x + c.y * c.y;
  return {
    x: (a2 * (b.y - c.y) + b2 * (c.y - a.y) + c2 * (a.y - b.y)) / d,
    y: (a2 * (c.x - b.x) + b2 * (a.x - c.x) + c2 * (b.x - a.x)) / d,
  };
}

/** True when going counter-clockwise from `start` around `center` meets `mid` before `end`. */
export function isCounterClockwise(center: V, start: V, mid: V, end: V): boolean {
  const a0 = Math.atan2(start.y - center.y, start.x - center.x);
  const am = norm(Math.atan2(mid.y - center.y, mid.x - center.x) - a0);
  const ae = norm(Math.atan2(end.y - center.y, end.x - center.x) - a0);
  return am < ae;
}

/**
 * Arc that leaves `start` along the unit direction `dir` and ends at `end`: its centre lies on
 * the normal at `start`, equidistant from both points. Null when `end` is on the tangent line.
 */
export function tangentArc(start: V, dir: V, end: V): { center: V; counterClockwise: boolean } | null {
  const n = { x: -dir.y, y: dir.x };
  const e = sub(end, start);
  const dot = n.x * e.x + n.y * e.y;
  if (Math.abs(dot) <= 1e-9 * len(e)) return null;
  const t = (e.x * e.x + e.y * e.y) / (2 * dot);
  return { center: { x: start.x + n.x * t, y: start.y + n.y * t }, counterClockwise: t > 0 };
}

/**
 * Ellipse from its centre, an end of one axis and a point on it: the longer semi-axis becomes the
 * major one. Returns the focus (direction of the major axis) and the semi-axes.
 */
export function ellipseFrom(center: V, axisEnd: V, onCurve: V): { focus: V; major: number; minor: number } | null {
  const u0 = sub(axisEnd, center);
  const a0 = len(u0);
  if (a0 <= 0) return null;
  const u = { x: u0.x / a0, y: u0.y / a0 };
  const r = sub(onCurve, center);
  const along = r.x * u.x + r.y * u.y;
  const across = Math.abs(cross(u, r));
  // (along / a0)^2 + (across / b)^2 = 1
  const k = 1 - (along * along) / (a0 * a0);
  if (k <= 1e-12 || across <= 0) return null;
  const b0 = across / Math.sqrt(k);
  let major = a0;
  let minor = b0;
  let dir = u;
  if (b0 > a0) {
    major = b0;
    minor = a0;
    dir = { x: -u.y, y: u.x };
  }
  if (major - minor <= 1e-6 * major) return null;
  const f = Math.sqrt(major * major - minor * minor);
  return { focus: { x: center.x + dir.x * f, y: center.y + dir.y * f }, major, minor };
}

/** Clamped uniform knots of a B-spline with `poles` poles: degree min(3, poles - 1). */
export function clampedKnots(poles: number): { degree: number; knots: number[]; multiplicities: number[] } {
  const degree = Math.min(3, poles - 1);
  const spans = poles - degree;
  const knots = Array.from({ length: spans + 1 }, (_, i) => i);
  const multiplicities = knots.map((_, i) => (i === 0 || i === spans ? degree + 1 : 1));
  return { degree, knots, multiplicities };
}

/**
 * Sketch point for a pick: an existing point when the snap hit one, the origin (fixed point at
 * (0, 0), created once), otherwise a new point. Snaps to a midpoint, a curve or an intersection
 * give the new point its constraint.
 */
export function pointForSnap(data: SketchData, snap: Snap): number {
  if (snap.point !== undefined && snap.point < data.points.length) return snap.point;
  if (snap.kind === 'origin') {
    const existing = originPoint(data);
    return existing >= 0 ? existing : addPoint(data, 0, 0, true);
  }
  const p = addPoint(data, snap.x, snap.y);
  if (snap.kind === 'midpoint' && snap.entity) addConstraint(data, 'midpoint', p, snap.entity);
  if (snap.kind === 'on-curve' && snap.entity && data.geometry[snap.entity - 1]?.type !== 'bspline') {
    addConstraint(data, 'point-on', p, snap.entity);
  }
  if (snap.kind === 'intersection' && snap.entities) {
    for (const e of snap.entities) {
      if (data.geometry[e - 1]?.type !== 'bspline') addConstraint(data, 'point-on', p, e);
    }
  }
  return p;
}

const free = (p: V): Snap => ({ x: p.x, y: p.y, kind: 'free' });

/** Point-on constraint for a rim pick that hit an existing point. */
function rimConstraint(data: SketchData, snap: Snap, entity: number): void {
  if (snap.point !== undefined || snap.kind === 'origin') addConstraint(data, 'point-on', pointForSnap(data, snap), entity);
}

/** End of a line or arc at point p: the entity and its unit travel direction at p (leaving it). */
export function endDirection(data: SketchData, p: number, positions: V[]): { entity: number; dir: V } | null {
  for (let i = data.geometry.length - 1; i >= 0; --i) {
    const g = data.geometry[i];
    if (g.start !== p && g.end !== p) continue;
    const at = positions[p];
    if (g.type === 'line') {
      const other = positions[g.start === p ? g.end : g.start];
      const d = sub(at, other);
      const l = len(d);
      if (l > 0) return { entity: i + 1, dir: { x: d.x / l, y: d.y / l } };
    }
    if (g.type === 'arc') {
      const r = sub(at, positions[g.center]);
      const l = len(r);
      if (l <= 0) continue;
      // Travel direction of the arc at its end; leaving through the start reverses it.
      const forward = g.counterClockwise ? { x: -r.y / l, y: r.x / l } : { x: r.y / l, y: -r.x / l };
      return { entity: i + 1, dir: g.end === p ? forward : { x: -forward.x, y: -forward.y } };
    }
  }
  return null;
}

/**
 * Adds the shape of `tool` defined by `picks` to a copy of `data`. `positions` are the solved
 * positions of the existing points (for the tangent arc). Throws DrawError when the picks do not
 * define the shape (coincident or collinear points).
 */
export function drawShape(
  data0: SketchData,
  tool: DrawTool,
  picks: Snap[],
  positions: V[],
  options: DrawOptions = {},
): DrawResult {
  const data = cloneSketch(data0);
  const construction = options.construction ?? false;
  const created: number[] = [];
  const geo = (def: Parameters<typeof addGeometry>[1]) => {
    const id = addGeometry(data, { ...def, construction });
    created.push(id);
    return id;
  };
  const distinct = (a: V, b: V, what: MessageKey) => {
    if (len(sub(a, b)) <= 1e-9) throw new DrawError(what);
  };

  switch (tool) {
    case 'point': {
      const before = data.points.length;
      pointForSnap(data, picks[0]);
      if (data.points.length === before) throw new DrawError('sketch.err.pointExists');
      return { data, created };
    }
    case 'line': {
      const [a, b] = picks;
      distinct(a, b, 'sketch.err.sameLine');
      const reusedBoth = a.point !== undefined && b.point !== undefined;
      const pa = pointForSnap(data, a);
      const pb = pointForSnap(data, b);
      if (pa === pb) throw new DrawError('sketch.err.sameLine');
      geo({ type: 'line', start: pa, end: pb });
      if (!reusedBoth) {
        const limit = Math.tan(((options.axisAngle ?? 2) * Math.PI) / 180);
        const dx = Math.abs(b.x - a.x);
        const dy = Math.abs(b.y - a.y);
        if (b.kind === 'horizontal' || dy <= limit * dx) addConstraint(data, 'horizontal', pa, pb);
        else if (b.kind === 'vertical' || dx <= limit * dy) addConstraint(data, 'vertical', pa, pb);
      }
      return { data, created, next: { x: b.x, y: b.y, kind: 'endpoint', point: pb } };
    }
    case 'rectangle':
    case 'rectangle-center': {
      const centered = tool === 'rectangle-center';
      const c = picks[1];
      const a: Snap = centered ? free({ x: 2 * picks[0].x - c.x, y: 2 * picks[0].y - c.y }) : picks[0];
      if (Math.abs(c.x - a.x) <= 1e-9 || Math.abs(c.y - a.y) <= 1e-9) throw new DrawError('sketch.err.rectangle');
      const p0 = pointForSnap(data, a);
      const p1 = addPoint(data, c.x, a.y);
      const p2 = pointForSnap(data, c);
      const p3 = addPoint(data, a.x, c.y);
      geo({ type: 'line', start: p0, end: p1 });
      geo({ type: 'line', start: p1, end: p2 });
      geo({ type: 'line', start: p2, end: p3 });
      geo({ type: 'line', start: p3, end: p0 });
      addConstraint(data, 'horizontal', p0, p1);
      addConstraint(data, 'vertical', p1, p2);
      addConstraint(data, 'horizontal', p2, p3);
      addConstraint(data, 'vertical', p3, p0);
      if (centered) addConstraint(data, 'symmetric-point', p0, p2, pointForSnap(data, picks[0]));
      return { data, created };
    }
    case 'circle': {
      const [c, rim] = picks;
      distinct(c, rim, 'sketch.err.sameCircle');
      const pc = pointForSnap(data, c);
      const id = geo({ type: 'circle', center: pc, radius: String(len(sub(rim, c))) });
      rimConstraint(data, rim, id);
      return { data, created };
    }
    case 'circle-3': {
      const center = circumcenter(picks[0], picks[1], picks[2]);
      if (!center) throw new DrawError('sketch.err.circleCollinear');
      const pc = addPoint(data, center.x, center.y);
      const id = geo({ type: 'circle', center: pc, radius: String(len(sub(picks[0], center))) });
      for (const p of picks) rimConstraint(data, p, id);
      return { data, created };
    }
    case 'arc': {
      const [c, s, e0] = picks;
      distinct(c, s, 'sketch.err.sameArc');
      distinct(c, e0, 'sketch.err.sameArc');
      const r = len(sub(s, c));
      const d = sub(e0, c);
      const onCircle = { x: c.x + (d.x / len(d)) * r, y: c.y + (d.y / len(d)) * r };
      distinct(s, onCircle, 'sketch.err.sameArc');
      const pc = pointForSnap(data, c);
      const ps = pointForSnap(data, s);
      // The end is put on the circle; a snapped end keeps its snap (the solver moves the arc).
      const pe = pointForSnap(data, e0.kind === 'free' || e0.kind === 'grid' ? free(onCircle) : e0);
      geo({ type: 'arc', center: pc, start: ps, end: pe, counterClockwise: cross(sub(s, c), d) >= 0 });
      return { data, created };
    }
    case 'arc-3': {
      const [s, e, mid] = picks;
      const center = circumcenter(s, mid, e);
      if (!center) throw new DrawError('sketch.err.arcCollinear');
      const ps = pointForSnap(data, s);
      const pe = pointForSnap(data, e);
      const pc = addPoint(data, center.x, center.y);
      const id = geo({ type: 'arc', center: pc, start: ps, end: pe, counterClockwise: isCounterClockwise(center, s, mid, e) });
      rimConstraint(data, mid, id);
      return { data, created };
    }
    case 'arc-tangent': {
      const [s, e] = picks;
      if (s.point === undefined) throw new DrawError('sketch.err.tangentStart');
      const from = endDirection(data, s.point, positions);
      if (!from) throw new DrawError('sketch.err.tangentStart');
      const start = positions[s.point];
      distinct(start, e, 'sketch.err.sameArc');
      const arc = tangentArc(start, from.dir, e);
      if (!arc) throw new DrawError('sketch.err.tangentEnd');
      const pe = pointForSnap(data, e);
      const pc = addPoint(data, arc.center.x, arc.center.y);
      const id = geo({ type: 'arc', center: pc, start: s.point, end: pe, counterClockwise: arc.counterClockwise });
      addConstraint(data, 'tangent', from.entity, id);
      return { data, created };
    }
    case 'ellipse': {
      const el = ellipseFrom(picks[0], picks[1], picks[2]);
      if (!el) throw new DrawError('sketch.err.ellipse');
      const pc = pointForSnap(data, picks[0]);
      const pf = addPoint(data, el.focus.x, el.focus.y);
      geo({ type: 'ellipse', center: pc, focus: pf, radius: String(el.minor) });
      return { data, created };
    }
    case 'spline': {
      if (picks.length < 2) throw new DrawError('sketch.err.splinePoles');
      for (let i = 1; i < picks.length; ++i) distinct(picks[i - 1], picks[i], 'sketch.err.sameSpline');
      const poles = picks.map((p) => pointForSnap(data, p));
      geo({ type: 'bspline', poles, ...clampedKnots(poles.length), weights: [], periodic: false });
      return { data, created };
    }
  }
  throw new DrawError('sketch.err.unknownTool', { tool: String(tool) });
}
