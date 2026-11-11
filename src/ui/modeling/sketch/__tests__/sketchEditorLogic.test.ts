// UI-side logic of the sketch editor (docs/ui/SKETCH_EDITOR.md): snapping, drawing tools,
// selection -> constraints, deletion with renumbering, dimension layout. Kernel answers (the
// solved geometry) are given as fixed solution objects of the shape solveSketch returns.

import type { SketchData, SketchSolution } from '../../../../wasm/bendcore_modeling_api.mjs';
import { dimensionGraphic, dimensionLabel, twoPointDimension } from '../dimensionLayout';
import {
  DrawError,
  circumcenter,
  clampedKnots,
  drawShape,
  ellipseFrom,
  endDirection,
  isCounterClockwise,
  tangentArc,
} from '../drawTools';
import {
  applicableConstraints,
  cornerLines,
  definitionState,
  emptySketch,
  entityRef,
  highlights,
  pointRef,
  removeItems,
  toggleConstruction,
} from '../sketchModel';
import { findSnap, gridStep, nearestOnPolyline, pointRoles } from '../snap';

/** Rectangle 60 x 40 with corner (0,0) fixed: points 0..3, lines 1..4, H/V and two distances. */
function rectangle(): SketchData {
  return {
    points: [
      { x: '0', y: '0', fixed: true },
      { x: '60', y: '0', fixed: false },
      { x: '60', y: '40', fixed: false },
      { x: '0', y: '40', fixed: false },
    ],
    geometry: [
      [0, 1],
      [1, 2],
      [2, 3],
      [3, 0],
    ].map(([start, end]) => ({ type: 'line' as const, start, end, center: -1, radius: '1', counterClockwise: true, construction: false, projection: -1 })),
    constraints: [
      { type: 'horizontal', a: 0, b: 1, c: -1, value: '0' },
      { type: 'vertical', a: 1, b: 2, c: -1, value: '0' },
      { type: 'horizontal', a: 2, b: 3, c: -1, value: '0' },
      { type: 'vertical', a: 3, b: 0, c: -1, value: '0' },
      { type: 'distance', a: 0, b: 1, c: -1, value: '60' },
      { type: 'distance', a: 1, b: 2, c: -1, value: '40' },
    ],
    regions: [],
  };
}

/** Solution of rectangle() as the kernel gives it (corners, lines as 2-point polylines). */
function rectangleSolution(extra: Partial<SketchSolution> = {}): SketchSolution {
  const pts = [
    [0, 0],
    [60, 0],
    [60, 40],
    [0, 40],
  ];
  return {
    converged: true,
    status: 'определено',
    dof: 0,
    definition: 'fully-defined',
    conflicting: [],
    redundant: [],
    partiallyRedundant: [],
    placement: { origin: [0, 0, 0], xDirection: [1, 0, 0], yDirection: [0, 1, 0], normal: [0, 0, 1] },
    points: pts.map(([x, y], i) => ({ x, y, fixed: i === 0, defined: true })),
    constraintValues: [0, 0, 0, 0, 60, 40],
    residuals: [0, 0, 0, 0, 0, 0],
    curves: [
      [0, 1],
      [1, 2],
      [2, 3],
      [3, 0],
    ].map(([a, b], i) => ({
      entity: i + 1,
      type: 'line' as const,
      construction: false,
      projection: -1,
      defined: true,
      polyline: new Float64Array([...pts[a], ...pts[b]]),
      midpoint: [(pts[a][0] + pts[b][0]) / 2, (pts[a][1] + pts[b][1]) / 2] as [number, number],
    })),
    intersections: [],
    profile: null,
    sketch: rectangle(),
    ...extra,
  };
}

describe('snapping', () => {
  const data = rectangle();
  const sol = rectangleSolution({ intersections: [{ x: 30, y: 20, a: 5, b: 6 }] });

  it('roles of points', () => {
    expect(pointRoles(data)).toEqual(['endpoint', 'endpoint', 'endpoint', 'endpoint']);
  });

  it('priority: point, intersection, midpoint, origin, curve, alignment, grid', () => {
    expect(findSnap({ x: 59.5, y: 0.4 }, data, sol, { tolerance: 1 })).toMatchObject({ kind: 'endpoint', point: 1, x: 60, y: 0 });
    expect(findSnap({ x: 30.3, y: 19.6 }, data, sol, { tolerance: 1 })).toMatchObject({ kind: 'intersection', entities: [5, 6] });
    expect(findSnap({ x: 30.4, y: 0.2 }, data, sol, { tolerance: 1 })).toMatchObject({ kind: 'midpoint', entity: 1, x: 30, y: 0 });
    expect(findSnap({ x: 45, y: 0.5 }, data, sol, { tolerance: 1 })).toMatchObject({ kind: 'on-curve', entity: 1, x: 45, y: 0 });
    expect(findSnap({ x: 0.3, y: -0.2 }, emptySketch(), null, { tolerance: 1 })).toMatchObject({ kind: 'origin', x: 0, y: 0 });
    expect(findSnap({ x: 80, y: 40.6 }, data, sol, { tolerance: 1, reference: { x: 70, y: 40.0001 + 10 } })).toMatchObject({ kind: 'free' });
    expect(findSnap({ x: 90, y: 10.7 }, data, sol, { tolerance: 1, reference: { x: 70, y: 10 } })).toMatchObject({ kind: 'horizontal', x: 90, y: 10 });
    expect(findSnap({ x: 70.4, y: 90 }, data, sol, { tolerance: 1, reference: { x: 70, y: 70 } })).toMatchObject({ kind: 'vertical', x: 70, y: 90 });
    expect(findSnap({ x: 81.4, y: 72.6 }, data, sol, { tolerance: 0.1, grid: 1 })).toMatchObject({ kind: 'grid', x: 81, y: 73 });
  });

  it('excluded elements are not snap targets', () => {
    expect(findSnap({ x: 60, y: 0 }, data, sol, { tolerance: 1, excludePoints: new Set([1]), excludeEntities: new Set([1, 2]) }).kind).toBe('free');
  });

  it('nearest point on a polyline and grid step', () => {
    expect(nearestOnPolyline([0, 0, 10, 0, 10, 10], 12, 5)).toEqual({ x: 10, y: 5, distance: 2 });
    expect(gridStep(6)).toBe(2);
    expect(gridStep(1)).toBe(20);
    expect(gridStep(100)).toBe(0.2);
  });
});

describe('drawing tools', () => {
  it('circumcentre, arc direction, tangent arc, ellipse, knots', () => {
    expect(circumcenter({ x: 10, y: 0 }, { x: 0, y: 10 }, { x: -10, y: 0 })).toEqual({ x: 0, y: 0 });
    expect(circumcenter({ x: 0, y: 0 }, { x: 1, y: 1 }, { x: 2, y: 2 })).toBeNull();
    const c = { x: 0, y: 0 };
    expect(isCounterClockwise(c, { x: 1, y: 0 }, { x: 0, y: 1 }, { x: -1, y: 0 })).toBe(true);
    expect(isCounterClockwise(c, { x: 1, y: 0 }, { x: 0, y: -1 }, { x: -1, y: 0 })).toBe(false);
    // Leaving (10, 0) along +x, ending at (10, 20): the half circle of radius 10 centred at (10, 10).
    const t = tangentArc({ x: 10, y: 0 }, { x: 1, y: 0 }, { x: 10, y: 20 })!;
    expect(t.center.x).toBeCloseTo(10, 12);
    expect(t.center.y).toBeCloseTo(10, 12);
    expect(t.counterClockwise).toBe(true);
    expect(tangentArc({ x: 0, y: 0 }, { x: 1, y: 0 }, { x: 5, y: 0 })).toBeNull();
    // Semi-axes 10 (x) and 6: focus at c = 8 on the major axis; the order of the axes does not matter.
    const e = ellipseFrom({ x: 0, y: 0 }, { x: 10, y: 0 }, { x: 0, y: 6 })!;
    expect(e).toEqual({ focus: { x: 8, y: 0 }, major: 10, minor: 6 });
    // The picked axis is the shorter one: the major axis is the perpendicular one.
    const e2 = ellipseFrom({ x: 0, y: 0 }, { x: 0, y: 6 }, { x: 10, y: 0 })!;
    expect([e2.major, e2.minor, Math.abs(e2.focus.x), e2.focus.y + 0]).toEqual([10, 6, 8, 0]);
    expect(ellipseFrom({ x: 0, y: 0 }, { x: 10, y: 0 }, { x: 10, y: 0 })).toBeNull();
    const e3 = ellipseFrom({ x: 0, y: 0 }, { x: 6, y: 0 }, { x: 3, y: 10 * Math.sqrt(0.75) })!;
    expect(e3.major).toBeCloseTo(10, 12);
    expect(e3.minor).toBeCloseTo(6, 12);
    expect(e3.focus.x).toBeCloseTo(0, 12);
    expect(e3.focus.y).toBeCloseTo(8, 12);
    expect(clampedKnots(5)).toEqual({ degree: 3, knots: [0, 1, 2], multiplicities: [4, 1, 4] });
    expect(clampedKnots(2)).toEqual({ degree: 1, knots: [0, 1], multiplicities: [2, 2] });
  });

  it('rectangle from the origin: fixed origin point, four lines, H/V constraints', () => {
    const r = drawShape(emptySketch(), 'rectangle', [{ x: 0, y: 0, kind: 'origin' }, { x: 50, y: 30, kind: 'free' }], []);
    expect(r.created).toEqual([1, 2, 3, 4]);
    expect(r.data.points).toEqual([
      { x: '0', y: '0', fixed: true },
      { x: '50', y: '0', fixed: false },
      { x: '50', y: '30', fixed: false },
      { x: '0', y: '30', fixed: false },
    ]);
    expect(r.data.constraints.map((c) => `${c.type}:${c.a}-${c.b}`)).toEqual(['horizontal:0-1', 'vertical:1-2', 'horizontal:2-3', 'vertical:3-0']);
  });

  it('centre rectangle: symmetric corners about the centre point', () => {
    const r = drawShape(emptySketch(), 'rectangle-center', [{ x: 0, y: 0, kind: 'origin' }, { x: 20, y: 10, kind: 'free' }], []);
    expect(r.data.points[0]).toEqual({ x: '-20', y: '-10', fixed: false });
    const sym = r.data.constraints.find((c) => c.type === 'symmetric-point')!;
    expect([sym.a, sym.b]).toEqual([0, 2]);
    expect(r.data.points[sym.c]).toEqual({ x: '0', y: '0', fixed: true });
  });

  it('line: snapped ends are reused, nearly horizontal lines get a constraint, chains continue', () => {
    const data = rectangle();
    const r = drawShape(data, 'line', [{ x: 60, y: 40, kind: 'endpoint', point: 2 }, { x: 100, y: 41, kind: 'free' }], []);
    expect(r.data.geometry[4]).toMatchObject({ type: 'line', start: 2, end: 4 });
    expect(r.data.constraints.at(-1)).toMatchObject({ type: 'horizontal', a: 2, b: 4 });
    expect(r.next).toMatchObject({ kind: 'endpoint', point: 4 });
    const v = drawShape(data, 'line', [{ x: 60, y: 40, kind: 'endpoint', point: 2 }, { x: 61, y: 90, kind: 'free' }], []);
    expect(v.data.constraints.at(-1)).toMatchObject({ type: 'vertical' });
    const slanted = drawShape(data, 'line', [{ x: 60, y: 40, kind: 'endpoint', point: 2 }, { x: 80, y: 60, kind: 'free' }], []);
    expect(slanted.data.constraints).toHaveLength(data.constraints.length);
    // Snap to a midpoint / a curve / an intersection: constraints of the new point.
    const m = drawShape(data, 'line', [{ x: 30, y: 0, kind: 'midpoint', entity: 1 }, { x: 30, y: -20, kind: 'free' }], []);
    expect(m.data.constraints.some((c) => c.type === 'midpoint' && c.a === 4 && c.b === 1)).toBe(true);
    const i = drawShape(data, 'point', [{ x: 5, y: 5, kind: 'intersection', entities: [1, 2] }], []);
    expect(i.data.constraints.slice(-2).map((c) => `${c.type}:${c.a}:${c.b}`)).toEqual(['point-on:4:1', 'point-on:4:2']);
    expect(() => drawShape(data, 'line', [{ x: 1, y: 1, kind: 'free' }, { x: 1, y: 1, kind: 'free' }], [])).toThrow(DrawError);
  });

  it('circles, arcs, ellipse and spline definitions', () => {
    const circle = drawShape(emptySketch(), 'circle', [{ x: 30, y: 20, kind: 'free' }, { x: 38, y: 20, kind: 'free' }], []);
    expect(circle.data.geometry[0]).toMatchObject({ type: 'circle', center: 0, radius: '8' });
    const c3 = drawShape(emptySketch(), 'circle-3', [{ x: 10, y: 0, kind: 'free' }, { x: 0, y: 10, kind: 'free' }, { x: -10, y: 0, kind: 'free' }], []);
    expect(c3.data.geometry[0]).toMatchObject({ type: 'circle', radius: '10' });
    expect(c3.data.points[0]).toEqual({ x: '0', y: '0', fixed: false });
    const arc = drawShape(emptySketch(), 'arc', [{ x: 0, y: 0, kind: 'free' }, { x: 10, y: 0, kind: 'free' }, { x: 0, y: 20, kind: 'free' }], []);
    expect(arc.data.geometry[0]).toMatchObject({ type: 'arc', center: 0, start: 1, end: 2, counterClockwise: true });
    expect(arc.data.points[2]).toEqual({ x: '0', y: '10', fixed: false }); // end put on the circle
    const a3 = drawShape(emptySketch(), 'arc-3', [{ x: 10, y: 0, kind: 'free' }, { x: -10, y: 0, kind: 'free' }, { x: 0, y: -10, kind: 'free' }], []);
    expect(a3.data.geometry[0]).toMatchObject({ type: 'arc', counterClockwise: false });
    const el = drawShape(emptySketch(), 'ellipse', [{ x: 0, y: 0, kind: 'free' }, { x: 10, y: 0, kind: 'free' }, { x: 0, y: 6, kind: 'free' }], []);
    expect(el.data.geometry[0]).toMatchObject({ type: 'ellipse', center: 0, focus: 1, radius: '6' });
    expect(el.data.points[1]).toEqual({ x: '8', y: '0', fixed: false });
    const sp = drawShape(
      emptySketch(),
      'spline',
      [0, 1, 2, 3].map((i) => ({ x: i * 10, y: i % 2 ? 5 : 0, kind: 'free' as const })),
      [],
      { construction: true },
    );
    expect(sp.data.geometry[0]).toMatchObject({ type: 'bspline', poles: [0, 1, 2, 3], degree: 3, knots: [0, 1], multiplicities: [4, 4], construction: true });
  });

  it('tangent arc continues a line and gets a tangent constraint', () => {
    // Open chain: bottom, right, left (the top line removed); point 2 ends the right line only.
    const data = removeItems(rectangle(), [entityRef(3)]).data;
    const positions = rectangleSolution().points;
    // Continuing the right line (1 -> 2) through its start (60, 0) means going down.
    expect(endDirection(data, 1, positions)).toEqual({ entity: 2, dir: { x: 0, y: -1 } });
    expect(endDirection(data, 2, positions)).toEqual({ entity: 2, dir: { x: 0, y: 1 } });
    const r = drawShape(data, 'arc-tangent', [{ x: 60, y: 40, kind: 'endpoint', point: 2 }, { x: 40, y: 40, kind: 'free' }], positions);
    // Line 2 (1 -> 2) arrives at (60, 40) going +y; the arc to (40, 40) is centred at (50, 40), ccw.
    const arc = r.data.geometry[3];
    expect(arc).toMatchObject({ type: 'arc', start: 2, counterClockwise: true });
    expect(r.data.points[arc.center]).toEqual({ x: '50', y: '40', fixed: false });
    expect(r.data.constraints.at(-1)).toMatchObject({ type: 'tangent', a: 2, b: 4 });
    expect(() => drawShape(data, 'arc-tangent', [{ x: 5, y: 5, kind: 'free' }, { x: 9, y: 9, kind: 'free' }], positions)).toThrow(DrawError);
  });
});

describe('selection, deletion, diagnostics', () => {
  it('constraints offered for selections', () => {
    const data = rectangle();
    data.points.push({ x: '30', y: '20', fixed: false });
    data.geometry.push({ type: 'circle', start: -1, end: -1, center: 4, radius: '8', counterClockwise: true, construction: false, projection: -1 });
    const types = (sel: Parameters<typeof applicableConstraints>[1]) => applicableConstraints(data, sel).map((p) => p.type);
    expect(types([pointRef(0), pointRef(2)])).toEqual(['coincident', 'horizontal', 'vertical', 'distance', 'distance-x', 'distance-y']);
    expect(types([entityRef(1)])).toEqual(['horizontal', 'vertical', 'distance', 'distance-x', 'distance-y']);
    expect(applicableConstraints(data, [entityRef(1)])[2]).toEqual({ type: 'distance', a: 0, b: 1, c: -1 });
    expect(types([entityRef(1), entityRef(2)])).toEqual(['parallel', 'perpendicular', 'angle', 'equal']);
    expect(types([entityRef(1), entityRef(5)])).toEqual(['tangent']);
    expect(types([entityRef(5)])).toEqual(['radius', 'diameter']);
    expect(types([pointRef(4), entityRef(1)])).toEqual(['midpoint', 'point-on', 'point-curve-distance']);
    expect(types([pointRef(0), pointRef(2), entityRef(1)])).toEqual(['symmetric']);
    expect(types([pointRef(0), pointRef(1), pointRef(2)])).toEqual(['symmetric-point', 'rotation']);
    expect(types([pointRef(3)])).toEqual(['fix']);
  });

  it('deleting a line removes its constraints and renumbers', () => {
    const data = rectangle();
    data.regions = [3];
    const { data: out, constraintMap } = removeItems(data, [entityRef(1)]);
    // Points 0 and 1 are still used by lines 4 and 2: kept.
    expect(out.points).toHaveLength(4);
    expect(out.geometry.map((g) => [g.start, g.end])).toEqual([
      [1, 2],
      [2, 3],
      [3, 0],
    ]);
    // H(0,1) and distance(0,1) referred to points only — they stay.
    expect(constraintMap).toEqual([0, 1, 2, 3, 4, 5, 6]);
    expect(out.regions).toEqual([2]);
    // Deleting two adjacent lines frees their shared corner.
    const two = removeItems(data, [entityRef(1), entityRef(2)]);
    expect(two.data.points).toHaveLength(3);
    expect(two.data.constraints.map((c) => c.type)).toEqual(['horizontal', 'vertical']);
    expect(two.constraintMap).toEqual([0, 0, 0, 1, 2, 0, 0]);
    // Deleting one constraint.
    expect(removeItems(data, [], [5]).data.constraints).toHaveLength(5);
  });

  it('construction toggle, corners', () => {
    const t = toggleConstruction(rectangle(), [1, 2]);
    expect(t.geometry.map((g) => g.construction)).toEqual([true, true, false, false]);
    expect(toggleConstruction(t, [1]).geometry[0].construction).toBe(false);
    expect(cornerLines(rectangle(), 1)).toEqual([1, 2]);
  });

  it('conflict highlights override the definition state', () => {
    const data = rectangle();
    data.constraints.push({ type: 'distance', a: 0, b: 1, c: -1, value: '50' });
    const sol = rectangleSolution({ conflicting: [5, 7], definition: 'over-defined' });
    sol.points[3].defined = false;
    sol.curves[2].defined = false;
    const h = highlights(data, sol);
    expect([...h.conflicting]).toEqual([5, 7]);
    expect(definitionState(pointRef(1), sol, h)).toBe('conflict');
    expect(definitionState(pointRef(2), sol, h)).toBe('defined');
    expect(definitionState(pointRef(3), sol, h)).toBe('under');
    expect(definitionState(entityRef(3), sol, h)).toBe('under');
  });
});

describe('dimension layout', () => {
  it('labels and the dimension line of a distance', () => {
    expect(dimensionLabel({ type: 'radius', a: 1, b: -1, c: -1, value: '5' }, 5)).toBe('R5');
    expect(dimensionLabel({ type: 'diameter', a: 1, b: -1, c: -1, value: '16', driven: true }, 16)).toBe('(⌀16)');
    expect(dimensionLabel({ type: 'distance-x', a: 0, b: 1, c: -1, value: '-20' }, -20)).toBe('20');
    expect(dimensionLabel({ type: 'angle', a: 1, b: 2, c: -1, value: '30' }, 30.00004)).toBe('30°');
    const g = dimensionGraphic(rectangle(), rectangleSolution(), 5, 4)!;
    // Bottom line (0,0)-(60,0): normal +y, text 4 above the middle, dimension line at y = 4.
    expect(g.text).toEqual({ x: 30, y: 4 });
    expect(g.lines[2]).toEqual([0, 4, 60, 4]);
    expect(g.label).toBe('60');
    const moved = dimensionGraphic(rectangle(), rectangleSolution(), 5, 4, { x: 5, y: -10 })!;
    expect(moved.text).toEqual({ x: 35, y: -6 });
    expect(moved.lines[2]).toEqual([0, -6, 60, -6]);
  });

  it('smart dimension type from the place of the text', () => {
    const a = { x: 0, y: 0 };
    const b = { x: 30, y: 20 };
    expect(twoPointDimension(a, b, { x: 15, y: 30 }, 0.5)).toBe('distance-x');
    expect(twoPointDimension(a, b, { x: 40, y: 10 }, 0.5)).toBe('distance-y');
    expect(twoPointDimension(a, b, { x: 15, y: 10 }, 0.5)).toBe('distance');
    expect(twoPointDimension(a, { x: 30, y: 0 }, { x: 15, y: 30 }, 0.5)).toBe('distance');
  });
});
