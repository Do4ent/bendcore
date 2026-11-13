/**
 * View of the sketch editor (docs/ui/SKETCH_EDITOR.md): the sketch plane seen along its normal.
 *
 * Two layers share one 2D view (centre and scale in sketch millimetres): a three.js canvas with
 * the body the sketch is drawn on (the kernel mesh, moved into the sketch frame and seen through
 * an orthographic camera), and an SVG overlay with the grid, the regions of the profile (kernel
 * triangulation), the curves (kernel polylines), points, dimensions, the preview of the shape
 * being drawn and the snap marker. Pointer events are reported in sketch coordinates.
 */

import React, { useEffect, useImperativeHandle, useLayoutEffect, useRef, useState } from 'react';
import * as THREE from 'three';
import type { KernelMesh } from '../../../wasm/bendcore_kernel_api.mjs';
import type { SketchPlacementInfo, SketchSolution, SketchSolvedCurve } from '../../../wasm/bendcore_modeling_api.mjs';
import { packEdgePolylines } from '../../kernel/edgeBuffers';
import type { DimensionGraphic } from './dimensionLayout';
import type { DefinitionState } from './sketchModel';
import { gridStep, nearestPoint } from './snap';
import { caption, useI18n } from '../../app/i18n';
import type { Snap } from './snap';

export interface SketchPointerEvent {
  /** sketch coordinates, mm */
  x: number;
  y: number;
  /** sketch units per CSS pixel */
  unitsPerPixel: number;
  shift: boolean;
  ctrl: boolean;
}

export interface SketchCanvasHandle {
  sketchToClient(x: number, y: number): { x: number; y: number };
  fit(): void;
}

export interface SketchCanvasProps {
  solution: SketchSolution | null;
  /** curves of the shape being drawn (kernel solve of the tentative data) */
  preview: SketchSolvedCurve[];
  placement: SketchPlacementInfo | null;
  body: KernelMesh | null;
  dimensions: DimensionGraphic[];
  curveState: (entity: number) => DefinitionState;
  pointState: (index: number) => DefinitionState;
  constraintClass: (k: number) => string;
  selectedEntities: ReadonlySet<number>;
  selectedPoints: ReadonlySet<number>;
  selectedConstraints: ReadonlySet<number>;
  snap: Snap | null;
  showRegions: boolean;
  cursor: string;
  onDown: (e: SketchPointerEvent) => void;
  onMove: (e: SketchPointerEvent) => void;
  onUp: (e: SketchPointerEvent, moved: boolean) => void;
  onDoubleClick: (e: SketchPointerEvent) => void;
  onDimensionClick: (k: number) => void;
  onDimensionEdit: (k: number) => void;
  /** text of dimension k dragged to (x, y) */
  onDimensionMove: (k: number, x: number, y: number) => void;
  children?: React.ReactNode;
}

interface View {
  cx: number;
  cy: number;
  scale: number;
}

const CLICK_TOLERANCE = 4;
/** A dimension text does not take clicks within this distance (px) of a sketch point. */
const POINT_PRIORITY_PIXELS = 6;

interface BodyLayer {
  renderer: THREE.WebGLRenderer;
  scene: THREE.Scene;
  camera: THREE.OrthographicCamera;
  group: THREE.Group;
}

function clearGroup(group: THREE.Group): void {
  for (const child of [...group.children]) {
    group.remove(child);
    const o = child as THREE.Object3D & { geometry?: THREE.BufferGeometry; material?: THREE.Material };
    o.geometry?.dispose();
    o.material?.dispose();
  }
}

/** Bounding box of everything solved, in sketch coordinates; null when the sketch is empty. */
function bounds(solution: SketchSolution | null): { minX: number; minY: number; maxX: number; maxY: number } | null {
  if (!solution) return null;
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  const add = (x: number, y: number) => {
    minX = Math.min(minX, x);
    minY = Math.min(minY, y);
    maxX = Math.max(maxX, x);
    maxY = Math.max(maxY, y);
  };
  for (const p of solution.points) add(p.x, p.y);
  for (const c of solution.curves) for (let i = 0; i + 1 < c.polyline.length; i += 2) add(c.polyline[i], c.polyline[i + 1]);
  return Number.isFinite(minX) ? { minX, minY, maxX, maxY } : null;
}

const SketchCanvas = React.forwardRef<SketchCanvasHandle, SketchCanvasProps>((props, ref) => {
  const { t } = useI18n();
  const rootRef = useRef<HTMLDivElement>(null);
  const glRef = useRef<HTMLDivElement>(null);
  const layerRef = useRef<BodyLayer | null>(null);
  const [size, setSize] = useState({ w: 800, h: 600 });
  const [view, setView] = useState<View>({ cx: 30, cy: 20, scale: 6 });
  const viewRef = useRef(view);
  viewRef.current = view;
  const sizeRef = useRef(size);
  sizeRef.current = size;
  const propsRef = useRef(props);
  propsRef.current = props;

  const toClientLocal = (x: number, y: number, v = view, s = size) => ({
    x: s.w / 2 + (x - v.cx) * v.scale,
    y: s.h / 2 - (y - v.cy) * v.scale,
  });
  const toSketch = (px: number, py: number) => {
    const v = viewRef.current;
    const s = sizeRef.current;
    return { x: v.cx + (px - s.w / 2) / v.scale, y: v.cy - (py - s.h / 2) / v.scale };
  };

  const fit = () => {
    const b = bounds(propsRef.current.solution);
    const s = sizeRef.current;
    if (!b) {
      setView({ cx: 0, cy: 0, scale: Math.min(s.w, s.h) / 120 });
      return;
    }
    const w = Math.max(b.maxX - b.minX, 10);
    const h = Math.max(b.maxY - b.minY, 10);
    setView({ cx: (b.minX + b.maxX) / 2, cy: (b.minY + b.maxY) / 2, scale: Math.min(s.w / w, s.h / h) * 0.7 });
  };

  useImperativeHandle(ref, () => ({
    sketchToClient(x, y) {
      const rect = rootRef.current!.getBoundingClientRect();
      const p = toClientLocal(x, y, viewRef.current, sizeRef.current);
      return { x: rect.left + p.x, y: rect.top + p.y };
    },
    fit,
  }));

  // Size of the view; the first measured size frames the sketch.
  const framed = useRef(false);
  useLayoutEffect(() => {
    const root = rootRef.current;
    if (!root) return undefined;
    const measure = () => {
      const s = { w: Math.max(root.clientWidth, 1), h: Math.max(root.clientHeight, 1) };
      sizeRef.current = s;
      setSize(s);
      if (!framed.current) {
        framed.current = true;
        fit();
      }
    };
    const observer = new ResizeObserver(measure);
    observer.observe(root);
    measure();
    return () => observer.disconnect();
    // The observer lives as long as the component.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // The first solution frames the view when it has geometry (entering an existing sketch);
  // later solutions never move the view.
  const framedData = useRef(false);
  useEffect(() => {
    if (framedData.current || !props.solution) return;
    framedData.current = true;
    if (bounds(props.solution)) fit();
    // fit reads the latest props through propsRef.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [props.solution]);

  // Body layer: renderer and scene once.
  useEffect(() => {
    const host = glRef.current;
    if (!host) return undefined;
    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.setClearColor(0xf8fafc, 1);
    host.appendChild(renderer.domElement);
    const scene = new THREE.Scene();
    const camera = new THREE.OrthographicCamera(-1, 1, 1, -1, 0.1, 2e6);
    camera.position.set(0, 0, 1e6);
    camera.lookAt(0, 0, 0);
    const group = new THREE.Group();
    group.matrixAutoUpdate = false;
    scene.add(group);
    layerRef.current = { renderer, scene, camera, group };
    return () => {
      clearGroup(group);
      renderer.dispose();
      host.removeChild(renderer.domElement);
      layerRef.current = null;
    };
  }, []);

  // Body mesh in the sketch frame: local = R^T (world - origin).
  const { body, placement } = props;
  useEffect(() => {
    const layer = layerRef.current;
    if (!layer) return;
    clearGroup(layer.group);
    if (body && placement && body.indices.length > 0) {
      const x = new THREE.Vector3(...placement.xDirection);
      const y = new THREE.Vector3(...placement.yDirection);
      const n = new THREE.Vector3(...placement.normal);
      const m = new THREE.Matrix4().makeBasis(x, y, n).setPosition(new THREE.Vector3(...placement.origin));
      layer.group.matrix.copy(m).invert();
      const geometry = new THREE.BufferGeometry();
      geometry.setAttribute('position', new THREE.BufferAttribute(body.positions, 3));
      geometry.setIndex(new THREE.BufferAttribute(body.indices, 1));
      layer.group.add(new THREE.Mesh(geometry, new THREE.MeshBasicMaterial({ color: 0xdfe5ee, side: THREE.DoubleSide })));
      const edges = packEdgePolylines(body.edges);
      const eg = new THREE.BufferGeometry();
      eg.setAttribute('position', new THREE.BufferAttribute(edges.positions, 3));
      eg.setIndex(new THREE.BufferAttribute(edges.indices, 1));
      layer.group.add(new THREE.LineSegments(eg, new THREE.LineBasicMaterial({ color: 0x9aa7b8 })));
    }
    layer.renderer.render(layer.scene, layer.camera);
  }, [body, placement]);

  // Camera follows the 2D view.
  useEffect(() => {
    const layer = layerRef.current;
    if (!layer) return;
    layer.renderer.setSize(size.w, size.h);
    const hw = size.w / (2 * view.scale);
    const hh = size.h / (2 * view.scale);
    layer.camera.left = view.cx - hw;
    layer.camera.right = view.cx + hw;
    layer.camera.top = view.cy + hh;
    layer.camera.bottom = view.cy - hh;
    layer.camera.updateProjectionMatrix();
    layer.renderer.render(layer.scene, layer.camera);
  }, [view, size, body, placement]);

  // ------------------------------------------------------------------ pointer

  const drag = useRef<{ x: number; y: number; button: number; moved: boolean; view: View } | null>(null);
  const labelDrag = useRef<{ k: number; x: number; y: number; moved: boolean } | null>(null);
  // Dimension text clicked last: with pointer capture on the SVG the double click is delivered
  // to the SVG, not to the text.
  const lastLabel = useRef<number | null>(null);

  const eventOf = (e: { clientX: number; clientY: number; shiftKey: boolean; ctrlKey: boolean; metaKey: boolean }): SketchPointerEvent => {
    const rect = rootRef.current!.getBoundingClientRect();
    const p = toSketch(e.clientX - rect.left, e.clientY - rect.top);
    return { ...p, unitsPerPixel: 1 / viewRef.current.scale, shift: e.shiftKey, ctrl: e.ctrlKey || e.metaKey };
  };

  const onPointerDown = (e: React.PointerEvent) => {
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    drag.current = { x: e.clientX, y: e.clientY, button: e.button, moved: false, view: viewRef.current };
    if (e.button === 0 && !labelDrag.current) props.onDown(eventOf(e));
  };
  const onPointerMove = (e: React.PointerEvent) => {
    const d = drag.current;
    if (d && Math.hypot(e.clientX - d.x, e.clientY - d.y) > CLICK_TOLERANCE) d.moved = true;
    if (d && d.button !== 0) {
      // Pan with the middle or right button.
      setView({ ...d.view, cx: d.view.cx - (e.clientX - d.x) / d.view.scale, cy: d.view.cy + (e.clientY - d.y) / d.view.scale });
      return;
    }
    const l = labelDrag.current;
    if (l && d) {
      if (Math.hypot(e.clientX - l.x, e.clientY - l.y) > CLICK_TOLERANCE) l.moved = true;
      if (l.moved) {
        const p = eventOf(e);
        props.onDimensionMove(l.k, p.x, p.y);
      }
      return;
    }
    props.onMove(eventOf(e));
  };
  const onPointerUp = (e: React.PointerEvent) => {
    const d = drag.current;
    drag.current = null;
    const l = labelDrag.current;
    labelDrag.current = null;
    if (!d || d.button !== 0) return;
    if (l) {
      lastLabel.current = l.moved ? null : l.k;
      if (!l.moved) props.onDimensionClick(l.k);
      return;
    }
    lastLabel.current = null;
    props.onUp(eventOf(e), d.moved);
  };
  const onWheel = (e: React.WheelEvent) => {
    const rect = rootRef.current!.getBoundingClientRect();
    const px = e.clientX - rect.left;
    const py = e.clientY - rect.top;
    const before = toSketch(px, py);
    const v = viewRef.current;
    const scale = Math.min(1e4, Math.max(1e-3, v.scale * Math.exp(-e.deltaY * 0.0015)));
    const s = sizeRef.current;
    // Keep the point under the cursor in place.
    setView({ scale, cx: before.x - (px - s.w / 2) / scale, cy: before.y + (py - s.h / 2) / scale });
  };

  // ------------------------------------------------------------------ drawing

  const c = (x: number, y: number) => toClientLocal(x, y);
  const pointsAttr = (poly: ArrayLike<number>) => {
    const parts: string[] = [];
    for (let i = 0; i + 1 < poly.length; i += 2) {
      const p = c(poly[i], poly[i + 1]);
      parts.push(`${p.x.toFixed(2)},${p.y.toFixed(2)}`);
    }
    return parts.join(' ');
  };

  const step = gridStep(view.scale);
  const gridLines: React.ReactNode[] = [];
  {
    const min = toSketch(0, size.h);
    const max = toSketch(size.w, 0);
    for (let i = Math.ceil(min.x / step); i * step <= max.x; ++i) {
      const x = c(i * step, 0).x;
      gridLines.push(<line key={`gx${i}`} x1={x} y1={0} x2={x} y2={size.h} className={i % 5 === 0 ? 'sk-grid-major' : 'sk-grid'} />);
    }
    for (let j = Math.ceil(min.y / step); j * step <= max.y; ++j) {
      const y = c(0, j * step).y;
      gridLines.push(<line key={`gy${j}`} x1={0} y1={y} x2={size.w} y2={y} className={j % 5 === 0 ? 'sk-grid-major' : 'sk-grid'} />);
    }
  }
  const origin = c(0, 0);
  const { solution } = props;

  const regionPaths: React.ReactNode[] = [];
  if (props.showRegions && solution?.profile) {
    for (const r of solution.profile.regions) {
      let d = '';
      for (let t = 0; t + 2 < r.indices.length; t += 3) {
        const v = [0, 1, 2].map((k) => c(r.positions[r.indices[t + k] * 3], r.positions[r.indices[t + k] * 3 + 1]));
        d += `M${v[0].x.toFixed(1)} ${v[0].y.toFixed(1)}L${v[1].x.toFixed(1)} ${v[1].y.toFixed(1)}L${v[2].x.toFixed(1)} ${v[2].y.toFixed(1)}Z`;
      }
      regionPaths.push(
        <path key={r.index} d={d} className={`sk-region${r.selected ? ' sk-region-selected' : ''}`} data-testid={`sketch-region-${r.index}`} data-selected={r.selected} />,
      );
    }
  }

  const snap = props.snap;
  const snapAt = snap && snap.kind !== 'free' ? c(snap.x, snap.y) : null;

  return (
    <div
      ref={rootRef}
      className="sk-canvas"
      data-testid="sketch-canvas"
      style={{ cursor: props.cursor }}
      onContextMenu={(e) => e.preventDefault()}
    >
      <div ref={glRef} className="sk-layer" />
      <svg
        className="sk-layer sk-svg"
        width={size.w}
        height={size.h}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onWheel={onWheel}
        onDoubleClick={(e) => {
          if (lastLabel.current !== null) props.onDimensionEdit(lastLabel.current);
          else props.onDoubleClick(eventOf(e));
        }}
      >
        {gridLines}
        <line x1={0} y1={origin.y} x2={size.w} y2={origin.y} className="sk-axis-x" />
        <line x1={origin.x} y1={0} x2={origin.x} y2={size.h} className="sk-axis-y" />
        <circle cx={origin.x} cy={origin.y} r={3} className="sk-origin" />
        {regionPaths}
        {solution?.curves.map((cv) => (
          <polyline
            key={cv.entity}
            points={pointsAttr(cv.polyline)}
            className={`sk-curve sk-${props.curveState(cv.entity)}${cv.construction ? ' sk-construction' : ''}${
              props.selectedEntities.has(cv.entity) ? ' sk-selected' : ''
            }`}
            data-testid={`sketch-curve-${cv.entity}`}
            data-state={props.curveState(cv.entity)}
          />
        ))}
        {props.preview.map((cv, i) => (
          <polyline key={`pv${i}`} points={pointsAttr(cv.polyline)} className="sk-curve sk-preview" />
        ))}
        {solution?.points.map((p, i) => {
          const q = c(p.x, p.y);
          return (
            <circle
              key={i}
              cx={q.x}
              cy={q.y}
              r={props.selectedPoints.has(i) ? 4.5 : 3}
              className={`sk-point sk-${props.pointState(i)}${p.fixed ? ' sk-fixed' : ''}${props.selectedPoints.has(i) ? ' sk-selected' : ''}`}
              data-testid={`sketch-point-${i}`}
              data-state={props.pointState(i)}
            />
          );
        })}
        {props.dimensions.map((g) => {
          const t = c(g.text.x, g.text.y);
          const cls = `sk-dim ${props.constraintClass(g.constraint)}${props.selectedConstraints.has(g.constraint) ? ' sk-selected' : ''}`;
          return (
            <g key={g.constraint} className={cls}>
              {g.lines.map((l, i) => {
                const a = c(l[0], l[1]);
                const b = c(l[2], l[3]);
                return <line key={i} x1={a.x} y1={a.y} x2={b.x} y2={b.y} />;
              })}
              <text
                x={t.x}
                y={t.y}
                textAnchor="middle"
                dominantBaseline="middle"
                data-testid={`sketch-dim-${g.constraint}`}
                data-state={props.constraintClass(g.constraint)}
                onPointerDown={(e) => {
                  // A sketch point under the text keeps priority (corners under radius texts).
                  const p = eventOf(e);
                  const onPoint = solution && nearestPoint(solution, p.x, p.y, POINT_PRIORITY_PIXELS * p.unitsPerPixel);
                  if (e.button === 0 && !onPoint) labelDrag.current = { k: g.constraint, x: e.clientX, y: e.clientY, moved: false };
                }}
              >
                {g.label}
              </text>
            </g>
          );
        })}
        {snapAt && (
          <g className="sk-snap" data-testid="sketch-snap" data-kind={snap!.kind}>
            <rect x={snapAt.x - 5} y={snapAt.y - 5} width={10} height={10} />
            <text x={snapAt.x + 9} y={snapAt.y - 9}>
              {snap!.kind === 'free' ? '' : caption(t, 'sketch.snap.', snap!.kind)}
            </text>
          </g>
        )}
      </svg>
      {props.children}
    </div>
  );
});
SketchCanvas.displayName = 'SketchCanvas';

export default SketchCanvas;
