/**
 * Automatic "show all" of the 3D viewports (docs/ui/KERNEL_VIEWPORT.md, "Автоматическое
 * вписывание"). The view is framed when a document is opened (frameKey) and, after that, only when
 * an operation adds a body (a part body, an assembly instance) that the camera does not show:
 * ordinary edits — a fillet, a new extrusion length, a moved component — keep the camera the user
 * set.
 */

import * as THREE from 'three';

/** Whether all of `box` is inside the view frustum of `camera` (matrices up to date). */
export function boxInView(camera: THREE.Camera, box: THREE.Box3): boolean {
  if (box.isEmpty()) return true;
  camera.updateMatrixWorld();
  const p = new THREE.Vector3();
  for (let i = 0; i < 8; ++i) {
    p.set(i & 1 ? box.max.x : box.min.x, i & 2 ? box.max.y : box.min.y, i & 4 ? box.max.z : box.min.z);
    p.project(camera);
    if (!(Math.abs(p.x) <= 1 && Math.abs(p.y) <= 1 && p.z >= -1 && p.z <= 1)) return false;
  }
  return true;
}

/** `outer` contains `inner` up to `tol` (relative to the size of `outer`). */
export function boxContains(outer: THREE.Box3, inner: THREE.Box3, tol = 1e-6): boolean {
  if (inner.isEmpty()) return true;
  if (outer.isEmpty()) return false;
  const eps = tol * Math.max(outer.getSize(new THREE.Vector3()).length(), 1);
  return (
    inner.min.x >= outer.min.x - eps &&
    inner.min.y >= outer.min.y - eps &&
    inner.min.z >= outer.min.z - eps &&
    inner.max.x <= outer.max.x + eps &&
    inner.max.y <= outer.max.y + eps &&
    inner.max.z <= outer.max.z + eps
  );
}

export interface ReframeInput {
  /** bodies of the model shown before and now (null: unknown) */
  bodiesBefore: number | null;
  bodies: number | null;
  /** bounding box of the model before and now */
  boxBefore: THREE.Box3;
  box: THREE.Box3;
  camera: THREE.Camera;
}

/**
 * A part: re-frame when the number of bodies grew, the model grew beyond its previous bounding box
 * (the new body is not inside the old model) and the camera does not show the whole model.
 */
export function shouldReframePart({ bodiesBefore, bodies, boxBefore, box, camera }: ReframeInput): boolean {
  if (bodiesBefore === null || bodies === null || bodies <= bodiesBefore) return false;
  if (boxContains(boxBefore, box)) return false;
  return !boxInView(camera, box);
}

/** An assembly: re-frame when one of the instances added by the last change is not in view. */
export function shouldReframeInstances(added: readonly THREE.Box3[], camera: THREE.Camera): boolean {
  return added.some((b) => !boxInView(camera, b));
}
