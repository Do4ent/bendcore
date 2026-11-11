/**
 * Assembly page (docs/ui/ASSEMBLY_UI.md): the assembly document of bendcore_assembly_wasm with
 * a component/mate tree, mates picked in the viewport, dragging with re-solving, degrees of
 * freedom, interference report with highlighting, BOM, exploded views, fastener library,
 * patterns, undo/redo (Ctrl+Z / Ctrl+Y), JSON and STEP files.
 *
 * Every action is an assembly document call; the geometry shown is the kernel's tessellation of
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
      faces(path: number[]): ElementInfo[];
    };
  }
}

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

const AssemblyApp: React.FC = () => {
  const [kernel, setKernel] = useState<AssemblyKernel | null>(null);
  const docRef = useRef<AssemblyDocumentHandle | null>(null);
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
    (opts: { explode?: { view: number | null; progress: number }; panel?: PanelId; bomMode?: 'flat' | 'structured' } = {}) => {
      const doc = docRef.current;
      if (!doc) return;
      const st = doc.state();
      setState(st);
      const ex = opts.explode ?? explode;
      const view = ex.view !== null && ex.view < st.explodedViews.length ? ex.view : null;
      const tess = doc.tessellate({
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
      if ((opts.panel ?? panel) === 'bom') setBom(doc.bom(opts.bomMode ?? bomMode));
    },
    [explode, panel, bomMode],
  );

  /** Runs a document edit and refreshes; edits invalidate the interference report. */
  const run = useCallback(
    (action: (doc: AssemblyDocumentHandle) => void) => {
      const doc = docRef.current;
      if (!doc) return false;
      try {
        action(doc);
        setError(null);
        setInterference(null);
        setSelectedPair(null);
        refresh();
        return true;
      } catch (e) {
        fail(e);
        try {
          refresh();
        } catch {
          /* the state read failed too: keep the error of the action */
        }
        return false;
      }
    },
    [refresh],
  );

  const replaceDocument = useCallback((doc: AssemblyDocumentHandle) => {
    docRef.current?.dispose();
    docRef.current = doc;
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
    loadAssemblyFrom(defaultAssemblyBaseUrl())
      .then((k) => {
        if (cancelled) return;
        setKernel(k);
        docRef.current = k.createDocument('Сборка');
        setStatus('ready');
      })
      .catch((e) => {
        if (cancelled) return;
        setStatus('failed');
        fail(e);
      });
    return () => {
      cancelled = true;
      docRef.current?.dispose();
      docRef.current = null;
    };
  }, []);

  useEffect(() => {
    if (status === 'ready') refresh();
    // First state after the module loaded.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [status]);

  // ---------------------------------------------------------------- files

  const readFiles = async (list: FileList | null) => {
    const files = list ? [...list] : [];
    return Promise.all(files.map(async (f) => ({ name: f.name, text: await f.text() })));
  };

  const onInsertParts = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = await readFiles(e.target.files);
    e.target.value = '';
    for (const f of files) {
      const step = /\.(step|stp)$/i.test(f.name);
      run((doc) => {
        const first = doc.state().occurrences.length === 0;
        doc.insertPart({ name: baseName(f.name), format: step ? 'step' : 'part-json', data: f.text, fixed: first, beside: !first });
      });
    }
    setFrameKey((k) => k + 1);
  };

  const onInsertSubassembly = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = await readFiles(e.target.files);
    e.target.value = '';
    if (!kernel) return;
    for (const f of files) {
      run((doc) => {
        kernel.registerSource(f.name, f.text);
        const first = doc.state().occurrences.length === 0;
        doc.insertSubassembly({ source: f.name, name: baseName(f.name), fixed: first, beside: !first });
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
      for (const f of files) kernel.registerSource(f.name, f.text);
      const { document: doc, report } = kernel.loadDocument(files[0].text);
      replaceDocument(doc);
      const missing = report.sources.filter((s) => s.status === 'Missing' || s.status === 'AssemblyMissing');
      if (missing.length) setError({ code: 'Источник', message: missing.map((s) => `${s.source}: ${s.message}`).join('; ') });
      else setError(null);
      refresh({ explode: { view: null, progress: 0 } });
    } catch (err) {
      fail(err);
    }
  };

  const onImportStep = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = await readFiles(e.target.files);
    e.target.value = '';
    if (!kernel || files.length === 0) return;
    try {
      replaceDocument(kernel.importStep(files[0].text));
      setError(null);
      refresh({ explode: { view: null, progress: 0 } });
    } catch (err) {
      fail(err);
    }
  };

  const onNew = () => {
    if (!kernel) return;
    replaceDocument(kernel.createDocument('Сборка'));
    setError(null);
    refresh({ explode: { view: null, progress: 0 } });
  };

  const onSave = () => {
    const doc = docRef.current;
    if (!doc) return;
    try {
      download(`${doc.state().name || 'assembly'}.json`, doc.save(), 'application/json');
    } catch (e) {
      fail(e);
    }
  };

  const onExportStep = () => {
    const doc = docRef.current;
    if (!doc) return;
    try {
      download(`${doc.state().name || 'assembly'}.step`, doc.exportStep(), 'model/step');
    } catch (e) {
      fail(e);
    }
  };

  // ---------------------------------------------------------------- picking and drag

  const pickedElement = useCallback(
    (pick: AssemblyPick): PickedElement | null => {
      const doc = docRef.current;
      if (!doc || !state) return null;
      try {
        const info = doc.element({ path: pick.path, kind: pick.kind, index: pick.index });
        return {
          ref: { path: pick.path, kind: pick.kind, index: pick.index },
          geometry: info.geometry,
          entity: info.entity,
          label: refLabel(state, { path: pick.path, kind: pick.kind, index: pick.index }),
        };
      } catch (e) {
        fail(e);
        return null;
      }
    },
    [state],
  );

  const onPick = (pick: AssemblyPick | null) => {
    if (!pick) {
      setSelectedComponent(null);
      return;
    }
    setSelectedComponent(pick.path[0]);
    const element = pickedElement(pick);
    if (!element) return;
    if (panel === 'fasteners' || panel === 'pattern') setHole(element);
    else if (panel === 'mate') setMateSlots((s) => nextMateSlots(s, element));
  };

  const onDragStart = (path: number[], face: number, point: V3): false | { planeNormal: V3 | null } => {
    const doc = docRef.current;
    if (!doc) return false;
    try {
      const entity = doc.element({ path, kind: 'face', index: face }).entity;
      doc.dragBegin(path[0], point);
      setSelectedComponent(path[0]);
      setError(null);
      return { planeNormal: entity?.type === 'plane' && entity.direction ? (entity.direction as V3) : null };
    } catch (e) {
      fail(e);
      return false;
    }
  };
  const onDragMove = (target: V3) => {
    const doc = docRef.current;
    if (!doc) return;
    try {
      const r = doc.dragMove(target);
      setParts(r.placements);
      setDragSolve(r.solve);
    } catch (e) {
      fail(e);
    }
  };
  const onDragEnd = () => {
    setDragSolve(null);
    run((doc) => doc.dragEnd());
  };

  // ---------------------------------------------------------------- keyboard

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const t = e.target as HTMLElement | null;
      if (t && (t.tagName === 'INPUT' || t.tagName === 'SELECT' || t.tagName === 'TEXTAREA')) return;
      if (!(e.ctrlKey || e.metaKey)) return;
      const key = e.key.toLowerCase();
      if (key === 'z' && !e.shiftKey) {
        e.preventDefault();
        if (docRef.current?.state().history.canUndo) run((d) => d.undo());
      } else if (key === 'y' || (key === 'z' && e.shiftKey)) {
        e.preventDefault();
        if (docRef.current?.state().history.canRedo) run((d) => d.redo());
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [run]);

  // ---------------------------------------------------------------- panels

  const switchPanel = (p: PanelId) => {
    setPanel(p);
    if (p === 'bom') {
      try {
        setBom(docRef.current?.bom(bomMode) ?? null);
      } catch (e) {
        fail(e);
      }
    }
    if (explode.view !== null && p !== 'explode') {
      const ex = { view: null, progress: 0 };
      setExplode(ex);
      refresh({ explode: ex, panel: p });
    }
  };

  const onExplode = (view: number | null, progress: number) => {
    const ex = { view, progress: view === null ? 0 : clampProgress(progress, state?.explodedViews[view]?.steps.length ?? 0) };
    setExplode(ex);
    try {
      refresh({ explode: ex });
    } catch (e) {
      fail(e);
    }
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
      faces: (path) => {
        const doc = docRef.current;
        if (!doc) return [];
        const st = doc.state();
        let list = st.occurrences;
        let occ: OccurrenceInfo | undefined;
        for (const id of path) {
          occ = list.find((o) => o.id === id);
          list = occ?.children ?? [];
        }
        const def = occ ? st.definitions.find((d) => d.id === occ!.definition) : undefined;
        const out: ElementInfo[] = [];
        for (let i = 0; i < (def?.faces ?? 0); ++i) out.push(doc.element({ path, kind: 'face', index: i }));
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
        <button type="button" onClick={onNew} disabled={status !== 'ready'}>
          Новая
        </button>
        <label className="kv-file">
          Открыть…
          <input type="file" accept=".json" multiple onChange={onOpen} data-testid="open-input" />
        </label>
        <button type="button" onClick={onSave} disabled={status !== 'ready'} data-testid="save">
          Сохранить
        </button>
        <label className="kv-file">
          Импорт STEP…
          <input type="file" accept=".step,.stp" onChange={onImportStep} data-testid="import-step-input" />
        </label>
        <button type="button" onClick={onExportStep} disabled={status !== 'ready'} data-testid="export-step">
          Экспорт STEP
        </button>
        <span className="pm-sep" />
        <label className="kv-file">
          Вставить деталь…
          <input type="file" accept=".json,.step,.stp" multiple onChange={onInsertParts} data-testid="insert-part-input" />
        </label>
        <label className="kv-file">
          Вставить подсборку…
          <input type="file" accept=".json" onChange={onInsertSubassembly} data-testid="insert-subassembly-input" />
        </label>
        <span className="pm-sep" />
        <button
          type="button"
          disabled={!history?.canUndo}
          title={history?.undoLabel ? `Отменить: ${history.undoLabel}` : 'Отменить'}
          onClick={() => run((d) => d.undo())}
          data-testid="undo"
        >
          Отменить
        </button>
        <button
          type="button"
          disabled={!history?.canRedo}
          title={history?.redoLabel ? `Повторить: ${history.redoLabel}` : 'Повторить'}
          onClick={() => run((d) => d.redo())}
          data-testid="redo"
        >
          Повторить
        </button>
        <button type="button" onClick={() => run((d) => d.solve())} disabled={status !== 'ready'} data-testid="solve">
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
      <div className="kv-main">
        <aside className="pm-left">
          {state && (
            <AssemblyTree
              state={state}
              selectedComponent={selectedComponent}
              onSelectComponent={setSelectedComponent}
              onSetFixed={(id, fixed) => run((d) => d.setOccurrence(id, { fixed }))}
              onSetSuppressed={(id, suppressed) => run((d) => d.setOccurrence(id, { suppressed }))}
              onRemoveComponent={(id) => {
                if (selectedComponent === id) setSelectedComponent(null);
                setMateSlots([null, null]);
                run((d) => d.removeOccurrence(id));
              }}
              onSelectMate={(m: MateInfo) => {
                const el = (r: MateInfo['a']) =>
                  r.index !== undefined && r.kind && r.kind !== 'vertex'
                    ? pickedElement({ path: r.path, kind: r.kind, index: r.index, point: [0, 0, 0] })
                    : null;
                setPanel('mate');
                setMateSlots([el(m.a), el(m.b)]);
              }}
              onSuppressMate={(id, s) => run((d) => d.setMateSuppressed(id, s))}
              onRemoveMate={(id) => run((d) => d.removeMate(id))}
              onRemovePattern={(id) => run((d) => d.removePattern(id))}
            />
          )}
        </aside>
        <AssemblyViewport
          meshes={meshes}
          parts={parts}
          pickMode={pickMode}
          selected={selectedRefs}
          highlights={highlights}
          onPick={onPick}
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
                if (run((d) => d.addMate(spec))) setMateSlots([null, null]);
              }}
            />
          )}
          {panel === 'fasteners' && kernel && (
            <FastenerPanel
              kernel={kernel}
              hole={hole}
              onClearHole={() => setHole(null)}
              onInsert={(spec) => {
                if (!hole?.entity?.direction || hole.entity.radius === undefined) return;
                const view = viewportRef.current?.viewDirection() ?? [0, 0, -1];
                const insertion = insertionDirection(hole.entity.direction as V3, view);
                if (
                  run((d) =>
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
                  )
                )
                  setHole(null);
              }}
            />
          )}
          {panel === 'interference' && (
            <InterferencePanel
              report={interference}
              showTouching={showTouching}
              selectedPair={selectedPair}
              onCheck={() => {
                try {
                  setInterference(docRef.current?.interference() ?? null);
                  setSelectedPair(null);
                  setError(null);
                } catch (e) {
                  fail(e);
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
                try {
                  setBom(docRef.current?.bom(m) ?? null);
                } catch (e) {
                  fail(e);
                }
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
                if (run((d) => (index = d.addExplodedView(view)))) onExplode(index, 0);
              }}
              onRemoveView={(i) => {
                const ex = { view: null, progress: 0 };
                setExplode(ex);
                run((d) => d.removeExplodedView(i));
              }}
            />
          )}
          {panel === 'pattern' && (
            <PatternPanel seed={selectedOccurrence} hole={hole} onAdd={(spec) => run((d) => d.addPattern(spec))} />
          )}
          {panel === 'properties' && (
            <PropertiesPanel
              occurrence={selectedOccurrence}
              definition={selectedDefinition}
              onRename={(name) => selectedOccurrence && run((d) => d.setOccurrence(selectedOccurrence.id, { name }))}
              onProperties={(p) => selectedDefinition && run((d) => d.setProperties(selectedDefinition.id, p))}
            />
          )}
        </aside>
      </div>
    </div>
  );
};

export default AssemblyApp;
