/**
 * AssemblyViewport: three.js view of all placed parts of an assembly (docs/ui/ASSEMBLY_UI.md).
 *
 * Geometry is the kernel's: one tessellation per part definition (positions, normals, indices,
 * face spans, edge polylines from AssemblyDocumentHandle.tessellate) shared by every occurrence;
 * each occurrence is a group with the world matrix the assembly document computed. A click casts
 * a ray against all occurrences; the hit triangle maps to its B-rep face by faceOfTriangle, the
 * hit object to its occurrence path. In edge mode the nearest kernel edge on screen is picked
 * (edgePick.ts) unless a face hides it. In drag mode a press on a part starts a drag: the cursor
 * ray is intersected with the plane through the grabbed point — the grabbed face's own plane
 * when it is planar and not seen edge-on, else the plane facing the camera — and the result
 * goes to onDragMove, which re-solves the mates in the kernel and sends back new placements.
 */

import React, { useEffect, useRef } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import { faceOfTriangle } from '../../wasm/bendcore_assembly_api.mjs';
import type { GeometryRef, KernelMesh, PlacedPart } from '../../wasm/bendcore_assembly_api.mjs';
import { packEdgePolylines } from '../kernel/edgeBuffers';
import type { EdgeBuffers } from '../kernel/edgeBuffers';
import { closestPointOnSegmentToRay, edgeCandidates } from '../kernel/edgePick';
import { rayPlane } from './assemblyMath';
import type { V3 } from './assemblyMath';

export type AssemblyPickMode = 'face' | 'edge' | 'drag' | 'none';

export interface AssemblyPick {
  path: number[];
  kind: 'face' | 'edge';
  index: number;
  /** World point of the hit, mm. */
  point: V3;
}

export type PartHighlight = 'interference' | 'touching' | 'selected';

export interface AssemblyViewportHandle {
  worldToClient(x: number, y: number, z: number): { x: number; y: number };
  /** Client point of a visible triangle of the face (first hit of a ray through it), or null. */
  faceClientPoint(path: number[], faceIndex: number): { x: number; y: number } | null;
  /** Unit view direction (camera → target), world. */
  viewDirection(): V3;
}

export interface AssemblyViewportProps {
  meshes: Record<string, KernelMesh>;
  parts: PlacedPart[];
  pickMode: AssemblyPickMode;
  selected: GeometryRef[];
  highlights: Record<string, PartHighlight>;
  onPick: (pick: AssemblyPick | null) => void;
  /**
   * Press on face `face` of the part at `path`: false refuses the drag; otherwise the drag plane
   * normal (the grabbed face's plane, so the point slides in it), or null for the plane facing
   * the camera.
   */
  onDragStart?: (path: number[], face: number, point: V3) => false | { planeNormal: V3 | null };
  onDragMove?: (target: V3) => void;
  onDragEnd?: () => void;
  onReady?: (handle: AssemblyViewportHandle | null) => void;
  /** Changing it re-frames the view on the next non-empty scene. */
  frameKey?: number;
}

const BASE_COLOR = 0xb8c4d6;
const SELECTED_COLOR = 0xff8c1a;
const EDGE_COLOR = 0x1d2733;
const HIGHLIGHT_COLORS: Record<PartHighlight, number> = {
  interference: 0xe53935,
  touching: 0xf2c14e,
  selected: 0x7fb0ec,
};
const BACKGROUND = 0xf3f5f8;
const CLICK_TOLERANCE = 4;
const EDGE_TOLERANCE = 6;

export const pathKey = (path: readonly number[]) => path.join('/');

interface Instance {
  key: string;
  path: number[];
  meshKey: string;
  group: THREE.Group;
  faces: THREE.Mesh;
  baseMaterial: THREE.MeshPhongMaterial;
  highlight: THREE.Group;
}

interface SceneState {
  renderer: THREE.WebGLRenderer;
  scene: THREE.Scene;
  camera: THREE.PerspectiveCamera;
  controls: OrbitControls;
  root: THREE.Group;
  instances: Map<string, Instance>;
  edgeBuffers: Map<string, EdgeBuffers>;
  geometries: Map<string, THREE.BufferGeometry>;
  framed: boolean;
  render: () => void;
}

function disposeObject(obj: THREE.Object3D): void {
  obj.traverse((child) => {
    const c = child as THREE.Object3D & { geometry?: THREE.BufferGeometry; material?: THREE.Material | THREE.Material[] };
    c.geometry?.dispose();
    const mats = c.material;
    if (Array.isArray(mats)) mats.forEach((m) => m.dispose());
    else mats?.dispose();
  });
}

function sharedGeometry(mesh: KernelMesh): THREE.BufferGeometry {
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(mesh.positions, 3));
  g.setAttribute('normal', new THREE.BufferAttribute(mesh.normals, 3));
  g.setIndex(new THREE.BufferAttribute(mesh.indices, 1));
  g.computeBoundingSphere();
  return g;
}

/** Per-occurrence geometry: the shared attributes with its own draw groups (face highlight). */
function instanceGeometry(shared: THREE.BufferGeometry, mesh: KernelMesh): THREE.BufferGeometry {
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', shared.getAttribute('position'));
  g.setAttribute('normal', shared.getAttribute('normal'));
  g.setIndex(shared.getIndex()!);
  for (let s = 0; s < mesh.faceSpans.length; s += 3) g.addGroup(mesh.faceSpans[s + 1], mesh.faceSpans[s + 2], 0);
  g.boundingSphere = shared.boundingSphere;
  return g;
}

function edgeLines(buffers: EdgeBuffers, color: number): THREE.LineSegments {
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(buffers.positions, 3));
  g.setIndex(new THREE.BufferAttribute(buffers.indices, 1));
  return new THREE.LineSegments(g, new THREE.LineBasicMaterial({ color }));
}

function frame(state: SceneState): boolean {
  const box = new THREE.Box3();
  for (const inst of state.instances.values()) box.expandByObject(inst.faces);
  if (box.isEmpty()) return false;
  const sphere = box.getBoundingSphere(new THREE.Sphere());
  const { camera, controls } = state;
  const radius = Math.max(sphere.radius, 1e-3);
  const distance = (radius / Math.sin((camera.fov * Math.PI) / 360)) * 1.2;
  const dir = new THREE.Vector3(1, -1.2, 0.9).normalize();
  controls.target.copy(sphere.center);
  camera.position.copy(sphere.center).addScaledVector(dir, distance);
  camera.near = distance / 100;
  camera.far = distance * 20;
  camera.updateProjectionMatrix();
  controls.update();
  return true;
}

function setMatrix(group: THREE.Group, m: number[]): void {
  group.matrix.fromArray(m);
  group.matrixWorldNeedsUpdate = true;
}

const AssemblyViewport: React.FC<AssemblyViewportProps> = ({
  meshes,
  parts,
  pickMode,
  selected,
  highlights,
  onPick,
  onDragStart,
  onDragMove,
  onDragEnd,
  onReady,
  frameKey,
}) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const stateRef = useRef<SceneState | null>(null);
  const meshesRef = useRef(meshes);
  meshesRef.current = meshes;
  const cb = useRef({ onPick, onDragStart, onDragMove, onDragEnd, pickMode });
  cb.current = { onPick, onDragStart, onDragMove, onDragEnd, pickMode };

  useEffect(() => {
    const container = containerRef.current;
    if (!container) return undefined;
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.setClearColor(BACKGROUND, 1);
    container.appendChild(renderer.domElement);
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(BACKGROUND);
    const camera = new THREE.PerspectiveCamera(35, 1, 0.1, 1000);
    camera.up.set(0, 0, 1);
    scene.add(camera);
    scene.add(new THREE.AmbientLight(0xffffff, 0.45));
    const headlight = new THREE.DirectionalLight(0xffffff, 0.7);
    headlight.position.set(0.3, 0.6, 1);
    camera.add(headlight);
    const root = new THREE.Group();
    scene.add(root);
    const controls = new OrbitControls(camera, renderer.domElement);
    controls.screenSpacePanning = true;
    const render = () => renderer.render(scene, camera);
    controls.addEventListener('change', render);
    const state: SceneState = {
      renderer,
      scene,
      camera,
      controls,
      root,
      instances: new Map(),
      edgeBuffers: new Map(),
      geometries: new Map(),
      framed: false,
      render,
    };
    stateRef.current = state;

    const resize = () => {
      const width = Math.max(container.clientWidth, 1);
      const height = Math.max(container.clientHeight, 1);
      renderer.setSize(width, height);
      camera.aspect = width / height;
      camera.updateProjectionMatrix();
      render();
    };
    const observer = new ResizeObserver(resize);
    observer.observe(container);
    resize();

    const rayAt = (clientX: number, clientY: number) => {
      const rect = renderer.domElement.getBoundingClientRect();
      const ndc = new THREE.Vector2(
        ((clientX - rect.left) / rect.width) * 2 - 1,
        -((clientY - rect.top) / rect.height) * 2 + 1,
      );
      const raycaster = new THREE.Raycaster();
      raycaster.setFromCamera(ndc, camera);
      return raycaster;
    };
    const faceHit = (raycaster: THREE.Raycaster) => {
      const objects = [...state.instances.values()].map((i) => i.faces);
      const hit = raycaster.intersectObjects(objects, false)[0];
      if (!hit || hit.faceIndex === undefined) return null;
      const inst = state.instances.get(hit.object.userData.instance as string);
      const mesh = inst ? meshesRef.current[inst.meshKey] : undefined;
      if (!inst || !mesh) return null;
      const face = faceOfTriangle(mesh.faceSpans, hit.faceIndex);
      if (face < 0) return null;
      return { inst, face, hit };
    };
    const project = (world: THREE.Vector3) => {
      const rect = renderer.domElement.getBoundingClientRect();
      const p = world.clone().project(camera);
      return {
        x: rect.left + ((p.x + 1) / 2) * rect.width,
        y: rect.top + ((1 - p.y) / 2) * rect.height,
        visible: p.z > -1 && p.z < 1,
      };
    };

    const pickEdge = (clientX: number, clientY: number): AssemblyPick | null => {
      const raycaster = rayAt(clientX, clientY);
      const fh = faceHit(raycaster);
      const { origin, direction } = raycaster.ray;
      let best: { pick: AssemblyPick; distance: number } | null = null;
      for (const inst of state.instances.values()) {
        const buffers = state.edgeBuffers.get(inst.meshKey);
        if (!buffers) continue;
        inst.group.updateMatrixWorld();
        const m = inst.group.matrixWorld;
        const toWorld = (x: number, y: number, z: number) => new THREE.Vector3(x, y, z).applyMatrix4(m);
        const size = inst.faces.geometry.boundingSphere?.radius ?? 1;
        const vertex = (v: number): [number, number, number] => {
          const w = toWorld(buffers.positions[v * 3], buffers.positions[v * 3 + 1], buffers.positions[v * 3 + 2]);
          return [w.x, w.y, w.z];
        };
        for (const c of edgeCandidates(buffers, (x, y, z) => project(toWorld(x, y, z)), clientX, clientY, EDGE_TOLERANCE)) {
          if (best && best.distance <= c.distance) break;
          const near = closestPointOnSegmentToRay(
            [origin.x, origin.y, origin.z],
            [direction.x, direction.y, direction.z],
            vertex(buffers.indices[c.segment * 2]),
            vertex(buffers.indices[c.segment * 2 + 1]),
          );
          if (fh && near.rayDistance > fh.hit.distance + size * 2e-3) continue;
          best = { pick: { path: inst.path, kind: 'edge', index: c.edgeIndex, point: near.point }, distance: c.distance };
          break;
        }
      }
      return best?.pick ?? null;
    };

    // Drag: capture-phase listener so that OrbitControls is disabled before it sees the press.
    let down: { x: number; y: number } | null = null;
    let drag: { point: THREE.Vector3; normal: THREE.Vector3; pending: V3 | null; frame: number } | null = null;
    const onDragPointerMove = (e: PointerEvent) => {
      if (!drag) return;
      const ray = rayAt(e.clientX, e.clientY).ray;
      const target = rayPlane(
        [ray.origin.x, ray.origin.y, ray.origin.z],
        [ray.direction.x, ray.direction.y, ray.direction.z],
        [drag.point.x, drag.point.y, drag.point.z],
        [drag.normal.x, drag.normal.y, drag.normal.z],
      );
      if (!target) return;
      drag.pending = target;
      if (!drag.frame) {
        drag.frame = requestAnimationFrame(() => {
          if (!drag) return;
          drag.frame = 0;
          const t = drag.pending;
          drag.pending = null;
          if (t) cb.current.onDragMove?.(t);
        });
      }
    };
    const finishDrag = () => {
      if (!drag) return;
      if (drag.frame) cancelAnimationFrame(drag.frame);
      if (drag.pending) cb.current.onDragMove?.(drag.pending);
      drag = null;
      controls.enabled = true;
      window.removeEventListener('pointermove', onDragPointerMove);
      window.removeEventListener('pointerup', onDragPointerUp);
      cb.current.onDragEnd?.();
    };
    const onDragPointerUp = () => finishDrag();

    const onPointerDown = (e: PointerEvent) => {
      down = { x: e.clientX, y: e.clientY };
      if (cb.current.pickMode !== 'drag' || e.button !== 0) return;
      const fh = faceHit(rayAt(e.clientX, e.clientY));
      if (!fh) return;
      const p = fh.hit.point;
      const accepted = cb.current.onDragStart?.(fh.inst.path, fh.face, [p.x, p.y, p.z]);
      if (!accepted) return;
      controls.enabled = false;
      const normal = new THREE.Vector3();
      camera.getWorldDirection(normal);
      // A face plane seen nearly edge-on would throw the target far away: camera plane then.
      const n = accepted.planeNormal;
      if (n && Math.abs(normal.x * n[0] + normal.y * n[1] + normal.z * n[2]) > 0.25) normal.set(n[0], n[1], n[2]);
      drag = { point: p.clone(), normal, pending: null, frame: 0 };
      window.addEventListener('pointermove', onDragPointerMove);
      window.addEventListener('pointerup', onDragPointerUp);
    };
    const onPointerUp = (e: PointerEvent) => {
      if (!down || e.button !== 0) return;
      const moved = Math.hypot(e.clientX - down.x, e.clientY - down.y);
      down = null;
      const mode = cb.current.pickMode;
      if (moved > CLICK_TOLERANCE || mode === 'none' || mode === 'drag') return;
      if (mode === 'edge') {
        cb.current.onPick(pickEdge(e.clientX, e.clientY));
        return;
      }
      const fh = faceHit(rayAt(e.clientX, e.clientY));
      if (!fh) {
        cb.current.onPick(null);
        return;
      }
      const p = fh.hit.point;
      cb.current.onPick({ path: fh.inst.path, kind: 'face', index: fh.face, point: [p.x, p.y, p.z] });
    };
    renderer.domElement.addEventListener('pointerdown', onPointerDown, { capture: true });
    renderer.domElement.addEventListener('pointerup', onPointerUp);

    onReady?.({
      worldToClient(x, y, z) {
        const p = project(new THREE.Vector3(x, y, z));
        return { x: p.x, y: p.y };
      },
      faceClientPoint(path, faceIndex) {
        const inst = state.instances.get(pathKey(path));
        const mesh = inst ? meshesRef.current[inst.meshKey] : undefined;
        if (!inst || !mesh) return null;
        let span = -1;
        for (let s = 0; s < mesh.faceSpans.length; s += 3) if (mesh.faceSpans[s] === faceIndex) span = s;
        if (span < 0) return null;
        inst.group.updateMatrixWorld();
        const first = mesh.faceSpans[span + 1];
        const count = mesh.faceSpans[span + 2] / 3;
        // Larger triangles first: a click near their centroid is robust.
        const order = [...Array(count).keys()];
        const area = (t: number) => {
          const v = (k: number) => new THREE.Vector3().fromArray(mesh.positions, mesh.indices[first + t * 3 + k] * 3);
          return new THREE.Vector3().crossVectors(v(1).sub(v(0)), v(2).sub(v(0))).length();
        };
        const areas = order.map(area);
        order.sort((x, y) => areas[y] - areas[x]);
        for (const t of order.slice(0, 400)) {
          const c = new THREE.Vector3();
          for (let k = 0; k < 3; ++k) {
            c.add(new THREE.Vector3().fromArray(mesh.positions, mesh.indices[first + t * 3 + k] * 3));
          }
          c.divideScalar(3).applyMatrix4(inst.group.matrixWorld);
          const p = project(c);
          const rect = renderer.domElement.getBoundingClientRect();
          const inside = p.x > rect.left + 2 && p.x < rect.right - 2 && p.y > rect.top + 2 && p.y < rect.bottom - 2;
          if (!p.visible || !inside) continue;
          const fh = faceHit(rayAt(p.x, p.y));
          if (fh && fh.inst === inst && fh.face === faceIndex) return { x: p.x, y: p.y };
        }
        return null;
      },
      viewDirection() {
        const d = new THREE.Vector3();
        camera.getWorldDirection(d);
        return [d.x, d.y, d.z];
      },
    });

    return () => {
      onReady?.(null);
      observer.disconnect();
      finishDrag();
      renderer.domElement.removeEventListener('pointerdown', onPointerDown, { capture: true });
      renderer.domElement.removeEventListener('pointerup', onPointerUp);
      controls.removeEventListener('change', render);
      controls.dispose();
      disposeObject(root);
      for (const g of state.geometries.values()) g.dispose();
      renderer.dispose();
      container.removeChild(renderer.domElement);
      stateRef.current = null;
    };
    // onReady is read once per mount; the scene lives as long as the component.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    const state = stateRef.current;
    if (state) state.framed = false;
  }, [frameKey]);

  // Instances: create/remove by occurrence path; update world matrices in place.
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    // Shared geometry and edge buffers per mesh key.
    for (const [key, mesh] of Object.entries(meshes)) {
      if (!state.geometries.has(key)) {
        state.geometries.set(key, sharedGeometry(mesh));
        state.edgeBuffers.set(key, packEdgePolylines(mesh.edges));
      }
    }
    const wanted = new Set<string>();
    for (const part of parts) {
      if (!part.mesh || !meshes[part.mesh]) continue;
      const key = pathKey(part.path);
      wanted.add(key);
      let inst = state.instances.get(key);
      if (inst && inst.meshKey !== part.mesh) {
        state.root.remove(inst.group);
        disposeObject(inst.group);
        state.instances.delete(key);
        inst = undefined;
      }
      if (!inst) {
        const mesh = meshes[part.mesh];
        const group = new THREE.Group();
        group.matrixAutoUpdate = false;
        const offset = { polygonOffset: true, polygonOffsetFactor: 1, polygonOffsetUnits: 1 };
        const baseMaterial = new THREE.MeshPhongMaterial({ color: BASE_COLOR, shininess: 40, ...offset });
        const faces = new THREE.Mesh(instanceGeometry(state.geometries.get(part.mesh)!, mesh), [
          baseMaterial,
          new THREE.MeshPhongMaterial({ color: SELECTED_COLOR, shininess: 40, ...offset }),
        ]);
        faces.userData.instance = key;
        group.add(faces);
        group.add(edgeLines(state.edgeBuffers.get(part.mesh)!, EDGE_COLOR));
        const highlight = new THREE.Group();
        group.add(highlight);
        state.root.add(group);
        inst = { key, path: part.path, meshKey: part.mesh, group, faces, baseMaterial, highlight };
        state.instances.set(key, inst);
      }
      setMatrix(inst.group, part.world.matrix);
      const c = part.color;
      inst.baseMaterial.userData.partColor = c ? new THREE.Color(c[0], c[1], c[2]).getHex() : BASE_COLOR;
    }
    for (const [key, inst] of [...state.instances]) {
      if (wanted.has(key)) continue;
      state.root.remove(inst.group);
      disposeObject(inst.group);
      state.instances.delete(key);
    }
    for (const key of [...state.geometries.keys()]) {
      if (!meshes[key]) {
        state.geometries.get(key)!.dispose();
        state.geometries.delete(key);
        state.edgeBuffers.delete(key);
      }
    }
    if (!state.framed && state.instances.size > 0) state.framed = frame(state);
    state.render();
  }, [meshes, parts]);

  // Highlights: part colour, selected faces (material 1), selected edges (overlay lines).
  const selectionKey = selected.map((r) => `${pathKey(r.path)}:${r.kind}:${r.index}`).join(',');
  const highlightKey = Object.entries(highlights)
    .map(([k, v]) => `${k}=${v}`)
    .join(',');
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    for (const inst of state.instances.values()) {
      const mesh = meshes[inst.meshKey];
      const h = highlights[inst.key];
      inst.baseMaterial.color.setHex(h ? HIGHLIGHT_COLORS[h] : (inst.baseMaterial.userData.partColor as number) ?? BASE_COLOR);
      const faceSet = new Set<number>();
      const edgeSet = new Set<number>();
      for (const r of selected) {
        if (pathKey(r.path) !== inst.key || r.index === undefined) continue;
        if (r.kind === 'edge') edgeSet.add(r.index);
        else faceSet.add(r.index);
      }
      const groups = inst.faces.geometry.groups;
      for (let g = 0; g < groups.length; ++g) groups[g].materialIndex = faceSet.has(mesh.faceSpans[g * 3]) ? 1 : 0;
      disposeObject(inst.highlight);
      inst.highlight.clear();
      if (edgeSet.size > 0) {
        const chosen = mesh.edges.filter((e) => edgeSet.has(e.edgeIndex));
        inst.highlight.add(edgeLines(packEdgePolylines(chosen), SELECTED_COLOR));
      }
    }
    state.render();
    // selectionKey and highlightKey stand for the arrays.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [meshes, parts, selectionKey, highlightKey]);

  return <div ref={containerRef} className="kernel-viewport" data-testid="assembly-viewport" />;
};

export default AssemblyViewport;
