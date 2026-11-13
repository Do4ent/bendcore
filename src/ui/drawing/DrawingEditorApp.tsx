/**
 * Drawing editor page (docs/ui/DRAWING_UI.md): the drawing document of bendcore_drawing_wasm made
 * from a part or assembly document — sheets with the title block, views (base, projected,
 * axonometric, section, detail), dimensions by picked 2D edges, roughness, form tolerances,
 * datums, technical requirements, update from the model with a report of changed and dangling
 * dimensions, the specification of an assembly, export to PDF/DXF/SVG and save/open.
 *
 * Every action is a call of the drawing kernel, executed in the WASM worker (docs/ui/APP_SHELL.md):
 * long ones (views with HLR, update from the model, PDF) show the progress indicator and can be
 * cancelled, then the drawing is reloaded from the snapshot taken after the last completed
 * change. The sheet on screen is the kernel's SVG of the
 * sheet and the pickable edges are the kernel's 2D view edges. window.bendcoreDrawingEditor
 * exposes the page state (read-only) for end-to-end tests.
 */

import React, { useCallback, useEffect, useRef, useState } from 'react';
import type {
  DrawingDescription,
  DrawingHandle,
  DrawingKernel,
  GeometricCharacteristic,
  PictureView,
  Point,
  SheetInfo,
  SheetPicture,
  SpecificationTable,
  StalenessReport,
  StandardView,
  UpdateReport,
  ViewSpec,
} from '../../wasm/bendcore_drawing_api.mjs';
import SheetCanvas, { SheetCanvasHandle } from './SheetCanvas';
import { defaultDrawingBaseUrl, loadDrawingFrom } from './drawingLoader';
import { CHARACTERISTICS, SCALES, SHEET_SIDES, STANDARD_VIEWS } from './labels';
import {
  EdgePick,
  PickState,
  Tool,
  addPick,
  balloonShelf,
  canDimensionSingle,
  downloadName,
  emptyPicks,
  sheetToViewPoint,
  viewPointToModel,
} from './sheetGeometry';
import type { Remote, WasmClient } from '../worker/wasmClient';
import { isCancelled } from '../worker/wasmClient';
import { waitIdle } from '../worker/idle';
import BusyIndicator from '../app/BusyIndicator';
import { useHotkeys } from '../app/hotkeys';
import { useI18n } from '../app/i18n';
import type { MessageKey } from '../app/i18n';
import type { DocumentFile, EditorHostProps } from '../app/editorHost';
import { useFaultReport } from '../app/faultReport';
import { DRAWING_FILE_EXTENSION, drawingFileName, packDrawingFile, readDrawingFiles, unpackDrawingFile } from './drawingFile';
import type { DrawingFileContent, DrawingFileError } from './drawingFile';
import './DrawingEditor.css';

type Variable = { name: string; expression: string; value: number | null; error: string | null };

export interface DrawingEditorState {
  status: 'loading' | 'ready' | 'failed';
  error: { code: string; message: string } | null;
  message: string;
  sheet: string | null;
  tool: Tool;
  description: DrawingDescription | null;
  staleness: StalenessReport | null;
  report: UpdateReport | null;
  picks: EdgePick[];
}

declare global {
  interface Window {
    bendcoreDrawingEditor?: DrawingEditorState;
    /** Resolves when no kernel call is in flight and the page has rendered the last result. */
    bendcoreDrawingEditorIdle?: () => Promise<void>;
  }
}

/** Scale of a list value "n:d" ('' — automatic). */
const scaleOf = (v: string): [number, number] | null => (v ? (v.split(':').map(Number) as [number, number]) : null);

const scaleText = ([n, d]: [number, number]) => (n === 5 && d === 2 ? '2,5:1' : n === 2 && d === 5 ? '1:2,5' : `${n}:${d}`);

function download(bytes: Uint8Array | string, name: string, type: string) {
  const blob = new Blob([typeof bytes === 'string' ? bytes : bytes.slice().buffer], { type });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = name;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function errorOf(e: unknown): { code: string; message: string } {
  if (e && typeof e === 'object' && 'code' in e && 'detail' in e) {
    return { code: String((e as { code: string }).code), message: String((e as { detail: string }).detail) };
  }
  return { code: 'Error', message: e instanceof Error ? e.message : String(e) };
}

const STANDARD_PROJECTED: StandardView[] = ['top', 'left', 'right', 'bottom', 'rear'];

const TOOLS: Tool[] = ['select', 'dimension', 'roughness', 'tolerance', 'datum', 'balloon', 'section', 'detail'];

const SPEC_COLUMNS = ['format', 'zone', 'position', 'designation', 'name', 'quantity', 'note'] as const;

type Drawing = Remote<DrawingHandle>;
type DrawingModule = Remote<DrawingKernel>;

/** Kind of a model document from its JSON ("bendcore-assembly" or a part). */
function modelKind(text: string): 'part' | 'assembly' {
  try {
    return JSON.parse(text).format === 'bendcore-assembly' ? 'assembly' : 'part';
  } catch {
    // The kernel reports the parse error.
    return 'part';
  }
}

/** A drawing made from the model `text` named `fileName`. */
function fromModel(k: DrawingModule, text: string, fileName: string): Promise<Drawing> {
  const base = fileName.replace(/(\.part|\.assembly|\.bcpart|\.bcasm)?\.json$/i, '');
  return modelKind(text) === 'assembly'
    ? k.createFromAssembly(text, { path: fileName })
    : k.createFromPart(text, { path: fileName, properties: { name: base } });
}

export default function DrawingEditorApp({
  active = true,
  initial = null,
  initialDirty = false,
  onChange,
  onFile,
  onFault,
}: EditorHostProps = {}): JSX.Element {
  const { t } = useI18n();
  const [kernel, setKernel] = useState<WasmClient<DrawingKernel> | null>(null);
  useFaultReport(kernel as WasmClient<unknown> | null, onFault);
  const kernelRef = useRef(kernel);
  kernelRef.current = kernel;
  /** Serialises drawing bodies: one runs at a time, in click order. */
  const queueRef = useRef<Promise<unknown>>(Promise.resolve());
  /** Drawing and model JSON after the last completed change (cancel restores them). */
  const snapshotRef = useRef<{ text: string; model: string; name: string; modelName: string } | null>(null);
  const savedRef = useRef<string | null>(null);
  const hostRef = useRef({ onChange, onFile });
  hostRef.current = { onChange, onFile };
  const [notice, setNotice] = useState<string | null>(null);
  const [status, setStatus] = useState<DrawingEditorState['status']>('loading');
  const [error, setError] = useState<DrawingEditorState['error']>(null);
  const [message, setMessage] = useState(() => t('drawing.loading'));
  const drawingRef = useRef<Drawing | null>(null);
  const [desc, setDesc] = useState<DrawingDescription | null>(null);
  const [picture, setPicture] = useState<SheetPicture | null>(null);
  const [sheetId, setSheetId] = useState<string | null>(null);
  const [tool, setTool] = useState<Tool>('select');
  const [pickState, setPickState] = useState<PickState>(emptyPicks);
  const [selectedView, setSelectedView] = useState<string | null>(null);
  const [variables, setVariables] = useState<Variable[]>([]);
  const [staleness, setStaleness] = useState<StalenessReport | null>(null);
  const [report, setReport] = useState<UpdateReport | null>(null);
  const [tab, setTab] = useState<'sheet' | 'specification'>('sheet');
  const [spec, setSpec] = useState<{ table: SpecificationTable | null; reason: string | null }>({ table: null, reason: null });
  const [modelName, setModelName] = useState('');
  const canvas = useRef<SheetCanvasHandle>(null);

  // Tool options.
  const [baseStandard, setBaseStandard] = useState<StandardView>('front');
  const [dimOffset, setDimOffset] = useState('10');
  const [rough, setRough] = useState({ method: 'materialRemoval', parameter: 'Ra', value: '3,2' });
  const [tol, setTol] = useState<{ characteristic: GeometricCharacteristic; tolerance: string; datums: string }>({
    characteristic: 'parallelism',
    tolerance: '0,02',
    datums: t('drawing.datumDefault'),
  });
  const [datumLetter, setDatumLetter] = useState(() => t('drawing.datumDefault'));
  const [detailRadius, setDetailRadius] = useState('10');
  const [sectionSide, setSectionSide] = useState('leftOf');

  useEffect(() => {
    let cancelled = false;
    let client: WasmClient<DrawingKernel> | null = null;
    loadDrawingFrom(defaultDrawingBaseUrl())
      .then((k) => {
        client = k;
        if (cancelled) {
          k.dispose();
          return;
        }
        setKernel(k);
        setStatus('ready');
        setMessage(t('drawing.ready'));
        if (initial) void openInitial(k.root, initial);
      })
      .catch((e) => {
        if (cancelled) return;
        setStatus('failed');
        setError(errorOf(e));
        setMessage(t('drawing.loadFailed'));
      });
    return () => {
      cancelled = true;
      drawingRef.current = null;
      client?.dispose();
    };
    // The module loads once per editor; `initial` is a mount-time choice.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const sheet: SheetInfo | null = desc?.sheets.find((s) => s.id === sheetId) ?? null;

  const refresh = useCallback(
    async (wantedSheet?: string | null) => {
      const d = drawingRef.current;
      if (!d) return;
      const description = await d.describe();
      const id =
        wantedSheet && description.sheets.some((s) => s.id === wantedSheet) ? wantedSheet : description.sheets[0]?.id ?? null;
      const pic = id ? await d.sheetPicture(id) : null;
      const vars = description.model.kind === 'part' ? await d.variables() : [];
      const stale = await d.checkModel();
      let specification: { table: SpecificationTable | null; reason: string | null };
      try {
        specification = { table: await d.specification(), reason: null };
      } catch (e) {
        if (isCancelled(e)) throw e;
        specification = { table: null, reason: errorOf(e).message };
      }
      if (drawingRef.current !== d) return;
      setDesc(description);
      setSheetId(id);
      setPicture(pic);
      setVariables(vars);
      setStaleness(stale);
      setSpec(specification);
    },
    [],
  );

  /** Snapshot of the drawing and its model after a completed change; reported as a draft. */
  const takeSnapshot = useCallback(async (d: Drawing) => {
    const [text, model] = await Promise.all([d.save(), d.saveModel()]);
    if (drawingRef.current !== d) return;
    const names = namesRef.current;
    snapshotRef.current = { text, model, name: names.drawing, modelName: names.model };
    const file = packDrawingFile({ drawing: text, model, modelName: names.model });
    hostRef.current.onChange?.({ kind: 'drawing', name: names.drawing, text: file }, text !== savedRef.current);
  }, []);
  /** File name of the drawing (.bcdrw.json) and of the model it was made from. */
  const namesRef = useRef({ drawing: '', model: '' });

  // Runs a kernel action; any failure is shown in the status line and the page is refreshed.
  const sheetIdRef = useRef(sheetId);
  sheetIdRef.current = sheetId;

  const run = useCallback(
    (label: string, action: (d: Drawing) => unknown, keepPicks = false): Promise<boolean> => {
      const task = queueRef.current.then(async () => {
        const d = drawingRef.current;
        if (!d) return false;
        let next = sheetIdRef.current;
        let ok = true;
        try {
          const result = (await action(d)) as { warnings?: string[]; switchTo?: string } | undefined;
          if (result && typeof result.switchTo === 'string') next = result.switchTo;
          setError(null);
          const warnings = result && Array.isArray(result.warnings) && result.warnings.length ? ` (${result.warnings.join('; ')})` : '';
          setMessage(label + warnings);
        } catch (e) {
          if (isCancelled(e) || drawingRef.current !== d) return false;
          setError(errorOf(e));
          setMessage(t('drawing.failedAction', { action: label }));
          ok = false;
        }
        if (!keepPicks) setPickState(emptyPicks);
        try {
          await refresh(next);
          if (ok) await takeSnapshot(d);
        } catch (e) {
          if (!isCancelled(e)) setError(errorOf(e));
        }
        return ok;
      });
      queueRef.current = task.catch(() => undefined);
      return task;
    },
    [refresh, takeSnapshot],
  );

  /**
   * Makes `make`'s drawing the current one; `saved` is the drawing document JSON of the file it was
   * opened from (null: not saved yet). `name` is the drawing file name, `model` the model's.
   */
  const openDrawing = async (
    make: (k: DrawingModule) => Promise<Drawing>,
    name: string,
    model: string,
    saved: string | null = null,
    k: DrawingModule | undefined = kernel?.root,
  ): Promise<boolean> => {
    if (!k) return false;
    try {
      const d = await make(k);
      const old = drawingRef.current;
      if (old) void old.dispose().catch(() => undefined);
      drawingRef.current = d;
      savedRef.current = saved;
      namesRef.current = { drawing: name, model };
      setModelName(model);
      setReport(null);
      setSelectedView(null);
      setPickState(emptyPicks);
      setTool('select');
      setError(null);
      setMessage(t('drawing.opened', { name }));
      await run(t('drawing.opened', { name }), () => undefined);
      return true;
    } catch (e) {
      if (isCancelled(e)) return false;
      setError(errorOf(e));
      setMessage(t('drawing.openFailed', { name }));
      return false;
    }
  };

  /** Opens drawing file content: the drawing on its embedded model, with the load report. */
  const openContent = async (content: DrawingFileContent, name: string, saved: boolean, k?: DrawingModule): Promise<boolean> => {
    const loaded: { report: UpdateReport | null } = { report: null };
    const ok = await openDrawing(
      async (m) => {
        const r = await m.load(content.drawing, content.model);
        loaded.report = r.report;
        return r.drawing;
      },
      name,
      content.modelName,
      saved ? content.drawing : null,
      k,
    );
    if (loaded.report) setReport(loaded.report);
    return ok;
  };

  const fileError = (e: DrawingFileError) =>
    setError({ code: 'InvalidInput', message: e.code === 'needsModel' ? t('drawing.file.needsModel') : e.detail });

  /** Document given by the shell: a model to draw, or a drawing file. */
  const openInitial = async (k: DrawingModule, file: DocumentFile) => {
    if (file.kind === 'drawing') {
      const r = unpackDrawingFile(file.text);
      if (!r.ok) {
        fileError(r.error);
        return;
      }
      const ok = await openContent(r.content, file.name, !initialDirty, k);
      if (ok && !initialDirty) hostRef.current.onFile?.(file);
    } else {
      // A new drawing of a part or an assembly document.
      await openDrawing((m) => fromModel(m, file.text, file.name), drawingFileName(file.name), file.name, null, k);
    }
  };

  const onOpenModel = async (files: FileList | null) => {
    if (!files || files.length === 0) return;
    const file = files[0];
    const text = await file.text();
    await openDrawing((k) => fromModel(k, text, file.name), drawingFileName(file.name), file.name);
  };

  /** A drawing file (.bcdrw.json), or an older drawing document chosen with its model file. */
  const onOpenDrawing = async (files: FileList | null) => {
    if (!files || files.length === 0) return;
    const texts = await Promise.all(Array.from(files).map(async (f) => ({ name: f.name, text: await f.text() })));
    const r = readDrawingFiles(texts);
    if (!r.ok) {
      fileError(r.error);
      return;
    }
    const ok = await openContent(r.content, r.name, !r.migrated);
    if (!ok) return;
    if (r.migrated) setMessage(t('drawing.file.migrated', { name: r.name }));
    else hostRef.current.onFile?.({ kind: 'drawing', name: r.name, text: packDrawingFile(r.content) });
  };

  /**
   * Replaces the model of the drawing by a newer file of it: the drawing is loaded on that model
   * (DrawingKernel.load), the report lists the changed and dangling dimensions.
   */
  const onReplaceModel = async (files: FileList | null) => {
    const d = drawingRef.current;
    if (!files || files.length === 0 || !d) return;
    const file = files[0];
    const model = await file.text();
    const drawingText = await d.save();
    if (await openContent({ drawing: drawingText, model, modelName: file.name }, namesRef.current.drawing, false)) {
      setMessage(t('drawing.file.modelReplaced', { name: file.name }));
    }
  };

  const onSave = async () => {
    const d = drawingRef.current;
    if (!d || !desc) return;
    try {
      const designation = sheet?.titleShown?.designation?.trim() ?? '';
      const [text, model] = await Promise.all([d.save(), d.saveModel()]);
      const name = designation ? downloadName(designation, '', DRAWING_FILE_EXTENSION.slice(1)) : drawingFileName(namesRef.current.drawing);
      const fileText = packDrawingFile({ drawing: text, model, modelName: namesRef.current.model });
      download(fileText, name, 'application/json');
      savedRef.current = text;
      namesRef.current = { ...namesRef.current, drawing: name };
      const file: DocumentFile = { kind: 'drawing', name, text: fileText };
      hostRef.current.onFile?.(file);
      hostRef.current.onChange?.(file, false);
      setMessage(t('drawing.file.saved', { name }));
    } catch (e) {
      if (!isCancelled(e)) setError(errorOf(e));
    }
  };

  /**
   * Cancels the running kernel call (e.g. a long HLR): the worker is terminated, a new one is
   * started and the drawing is loaded from the last snapshot with its model.
   */
  const cancelOperation = async () => {
    const old = kernelRef.current;
    if (!old) return;
    old.cancel(t('drawing.cancelledByUser'));
    setKernel(null);
    setStatus('loading');
    queueRef.current = Promise.resolve();
    const snap = snapshotRef.current;
    const saved = savedRef.current;
    drawingRef.current = null;
    try {
      const client = await loadDrawingFrom(defaultDrawingBaseUrl());
      setKernel(client);
      setStatus('ready');
      if (snap) await openDrawing(async (m) => (await m.load(snap.text, snap.model)).drawing, snap.name, snap.modelName, saved, client.root);
      else {
        setDesc(null);
        setPicture(null);
      }
      setNotice(t('busy.cancelled'));
    } catch (e) {
      setStatus('failed');
      setError({ code: 'Cancelled', message: t('busy.restoreFailed', { reason: errorOf(e).message }) });
    }
  };

  const onExport = async (format: 'pdf' | 'dxf' | 'svg') => {
    const d = drawingRef.current;
    if (!d || !sheet) return;
    try {
      await queueRef.current;
      const designation = sheet.titleShown?.designation ?? '';
      if (format === 'pdf') {
        download(await d.exportPdf(), downloadName(designation, '', 'pdf'), 'application/pdf');
      } else {
        const bytes = await d.exportSheet(sheet.id, format);
        download(bytes, downloadName(designation, `_${sheet.id}`, format), format === 'dxf' ? 'application/dxf' : 'image/svg+xml');
      }
      setError(null);
      setMessage(t('drawing.exported', { format: format.toUpperCase() }));
    } catch (e) {
      if (isCancelled(e)) return;
      setError(errorOf(e));
      setMessage(t('drawing.exportFailed', { format: format.toUpperCase() }));
    }
  };

  // ---- picks of the edge tools

  const createFromPicks = useCallback(
    (state: PickState) => {
      if (!sheetId || state.picks.length === 0) return;
      const view = state.picks[0].view;
      const indices = state.picks.map((p) => p.index);
      if (tool === 'dimension') {
        run(t('drawing.done.dimension'), (d) => d.addDimension(sheetId, view, indices, { offset: Number(dimOffset) || 10 }));
      } else if (tool === 'roughness') {
        run(t('drawing.done.roughness'), (d) =>
          d.addRoughness(sheetId, view, indices[0], { method: rough.method as 'materialRemoval', parameter: rough.parameter, value: rough.value }),
        );
      } else if (tool === 'tolerance') {
        const datums = tol.datums.split(/[\s,;]+/).filter(Boolean);
        run(t('drawing.done.tolerance'), (d) =>
          d.addTolerance(sheetId, view, indices[0], { characteristic: tol.characteristic, tolerance: tol.tolerance, datums }),
        );
      } else if (tool === 'datum') {
        run(t('drawing.done.datum'), (d) => d.addDatum(sheetId, view, indices[0], datumLetter));
      }
    },
    [sheetId, tool, run, dimOffset, rough, tol, datumLetter, t],
  );

  const onEdgeClick = (pick: EdgePick) => {
    const next = addPick(pickState, tool, pick);
    if (next.message) setMessage(t(next.message));
    if (next.ready) {
      createFromPicks(next);
    } else {
      setPickState(next);
    }
  };

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if ((e.target as HTMLElement)?.tagName === 'INPUT' || (e.target as HTMLElement)?.tagName === 'TEXTAREA') return;
      if (e.key === 'Escape') {
        setPickState(emptyPicks);
        setTool('select');
      } else if (e.key === 'Enter' && tool === 'dimension' && canDimensionSingle(pickState)) {
        createFromPicks(pickState);
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [tool, pickState, createFromPicks]);

  const onSheetClick = (p: Point, view: PictureView | null) => {
    if (!sheetId) return;
    if (tool === 'balloon') {
      // The shelf of a manual position: anywhere on the sheet, after the leader edge.
      const b = balloonShelf(pickState, p);
      if (!b) {
        setMessage(t('drawing.help.balloon'));
        return;
      }
      run(t('drawing.done.balloon'), (d) => d.addBalloon(sheetId, b.view, b.index, { at: b.at }));
      return;
    }
    if (!view) {
      setMessage(t('drawing.pick.clickView'));
      return;
    }
    const vp = sheetToViewPoint(view, p);
    if (tool === 'detail') {
      run(t('drawing.done.detail'), (d) =>
        d.addView(sheetId, { kind: 'detail', parent: view.id, detailCenter: vp, detailRadius: Number(detailRadius) || 10 }),
      );
    } else if (tool === 'section') {
      const spec: ViewSpec = { kind: 'section', parent: view.id, planePoint: viewPointToModel(view, vp), markView: view.id };
      if (sectionSide !== 'none') spec.side = sectionSide as ViewSpec['side'];
      run(t('drawing.done.section'), (d) => d.addView(sheetId, spec));
    }
  };

  const switchTool = (next: Tool) => {
    setTool(next);
    setPickState(emptyPicks);
  };

  const onUpdate = () => {
    let r: UpdateReport | null = null;
    void run(t('drawing.done.update'), async (d) => {
      r = await d.updateFromModel();
      setReport(r);
    }).then((ok) => {
      const rep = r as UpdateReport | null;
      if (!ok || !rep) return;
      setMessage(
        rep.modelChanged
          ? t('drawing.done.updated', { revision: rep.revision, stale: rep.stale.length, dangling: rep.dangling.length })
          : t('drawing.done.modelSame'),
      );
    });
  };

  useHotkeys(active, {
    save: () => void onSave(),
    open: () => openDrawingInput.current?.click(),
    escape: () => {
      setTool('select');
      setPickState(emptyPicks);
      setSelectedView(null);
    },
    delete: () => {
      const id = selectedView;
      const sid = sheetIdRef.current;
      if (!id || !sid) return;
      void run(t('drawing.done.viewRemoved'), async (d) => {
        await d.removeView(sid, id);
        setSelectedView(null);
      });
    },
  });
  const openDrawingInput = useRef<HTMLInputElement | null>(null);

  // State for tests.
  useEffect(() => {
    window.bendcoreDrawingEditorIdle = () => waitIdle(() => [kernelRef.current]);
    return () => {
      delete window.bendcoreDrawingEditorIdle;
    };
  }, []);
  useEffect(() => {
    window.bendcoreDrawingEditor = {
      status,
      error,
      message,
      sheet: sheetId,
      tool,
      description: desc,
      staleness,
      report,
      picks: pickState.picks,
    };
  }, [status, error, message, sheetId, tool, desc, staleness, report, pickState]);

  const hasBase = !!sheet?.views.some((v) => v.kind === 'base');
  // Positions of the specification and the balloons that show them on this sheet.
  const specPositions = (spec.table?.sections ?? []).flatMap((s) => s.items.filter((i) => i.position > 0).map((i) => i.position));
  const balloonCount = (position: number) => (sheet?.balloons ?? []).filter((b) => b.text === String(position)).length;
  const missingPositions = specPositions.filter((p) => balloonCount(p) === 0);
  const selected = sheet?.views.find((v) => v.id === selectedView) ?? null;
  const drawing = drawingRef.current;

  return (
    <div className="dr-root">
      <header className="dr-toolbar">
        <strong className="dr-title">{t('drawing.title')}</strong>
        <label className="dr-file">
          {t('drawing.openModel')}
          <input type="file" accept=".json,application/json" data-testid="open-model" onChange={(e) => void onOpenModel(e.target.files)} disabled={!kernel} />
        </label>
        <label className="dr-file" title={t('drawing.file.openHint')}>
          {t('drawing.file.open')}
          <input ref={openDrawingInput} type="file" accept=".json,application/json" multiple data-testid="open-drawing" onChange={(e) => void onOpenDrawing(e.target.files)} disabled={!kernel} />
        </label>
        <label className="dr-file" title={t('drawing.file.replaceModelHint')}>
          {t('drawing.file.replaceModel')}
          <input type="file" accept=".json,application/json" data-testid="replace-model" onChange={(e) => void onReplaceModel(e.target.files)} disabled={!drawing} />
        </label>
        <button onClick={() => void onSave()} disabled={!drawing} data-testid="drawing-save" title="Ctrl+S">{t('drawing.save')}</button>
        <span className="dr-sep" />
        <button onClick={() => void onExport('pdf')} disabled={!drawing} data-testid="export-pdf">PDF</button>
        <button onClick={() => void onExport('dxf')} disabled={!drawing} data-testid="export-dxf">DXF</button>
        <button onClick={() => void onExport('svg')} disabled={!drawing} data-testid="export-svg">SVG</button>
        <BusyIndicator client={kernel} onCancel={() => void cancelOperation()} />
        <span className="dr-sep" />
        {TOOLS.map((k) => (
          <button
            key={k}
            className={tool === k ? 'dr-active' : ''}
            onClick={() => switchTool(k)}
            disabled={!drawing || (k === 'balloon' && desc?.model.kind !== 'assembly')}
            title={k === 'balloon' && desc?.model.kind !== 'assembly' ? t('drawing.help.balloonPart') : undefined}
            data-testid={`tool-${k}`}
          >
            {t(`drawing.tool.${k}`)}
          </button>
        ))}
        <span className="dr-sep" />
        <button onClick={() => canvas.current?.fit()} disabled={!picture}>{t('drawing.fit')}</button>
      </header>

      <div className="dr-main">
        <aside className="dr-left">
          {sheet && desc && drawing && (
            <>
              <h2>{t('drawing.sheets')}</h2>
              <div className="dr-sheet-tabs">
                {desc.sheets.map((s) => (
                  <button key={s.id} className={s.id === sheetId ? 'dr-active' : ''} onClick={() => refresh(s.id)}>
                    {s.id} · {s.name}
                  </button>
                ))}
                <button onClick={() => run(t('drawing.done.sheetAdded'), async (d) => ({ switchTo: await d.addSheet({}) }))} title={t('drawing.addSheet')} data-testid="add-sheet">+</button>
              </div>
              <div className="dr-row">
                <label>
                  {t('drawing.format')}
                  <select
                    value={sheet.format}
                    data-testid="sheet-format"
                    onChange={(e) => {
                      const format = e.target.value as SheetInfo['format'];
                      run(t('drawing.done.format'), (d) => d.setSheet(sheet.id, { format }));
                    }}
                  >
                    {['A0', 'A1', 'A2', 'A3', 'A4'].map((f) => (
                      <option key={f} value={f}>{t(`drawing.sheetFormat.${f}` as MessageKey)}</option>
                    ))}
                  </select>
                </label>
                <label>
                  <select
                    value={sheet.orientation}
                    data-testid="sheet-orientation"
                    onChange={(e) => {
                      const orientation = e.target.value as SheetInfo['orientation'];
                      run(t('drawing.done.orientation'), (d) => d.setSheet(sheet.id, { orientation }));
                    }}
                  >
                    <option value="landscape">{t('drawing.landscape')}</option>
                    <option value="portrait">{t('drawing.portrait')}</option>
                  </select>
                </label>
                <label>
                  {t('drawing.scale')}
                  <select
                    value={sheet.scale ? sheet.scale.ratio.join(':') : ''}
                    data-testid="sheet-scale"
                    onChange={(e) => {
                      const scale = scaleOf(e.target.value);
                      run(t('drawing.done.sheetScale'), (d) => d.setSheet(sheet.id, { scale }));
                    }}
                  >
                    <option value="">{t('drawing.scaleAuto', { scale: sheet.computedScale?.label ?? '' })}</option>
                    {SCALES.map((s) => (
                      <option key={s.join(':')} value={s.join(':')}>{scaleText(s)}</option>
                    ))}
                  </select>
                </label>
              </div>
              {desc.sheets.length > 1 && (
                <button onClick={() => run(t('drawing.done.sheetRemoved'), (d) => d.removeSheet(sheet.id))}>{t('drawing.removeSheet', { id: sheet.id })}</button>
              )}

              <h2>{t('drawing.views')}</h2>
              {!hasBase && (
                <div className="dr-row">
                  <select value={baseStandard} onChange={(e) => setBaseStandard(e.target.value as StandardView)}>
                    {STANDARD_VIEWS.map((s) => (
                      <option key={s} value={s}>{t('drawing.viewOf', { side: t(`drawing.side.${s}`) })}</option>
                    ))}
                  </select>
                  <button data-testid="add-base" onClick={() => run(t('drawing.done.baseView'), (d) => d.addView(sheet.id, { kind: 'base', standard: baseStandard }))}>
                    {t('drawing.baseView')}
                  </button>
                </div>
              )}
              {hasBase && (
                <div className="dr-buttons">
                  {STANDARD_PROJECTED.map((s) => (
                    <button key={s} data-testid={`add-${s}`} onClick={() => run(t('drawing.done.projected', { side: t(`drawing.side.${s}`) }), (d) => d.addView(sheet.id, { kind: 'projected', standard: s }))}>
                      {t('drawing.projectedView', { side: t(`drawing.side.${s}`) })}
                    </button>
                  ))}
                  <button data-testid="add-isometric" onClick={() => run(t('drawing.done.isometric'), (d) => d.addView(sheet.id, { kind: 'axonometric', axonometry: 'isometric' }))}>
                    {t('drawing.isometric')}
                  </button>
                  <button data-testid="add-dimetric" onClick={() => run(t('drawing.done.dimetric'), (d) => d.addView(sheet.id, { kind: 'axonometric', axonometry: 'dimetric' }))}>
                    {t('drawing.dimetric')}
                  </button>
                </div>
              )}
              <ul className="dr-list" data-testid="view-list">
                {sheet.views.map((v) => (
                  <li key={v.id} className={v.id === selectedView ? 'dr-selected' : ''} onClick={() => setSelectedView(v.id)}>
                    <span>{v.id}</span> {t(`drawing.viewKind.${v.kind}`)}
                    {v.label ? ` ${v.label}` : ''} <span className="dr-muted">{v.computedScale?.label}</span>
                  </li>
                ))}
              </ul>
              {selected && (
                <div className="dr-box" data-testid="view-properties">
                  <div>
                    {selected.id}: {t(`drawing.viewKind.${selected.kind}`)}
                    {selected.inGroup ? t('drawing.inGroup') : ''}
                  </div>
                  {!selected.inGroup && (
                    <label>
                      {t('drawing.viewScale')}
                      <select
                        value={selected.scale ? selected.scale.ratio.join(':') : ''}
                        onChange={(e) => {
                          const scale = scaleOf(e.target.value);
                          run(t('drawing.done.viewScale'), (d) => d.setView(sheet.id, selected.id, { scale }));
                        }}
                      >
                        <option value="">{t('drawing.scaleAuto', { scale: selected.computedScale?.label ?? '' })}</option>
                        {SCALES.map((s) => (
                          <option key={s.join(':')} value={s.join(':')}>{scaleText(s)}</option>
                        ))}
                      </select>
                    </label>
                  )}
                  <label className="dr-check">
                    <input type="checkbox" checked={selected.hidden} onChange={(e) => run(t('drawing.done.hidden'), (d) => d.setView(sheet.id, selected.id, { hidden: !selected.hidden }))} />
                    {t('drawing.hiddenLines')}
                  </label>
                  {desc.model.kind === 'assembly' && (
                    <label className="dr-check" title={t('drawing.autoBalloonsHint')}>
                      <input
                        type="checkbox"
                        checked={selected.autoBalloons}
                        data-testid="view-auto-balloons"
                        onChange={(e) => run(t('drawing.done.autoBalloons'), (d) => d.setView(sheet.id, selected.id, { autoBalloons: !selected.autoBalloons }))}
                      />
                      {t('drawing.autoBalloons')}
                    </label>
                  )}
                  {selected.position && (
                    <button onClick={() => run(t('drawing.done.autoPosition'), (d) => d.setView(sheet.id, selected.id, { position: null }))}>{t('drawing.autoPosition')}</button>
                  )}
                  <button
                    onClick={() =>
                      run(t('drawing.done.viewRemoved'), async (d) => {
                        await d.removeView(sheet.id, selected.id);
                        setSelectedView(null);
                      })
                    }
                  >
                    {t('drawing.removeView')}
                  </button>
                </div>
              )}

              <h2>{t('drawing.tool')}</h2>
              <div className="dr-box" data-testid="tool-options">
                {tool === 'select' && <p className="dr-muted">{t('drawing.help.select')}</p>}
                {tool === 'dimension' && (
                  <>
                    <p className="dr-muted">{t('drawing.help.dimension')}</p>
                    <label>
                      {t('drawing.offset')} <input value={dimOffset} onChange={(e) => setDimOffset(e.target.value)} size={4} />
                    </label>
                    <button disabled={!canDimensionSingle(pickState)} onClick={() => createFromPicks(pickState)}>{t('drawing.tool.dimension')}</button>
                    <div className="dr-muted" data-testid="picks">{t('drawing.picks', { count: pickState.picks.length })}</div>
                  </>
                )}
                {tool === 'roughness' && (
                  <>
                    <select value={rough.method} onChange={(e) => setRough({ ...rough, method: e.target.value })}>
                      <option value="materialRemoval">{t('drawing.rough.materialRemoval')}</option>
                      <option value="noMaterialRemoval">{t('drawing.rough.noMaterialRemoval')}</option>
                      <option value="unspecified">{t('drawing.rough.unspecified')}</option>
                    </select>
                    <div className="dr-row">
                      <input value={rough.parameter} onChange={(e) => setRough({ ...rough, parameter: e.target.value })} size={4} />
                      <input value={rough.value} onChange={(e) => setRough({ ...rough, value: e.target.value })} size={6} />
                    </div>
                    <p className="dr-muted">{t('drawing.help.straight')}</p>
                  </>
                )}
                {tool === 'tolerance' && (
                  <>
                    <select value={tol.characteristic} onChange={(e) => setTol({ ...tol, characteristic: e.target.value as GeometricCharacteristic })}>
                      {CHARACTERISTICS.map(([c]) => (
                        <option key={c} value={c}>{t(`drawing.char.${c}`)}</option>
                      ))}
                    </select>
                    <label>
                      {t('drawing.toleranceValue')} <input value={tol.tolerance} onChange={(e) => setTol({ ...tol, tolerance: e.target.value })} size={6} />
                    </label>
                    <label>
                      {t('drawing.datums')} <input value={tol.datums} onChange={(e) => setTol({ ...tol, datums: e.target.value })} size={6} />
                    </label>
                    <p className="dr-muted">{t('drawing.help.straight')}</p>
                  </>
                )}
                {tool === 'datum' && (
                  <label>
                    {t('drawing.datumLetter')} <input value={datumLetter} onChange={(e) => setDatumLetter(e.target.value)} size={3} />
                  </label>
                )}
                {tool === 'balloon' && (
                  <>
                    <p className="dr-muted">{t('drawing.help.balloon')}</p>
                    <div className="dr-muted" data-testid="balloon-pick">
                      {pickState.picks.length ? t('drawing.pick.shelf') : t('drawing.picks', { count: 0 })}
                    </div>
                  </>
                )}
                {tool === 'detail' && (
                  <label>
                    {t('drawing.detailRadius')} <input value={detailRadius} onChange={(e) => setDetailRadius(e.target.value)} size={4} />
                    <span className="dr-muted">{t('drawing.detailHint')}</span>
                  </label>
                )}
                {tool === 'section' && (
                  <label>
                    {t('drawing.sectionSide')}
                    <select value={sectionSide} onChange={(e) => setSectionSide(e.target.value)}>
                      {SHEET_SIDES.map((s) => (
                        <option key={s} value={s}>{t(`drawing.sectionAt.${s}`)}</option>
                      ))}
                      <option value="none">{t('drawing.sectionFree')}</option>
                    </select>
                    <span className="dr-muted">{t('drawing.sectionHint')}</span>
                  </label>
                )}
              </div>

              <h2>{t('drawing.annotations')}</h2>
              <ul className="dr-list" data-testid="annotation-list">
                {sheet.dimensions.map((d) => (
                  <li key={d.id} className={d.state === 'dangling' ? 'dr-dangling' : ''} data-testid={`dimension-${d.id}`} data-value={d.value}>
                    <span>{d.id}</span> {t(`drawing.dimType.${d.type}`)} <strong>{d.text}</strong>
                    {d.state === 'dangling' && <span className="dr-muted">{t('drawing.dangling', { reason: d.reason ?? '' })}</span>}
                    <input
                      className="dr-tol"
                      placeholder={t('drawing.tolerancePlaceholder')}
                      defaultValue={d.tolerance ?? ''}
                      title={t('drawing.toleranceHint')}
                      onBlur={(e) => {
                        if ((e.target.value || null) !== d.tolerance) run(t('drawing.done.dimTolerance'), (h) => h.setDimension(sheet.id, d.id, { tolerance: e.target.value || null }));
                      }}
                    />
                    <button title={t('drawing.remove')} onClick={() => run(t('drawing.done.dimRemoved'), (h) => h.removeAnnotation(sheet.id, d.id))}>×</button>
                  </li>
                ))}
                {[...sheet.roughness.map((r) => ({ ...r, label: t('drawing.roughnessItem', { parameter: r.spec.parameter ?? '', value: r.spec.value ?? '' }) })),
                  ...sheet.tolerances.map((x) => ({ ...x, label: `${t(`drawing.char.${x.characteristic}`)} ${x.tolerance} ${x.datums.join(' ')}` })),
                  ...sheet.datums.map((b) => ({ ...b, label: t('drawing.datumItem', { letter: b.letter }) }))].map((a) => (
                  <li key={a.id} className={a.state === 'dangling' ? 'dr-dangling' : ''}>
                    <span>{a.id}</span> {a.label}
                    <button title={t('drawing.remove')} onClick={() => run(t('drawing.done.symbolRemoved'), (h) => h.removeAnnotation(sheet.id, a.id))}>×</button>
                  </li>
                ))}
                {(sheet.positions ?? []).map((p) => (
                  <li key={p.id} className={p.state === 'dangling' ? 'dr-dangling' : ''} data-testid={`balloon-${p.id}`} data-number={p.text ?? ''} data-state={p.state ?? ''}>
                    <span>{p.id}</span> {p.state === 'ok' ? t('drawing.balloonItem', { number: p.text ?? '' }) : t('drawing.balloonDangling')}
                    {p.at && (
                      <button title={t('drawing.balloonDefault')} onClick={() => run(t('drawing.done.balloonShelf'), (h) => h.setBalloon(sheet.id, p.id, { at: null }))}>
                        ↺
                      </button>
                    )}
                    <button title={t('drawing.remove')} onClick={() => run(t('drawing.done.balloonRemoved'), (h) => h.removeAnnotation(sheet.id, p.id))}>×</button>
                  </li>
                ))}
              </ul>
              <TechnicalRequirements
                key={`${sheet.id}-${sheet.technicalRequirements.join('|')}`}
                items={sheet.technicalRequirements}
                onApply={(items) => run(t('drawing.done.requirements'), (d) => d.setSheet(sheet.id, { technicalRequirements: items }))}
              />
              <label className="dr-check">
                <input
                  type="checkbox"
                  checked={!!sheet.roughnessRemainder}
                  onChange={(e) =>
                    run(t('drawing.done.remainder'), (d) => d.setSheet(sheet.id, { roughnessRemainder: !sheet.roughnessRemainder ? { value: rough.value, parameter: rough.parameter } : null }))
                  }
                />
                {t('drawing.remainder', { parameter: rough.parameter, value: rough.value })}
              </label>
            </>
          )}
        </aside>

        <section className="dr-center">
          <div className="dr-tabs">
            <button className={tab === 'sheet' ? 'dr-active' : ''} onClick={() => setTab('sheet')}>{t('drawing.tab.sheet')}</button>
            <button className={tab === 'specification' ? 'dr-active' : ''} onClick={() => setTab('specification')} data-testid="tab-specification">
              {t('drawing.tab.specification')}
            </button>
            {sheet && <span className="dr-muted">{t('drawing.sheetInfo', { name: sheet.name ?? '', scale: sheet.computedScale?.label ?? '', model: modelName })}</span>}
          </div>
          {tab === 'sheet' && picture && (
            <SheetCanvas
              ref={canvas}
              picture={picture}
              tool={tool}
              picks={pickState.picks}
              selectedView={selectedView}
              onSelectView={setSelectedView}
              onEdgeClick={onEdgeClick}
              onSheetClick={onSheetClick}
              onMoveView={(view, x, y) => run(t('drawing.done.viewMoved'), (d) => d.moveView(sheetId!, view, x, y))}
            />
          )}
          {tab === 'sheet' && !picture && <div className="dr-empty">{status === 'loading' ? t('drawing.loadingShort') : t('drawing.noDrawing')}</div>}
          {tab === 'specification' && (
            <div className="dr-spec" data-testid="specification">
              {spec.table ? (
                <table>
                  <thead>
                    <tr>
                      {SPEC_COLUMNS.map((c) => (
                        <th key={c}>{t(`drawing.spec.${c}`)}</th>
                      ))}
                      <th>{t('drawing.spec.onSheet')}</th>
                    </tr>
                  </thead>
                  <tbody>
                    {spec.table.sections.flatMap((s) => [
                      <tr key={`h-${s.section}`} className="dr-spec-section"><td colSpan={8}>{s.title}</td></tr>,
                      ...s.items.map((it, i) => (
                        <tr key={`${s.section}-${i}`} data-testid={it.position ? `spec-position-${it.position}` : undefined} data-balloons={it.position ? balloonCount(it.position) : undefined}>
                          <td>{it.format}</td><td>{it.zone}</td><td>{it.position || ''}</td><td>{it.designation}</td><td>{it.name}</td><td>{it.quantity ?? ''}</td><td>{it.note}</td>
                          <td>{it.position ? balloonCount(it.position) : ''}</td>
                        </tr>
                      )),
                    ])}
                  </tbody>
                </table>
              ) : (
                <p className="dr-muted">{spec.reason ?? t('drawing.noDrawing')}</p>
              )}
              {spec.table && sheet && (
                <div className="dr-muted" data-testid="balloon-summary">
                  {t('drawing.balloonsCount', { count: sheet.balloons.length, total: specPositions.length })}
                  {missingPositions.length > 0 && <div data-testid="balloon-missing">{t('drawing.balloonsMissing', { list: missingPositions.join(', ') })}</div>}
                </div>
              )}
              {desc?.model.kind === 'assembly' && sheet && (
                <label className="dr-check">
                  <input type="checkbox" checked={sheet.partsList} onChange={(e) => run(t('drawing.done.partsList'), (d) => d.setSheet(sheet.id, { partsList: !sheet.partsList }))} />
                  {t('drawing.partsList')}
                </label>
              )}
            </div>
          )}
        </section>

        <aside className="dr-right">
          {sheet && desc && drawing && (
            <>
              <h2>{t('drawing.model')}</h2>
              {variables.length > 0 && (
                <table className="dr-vars" data-testid="variables">
                  <tbody>
                    {variables.map((v) => (
                      <tr key={v.name}>
                        <td>{v.name}</td>
                        <td>
                          <input
                            key={`${v.name}=${v.expression}`}
                            defaultValue={v.expression}
                            data-testid={`variable-${v.name}`}
                            onKeyDown={(e) => {
                              if (e.key === 'Enter') (e.target as HTMLInputElement).blur();
                            }}
                            onBlur={(e) => {
                              if (e.target.value !== v.expression) run(t('drawing.done.variable', { name: v.name }), (d) => d.setVariable(v.name, e.target.value));
                            }}
                          />
                        </td>
                        <td className="dr-muted">{v.value ?? v.error}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              )}
              {desc.model.kind === 'part' && (
                <ModelPropertiesForm
                  key={JSON.stringify(desc.model.properties)}
                  properties={desc.model.properties}
                  onApply={(p) =>
                    run(t('drawing.done.properties'), async (d) => {
                      const r = await d.setModelProperties(p);
                      setReport(r);
                    })
                  }
                />
              )}

              <h2>{t('drawing.update')}</h2>
              <div className={`dr-box ${staleness?.modelChanged ? 'dr-stale' : ''}`} data-testid="update-panel">
                <div data-testid="model-state">
                  {staleness?.modelChanged
                    ? t('drawing.modelChanged', { views: staleness.staleViews.length, dimensions: staleness.dimensions.length })
                    : t('drawing.modelSame')}
                </div>
                <button data-testid="update-from-model" onClick={onUpdate}>{t('drawing.updateButton')}</button>
                {report && (
                  <div className="dr-report" data-testid="update-report">
                    <div>
                      {t('drawing.report.revision', { revision: report.revision, computed: report.viewsComputed, cached: report.viewsFromCache })}
                    </div>
                    <div>{t('drawing.report.stale', { count: report.stale.length })}</div>
                    <ul data-testid="report-stale">
                      {report.stale.map((s) => (
                        <li key={s}>{s}</li>
                      ))}
                    </ul>
                    <div>{t('drawing.report.dangling', { count: report.dangling.length })}</div>
                    <ul data-testid="report-dangling">
                      {report.dangling.map((s) => (
                        <li key={s} className="dr-dangling">{s}</li>
                      ))}
                    </ul>
                    {report.warnings.length > 0 && <div className="dr-muted">{report.warnings.join('; ')}</div>}
                  </div>
                )}
              </div>

              <h2>{t('drawing.titleBlock')}</h2>
              <TitleBlockForm
                key={`${sheet.id}-${JSON.stringify(sheet.title)}`}
                sheet={sheet}
                onApply={(title) => run(t('drawing.done.title'), (d) => d.setSheet(sheet.id, { title }))}
              />
            </>
          )}
        </aside>
      </div>

      {notice && (
        <div className="app-notice" role="status" data-testid="drawing-notice">
          {notice}
          <button type="button" onClick={() => setNotice(null)} aria-label={t('notify.dismiss')}>
            ×
          </button>
        </div>
      )}
      <footer className={`dr-status${error ? ' dr-error' : ''}`} data-testid="status" role={error ? 'alert' : 'status'}>
        {message}
        {error ? ` — ${error.code}: ${error.message}` : ''}
      </footer>
    </div>
  );
}

function TechnicalRequirements(props: { items: string[]; onApply(items: string[]): void }) {
  const { t } = useI18n();
  const [text, setText] = useState(props.items.join('\n'));
  return (
    <div className="dr-box">
      <div>{t('drawing.requirements')}</div>
      <textarea rows={3} value={text} onChange={(e) => setText(e.target.value)} data-testid="technical-requirements" />
      <button onClick={() => props.onApply(text.split('\n').map((s) => s.trim()).filter(Boolean))}>{t('drawing.apply')}</button>
    </div>
  );
}

function ModelPropertiesForm(props: {
  properties: DrawingDescription['model']['properties'];
  onApply(p: Partial<DrawingDescription['model']['properties']>): void;
}) {
  const { t } = useI18n();
  const [p, setP] = useState({ ...props.properties, density: props.properties.density === null ? '' : String(props.properties.density) });
  return (
    <div className="dr-box dr-form">
      <label>{t('drawing.prop.designation')} <input value={p.designation} onChange={(e) => setP({ ...p, designation: e.target.value })} data-testid="model-designation" /></label>
      <label>{t('drawing.prop.name')} <input value={p.name} onChange={(e) => setP({ ...p, name: e.target.value })} /></label>
      <label>{t('drawing.prop.material')} <input value={p.material} onChange={(e) => setP({ ...p, material: e.target.value })} /></label>
      <label>{t('drawing.prop.density')} <input value={p.density} onChange={(e) => setP({ ...p, density: e.target.value })} size={6} /></label>
      <button onClick={() => props.onApply({ ...p, density: p.density.trim() === '' ? null : Number(p.density) })}>{t('drawing.apply')}</button>
    </div>
  );
}

const TITLE_ROWS: (keyof Pick<SheetInfo['title'], 'designation' | 'name' | 'material' | 'mass' | 'litera' | 'organization'>)[] = [
  'designation',
  'name',
  'material',
  'mass',
  'litera',
  'organization',
];
const SIGNATURES: (keyof Pick<SheetInfo['title'], 'developer' | 'checker' | 'techControl' | 'normControl' | 'approver'>)[] = [
  'developer',
  'checker',
  'techControl',
  'normControl',
  'approver',
];

function TitleBlockForm(props: { sheet: SheetInfo; onApply(title: SheetInfo['title']): void }) {
  const { t } = useI18n();
  const [title, setTitle] = useState(props.sheet.title);
  const shown = props.sheet.titleShown;
  return (
    <div className="dr-box dr-form" data-testid="title-block">
      {TITLE_ROWS.map((k) => (
        <label key={k}>
          {t(`drawing.title.${k}`)}
          <input
            value={title[k]}
            placeholder={shown && k in shown ? String((shown as Record<string, unknown>)[k] ?? '') : ''}
            onChange={(e) => setTitle({ ...title, [k]: e.target.value })}
            data-testid={`title-${k}`}
          />
        </label>
      ))}
      {SIGNATURES.map((k) => (
        <label key={k}>
          {t(`drawing.title.${k}`)}
          <input value={title[k].surname} placeholder={t('drawing.title.surname')} onChange={(e) => setTitle({ ...title, [k]: { ...title[k], surname: e.target.value } })} data-testid={`title-${k}`} />
          <input value={title[k].date} placeholder={t('drawing.title.date')} size={5} onChange={(e) => setTitle({ ...title, [k]: { ...title[k], date: e.target.value } })} />
        </label>
      ))}
      <button onClick={() => props.onApply(title)} data-testid="title-apply">{t('drawing.apply')}</button>
    </div>
  );
}
