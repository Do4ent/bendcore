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
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { applyLegacyOutput, lightIntensity } from '../legacyLook';
import { faceOfTriangle } from '../../wasm/bendcore_assembly_api.mjs';
import type { GeometryRef, KernelMesh, PlacedPart } from '../../wasm/bendcore_assembly_api.mjs';
import { packEdgePolylines } from '../kernel/edgeBuffers';
import type { EdgeBuffers } from '../kernel/edgeBuffers';
import { closestPointOnSegmentToRay, edgeCandidates } from '../kernel/edgePick';
import { rayPlane } from './assemblyMath';
import type { MeasureOverlay } from '../kernel/KernelViewport';
import { shouldReframeInstances } from '../kernel/autoFrame';
import { buildReferenceMesh, referenceMeshHit } from '../kernel/referenceMeshObject';
import type { MeshPick, ReferenceMeshDisplay } from '../kernel/referenceMeshObject';
import type { V3 } from './assemblyMath';
import {
  boxFaces,
  boxEdges,
  boxKind,
  defaultFilter,
  edgeAllowed,
  faceAllowed,
  normalizeRect,
  selectOp,
} from '../kernel/selection';
import type { SelectOp, SelectionFilter } from '../kernel/selection';
import type { ClipPlane } from '../kernel/KernelViewport';

/** 'component' picks a whole occurrence (the body mode of the selection panel). */
export type AssemblyPickMode = 'face' | 'edge' | 'component' | 'drag' | 'none';

/** Elements and components a box took (docs/ui/ASSEMBLY_UI.md, "Выбор рамкой"). */
export interface AssemblyBoxResult {
  faces: { path: number[]; index: number }[];
  edges: { path: number[]; index: number }[];
  components: number[][];
}

export interface AssemblyPick {
  /** Shift adds to the selection, Ctrl removes from it. */
  op?: SelectOp;
  path: number[];
  kind: 'face' | 'edge';
  index: number;
  /** World point of the hit, mm. */
  point: V3;
}

export type PartHighlight = 'interference' | 'touching' | 'selected';

export type DragStart = false | { planeNormal: V3 | null };

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
   * the camera. The answer may come asynchronously (kernel call in the worker): the view stops
   * orbiting at once, and a release before the answer ends the drag as soon as it arrives.
   */
  onDragStart?: (path: number[], face: number, point: V3) => DragStart | Promise<DragStart>;
  onDragMove?: (target: V3) => void;
  onDragEnd?: () => void;
  onReady?: (handle: AssemblyViewportHandle | null) => void;
  /** Changing it re-frames the view on the next non-empty scene. */
  frameKey?: number;
  /** A measurement drawn over the model (world coordinates) with its label. */
  measure?: MeasureOverlay | null;
  /** Reference meshes (visible ones), world coordinates; not parts. */
  referenceMeshes?: readonly ReferenceMeshDisplay[];
  /** When set, a click that hits a reference mesh before the parts picks a point on it. */
  onPickMesh?: ((pick: MeshPick) => void) | null;
  /** Selection filter by geometry type (selection.ts); the element kind is `pickMode`. */
  filter?: SelectionFilter;
  /** Component mode: the picked occurrence path, or null for a click on nothing. */
  onPickComponent?: (path: number[] | null, op: SelectOp) => void;
  /** Box selection (left to right: entirely inside, right to left: touched). */
  boxSelect?: boolean;
  onBoxSelect?: (result: AssemblyBoxResult, op: SelectOp, kind: 'inside' | 'cross') => void;
  /** Section: parts are clipped by this plane (the side n.p + c >= 0 is kept). */
  clip?: ClipPlane | null;
  /** Section contours (world xyz triples) drawn over the cut. */
  sectionLines?: readonly Float32Array[];
}

const SECTION_COLOR = 0xd62828;
const HOVER_COLOR = 0xffd166;
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
  /** hovered edge line (instance frame) */
  hover: THREE.Group;
  selectedFaces: Set<number>;
  hoverFaces: Set<number>;
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
  /** measurement lines (not pickable) and label */
  tools: THREE.Group;
  /** reference meshes (pickable for measurements only) */
  refMeshes: THREE.Group;
  /** section contours */
  section: THREE.Group;
  clip: THREE.Plane | null;
  label: { anchor: THREE.Vector3; element: HTMLDivElement } | null;
  framed: boolean;
  /** times the view was framed automatically (data-frames, for end-to-end tests) */
  frames: number;
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

/** Draw groups of an occurrence: selected faces use material 1, hovered ones material 2. */
function applyInstanceMaterials(inst: Instance, mesh: KernelMesh): void {
  const groups = inst.faces.geometry.groups;
  for (let g = 0; g < groups.length; ++g) {
    const face = mesh.faceSpans[g * 3];
    groups[g].materialIndex = inst.selectedFaces.has(face) ? 1 : inst.hoverFaces.has(face) ? 2 : 0;
  }
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
  for (const m of state.refMeshes.children) box.expandByObject(m);
  if (box.isEmpty()) return false;
  state.frames += 1;
  state.renderer.domElement.parentElement?.setAttribute('data-frames', String(state.frames));
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
  measure = null,
  referenceMeshes,
  onPickMesh = null,
  filter = defaultFilter(),
  onPickComponent,
  boxSelect = false,
  onBoxSelect,
  clip = null,
  sectionLines,
}) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const stateRef = useRef<SceneState | null>(null);
  const meshesRef = useRef(meshes);
  meshesRef.current = meshes;
  const activeFilter: SelectionFilter = { ...filter, mode: pickMode === 'component' ? 'body' : pickMode === 'edge' ? 'edge' : 'face' };
  const cb = useRef({ onPick, onDragStart, onDragMove, onDragEnd, pickMode, onPickMesh, onPickComponent, onBoxSelect, boxSelect, filter: activeFilter });
  cb.current = { onPick, onDragStart, onDragMove, onDragEnd, pickMode, onPickMesh, onPickComponent, onBoxSelect, boxSelect, filter: activeFilter };

  useEffect(() => {
    const container = containerRef.current;
    if (!container) return undefined;
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setPixelRatio(window.devicePixelRatio);
    applyLegacyOutput(renderer);
    renderer.setClearColor(BACKGROUND, 1);
    container.appendChild(renderer.domElement);
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(BACKGROUND);
    const camera = new THREE.PerspectiveCamera(35, 1, 0.1, 1000);
    camera.up.set(0, 0, 1);
    scene.add(camera);
    scene.add(new THREE.AmbientLight(0xffffff, lightIntensity(0.45)));
    const headlight = new THREE.DirectionalLight(0xffffff, lightIntensity(0.7));
    headlight.position.set(0.3, 0.6, 1);
    camera.add(headlight);
    const root = new THREE.Group();
    scene.add(root);
    const controls = new OrbitControls(camera, renderer.domElement);
    controls.screenSpacePanning = true;
    const toolsGroup = new THREE.Group();
    scene.add(toolsGroup);
    const refMeshesGroup = new THREE.Group();
    scene.add(refMeshesGroup);
    const sectionGroup = new THREE.Group();
    scene.add(sectionGroup);
    const render = () => {
      renderer.render(scene, camera);
      const label = stateRef.current?.label;
      if (label) {
        const p = label.anchor.clone().project(camera);
        label.element.style.display = p.z > -1 && p.z < 1 ? 'block' : 'none';
        label.element.style.left = `${((p.x + 1) / 2) * renderer.domElement.clientWidth}px`;
        label.element.style.top = `${((1 - p.y) / 2) * renderer.domElement.clientHeight}px`;
      }
    };
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
      tools: toolsGroup,
      refMeshes: refMeshesGroup,
      section: sectionGroup,
      clip: null,
      label: null,
      framed: false,
      frames: 0,
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
      const hit = raycaster.intersectObjects(objects, false).find((h) => !state.clip || state.clip.distanceToPoint(h.point) >= 0);
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
        visible: p.z > -1 && p.z < 1 && !(state.clip && state.clip.distanceToPoint(world) < -1e-6),
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
        const edgeTypes = meshesRef.current[inst.meshKey]?.edgeTypes;
        for (const c of edgeCandidates(buffers, (x, y, z) => project(toWorld(x, y, z)), clientX, clientY, EDGE_TOLERANCE)) {
          if (best && best.distance <= c.distance) break;
          if (!edgeAllowed(cb.current.filter, edgeTypes?.[c.edgeIndex])) continue;
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
      const answer = cb.current.onDragStart?.(fh.inst.path, fh.face, [p.x, p.y, p.z]);
      if (!answer) return;
      controls.enabled = false;
      const begin = (accepted: { planeNormal: V3 | null }) => {
        const normal = new THREE.Vector3();
        camera.getWorldDirection(normal);
        // A face plane seen nearly edge-on would throw the target far away: camera plane then.
        const n = accepted.planeNormal;
        if (n && Math.abs(normal.x * n[0] + normal.y * n[1] + normal.z * n[2]) > 0.25) normal.set(n[0], n[1], n[2]);
        drag = { point: p.clone(), normal, pending: null, frame: 0 };
        window.addEventListener('pointermove', onDragPointerMove);
        window.addEventListener('pointerup', onDragPointerUp);
      };
      if (!(answer instanceof Promise)) {
        begin(answer);
        return;
      }
      let released = false;
      const onEarlyUp = () => {
        released = true;
      };
      window.addEventListener('pointerup', onEarlyUp);
      void answer.then(
        (accepted) => {
          window.removeEventListener('pointerup', onEarlyUp);
          if (!accepted) {
            controls.enabled = true;
            return;
          }
          begin(accepted);
          if (released) finishDrag();
        },
        () => {
          window.removeEventListener('pointerup', onEarlyUp);
          controls.enabled = true;
        },
      );
    };
    // ---- hover: only elements the filter allows are highlighted
    let hoverKey = '';
    let hoverFrame = 0;
    let hovered: Instance[] = [];
    const clearHover = () => {
      for (const inst of hovered) {
        inst.hoverFaces = new Set();
        disposeObject(inst.hover);
        inst.hover.clear();
        inst.baseMaterial.emissive.setHex(0x000000);
        const mesh = meshesRef.current[inst.meshKey];
        if (mesh) applyInstanceMaterials(inst, mesh);
      }
      hovered = [];
    };
    const hoverAt = (clientX: number, clientY: number) => {
      const mode = cb.current.pickMode;
      let key = '';
      let next: (() => void) | null = null;
      if (mode === 'face' || mode === 'component') {
        const fh = faceHit(rayAt(clientX, clientY));
        if (fh) {
          const mesh = meshesRef.current[fh.inst.meshKey];
          if (mode === 'component') {
            key = `component:${fh.inst.key}`;
            next = () => {
              fh.inst.baseMaterial.emissive.setHex(0x4a3a00);
              hovered.push(fh.inst);
            };
          } else if (faceAllowed(cb.current.filter, mesh?.faceTypes?.[fh.face])) {
            key = `face:${fh.inst.key}:${fh.face}`;
            next = () => {
              fh.inst.hoverFaces = new Set([fh.face]);
              if (mesh) applyInstanceMaterials(fh.inst, mesh);
              hovered.push(fh.inst);
            };
          }
        }
      } else if (mode === 'edge') {
        const pe = pickEdge(clientX, clientY);
        const inst = pe ? state.instances.get(pathKey(pe.path)) : undefined;
        const mesh = inst ? meshesRef.current[inst.meshKey] : undefined;
        if (pe && inst && mesh) {
          key = `edge:${inst.key}:${pe.index}`;
          next = () => {
            const chosen = mesh.edges.filter((e) => e.edgeIndex === pe.index);
            const line = edgeLines(packEdgePolylines(chosen), HOVER_COLOR);
            line.renderOrder = 9;
            (line.material as unknown as { depthTest: boolean }).depthTest = false;
            inst.hover.add(line);
            hovered.push(inst);
          };
        }
      }
      if (key === hoverKey) return;
      clearHover();
      hoverKey = key;
      next?.();
      renderer.domElement.parentElement?.setAttribute('data-hover', key);
      render();
    };

    // ---- box selection
    let boxElement: HTMLDivElement | null = null;
    const drawBox = (x0: number, y0: number, x1: number, y1: number) => {
      const host = renderer.domElement.parentElement;
      if (!host) return;
      if (!boxElement) {
        boxElement = document.createElement('div');
        boxElement.className = 'kernel-box';
        boxElement.setAttribute('data-testid', 'box-rect');
        host.appendChild(boxElement);
      }
      const rect = renderer.domElement.getBoundingClientRect();
      const box = normalizeRect({ x: x0, y: y0 }, { x: x1, y: y1 });
      boxElement.style.left = `${box.x0 - rect.left}px`;
      boxElement.style.top = `${box.y0 - rect.top}px`;
      boxElement.style.width = `${box.x1 - box.x0}px`;
      boxElement.style.height = `${box.y1 - box.y0}px`;
      boxElement.setAttribute('data-direction', x1 >= x0 ? 'inside' : 'cross');
    };
    const hideBox = () => {
      boxElement?.remove();
      boxElement = null;
    };
    const finishBox = (start: { x: number; y: number }, e: PointerEvent): void => {
      hideBox();
      const mode = cb.current.pickMode;
      const kind = boxKind(start, { x: e.clientX });
      const rect = normalizeRect(start, { x: e.clientX, y: e.clientY });
      const result: AssemblyBoxResult = { faces: [], edges: [], components: [] };
      for (const inst of state.instances.values()) {
        const mesh = meshesRef.current[inst.meshKey];
        if (!mesh) continue;
        inst.group.updateMatrixWorld();
        const m = inst.group.matrixWorld;
        const proj = (x: number, y: number, z: number) => project(new THREE.Vector3(x, y, z).applyMatrix4(m));
        if (mode === 'face') {
          for (const index of boxFaces(mesh, proj, rect, kind, cb.current.filter)) result.faces.push({ path: inst.path, index });
        } else if (mode === 'edge') {
          for (const index of boxEdges(mesh, proj, rect, kind, cb.current.filter)) result.edges.push({ path: inst.path, index });
        } else if (mode === 'component') {
          const total = mesh.faceSpans.length / 3;
          const found = boxFaces(mesh, proj, rect, kind).length;
          if (total > 0 && (kind === 'inside' ? found === total : found > 0)) result.components.push(inst.path);
        }
      }
      cb.current.onBoxSelect?.(result, selectOp(e), kind);
    };
    const onPointerMove = (e: PointerEvent) => {
      if (down) {
        if (cb.current.boxSelect && (e.buttons & 1) !== 0 && Math.hypot(e.clientX - down.x, e.clientY - down.y) > CLICK_TOLERANCE) {
          drawBox(down.x, down.y, e.clientX, e.clientY);
        }
        return;
      }
      if (drag || cb.current.pickMode === 'drag' || cb.current.pickMode === 'none') return;
      if (hoverFrame) cancelAnimationFrame(hoverFrame);
      const { clientX, clientY } = e;
      hoverFrame = requestAnimationFrame(() => {
        hoverFrame = 0;
        hoverAt(clientX, clientY);
      });
    };
    const onPointerLeave = () => {
      if (!hoverKey) return;
      clearHover();
      hoverKey = '';
      renderer.domElement.parentElement?.setAttribute('data-hover', '');
      render();
    };
    const onPointerUp = (e: PointerEvent) => {
      if (!down || e.button !== 0) return;
      const start = down;
      const moved = Math.hypot(e.clientX - down.x, e.clientY - down.y);
      down = null;
      const mode = cb.current.pickMode;
      if (mode === 'none' || mode === 'drag') return;
      if (moved > CLICK_TOLERANCE) {
        if (cb.current.boxSelect) finishBox(start, e);
        return;
      }
      const op = selectOp(e);
      const meshPick = cb.current.onPickMesh;
      if (meshPick) {
        const raycaster = rayAt(e.clientX, e.clientY);
        const mh = referenceMeshHit(raycaster, state.refMeshes);
        const fh = faceHit(raycaster);
        if (mh && (!fh || mh.distance < fh.hit.distance)) {
          meshPick(mh.pick);
          return;
        }
      }
      if (mode === 'edge') {
        const picked = pickEdge(e.clientX, e.clientY);
        cb.current.onPick(picked ? { ...picked, op } : null);
        return;
      }
      const fh = faceHit(rayAt(e.clientX, e.clientY));
      if (mode === 'component') {
        cb.current.onPickComponent?.(fh ? fh.inst.path : null, op);
        return;
      }
      if (!fh) {
        cb.current.onPick(null);
        return;
      }
      // A face the type filter refuses does not select (and does not clear the selection).
      if (!faceAllowed(cb.current.filter, meshesRef.current[fh.inst.meshKey]?.faceTypes?.[fh.face])) return;
      const p = fh.hit.point;
      cb.current.onPick({ path: fh.inst.path, kind: 'face', index: fh.face, point: [p.x, p.y, p.z], op });
    };
    renderer.domElement.addEventListener('pointerdown', onPointerDown, { capture: true });
    renderer.domElement.addEventListener('pointerup', onPointerUp);
    renderer.domElement.addEventListener('pointermove', onPointerMove);
    renderer.domElement.addEventListener('pointerleave', onPointerLeave);

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
      renderer.domElement.removeEventListener('pointermove', onPointerMove);
      renderer.domElement.removeEventListener('pointerleave', onPointerLeave);
      if (hoverFrame) cancelAnimationFrame(hoverFrame);
      hideBox();
      controls.removeEventListener('change', render);
      controls.dispose();
      disposeObject(root);
      disposeObject(toolsGroup);
      disposeObject(refMeshesGroup);
      disposeObject(sectionGroup);
      state.label?.element.remove();
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

  // Reference meshes: rebuilt when the list changes; framed with the parts.
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    disposeObject(state.refMeshes);
    state.refMeshes.clear();
    for (const m of referenceMeshes ?? []) if (m.indices.length > 0) state.refMeshes.add(buildReferenceMesh(m));
    state.renderer.domElement.parentElement?.setAttribute('data-reference-meshes', String(state.refMeshes.children.length));
    if (!state.framed && state.refMeshes.children.length > 0) state.framed = frame(state);
    state.render();
  }, [referenceMeshes]);

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
    const added: Instance[] = [];
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
          new THREE.MeshPhongMaterial({ color: HOVER_COLOR, shininess: 40, ...offset }),
        ]);
        faces.userData.instance = key;
        group.add(faces);
        group.add(edgeLines(state.edgeBuffers.get(part.mesh)!, EDGE_COLOR));
        const highlight = new THREE.Group();
        group.add(highlight);
        const hover = new THREE.Group();
        group.add(hover);
        state.root.add(group);
        inst = {
          key,
          path: part.path,
          meshKey: part.mesh,
          group,
          faces,
          baseMaterial,
          highlight,
          hover,
          selectedFaces: new Set(),
          hoverFaces: new Set(),
        };
        state.instances.set(key, inst);
        added.push(inst);
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
    else if (state.framed && added.length > 0) {
      // An operation added instances (a pattern, fasteners) the camera does not show: show all.
      const boxes = added.map((inst) => {
        inst.group.updateMatrixWorld(true);
        return new THREE.Box3().setFromObject(inst.faces);
      });
      if (shouldReframeInstances(boxes, state.camera)) frame(state);
    }
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
      inst.selectedFaces = faceSet;
      applyInstanceMaterials(inst, mesh);
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

  // Box selection: the left button drags a rectangle, so it no longer orbits.
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    state.controls.mouseButtons.LEFT = boxSelect ? -1 : 0;
    state.renderer.domElement.parentElement?.setAttribute('data-box-select', boxSelect ? '1' : '0');
  }, [boxSelect]);

  // Section plane: a renderer clipping plane (the slider needs no kernel call).
  const clipKey = clip ? `${clip.normal.join(',')}|${clip.constant}` : '';
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    if (clip) {
      state.clip = new THREE.Plane(new THREE.Vector3(...clip.normal), clip.constant);
      state.renderer.localClippingEnabled = false;
      state.renderer.clippingPlanes = [state.clip];
    } else {
      state.clip = null;
      state.renderer.clippingPlanes = [];
    }
    state.renderer.domElement.parentElement?.setAttribute('data-clipped', clip ? 'true' : 'false');
    state.render();
    // clipKey stands for the plane.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [clipKey]);

  // Section contours of the kernel.
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    disposeObject(state.section);
    state.section.clear();
    for (const line of sectionLines ?? []) {
      if (line.length < 6) continue;
      const geometry = new THREE.BufferGeometry();
      geometry.setAttribute('position', new THREE.BufferAttribute(Float32Array.from(line), 3));
      const obj = new THREE.Line(geometry, new THREE.LineBasicMaterial({ color: SECTION_COLOR, depthTest: false }));
      obj.renderOrder = 10;
      obj.name = 'assembly-section';
      state.section.add(obj);
    }
    state.renderer.domElement.parentElement?.setAttribute('data-section-lines', String(sectionLines?.length ?? 0));
    state.render();
  }, [sectionLines]);

  // Measurement: lines and a label over the projection of its anchor.
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    disposeObject(state.tools);
    state.tools.clear();
    if (measure) {
      for (const line of measure.lines) {
        const geometry = new THREE.BufferGeometry();
        geometry.setAttribute('position', new THREE.BufferAttribute(Float32Array.from(line.flat()), 3));
        const obj = new THREE.Line(geometry, new THREE.LineBasicMaterial({ color: 0x0b8a3e, depthTest: false }));
        obj.renderOrder = 10;
        obj.name = 'assembly-measure';
        state.tools.add(obj);
      }
    }
    if (state.label && !measure) {
      state.label.element.remove();
      state.label = null;
    }
    if (measure) {
      if (!state.label) {
        const element = document.createElement('div');
        element.className = 'kernel-measure-label';
        element.setAttribute('data-testid', 'measure-label');
        state.renderer.domElement.parentElement?.appendChild(element);
        state.label = { anchor: new THREE.Vector3(), element };
      }
      state.label.anchor.set(...measure.anchor);
      state.label.element.textContent = measure.label;
      state.label.element.setAttribute('data-label', measure.label);
    }
    state.render();
  }, [measure]);

  return <div ref={containerRef} className="kernel-viewport" data-testid="assembly-viewport" />;
};

export default AssemblyViewport;
