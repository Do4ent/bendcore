// Sketch data editing on the UI side (docs/ui/SKETCH_EDITOR.md): adding points, curves and
// constraints to the document's SketchData, deleting with renumbering, which constraints apply
// to a selection, and which elements take part in conflicts. The data is the part document's
// format (PART_DOCUMENT.md); geometry and diagnostics always come from the kernel solve.

import type {
  SketchConstraintDef,
  SketchData,
  SketchGeometryDef,
  SketchSolution,
} from '../../../wasm/bendcore_modeling_api.mjs';

export type SketchRef = { kind: 'point'; index: number } | { kind: 'entity'; id: number };

export const refKey = (r: SketchRef): string => (r.kind === 'point' ? `p${r.index}` : `e${r.id}`);
export const pointRef = (index: number): SketchRef => ({ kind: 'point', index });
export const entityRef = (id: number): SketchRef => ({ kind: 'entity', id });

export type OperandKind = 'point' | 'entity';

/** Operands (a, b, c) of every constraint type of the sketch node (part_sketch.hpp). */
export const CONSTRAINT_OPERANDS: Record<string, OperandKind[]> = {
  coincident: ['point', 'point'],
  horizontal: ['point', 'point'],
  vertical: ['point', 'point'],
  distance: ['point', 'point'],
  'distance-x': ['point', 'point'],
  'distance-y': ['point', 'point'],
  'point-curve-distance': ['point', 'entity'],
  radius: ['entity'],
  diameter: ['entity'],
  'arc-length': ['entity'],
  'major-radius': ['entity'],
  'minor-radius': ['entity'],
  parallel: ['entity', 'entity'],
  perpendicular: ['entity', 'entity'],
  'equal-length': ['entity', 'entity'],
  'equal-radius': ['entity', 'entity'],
  equal: ['entity', 'entity'],
  angle: ['entity', 'entity'],
  tangent: ['entity', 'entity'],
  concentric: ['entity', 'entity'],
  'point-on-line': ['point', 'entity'],
  'point-on-circle': ['point', 'entity'],
  'point-on': ['point', 'entity'],
  midpoint: ['point', 'entity'],
  symmetric: ['point', 'point', 'entity'],
  'symmetric-point': ['point', 'point', 'point'],
  rotation: ['point', 'point', 'point'],
  fix: ['point'],
};

/** Dimensions: have a value and may be reference (driven) dimensions. */
export const DIMENSION_TYPES = new Set([
  'distance',
  'distance-x',
  'distance-y',
  'point-curve-distance',
  'radius',
  'diameter',
  'angle',
  'arc-length',
  'major-radius',
  'minor-radius',
]);

/** Constraints with a value that are not dimensions. */
export const VALUE_TYPES = new Set(['rotation']);

export const hasValue = (type: string) => DIMENSION_TYPES.has(type) || VALUE_TYPES.has(type);

export function emptySketch(): SketchData {
  return { points: [], geometry: [], constraints: [], regions: [] };
}

export function cloneSketch(data: SketchData): SketchData {
  return {
    points: data.points.map((p) => ({ ...p })),
    geometry: data.geometry.map((g) => ({ ...g, poles: g.poles ? [...g.poles] : undefined })),
    constraints: data.constraints.map((c) => ({ ...c })),
    regions: [...data.regions],
  };
}

/**
 * Decimal spelling of a coordinate (the data stores expressions; the expression parser has no
 * exponent notation): values below 1e-9 become 0, the rest is written positionally.
 */
export function num(v: number): string {
  if (Math.abs(v) < 1e-9) return '0';
  const text = String(v);
  return text.includes('e') ? v.toFixed(12).replace(/0+$/, '').replace(/\.$/, '') : text;
}

/** Appends a point; returns its index. */
export function addPoint(data: SketchData, x: number, y: number, fixed = false): number {
  data.points.push({ x: num(x), y: num(y), fixed });
  return data.points.length - 1;
}

const GEOMETRY_DEFAULTS: Omit<SketchGeometryDef, 'type'> = {
  start: -1,
  end: -1,
  center: -1,
  radius: '1',
  counterClockwise: true,
  construction: false,
  projection: -1,
};

/** Appends a curve; returns its entity id (1-based). */
export function addGeometry(data: SketchData, def: Partial<SketchGeometryDef> & { type: SketchGeometryDef['type'] }): number {
  data.geometry.push({ ...GEOMETRY_DEFAULTS, ...def });
  return data.geometry.length;
}

/** Appends a constraint; returns its number (1-based). */
export function addConstraint(
  data: SketchData,
  type: string,
  a: number,
  b = -1,
  c = -1,
  value = '0',
  driven = false,
): number {
  const def: SketchConstraintDef = { type, a, b, c, value };
  if (driven) def.driven = true;
  data.constraints.push(def);
  return data.constraints.length;
}

/** Point indices a curve uses (ends, centre, focus, poles). */
export function curvePoints(g: SketchGeometryDef): number[] {
  const out = [g.start, g.end, g.center, g.focus ?? -1].filter((p) => p >= 0);
  for (const p of g.poles ?? []) out.push(p);
  return out;
}

/** Elements a constraint refers to. */
export function constraintRefs(c: SketchConstraintDef): SketchRef[] {
  const ops = CONSTRAINT_OPERANDS[c.type] ?? [];
  const values = [c.a, c.b, c.c];
  const out: SketchRef[] = [];
  ops.forEach((kind, i) => {
    const v = values[i];
    if (v === undefined || v < 0 || (kind === 'entity' && v < 1)) return;
    out.push(kind === 'point' ? pointRef(v) : entityRef(v));
  });
  return out;
}

/** Index of the fixed point at (0, 0) — the sketch origin — or -1. */
export function originPoint(data: SketchData): number {
  return data.points.findIndex((p) => p.fixed && Number(p.x) === 0 && Number(p.y) === 0);
}

export interface RemoveResult {
  data: SketchData;
  /** old constraint number -> new number (0: removed) */
  constraintMap: number[];
}

/**
 * Deletes entities, points and constraints. A deleted curve takes along its points unless another
 * remaining curve uses them; a deleted point must not be used by a remaining curve (such points
 * are kept). Constraints on deleted elements are deleted; the rest is renumbered, regions too.
 */
export function removeItems(data: SketchData, refs: SketchRef[], constraints: number[] = []): RemoveResult {
  const dropEntity = new Set(refs.filter((r) => r.kind === 'entity').map((r) => (r as { id: number }).id));
  const keptCurves = data.geometry.filter((_, i) => !dropEntity.has(i + 1));
  const usedByKept = new Set(keptCurves.flatMap(curvePoints));
  const dropPoint = new Set<number>();
  data.geometry.forEach((g, i) => {
    if (!dropEntity.has(i + 1)) return;
    for (const p of curvePoints(g)) if (!usedByKept.has(p)) dropPoint.add(p);
  });
  for (const r of refs) if (r.kind === 'point' && !usedByKept.has(r.index)) dropPoint.add(r.index);

  const pointMap: number[] = [];
  const points = data.points.filter((_, i) => !dropPoint.has(i));
  let n = 0;
  data.points.forEach((_, i) => (pointMap[i] = dropPoint.has(i) ? -1 : n++));
  const entityMap: number[] = [0];
  let m = 0;
  data.geometry.forEach((_, i) => (entityMap[i + 1] = dropEntity.has(i + 1) ? 0 : ++m));
  const mapPoint = (p: number) => (p >= 0 ? pointMap[p] : p);

  const geometry = keptCurves.map((g) => ({
    ...g,
    start: mapPoint(g.start),
    end: mapPoint(g.end),
    center: mapPoint(g.center),
    ...(g.focus !== undefined ? { focus: mapPoint(g.focus) } : {}),
    ...(g.poles ? { poles: g.poles.map(mapPoint) } : {}),
  }));

  const dropConstraint = new Set(constraints);
  const constraintMap: number[] = [0];
  const kept: SketchConstraintDef[] = [];
  data.constraints.forEach((c, i) => {
    const gone =
      dropConstraint.has(i + 1) ||
      constraintRefs(c).some((r) => (r.kind === 'point' ? pointMap[r.index] < 0 : entityMap[r.id] === 0));
    if (gone) {
      constraintMap[i + 1] = 0;
      return;
    }
    const ops = CONSTRAINT_OPERANDS[c.type] ?? [];
    const remap = (v: number, i2: number) => {
      if (v < 0 || ops[i2] === undefined) return v;
      return ops[i2] === 'point' ? pointMap[v] : entityMap[v];
    };
    kept.push({ ...c, a: remap(c.a, 0), b: remap(c.b, 1), c: remap(c.c, 2) });
    constraintMap[i + 1] = kept.length;
  });
  const regions = data.regions.map((r) => entityMap[r] ?? 0).filter((r) => r > 0);
  return { data: { points, geometry, constraints: kept, regions }, constraintMap };
}

/** Switches the construction flag of the given entities (all on if any is off). */
export function toggleConstruction(data: SketchData, ids: number[]): SketchData {
  const out = cloneSketch(data);
  const on = ids.some((id) => !out.geometry[id - 1]?.construction);
  for (const id of ids) if (out.geometry[id - 1]) out.geometry[id - 1].construction = on;
  return out;
}

// --------------------------------------------------------------------------------- selection

export interface ConstraintProposal {
  type: string;
  a: number;
  b: number;
  c: number;
}

type GeoType = SketchGeometryDef['type'];
const CIRCULAR: GeoType[] = ['circle', 'arc'];
const CONIC: GeoType[] = ['ellipse', 'elliptical-arc'];

/**
 * Constraints (and dimensions) that can be applied to a selection, in a fixed order; operands
 * follow the selection order (points first as selected, then entities).
 */
export function applicableConstraints(data: SketchData, selection: SketchRef[]): ConstraintProposal[] {
  const pts = selection.filter((r) => r.kind === 'point').map((r) => (r as { index: number }).index);
  const ents = selection.filter((r) => r.kind === 'entity').map((r) => (r as { id: number }).id);
  const types = ents.map((id) => data.geometry[id - 1]?.type);
  const out: ConstraintProposal[] = [];
  const add = (type: string, a: number, b = -1, c = -1) => out.push({ type, a, b, c });
  const P = pts.length;
  const E = ents.length;
  const isType = (i: number, set: GeoType[]) => set.includes(types[i]);
  const line0 = E === 1 && types[0] === 'line' ? data.geometry[ents[0] - 1] : null;

  if (P === 2 && E === 0) {
    add('coincident', pts[0], pts[1]);
    add('horizontal', pts[0], pts[1]);
    add('vertical', pts[0], pts[1]);
    add('distance', pts[0], pts[1]);
    add('distance-x', pts[0], pts[1]);
    add('distance-y', pts[0], pts[1]);
  }
  if (P === 0 && line0) {
    add('horizontal', line0.start, line0.end);
    add('vertical', line0.start, line0.end);
    add('distance', line0.start, line0.end);
    add('distance-x', line0.start, line0.end);
    add('distance-y', line0.start, line0.end);
  }
  if (P === 1 && E === 0) add('fix', pts[0]);
  if (P === 0 && E === 2) {
    const [a, b] = ents;
    const lines = types.every((t) => t === 'line');
    if (lines) {
      add('parallel', a, b);
      add('perpendicular', a, b);
      add('angle', a, b);
    }
    if (lines || (isType(0, CIRCULAR) && isType(1, CIRCULAR)) || (isType(0, CONIC) && isType(1, CONIC))) add('equal', a, b);
    const tangentPair =
      !lines &&
      !(isType(0, CONIC) && isType(1, CONIC)) &&
      !((isType(0, CONIC) && isType(1, CIRCULAR)) || (isType(1, CONIC) && isType(0, CIRCULAR)));
    if (tangentPair) add('tangent', a, b);
    const centred = [...CIRCULAR, ...CONIC];
    if (isType(0, centred) && isType(1, centred)) add('concentric', a, b);
  }
  if (P === 1 && E === 1) {
    const t = types[0];
    if (t === 'line' || t === 'arc') add('midpoint', pts[0], ents[0]);
    if (t !== 'bspline') add('point-on', pts[0], ents[0]);
    if (t === 'line' || CIRCULAR.includes(t)) add('point-curve-distance', pts[0], ents[0]);
  }
  if (P === 2 && E === 1 && types[0] === 'line') add('symmetric', pts[0], pts[1], ents[0]);
  if (P === 3 && E === 0) {
    add('symmetric-point', pts[0], pts[1], pts[2]);
    add('rotation', pts[0], pts[1], pts[2]);
  }
  if (P === 0 && E === 1) {
    if (isType(0, CIRCULAR)) {
      add('radius', ents[0]);
      add('diameter', ents[0]);
    }
    if (types[0] === 'arc') add('arc-length', ents[0]);
    if (isType(0, CONIC)) {
      add('major-radius', ents[0]);
      add('minor-radius', ents[0]);
    }
  }
  return out;
}

// --------------------------------------------------------------------------------- diagnostics

export type DefinitionState = 'defined' | 'under' | 'conflict' | 'redundant';

export interface Highlights {
  /** constraint numbers (1-based) */
  conflicting: Set<number>;
  redundant: Set<number>;
  /** elements of conflicting / redundant constraints */
  conflictRefs: Set<string>;
  redundantRefs: Set<string>;
}

export function highlights(data: SketchData, solution: SketchSolution | null): Highlights {
  const out: Highlights = {
    conflicting: new Set(solution?.conflicting ?? []),
    redundant: new Set([...(solution?.redundant ?? []), ...(solution?.partiallyRedundant ?? [])]),
    conflictRefs: new Set(),
    redundantRefs: new Set(),
  };
  const mark = (numbers: Set<number>, refs: Set<string>) => {
    for (const k of numbers) {
      const c = data.constraints[k - 1];
      if (!c) continue;
      for (const r of constraintRefs(c)) {
        refs.add(refKey(r));
        if (r.kind === 'entity') continue;
        // A point of a curve marks the curve too only when the point is its own constraint operand.
      }
    }
  };
  mark(out.conflicting, out.conflictRefs);
  mark(out.redundant, out.redundantRefs);
  return out;
}

/** Display state of an element: conflict > redundant > defined/under (kernel definition). */
export function definitionState(ref: SketchRef, solution: SketchSolution | null, h: Highlights): DefinitionState {
  const key = refKey(ref);
  if (h.conflictRefs.has(key)) return 'conflict';
  if (h.redundantRefs.has(key)) return 'redundant';
  if (!solution) return 'under';
  if (ref.kind === 'point') return solution.points[ref.index]?.defined ? 'defined' : 'under';
  return solution.curves[ref.id - 1]?.defined ? 'defined' : 'under';
}

export function constraintState(k: number, h: Highlights): 'ok' | 'conflict' | 'redundant' {
  if (h.conflicting.has(k)) return 'conflict';
  if (h.redundant.has(k)) return 'redundant';
  return 'ok';
}

/** Entities sharing point p (curve ends and centres). */
export function curvesAtPoint(data: SketchData, p: number): number[] {
  const out: number[] = [];
  data.geometry.forEach((g, i) => {
    if (g.start === p || g.end === p) out.push(i + 1);
  });
  return out;
}

/** Two lines meeting at point p (for fillet / chamfer of a corner), or null. */
export function cornerLines(data: SketchData, p: number): [number, number] | null {
  const lines = curvesAtPoint(data, p).filter((id) => data.geometry[id - 1].type === 'line');
  return lines.length === 2 ? [lines[0], lines[1]] : null;
}
