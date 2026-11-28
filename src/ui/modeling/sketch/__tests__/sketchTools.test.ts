// Sketch editor support of the kernel tools (docs/ui/SKETCH_EDITOR.md): offset dimensions
// (one driving dimension, the others follow it) in deletion and captions, and the picking of
// model edges and faces for the projection tool.

import type { KernelMesh } from '../../../../wasm/bendcore_kernel_api.mjs';
import type { SketchData, SketchPlacementInfo } from '../../../../wasm/bendcore_modeling_api.mjs';
import { dimensionLabel } from '../dimensionLayout';
import { pickBodyElement, toSketchFrame } from '../projectPick';
import { describeConstraint } from '../sketchLabels';
import { addConstraint, addGeometry, addPoint, emptySketch, entityRef, isDimension, masterOf, removeItems } from '../sketchModel';

/** Two source circles and their offsets: offset dimension #1 drives #2. */
function offsetSketch(): SketchData {
  const d = emptySketch();
  const c = addPoint(d, 0, 0, true);
  const c2 = addPoint(d, 50, 0, true);
  const a = addGeometry(d, { type: 'circle', center: c, radius: '10' });
  const b = addGeometry(d, { type: 'circle', center: c2, radius: '10' });
  const oa = addGeometry(d, { type: 'circle', center: c, radius: '13' });
  const ob = addGeometry(d, { type: 'circle', center: c2, radius: '13' });
  addConstraint(d, 'offset-distance', oa, a, -1, '3');
  addConstraint(d, 'offset-distance', ob, b, 1, '0');
  return d;
}

describe('offset dimensions', () => {
  it('the first is a dimension, the second follows it', () => {
    const d = offsetSketch();
    expect(isDimension(d.constraints[0])).toBe(true);
    expect(isDimension(d.constraints[1])).toBe(false);
    expect(masterOf(d.constraints[1])).toBe(1);
    expect(dimensionLabel(d.constraints[0], 3)).toBe('3');
    expect(describeConstraint(d.constraints[1], 3)).toBe('Смещение к4, к2 = размер #1: 3');
  });

  it('deleting a curve renumbers the master; deleting the master takes its followers', () => {
    const d = offsetSketch();
    // Drop circle 2 (operand of #2 only): #1 stays, #2 goes.
    let r = removeItems(d, [entityRef(2)]);
    expect(r.data.constraints).toHaveLength(1);
    expect(r.constraintMap).toEqual([0, 1, 0]);
    // A constraint before the master: the follower's c moves with the master's number.
    const e = offsetSketch();
    e.constraints.unshift({ type: 'fix', a: 0, b: -1, c: -1, value: '0' });
    e.constraints[2].c = 2;
    r = removeItems(e, [], [1]);
    expect(r.data.constraints.map((c) => [c.type, c.c])).toEqual([
      ['offset-distance', -1],
      ['offset-distance', 1],
    ]);
    // Removing the master constraint removes the follower.
    r = removeItems(offsetSketch(), [], [1]);
    expect(r.data.constraints).toHaveLength(0);
  });
});

describe('projection picking', () => {
  // Sketch on the top face z = 10 of a 60 x 40 x 10 box; the view looks down -Z.
  const placement: SketchPlacementInfo = { origin: [0, 0, 10], xDirection: [1, 0, 0], yDirection: [0, 1, 0], normal: [0, 0, 1] };
  const quad = (z: number): number[] => [0, 0, z, 60, 0, z, 60, 40, z, 0, 40, z];
  const body: KernelMesh = {
    positions: new Float32Array([...quad(0), ...quad(10)]),
    normals: new Float32Array(24),
    indices: new Uint32Array([0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7]),
    faceSpans: new Uint32Array([4, 0, 6, 9, 6, 6]),
    edges: [
      { edgeIndex: 3, points: new Float32Array([0, 0, 10, 60, 0, 10]) },
      { edgeIndex: 7, points: new Float32Array([60, 0, 10, 60, 40, 10]) },
    ],
    missingFaces: new Uint32Array(),
    missingEdges: new Uint32Array(),
  };

  it('moves model points into the sketch frame', () => {
    const tilted: SketchPlacementInfo = { origin: [1, 2, 3], xDirection: [0, 1, 0], yDirection: [0, 0, 1], normal: [1, 0, 0] };
    expect(toSketchFrame(tilted)(5, 7, 13)).toEqual([5, 10, 4]);
  });

  it('prefers the nearest edge within the tolerance, else the front face', () => {
    expect(pickBodyElement(body, placement, 30, 0.5, 1)).toEqual({ kind: 'edge', index: 3 });
    expect(pickBodyElement(body, placement, 59.6, 20, 1)).toEqual({ kind: 'edge', index: 7 });
    // Inside: the top face (z = 10) is in front of the bottom one (z = 0).
    expect(pickBodyElement(body, placement, 30, 20, 1)).toEqual({ kind: 'face', index: 9 });
    expect(pickBodyElement(body, placement, 100, 100, 1)).toBeNull();
  });
});
