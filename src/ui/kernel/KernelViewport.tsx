/**
 * KernelViewport: three.js view of a tessellated kernel shape (docs/ui/KERNEL_VIEWPORT.md).
 *
 * The geometry is the kernel's mesh as is: positions, normals and indices from
 * KernelShape.tessellate() become one indexed BufferGeometry with one draw group per B-rep face
 * (faceSpans), edge polylines become one LineSegments object. A click casts a ray; the hit
 * triangle maps to its B-rep face by binary search over faceSpans (faceOfTriangle), and the
 * selected face is drawn with the highlight material by switching its group's material index.
 */

import React, { useEffect, useRef } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import { faceOfTriangle } from '../../wasm/bendcore_kernel_api.mjs';
import type { KernelMesh } from '../../wasm/bendcore_kernel_api.mjs';
import { packEdgePolylines } from './edgeBuffers';

export interface FacePick {
  faceIndex: number;
  /** Hit point in model coordinates, mm. */
  point: [number, number, number];
}

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
}

const FACE_COLOR = 0xb8c4d6;
const SELECTED_COLOR = 0xff8c1a;
const EDGE_COLOR = 0x1d2733;
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

function buildEdges(mesh: KernelMesh): THREE.LineSegments {
  const buffers = packEdgePolylines(mesh.edges);
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(buffers.positions, 3));
  geometry.setIndex(new THREE.BufferAttribute(buffers.indices, 1));
  const edges = new THREE.LineSegments(geometry, new THREE.LineBasicMaterial({ color: EDGE_COLOR }));
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

const KernelViewport: React.FC<KernelViewportProps> = ({ mesh, selectedFace, onPick, onReady }) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const stateRef = useRef<SceneState | null>(null);
  const meshRef = useRef<KernelMesh | null>(mesh);
  const onPickRef = useRef(onPick);
  onPickRef.current = onPick;
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

    const controls = new OrbitControls(camera, renderer.domElement);
    controls.screenSpacePanning = true;
    const render = () => renderer.render(scene, camera);
    controls.addEventListener('change', render);
    const state: SceneState = { renderer, scene, camera, controls, model, faces: null, render };
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
      if (!state.faces || !current) return;
      const rect = renderer.domElement.getBoundingClientRect();
      const ndc = new THREE.Vector2(
        ((e.clientX - rect.left) / rect.width) * 2 - 1,
        -((e.clientY - rect.top) / rect.height) * 2 + 1,
      );
      const raycaster = new THREE.Raycaster();
      raycaster.setFromCamera(ndc, camera);
      const hit = raycaster.intersectObject(state.faces, false)[0];
      if (!hit || hit.faceIndex === undefined) {
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
      renderer.dispose();
      container.removeChild(renderer.domElement);
      stateRef.current = null;
    };
    // onReady is read once per mount; the scene lives as long as the component.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Replace the model when the kernel mesh changes.
  useEffect(() => {
    const state = stateRef.current;
    if (!state) return;
    disposeModel(state.model);
    state.faces = null;
    if (mesh && mesh.indices.length > 0) {
      const faces = buildFaces(mesh);
      state.faces = faces;
      state.model.add(faces);
      state.model.add(buildEdges(mesh));
      if (faces.geometry.boundingSphere) frame(state, faces.geometry.boundingSphere);
    }
    state.render();
  }, [mesh]);

  // Highlight: the selected face's draw group uses material 1.
  useEffect(() => {
    const state = stateRef.current;
    if (!state || !state.faces || !mesh) return;
    const groups = state.faces.geometry.groups;
    for (let g = 0; g < groups.length; ++g) {
      groups[g].materialIndex = mesh.faceSpans[g * 3] === selectedFace ? 1 : 0;
    }
    state.render();
  }, [mesh, selectedFace]);

  return <div ref={containerRef} className="kernel-viewport" data-testid="kernel-viewport" />;
};

export default KernelViewport;
