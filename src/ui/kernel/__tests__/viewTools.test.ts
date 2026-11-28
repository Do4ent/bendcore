/**
 * @jest-environment node
 */
// Logic of the measurement and section tools (docs/ui/KERNEL_VIEWPORT.md, "Измерения и сечение").
import {
  addPick,
  boxOfPositions,
  clipEquation,
  formatMeasure,
  isComplete,
  measureOverlay,
  measureSpec,
  picksNeeded,
  removedSide,
  sectionPlane,
} from '../viewTools';
import type { MeasurePick, MeasureResult } from '../viewTools';

const face = (index: number, path?: number[]): MeasurePick => ({ kind: 'face', index, path });
const edge = (index: number): MeasurePick => ({ kind: 'edge', index });

describe('measurement picks', () => {
  it('needs two elements for a distance and an angle, one for the rest', () => {
    expect(picksNeeded('distance')).toBe(2);
    expect(picksNeeded('angle')).toBe(2);
    expect(picksNeeded('length')).toBe(1);
    expect(picksNeeded('area')).toBe(1);
    expect(picksNeeded('radius')).toBe(1);
  });

  it('collects two picks, toggles a repeated one and restarts after a complete measurement', () => {
    let picks = addPick([], face(1), 'distance');
    expect(picks).toEqual([face(1)]);
    picks = addPick(picks, face(1), 'distance');
    expect(picks).toEqual([]);
    picks = addPick(addPick([], face(1), 'distance'), edge(4), 'distance');
    expect(picks).toHaveLength(2);
    expect(isComplete(picks, 'distance')).toBe(true);
    expect(addPick(picks, face(7), 'distance')).toEqual([face(7)]);
  });

  it('keeps the angle to two faces or two edges', () => {
    const first = addPick([], face(1), 'angle');
    expect(addPick(first, edge(2), 'angle')).toEqual([edge(2)]);
    expect(addPick(first, face(3), 'angle')).toHaveLength(2);
  });

  it('accepts only edges for a length and only faces for an area', () => {
    expect(addPick([], face(1), 'length')).toEqual([]);
    expect(addPick([], edge(1), 'length')).toEqual([edge(1)]);
    expect(addPick([], edge(1), 'area')).toEqual([]);
    expect(addPick([], face(1), 'area')).toEqual([face(1)]);
    expect(addPick([], face(1), 'radius')).toEqual([face(1)]);
  });

  it('tells occurrences of an assembly apart by their path', () => {
    const a = face(0, [1]);
    const b = face(0, [2]);
    expect(addPick([a], b, 'distance')).toHaveLength(2);
    expect(addPick([a], face(0, [1]), 'distance')).toEqual([]);
  });

  it('builds the kernel spec only from a complete selection', () => {
    expect(measureSpec('distance', [face(1)], false)).toBeNull();
    expect(measureSpec('distance', [face(1), edge(2)], false)).toEqual({
      type: 'distance',
      a: { kind: 'face', index: 1 },
      b: { kind: 'edge', index: 2 },
    });
    expect(measureSpec('area', [face(5, [3, 4])], true)).toEqual({ type: 'area', a: { path: [3, 4], kind: 'face', index: 5 } });
  });
});

describe('measurement readout', () => {
  it('formats lengths, areas and angles', () => {
    expect(formatMeasure({ value: 12.34567, unit: 'mm' })).toBe('12.346 mm');
    expect(formatMeasure({ value: 100, unit: 'mm2' })).toBe('100 mm²');
    expect(formatMeasure({ value: 90.004, unit: 'deg' })).toBe('90°');
  });

  it('draws a distance between the nearest points and labels its middle', () => {
    const m: MeasureResult = { type: 'distance', unit: 'mm', value: 10, points: [[0, 0, 0], [10, 0, 0]] };
    expect(measureOverlay(m)).toEqual({ lines: [[[0, 0, 0], [10, 0, 0]]], anchor: [5, 0, 0] });
  });

  it('draws the two rays of an angle from its vertex', () => {
    const m: MeasureResult = { type: 'angle', unit: 'deg', value: 90, points: [[0, 0, 0], [1, 0, 0], [0, 1, 0]] };
    const o = measureOverlay(m);
    expect(o.lines).toEqual([[[0, 0, 0], [1, 0, 0]], [[0, 0, 0], [0, 1, 0]]]);
    expect(o.anchor).toEqual([0, 0, 0]);
  });
});

describe('section plane', () => {
  const box = { min: [0, -10, 0] as [number, number, number], max: [60, 10, 20] as [number, number, number] };

  it('puts the plane at a fraction of the box along the axis', () => {
    const p = sectionPlane({ axis: 'X', fraction: 0.25, flip: false }, box);
    expect(p.position).toBe(15);
    expect(p.normal).toEqual([1, 0, 0]);
    expect(p.origin).toEqual([15, 0, 10]);
    expect(sectionPlane({ axis: 'Z', fraction: 2, flip: true }, box).position).toBe(20);
    expect(sectionPlane({ axis: 'Z', fraction: 2, flip: true }, box).normal).toEqual([0, 0, -1]);
  });

  it('keeps the side opposite to the normal, like the kernel clipByPlane', () => {
    const p = sectionPlane({ axis: 'X', fraction: 0.5, flip: false }, box);
    expect(removedSide(p, [10, 0, 0])).toBeLessThan(0);
    expect(removedSide(p, [50, 0, 0])).toBeGreaterThan(0);
    const { normal, constant } = clipEquation(p);
    // three.js keeps normal . x + constant >= 0
    const kept = (x: [number, number, number]) => normal[0] * x[0] + normal[1] * x[1] + normal[2] * x[2] + constant >= 0;
    expect(kept([10, 0, 0])).toBe(true);
    expect(kept([50, 0, 0])).toBe(false);
  });

  it('measures the box of a mesh', () => {
    expect(boxOfPositions(new Float32Array([0, 1, 2, 4, -1, 3]))).toEqual({ min: [0, -1, 2], max: [4, 1, 3] });
    expect(boxOfPositions(new Float32Array([]))).toBeNull();
  });
});
