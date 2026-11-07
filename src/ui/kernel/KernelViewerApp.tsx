/**
 * Kernel viewer page: open a STEP file, read and tessellate it in bendcore_kernel_wasm, show it
 * in KernelViewport, pick faces and show their kernel properties (docs/ui/KERNEL_VIEWPORT.md).
 *
 * window.bendcoreKernelViewer exposes the page state (read-only) and the view projection, so
 * that end-to-end tests check selection through application state rather than pixels.
 */

import React, { useCallback, useEffect, useRef, useState } from 'react';
import type {
  BoundingBox,
  FaceInfo,
  Kernel,
  KernelMesh,
  KernelShape,
  SurfaceType,
  TopologyCounts,
} from '../../wasm/bendcore_kernel_api.mjs';
import KernelViewport from './KernelViewport';
import type { FacePick, KernelViewportHandle } from './KernelViewport';
import { defaultKernelBaseUrl, loadKernelFrom } from './kernelLoader';
import './KernelViewer.css';

interface ModelSummary {
  fileName: string;
  counts: TopologyCounts;
  /** null when the shape has no solid (the kernel defines volume only for solids). */
  volume: number | null;
  area: number;
  boundingBox: BoundingBox;
  faceTypes: SurfaceType[];
  triangles: number;
}

interface Selection {
  face: FaceInfo;
  point: [number, number, number];
}

export interface KernelViewerState {
  kernelStatus: 'loading' | 'ready' | 'failed';
  busy: boolean;
  error: { code: string; message: string } | null;
  model: ModelSummary | null;
  selection: Selection | null;
}

export interface KernelViewerTestApi {
  getState(): KernelViewerState;
  worldToClient(x: number, y: number, z: number): { x: number; y: number };
}

declare global {
  interface Window {
    bendcoreKernelViewer?: KernelViewerTestApi;
  }
}

function describeError(e: unknown): { code: string; message: string } {
  if (e && typeof e === 'object' && 'code' in e && 'detail' in e) {
    const ke = e as { code: string; detail: string };
    return { code: ke.code, message: ke.detail };
  }
  return { code: 'Error', message: e instanceof Error ? e.message : String(e) };
}

const fmt = (v: number, digits = 3) => (Number.isFinite(v) ? Number(v.toFixed(digits)).toString() : String(v));
const fmtVec = (v: number[]) => `(${v.map((c) => fmt(c)).join(', ')})`;

const PARAMETER_LABELS: Record<string, string> = {
  origin: 'Точка плоскости',
  normal: 'Нормаль (наружу)',
  radius: 'Радиус, мм',
  center: 'Центр',
  referenceRadius: 'Радиус в опорной плоскости, мм',
  semiAngle: 'Полуугол, °',
  apex: 'Вершина',
  majorRadius: 'Большой радиус, мм',
  minorRadius: 'Малый радиус, мм',
  axisOrigin: 'Точка оси',
  axisDirection: 'Направление оси',
  uDegree: 'Степень U',
  vDegree: 'Степень V',
  uPoles: 'Полюсов U',
  vPoles: 'Полюсов V',
};

function parameterValue(key: string, value: unknown): string {
  if (Array.isArray(value)) return fmtVec(value as number[]);
  if (typeof value === 'number') return key === 'semiAngle' ? fmt((value * 180) / Math.PI, 4) : fmt(value, 4);
  return String(value);
}

const KernelViewerApp: React.FC = () => {
  const [kernel, setKernel] = useState<Kernel | null>(null);
  const [kernelStatus, setKernelStatus] = useState<KernelViewerState['kernelStatus']>('loading');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<KernelViewerState['error']>(null);
  const [model, setModel] = useState<ModelSummary | null>(null);
  const [mesh, setMesh] = useState<KernelMesh | null>(null);
  const [selection, setSelection] = useState<Selection | null>(null);
  const [linearDeflection, setLinearDeflection] = useState('0.1');
  const [angularDeflection, setAngularDeflection] = useState('0.3');
  const shapeRef = useRef<KernelShape | null>(null);
  const viewRef = useRef<KernelViewportHandle | null>(null);
  const stateRef = useRef<KernelViewerState>({ kernelStatus, busy, error, model, selection });
  stateRef.current = { kernelStatus, busy, error, model, selection };

  useEffect(() => {
    let cancelled = false;
    loadKernelFrom(defaultKernelBaseUrl())
      .then((k) => {
        if (cancelled) return;
        setKernel(k);
        setKernelStatus('ready');
      })
      .catch((e: unknown) => {
        if (cancelled) return;
        setKernelStatus('failed');
        setError(describeError(e));
      });
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    window.bendcoreKernelViewer = {
      getState: () => stateRef.current,
      worldToClient: (x, y, z) => {
        if (!viewRef.current) throw new Error('viewport is not mounted');
        return viewRef.current.worldToClient(x, y, z);
      },
    };
    return () => {
      delete window.bendcoreKernelViewer;
      shapeRef.current?.dispose();
      shapeRef.current = null;
    };
  }, []);

  const tessellationParams = useCallback(() => {
    const linear = Number(linearDeflection);
    const angular = Number(angularDeflection);
    return { linearDeflection: linear, angularDeflection: angular };
  }, [linearDeflection, angularDeflection]);

  const openFile = useCallback(
    async (file: File) => {
      if (!kernel) return;
      setBusy(true);
      setError(null);
      try {
        const bytes = new Uint8Array(await file.arrayBuffer());
        const shape = kernel.readStep(bytes);
        try {
          const counts = shape.counts();
          const tessellated = shape.tessellate(tessellationParams());
          const summary: ModelSummary = {
            fileName: file.name,
            counts,
            volume: counts.solids > 0 ? shape.volumeProperties().volume : null,
            area: shape.surfaceProperties().area,
            boundingBox: shape.boundingBox(),
            faceTypes: shape.faceTypes(),
            triangles: tessellated.indices.length / 3,
          };
          shapeRef.current?.dispose();
          shapeRef.current = shape;
          setModel(summary);
          setMesh(tessellated);
          setSelection(null);
        } catch (e) {
          shape.dispose();
          throw e;
        }
      } catch (e) {
        setError(describeError(e));
      } finally {
        setBusy(false);
      }
    },
    [kernel, tessellationParams],
  );

  const retessellate = useCallback(() => {
    const shape = shapeRef.current;
    if (!shape) return;
    setError(null);
    try {
      const tessellated = shape.tessellate(tessellationParams());
      setMesh(tessellated);
      setModel((m) => (m ? { ...m, triangles: tessellated.indices.length / 3 } : m));
    } catch (e) {
      setError(describeError(e));
    }
  }, [tessellationParams]);

  const onPick = useCallback((pick: FacePick | null) => {
    const shape = shapeRef.current;
    if (!pick || !shape) {
      setSelection(null);
      return;
    }
    try {
      setSelection({ face: shape.face(pick.faceIndex), point: pick.point });
    } catch (e) {
      setError(describeError(e));
    }
  }, []);

  const onReady = useCallback((handle: KernelViewportHandle | null) => {
    viewRef.current = handle;
  }, []);

  return (
    <div className="kv-root">
      <header className="kv-toolbar">
        <span className="kv-title">bendcore · ядро</span>
        <label className="kv-file">
          <input
            type="file"
            accept=".step,.stp,.STEP,.STP"
            data-testid="step-input"
            disabled={kernelStatus !== 'ready' || busy}
            onChange={(e) => {
              const file = e.target.files?.[0];
              if (file) void openFile(file);
              e.target.value = '';
            }}
          />
          Открыть STEP…
        </label>
        <label className="kv-param">
          Прогиб, мм
          <input type="number" min="0" step="0.01" value={linearDeflection} onChange={(e) => setLinearDeflection(e.target.value)} />
        </label>
        <label className="kv-param">
          Угол, рад
          <input type="number" min="0" step="0.05" value={angularDeflection} onChange={(e) => setAngularDeflection(e.target.value)} />
        </label>
        <button type="button" onClick={retessellate} disabled={!model || busy}>
          Перестроить сетку
        </button>
        <span className="kv-status" data-testid="kernel-status">
          {kernelStatus === 'loading' && 'Загрузка ядра…'}
          {kernelStatus === 'ready' && (busy ? 'Чтение…' : 'Ядро готово')}
          {kernelStatus === 'failed' && 'Ядро не загружено'}
        </span>
      </header>
      {error && (
        <div className="kv-error" role="alert" data-testid="kernel-error">
          {error.code}: {error.message}
        </div>
      )}
      <main className="kv-main">
        <KernelViewport mesh={mesh} selectedFace={selection?.face.index ?? null} onPick={onPick} onReady={onReady} />
        <aside className="kv-panel">
          <section>
            <h2>Модель</h2>
            {model ? (
              <dl data-testid="model-summary">
                <dt>Файл</dt>
                <dd>{model.fileName}</dd>
                <dt>Тела / грани / рёбра</dt>
                <dd>
                  {model.counts.solids} / {model.counts.faces} / {model.counts.edges}
                </dd>
                <dt>Объём, мм³</dt>
                <dd>{model.volume === null ? 'нет тел' : fmt(model.volume)}</dd>
                <dt>Площадь, мм²</dt>
                <dd>{fmt(model.area)}</dd>
                <dt>Габарит, мм</dt>
                <dd>
                  {fmtVec(model.boundingBox.min)} – {fmtVec(model.boundingBox.max)}
                </dd>
                <dt>Треугольников</dt>
                <dd>{model.triangles}</dd>
              </dl>
            ) : (
              <p className="kv-hint">Откройте STEP-файл.</p>
            )}
          </section>
          <section>
            <h2>Грань</h2>
            {selection ? (
              <dl data-testid="face-panel">
                <dt>Индекс</dt>
                <dd data-testid="face-index">{selection.face.index}</dd>
                <dt>Поверхность</dt>
                <dd data-testid="face-type">{selection.face.surfaceType}</dd>
                <dt>Площадь, мм²</dt>
                <dd>{fmt(selection.face.area)}</dd>
                <dt>Ориентация</dt>
                <dd>{selection.face.orientation}</dd>
                {Object.entries(selection.face.parameters).map(([key, value]) => (
                  <React.Fragment key={key}>
                    <dt>{PARAMETER_LABELS[key] ?? key}</dt>
                    <dd>{parameterValue(key, value)}</dd>
                  </React.Fragment>
                ))}
                <dt>Точка выбора</dt>
                <dd>{fmtVec(selection.point)}</dd>
              </dl>
            ) : (
              <p className="kv-hint">{model ? 'Щёлкните по грани.' : '—'}</p>
            )}
          </section>
        </aside>
      </main>
    </div>
  );
};

export default KernelViewerApp;
