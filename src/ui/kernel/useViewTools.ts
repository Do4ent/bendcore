// State of the viewport tools of a page (docs/ui/KERNEL_VIEWPORT.md, "Измерения и сечение"):
// the measurement being collected and its kernel result, the section plane with its contour and
// area. The page supplies the kernel calls; the hook returns the props of the panels and of the
// viewport. A change of `modelKey` (the model was rebuilt) drops the picks and recomputes the section.

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { DEFAULT_SECTION, addPick, clipEquation, formatMeasure, measureOverlay, measureSpec, sectionPlane } from './viewTools';
import type { Box3, MeasurePick, MeasureResult, MeasureSpecOut, MeasureType, SectionPlaneOut, SectionState, Vec3 } from './viewTools';
import type { MeasurePanelProps, SectionPanelProps } from './ViewToolsPanel';

export interface SectionOutcome {
  area: number;
  polylines: Float32Array[];
}

export interface ViewToolsOptions {
  /** Kernel measurement; `path` is present in the spec when `withPath` is set (assemblies). */
  measure(spec: MeasureSpecOut): Promise<MeasureResult>;
  /** Kernel section; omitted by pages without a section tool. */
  section?(plane: Pick<SectionPlaneOut, 'origin' | 'normal'>): Promise<SectionOutcome>;
  withPath?: boolean;
  /** Bounding box of the model shown (section slider range); null without a model. */
  box: Box3 | null;
  /** Changes when the model changes. */
  modelKey: unknown;
  /** Delay of the section recomputation after the slider moves, ms. */
  sectionDelay?: number;
}

export interface ViewTools {
  /** Clicks in the viewport belong to the measurement. */
  capturing: boolean;
  pickMode: 'face' | 'edge';
  onPickFace(index: number | null, path?: number[]): void;
  onPickEdge(index: number | null, path?: number[]): void;
  /** A click on a reference mesh: triangle, hit point, vertex snapping distance (mm). */
  onPickMesh(mesh: string | number, triangle: number, point: Vec3, snap: number): void;
  /** Whether clicks on reference meshes belong to the measurement (a distance is being measured). */
  meshPickable: boolean;
  picks: readonly MeasurePick[];
  measurePanel: MeasurePanelProps;
  sectionPanel: SectionPanelProps | null;
  /** Props for KernelViewport. */
  viewport: {
    clip: { normal: Vec3; constant: number } | null;
    sectionLines: Float32Array[];
    measure: { lines: Vec3[][]; anchor: Vec3; label: string } | null;
  };
  /** For the page's test API. */
  state: {
    measureActive: boolean;
    measureType: MeasureType;
    picks: readonly MeasurePick[];
    result: MeasureResult | null;
    section: SectionState;
    sectionPosition: number | null;
    sectionArea: number | null;
  };
}

const NO_LINES: Float32Array[] = [];

function describe(e: unknown): string {
  if (e && typeof e === 'object' && 'detail' in e) return String((e as { detail: unknown }).detail);
  return e instanceof Error ? e.message : String(e);
}

export function useViewTools(options: ViewToolsOptions): ViewTools {
  const [active, setActive] = useState(false);
  const [type, setType] = useState<MeasureType>('distance');
  const [pickKind, setPickKind] = useState<'face' | 'edge'>('face');
  const [picks, setPicks] = useState<MeasurePick[]>([]);
  const [result, setResult] = useState<MeasureResult | null>(null);
  const [measureError, setMeasureError] = useState<string | null>(null);
  const [section, setSection] = useState<SectionState>(DEFAULT_SECTION);
  const [outcome, setOutcome] = useState<SectionOutcome | null>(null);
  const [sectionError, setSectionError] = useState<string | null>(null);
  const optionsRef = useRef(options);
  optionsRef.current = options;

  // The model changed: picks refer to old indices.
  useEffect(() => {
    setPicks([]);
    setResult(null);
    setMeasureError(null);
  }, [options.modelKey]);

  // Complete picks: ask the kernel.
  useEffect(() => {
    setResult(null);
    setMeasureError(null);
    const spec = measureSpec(type, picks, options.withPath === true);
    if (!active || !spec) return undefined;
    let stale = false;
    optionsRef.current.measure(spec).then(
      (r) => {
        if (!stale) setResult(r);
      },
      (e: unknown) => {
        if (!stale) setMeasureError(describe(e));
      },
    );
    return () => {
      stale = true;
    };
    // `options.measure` is read through the ref; picks and type are the inputs.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [active, type, picks]);

  const plane = section.enabled && options.box ? sectionPlane(section, options.box) : null;
  const planeKey = plane ? `${plane.origin.join(',')}|${plane.normal.join(',')}` : '';

  // The section contour and area, after the slider rests.
  useEffect(() => {
    setOutcome(null);
    setSectionError(null);
    const run = optionsRef.current.section;
    if (!plane || !run) return undefined;
    let stale = false;
    const timer = setTimeout(() => {
      run({ origin: plane.origin, normal: plane.normal }).then(
        (o) => {
          if (!stale) setOutcome(o);
        },
        (e: unknown) => {
          if (!stale) setSectionError(describe(e));
        },
      );
    }, optionsRef.current.sectionDelay ?? 150);
    return () => {
      stale = true;
      clearTimeout(timer);
    };
    // planeKey stands for the plane; modelKey for the model.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [planeKey, options.modelKey]);

  const pick = useCallback(
    (kind: 'face' | 'edge', index: number | null, path?: number[]) => {
      if (index === null) return;
      setPicks((current) => addPick(current, { kind, index, path }, type));
    },
    [type],
  );

  const pickMesh = useCallback(
    (mesh: string | number, triangle: number, point: Vec3, snap: number) => {
      setPicks((current) => addPick(current, { kind: 'mesh', index: triangle, mesh, point, snap }, type));
    },
    [type],
  );

  const viewport = useMemo(() => {
    const overlay = result ? measureOverlay(result) : null;
    return {
      clip: plane ? clipEquation(plane) : null,
      sectionLines: outcome ? outcome.polylines : NO_LINES,
      measure: result && overlay ? { ...overlay, label: formatMeasure(result) } : null,
    };
    // planeKey stands for the plane.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [planeKey, outcome, result]);
  const fixedKind = type === 'length' ? 'edge' : type === 'area' ? 'face' : null;

  return {
    capturing: active,
    pickMode: fixedKind ?? pickKind,
    onPickFace: (index, path) => pick('face', index, path),
    onPickEdge: (index, path) => pick('edge', index, path),
    onPickMesh: pickMesh,
    meshPickable: active && type === 'distance',
    picks,
    measurePanel: {
      active,
      type,
      picks,
      result,
      error: measureError,
      pickKind: fixedKind ?? pickKind,
      onActive: (on) => {
        setActive(on);
        setPicks([]);
      },
      onType: (next) => {
        setType(next);
        setPicks([]);
      },
      onPickKind: (kind) => {
        setPickKind(kind);
        setPicks([]);
      },
      onClear: () => setPicks([]),
    },
    sectionPanel: options.section
      ? {
          state: section,
          position: plane ? plane.position : null,
          area: outcome && outcome.area > 0 ? outcome.area : null,
          error: sectionError,
          disabled: options.box === null,
          onChange: setSection,
        }
      : null,
    viewport,
    state: {
      measureActive: active,
      measureType: type,
      picks,
      result,
      section,
      sectionPosition: plane ? plane.position : null,
      sectionArea: outcome && outcome.area > 0 ? outcome.area : null,
    },
  };
}
