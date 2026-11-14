/**
 * @jest-environment node
 */
// Automatic "show all" of the viewports (docs/ui/KERNEL_VIEWPORT.md, "Автоматическое вписывание").
import * as THREE from 'three';
import { boxContains, boxInView, shouldReframeInstances, shouldReframePart } from '../autoFrame';

/** Camera at (0, 0, 100) looking at the origin, 60° vertical field of view, square image. */
function camera(): THREE.PerspectiveCamera {
  const c = new THREE.PerspectiveCamera(60, 1, 1, 1000);
  c.position.set(0, 0, 100);
  c.lookAt(0, 0, 0);
  c.updateProjectionMatrix();
  return c;
}

const box = (min: [number, number, number], max: [number, number, number]) => new THREE.Box3(new THREE.Vector3(...min), new THREE.Vector3(...max));

// Half width of the view at z = 0: 100·tan 30° = 57.7.
const HALF = 100 * Math.tan(Math.PI / 6);

describe('frustum test', () => {
  test('a box is in view when every corner projects inside the image', () => {
    expect(boxInView(camera(), box([-10, -10, -10], [10, 10, 10]))).toBe(true);
    expect(boxInView(camera(), box([-HALF + 1, -HALF + 1, 0], [HALF - 1, HALF - 1, 0]))).toBe(true);
    // One corner beyond the edge of the image.
    expect(boxInView(camera(), box([0, 0, 0], [HALF + 1, 10, 0]))).toBe(false);
    // Behind the camera and beyond the far plane.
    expect(boxInView(camera(), box([-1, -1, 150], [1, 1, 160]))).toBe(false);
    expect(boxInView(camera(), box([-1, -1, -1000], [1, 1, -950]))).toBe(false);
    expect(boxInView(camera(), new THREE.Box3())).toBe(true);
  });

  test('containment with a relative tolerance', () => {
    expect(boxContains(box([0, 0, 0], [20, 10, 10]), box([0, 0, 0], [20, 10, 10]))).toBe(true);
    expect(boxContains(box([0, 0, 0], [20, 10, 10]), box([5, 0, 0], [20, 10, 10 + 1e-9]))).toBe(true);
    expect(boxContains(box([0, 0, 0], [20, 10, 10]), box([0, 0, 0], [50, 10, 10]))).toBe(false);
    expect(boxContains(new THREE.Box3(), box([0, 0, 0], [1, 1, 1]))).toBe(false);
  });
});

describe('when the part view is re-framed', () => {
  const block = box([0, 0, 0], [20, 10, 10]);

  test('a pattern adds bodies outside the view: re-framed', () => {
    // Bodies at x = 0, 30, 60 … 180 (the last ones beyond the image).
    const pattern = box([0, 0, 0], [200, 10, 10]);
    expect(shouldReframePart({ bodiesBefore: 1, bodies: 7, boxBefore: block, box: pattern, camera: camera() })).toBe(true);
  });

  test('a new body inside the view: the camera stays', () => {
    const near = box([0, 0, 0], [50, 10, 10]);
    expect(shouldReframePart({ bodiesBefore: 1, bodies: 2, boxBefore: block, box: near, camera: camera() })).toBe(false);
  });

  test('ordinary edits keep the camera even when the model leaves the view', () => {
    const tall = box([0, 0, 0], [20, 10, 500]);
    // Same number of bodies: a longer extrusion, a fillet.
    expect(shouldReframePart({ bodiesBefore: 1, bodies: 1, boxBefore: block, box: tall, camera: camera() })).toBe(false);
    // Fewer bodies (delete, union).
    expect(shouldReframePart({ bodiesBefore: 3, bodies: 2, boxBefore: tall, box: tall, camera: camera() })).toBe(false);
    // A body split inside the old model.
    const big = box([-100, -100, -100], [100, 100, 100]);
    expect(shouldReframePart({ bodiesBefore: 1, bodies: 2, boxBefore: big, box: big, camera: camera() })).toBe(false);
    // Unknown counts (the document is being read).
    expect(shouldReframePart({ bodiesBefore: null, bodies: 3, boxBefore: block, box: tall, camera: camera() })).toBe(false);
  });

  test('assembly: only the added instances count', () => {
    expect(shouldReframeInstances([], camera())).toBe(false);
    expect(shouldReframeInstances([box([0, 0, 0], [10, 10, 10])], camera())).toBe(false);
    expect(shouldReframeInstances([box([0, 0, 0], [10, 10, 10]), box([300, 0, 0], [310, 10, 10])], camera())).toBe(true);
  });
});
