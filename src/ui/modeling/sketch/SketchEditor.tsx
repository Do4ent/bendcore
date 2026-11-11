/**
 * Sketch editor with constraints (docs/ui/SKETCH_EDITOR.md). Edits the SketchData of one sketch
 * node of the part document: drawing tools, constraints and dimensions, dragging, editing tools,
 * region selection. Every change is solved by the kernel (PartDocumentHandle.solveSketch); the
 * geometry shown, the degrees of freedom, the definition of each element and the conflicting /
 * redundant constraints are the kernel's answer. Editing tools (trim, extend, offset, mirror,
 * patterns, corner fillet and chamfer) are kernel functions (editSketch). The document itself is
 * changed once, when the user leaves the sketch.
 */

import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { KernelMesh } from '../../../wasm/bendcore_kernel_api.mjs';
import type {
  PartDocumentHandle,
  SketchData,
  SketchSolution,
  SketchSolvedCurve,
  SketchTool,
} from '../../../wasm/bendcore_modeling_api.mjs';
import SketchCanvas from './SketchCanvas';
import type { SketchCanvasHandle, SketchPointerEvent } from './SketchCanvas';
import { dimensionGraphic, formatValue, offsetFor, twoPointDimension } from './dimensionLayout';
import type { DimensionGraphic } from './dimensionLayout';
import { DRAW_TOOL_LABELS, DrawError, DrawTool, PICK_COUNT, PICK_HINTS, drawShape } from './drawTools';
import { CONSTRAINT_LABELS, DEFINITION_LABELS, describeConstraint } from './sketchLabels';
import {
  ConstraintProposal,
  DIMENSION_TYPES,
  SketchRef,
  addConstraint,
  applicableConstraints,
  cloneSketch,
  constraintState,
  cornerLines,
  definitionState,
  emptySketch,
  entityRef,
  hasValue,
  highlights,
  pointRef,
  refKey,
  removeItems,
  toggleConstruction,
} from './sketchModel';
import { findSnap, nearestEntity, nearestPoint } from './snap';
import type { Snap } from './snap';
import './SketchEditor.css';

export type EditorTool = 'select' | DrawTool | 'dimension' | 'trim' | 'extend' | 'fillet' | 'chamfer' | 'regions';

const DRAW_TOOLS = Object.keys(DRAW_TOOL_LABELS) as DrawTool[];
const isDrawTool = (t: EditorTool): t is DrawTool => (DRAW_TOOLS as string[]).includes(t);

const EDIT_TOOL_LABELS: Record<string, string> = {
  select: 'Выбор',
  dimension: 'Размер',
  trim: 'Обрезать',
  extend: 'Удлинить',
  fillet: 'Скругление угла',
  chamfer: 'Фаска угла',
  regions: 'Области',
};

const EDIT_TOOL_HINTS: Record<string, string> = {
  select: 'Щелчок выбирает точку или кривую (Shift — добавить); точку можно перетащить.',
  dimension: 'Щёлкните элемент (или два), затем место текста размера.',
  trim: 'Щёлкните участок кривой, который нужно удалить.',
  extend: 'Щёлкните кривую у конца, который нужно продлить.',
  fillet: 'Щёлкните вершину угла двух отрезков.',
  chamfer: 'Щёлкните вершину угла двух отрезков.',
  regions: 'Щёлкните внутри области, чтобы включить или исключить её из профиля.',
};

/**
 * Dimension text offsets by "<document handle>:<sketch id>": the part file has no place for
 * them, so they live as long as the page.
 */
const TEXT_OFFSETS = new Map<string, Record<number, { x: number; y: number }>>();

/** Pixel radius of snapping and picking. */
const PICK_PIXELS = 8;

export interface SketchEditorState {
  featureId: string;
  tool: EditorTool;
  definition: string;
  dof: number;
  converged: boolean;
  conflicting: number[];
  redundant: number[];
  constraints: { type: string; a: number; b: number; c: number; value: string; driven: boolean; measured: number; state: string }[];
  points: { x: number; y: number; fixed: boolean; defined: boolean }[];
  curves: { entity: number; type: string; construction: boolean; defined: boolean }[];
  regions: { index: number; area: number; selected: boolean }[] | null;
  profileIssues: string[];
  selection: string[];
  message: string | null;
  pendingDimension: boolean;
}

export interface SketchEditorTestApi {
  getState(): SketchEditorState;
  sketchToClient(x: number, y: number): { x: number; y: number };
}

declare global {
  interface Window {
    bendcoreSketchEditor?: SketchEditorTestApi;
  }
}

export interface SketchEditorProps {
  doc: PartDocumentHandle;
  featureId: string;
  /** Body the sketch is drawn on (shown behind the sketch). */
  body: KernelMesh | null;
  /** Leave the sketch keeping the data. */
  onExit: (data: SketchData) => void;
  /** Leave the sketch dropping the changes. */
  onCancel: () => void;
}

interface PendingDimension {
  /** data with the dimension as the last constraint (reference while pending) */
  data: SketchData;
  k: number;
  text: string;
  /** existing dimension being edited (then data is the current data) */
  existing: boolean;
  /** place of the text chosen by the click */
  at: { x: number; y: number } | null;
}

function errorText(e: unknown): string {
  if (e && typeof e === 'object' && 'detail' in e) return String((e as { detail: string }).detail);
  return e instanceof Error ? e.message : String(e);
}

const SketchEditor: React.FC<SketchEditorProps> = ({ doc, featureId, body, onExit, onCancel }) => {
  const [data, setData] = useState<SketchData>(emptySketch);
  const [solution, setSolution] = useState<SketchSolution | null>(null);
  const [history, setHistory] = useState<SketchData[]>([]);
  const [tool, setToolState] = useState<EditorTool>('select');
  const [picks, setPicks] = useState<Snap[]>([]);
  const [hover, setHover] = useState<Snap | null>(null);
  const [preview, setPreview] = useState<SketchSolvedCurve[]>([]);
  const [selection, setSelection] = useState<SketchRef[]>([]);
  const [selectedConstraints, setSelectedConstraints] = useState<number[]>([]);
  const [dimPicks, setDimPicks] = useState<SketchRef[]>([]);
  const [pending, setPending] = useState<PendingDimension | null>(null);
  // Text offsets of the dimensions, kept for the page session per document and sketch.
  const offsetKey = `${doc.handle}:${featureId}`;
  const [offsets, setOffsetsState] = useState<Record<number, { x: number; y: number }>>(() => TEXT_OFFSETS.get(offsetKey) ?? {});
  const setOffsets = (next: Record<number, { x: number; y: number }> | ((o: Record<number, { x: number; y: number }>) => Record<number, { x: number; y: number }>)) =>
    setOffsetsState((o) => {
      const value = typeof next === 'function' ? next(o) : next;
      TEXT_OFFSETS.set(offsetKey, value);
      return value;
    });
  const [message, setMessage] = useState<string | null>(null);
  const [construction, setConstruction] = useState(false);
  const [gridSnap, setGridSnap] = useState(false);
  const [reference, setReference] = useState(false);
  const [params, setParams] = useState({
    filletRadius: '5',
    chamfer1: '5',
    chamfer2: '5',
    offset: '5',
    stepX: '20',
    stepY: '0',
    count: '3',
    angle: '90',
  });
  const [unitsPerPixel, setUnitsPerPixel] = useState(0.2);

  const canvasRef = useRef<SketchCanvasHandle>(null);
  const dataRef = useRef(data);
  dataRef.current = data;
  const solutionRef = useRef(solution);
  solutionRef.current = solution;
  const dragRef = useRef<{ point: number; active: boolean; last: SketchSolution | null } | null>(null);

  /** Solves `next` and makes it the current data (free points move to the solution). */
  const commit = useCallback(
    (next: SketchData, record = true): SketchSolution | null => {
      try {
        const sol = doc.solveSketch(featureId, next);
        // The updater runs at the next render, after dataRef moves on: capture the data now.
        const previous = dataRef.current;
        if (record) setHistory((h) => [...h, previous]);
        dataRef.current = sol.sketch;
        solutionRef.current = sol;
        setData(sol.sketch);
        setSolution(sol);
        setMessage(null);
        return sol;
      } catch (e) {
        setMessage(errorText(e));
        return null;
      }
    },
    [doc, featureId],
  );

  // Enter: the sketch data of the node.
  useEffect(() => {
    let initial: SketchData;
    try {
      initial = doc.feature(featureId).sketch ?? emptySketch();
    } catch (e) {
      setMessage(errorText(e));
      return;
    }
    dataRef.current = initial;
    setData(initial);
    setHistory([]);
    commit(initial, false);
  }, [doc, featureId, commit]);

  const setTool = (t: EditorTool) => {
    setToolState(t);
    setPicks([]);
    setPreview([]);
    setDimPicks([]);
    setPending(null);
    setMessage(null);
  };

  const marks = useMemo(() => highlights(data, solution), [data, solution]);

  // ------------------------------------------------------------------ edits

  const undoStep = () => {
    if (history.length === 0) return;
    const prev = history[history.length - 1];
    setHistory(history.slice(0, -1));
    setSelection([]);
    setSelectedConstraints([]);
    setOffsets({});
    commit(prev, false);
  };

  const deleteSelection = () => {
    if (selection.length === 0 && selectedConstraints.length === 0) return;
    const { data: next, constraintMap } = removeItems(data, selection, selectedConstraints);
    if (commit(next)) {
      const moved: Record<number, { x: number; y: number }> = {};
      for (const [k, o] of Object.entries(offsets)) {
        const nk = constraintMap[Number(k)];
        if (nk > 0) moved[nk] = o;
      }
      setOffsets(moved);
      setSelection([]);
      setSelectedConstraints([]);
    }
  };

  /** Runs a kernel editing tool on the current data. */
  const runTool = (spec: SketchTool): boolean => {
    try {
      const out = doc.editSketch(featureId, dataRef.current, spec);
      // Constraint numbers may change (removed constraints): text offsets are dropped.
      setOffsets({});
      setSelection([]);
      setSelectedConstraints([]);
      return commit(out.sketch) !== null;
    } catch (e) {
      setMessage(errorText(e));
      return false;
    }
  };

  const numberParam = (key: keyof typeof params, what: string): number | null => {
    const v = Number(params[key]);
    if (!Number.isFinite(v)) {
      setMessage(`${what}: нужно число`);
      return null;
    }
    return v;
  };

  /** Adds a dimension as a reference one, lets the kernel measure it and asks for the value. */
  const startDimension = (p: ConstraintProposal, at: { x: number; y: number } | null) => {
    const next = cloneSketch(dataRef.current);
    const k = addConstraint(next, p.type, p.a, p.b, p.c, '0', true);
    try {
      const sol = doc.solveSketch(featureId, next, { lite: true });
      setPending({ data: next, k, text: formatValue(sol.constraintValues[k - 1]), existing: false, at });
      setMessage(null);
    } catch (e) {
      setMessage(errorText(e));
    }
  };

  const confirmDimension = () => {
    if (!pending) return;
    const next = cloneSketch(pending.data);
    const c = next.constraints[pending.k - 1];
    const makeReference = pending.existing ? Boolean(c.driven) : reference;
    if (!makeReference) {
      if (!pending.text.trim()) {
        setMessage('введите значение размера');
        return;
      }
      c.value = pending.text.trim();
      delete c.driven;
    }
    const sol = commit(next);
    if (!sol) return;
    if (pending.at && !pending.existing) {
      const g = dimensionGraphic(sol.sketch, sol, pending.k, 24 * unitsPerPixel);
      if (g) setOffsets((o) => ({ ...o, [pending.k]: offsetFor(g, pending.at!) }));
    }
    setPending(null);
    setDimPicks([]);
  };

  const applyConstraint = (p: ConstraintProposal) => {
    if (DIMENSION_TYPES.has(p.type)) {
      startDimension(p, null);
      return;
    }
    const next = cloneSketch(data);
    let value = '0';
    if (p.type === 'rotation' && solution) {
      // Angle from (a - c) to (b - c), the current rotation of a onto b.
      const [a, b, c] = [solution.points[p.a], solution.points[p.b], solution.points[p.c]];
      const deg = ((Math.atan2(b.y - c.y, b.x - c.x) - Math.atan2(a.y - c.y, a.x - c.x)) * 180) / Math.PI;
      value = formatValue(deg);
    }
    addConstraint(next, p.type, p.a, p.b, p.c, value);
    if (commit(next)) setSelection([]);
  };

  const toggleDriven = (k: number) => {
    const next = cloneSketch(data);
    const c = next.constraints[k - 1];
    if (c.driven) {
      delete c.driven;
      // A reference dimension becomes driving at its measured value.
      c.value = formatValue(solution?.constraintValues[k - 1] ?? Number(c.value));
    } else {
      c.driven = true;
    }
    commit(next);
  };

  const toggleRegion = (x: number, y: number) => {
    const regions = solution?.profile?.regions ?? [];
    // Innermost region whose kernel triangulation contains the point.
    let hit: (typeof regions)[number] | null = null;
    for (const r of regions) {
      for (let t = 0; t + 2 < r.indices.length; t += 3) {
        const [a, b, c] = [0, 1, 2].map((i) => ({ x: r.positions[r.indices[t + i] * 3], y: r.positions[r.indices[t + i] * 3 + 1] }));
        const s1 = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
        const s2 = (c.x - b.x) * (y - b.y) - (c.y - b.y) * (x - b.x);
        const s3 = (a.x - c.x) * (y - c.y) - (a.y - c.y) * (x - c.x);
        if ((s1 >= 0 && s2 >= 0 && s3 >= 0) || (s1 <= 0 && s2 <= 0 && s3 <= 0)) {
          if (!hit || r.depth > hit.depth) hit = r;
          break;
        }
      }
    }
    if (!hit) return;
    const chosen = regions.filter((r) => (r === hit ? !r.selected : r.selected));
    if (chosen.some((r) => r.selector === null)) {
      setMessage('эту область нельзя выбрать отдельно: у неё нет собственной кривой внешнего контура');
      return;
    }
    if (chosen.length === 0) {
      setMessage('в профиле должна остаться хотя бы одна область');
      return;
    }
    commit({ ...cloneSketch(data), regions: chosen.map((r) => r.selector as number) });
  };

  // ------------------------------------------------------------------ pointer

  const hitAt = (e: SketchPointerEvent): SketchRef | null => {
    const sol = solutionRef.current;
    if (!sol) return null;
    const tol = PICK_PIXELS * e.unitsPerPixel;
    const p = nearestPoint(sol, e.x, e.y, tol);
    if (p) return pointRef(p.point);
    const c = nearestEntity(sol, e.x, e.y, tol);
    return c ? entityRef(c.entity) : null;
  };

  const snapAt = (e: SketchPointerEvent): Snap =>
    findSnap(e, dataRef.current, solutionRef.current, {
      tolerance: PICK_PIXELS * e.unitsPerPixel,
      reference: picks.length > 0 ? picks[picks.length - 1] : null,
      grid: gridSnap ? 1 : 0,
    });

  const positions = () => (solutionRef.current?.points ?? []).map((p) => ({ x: p.x, y: p.y }));

  const finishShape = (all: Snap[]) => {
    if (!isDrawTool(tool)) return;
    try {
      const shape = drawShape(dataRef.current, tool, all, positions(), { construction });
      if (commit(shape.data)) setPicks(shape.next ? [shape.next] : []);
      else setPicks([]);
    } catch (e) {
      if (!(e instanceof DrawError)) throw e;
      setMessage(e.message);
      setPicks([]);
    }
    setPreview([]);
  };

  const onDown = (e: SketchPointerEvent) => {
    setUnitsPerPixel(e.unitsPerPixel);
    if (tool !== 'select' || pending) return;
    const hit = hitAt(e);
    dragRef.current =
      hit && hit.kind === 'point' && !solutionRef.current?.points[hit.index]?.fixed
        ? { point: hit.index, active: false, last: null }
        : null;
  };

  const onMove = (e: SketchPointerEvent) => {
    const drag = dragRef.current;
    if (drag) {
      // Drag: the kernel pulls the point and re-solves; the data is committed on release.
      try {
        const sol = doc.solveSketch(featureId, dataRef.current, { drag: { point: drag.point, x: e.x, y: e.y }, lite: true });
        drag.active = true;
        drag.last = sol;
        solutionRef.current = { ...sol, profile: solutionRef.current?.profile ?? null, intersections: [] };
        setSolution(solutionRef.current);
      } catch (err) {
        setMessage(errorText(err));
      }
      return;
    }
    if (!isDrawTool(tool)) {
      if (hover) setHover(null);
      return;
    }
    const snap = snapAt(e);
    setHover(snap);
    const all = [...picks, snap];
    const enough = tool === 'spline' ? all.length >= 2 : all.length === PICK_COUNT[tool] && picks.length > 0;
    if (!enough) {
      if (preview.length > 0) setPreview([]);
      return;
    }
    try {
      const shape = drawShape(dataRef.current, tool, all, positions(), { construction });
      const sol = doc.solveSketch(featureId, shape.data, { lite: true });
      setPreview(shape.created.map((id) => sol.curves[id - 1]).filter(Boolean));
    } catch {
      // A degenerate tentative shape (cursor on a pick) has no preview.
      setPreview([]);
    }
  };

  const onUp = (e: SketchPointerEvent, moved: boolean) => {
    const drag = dragRef.current;
    dragRef.current = null;
    if (drag?.active && drag.last) {
      commit(drag.last.sketch);
      return;
    }
    if (pending) return;
    if (isDrawTool(tool)) {
      if (moved) return;
      const all = [...picks, snapAt(e)];
      if (all.length >= PICK_COUNT[tool]) finishShape(all);
      else setPicks(all);
      return;
    }
    if (moved) return;
    const hit = hitAt(e);
    switch (tool) {
      case 'select': {
        setSelectedConstraints([]);
        if (!hit) {
          if (!e.shift && !e.ctrl) setSelection([]);
          return;
        }
        const key = refKey(hit);
        const has = selection.some((r) => refKey(r) === key);
        if (e.shift || e.ctrl) setSelection(has ? selection.filter((r) => refKey(r) !== key) : [...selection, hit]);
        else setSelection(has && selection.length === 1 ? [] : [hit]);
        return;
      }
      case 'dimension': {
        if (hit && dimPicks.length < 2 && !dimPicks.some((r) => refKey(r) === refKey(hit))) {
          setDimPicks([...dimPicks, hit]);
          return;
        }
        if (dimPicks.length === 0) return;
        const options = applicableConstraints(dataRef.current, dimPicks).filter((p) => DIMENSION_TYPES.has(p.type));
        let choice = options[0];
        const pts = dimPicks.filter((r) => r.kind === 'point');
        const sol = solutionRef.current;
        if (pts.length === 2 && dimPicks.length === 2 && sol) {
          const [a, b] = pts.map((r) => sol.points[(r as { index: number }).index]);
          const type = twoPointDimension(a, b, e, PICK_PIXELS * e.unitsPerPixel);
          choice = options.find((p) => p.type === type) ?? choice;
        }
        // A circle is dimensioned by its diameter, an arc by its radius (as in SolidWorks / КОМПАС).
        const only = dimPicks.length === 1 && dimPicks[0].kind === 'entity' ? dataRef.current.geometry[dimPicks[0].id - 1] : null;
        if (only?.type === 'circle') choice = options.find((p) => p.type === 'diameter') ?? choice;
        if (!choice) {
          setMessage('для выбранных элементов нет размера');
          setDimPicks([]);
          return;
        }
        startDimension(choice, { x: e.x, y: e.y });
        return;
      }
      case 'trim':
      case 'extend':
        if (hit?.kind === 'entity') runTool({ name: tool, entity: hit.id, pick: [e.x, e.y] });
        return;
      case 'fillet':
      case 'chamfer': {
        if (hit?.kind !== 'point') return;
        const lines = cornerLines(dataRef.current, hit.index);
        if (!lines) {
          setMessage('в этой точке не сходятся два отрезка');
          return;
        }
        if (tool === 'fillet') {
          const r = numberParam('filletRadius', 'радиус скругления');
          if (r !== null) runTool({ name: 'fillet', a: lines[0], b: lines[1], radius: r });
        } else {
          const d1 = numberParam('chamfer1', 'катет фаски');
          const d2 = numberParam('chamfer2', 'катет фаски');
          if (d1 !== null && d2 !== null) runTool({ name: 'chamfer', a: lines[0], b: lines[1], d1, d2 });
        }
        return;
      }
      case 'regions':
        toggleRegion(e.x, e.y);
        return;
    }
  };

  const finishChain = () => {
    if (tool === 'spline' && picks.length >= 2) finishShape(picks);
    else {
      setPicks([]);
      setPreview([]);
    }
  };

  const keyHandler = useRef<(e: KeyboardEvent) => void>(() => undefined);
  keyHandler.current = (e: KeyboardEvent) => {
    const target = e.target as HTMLElement | null;
    const typing = target && ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName);
    if (e.key === 'Escape') {
      if (pending) setPending(null);
      else if (picks.length > 0 || dimPicks.length > 0) {
        setPicks([]);
        setPreview([]);
        setDimPicks([]);
      } else setTool('select');
      return;
    }
    if (typing) return;
    if (e.key === 'Enter') finishChain();
    else if (e.key === 'Delete' || e.key === 'Backspace') deleteSelection();
    else if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z') {
      e.preventDefault();
      undoStep();
    }
  };
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => keyHandler.current(e);
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  // ------------------------------------------------------------------ derived view data

  const shown = pending && !pending.existing ? pending.data : data;
  const dimensions: DimensionGraphic[] = useMemo(() => {
    if (!solution) return [];
    const out: DimensionGraphic[] = [];
    data.constraints.forEach((c, i) => {
      if (!DIMENSION_TYPES.has(c.type)) return;
      const g = dimensionGraphic(data, solution, i + 1, 24 * unitsPerPixel, offsets[i + 1]);
      if (g) out.push(g);
    });
    return out;
  }, [data, solution, offsets, unitsPerPixel]);

  const selectedEntities = useMemo(() => {
    const s = new Set<number>();
    for (const r of [...selection, ...dimPicks]) if (r.kind === 'entity') s.add(r.id);
    return s;
  }, [selection, dimPicks]);
  const selectedPoints = useMemo(() => {
    const s = new Set<number>();
    for (const r of [...selection, ...dimPicks]) if (r.kind === 'point') s.add(r.index);
    return s;
  }, [selection, dimPicks]);

  const proposals = tool === 'select' ? applicableConstraints(data, selection) : [];
  const selEntities = selection.filter((r) => r.kind === 'entity').map((r) => (r as { id: number }).id);
  const selPoints = selection.filter((r) => r.kind === 'point').map((r) => (r as { index: number }).index);
  const definition = solution?.definition ?? 'under-defined';
  const profile = solution?.profile ?? null;

  // ------------------------------------------------------------------ test API

  const stateRef = useRef<SketchEditorState | null>(null);
  stateRef.current = {
    featureId,
    tool,
    definition,
    dof: solution?.dof ?? -1,
    converged: solution?.converged ?? false,
    conflicting: solution?.conflicting ?? [],
    redundant: solution?.redundant ?? [],
    constraints: data.constraints.map((c, i) => ({
      type: c.type,
      a: c.a,
      b: c.b,
      c: c.c,
      value: c.value,
      driven: Boolean(c.driven),
      measured: solution?.constraintValues[i] ?? NaN,
      state: constraintState(i + 1, marks),
    })),
    points: solution?.points ?? [],
    curves: (solution?.curves ?? []).map((c) => ({ entity: c.entity, type: c.type, construction: c.construction, defined: c.defined })),
    regions: profile ? profile.regions.map((r) => ({ index: r.index, area: r.area, selected: r.selected })) : null,
    profileIssues: profile ? profile.issues.map((i) => i.kind) : [],
    selection: selection.map(refKey),
    message,
    pendingDimension: pending !== null,
  };
  useEffect(() => {
    window.bendcoreSketchEditor = {
      getState: () => stateRef.current!,
      sketchToClient: (x, y) => {
        if (!canvasRef.current) throw new Error('sketch canvas is not mounted');
        return canvasRef.current.sketchToClient(x, y);
      },
    };
    return () => {
      delete window.bendcoreSketchEditor;
    };
  }, []);

  // ------------------------------------------------------------------ render

  const hint = isDrawTool(tool)
    ? `${DRAW_TOOL_LABELS[tool]}: ${PICK_HINTS[tool][Math.min(picks.length, PICK_HINTS[tool].length - 1)]}`
    : EDIT_TOOL_HINTS[tool];
  const pendingGraphic =
    pending && solution && pending.existing ? dimensions.find((g) => g.constraint === pending.k) ?? null : null;
  const inputAt = (() => {
    if (!pending || !canvasRef.current) return null;
    const p = pending.at ?? pendingGraphic?.text ?? null;
    return p ? canvasRef.current.sketchToClient(p.x, p.y) : null;
  })();
  const paramField = (key: keyof typeof params, text: string) => (
    <label className="sk-param">
      <span>{text}</span>
      <input value={params[key]} data-testid={`sketch-param-${key}`} onChange={(e) => setParams({ ...params, [key]: e.target.value })} />
    </label>
  );
  const toolButton = (t: EditorTool, text: string) => (
    <button
      type="button"
      key={t}
      className={tool === t ? 'sk-active' : ''}
      data-testid={`tool-${t}`}
      aria-pressed={tool === t}
      onClick={() => setTool(t)}
    >
      {text}
    </button>
  );
  const lastLine = [...selEntities].reverse().find((id) => data.geometry[id - 1]?.type === 'line');

  return (
    <>
      <SketchCanvas
        ref={canvasRef}
        solution={solution}
        preview={preview}
        placement={solution?.placement ?? null}
        body={body}
        dimensions={dimensions}
        curveState={(id) => definitionState(entityRef(id), solution, marks)}
        pointState={(i) => definitionState(pointRef(i), solution, marks)}
        constraintClass={(k) => `sk-c-${constraintState(k, marks)}${data.constraints[k - 1]?.driven ? ' sk-driven' : ''}`}
        selectedEntities={selectedEntities}
        selectedPoints={selectedPoints}
        selectedConstraints={new Set(selectedConstraints)}
        snap={isDrawTool(tool) ? hover : null}
        showRegions={tool === 'regions' || tool === 'select'}
        cursor={tool === 'select' ? 'default' : 'crosshair'}
        onDown={onDown}
        onMove={onMove}
        onUp={onUp}
        onDoubleClick={() => {
          if (tool === 'line' || tool === 'spline') finishChain();
        }}
        onDimensionClick={(k) => {
          setSelection([]);
          setSelectedConstraints(selectedConstraints.includes(k) ? [] : [k]);
        }}
        onDimensionEdit={(k) => {
          const c = data.constraints[k - 1];
          if (!c) return;
          setPending({
            data,
            k,
            text: c.driven ? formatValue(solution?.constraintValues[k - 1] ?? 0) : c.value,
            existing: true,
            at: null,
          });
        }}
        onDimensionMove={(k, x, y) => {
          if (!solution) return;
          const g = dimensionGraphic(data, solution, k, 24 * unitsPerPixel);
          if (g) setOffsets((o) => ({ ...o, [k]: offsetFor(g, { x, y }) }));
        }}
      >
        <div className="sk-hint" data-testid="sketch-hint">
          {hint}
        </div>
        {pending && (
          <form
            className="sk-dim-input"
            style={inputAt ? { position: 'fixed', left: inputAt.x - 50, top: inputAt.y - 14 } : undefined}
            onSubmit={(e) => {
              e.preventDefault();
              confirmDimension();
            }}
          >
            <input
              autoFocus
              value={pending.text}
              data-testid="dim-input"
              disabled={pending.existing ? Boolean(shown.constraints[pending.k - 1]?.driven) : reference}
              aria-label={`${CONSTRAINT_LABELS[shown.constraints[pending.k - 1]?.type] ?? 'размер'}: выражение`}
              onChange={(e) => setPending({ ...pending, text: e.target.value })}
              onFocus={(e) => e.target.select()}
            />
            <button type="submit" data-testid="dim-ok">
              ОК
            </button>
          </form>
        )}
      </SketchCanvas>
      <aside className="pm-right kv-panel sk-panel" data-testid="sketch-panel">
        <h2>Эскиз {featureId}</h2>
        <div
          className={`sk-status sk-status-${definition}`}
          data-testid="sketch-status"
          data-definition={definition}
          data-dof={solution?.dof ?? ''}
        >
          {DEFINITION_LABELS[definition] ?? definition}
          {solution ? ` · степеней свободы: ${solution.dof}` : ''}
          {solution && !solution.converged ? ' · решатель не сошёлся' : ''}
        </div>
        {message && (
          <div className="sk-message" role="alert" data-testid="sketch-message">
            {message}
          </div>
        )}
        <div className="pm-panel-buttons">
          <button type="button" className="pm-primary" data-testid="sketch-exit" onClick={() => onExit(dataRef.current)}>
            Выйти из эскиза
          </button>
          <button type="button" data-testid="sketch-cancel" onClick={onCancel}>
            Отмена
          </button>
          <button type="button" data-testid="sketch-undo" disabled={history.length === 0} onClick={undoStep}>
            Шаг назад
          </button>
          <button type="button" data-testid="sketch-fit" onClick={() => canvasRef.current?.fit()}>
            Вписать
          </button>
        </div>

        <fieldset className="pm-group sk-tools">
          <legend>Рисование</legend>
          {toolButton('select', EDIT_TOOL_LABELS.select)}
          {DRAW_TOOLS.map((t) => toolButton(t, DRAW_TOOL_LABELS[t]))}
          <label className="sk-check">
            <input type="checkbox" checked={construction} data-testid="sketch-construction" onChange={(e) => setConstruction(e.target.checked)} />
            вспомогательная геометрия
          </label>
          <label className="sk-check">
            <input type="checkbox" checked={gridSnap} data-testid="sketch-grid-snap" onChange={(e) => setGridSnap(e.target.checked)} />
            привязка к сетке 1 мм
          </label>
        </fieldset>

        <fieldset className="pm-group sk-tools">
          <legend>Размеры и редактирование</legend>
          {(['dimension', 'trim', 'extend', 'fillet', 'chamfer', 'regions'] as EditorTool[]).map((t) => toolButton(t, EDIT_TOOL_LABELS[t]))}
          <label className="sk-check">
            <input type="checkbox" checked={reference} data-testid="sketch-reference" onChange={(e) => setReference(e.target.checked)} />
            новый размер — справочный
          </label>
          {tool === 'fillet' && paramField('filletRadius', 'Радиус, мм')}
          {tool === 'chamfer' && (
            <>
              {paramField('chamfer1', 'Катет 1, мм')}
              {paramField('chamfer2', 'Катет 2, мм')}
            </>
          )}
        </fieldset>

        {tool === 'select' && selection.length > 0 && (
          <fieldset className="pm-group sk-tools" data-testid="sketch-selection">
            <legend>
              Выбрано: {selPoints.length > 0 ? `точек ${selPoints.length}` : ''} {selEntities.length > 0 ? `кривых ${selEntities.length}` : ''}
            </legend>
            {proposals.map((p) => (
              <button type="button" key={p.type} data-testid={`constraint-${p.type}`} onClick={() => applyConstraint(p)}>
                {CONSTRAINT_LABELS[p.type] ?? p.type}
              </button>
            ))}
            {selEntities.length > 0 && (
              <button type="button" data-testid="sketch-toggle-construction" onClick={() => commit(toggleConstruction(data, selEntities))}>
                Вспомогательная ⇄ основная
              </button>
            )}
            <button type="button" data-testid="sketch-delete" onClick={deleteSelection}>
              Удалить
            </button>
            {selEntities.length > 0 && (
              <div className="sk-subtools">
                {paramField('offset', 'Эквидистанта, мм')}
                <button
                  type="button"
                  data-testid="sketch-offset"
                  onClick={() => {
                    const d = numberParam('offset', 'смещение');
                    if (d !== null) runTool({ name: 'offset', chain: selEntities, distance: d });
                  }}
                >
                  Эквидистанта
                </button>
                <button
                  type="button"
                  data-testid="sketch-mirror"
                  disabled={lastLine === undefined || selEntities.length < 2}
                  title="Ось — отрезок, выбранный последним"
                  onClick={() =>
                    lastLine !== undefined && runTool({ name: 'mirror', entities: selEntities.filter((id) => id !== lastLine), axis: lastLine })
                  }
                >
                  Зеркало (ось — последний отрезок)
                </button>
                {paramField('stepX', 'Шаг X, мм')}
                {paramField('stepY', 'Шаг Y, мм')}
                {paramField('count', 'Количество')}
                <button
                  type="button"
                  data-testid="sketch-linear-pattern"
                  onClick={() => {
                    const [dx, dy, n] = [numberParam('stepX', 'шаг X'), numberParam('stepY', 'шаг Y'), numberParam('count', 'количество')];
                    if (dx !== null && dy !== null && n !== null) runTool({ name: 'linear-pattern', entities: selEntities, step: [dx, dy], count: n });
                  }}
                >
                  Линейный массив
                </button>
                {paramField('angle', 'Шаг угла, °')}
                <button
                  type="button"
                  data-testid="sketch-circular-pattern"
                  disabled={selPoints.length !== 1}
                  title="Центр — выбранная точка"
                  onClick={() => {
                    const [n, a] = [numberParam('count', 'количество'), numberParam('angle', 'шаг угла')];
                    if (n !== null && a !== null) runTool({ name: 'circular-pattern', entities: selEntities, center: selPoints[0], count: n, angle: a });
                  }}
                >
                  Круговой массив (центр — точка)
                </button>
              </div>
            )}
          </fieldset>
        )}

        {(marks.conflicting.size > 0 || marks.redundant.size > 0) && (
          <section className="sk-conflicts" data-testid="sketch-conflicts">
            {marks.conflicting.size > 0 && <h2>Конфликтующие ограничения</h2>}
            <ul>
              {[...marks.conflicting].sort((a, b) => a - b).map((k) => (
                <li key={`c${k}`} className="sk-c-conflict" data-testid={`conflict-${k}`}>
                  <button type="button" onClick={() => setSelectedConstraints([k])}>
                    #{k} {describeConstraint(data.constraints[k - 1], solution?.constraintValues[k - 1])}
                  </button>
                </li>
              ))}
            </ul>
            {marks.redundant.size > 0 && <h2>Избыточные ограничения</h2>}
            <ul>
              {[...marks.redundant].sort((a, b) => a - b).map((k) => (
                <li key={`r${k}`} className="sk-c-redundant" data-testid={`redundant-${k}`}>
                  <button type="button" onClick={() => setSelectedConstraints([k])}>
                    #{k} {describeConstraint(data.constraints[k - 1], solution?.constraintValues[k - 1])}
                  </button>
                </li>
              ))}
            </ul>
          </section>
        )}

        <section data-testid="sketch-profile">
          <h2>Профиль</h2>
          {!profile && <p className="pm-hint">Профиль строится, когда эскиз решён без конфликтов.</p>}
          {profile && !profile.hasProfile && <p className="pm-hint">Нет кривых профиля.</p>}
          {profile?.issues.map((i, k) => (
            <p key={k} className="sk-issue">
              {i.kind}: кривые {i.entities.join(', ')} {i.detail}
            </p>
          ))}
          {profile?.error && <p className="sk-issue">{profile.error}</p>}
          {profile && profile.regions.length > 0 && (
            <p>
              Областей: {profile.regions.length}, в профиле: {profile.regions.filter((r) => r.selected).length} (площадь{' '}
              {formatValue(profile.regions.filter((r) => r.selected).reduce((s, r) => s + r.area, 0))} мм²)
            </p>
          )}
        </section>

        <section>
          <h2>Ограничения ({data.constraints.length})</h2>
          <ul className="sk-constraints" data-testid="sketch-constraints">
            {data.constraints.map((c, i) => {
              const k = i + 1;
              const st = constraintState(k, marks);
              return (
                <li
                  key={k}
                  className={`sk-c-${st}${selectedConstraints.includes(k) ? ' sk-selected' : ''}`}
                  data-testid={`constraint-row-${k}`}
                  data-state={st}
                  data-type={c.type}
                >
                  <button type="button" className="sk-c-name" onClick={() => setSelectedConstraints(selectedConstraints.includes(k) ? [] : [k])}>
                    #{k} {describeConstraint(c, solution?.constraintValues[i])}
                  </button>
                  {DIMENSION_TYPES.has(c.type) && (
                    <label title="Справочный размер: измеряется, не управляет эскизом">
                      <input type="checkbox" checked={Boolean(c.driven)} data-testid={`constraint-driven-${k}`} onChange={() => toggleDriven(k)} />
                      спр.
                    </label>
                  )}
                  {hasValue(c.type) && !c.driven && (
                    <input
                      className="sk-c-value"
                      defaultValue={c.value}
                      key={`${k}=${c.value}`}
                      data-testid={`constraint-value-${k}`}
                      onBlur={(e) => {
                        const v = e.target.value.trim();
                        if (!v || v === c.value) return;
                        const next = cloneSketch(data);
                        next.constraints[i].value = v;
                        commit(next);
                      }}
                      onKeyDown={(e) => {
                        if (e.key === 'Enter') (e.target as HTMLInputElement).blur();
                      }}
                    />
                  )}
                  <button
                    type="button"
                    aria-label={`удалить ограничение ${k}`}
                    data-testid={`constraint-remove-${k}`}
                    onClick={() => {
                      const { data: next, constraintMap } = removeItems(data, [], [k]);
                      if (commit(next)) {
                        const moved: Record<number, { x: number; y: number }> = {};
                        for (const [key, o] of Object.entries(offsets)) if (constraintMap[Number(key)] > 0) moved[constraintMap[Number(key)]] = o;
                        setOffsets(moved);
                        setSelectedConstraints([]);
                      }
                    }}
                  >
                    ×
                  </button>
                </li>
              );
            })}
          </ul>
        </section>
      </aside>
    </>
  );
};

export default SketchEditor;
