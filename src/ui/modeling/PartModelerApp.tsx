/**
 * Part modeling page (docs/ui/PART_MODELING_UI.md): the part document of bendcore_modeling_wasm
 * with a feature tree, operation panels with expression fields and preview, face/edge picking
 * in the kernel viewport, undo/redo (Ctrl+Z / Ctrl+Y), variables, mass properties and
 * save/open of the "bendcore-part" JSON file.
 *
 * Every action is a part document call; geometry shown is the kernel's tessellation of the
 * document result. window.bendcorePartModeler exposes the page state (read-only) and the view
 * projection for end-to-end tests.
 */

import React, { useCallback, useEffect, useRef, useState } from 'react';
import type { KernelMesh } from '../../wasm/bendcore_kernel_api.mjs';
import type {
  DocumentState,
  FeatureTypeSpec,
  FeatureView,
  ModelingKernel,
  PartDocumentHandle,
  PartProperties,
  SketchData,
  VariableView,
} from '../../wasm/bendcore_modeling_api.mjs';
import KernelViewport from '../kernel/KernelViewport';
import type { EdgePick, FacePick, KernelViewportHandle, PickMode } from '../kernel/KernelViewport';
import FeaturePanel from './FeaturePanel';
import FeatureTree from './FeatureTree';
import SketchEditor from './sketch/SketchEditor';
import {
  FeatureForm,
  existingFeatureForm,
  newFeatureForm,
  toFeatureEdits,
  toFeatureSpec,
  togglePick,
} from './featureForm';
import { FEATURE_LABELS, TOOLBAR_TYPES, label } from './labels';
import { defaultModelingBaseUrl, loadModelingFrom } from './modelingLoader';
import { applySketchDimensions, templateProfiles } from './sketchDimensions';
import type { TreeAction } from './treeActions';
import '../kernel/KernelViewer.css';
import './PartModeler.css';

const MESH_PARAMS = { linearDeflection: 0.05, angularDeflection: 0.25 };

interface PanelState {
  mode: 'add' | 'edit';
  type: string;
  /** Node being edited, or added by a preview; null before the first preview of a new node. */
  featureId: string | null;
  /** Insertion index of a new node. */
  position: number;
  form: FeatureForm;
  activeSlot: string | null;
}

interface Snapshot {
  features: FeatureView[];
  doc: DocumentState | null;
  variables: VariableView[];
  properties: PartProperties | null;
}

interface SelectionInfo {
  kind: 'face' | 'edge';
  index: number;
  name: string;
}

export interface PartModelerState {
  status: 'loading' | 'ready' | 'failed';
  error: { code: string; message: string } | null;
  features: { id: string; type: string; label: string; state: string; messages: string[]; suppressed: boolean }[];
  doc: DocumentState | null;
  properties: PartProperties | null;
  density: number | null;
  panel: { mode: string; type: string; featureId: string | null; activeSlot: string | null; picks: Record<string, string[]> } | null;
  selection: SelectionInfo | null;
  /** Shape shown in the viewport (target string of the document API). */
  shownTarget: string;
  /** Sketch being edited in the sketch editor. */
  sketchMode: { featureId: string; isNew: boolean } | null;
}

export interface PartModelerTestApi {
  getState(): PartModelerState;
  worldToClient(x: number, y: number, z: number): { x: number; y: number };
}

declare global {
  interface Window {
    bendcorePartModeler?: PartModelerTestApi;
  }
}

function describeError(e: unknown): { code: string; message: string } {
  if (e && typeof e === 'object' && 'code' in e && 'detail' in e) {
    const ke = e as { code: string; detail: string };
    return { code: ke.code, message: ke.detail };
  }
  return { code: 'Error', message: e instanceof Error ? e.message : String(e) };
}

const fmt = (v: number | null | undefined, digits = 3) =>
  v === null || v === undefined ? '—' : Number.isFinite(v) ? Number(v.toFixed(digits)).toString() : String(v);
const fmtVec = (v: number[]) => `(${v.map((c) => fmt(c, 2)).join(', ')})`;

/** Shape the viewport shows: the input of the edited node while picking references, else the result. */
function shownTarget(panel: PanelState | null): string {
  if (panel && panel.activeSlot) return panel.featureId ? `before:${panel.featureId}` : 'final';
  return 'final';
}

const PartModelerApp: React.FC = () => {
  const [kernel, setKernel] = useState<ModelingKernel | null>(null);
  const [status, setStatus] = useState<PartModelerState['status']>('loading');
  const [catalog, setCatalog] = useState<FeatureTypeSpec[]>([]);
  const [snapshot, setSnapshot] = useState<Snapshot>({ features: [], doc: null, variables: [], properties: null });
  const [mesh, setMesh] = useState<KernelMesh | null>(null);
  const [overlay, setOverlay] = useState<KernelMesh | null>(null);
  const [panel, setPanelState] = useState<PanelState | null>(null);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [selection, setSelection] = useState<SelectionInfo | null>(null);
  const [error, setError] = useState<PartModelerState['error']>(null);
  const [density, setDensity] = useState('7850');
  const [frameKey, setFrameKey] = useState(0);
  const [fileName, setFileName] = useState('деталь.bcpart.json');
  const [newVar, setNewVar] = useState({ name: '', expression: '' });
  const [sketchMode, setSketchMode] = useState<{ featureId: string; isNew: boolean; body: KernelMesh | null } | null>(null);
  const [sketchPlane, setSketchPlane] = useState('XY');

  const docRef = useRef<PartDocumentHandle | null>(null);
  const panelRef = useRef<PanelState | null>(null);
  const viewRef = useRef<KernelViewportHandle | null>(null);
  const densityRef = useRef(density);
  densityRef.current = density;
  const sketchModeRef = useRef(sketchMode);
  sketchModeRef.current = sketchMode;
  const selectedRef = useRef(selectedId);
  selectedRef.current = selectedId;

  const setPanel = (p: PanelState | null) => {
    panelRef.current = p;
    setPanelState(p);
  };

  const densityValue = (): number | null => {
    const d = Number(densityRef.current);
    return Number.isFinite(d) && d > 0 ? d : null;
  };

  /** Reads everything shown from the document (after every call that changes it). */
  const refresh = useCallback((p: PanelState | null = panelRef.current, selected: string | null = selectedRef.current) => {
    const doc = docRef.current;
    if (!doc) return;
    try {
      const features = doc.features();
      setSnapshot({
        features,
        doc: doc.state(),
        variables: doc.variables(),
        properties: doc.properties('final', densityValue()),
      });
      setMesh(doc.tessellate(shownTarget(p), MESH_PARAMS));
      const sketchId = p?.type === 'Sketch' ? p.featureId : selected;
      const sketch = features.find((f) => f.id === sketchId && f.type === 'Sketch');
      setOverlay(sketch?.sketchResult?.hasFaces ? doc.tessellate(`sketch:${sketch.id}`, MESH_PARAMS) : null);
    } catch (e) {
      setError(describeError(e));
    }
  }, []);

  /** Runs a document call; errors go to the error bar, the view is refreshed either way. */
  const run = useCallback(
    (body: (doc: PartDocumentHandle) => void, p?: PanelState | null | (() => PanelState | null)) => {
      const doc = docRef.current;
      if (!doc) return false;
      setError(null);
      let ok = true;
      try {
        body(doc);
      } catch (e) {
        setError(describeError(e));
        ok = false;
      }
      setSelection(null);
      refresh(p === undefined ? panelRef.current : typeof p === 'function' ? p() : p);
      return ok;
    },
    [refresh],
  );

  const replaceDocument = useCallback(
    (doc: PartDocumentHandle) => {
      docRef.current?.dispose();
      docRef.current = doc;
      setPanel(null);
      setSelectedId(null);
      setSelection(null);
      setFrameKey((k) => k + 1);
      refresh(null, null);
    },
    [refresh],
  );

  useEffect(() => {
    let cancelled = false;
    loadModelingFrom(defaultModelingBaseUrl())
      .then((k) => {
        if (cancelled) return;
        setKernel(k);
        setCatalog(k.catalog());
        docRef.current = k.createDocument();
        setStatus('ready');
        refresh(null, null);
      })
      .catch((e: unknown) => {
        if (cancelled) return;
        setStatus('failed');
        setError(describeError(e));
      });
    return () => {
      cancelled = true;
      docRef.current?.dispose();
      docRef.current = null;
    };
  }, [refresh]);

  // ------------------------------------------------------------- panel

  const specOf = (type: string) => catalog.find((c) => c.type === type);

  const openAdd = (type: string) => {
    const spec = specOf(type);
    const doc = docRef.current;
    if (!spec || !doc || panelRef.current) return;
    const position = snapshot.doc?.rollback ?? snapshot.features.length;
    const form = newFeatureForm(spec, snapshot.features, position);
    const firstSlot = spec.references.find((r) => !r.optional && r.kind !== 'vertex');
    const p: PanelState = { mode: 'add', type, featureId: null, position, form, activeSlot: firstSlot?.name ?? null };
    if (run((d) => d.beginTransaction(`Добавить: ${label(FEATURE_LABELS, type)}`), p)) setPanel(p);
  };

  const openEdit = (feature: FeatureView) => {
    const spec = specOf(feature.type);
    if (!spec || panelRef.current) return;
    const p: PanelState = {
      mode: 'edit',
      type: feature.type,
      featureId: feature.id,
      position: feature.index,
      form: existingFeatureForm(spec, feature),
      activeSlot: null,
    };
    if (run((d) => d.beginTransaction(`Изменить: ${feature.label}`), p)) setPanel(p);
  };

  /** Applies the form to the document (inside the panel's transaction) and rebuilds. */
  const applyPanel = (p: PanelState): PanelState | null => {
    const doc = docRef.current;
    if (!doc || !kernel) return null;
    let next: PanelState | null = null;
    run((d) => {
      let sketch;
      if (p.form.template) sketch = kernel.sketchTemplate(templateProfiles(p.form.template));
      if (p.form.dimensions && p.featureId) {
        const current = d.feature(p.featureId).sketch;
        if (current) sketch = applySketchDimensions(current, p.form.dimensions);
      }
      let id = p.featureId;
      if (id === null) id = d.addFeature(toFeatureSpec(p.form, sketch), p.position);
      else d.editFeature(id, toFeatureEdits(p.form, sketch));
      d.rebuild();
      next = { ...p, featureId: id, activeSlot: null };
    }, () => next ?? { ...p, activeSlot: null });
    return next;
  };

  const preview = () => {
    const p = panelRef.current;
    if (!p) return;
    const next = applyPanel(p);
    if (next) setPanel(next);
  };

  const confirm = () => {
    const p = panelRef.current;
    if (!p) return;
    const next = applyPanel(p);
    if (!next) return;
    if (run((d) => d.commitTransaction(), null)) setPanel(null);
  };

  const cancel = () => {
    if (!panelRef.current) return;
    run((d) => {
      d.abortTransaction();
      d.rebuild();
    }, null);
    setPanel(null);
  };

  const changeForm = (form: FeatureForm) => {
    const p = panelRef.current;
    if (p) setPanel({ ...p, form });
  };

  const activateSlot = (slot: string | null) => {
    const p = panelRef.current;
    if (!p) return;
    const next = { ...p, activeSlot: slot };
    setPanel(next);
    refresh(next);
  };

  // ------------------------------------------------------------- picking

  const activeSlotSpec = () => {
    const p = panelRef.current;
    if (!p || !p.activeSlot) return null;
    return specOf(p.type)?.references.find((r) => r.name === p.activeSlot) ?? null;
  };

  const pickElement = (kind: 'face' | 'edge', index: number | null) => {
    const doc = docRef.current;
    if (!doc) return;
    const p = panelRef.current;
    const slot = activeSlotSpec();
    try {
      if (p && slot) {
        if (index === null || slot.kind !== kind) return;
        const ref = doc.makeReference(shownTarget(p), kind, index);
        const picks = togglePick(p.form.references[slot.name] ?? [], { ref, index }, slot.multiple);
        setPanel({ ...p, form: { ...p.form, references: { ...p.form.references, [slot.name]: picks } } });
        return;
      }
      setSelection(index === null ? null : { kind, index, name: doc.element('final', kind, index).name });
    } catch (e) {
      setError(describeError(e));
    }
  };

  const onPickFace = useCallback((pick: FacePick | null) => pickElement('face', pick ? pick.faceIndex : null), [catalog, kernel]); // eslint-disable-line react-hooks/exhaustive-deps
  const onPickEdge = useCallback((pick: EdgePick | null) => pickElement('edge', pick ? pick.edgeIndex : null), [catalog, kernel]); // eslint-disable-line react-hooks/exhaustive-deps

  // ------------------------------------------------------------- tree, undo, files

  const treeAction = (f: FeatureView, action: TreeAction) => {
    switch (action) {
      case 'edit':
        openEdit(f);
        return;
      case 'editSketch':
        editSketch(f);
        return;
      case 'suppress':
      case 'unsuppress':
        run((d) => {
          d.setSuppressed(f.id, action === 'suppress');
          d.rebuild();
        });
        return;
      case 'remove':
        if (selectedId === f.id) setSelectedId(null);
        run((d) => {
          d.removeFeature(f.id);
          d.rebuild();
        });
        return;
      case 'moveUp':
      case 'moveDown':
        run((d) => {
          d.moveFeature(f.id, f.index + (action === 'moveUp' ? -1 : 1));
          d.rebuild();
        });
        return;
      case 'rollbackHere':
        run((d) => {
          d.setRollback(f.index + 1);
          d.rebuild();
        });
        return;
    }
  };

  const undo = useCallback(() => {
    if (panelRef.current || sketchModeRef.current || !docRef.current?.state().canUndo) return;
    run((d) => d.undo());
  }, [run]);

  const redo = useCallback(() => {
    if (panelRef.current || sketchModeRef.current || !docRef.current?.state().canRedo) return;
    run((d) => d.redo());
  }, [run]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (!(e.ctrlKey || e.metaKey)) return;
      const target = e.target as HTMLElement | null;
      if (target && ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName)) return;
      const key = e.key.toLowerCase();
      if (key === 'z' && !e.shiftKey) {
        e.preventDefault();
        undo();
      } else if (key === 'y' || (key === 'z' && e.shiftKey)) {
        e.preventDefault();
        redo();
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [undo, redo]);

  const newDocument = () => {
    if (!kernel || panelRef.current) return;
    setError(null);
    replaceDocument(kernel.createDocument());
    setFileName('деталь.bcpart.json');
  };

  const openFile = async (file: File) => {
    if (!kernel || panelRef.current) return;
    setError(null);
    try {
      const { document } = kernel.loadDocument(await file.text());
      replaceDocument(document);
      setFileName(file.name);
    } catch (e) {
      setError(describeError(e));
    }
  };

  const saveFile = () => {
    const doc = docRef.current;
    if (!doc) return;
    try {
      const blob = new Blob([doc.save()], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = fileName;
      document.body.appendChild(a);
      a.click();
      a.remove();
      URL.revokeObjectURL(url);
    } catch (e) {
      setError(describeError(e));
    }
  };

  const selectNode = (id: string | null) => {
    setSelectedId(id);
    refresh(panelRef.current, id);
  };

  // ------------------------------------------------------------- sketch editor

  /** Opens the sketch editor on node `id` inside a transaction (docs/ui/SKETCH_EDITOR.md). */
  const enterSketch = (id: string, isNew: boolean) => {
    const doc = docRef.current;
    if (!doc) return;
    let body: KernelMesh | null = null;
    try {
      body = doc.tessellate(`before:${id}`, MESH_PARAMS);
    } catch (e) {
      setError(describeError(e));
    }
    setSketchMode({ featureId: id, isNew, body });
  };

  const newSketch = () => {
    const doc = docRef.current;
    if (!doc || panelRef.current || sketchMode) return;
    const position = snapshot.doc?.rollback ?? snapshot.features.length;
    let id: string | null = null;
    run((d) => {
      d.beginTransaction('Эскиз');
      const spec: Parameters<PartDocumentHandle['addFeature']>[0] = {
        type: 'Sketch',
        options: { plane: sketchPlane },
        sketch: { points: [], geometry: [], constraints: [], regions: [] },
      };
      if (sketchPlane === 'face') {
        if (selection?.kind !== 'face') throw new Error('выберите плоскую грань во вьюпорте');
        spec.references = { face: [d.makeReference('final', 'face', selection.index)] };
      }
      try {
        id = d.addFeature(spec, position);
        d.rebuild();
      } catch (e) {
        d.abortTransaction();
        d.rebuild();
        throw e;
      }
    }, null);
    if (id) enterSketch(id, true);
  };

  const editSketch = (f: FeatureView) => {
    if (panelRef.current || sketchMode) return;
    if (run((d) => d.beginTransaction(`Изменить эскиз: ${f.label}`), null)) enterSketch(f.id, false);
  };

  const exitSketch = (data: SketchData) => {
    const mode = sketchMode;
    if (!mode) return;
    run((d) => {
      d.editFeature(mode.featureId, { sketch: data });
      d.rebuild();
      d.commitTransaction();
    }, null);
    setSketchMode(null);
    setSelectedId(mode.featureId);
    refresh(null, mode.featureId);
  };

  const cancelSketch = () => {
    run((d) => {
      d.abortTransaction();
      d.rebuild();
    }, null);
    setSketchMode(null);
  };

  // ------------------------------------------------------------- test API

  const stateRef = useRef<PartModelerState | null>(null);
  stateRef.current = {
    status,
    error,
    features: snapshot.features.map((f) => ({
      id: f.id,
      type: f.type,
      label: f.label,
      state: f.state,
      messages: f.messages,
      suppressed: f.suppressed,
    })),
    doc: snapshot.doc,
    properties: snapshot.properties,
    density: densityValue(),
    panel: panel && {
      mode: panel.mode,
      type: panel.type,
      featureId: panel.featureId,
      activeSlot: panel.activeSlot,
      picks: Object.fromEntries(Object.entries(panel.form.references).map(([s, l]) => [s, l.map((p) => p.ref.name)])),
    },
    selection,
    shownTarget: shownTarget(panel),
    sketchMode: sketchMode && { featureId: sketchMode.featureId, isNew: sketchMode.isNew },
  };
  useEffect(() => {
    window.bendcorePartModeler = {
      getState: () => stateRef.current!,
      worldToClient: (x, y, z) => {
        if (!viewRef.current) throw new Error('viewport is not mounted');
        return viewRef.current.worldToClient(x, y, z);
      },
    };
    return () => {
      delete window.bendcorePartModeler;
    };
  }, []);

  // ------------------------------------------------------------- render

  const slot = panel ? activeSlotSpec() : null;
  const pickMode: PickMode = slot ? (slot.kind === 'edge' ? 'edge' : 'face') : 'face';
  const slotPicks = panel && panel.activeSlot ? panel.form.references[panel.activeSlot] ?? [] : [];
  const pickedIndices = slotPicks.map((p) => p.index).filter((i): i is number => i !== null);
  const props = snapshot.properties;
  const docState = snapshot.doc;
  const busyPanel = panel !== null || sketchMode !== null;
  const panelSpec = panel ? specOf(panel.type) : undefined;
  const panelView = panel?.featureId ? snapshot.features.find((f) => f.id === panel.featureId) ?? null : null;

  return (
    <div className="kv-root pm-root">
      <header className="kv-toolbar pm-toolbar">
        <span className="kv-title">bendcore · деталь</span>
        <button type="button" onClick={newDocument} disabled={status !== 'ready' || busyPanel} data-testid="doc-new">
          Новая
        </button>
        <label className="kv-file">
          <input
            type="file"
            accept=".json,application/json"
            data-testid="doc-open"
            disabled={status !== 'ready' || busyPanel}
            onChange={(e) => {
              const file = e.target.files?.[0];
              if (file) void openFile(file);
              e.target.value = '';
            }}
          />
          Открыть…
        </label>
        <button type="button" onClick={saveFile} disabled={status !== 'ready'} data-testid="doc-save">
          Сохранить
        </button>
        <span className="pm-sep" />
        <button
          type="button"
          onClick={undo}
          disabled={busyPanel || !docState?.canUndo}
          data-testid="undo"
          title={docState?.undoHistory.at(-1)?.name ? `Отменить: ${docState.undoHistory.at(-1)!.name} (Ctrl+Z)` : 'Ctrl+Z'}
        >
          Отменить
        </button>
        <button
          type="button"
          onClick={redo}
          disabled={busyPanel || !docState?.canRedo}
          data-testid="redo"
          title={docState?.redoHistory.at(-1)?.name ? `Повторить: ${docState.redoHistory.at(-1)!.name} (Ctrl+Y)` : 'Ctrl+Y'}
        >
          Повторить
        </button>
        <span className="pm-sep" />
        <select
          value={sketchPlane}
          data-testid="sketch-plane"
          title="Плоскость нового эскиза"
          disabled={status !== 'ready' || busyPanel}
          onChange={(e) => setSketchPlane(e.target.value)}
        >
          <option value="XY">XY</option>
          <option value="XZ">XZ</option>
          <option value="YZ">YZ</option>
          <option value="face">выбранная грань</option>
        </select>
        <button type="button" data-testid="sketch-draw" disabled={status !== 'ready' || busyPanel} onClick={newSketch}>
          Нарисовать эскиз
        </button>
        <span className="pm-sep" />
        {TOOLBAR_TYPES.filter((t) => specOf(t)).map((t) => (
          <button
            type="button"
            key={t}
            data-testid={`add-${t}`}
            disabled={status !== 'ready' || busyPanel}
            onClick={() => openAdd(t)}
          >
            {label(FEATURE_LABELS, t)}
          </button>
        ))}
        <span className="kv-status" data-testid="modeling-status">
          {status === 'loading' && 'Загрузка ядра…'}
          {status === 'ready' && 'Ядро готово'}
          {status === 'failed' && 'Ядро не загружено'}
        </span>
      </header>
      {error && (
        <div className="kv-error" role="alert" data-testid="modeling-error">
          {error.code}: {error.message}
        </div>
      )}
      <main className="pm-main">
        <aside className="pm-left">
          <FeatureTree
            features={snapshot.features}
            rollback={docState?.rollback ?? null}
            selectedId={selectedId}
            disabled={busyPanel}
            onSelect={selectNode}
            onAction={treeAction}
            onClearRollback={() =>
              run((d) => {
                d.setRollback(null);
                d.rebuild();
              })
            }
          />
          <section className="pm-variables">
            <h2>Переменные</h2>
            <table>
              <tbody>
                {snapshot.variables.map((v) => (
                  <tr key={v.name} data-testid={`var-${v.name}`}>
                    <td>{v.name}</td>
                    <td>
                      <input
                        defaultValue={v.expression}
                        key={`${v.name}=${v.expression}`}
                        disabled={busyPanel}
                        onBlur={(e) => {
                          const expr = e.target.value;
                          if (expr !== v.expression) {
                            run((d) => {
                              d.setVariable(v.name, expr);
                              d.rebuild();
                            });
                          }
                        }}
                      />
                    </td>
                    <td title={v.error ?? ''}>{v.error ? '!' : `= ${fmt(v.value, 4)}`}</td>
                    <td>
                      <button
                        type="button"
                        aria-label={`удалить ${v.name}`}
                        disabled={busyPanel}
                        onClick={() =>
                          run((d) => {
                            d.removeVariable(v.name);
                            d.rebuild();
                          })
                        }
                      >
                        ×
                      </button>
                    </td>
                  </tr>
                ))}
                <tr>
                  <td>
                    <input
                      placeholder="имя"
                      value={newVar.name}
                      data-testid="var-new-name"
                      disabled={busyPanel}
                      onChange={(e) => setNewVar({ ...newVar, name: e.target.value })}
                    />
                  </td>
                  <td>
                    <input
                      placeholder="выражение"
                      value={newVar.expression}
                      data-testid="var-new-expression"
                      disabled={busyPanel}
                      onChange={(e) => setNewVar({ ...newVar, expression: e.target.value })}
                    />
                  </td>
                  <td colSpan={2}>
                    <button
                      type="button"
                      data-testid="var-add"
                      disabled={busyPanel || !newVar.name.trim() || !newVar.expression.trim()}
                      onClick={() => {
                        if (
                          run((d) => {
                            d.setVariable(newVar.name.trim(), newVar.expression.trim());
                            d.rebuild();
                          })
                        ) {
                          setNewVar({ name: '', expression: '' });
                        }
                      }}
                    >
                      +
                    </button>
                  </td>
                </tr>
              </tbody>
            </table>
          </section>
        </aside>
        {sketchMode && docRef.current ? (
          <SketchEditor
            doc={docRef.current}
            featureId={sketchMode.featureId}
            body={sketchMode.body}
            onExit={exitSketch}
            onCancel={cancelSketch}
          />
        ) : (
          <>
          <KernelViewport
            mesh={mesh}
            selectedFace={pickMode === 'face' && !slot && selection?.kind === 'face' ? selection.index : null}
            selectedFaces={slot && slot.kind === 'face' ? pickedIndices : []}
            selectedEdges={
              slot && slot.kind === 'edge' ? pickedIndices : selection?.kind === 'edge' ? [selection.index] : []
            }
            pickMode={pickMode}
            onPick={onPickFace}
            onPickEdge={onPickEdge}
            overlay={overlay}
            autoFrame="first"
            frameKey={frameKey}
            onReady={(h) => {
              viewRef.current = h;
            }}
          />
          <aside className="pm-right kv-panel">
            {panel && panelSpec ? (
              <FeaturePanel
                mode={panel.mode}
                spec={panelSpec}
                form={panel.form}
                view={panelView}
                features={snapshot.features}
                position={panel.position}
                activeSlot={panel.activeSlot}
                onChange={changeForm}
                onActivateSlot={activateSlot}
                onPreview={preview}
                onOk={confirm}
                onCancel={cancel}
              />
            ) : (
              <p className="pm-hint">Выберите операцию на панели инструментов или узел дерева.</p>
            )}
            <section>
              <h2>Свойства</h2>
              <dl data-testid="properties">
                <dt>Объём, мм³</dt>
                <dd data-testid="prop-volume" data-value={props?.volume ?? ''}>
                  {props?.volume === null || props?.volume === undefined ? 'нет тела' : fmt(props.volume)}
                </dd>
                <dt>Плотность, кг/м³</dt>
                <dd>
                  <input
                    value={density}
                    data-testid="density"
                    onChange={(e) => setDensity(e.target.value)}
                    onBlur={() => refresh()}
                  />
                </dd>
                <dt>Масса, кг</dt>
                <dd data-testid="prop-mass" data-value={props?.mass ?? ''}>
                  {fmt(props?.mass, 4)}
                </dd>
                <dt>Площадь, мм²</dt>
                <dd>{fmt(props?.area)}</dd>
                <dt>Центр масс</dt>
                <dd>{props?.centerOfMass ? fmtVec(props.centerOfMass) : '—'}</dd>
                <dt>Габарит</dt>
                <dd>{props?.boundingBox ? `${fmtVec(props.boundingBox.min)} – ${fmtVec(props.boundingBox.max)}` : '—'}</dd>
                <dt>Грани / рёбра</dt>
                <dd>{props ? `${props.counts.faces} / ${props.counts.edges}` : '—'}</dd>
                {selection && (
                  <>
                    <dt>{selection.kind === 'face' ? 'Грань' : 'Ребро'}</dt>
                    <dd data-testid="selection-name">
                      <code>{selection.name}</code>
                    </dd>
                  </>
                )}
              </dl>
            </section>
          </aside>
          </>
        )}
      </main>
    </div>
  );
};

export default PartModelerApp;
