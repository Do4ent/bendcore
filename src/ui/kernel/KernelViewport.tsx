/**
 * KernelViewport: three.js view of a tessellated kernel shape (docs/ui/KERNEL_VIEWPORT.md).
 *
 * The geometry is the kernel's mesh as is: positions, normals and indices from
 * KernelShape.tessellate() become one indexed BufferGeometry with one draw group per B-rep face
 * (faceSpans), edge polylines become one LineSegments object. A click casts a ray; the hit
 * triangle maps to its B-rep face by binary search over faceSpans (faceOfTriangle), and the
 * selected face is drawn with the highlight material by switching its group's material index.
 * In edge mode a click selects the kernel edge nearest to the cursor on screen (edgePick.ts),
 * unless a face hides it; selected edges are drawn as a separate highlighted LineSegments.
 * An optional overlay mesh (sketch regions) is drawn translucent on top of the model.
 * In vertex mode a click selects the vertex (`vertices`) nearest to the cursor on screen, unless a
 * face hides it; the vertices are drawn as dots while the mode is on. Datum glyphs (`datums`:
 * point markers, the three axes of a coordinate system, plane outlines, axis lines) are drawn
 * over the model and are not pickable.
 *
 * Tools (docs/ui/KERNEL_VIEWPORT.md, "Измерения и сечение"): `clip` is a clipping plane of the
 * renderer (the section slider moves it without a kernel call; the faces and edges of the removed
 * side are neither drawn nor picked), `sectionLines` the section contour of the kernel and
 * `measure` the lines and the label of a kernel measurement.
 */

import React, { useEffect, useRef } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { applyLegacyOutput, lightIntensity } from '../legacyLook';
import { faceOfTriangle } from '../../wasm/bendcore_kernel_api.mjs';
import type { KernelMesh } from '../../wasm/bendcore_kernel_api.mjs';
import { packEdgePolylines } from './edgeBuffers';
import type { EdgeBuffers } from './edgeBuffers';
import { closestPointOnSegmentToRay, edgeCandidates } from './edgePick';
import { shouldReframePart } from './autoFrame';
import { buildReferenceMesh, referenceMeshHit } from './referenceMeshObject';
import {
  NO_BODY,
  boxKind,
  boxSelect as boxSelectMesh,
  defaultFilter,
  edgeAllowed,
  facesOfBodies,
  faceAllowed,
  normalizeRect,
  selectOp,
} from './selection';
import type { BoxResult, SelectOp, SelectionFilter } from './selection';
import type { MeshPick, ReferenceMeshDisplay } from './referenceMeshObject';

export type { MeshPick, ReferenceMeshDisplay, BoxResult, SelectOp, SelectionFilter };

export interface FacePick {
  /** Shift adds to the selection, Ctrl removes from it (selection.ts, selectOp). */
  op?: SelectOp;
  faceIndex: number;
  /** Hit point in model coordinates, mm. */
  point: [number, number, number];
}

export interface EdgePick {
  op?: SelectOp;
  edgeIndex: number;
  /** Closest point of the edge polyline to the cursor, mm. */
  point: [number, number, number];
}

export interface VertexPick {
  op?: SelectOp;
  vertexIndex: number;
  /** Position of the vertex, mm. */
  point: [number, number, number];
}

/** A body (solid) of the shown shape, picked through one of its faces. */
export interface BodyPick {
  op?: SelectOp;
  bodyIndex: number;
  faceIndex: number;
  point: [number, number, number];
}

export type PickMode = 'face' | 'edge' | 'vertex' | 'body' | 'none';

type Vec3 = [number, number, number];

/** Datum geometry drawn over the model (docs/ui/PART_MODELING_UI.md, "Вспомогательная геометрия"). */
export type DatumGlyph =
  | { kind: 'point'; id: string; point: Vec3 }
  | { kind: 'csys'; id: string; origin: Vec3; x: Vec3; y: Vec3; z: Vec3 }
  | { kind: 'plane'; id: string; origin: Vec3; normal: Vec3 }
  | { kind: 'axis'; id: string; origin: Vec3; direction: Vec3 };

export interface KernelViewportHandle {
  /** Client (CSS pixel) coordinates of a model point in the current view. */
  worldToClient(x: number, y: number, z: number): { x: number; y: number };
}

/** Clipping plane in three.js form: the side with normal . p + constant >= 0 is kept. */
export interface ClipPlane {
  normal: [number, number, number];
  constant: number;
}

export interface MeasureOverlay {
  /** Polylines to draw, model coordinates. */
  lines: [number, number, number][][];
  /** Where the label is anchored. */
  anchor: [number, number, number];
  label: string;
}

export interface KernelViewportProps {
  mesh: KernelMesh | null;
  selectedFace: number | null;
  /** Called on a click without drag: the picked face, or null when the click hit nothing. */
  onPick: (pick: FacePick | null) => void;
  onReady?: (handle: KernelViewportHandle | null) => void;
  /** What a click selects (default 'face'). */
  pickMode?: PickMode;
  /** Edge mode: the picked edge, or null when the click is not near a visible edge. */
  onPickEdge?: (pick: EdgePick | null) => void;
  /** Further highlighted faces and edges (kernel indices of `mesh`). */
  selectedFaces?: readonly number[];
  selectedEdges?: readonly number[];
  /** Vertex positions (xyz triples, index = kernel vertex index) for the vertex mode. */
  vertices?: Float32Array | null;
  /** Vertex mode: the picked vertex, or null when the click is not near a visible vertex. */
  onPickVertex?: (pick: VertexPick | null) => void;
  selectedVertices?: readonly number[];
  /** Datum points, coordinate systems, planes and axes drawn over the model. */
  datums?: readonly DatumGlyph[];
  /**
   * Selection filter by geometry type (selection.ts): hover highlight, clicks and the box take only
   * faces/edges that pass it. Its `mode` is ignored: `pickMode` decides the element kind.
   */
  filter?: SelectionFilter;
  /** Surface type of every face and curve type of every edge (KernelShape.faceTypes(), PartMesh.faceTypes). */
  faceTypes?: readonly string[] | null;
  edgeTypes?: readonly string[] | null;
  /** Body of every face (PartMesh.faceBodies) for the body mode. */
  faceBodies?: ArrayLike<number> | null;
  onPickBody?: (pick: BodyPick | null) => void;
  selectedBodies?: readonly number[];
  /**
   * Box selection: the left button drags a rectangle instead of orbiting (left to right takes the
   * elements entirely inside, right to left the touched ones; Shift adds, Ctrl removes).
   */
  boxSelect?: boolean;
  onBoxSelect?: (result: BoxResult, op: SelectOp, kind: 'inside' | 'cross') => void;
  /** Translucent mesh drawn over the model (e.g. sketch regions); not pickable. */
  overlay?: KernelMesh | null;
  /**
   * 'always' (default): frame the view on every new mesh. 'first': only on the first non-empty
   * mesh after mount or after frameKey changes, so edits keep the camera.
   */
  autoFrame?: 'always' | 'first';
  frameKey?: number;
  /**
   * Number of bodies of `mesh` (autoFrame 'first'): when it grows and the model grows beyond the
   * view, the view is framed again (src/ui/kernel/autoFrame.ts).
   */
  bodyCount?: number | null;
  /** Section: the model is clipped by this plane (null: whole). */
  clip?: ClipPlane | null;
  /** Section contour polylines (xyz triples) drawn over the cut. */
  sectionLines?: readonly Float32Array[];
  /** A measurement drawn over the model. */
  measure?: MeasureOverlay | null;
  /** Reference meshes (visible ones only), drawn translucent; not part of the model. */
  referenceMeshes?: readonly ReferenceMeshDisplay[];
  /** When set, a click that hits a reference mesh before the model picks a point on it. */
  onPickMesh?: ((pick: MeshPick) => void) | null;
}

const FACE_COLOR = 0xb8c4d6;
const SELECTED_COLOR = 0xff8c1a;
const HOVER_COLOR = 0xffd166;
const EDGE_COLOR = 0x1d2733;
const OVERLAY_COLOR = 0x2f7de1;
const SECTION_COLOR = 0xd62828;
const MEASURE_COLOR = 0x0b8a3e;
const VERTEX_COLOR = 0x1d2733;
const DATUM_COLOR = 0x7a3fd1;
const AXIS_COLORS = [0xd62828, 0x2a9d38, 0x1f5fd6];
/** Pointer distance (px) within which a click picks an edge. */
const EDGE_TOLERANCE = 6;
/** Pointer distance (px) within which a click picks a vertex. */
const VERTEX_TOLERANCE = 10;
const BACKGROUND = 0xf3f5f8;
/** Pointer travel (px) up to which a press-release counts as a click, not an orbit drag. */
const CLICK_TOLERANCE = 4;

interface SceneState {
  renderer: THREE.WebGLRenderer;
  scene: THREE.Scene;
  camera: THREE.PerspectiveCamera;
  controls: OrbitControls;
  model: THREE.Group;
  faces: THREE.Mesh | null;
  edgeBuffers: EdgeBuffers | null;
  highlight: THREE.Group;
  /** hover highlight of edges and vertices (faces use material 2) */
  hover: THREE.Group;
  selectedFaceSet: Set<number>;
  hoverFaceSet: Set<number>;
  overlay: THREE.Group;
  /** reference meshes (pickable for measurements only) */
  meshes: THREE.Group;
  /** section contour and measurement lines (not pickable) */
  tools: THREE.Group;
  /** vertex dots of the vertex mode and selected vertices; datum glyphs (not pickable) */
  marks: THREE.Group;
  datums: THREE.Group;
  clip: THREE.Plane | null;
  /** measurement label: anchor in model coordinates, element placed over the viewport */
  label: { anchor: THREE.Vector3; element: HTMLDivElement } | null;
  framed: boolean;
  /** bodies and bounding box of the mesh shown (for the re-framing of new bodies) */
  bodies: number | null;
  box: THREE.Box3;
  /** times the view was framed automatically (data-frames, for end-to-end tests) */
  frames: number;
  render: () => void;
}

function disposeModel(model: THREE.Group): void {
  for (const child of [...model.children]) {
    model.remove(child);
    const withGeometry = child as THREE.Object3D & { geometry?: THREE.BufferGeometry; material?: THREE.Material | THREE.Material[] };
    withGeometry.geometry?.dispose();
    const mats = withGeometry.material;
    if (Array.isArray(mats)) mats.forEach((m) => m.dispose());
    else mats?.dispose();
  }
}

function buildFaces(mesh: KernelMesh): THREE.Mesh {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(mesh.positions, 3));
  geometry.setAttribute('normal', new THREE.BufferAttribute(mesh.normals, 3));
  geometry.setIndex(new THREE.BufferAttribute(mesh.indices, 1));
  for (let s = 0; s < mesh.faceSpans.length; s += 3) {
    geometry.addGroup(mesh.faceSpans[s + 1], mesh.faceSpans[s + 2], 0);
  }
  geometry.computeBoundingSphere();
  const offset = { polygonOffset: true, polygonOffsetFactor: 1, polygonOffsetUnits: 1 };
  const materials = [
    new THREE.MeshPhongMaterial({ color: FACE_COLOR, shininess: 40, ...offset }),
    new THREE.MeshPhongMaterial({ color: SELECTED_COLOR, shininess: 40, ...offset }),
    new THREE.MeshPhongMaterial({ color: HOVER_COLOR, shininess: 40, ...offset }),
  ];
  const faces = new THREE.Mesh(geometry, materials);
  faces.name = 'kernel-faces';
  return faces;
}

/** Draw groups of the faces: selected faces use material 1, the hovered ones material 2. */
function applyFaceMaterials(state: SceneState, mesh: KernelMesh | null): void {
  if (!state.faces || !mesh) return;
  const groups = state.faces.geometry.groups;
  for (let g = 0; g < groups.length; ++g) {
    const face = mesh.faceSpans[g * 3];
    groups[g].materialIndex = state.selectedFaceSet.has(face) ? 1 : state.hoverFaceSet.has(face) ? 2 : 0;
  }
}

function buildEdges(buffers: EdgeBuffers, color = EDGE_COLOR): THREE.LineSegments {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(buffers.positions, 3));
  geometry.setIndex(new THREE.BufferAttribute(buffers.indices, 1));
  const edges = new THREE.LineSegments(geometry, new THREE.LineBasicMaterial({ color }));
  edges.name = 'kernel-edges';
  return edges;
}

/** Isometric view (CAD convention, Z up) framing the bounding sphere. */
function frame(state: SceneState, sphere: THREE.Sphere): void {
  const { camera, controls } = state;
  state.frames += 1;
  state.renderer.domElement.parentElement?.setAttribute('data-frames', String(state.frames));
  const radius = Math.max(sphere.radius, 1e-3);
  const distance = radius / Math.sin((camera.fov * Math.PI) / 360) * 1.15;
  const dir = new THREE.Vector3(1, -1.2, 0.9).normalize();
  controls.target.copy(sphere.center);
  camera.position.copy(sphere.center).addScaledVector(dir, distance);
  camera.near = distance / 100;
  camera.far = distance * 10;
  camera.updateProjectionMatrix();
  controls.update();
}

function buildOverlay(mesh: KernelMesh): THREE.Object3D[] {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(mesh.positions, 3));
  geometry.setAttribute('normal', new THREE.BufferAttribute(mesh.normals, 3));
  geometry.setIndex(new THREE.BufferAttribute(mesh.indices, 1));
  const material = new THREE.MeshBasicMaterial({
    color: OVERLAY_COLOR,
    transparent: true,
    opacity: 0.25,
    side: THREE.DoubleSide,
    depthWrite: false,
  });
  const faces = new THREE.Mesh(geometry, material);
  faces.name = 'kernel-overlay';
  return [faces, buildEdges(packEdgePolylines(mesh.edges), OVERLAY_COLOR)];
}

/** Puts the measurement label over the projection of its anchor. */
function placeLabel(state: SceneState | null, camera: THREE.Camera, canvas: HTMLElement): void {
  const label = state?.label;
  if (!label) return;
  const p = label.anchor.clone().project(camera);
  const visible = p.z > -1 && p.z < 1;
  label.element.style.display = visible ? 'block' : 'none';
  label.element.style.left = `${((p.x + 1) / 2) * canvas.clientWidth}px`;
  label.element.style.top = `${((1 - p.y) / 2) * canvas.clientHeight}px`;
}

function pointsObject(points: ArrayLike<number>, color: number, size: number, name: string): THREE.Points {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(Float32Array.from(points as ArrayLike<number>), 3));
  const material = new THREE.PointsMaterial({ color, size, sizeAttenuation: false, depthTest: false });
  const object = new THREE.Points(geometry, material);
  object.renderOrder = 11;
  object.name = name;
  return object;
}

/**
 * Objects of the datum glyphs; `size` is the length of a coordinate system axis (a fraction of the
 * model size). Planes are square outlines 2·size wide, axes lines 3·size long.
 */
function buildDatums(datums: readonly DatumGlyph[], size: number): THREE.Object3D[] {
  const out: THREE.Object3D[] = [];
  const v = (a: Vec3) => new THREE.Vector3(...a);
  const at = (o: THREE.Vector3, d: THREE.Vector3, k: number) => o.clone().addScaledVector(d, k);
  for (const d of datums) {
    if (d.kind === 'point') {
      out.push(pointsObject(d.point, DATUM_COLOR, 9, `datum-point-${d.id}`));
    } else if (d.kind === 'csys') {
      const o = v(d.origin);
      [d.x, d.y, d.z].forEach((axis, i) => {
        const end = at(o, v(axis), size);
        out.push(lineObject([o.x, o.y, o.z, end.x, end.y, end.z], AXIS_COLORS[i], `datum-csys-${d.id}-${'xyz'[i]}`));
      });
      out.push(pointsObject(d.origin, DATUM_COLOR, 7, `datum-csys-${d.id}-origin`));
    } else if (d.kind === 'plane') {
      const o = v(d.origin);
      const n = v(d.normal).normalize();
      const helper = Math.abs(n.z) < 0.9 ? new THREE.Vector3(0, 0, 1) : new THREE.Vector3(1, 0, 0);
      const u = new THREE.Vector3().crossVectors(n, helper).normalize();
      const w = new THREE.Vector3().crossVectors(n, u);
      const corners = [
        [1, 1],
        [-1, 1],
        [-1, -1],
        [1, -1],
        [1, 1],
      ].flatMap(([a, b]) => {
        const p = o.clone().addScaledVector(u, a * size).addScaledVector(w, b * size);
        return [p.x, p.y, p.z];
      });
      out.push(lineObject(corners, DATUM_COLOR, `datum-plane-${d.id}`));
    } else {
      const o = v(d.origin);
      const dir = v(d.direction).normalize();
      const a = at(o, dir, -1.5 * size);
      const b = at(o, dir, 1.5 * size);
      out.push(lineObject([a.x, a.y, a.z, b.x, b.y, b.z], DATUM_COLOR, `datum-axis-${d.id}`));
    }
  }
  return out;
}

function lineObject(points: ArrayLike<number>, color: number, name: string): THREE.Line {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(Float32Array.from(points as ArrayLike<number>), 3));
  const line = new THREE.Line(geometry, new THREE.LineBasicMaterial({ color, depthTest: false }));
  line.renderOrder = 10;
  line.name = name;
  return line;
}

const KernelViewport: React.FC<KernelViewportProps> = ({
  mesh,
  selectedFace,
  onPick,
  onReady,
  pickMode = 'face',
  onPickEdge,
  selectedFaces,
  selectedEdges,
  overlay,
  autoFrame = 'always',
  frameKey,
  bodyCount = null,
  clip = null,
  sectionLines,
  measure = null,
  referenceMeshes,
  onPickMesh = null,
  vertices = null,
  onPickVertex,
  selectedVertices,
  datums,
  filter = defaultFilter(),
  faceTypes = null,
  edgeTypes = null,
  faceBodies = null,
  onPickBody,
  selectedBodies,
  boxSelect = false,
  onBoxSelect,
}) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const stateRef = useRef<SceneState | null>(null);
  const meshRef = useRef<KernelMesh | null>(mesh);
  const onPickRef = useRef(onPick);
  onPickRef.current = onPick;
  const onPickEdgeRef = useRef(onPickEdge);
  onPickEdgeRef.current = onPickEdge;
  const onPickVertexRef = useRef(onPickVertex);
  onPickVertexRef.current = onPickVertex;
  const verticesRef = useRef(vertices);
  verticesRef.current = vertices;
  const onPickMeshRef = useRef(onPickMesh);
  onPickMeshRef.current = onPickMesh;
  const pickModeRef = useRef(pickMode);
  pickModeRef.current = pickMode;
  const onPickBodyRef = useRef(onPickBody);
  onPickBodyRef.current = onPickBody;
  const onBoxSelectRef = useRef(onBoxSelect);
  onBoxSelectRef.current = onBoxSelect;
  const boxSelectRef = useRef(boxSelect);
  boxSelectRef.current = boxSelect;
  // The filter in force: the element kind is the pick mode.
  const activeFilter: SelectionFilter = { ...filter, mode: pickMode === 'none' ? 'face' : pickMode };
  const filterRef = useRef(activeFilter);
  filterRef.current = activeFilter;
  const typesRef = useRef({ faceTypes, edgeTypes, faceBodies });
  typesRef.current = { faceTypes, edgeTypes, faceBodies };
  meshRef.current = mesh;

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
    const model = new THREE.Group();
    scene.add(model);
    const highlight = new THREE.Group();
    scene.add(highlight);
    const overlayGroup = new THREE.Group();
    scene.add(overlayGroup);
    const meshesGroup = new THREE.Group();
    scene.add(meshesGroup);
    const toolsGroup = new THREE.Group();
    scene.add(toolsGroup);
    const marksGroup = new THREE.Group();
    scene.add(marksGroup);
    const hoverGroup = new THREE.Group();
    scene.add(hoverGroup);
    const datumsGroup = new THREE.Group();
    scene.add(datumsGroup);

    const controls = new OrbitControls(camera, renderer.domElement);
    controls.screenSpacePanning = true;
    const render = () => {
      renderer.render(scene, camera);
      placeLabel(stateRef.current, camera, renderer.domElement);
    };
    controls.addEventListener('change', render);
    const state: SceneState = {
      renderer,
      scene,
      camera,
      controls,
      model,
      faces: null,
      edgeBuffers: null,
      highlight,
      hover: hoverGroup,
      selectedFaceSet: new Set(),
      hoverFaceSet: new Set(),
      overlay: overlayGroup,
      meshes: meshesGroup,
      tools: toolsGroup,
      marks: marksGroup,
      datums: datumsGroup,
      clip: null,
      label: null,
      framed: false,
      bodies: null,
      box: new THREE.Box3(),
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

    type Picked =
      | { kind: 'face'; faceIndex: number; point: Vec3 }
      | { kind: 'edge'; edgeIndex: number; point: Vec3 }
      | { kind: 'vertex'; vertexIndex: number; point: Vec3 }
      | { kind: 'body'; bodyIndex: number; faceIndex: number; point: Vec3 }
      /** something lies under the cursor but the filter refuses it */
      | { kind: 'blocked' }
      | null;

    const clientProjector = (rect: DOMRect) => (x: number, y: number, z: number) => {
      const world = new THREE.Vector3(x, y, z);
      const p = world.clone().project(camera);
      const clipped = state.clip ? state.clip.distanceToPoint(world) < -1e-6 : false;
      return {
        x: rect.left + ((p.x + 1) / 2) * rect.width,
        y: rect.top + ((1 - p.y) / 2) * rect.height,
        visible: p.z > -1 && p.z < 1 && !clipped,
      };
    };

    const castRay = (clientX: number, clientY: number) => {
      const rect = renderer.domElement.getBoundingClientRect();
      const ndc = new THREE.Vector2(((clientX - rect.left) / rect.width) * 2 - 1, -((clientY - rect.top) / rect.height) * 2 + 1);
      const raycaster = new THREE.Raycaster();
      raycaster.setFromCamera(ndc, camera);
      const current = meshRef.current;
      // The first hit on the kept side of the section plane.
      const hit =
        state.faces && current
          ? raycaster.intersectObject(state.faces, false).find((h) => !state.clip || state.clip.distanceToPoint(h.point) >= 0)
          : undefined;
      return { rect, raycaster, hit };
    };

    /** What the cursor is over in the pick mode, with the selection filter applied. */
    const pickAt = (clientX: number, clientY: number, ray: ReturnType<typeof castRay>): Picked => {
      const mode = pickModeRef.current;
      const current = meshRef.current;
      const { rect, raycaster, hit } = ray;
      const types = typesRef.current;
      const active = filterRef.current;
      if (mode === 'vertex') {
        const points = verticesRef.current;
        if (!points || points.length === 0) return null;
        const size = state.faces?.geometry.boundingSphere?.radius ?? 1;
        const { origin, direction } = raycaster.ray;
        // Nearest vertex on screen within the tolerance, not hidden by a face nor clipped away.
        let best: { index: number; distance: number } | null = null;
        for (let i = 0; i * 3 < points.length; ++i) {
          const p = new THREE.Vector3(points[i * 3], points[i * 3 + 1], points[i * 3 + 2]);
          if (state.clip && state.clip.distanceToPoint(p) < -size * 1e-6) continue;
          const q = p.clone().project(camera);
          if (!(q.z > -1 && q.z < 1)) continue;
          const sx = rect.left + ((q.x + 1) / 2) * rect.width;
          const sy = rect.top + ((1 - q.y) / 2) * rect.height;
          const distance = Math.hypot(sx - clientX, sy - clientY);
          if (distance > VERTEX_TOLERANCE || (best && distance >= best.distance)) continue;
          const along = p.clone().sub(origin).dot(direction);
          if (hit && along > hit.distance + size * 2e-3) continue;
          best = { index: i, distance };
        }
        const k = best ? best.index * 3 : -1;
        return best ? { kind: 'vertex', vertexIndex: best.index, point: [points[k], points[k + 1], points[k + 2]] } : null;
      }
      if (mode === 'edge') {
        const buffers = state.edgeBuffers;
        if (!buffers || !current) return null;
        const size = state.faces?.geometry.boundingSphere?.radius ?? 1;
        const { origin, direction } = raycaster.ray;
        const o: Vec3 = [origin.x, origin.y, origin.z];
        const d: Vec3 = [direction.x, direction.y, direction.z];
        const vertex = (v: number): Vec3 => [buffers.positions[v * 3], buffers.positions[v * 3 + 1], buffers.positions[v * 3 + 2]];
        // Nearest edge not hidden by a face (its point closest to the cursor ray must not lie
        // behind the first face the ray hits) and passing the type filter.
        for (const c of edgeCandidates(buffers, clientProjector(rect), clientX, clientY, EDGE_TOLERANCE)) {
          if (!edgeAllowed(active, types.edgeTypes?.[c.edgeIndex])) continue;
          const near = closestPointOnSegmentToRay(o, d, vertex(buffers.indices[c.segment * 2]), vertex(buffers.indices[c.segment * 2 + 1]));
          if (hit && near.rayDistance > hit.distance + size * 2e-3) continue;
          return { kind: 'edge', edgeIndex: c.edgeIndex, point: near.point };
        }
        return null;
      }
      if (!hit || hit.faceIndex === undefined || !current) return null;
      const faceIndex = faceOfTriangle(current.faceSpans, hit.faceIndex);
      if (faceIndex < 0) return null;
      const point: Vec3 = [hit.point.x, hit.point.y, hit.point.z];
      if (mode === 'body') {
        const body = types.faceBodies?.[faceIndex];
        return body === undefined || body === NO_BODY ? null : { kind: 'body', bodyIndex: body, faceIndex, point };
      }
      if (!faceAllowed(active, types.faceTypes?.[faceIndex])) return { kind: 'blocked' };
      return { kind: 'face', faceIndex, point };
    };

    const hoverKeyOf = (picked: Picked): string => {
      if (!picked) return '';
      if (picked.kind === 'face') return `face:${picked.faceIndex}`;
      if (picked.kind === 'body') return `body:${picked.bodyIndex}`;
      if (picked.kind === 'edge') return `edge:${picked.edgeIndex}`;
      if (picked.kind === 'vertex') return `vertex:${picked.vertexIndex}`;
      return '';
    };

    // ---- hover: only elements the filter allows are highlighted
    let hoverKey = '';
    let hoverFrame = 0;
    const setHover = (picked: Picked) => {
      const current = meshRef.current;
      const faces = new Set<number>();
      disposeModel(state.hover);
      if (picked && picked.kind === 'face') {
        faces.add(picked.faceIndex);
      } else if (picked && picked.kind === 'body') {
        for (const f of facesOfBodies(typesRef.current.faceBodies ?? undefined, [picked.bodyIndex])) faces.add(f);
      } else if (picked && picked.kind === 'edge' && current) {
        const chosen = current.edges.filter((e) => e.edgeIndex === picked.edgeIndex);
        const lines = buildEdges(packEdgePolylines(chosen), HOVER_COLOR);
        lines.name = 'kernel-hover-edge';
        lines.renderOrder = 9;
        (lines.material as unknown as { depthTest: boolean }).depthTest = false;
        state.hover.add(lines);
      } else if (picked && picked.kind === 'vertex') {
        state.hover.add(pointsObject(picked.point, HOVER_COLOR, 12, 'kernel-hover-vertex'));
      }
      state.hoverFaceSet = faces;
      applyFaceMaterials(state, current);
      hoverKey = hoverKeyOf(picked);
      renderer.domElement.parentElement?.setAttribute('data-hover', hoverKey);
      render();
    };
    const hoverAt = (clientX: number, clientY: number) => {
      if (pickModeRef.current === 'none') {
        if (hoverKey) setHover(null);
        return;
      }
      const picked = pickAt(clientX, clientY, castRay(clientX, clientY));
      const next = picked && picked.kind !== 'blocked' ? picked : null;
      if (hoverKeyOf(next) !== hoverKey) setHover(next);
    };

    // ---- box selection rectangle
    let down: { x: number; y: number } | null = null;
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
    const onPointerDown = (e: PointerEvent) => {
      down = { x: e.clientX, y: e.clientY };
      if (hoverKey) setHover(null);
    };
    const onPointerMove = (e: PointerEvent) => {
      if (down) {
        if (boxSelectRef.current && (e.buttons & 1) !== 0 && Math.hypot(e.clientX - down.x, e.clientY - down.y) > CLICK_TOLERANCE) {
          drawBox(down.x, down.y, e.clientX, e.clientY);
        }
        return;
      }
      if (hoverFrame) cancelAnimationFrame(hoverFrame);
      const { clientX, clientY } = e;
      hoverFrame = requestAnimationFrame(() => {
        hoverFrame = 0;
        hoverAt(clientX, clientY);
      });
    };
    const onPointerLeave = () => {
      if (hoverKey) setHover(null);
    };
    const finishBox = (start: { x: number; y: number }, e: PointerEvent): void => {
      const current = meshRef.current;
      hideBox();
      if (!current) return;
      const rect = renderer.domElement.getBoundingClientRect();
      const kind = boxKind(start, { x: e.clientX });
      const types = typesRef.current;
      const result = boxSelectMesh(
        {
          ...current,
          vertices: verticesRef.current,
          faceTypes: types.faceTypes ?? undefined,
          edgeTypes: types.edgeTypes ?? undefined,
          faceBodies: types.faceBodies ?? undefined,
        },
        clientProjector(rect),
        normalizeRect(start, { x: e.clientX, y: e.clientY }),
        kind,
        filterRef.current,
      );
      onBoxSelectRef.current?.(result, selectOp(e), kind);
    };
    const onPointerUp = (e: PointerEvent) => {
      if (!down || e.button !== 0) return;
      const start = down;
      const moved = Math.hypot(e.clientX - down.x, e.clientY - down.y);
      down = null;
      if (moved > CLICK_TOLERANCE) {
        if (boxSelectRef.current && pickModeRef.current !== 'none') finishBox(start, e);
        return;
      }
      const current = meshRef.current;
      const mode = pickModeRef.current;
      if (mode === 'none') return;
      const ray = castRay(e.clientX, e.clientY);
      const { raycaster, hit } = ray;
      const op = selectOp(e);
      // A point on a reference mesh in front of the model (measurements).
      const meshPick = onPickMeshRef.current;
      if (meshPick) {
        const mh = referenceMeshHit(raycaster, state.meshes, (p) => !state.clip || state.clip.distanceToPoint(p) >= 0);
        if (mh && (!hit || mh.distance < hit.distance)) {
          meshPick(mh.pick);
          return;
        }
      }
      const picked = pickAt(e.clientX, e.clientY, ray);
      if (picked && picked.kind === 'blocked') return;
      if (mode === 'vertex') {
        onPickVertexRef.current?.(picked && picked.kind === 'vertex' ? { op, vertexIndex: picked.vertexIndex, point: picked.point } : null);
      } else if (mode === 'edge') {
        onPickEdgeRef.current?.(picked && picked.kind === 'edge' ? { op, edgeIndex: picked.edgeIndex, point: picked.point } : null);
      } else if (mode === 'body') {
        onPickBodyRef.current?.(
          picked && picked.kind === 'body' ? { op, bodyIndex: picked.bodyIndex, faceIndex: picked.faceIndex, point: picked.point } : null,
        );
      } else if (!current) {
        onPickRef.current(null);
      } else {
        onPickRef.current(picked && picked.kind === 'face' ? { op, faceIndex: picked.faceIndex, point: picked.point } : null);
      }
    };
    renderer.domElement.addEventListener('pointerdown', onPointerDown);
    renderer.domElement.addEventListener('pointerup', onPointerUp);
    renderer.domElement.addEventListener('pointermove', onPointerMove);
    renderer.domElement.addEventListener('pointerleave', onPointerLeave);

    onReady?.({
      worldToClient(x, y, z) {
        const p = new THREE.Vector3(x, y, z).project(camera);
        const rect = renderer.domElement.getBoundingClientRect();
        return { x: rect.left + ((p.x + 1) / 2) * rect.width, y: rect.top + ((1 - p.y) / 2) * rect.height };
      },
    });

    return () => {
      onReady?.(null);
      observer.disconnect();
      renderer.domElement.removeEventListener('pointerdown', onPointerDown);
      renderer.domElement.removeEventListener('pointerup', onPointerUp);
      renderer.domElement.removeEventListener('pointermove', onPointerMove);
      renderer.domElement.removeEventListener('pointerleave', onPointerLeave);
      if (hoverFrame) cancelAnimationFrame(hoverFrame);
      hideBox();
      controls.removeEventListener('change', render);
      controls.dispose();
      disposeModel(model);
      disposeModel(highlight);
      disposeModel(hoverGroup);
      disposeModel(overlayGroup);
      disposeModel(meshesGroup);
      disposeModel(toolsGroup);
      disposeModel(marksGroup);
      disposeModel(datumsGroup);
      state.label?.element.remove();
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

  // Box selection: the left button drags a rectangle, so it no longer orbits.
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    state.controls.mouseButtons.LEFT = boxSelect ? -1 : 0;
    state.renderer.domElement.parentElement?.setAttribute('data-box-select', boxSelect ? '1' : '0');
  }, [boxSelect]);

  // Replace the model when the kernel mesh changes.
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    disposeModel(state.model);
    state.faces = null;
    state.edgeBuffers = null;
    if (mesh && mesh.indices.length > 0) {
      const faces = buildFaces(mesh);
      state.faces = faces;
      state.edgeBuffers = packEdgePolylines(mesh.edges);
      state.model.add(faces);
      state.model.add(buildEdges(state.edgeBuffers));
      faces.geometry.computeBoundingBox();
      const box = new THREE.Box3();
      if (faces.geometry.boundingBox) box.copy(faces.geometry.boundingBox);
      if (faces.geometry.boundingSphere && (autoFrame === 'always' || !state.framed)) {
        frame(state, faces.geometry.boundingSphere);
        state.framed = true;
      } else if (
        faces.geometry.boundingSphere &&
        shouldReframePart({ bodiesBefore: state.bodies, bodies: bodyCount, boxBefore: state.box, box, camera: state.camera })
      ) {
        // An operation added a body the camera does not show (a pattern, a copy): show all.
        frame(state, faces.geometry.boundingSphere);
      }
      state.box = box;
      state.bodies = bodyCount;
    }
    state.render();
    // autoFrame is a mount-time choice.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [mesh]);

  // Highlight: the selected faces' draw groups use material 1 (hovered ones 2); selected edges are redrawn.
  const faceKey = [selectedFace ?? -1, ...(selectedFaces ?? [])].join(',');
  const edgeKey = (selectedEdges ?? []).join(',');
  const bodyKey = (selectedBodies ?? []).join(',');
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    const faceSet = new Set<number>(selectedFaces ?? []);
    if (selectedFace !== null && selectedFace !== undefined) faceSet.add(selectedFace);
    for (const f of facesOfBodies(faceBodies ?? undefined, selectedBodies ?? [])) faceSet.add(f);
    state.selectedFaceSet = faceSet;
    applyFaceMaterials(state, mesh);
    disposeModel(state.highlight);
    if (mesh && selectedEdges && selectedEdges.length > 0) {
      const edgeSet = new Set(selectedEdges);
      const chosen = mesh.edges.filter((e) => edgeSet.has(e.edgeIndex));
      const lines = buildEdges(packEdgePolylines(chosen), SELECTED_COLOR);
      lines.name = 'kernel-selected-edges';
      state.highlight.add(lines);
    }
    state.render();
    // faceKey, edgeKey and bodyKey stand for the selection arrays.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [mesh, faceKey, edgeKey, bodyKey, faceBodies]);

  // Vertex dots of the vertex mode and the selected vertices.
  const vertexKey = (selectedVertices ?? []).join(',');
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    disposeModel(state.marks);
    if (vertices && vertices.length > 0) {
      if (pickMode === 'vertex') state.marks.add(pointsObject(vertices, VERTEX_COLOR, 5, 'kernel-vertices'));
      const chosen = (selectedVertices ?? [])
        .filter((i) => i * 3 + 2 < vertices.length)
        .flatMap((i) => [vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]]);
      if (chosen.length > 0) state.marks.add(pointsObject(chosen, SELECTED_COLOR, 10, 'kernel-selected-vertices'));
    }
    state.render();
    // vertexKey stands for the selection array.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [vertices, pickMode, vertexKey]);

  // Datum glyphs; their size follows the model (a fifth of its bounding radius, 10 mm without one).
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    disposeModel(state.datums);
    const list = datums ?? [];
    const radius = state.faces?.geometry.boundingSphere?.radius;
    const size = radius && radius > 0 ? radius * 0.2 : 10;
    for (const o of buildDatums(list, size)) state.datums.add(o);
    const parent = state.renderer.domElement.parentElement;
    parent?.setAttribute('data-datum-points', String(list.filter((d) => d.kind === 'point').length));
    parent?.setAttribute('data-datum-csys', String(list.filter((d) => d.kind === 'csys').length));
    parent?.setAttribute('data-datum-planes', String(list.filter((d) => d.kind === 'plane').length));
    state.render();
  }, [datums, mesh]);

  // Overlay (sketch regions): translucent faces and their boundary edges.
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    disposeModel(state.overlay);
    if (overlay && overlay.indices.length > 0) {
      const parts = buildOverlay(overlay);
      for (const o of parts) state.overlay.add(o);
      const g = (parts[0] as THREE.Mesh).geometry;
      g.computeBoundingSphere();
      if (!state.faces && autoFrame === 'first' && !state.framed && g.boundingSphere) {
        frame(state, g.boundingSphere);
      }
    }
    state.render();
    // autoFrame is a mount-time choice.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [overlay]);

  // Reference meshes: translucent, framed when there is no model yet.
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    disposeModel(state.meshes);
    const list = referenceMeshes ?? [];
    const sphere = new THREE.Box3();
    for (const m of list) {
      if (m.indices.length === 0) continue;
      const obj = buildReferenceMesh(m);
      state.meshes.add(obj);
      sphere.expandByObject(obj);
    }
    state.renderer.domElement.parentElement?.setAttribute('data-reference-meshes', String(state.meshes.children.length));
    if (!state.faces && state.meshes.children.length > 0 && !sphere.isEmpty()) {
      const bs = new THREE.Sphere();
      sphere.getBoundingSphere(bs);
      if (!state.framed || autoFrame === 'always') frame(state, bs);
    }
    state.render();
    // autoFrame is a mount-time choice.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [referenceMeshes]);

  // Section plane: a renderer clipping plane, so the slider needs no kernel call.
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

  // Section contour from the kernel and the lines of a measurement.
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    disposeModel(state.tools);
    for (const line of sectionLines ?? []) {
      if (line.length >= 6) state.tools.add(lineObject(line, SECTION_COLOR, 'kernel-section'));
    }
    if (measure) {
      for (const line of measure.lines) state.tools.add(lineObject(line.flat(), MEASURE_COLOR, 'kernel-measure'));
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
    state.renderer.domElement.parentElement?.setAttribute('data-section-lines', String(sectionLines?.length ?? 0));
    state.render();
  }, [sectionLines, measure]);

  return <div ref={containerRef} className="kernel-viewport" data-testid="kernel-viewport" />;
};

export default KernelViewport;
