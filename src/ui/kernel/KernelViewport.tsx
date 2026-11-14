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
 */

import React, { useEffect, useRef } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import { faceOfTriangle } from '../../wasm/bendcore_kernel_api.mjs';
import type { KernelMesh } from '../../wasm/bendcore_kernel_api.mjs';
import { packEdgePolylines } from './edgeBuffers';
import type { EdgeBuffers } from './edgeBuffers';
import { closestPointOnSegmentToRay, edgeCandidates } from './edgePick';

export interface FacePick {
  faceIndex: number;
  /** Hit point in model coordinates, mm. */
  point: [number, number, number];
}

export interface EdgePick {
  edgeIndex: number;
  /** Closest point of the edge polyline to the cursor, mm. */
  point: [number, number, number];
}

export type PickMode = 'face' | 'edge' | 'none';

export interface KernelViewportHandle {
  /** Client (CSS pixel) coordinates of a model point in the current view. */
  worldToClient(x: number, y: number, z: number): { x: number; y: number };
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
  /** Translucent mesh drawn over the model (e.g. sketch regions); not pickable. */
  overlay?: KernelMesh | null;
  /**
   * 'always' (default): frame the view on every new mesh. 'first': only on the first non-empty
   * mesh after mount or after frameKey changes, so edits keep the camera.
   */
  autoFrame?: 'always' | 'first';
  frameKey?: number;
}

const FACE_COLOR = 0xb8c4d6;
const SELECTED_COLOR = 0xff8c1a;
const EDGE_COLOR = 0x1d2733;
const OVERLAY_COLOR = 0x2f7de1;
/** Pointer distance (px) within which a click picks an edge. */
const EDGE_TOLERANCE = 6;
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
  overlay: THREE.Group;
  framed: boolean;
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
  ];
  const faces = new THREE.Mesh(geometry, materials);
  faces.name = 'kernel-faces';
  return faces;
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
}) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const stateRef = useRef<SceneState | null>(null);
  const meshRef = useRef<KernelMesh | null>(mesh);
  const onPickRef = useRef(onPick);
  onPickRef.current = onPick;
  const onPickEdgeRef = useRef(onPickEdge);
  onPickEdgeRef.current = onPickEdge;
  const pickModeRef = useRef(pickMode);
  pickModeRef.current = pickMode;
  meshRef.current = mesh;

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
    const model = new THREE.Group();
    scene.add(model);
    const highlight = new THREE.Group();
    scene.add(highlight);
    const overlayGroup = new THREE.Group();
    scene.add(overlayGroup);

    const controls = new OrbitControls(camera, renderer.domElement);
    controls.screenSpacePanning = true;
    const render = () => renderer.render(scene, camera);
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
      overlay: overlayGroup,
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

    let down: { x: number; y: number } | null = null;
    const onPointerDown = (e: PointerEvent) => {
      down = { x: e.clientX, y: e.clientY };
    };
    const onPointerUp = (e: PointerEvent) => {
      if (!down || e.button !== 0) return;
      const moved = Math.hypot(e.clientX - down.x, e.clientY - down.y);
      down = null;
      if (moved > CLICK_TOLERANCE) return;
      const current = meshRef.current;
      const mode = pickModeRef.current;
      if (mode === 'none') return;
      const rect = renderer.domElement.getBoundingClientRect();
      const ndc = new THREE.Vector2(
        ((e.clientX - rect.left) / rect.width) * 2 - 1,
        -((e.clientY - rect.top) / rect.height) * 2 + 1,
      );
      const raycaster = new THREE.Raycaster();
      raycaster.setFromCamera(ndc, camera);
      const hit = state.faces && current ? raycaster.intersectObject(state.faces, false)[0] : undefined;
      if (mode === 'edge') {
        const buffers = state.edgeBuffers;
        if (!buffers || !current) {
          onPickEdgeRef.current?.(null);
          return;
        }
        const project = (x: number, y: number, z: number) => {
          const p = new THREE.Vector3(x, y, z).project(camera);
          return {
            x: rect.left + ((p.x + 1) / 2) * rect.width,
            y: rect.top + ((1 - p.y) / 2) * rect.height,
            visible: p.z > -1 && p.z < 1,
          };
        };
        const size = state.faces?.geometry.boundingSphere?.radius ?? 1;
        const { origin, direction } = raycaster.ray;
        const o: [number, number, number] = [origin.x, origin.y, origin.z];
        const d: [number, number, number] = [direction.x, direction.y, direction.z];
        const vertex = (v: number): [number, number, number] => [
          buffers.positions[v * 3],
          buffers.positions[v * 3 + 1],
          buffers.positions[v * 3 + 2],
        ];
        // Nearest edge not hidden by a face: its point closest to the cursor ray must not lie
        // behind the first face the ray hits.
        let picked: EdgePick | null = null;
        for (const c of edgeCandidates(buffers, project, e.clientX, e.clientY, EDGE_TOLERANCE)) {
          const near = closestPointOnSegmentToRay(
            o,
            d,
            vertex(buffers.indices[c.segment * 2]),
            vertex(buffers.indices[c.segment * 2 + 1]),
          );
          if (hit && near.rayDistance > hit.distance + size * 2e-3) continue;
          picked = { edgeIndex: c.edgeIndex, point: near.point };
          break;
        }
        onPickEdgeRef.current?.(picked);
        return;
      }
      if (!hit || hit.faceIndex === undefined || !current) {
        onPickRef.current(null);
        return;
      }
      const faceIndex = faceOfTriangle(current.faceSpans, hit.faceIndex);
      onPickRef.current(faceIndex < 0 ? null : { faceIndex, point: [hit.point.x, hit.point.y, hit.point.z] });
    };
    renderer.domElement.addEventListener('pointerdown', onPointerDown);
    renderer.domElement.addEventListener('pointerup', onPointerUp);

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
      controls.removeEventListener('change', render);
      controls.dispose();
      disposeModel(model);
      disposeModel(highlight);
      disposeModel(overlayGroup);
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
      if (faces.geometry.boundingSphere && (autoFrame === 'always' || !state.framed)) {
        frame(state, faces.geometry.boundingSphere);
        state.framed = true;
      }
    }
    state.render();
    // autoFrame is a mount-time choice.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [mesh]);

  // Highlight: the selected faces' draw groups use material 1; selected edges are redrawn.
  const faceKey = [selectedFace ?? -1, ...(selectedFaces ?? [])].join(',');
  const edgeKey = (selectedEdges ?? []).join(',');
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    const faceSet = new Set<number>(selectedFaces ?? []);
    if (selectedFace !== null && selectedFace !== undefined) faceSet.add(selectedFace);
    if (state.faces && mesh) {
      const groups = state.faces.geometry.groups;
      for (let g = 0; g < groups.length; ++g) {
        groups[g].materialIndex = faceSet.has(mesh.faceSpans[g * 3]) ? 1 : 0;
      }
    }
    disposeModel(state.highlight);
    if (mesh && selectedEdges && selectedEdges.length > 0) {
      const edgeSet = new Set(selectedEdges);
      const chosen = mesh.edges.filter((e) => edgeSet.has(e.edgeIndex));
      const lines = buildEdges(packEdgePolylines(chosen), SELECTED_COLOR);
      lines.name = 'kernel-selected-edges';
      state.highlight.add(lines);
    }
    state.render();
    // faceKey and edgeKey stand for the selection arrays.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [mesh, faceKey, edgeKey]);

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

  return <div ref={containerRef} className="kernel-viewport" data-testid="kernel-viewport" />;
};

export default KernelViewport;
