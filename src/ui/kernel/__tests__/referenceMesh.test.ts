/**
 * @jest-environment node
 */
// Reference meshes in the viewport tools (docs/ui/PART_MODELING_UI.md, "Справочная сетка"):
// mesh picks of a distance, the kernel spec of a mesh point, the union box of the section slider,
// the offset/rotation fields of the panel and the Euler angles of the assembly mesh placement.
import { addPick, meshSnap, measureSpec, unionBox } from '../viewTools';
import type { MeasurePick } from '../viewTools';
import { parseVec3, formatVec3 } from '../ReferenceMeshPanel';
import { eulerToQuaternion, quaternionToEuler } from '../../assembly/assemblyMath';

const meshPick = (triangle: number, point: [number, number, number]): MeasurePick => ({ kind: 'mesh', index: triangle, mesh: 'Mesh001', point, snap: 0.1 });

describe('mesh picks', () => {
  test('a distance takes two points, even on one triangle; the same point again is removed', () => {
    let picks = addPick([], meshPick(3, [0, 0, 10]), 'distance');
    picks = addPick(picks, meshPick(3, [9, 9, 10]), 'distance');
    expect(picks).toHaveLength(2);
    expect(addPick(picks.slice(0, 1), meshPick(3, [0.05, 0, 10]), 'distance')).toHaveLength(0);
    const spec = measureSpec('distance', picks, false);
    expect(spec).toEqual({
      type: 'distance',
      a: { kind: 'mesh', mesh: 'Mesh001', triangle: 3, point: [0, 0, 10], snap: 0.1 },
      b: { kind: 'mesh', mesh: 'Mesh001', triangle: 3, point: [9, 9, 10], snap: 0.1 },
    });
  });

  test('a mesh point mixes with a face in a distance and is refused by other measurements', () => {
    const picks = addPick([meshPick(0, [1, 2, 3])], { kind: 'face', index: 4 }, 'distance');
    expect(measureSpec('distance', picks, false)?.b).toEqual({ kind: 'face', index: 4 });
    for (const type of ['angle', 'length', 'area', 'radius'] as const) expect(addPick([], meshPick(0, [0, 0, 0]), type)).toEqual([]);
  });

  test('snapping distance: 1 % of the diagonal', () => {
    expect(meshSnap([30, 40, 0])).toBeCloseTo(0.5, 12);
  });
});

test('union box of the model and the meshes', () => {
  expect(unionBox([null, null])).toBeNull();
  expect(unionBox([{ min: [0, 0, 0], max: [1, 1, 1] }, null, { min: [-5, 0, 0.5], max: [0, 3, 0.7] }])).toEqual({ min: [-5, 0, 0], max: [1, 3, 1] });
});

test('offset and rotation fields', () => {
  expect(parseVec3('10, -2.5, 0')).toEqual([10, -2.5, 0]);
  expect(parseVec3('1 2 3')).toEqual([1, 2, 3]);
  expect(parseVec3('1; 2')).toBeNull();
  expect(parseVec3('a, 1, 2')).toBeNull();
  expect(formatVec3([0.1 + 0.2, 0, -1])).toBe('0.3, 0, -1');
});

test('Euler angles of R = Rz·Ry·Rx and back', () => {
  // 90° about Z: quaternion (0, 0, sin 45°, cos 45°).
  const q = eulerToQuaternion([0, 0, 90]);
  expect(q[2]).toBeCloseTo(Math.SQRT1_2, 12);
  expect(q[3]).toBeCloseTo(Math.SQRT1_2, 12);
  for (const e of [[10, 20, 30], [-45, 60, 170], [0, 0, 0]] as [number, number, number][]) {
    const back = quaternionToEuler(eulerToQuaternion(e));
    back.forEach((v, i) => expect(v).toBeCloseTo(e[i], 9));
  }
});
