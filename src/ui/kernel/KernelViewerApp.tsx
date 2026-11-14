/**
 * Kernel viewer page: open a STEP or IGES file, read it with its XCAF structure and tessellate it
 * in bendcore_kernel_wasm, show it in KernelViewport, pick faces and show their kernel properties
 * (docs/ui/KERNEL_VIEWPORT.md). In the shell it is also the editor of an imported part — a body
 * without feature history: export to STEP/IGES/STL/3MF and mass properties
 * (docs/ui/DATA_EXCHANGE.md).
 *
 * window.bendcoreKernelViewer exposes the page state (read-only) and the view projection, so
 * that end-to-end tests check selection through application state rather than pixels.
 */

import React, { useCallback, useEffect, useRef, useState } from 'react';
import type {
  BoundingBox,
  FaceInfo,
  ImportStructure,
  Kernel,
  KernelMesh,
  KernelShape,
  SurfaceType,
  TopologyCounts,
} from '../../wasm/bendcore_kernel_api.mjs';
import KernelViewport from './KernelViewport';
import type { FacePick, KernelViewportHandle } from './KernelViewport';
import { defaultKernelBaseUrl, loadKernelFrom } from './kernelLoader';
import type { Remote, WasmClient } from '../worker/wasmClient';
import { waitIdle } from '../worker/idle';
import type { EditorHostProps } from '../app/editorHost';
import { useFaultReport } from '../app/faultReport';
import { caption, useI18n } from '../app/i18n';
import ExportDialog from '../exchange/ExportDialog';
import MassSection from '../exchange/MassSection';
import type { MassResult } from '../exchange/MassSection';
import { IMPORT_ACCEPT, importFormatOf, parseDensity } from '../exchange/exchange';
import type { ImportFormat } from '../exchange/exchange';
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
  /** Parts, names and warnings of the file (STEP/IGES through XCAF). */
  structure: ImportStructure;
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
  /** Resolves when no kernel call is in flight and the page has rendered the last result. */
  idle(): Promise<void>;
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


function parameterValue(key: string, value: unknown): string {
  if (Array.isArray(value)) return fmtVec(value as number[]);
  if (typeof value === 'number') return key === 'semiAngle' ? fmt((value * 180) / Math.PI, 4) : fmt(value, 4);
  return String(value);
}

/** A STEP/IGES file given by the shell ("Import STEP/IGES" on the start page). */
export interface ImportedFile {
  name: string;
  bytes: Uint8Array;
}

export interface KernelViewerProps extends Pick<EditorHostProps, 'onFault' | 'exportRequest'> {
  /** File opened at start instead of an empty viewer. */
  importFile?: ImportedFile | null;
  /**
   * The opened file is an assembly (several parts or an assembly root): the shell opens it in the
   * assembly editor instead. Without this callback the viewer shows the assembly as one model.
   */
  onAssembly?: (file: { name: string; format: ImportFormat; text: string; parts: number }) => void;
}

const KernelViewerApp: React.FC<KernelViewerProps> = ({ onFault, importFile = null, onAssembly, exportRequest }) => {
  const { t } = useI18n();
  const [kernel, setKernel] = useState<WasmClient<Kernel> | null>(null);
  useFaultReport(kernel as WasmClient<unknown> | null, onFault);
  const [kernelStatus, setKernelStatus] = useState<KernelViewerState['kernelStatus']>('loading');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<KernelViewerState['error']>(null);
  const [model, setModel] = useState<ModelSummary | null>(null);
  const [mesh, setMesh] = useState<KernelMesh | null>(null);
  const [selection, setSelection] = useState<Selection | null>(null);
  const [linearDeflection, setLinearDeflection] = useState('0.1');
  const [angularDeflection, setAngularDeflection] = useState('0.3');
  const [density, setDensity] = useState('7850');
  const [exporting, setExporting] = useState(false);
  const [notice, setNotice] = useState<string | null>(null);
  const [zipStored, setZipStored] = useState(false);
  const onAssemblyRef = useRef(onAssembly);
  onAssemblyRef.current = onAssembly;
  const shapeRef = useRef<Remote<KernelShape> | null>(null);
  const kernelRef = useRef(kernel);
  kernelRef.current = kernel;
  const viewRef = useRef<KernelViewportHandle | null>(null);
  const stateRef = useRef<KernelViewerState>({ kernelStatus, busy, error, model, selection });
  stateRef.current = { kernelStatus, busy, error, model, selection };

  useEffect(() => {
    let cancelled = false;
    loadKernelFrom(defaultKernelBaseUrl())
      .then(async (k) => {
        if (cancelled) return;
        setZipStored((await k.root.exchangeInfo()).zipMethod === 'stored');
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

  useEffect(() => () => kernel?.dispose(), [kernel]);

  useEffect(() => {
    window.bendcoreKernelViewer = {
      getState: () => stateRef.current,
      idle: () => waitIdle(() => [kernelRef.current]),
      worldToClient: (x, y, z) => {
        if (!viewRef.current) throw new Error('viewport is not mounted');
        return viewRef.current.worldToClient(x, y, z);
      },
    };
    return () => {
      delete window.bendcoreKernelViewer;
      shapeRef.current = null;
    };
  }, []);

  const tessellationParams = useCallback(() => {
    const linear = Number(linearDeflection);
    const angular = Number(angularDeflection);
    return { linearDeflection: linear, angularDeflection: angular };
  }, [linearDeflection, angularDeflection]);

  const openBytes = useCallback(
    async (fileName: string, bytes: Uint8Array) => {
      if (!kernel) return;
      const format = importFormatOf(fileName);
      if (!format) {
        setError({ code: 'InvalidInput', message: t('exchange.unknownFormat', { name: fileName }) });
        return;
      }
      setBusy(true);
      setError(null);
      setNotice(null);
      try {
        const { shape, structure } = await kernel.root.importModel(bytes, format);
        try {
          const counts = await shape.counts();
          const tessellated = await shape.tessellate(tessellationParams());
          const summary: ModelSummary = {
            fileName,
            counts,
            volume: counts.solids > 0 ? (await shape.volumeProperties()).volume : null,
            area: (await shape.surfaceProperties()).area,
            boundingBox: await shape.boundingBox(),
            faceTypes: await shape.faceTypes(),
            triangles: tessellated.indices.length / 3,
            structure,
          };
          void shapeRef.current?.dispose();
          shapeRef.current = shape;
          setModel(summary);
          setMesh(tessellated);
          setSelection(null);
          if (structure.warnings.length) setNotice(t('exchange.importWarnings', { list: structure.warnings.join('; ') }));
          if (structure.assembly && onAssemblyRef.current) {
            onAssemblyRef.current({ name: fileName, format, text: new TextDecoder().decode(bytes), parts: structure.parts });
          }
        } catch (e) {
          void shape.dispose();
          throw e;
        }
      } catch (e) {
        setError(describeError(e));
      } finally {
        setBusy(false);
      }
    },
    [kernel, tessellationParams, t],
  );

  const openFile = useCallback(async (file: File) => openBytes(file.name, new Uint8Array(await file.arrayBuffer())), [openBytes]);

  // The file of "Import STEP/IGES" opens once the module is ready.
  const importedRef = useRef(false);
  useEffect(() => {
    if (!kernel || !importFile || importedRef.current) return;
    importedRef.current = true;
    void openBytes(importFile.name, importFile.bytes);
  }, [kernel, importFile, openBytes]);

  // "Export…" of the shell.
  const exportSeen = useRef(exportRequest ?? 0);
  useEffect(() => {
    if (exportRequest === undefined || exportRequest === exportSeen.current) return;
    exportSeen.current = exportRequest;
    if (shapeRef.current) setExporting(true);
  }, [exportRequest]);

  const loadMass = useCallback(async (): Promise<MassResult> => {
    const shape = shapeRef.current;
    if (!shape || !model || model.counts.solids === 0) return { kind: 'empty', reason: 'noBody' };
    const rho = parseDensity(density);
    if (rho === null) return { kind: 'empty', reason: 'noDensity' };
    return { kind: 'ok', props: await shape.massProperties(rho) };
  }, [model, density]);

  const retessellate = useCallback(async () => {
    const shape = shapeRef.current;
    if (!shape) return;
    setError(null);
    try {
      const tessellated = await shape.tessellate(tessellationParams());
      setMesh(tessellated);
      setModel((m) => (m ? { ...m, triangles: tessellated.indices.length / 3 } : m));
    } catch (e) {
      setError(describeError(e));
    }
  }, [tessellationParams]);

  const onPick = useCallback(async (pick: FacePick | null) => {
    const shape = shapeRef.current;
    if (!pick || !shape) {
      setSelection(null);
      return;
    }
    try {
      setSelection({ face: await shape.face(pick.faceIndex), point: pick.point });
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
        <span className="kv-title">{t('viewer.title')}</span>
        <label className="kv-file">
          <input
            type="file"
            accept={IMPORT_ACCEPT}
            data-testid="step-input"
            disabled={kernelStatus !== 'ready' || busy}
            onChange={(e) => {
              const file = e.target.files?.[0];
              if (file) void openFile(file);
              e.target.value = '';
            }}
          />
          {t('viewer.open')}
        </label>
        <label className="kv-param">
          {t('viewer.deflection')}
          <input type="number" min="0" step="0.01" value={linearDeflection} onChange={(e) => setLinearDeflection(e.target.value)} />
        </label>
        <label className="kv-param">
          {t('viewer.angle')}
          <input type="number" min="0" step="0.05" value={angularDeflection} onChange={(e) => setAngularDeflection(e.target.value)} />
        </label>
        <button type="button" onClick={retessellate} disabled={!model || busy}>
          {t('viewer.retessellate')}
        </button>
        <button type="button" onClick={() => setExporting(true)} disabled={!model || busy} data-testid="viewer-export">
          {t('exchange.export')}
        </button>
        <span className="kv-status" data-testid="kernel-status">
          {kernelStatus === 'loading' && t('viewer.loading')}
          {kernelStatus === 'ready' && (busy ? t('viewer.reading') : t('viewer.ready'))}
          {kernelStatus === 'failed' && t('viewer.failed')}
        </span>
      </header>
      {error && (
        <div className="kv-error" role="alert" data-testid="kernel-error">
          {error.code}: {error.message}
        </div>
      )}
      {notice && (
        <div className="kv-notice" role="status" data-testid="kernel-notice">
          {notice}
        </div>
      )}
      {exporting && model && shapeRef.current && (
        <ExportDialog
          name={model.structure.name || model.fileName}
          zipStored={zipStored}
          onExport={(format, options) => shapeRef.current!.exportFile(format, options)}
          onSaved={(name, size) => setNotice(t('exchange.saved', { name, size }))}
          onClose={() => setExporting(false)}
        />
      )}
      <main className="kv-main">
        <KernelViewport mesh={mesh} selectedFace={selection?.face.index ?? null} onPick={onPick} onReady={onReady} />
        <aside className="kv-panel">
          <section>
            <h2>{t('viewer.model')}</h2>
            {model ? (
              <dl data-testid="model-summary">
                <dt>{t('viewer.file')}</dt>
                <dd>{model.fileName}</dd>
                <dt>{t('viewer.counts')}</dt>
                <dd>
                  {model.counts.solids} / {model.counts.faces} / {model.counts.edges}
                </dd>
                <dt>{t('viewer.parts')}</dt>
                <dd data-testid="model-parts">{model.structure.parts}</dd>
                <dt>{t('viewer.volume')}</dt>
                <dd data-testid="model-volume" data-value={model.volume ?? ''}>
                  {model.volume === null ? t('viewer.noSolids') : fmt(model.volume)}
                </dd>
                <dt>{t('viewer.area')}</dt>
                <dd>{fmt(model.area)}</dd>
                <dt>{t('viewer.box')}</dt>
                <dd>
                  {fmtVec(model.boundingBox.min)} – {fmtVec(model.boundingBox.max)}
                </dd>
                <dt>{t('viewer.triangles')}</dt>
                <dd>{model.triangles}</dd>
              </dl>
            ) : (
              <p className="kv-hint">{t('viewer.openHint')}</p>
            )}
          </section>
          {model && <MassSection load={loadMass} version={model} density={{ value: density, onChange: setDensity }} />}
          <section>
            <h2>{t('viewer.face')}</h2>
            {selection ? (
              <dl data-testid="face-panel">
                <dt>{t('viewer.index')}</dt>
                <dd data-testid="face-index">{selection.face.index}</dd>
                <dt>{t('viewer.surface')}</dt>
                <dd data-testid="face-type">{selection.face.surfaceType}</dd>
                <dt>{t('viewer.area')}</dt>
                <dd>{fmt(selection.face.area)}</dd>
                <dt>{t('viewer.orientation')}</dt>
                <dd>{selection.face.orientation}</dd>
                {Object.entries(selection.face.parameters).map(([key, value]) => (
                  <React.Fragment key={key}>
                    <dt>{caption(t, 'viewer.param.', key)}</dt>
                    <dd>{parameterValue(key, value)}</dd>
                  </React.Fragment>
                ))}
                <dt>{t('viewer.pickPoint')}</dt>
                <dd>{fmtVec(selection.point)}</dd>
              </dl>
            ) : (
              <p className="kv-hint">{model ? t('viewer.pickHint') : '—'}</p>
            )}
          </section>
        </aside>
      </main>
    </div>
  );
};

export default KernelViewerApp;
