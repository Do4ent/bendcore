// three.js object of a reference mesh (docs/ui/PART_MODELING_UI.md, "Справочная сетка"), shared
// by the part viewport (KernelViewport) and the assembly viewport: translucent faces with normals
// from the kernel's triangles; userData.key is the mesh id for picking.

import * as THREE from 'three';

export const REFERENCE_MESH_COLOR = 0xb07d2b;

/** Reference mesh drawn translucent; positions in model/world mm. */
export interface ReferenceMeshDisplay {
  key: string | number;
  positions: Float32Array;
  indices: Uint32Array;
  opacity: number;
}

/** A click on a reference mesh: its key, the kernel triangle and the hit point. */
export interface MeshPick {
  key: string | number;
  triangle: number;
  point: [number, number, number];
}

export function buildReferenceMesh(m: ReferenceMeshDisplay): THREE.Mesh {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(m.positions, 3));
  geometry.setIndex(new THREE.BufferAttribute(m.indices, 1));
  geometry.computeVertexNormals();
  geometry.computeBoundingSphere();
  const material = new THREE.MeshPhongMaterial({
    color: REFERENCE_MESH_COLOR,
    transparent: m.opacity < 1,
    opacity: m.opacity,
    side: THREE.DoubleSide,
    depthWrite: m.opacity >= 1,
  });
  const mesh = new THREE.Mesh(geometry, material);
  mesh.name = 'reference-mesh';
  mesh.userData.key = m.key;
  mesh.renderOrder = 5;
  return mesh;
}

/** First hit on a reference mesh in `group` that is not clipped away (null: none). */
export function referenceMeshHit(
  raycaster: THREE.Raycaster,
  group: THREE.Group,
  kept: (p: THREE.Vector3) => boolean = () => true,
): { pick: MeshPick; distance: number } | null {
  if (group.children.length === 0) return null;
  const hit = raycaster.intersectObjects(group.children, false).find((h) => kept(h.point));
  if (!hit || hit.faceIndex === undefined || hit.faceIndex === null) return null;
  return {
    pick: { key: hit.object.userData.key as string | number, triangle: hit.faceIndex, point: [hit.point.x, hit.point.y, hit.point.z] },
    distance: hit.distance,
  };
}
