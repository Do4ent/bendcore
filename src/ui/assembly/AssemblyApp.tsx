/**
 * Assembly page (docs/ui/ASSEMBLY_UI.md): the assembly document of bendcore_assembly_wasm with
 * a component/mate tree, mates picked in the viewport, dragging with re-solving, degrees of
 * freedom, interference report with highlighting, BOM, exploded views, fastener library,
 * patterns, undo/redo (Ctrl+Z / Ctrl+Y), JSON and STEP files.
 *
 * Every action is an assembly document call, executed in the WASM worker (docs/ui/APP_SHELL.md):
 * a long one (solve, interference) shows the progress indicator and can be cancelled, then the
 * document is reloaded from the snapshot taken after the last completed change. The geometry shown is the kernel's tessellation of
 * the part definitions placed by the document. window.bendcoreAssembly exposes the page state
 * (read-only) and view helpers for end-to-end tests.
 */

import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type {
  AssemblyDocumentHandle,
  AssemblyKernel,
  AssemblyState,
  Bom,
  ElementInfo,
  InterferenceReport,
  KernelMesh,
  MateInfo,
  MateSpec,
  OccurrenceInfo,
  PlacedPart,
  SolveReport,
} from '../../wasm/bendcore_assembly_api.mjs';
import { BomPanel, ExplodePanel, FastenerPanel, InterferencePanel, MatePanel, PatternPanel, PropertiesPanel } from './AssemblyPanels';
import AssemblyTree, { refLabel } from './AssemblyTree';
import AssemblyViewport, { pathKey } from './AssemblyViewport';
import type { AssemblyPick, AssemblyPickMode, AssemblyViewportHandle, PartHighlight } from './AssemblyViewport';
import { clampProgress, insertionDirection } from './assemblyMath';
import type { V3 } from './assemblyMath';
import { defaultAssemblyBaseUrl, loadAssemblyFrom } from './assemblyLoader';
import type { DragStart } from './AssemblyViewport';
import type { Remote, WasmClient } from '../worker/wasmClient';
import { isCancelled } from '../worker/wasmClient';
import { waitIdle } from '../worker/idle';
import { LatestRunner } from '../worker/latestRunner';
import BusyIndicator from '../app/BusyIndicator';
import { useHotkeys } from '../app/hotkeys';
import { useI18n } from '../app/i18n';
import type { DocumentFile, EditorHostProps, PartSource } from '../app/editorHost';
import { dofText } from './labels';
import { nextMateSlots } from './mateRules';
import type { PickedElement } from './mateRules';
import '../kernel/KernelViewer.css';
import '../modeling/PartModeler.css';
import './Assembly.css';

const MESH_PARAMS = { linearDeflection: 0.05, angularDeflection: 0.25 };

type PanelId = 'mate' | 'fasteners' | 'interference' | 'bom' | 'explode' | 'pattern' | 'properties';

const PANELS: { id: PanelId; label: string }[] = [
  { id: 'mate', label: 'Сопряжение' },
  { id: 'fasteners', label: 'Крепёж' },
  { id: 'interference', label: 'Пересечения' },
  { id: 'bom', label: 'BOM' },
  { id: 'explode', label: 'Разнесённый вид' },
  { id: 'pattern', label: 'Массив' },
  { id: 'properties', label: 'Свойства' },
];

export interface AssemblyPageState {
  status: 'loading' | 'ready' | 'failed';
  error: { code: string; message: string } | null;
  state: AssemblyState | null;
  panel: PanelId;
  pickMode: AssemblyPickMode;
  mateSlots: (PickedElement | null)[];
  hole: PickedElement | null;
  selectedComponent: number | null;
  bom: Bom | null;
  interference: InterferenceReport | null;
  explode: { view: number | null; progress: number };
  /** Solve report of the last drag step while dragging. */
  dragSolve: SolveReport | null;
  parts: PlacedPart[];
}

declare global {
  interface Window {
    bendcoreAssembly?: {
      getState(): AssemblyPageState;
      worldToClient(x: number, y: number, z: number): { x: number; y: number } | null;
      faceClientPoint(path: number[], faceIndex: number): { x: number; y: number } | null;
      /** Element records of every face of the part at `path` (kernel element() calls). */
      faces(path: number[]): Promise<ElementInfo[]>;
      /** Resolves when no kernel call is in flight and the page has rendered the last result. */
      idle(): Promise<void>;
    };
  }
}

type AssemblyDoc = Remote<AssemblyDocumentHandle>;

function download(name: string, text: string, type: string): void {
  const url = URL.createObjectURL(new Blob([text], { type }));
  const a = document.createElement('a');
  a.href = url;
  a.download = name;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

const baseName = (file: string) => file.replace(/\.[^.]+$/, '');

function findOccurrence(list: OccurrenceInfo[], id: number): OccurrenceInfo | null {
  for (const o of list) {
    if (o.id === id) return o;
  }
  return null;
}

export interface AssemblyAppProps extends EditorHostProps {
  /** Parts the shell can insert without a file dialog (open part tabs, recent part files). */
  partLibrary?: PartSource[];
}

const AssemblyApp: React.FC<AssemblyAppProps> = ({
  active = true,
  initial = null,
  initialDirty = false,
  onChange,
  onFile,
  onOpenPart,
  partLibrary,
}) => {
  const { t } = useI18n();
  const [kernel, setKernel] = useState<WasmClient<AssemblyKernel> | null>(null);
  const kernelRef = useRef(kernel);
  kernelRef.current = kernel;
  const docRef = useRef<AssemblyDoc | null>(null);
  /** Serialises document bodies: one runs at a time, in click order. */
  const queueRef = useRef<Promise<unknown>>(Promise.resolve());
  /** Document JSON after the last completed change (cancel restores it). */
  const snapshotRef = useRef<string | null>(null);
  const savedRef = useRef<string | null>(null);
  /** Part JSON of definitions inserted from bendcore parts ("open component part"). */
  const partsRef = useRef<PartSource[]>(initial?.parts ?? []);
  const hostRef = useRef({ onChange, onFile, onOpenPart });
  hostRef.current = { onChange, onFile, onOpenPart };
  const openInputRef = useRef<HTMLInputElement | null>(null);
  const dragRunner = useRef(new LatestRunner()).current;
  const [notice, setNotice] = useState<string | null>(null);
  const [libraryChoice, setLibraryChoice] = useState('');
  const [status, setStatus] = useState<'loading' | 'ready' | 'failed'>('loading');
  const [error, setError] = useState<{ code: string; message: string } | null>(null);
  const [state, setState] = useState<AssemblyState | null>(null);
  const [meshes, setMeshes] = useState<Record<string, KernelMesh>>({});
  const meshesRef = useRef(meshes);
  meshesRef.current = meshes;
  const [parts, setParts] = useState<PlacedPart[]>([]);
  const [panel, setPanel] = useState<PanelId>('mate');
  const [pickMode, setPickMode] = useState<AssemblyPickMode>('face');
  const [mateSlots, setMateSlots] = useState<[PickedElement | null, PickedElement | null]>([null, null]);
  const [hole, setHole] = useState<PickedElement | null>(null);
  const [selectedComponent, setSelectedComponent] = useState<number | null>(null);
  const [bomMode, setBomMode] = useState<'flat' | 'structured'>('flat');
  const [bom, setBom] = useState<Bom | null>(null);
  const [interference, setInterference] = useState<InterferenceReport | null>(null);
  const [showTouching, setShowTouching] = useState(false);
  const [selectedPair, setSelectedPair] = useState<number | null>(null);
  const [explode, setExplode] = useState<{ view: number | null; progress: number }>({ view: null, progress: 0 });
  const [dragSolve, setDragSolve] = useState<SolveReport | null>(null);
  const [frameKey, setFrameKey] = useState(0);
  const viewportRef = useRef<AssemblyViewportHandle | null>(null);

  const fail = (e: unknown) => {
    const err = e as { code?: string; detail?: string; message?: string };
    setError({ code: err.code ?? 'Error', message: err.detail ?? err.message ?? String(e) });
  };

  /** Re-reads the document: state, placements, new meshes; BOM when its panel is open. */
  const refresh = useCallback(
    async (opts: { explode?: { view: number | null; progress: number }; panel?: PanelId; bomMode?: 'flat' | 'structured' } = {}) => {
      const doc = docRef.current;
      if (!doc) return;
      const st = await doc.state();
      if (docRef.current !== doc) return;
      setState(st);
      const ex = opts.explode ?? explode;
      const view = ex.view !== null && ex.view < st.explodedViews.length ? ex.view : null;
      const tess = await doc.tessellate({
        params: MESH_PARAMS,
        skip: Object.keys(meshesRef.current),
        ...(view !== null ? { view, progress: ex.progress } : {}),
      });
      const used = new Set(tess.occurrences.map((o) => o.mesh).filter((k): k is string => !!k));
      const next: Record<string, KernelMesh> = {};
      for (const k of used) next[k] = tess.meshes[k] ?? meshesRef.current[k];
      meshesRef.current = next;
      setMeshes(next);
      setParts(tess.occurrences);
      if ((opts.panel ?? panel) === 'bom') setBom(await doc.bom(opts.bomMode ?? bomMode));
    },
    [explode, panel, bomMode],
  );

  const takeSnapshot = useCallback(async (doc: AssemblyDoc) => {
    const text = await doc.save();
    if (docRef.current !== doc) return;
    snapshotRef.current = text;
    const st = await doc.state();
    hostRef.current.onChange?.(
      { kind: 'assembly', name: `${st.name || 'assembly'}.bcasm.json`, text, parts: partsRef.current },
      text !== savedRef.current,
    );
  }, []);

  /** Runs a document edit and refreshes; edits invalidate the interference report. */
  const refreshRef = useRef(refresh);
  refreshRef.current = refresh;

  const run = useCallback(
    (action: (doc: AssemblyDoc) => Promise<unknown> | unknown, opts?: Parameters<typeof refresh>[0]): Promise<boolean> => {
      const task = queueRef.current.then(async () => {
        const doc = docRef.current;
        if (!doc) return false;
        try {
          await action(doc);
          setError(null);
          setInterference(null);
          setSelectedPair(null);
          await refreshRef.current(opts);
          await takeSnapshot(doc).catch(() => undefined);
          return true;
        } catch (e) {
          if (isCancelled(e) || docRef.current !== doc) return false;
          fail(e);
          try {
            await refreshRef.current(opts);
          } catch {
            /* the state read failed too: keep the error of the action */
          }
          return false;
        }
      });
      queueRef.current = task.catch(() => undefined);
      return task;
    },
    [takeSnapshot],
  );

  const replaceDocument = useCallback((doc: AssemblyDoc, saved: string | null, disposeOld = true) => {
    const old = docRef.current;
    if (old && disposeOld) void old.dispose().catch(() => undefined);
    docRef.current = doc;
    savedRef.current = saved;
    snapshotRef.current = saved;
    meshesRef.current = {};
    setMeshes({});
    setMateSlots([null, null]);
    setHole(null);
    setSelectedComponent(null);
    setInterference(null);
    setExplode({ view: null, progress: 0 });
    setFrameKey((k) => k + 1);
  }, []);

  useEffect(() => {
    let cancelled = false;
    let client: WasmClient<AssemblyKernel> | null = null;
    (async () => {
      client = await loadAssemblyFrom(defaultAssemblyBaseUrl());
      if (cancelled) {
        client.dispose();
        return;
      }
      let doc: AssemblyDoc;
      if (initial) {
        doc = (await client.root.loadDocument(initial.text)).document;
        hostRef.current.onFile?.(initial);
      } else {
        doc = await client.root.createDocument('Сборка');
      }
      if (cancelled) return;
      setKernel(client);
      docRef.current = doc;
      savedRef.current = initial && !initialDirty ? initial.text : null;
      snapshotRef.current = initial?.text ?? (await doc.save());
      setStatus('ready');
    })().catch((e) => {
      if (cancelled) return;
      setStatus('failed');
      fail(e);
    });
    return () => {
      cancelled = true;
      docRef.current = null;
      client?.dispose();
    };
    // The module loads once per editor; `initial` is a mount-time choice.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (status === 'ready') void run(() => undefined);
    // First state after the module loaded.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [status]);

  /**
   * Cancels the running kernel call: the worker is terminated, a new one is started and the
   * document is loaded from the last snapshot (the cancelled change and the undo history are lost).
   */
  const cancelOperation = async () => {
    const old = kernelRef.current;
    if (!old) return;
    old.cancel('прервано пользователем');
    dragRunner.clear();
    setKernel(null);
    setStatus('loading');
    queueRef.current = Promise.resolve();
    const snapshot = snapshotRef.current;
    const saved = savedRef.current;
    try {
      const client = await loadAssemblyFrom(defaultAssemblyBaseUrl());
      for (const p of partsRef.current) await client.root.registerSource(p.name, p.text);
      const doc = snapshot ? (await client.root.loadDocument(snapshot)).document : await client.root.createDocument('Сборка');
      setKernel(client);
      replaceDocument(doc, saved, false);
      snapshotRef.current = snapshot;
      setStatus('ready');
      setNotice(t('busy.cancelled'));
    } catch (e) {
      setStatus('failed');
      setError({ code: 'Cancelled', message: t('busy.restoreFailed', { reason: (e as Error).message }) });
    }
  };

  // ---------------------------------------------------------------- files

  const readFiles = async (list: FileList | null) => {
    const files = list ? [...list] : [];
    return Promise.all(files.map(async (f) => ({ name: f.name, text: await f.text() })));
  };

  const onInsertParts = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = await readFiles(e.target.files);
    e.target.value = '';
    for (const f of files) await insertPart(f.name, f.text);
    setFrameKey((k) => k + 1);
  };

  /** Inserts a part file (bendcore part JSON or STEP); the first part is fixed. */
  const insertPart = (fileName: string, text: string) =>
    run(async (doc) => {
      const step = /\.(step|stp)$/i.test(fileName);
      const first = (await doc.state()).occurrences.length === 0;
      const name = baseName(fileName.replace(/\.bcpart\.json$/i, '.json'));
      const r = await doc.insertPart({ name, format: step ? 'step' : 'part-json', data: text, fixed: first, beside: !first });
      if (!step) partsRef.current = [...partsRef.current.filter((p) => p.definition !== r.definition), { definition: r.definition, name: fileName, text }];
    });

  /** "Open component part": the part JSON the selected component was inserted from. */
  const openComponentPart = () => {
    const occ = state && selectedComponent !== null ? findOccurrence(state.occurrences, selectedComponent) : null;
    if (!occ) {
      setNotice(t('notify.selectComponent'));
      return;
    }
    const source = partsRef.current.find((p) => p.definition === occ.definition);
    if (!source) {
      setNotice(t('notify.componentNotPart', { name: occ.name, format: occ.kind === 'assembly' ? 'assembly' : 'STEP / BRep' }));
      return;
    }
    hostRef.current.onOpenPart?.({ kind: 'part', name: source.name, text: source.text });
  };

  const onInsertSubassembly = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = await readFiles(e.target.files);
    e.target.value = '';
    if (!kernel) return;
    for (const f of files) {
      await run(async (doc) => {
        await kernel.root.registerSource(f.name, f.text);
        const first = (await doc.state()).occurrences.length === 0;
        await doc.insertSubassembly({ source: f.name, name: baseName(f.name), fixed: first, beside: !first });
      });
    }
    setFrameKey((k) => k + 1);
  };

  const onOpen = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = await readFiles(e.target.files);
    e.target.value = '';
    if (!kernel || files.length === 0) return;
    try {
      // Every chosen file is available as a subassembly source; the first one is opened.
      for (const f of files) await kernel.root.registerSource(f.name, f.text);
      const { document: doc, report } = await kernel.root.loadDocument(files[0].text);
      partsRef.current = [];
      replaceDocument(doc, files[0].text);
      await run(() => undefined, { explode: { view: null, progress: 0 } });
      const missing = report.sources.filter((s) => s.status === 'Missing' || s.status === 'AssemblyMissing');
      if (missing.length) setError({ code: 'Источник', message: missing.map((s) => `${s.source}: ${s.message}`).join('; ') });
      else setError(null);
      hostRef.current.onFile?.({ kind: 'assembly', name: files[0].name, text: files[0].text });
    } catch (err) {
      fail(err);
    }
  };

  const onImportStep = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = await readFiles(e.target.files);
    e.target.value = '';
    if (!kernel || files.length === 0) return;
    try {
      partsRef.current = [];
      replaceDocument(await kernel.root.importStep(files[0].text), null);
      setError(null);
      await run(() => undefined, { explode: { view: null, progress: 0 } });
    } catch (err) {
      fail(err);
    }
  };

  const onNew = async () => {
    if (!kernel) return;
    try {
      partsRef.current = [];
      replaceDocument(await kernel.root.createDocument('Сборка'), null);
      setError(null);
      await run(() => undefined, { explode: { view: null, progress: 0 } });
    } catch (e) {
      if (!isCancelled(e)) fail(e);
    }
  };

  const onSave = async () => {
    const doc = docRef.current;
    if (!doc) return;
    try {
      const text = await doc.save();
      const name = `${(await doc.state()).name || 'assembly'}.json`;
      download(name, text, 'application/json');
      savedRef.current = text;
      snapshotRef.current = text;
      const file: DocumentFile = { kind: 'assembly', name, text, parts: partsRef.current };
      hostRef.current.onFile?.(file);
      hostRef.current.onChange?.(file, false);
    } catch (e) {
      if (!isCancelled(e)) fail(e);
    }
  };

  const onExportStep = async () => {
    const doc = docRef.current;
    if (!doc) return;
    try {
      const text = await doc.exportStep();
      download(`${(await doc.state()).name || 'assembly'}.step`, text, 'model/step');
    } catch (e) {
      if (!isCancelled(e)) fail(e);
    }
  };

  // ---------------------------------------------------------------- picking and drag

  const pickedElement = useCallback(
    async (pick: AssemblyPick): Promise<PickedElement | null> => {
      const doc = docRef.current;
      if (!doc || !state) return null;
      try {
        const info = await doc.element({ path: pick.path, kind: pick.kind, index: pick.index });
        return {
          ref: { path: pick.path, kind: pick.kind, index: pick.index },
          geometry: info.geometry,
          entity: info.entity,
          label: refLabel(state, { path: pick.path, kind: pick.kind, index: pick.index }),
        };
      } catch (e) {
        if (!isCancelled(e)) fail(e);
        return null;
      }
    },
    [state],
  );

  const onPick = async (pick: AssemblyPick | null) => {
    if (!pick) {
      setSelectedComponent(null);
      return;
    }
    setSelectedComponent(pick.path[0]);
    const element = await pickedElement(pick);
    if (!element) return;
    if (panel === 'fasteners' || panel === 'pattern') setHole(element);
    else if (panel === 'mate') setMateSlots((s) => nextMateSlots(s, element));
  };

  const onDragStart = async (path: number[], face: number, point: V3): Promise<DragStart> => {
    const doc = docRef.current;
    if (!doc) return false;
    try {
      await queueRef.current;
      const entity = (await doc.element({ path, kind: 'face', index: face })).entity;
      await doc.dragBegin(path[0], point);
      setSelectedComponent(path[0]);
      setError(null);
      return { planeNormal: entity?.type === 'plane' && entity.direction ? (entity.direction as V3) : null };
    } catch (e) {
      if (!isCancelled(e)) fail(e);
      return false;
    }
  };
  /** Drag steps are solved at the rate the worker answers; the newest target wins. */
  const onDragMove = (target: V3) => {
    const doc = docRef.current;
    if (!doc) return;
    dragRunner.schedule(async () => {
      try {
        const r = await doc.dragMove(target);
        setParts(r.placements);
        setDragSolve(r.solve);
      } catch (e) {
        if (!isCancelled(e)) fail(e);
      }
    });
  };
  const onDragEnd = () => {
    void dragRunner.settled().then(() => {
      setDragSolve(null);
      return run((doc) => doc.dragEnd());
    });
  };

  // ---------------------------------------------------------------- keyboard

  useHotkeys(active, {
    undo: () =>
      void run(async (d) => {
        if ((await d.state()).history.canUndo) await d.undo();
      }),
    redo: () =>
      void run(async (d) => {
        if ((await d.state()).history.canRedo) await d.redo();
      }),
    save: () => void onSave(),
    open: () => openInputRef.current?.click(),
    escape: () => {
      setMateSlots([null, null]);
      setHole(null);
      setSelectedComponent(null);
    },
    delete: () => {
      const id = selectedComponent;
      if (id === null) return;
      setSelectedComponent(null);
      setMateSlots([null, null]);
      void run((d) => d.removeOccurrence(id));
    },
    fit: () => setFrameKey((k) => k + 1),
  });

  // ---------------------------------------------------------------- panels

  const switchPanel = (p: PanelId) => {
    setPanel(p);
    if (p === 'bom') {
      docRef.current?.bom(bomMode).then(setBom, (e: unknown) => !isCancelled(e) && fail(e));
    }
    if (explode.view !== null && p !== 'explode') {
      const ex = { view: null, progress: 0 };
      setExplode(ex);
      refresh({ explode: ex, panel: p }).catch((e: unknown) => !isCancelled(e) && fail(e));
    }
  };

  const onExplode = (view: number | null, progress: number) => {
    const ex = { view, progress: view === null ? 0 : clampProgress(progress, state?.explodedViews[view]?.steps.length ?? 0) };
    setExplode(ex);
    refresh({ explode: ex }).catch((e: unknown) => !isCancelled(e) && fail(e));
  };

  const highlights = useMemo(() => {
    const h: Record<string, PartHighlight> = {};
    if (selectedComponent !== null) {
      for (const p of parts) if (p.path[0] === selectedComponent) h[pathKey(p.path)] = 'selected';
    }
    if (interference) {
      interference.pairs.forEach((pair, i) => {
        if (selectedPair !== null && selectedPair !== i) return;
        if (pair.kind === 'touching' && !showTouching && selectedPair === null) return;
        const mark: PartHighlight = pair.kind === 'interference' ? 'interference' : 'touching';
        for (const idx of [pair.a, pair.b]) {
          const k = pathKey(interference.parts[idx].path);
          if (h[k] !== 'interference') h[k] = mark;
        }
      });
    }
    return h;
  }, [selectedComponent, parts, interference, selectedPair, showTouching]);

  const selectedRefs = useMemo(
    () => [mateSlots[0]?.ref, mateSlots[1]?.ref, panel === 'fasteners' || panel === 'pattern' ? hole?.ref : undefined].filter(
      (r): r is NonNullable<typeof r> => !!r,
    ),
    [mateSlots, hole, panel],
  );

  const selectedOccurrence = state && selectedComponent !== null ? findOccurrence(state.occurrences, selectedComponent) : null;
  const selectedDefinition =
    state && selectedOccurrence ? state.definitions.find((d) => d.id === selectedOccurrence.definition) ?? null : null;

  // ---------------------------------------------------------------- test hook

  const pageState = useRef<AssemblyPageState | null>(null);
  pageState.current = {
    status,
    error,
    state,
    panel,
    pickMode,
    mateSlots,
    hole,
    selectedComponent,
    bom,
    interference,
    explode,
    dragSolve,
    parts,
  };
  useEffect(() => {
    window.bendcoreAssembly = {
      getState: () => pageState.current!,
      worldToClient: (x, y, z) => viewportRef.current?.worldToClient(x, y, z) ?? null,
      faceClientPoint: (path, face) => viewportRef.current?.faceClientPoint(path, face) ?? null,
      idle: () => waitIdle(() => [kernelRef.current]),
      faces: async (path) => {
        const doc = docRef.current;
        if (!doc) return [];
        const st = await doc.state();
        let list = st.occurrences;
        let occ: OccurrenceInfo | undefined;
        for (const id of path) {
          occ = list.find((o) => o.id === id);
          list = occ?.children ?? [];
        }
        const def = occ ? st.definitions.find((d) => d.id === occ!.definition) : undefined;
        const out: ElementInfo[] = [];
        for (let i = 0; i < (def?.faces ?? 0); ++i) out.push(await doc.element({ path, kind: 'face', index: i }));
        return out;
      },
    };
    return () => {
      delete window.bendcoreAssembly;
    };
  }, []);

  const solve = dragSolve ?? state?.solve ?? null;
  const history = state?.history;

  return (
    <div className="kv-root pm-root as-root" data-testid="assembly-page">
      <div className="kv-toolbar pm-toolbar">
        <span className="kv-title">bendcore — сборка</span>
        <button type="button" onClick={() => void onNew()} disabled={status !== 'ready'}>
          Новая
        </button>
        <label className="kv-file">
          Открыть…
          <input ref={openInputRef} type="file" accept=".json" multiple onChange={onOpen} data-testid="open-input" />
        </label>
        <button type="button" onClick={() => void onSave()} disabled={status !== 'ready'} data-testid="save" title="Ctrl+S">
          Сохранить
        </button>
        <label className="kv-file">
          Импорт STEP…
          <input type="file" accept=".step,.stp" onChange={onImportStep} data-testid="import-step-input" />
        </label>
        <button type="button" onClick={() => void onExportStep()} disabled={status !== 'ready'} data-testid="export-step">
          Экспорт STEP
        </button>
        <span className="pm-sep" />
        <label className="kv-file">
          Вставить деталь…
          <input type="file" accept=".json,.step,.stp" multiple onChange={onInsertParts} data-testid="insert-part-input" />
        </label>
        {partLibrary && partLibrary.length > 0 && (
          <span className="as-library">
            <select value={libraryChoice} onChange={(e) => setLibraryChoice(e.target.value)} data-testid="library-part">
              <option value="">— деталь из открытых —</option>
              {partLibrary.map((p, i) => (
                <option key={`${p.name}:${i}`} value={String(i)}>
                  {p.name}
                </option>
              ))}
            </select>
            <button
              type="button"
              disabled={status !== 'ready' || libraryChoice === ''}
              data-testid="library-insert"
              onClick={() => {
                const p = partLibrary[Number(libraryChoice)];
                if (p) void insertPart(p.name, p.text).then(() => setFrameKey((k) => k + 1));
              }}
            >
              Вставить
            </button>
          </span>
        )}
        {onOpenPart && (
          <button type="button" disabled={status !== 'ready'} onClick={openComponentPart} data-testid="open-component-part">
            {t('action.openComponentPart')}
          </button>
        )}
        <label className="kv-file">
          Вставить подсборку…
          <input type="file" accept=".json" onChange={onInsertSubassembly} data-testid="insert-subassembly-input" />
        </label>
        <span className="pm-sep" />
        <button
          type="button"
          disabled={!history?.canUndo}
          title={history?.undoLabel ? `Отменить: ${history.undoLabel}` : 'Отменить'}
          onClick={() => void run((d) => d.undo())}
          data-testid="undo"
        >
          Отменить
        </button>
        <button
          type="button"
          disabled={!history?.canRedo}
          title={history?.redoLabel ? `Повторить: ${history.redoLabel}` : 'Повторить'}
          onClick={() => void run((d) => d.redo())}
          data-testid="redo"
        >
          Повторить
        </button>
        <button type="button" onClick={() => void run((d) => d.solve())} disabled={status !== 'ready'} data-testid="solve">
          Решить
        </button>
        <span className="pm-sep" />
        <span className="as-modes" role="radiogroup">
          {(
            [
              ['face', 'Грани'],
              ['edge', 'Рёбра'],
              ['drag', 'Перетаскивание'],
            ] as [AssemblyPickMode, string][]
          ).map(([m, l]) => (
            <label key={m}>
              <input type="radio" name="pick-mode" checked={pickMode === m} onChange={() => setPickMode(m)} data-testid={`mode-${m}`} />
              {l}
            </label>
          ))}
        </span>
        <BusyIndicator client={kernel} onCancel={() => void cancelOperation()} />
        <span className="kv-status" data-testid="status">
          {status === 'loading' && 'загрузка модуля сборки…'}
          {status === 'failed' && 'модуль не загружен'}
          {status === 'ready' && solve && (
            <>
              <span data-testid="total-dof">{dofText(solve.totalDof)}</span>
              {' · '}
              <span className={solve.success ? 'as-ok' : 'as-bad'} data-testid="solve-status">
                {solve.success ? 'сопряжения выполнены' : 'есть противоречия'}
              </span>
            </>
          )}
        </span>
      </div>
      {error && (
        <div className="kv-error" role="alert" data-testid="error">
          {error.code}: {error.message}
        </div>
      )}
      {notice && (
        <div className="app-notice" role="status" data-testid="assembly-notice">
          {notice}
          <button type="button" onClick={() => setNotice(null)} aria-label={t('notify.dismiss')}>
            ×
          </button>
        </div>
      )}
      <div className="kv-main">
        <aside className="pm-left">
          {state && (
            <AssemblyTree
              state={state}
              selectedComponent={selectedComponent}
              onSelectComponent={setSelectedComponent}
              onSetFixed={(id, fixed) => void run((d) => d.setOccurrence(id, { fixed }))}
              onSetSuppressed={(id, suppressed) => void run((d) => d.setOccurrence(id, { suppressed }))}
              onRemoveComponent={(id) => {
                if (selectedComponent === id) setSelectedComponent(null);
                setMateSlots([null, null]);
                void run((d) => d.removeOccurrence(id));
              }}
              onSelectMate={async (m: MateInfo) => {
                const el = (r: MateInfo['a']) =>
                  r.index !== undefined && r.kind && r.kind !== 'vertex'
                    ? pickedElement({ path: r.path, kind: r.kind, index: r.index, point: [0, 0, 0] })
                    : Promise.resolve(null);
                setPanel('mate');
                setMateSlots(await Promise.all([el(m.a), el(m.b)]));
              }}
              onSuppressMate={(id, s) => void run((d) => d.setMateSuppressed(id, s))}
              onRemoveMate={(id) => void run((d) => d.removeMate(id))}
              onRemovePattern={(id) => void run((d) => d.removePattern(id))}
            />
          )}
        </aside>
        <AssemblyViewport
          meshes={meshes}
          parts={parts}
          pickMode={pickMode}
          selected={selectedRefs}
          highlights={highlights}
          onPick={(pick) => void onPick(pick)}
          onDragStart={onDragStart}
          onDragMove={onDragMove}
          onDragEnd={onDragEnd}
          onReady={(h) => {
            viewportRef.current = h;
          }}
          frameKey={frameKey}
        />
        <aside className={`kv-panel pm-right as-right ${panel === 'bom' ? 'as-right-wide' : ''}`}>
          <nav className="as-tabs">
            {PANELS.map((p) => (
              <button
                key={p.id}
                type="button"
                className={panel === p.id ? 'as-tab-active' : ''}
                onClick={() => switchPanel(p.id)}
                data-testid={`tab-${p.id}`}
              >
                {p.label}
              </button>
            ))}
          </nav>
          {panel === 'mate' && (
            <MatePanel
              slots={mateSlots}
              onClearSlot={(i) => setMateSlots((s) => (i === 0 ? [s[1], null] : [s[0], null]))}
              onAdd={(spec: MateSpec) => {
                void run((d) => d.addMate(spec)).then((ok) => ok && setMateSlots([null, null]));
              }}
            />
          )}
          {panel === 'fasteners' && kernel && (
            <FastenerPanel
              kernel={kernel.root}
              hole={hole}
              onClearHole={() => setHole(null)}
              onInsert={(spec) => {
                if (!hole?.entity?.direction || hole.entity.radius === undefined) return;
                const view = viewportRef.current?.viewDirection() ?? [0, 0, -1];
                const insertion = insertionDirection(hole.entity.direction as V3, view);
                void run((d) =>
                    d.insertFasteners({
                      part: hole.ref.path,
                      insertion,
                      radius: hole.entity!.radius!,
                      tolerance: 1e-6,
                      holes: spec.allHoles ? undefined : [hole.ref.index!],
                      screw: spec.screw,
                      headWashers: spec.headWashers,
                      nutWashers: spec.nutWashers,
                      nut: spec.nut,
                    }),
                  ).then((ok) => ok && setHole(null));
              }}
            />
          )}
          {panel === 'interference' && (
            <InterferencePanel
              report={interference}
              showTouching={showTouching}
              selectedPair={selectedPair}
              onCheck={async () => {
                try {
                  await queueRef.current;
                  setInterference((await docRef.current?.interference()) ?? null);
                  setSelectedPair(null);
                  setError(null);
                } catch (e) {
                  if (!isCancelled(e)) fail(e);
                }
              }}
              onShowTouching={setShowTouching}
              onSelectPair={setSelectedPair}
            />
          )}
          {panel === 'bom' && (
            <BomPanel
              bom={bom}
              mode={bomMode}
              onMode={(m) => {
                setBomMode(m);
                docRef.current?.bom(m).then(setBom, (e: unknown) => !isCancelled(e) && fail(e));
              }}
            />
          )}
          {panel === 'explode' && state && (
            <ExplodePanel
              state={state}
              view={explode.view}
              progress={explode.progress}
              selectedComponent={selectedComponent}
              onSelectView={(v) => onExplode(v, 0)}
              onProgress={(p) => onExplode(explode.view, p)}
              onAddView={(view) => {
                let index = -1;
                void run(async (d) => {
                  index = await d.addExplodedView(view);
                }).then((ok) => ok && onExplode(index, 0));
              }}
              onRemoveView={(i) => {
                const ex = { view: null, progress: 0 };
                setExplode(ex);
                void run((d) => d.removeExplodedView(i));
              }}
            />
          )}
          {panel === 'pattern' && (
            <PatternPanel seed={selectedOccurrence} hole={hole} onAdd={(spec) => void run((d) => d.addPattern(spec))} />
          )}
          {panel === 'properties' && (
            <PropertiesPanel
              occurrence={selectedOccurrence}
              definition={selectedDefinition}
              onRename={(name) => selectedOccurrence && void run((d) => d.setOccurrence(selectedOccurrence.id, { name }))}
              onProperties={(p) => selectedDefinition && void run((d) => d.setProperties(selectedDefinition.id, p))}
            />
          )}
        </aside>
      </div>
    </div>
  );
};

export default AssemblyApp;
