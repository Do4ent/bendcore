/**
 * Part modeling page (docs/ui/PART_MODELING_UI.md): the part document of bendcore_modeling_wasm
 * with a feature tree, operation panels with expression fields and preview, face/edge picking
 * in the kernel viewport, undo/redo (Ctrl+Z / Ctrl+Y), variables, mass properties and
 * save/open of the "bendcore-part" JSON file.
 *
 * Every action is a part document call, executed in the WASM worker (docs/ui/APP_SHELL.md);
 * geometry shown is the kernel's tessellation of the document result. A long call shows the
 * progress indicator and can be cancelled: the worker is restarted and the document is reloaded
 * from the snapshot taken after the last completed change outside a transaction. window.bendcorePartModeler exposes the page state (read-only) and the view
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
import { toolbarTypes } from './labels';
import { defaultModelingBaseUrl, loadModelingFrom } from './modelingLoader';
import type { Remote, WasmClient } from '../worker/wasmClient';
import { isCancelled } from '../worker/wasmClient';
import { waitIdle } from '../worker/idle';
import BusyIndicator from '../app/BusyIndicator';
import { useHotkeys } from '../app/hotkeys';
import { caption, useI18n } from '../app/i18n';
import { downloadText } from '../app/editorHost';
import type { DocumentFile, EditorHostProps } from '../app/editorHost';
import { useFaultReport } from '../app/faultReport';
import { applySketchDimensions, pathSketch, templateProfiles } from './sketchDimensions';
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
  /** Resolves when no kernel call is in flight and the page has rendered the last result. */
  idle(): Promise<void>;
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

type PartDoc = Remote<PartDocumentHandle>;


const PartModelerApp: React.FC<EditorHostProps> = ({ active = true, initial = null, initialDirty = false, onChange, onFile, onCreateDrawing, onFault }) => {
  const { t } = useI18n();
  const [kernel, setKernel] = useState<WasmClient<ModelingKernel> | null>(null);
  useFaultReport(kernel as WasmClient<unknown> | null, onFault);
  const [notice, setNotice] = useState<string | null>(null);
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
  const [fileName, setFileName] = useState(() => initial?.name ?? t('part.newFile'));
  const [newVar, setNewVar] = useState({ name: '', expression: '' });
  const [sketchMode, setSketchMode] = useState<{ featureId: string; isNew: boolean; body: KernelMesh | null } | null>(null);
  const [sketchPlane, setSketchPlane] = useState('XY');

  const docRef = useRef<PartDoc | null>(null);
  const kernelRef = useRef(kernel);
  kernelRef.current = kernel;
  /** Serialises document bodies: one runs at a time, in click order. */
  const queueRef = useRef<Promise<unknown>>(Promise.resolve());
  /** Document JSON after the last completed change outside a transaction (cancel restores it). */
  const snapshotRef = useRef<string | null>(null);
  /** JSON of the last saved or opened file (dirty = snapshot differs). */
  const savedRef = useRef<string | null>(null);
  const fileNameRef = useRef(fileName);
  fileNameRef.current = fileName;
  const hostRef = useRef({ onChange, onFile, onCreateDrawing });
  hostRef.current = { onChange, onFile, onCreateDrawing };
  const openInputRef = useRef<HTMLInputElement | null>(null);
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
  const refresh = useCallback(async (p: PanelState | null = panelRef.current, selected: string | null = selectedRef.current) => {
    const doc = docRef.current;
    if (!doc) return;
    try {
      const [features, state, variables, properties, shown] = await Promise.all([
        doc.features(),
        doc.state(),
        doc.variables(),
        doc.properties('final', densityValue()),
        doc.tessellate(shownTarget(p), MESH_PARAMS),
      ]);
      const sketchId = p?.type === 'Sketch' ? p.featureId : selected;
      const sketch = features.find((f) => f.id === sketchId && f.type === 'Sketch');
      const sketchMesh = sketch?.sketchResult?.hasFaces ? await doc.tessellate(`sketch:${sketch.id}`, MESH_PARAMS) : null;
      if (docRef.current !== doc) return;
      setSnapshot({ features, doc: state, variables, properties });
      setMesh(shown);
      setOverlay(sketchMesh);
    } catch (e) {
      if (!isCancelled(e)) setError(describeError(e));
    }
  }, []);

  /** Takes the snapshot of the document when no transaction is open and reports it as a draft. */
  const takeSnapshot = useCallback(async (doc: PartDoc) => {
    if ((await doc.state()).inTransaction) return;
    const text = await doc.save();
    if (docRef.current !== doc) return;
    snapshotRef.current = text;
    hostRef.current.onChange?.({ kind: 'part', name: fileNameRef.current, text }, text !== savedRef.current);
  }, []);

  /** Runs a document call; errors go to the error bar, the view is refreshed either way. */
  const run = useCallback(
    (body: (doc: PartDoc) => Promise<void> | void, p?: PanelState | null | (() => PanelState | null)): Promise<boolean> => {
      const task = queueRef.current.then(async () => {
        const doc = docRef.current;
        if (!doc) return false;
        setError(null);
        let ok = true;
        try {
          await body(doc);
        } catch (e) {
          if (isCancelled(e) || docRef.current !== doc) return false;
          setError(describeError(e));
          ok = false;
        }
        setSelection(null);
        await refresh(p === undefined ? panelRef.current : typeof p === 'function' ? p() : p);
        if (ok) await takeSnapshot(doc).catch(() => undefined);
        return ok;
      });
      queueRef.current = task.catch(() => undefined);
      return task;
    },
    [refresh, takeSnapshot],
  );

  const replaceDocument = useCallback(
    async (doc: PartDoc, saved: string | null, disposeOld = true) => {
      const old = docRef.current;
      docRef.current = doc;
      if (old && disposeOld) void old.dispose().catch(() => undefined);
      savedRef.current = saved;
      snapshotRef.current = null;
      setPanel(null);
      setSketchMode(null);
      setSelectedId(null);
      setSelection(null);
      setFrameKey((k) => k + 1);
      await refresh(null, null);
      await takeSnapshot(doc).catch(() => undefined);
    },
    [refresh, takeSnapshot],
  );

  useEffect(() => {
    let cancelled = false;
    let client: WasmClient<ModelingKernel> | null = null;
    (async () => {
      client = await loadModelingFrom(defaultModelingBaseUrl());
      if (cancelled) {
        client.dispose();
        return;
      }
      const cat = await client.root.catalog();
      let doc: PartDoc;
      if (initial) {
        doc = (await client.root.loadDocument(initial.text)).document;
        onFile?.(initial);
      } else {
        doc = await client.root.createDocument();
      }
      if (cancelled) return;
      setKernel(client);
      setCatalog(cat);
      setStatus('ready');
      await replaceDocument(doc, initial && !initialDirty ? initial.text : null);
      // A restored draft keeps its "modified" state: the saved file is unknown.
      if (initial && !initialDirty) savedRef.current = snapshotRef.current;
    })().catch((e: unknown) => {
      if (cancelled) return;
      setStatus('failed');
      setError(describeError(e));
    });
    return () => {
      cancelled = true;
      docRef.current = null;
      client?.dispose();
    };
    // The kernel loads once per editor; `initial` is a mount-time choice.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  /**
   * Cancels the running kernel call: the worker is terminated (its documents are lost), a new
   * worker is started and the document is loaded from the last snapshot. What is lost: the
   * cancelled change, changes of an open panel or sketch (their transaction), the undo history.
   */
  const cancelOperation = useCallback(async () => {
    const old = kernelRef.current;
    if (!old) return;
    old.cancel(t('part.cancelledByUser'));
    setKernel(null);
    setStatus('loading');
    const snapshot = snapshotRef.current;
    const saved = savedRef.current;
    queueRef.current = Promise.resolve();
    try {
      const client = await loadModelingFrom(defaultModelingBaseUrl());
      const doc = snapshot ? (await client.root.loadDocument(snapshot)).document : await client.root.createDocument();
      setKernel(client);
      setStatus('ready');
      await replaceDocument(doc, saved, false);
      savedRef.current = saved;
      setNotice(t('busy.cancelled'));
    } catch (e) {
      setStatus('failed');
      setError({ code: 'Cancelled', message: t('busy.restoreFailed', { reason: describeError(e).message }) });
    }
  }, [replaceDocument, t]);

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
    void run((d) => d.beginTransaction(t('part.tx.add', { feature: caption(t, 'part.feature.', type) })), p).then((ok) => ok && setPanel(p));
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
    void run((d) => d.beginTransaction(t('part.tx.edit', { feature: feature.label })), p).then((ok) => ok && setPanel(p));
  };

  /** Applies the form to the document (inside the panel's transaction) and rebuilds. */
  const applyPanel = async (p: PanelState): Promise<PanelState | null> => {
    const doc = docRef.current;
    if (!doc || !kernel) return null;
    let next: PanelState | null = null;
    await run(async (d) => {
      let sketch;
      if (p.form.template) {
        sketch =
          p.form.template.kind === 'path'
            ? pathSketch(p.form.template.path, t)
            : await kernel.root.sketchTemplate(templateProfiles(p.form.template, t));
      }
      if (p.form.dimensions && p.featureId) {
        const current = (await d.feature(p.featureId)).sketch;
        if (current) sketch = applySketchDimensions(current, p.form.dimensions);
      }
      let id = p.featureId;
      if (id === null) id = await d.addFeature(toFeatureSpec(p.form, sketch), p.position);
      else await d.editFeature(id, toFeatureEdits(p.form, sketch));
      await d.rebuild();
      next = { ...p, featureId: id, activeSlot: null };
    }, () => next ?? { ...p, activeSlot: null });
    return next;
  };

  const preview = async () => {
    const p = panelRef.current;
    if (!p) return;
    const next = await applyPanel(p);
    if (next && panelRef.current) setPanel(next);
  };

  const confirm = async () => {
    const p = panelRef.current;
    if (!p) return;
    const next = await applyPanel(p);
    if (!next || !panelRef.current) return;
    if (await run((d) => d.commitTransaction(), null)) setPanel(null);
  };

  const cancel = () => {
    if (!panelRef.current) return;
    setPanel(null);
    void run(async (d) => {
      await d.abortTransaction();
      await d.rebuild();
    }, null);
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
    void refresh(next);
  };

  // ------------------------------------------------------------- picking

  const activeSlotSpec = () => {
    const p = panelRef.current;
    if (!p || !p.activeSlot) return null;
    return specOf(p.type)?.references.find((r) => r.name === p.activeSlot) ?? null;
  };

  const pickElement = async (kind: 'face' | 'edge', index: number | null) => {
    const doc = docRef.current;
    if (!doc) return;
    const p = panelRef.current;
    const slot = activeSlotSpec();
    try {
      if (p && slot) {
        if (index === null || slot.kind !== kind) return;
        const ref = await doc.makeReference(shownTarget(p), kind, index);
        const current = panelRef.current;
        if (!current || current.activeSlot !== slot.name) return;
        const picks = togglePick(current.form.references[slot.name] ?? [], { ref, index }, slot.multiple);
        setPanel({ ...current, form: { ...current.form, references: { ...current.form.references, [slot.name]: picks } } });
        return;
      }
      setSelection(index === null ? null : { kind, index, name: (await doc.element('final', kind, index)).name });
    } catch (e) {
      if (!isCancelled(e)) setError(describeError(e));
    }
  };

  const onPickFace = useCallback((pick: FacePick | null) => void pickElement('face', pick ? pick.faceIndex : null), [catalog, kernel]); // eslint-disable-line react-hooks/exhaustive-deps
  const onPickEdge = useCallback((pick: EdgePick | null) => void pickElement('edge', pick ? pick.edgeIndex : null), [catalog, kernel]); // eslint-disable-line react-hooks/exhaustive-deps

  // ------------------------------------------------------------- tree, undo, files

  const treeAction = (f: FeatureView, action: TreeAction) => {
    switch (action) {
      case 'edit':
        openEdit(f);
        return;
      case 'editSketch':
        void editSketch(f);
        return;
      case 'suppress':
      case 'unsuppress':
        void run(async (d) => {
          await d.setSuppressed(f.id, action === 'suppress');
          await d.rebuild();
        });
        return;
      case 'remove':
        if (selectedId === f.id) setSelectedId(null);
        void run(async (d) => {
          await d.removeFeature(f.id);
          await d.rebuild();
        });
        return;
      case 'moveUp':
      case 'moveDown':
        void run(async (d) => {
          await d.moveFeature(f.id, f.index + (action === 'moveUp' ? -1 : 1));
          await d.rebuild();
        });
        return;
      case 'rollbackHere':
        void run(async (d) => {
          await d.setRollback(f.index + 1);
          await d.rebuild();
        });
        return;
    }
  };

  const undo = useCallback(() => {
    if (panelRef.current || sketchModeRef.current) return;
    void run(async (d) => {
      if ((await d.state()).canUndo) await d.undo();
    });
  }, [run]);

  const redo = useCallback(() => {
    if (panelRef.current || sketchModeRef.current) return;
    void run(async (d) => {
      if ((await d.state()).canRedo) await d.redo();
    });
  }, [run]);

  const newDocument = async () => {
    if (!kernel || panelRef.current) return;
    setError(null);
    try {
      await replaceDocument(await kernel.root.createDocument(), null);
      setFileName(t('part.newFile'));
    } catch (e) {
      if (!isCancelled(e)) setError(describeError(e));
    }
  };

  const openFile = async (file: File) => {
    if (!kernel || panelRef.current) return;
    setError(null);
    try {
      const text = await file.text();
      const { document } = await kernel.root.loadDocument(text);
      setFileName(file.name);
      fileNameRef.current = file.name;
      await replaceDocument(document, text);
      hostRef.current.onFile?.({ kind: 'part', name: file.name, text });
    } catch (e) {
      if (!isCancelled(e)) setError(describeError(e));
    }
  };

  const saveFile = async () => {
    const doc = docRef.current;
    if (!doc) return;
    try {
      const text = await doc.save();
      downloadText(fileName, text, 'application/json');
      savedRef.current = text;
      if (!(await doc.state()).inTransaction) snapshotRef.current = text;
      const file: DocumentFile = { kind: 'part', name: fileName, text };
      hostRef.current.onFile?.(file);
      hostRef.current.onChange?.(file, false);
    } catch (e) {
      if (!isCancelled(e)) setError(describeError(e));
    }
  };

  const createDrawing = async () => {
    const doc = docRef.current;
    if (!doc || !hostRef.current.onCreateDrawing) return;
    try {
      // With the BRep cache the drawing kernel does not rebuild the part.
      const text = await doc.save({ includeCache: true });
      hostRef.current.onCreateDrawing({ kind: 'part', name: fileName, text });
    } catch (e) {
      if (!isCancelled(e)) setError(describeError(e));
    }
  };

  const selectNode = (id: string | null) => {
    setSelectedId(id);
    void refresh(panelRef.current, id);
  };

  // ------------------------------------------------------------- sketch editor

  /** Opens the sketch editor on node `id` inside a transaction (docs/ui/SKETCH_EDITOR.md). */
  const enterSketch = async (id: string, isNew: boolean) => {
    const doc = docRef.current;
    if (!doc) return;
    let body: KernelMesh | null = null;
    try {
      body = await doc.tessellate(`before:${id}`, MESH_PARAMS);
    } catch (e) {
      if (isCancelled(e)) return;
      setError(describeError(e));
    }
    setSketchMode({ featureId: id, isNew, body });
  };

  const newSketch = async () => {
    const doc = docRef.current;
    if (!doc || panelRef.current || sketchMode) return;
    const position = snapshot.doc?.rollback ?? snapshot.features.length;
    let id: string | null = null;
    await run(async (d) => {
      await d.beginTransaction(t('part.tx.sketch'));
      const spec: Parameters<PartDocumentHandle['addFeature']>[0] = {
        type: 'Sketch',
        options: { plane: sketchPlane },
        sketch: { points: [], geometry: [], constraints: [], regions: [] },
      };
      if (sketchPlane === 'face') {
        if (selection?.kind !== 'face') throw new Error(t('part.pickPlanarFace'));
        spec.references = { face: [await d.makeReference('final', 'face', selection.index)] };
      }
      try {
        id = await d.addFeature(spec, position);
        await d.rebuild();
      } catch (e) {
        if (isCancelled(e)) throw e;
        await d.abortTransaction();
        await d.rebuild();
        throw e;
      }
    }, null);
    if (id) await enterSketch(id, true);
  };

  const editSketch = async (f: FeatureView) => {
    if (panelRef.current || sketchMode) return;
    if (await run((d) => d.beginTransaction(t('part.tx.editSketch', { feature: f.label })), null)) await enterSketch(f.id, false);
  };

  const exitSketch = (data: SketchData) => {
    const mode = sketchMode;
    if (!mode) return;
    setSketchMode(null);
    setSelectedId(mode.featureId);
    selectedRef.current = mode.featureId;
    void run(async (d) => {
      await d.editFeature(mode.featureId, { sketch: data });
      await d.rebuild();
      await d.commitTransaction();
    }, null);
  };

  const cancelSketch = () => {
    setSketchMode(null);
    void run(async (d) => {
      await d.abortTransaction();
      await d.rebuild();
    }, null);
  };

  useHotkeys(active && !sketchMode, {
    undo,
    redo,
    save: () => void saveFile(),
    open: () => openInputRef.current?.click(),
    escape: () => cancel(),
    delete: () => {
      const f = snapshot.features.find((x) => x.id === selectedId);
      if (f && !panelRef.current) treeAction(f, 'remove');
    },
    fit: () => setFrameKey((k) => k + 1),
  });

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
      idle: () => waitIdle(() => [kernelRef.current]),
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
        <span className="kv-title">{t('part.title')}</span>
        <button type="button" onClick={() => void newDocument()} disabled={status !== 'ready' || busyPanel} data-testid="doc-new">
          {t('part.new')}
        </button>
        <label className="kv-file">
          <input
            ref={openInputRef}
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
          {t('part.open')}
        </label>
        <button type="button" onClick={() => void saveFile()} disabled={status !== 'ready'} data-testid="doc-save" title="Ctrl+S">
          {t('part.save')}
        </button>
        {onCreateDrawing && (
          <button type="button" onClick={() => void createDrawing()} disabled={status !== 'ready' || busyPanel} data-testid="doc-create-drawing">
            {t('action.createDrawing')}
          </button>
        )}
        <span className="pm-sep" />
        <button
          type="button"
          onClick={undo}
          disabled={busyPanel || !docState?.canUndo}
          data-testid="undo"
          title={docState?.undoHistory.at(-1)?.name ? t('part.undoNamed', { name: docState.undoHistory.at(-1)!.name }) : 'Ctrl+Z'}
        >
          {t('part.undo')}
        </button>
        <button
          type="button"
          onClick={redo}
          disabled={busyPanel || !docState?.canRedo}
          data-testid="redo"
          title={docState?.redoHistory.at(-1)?.name ? t('part.redoNamed', { name: docState.redoHistory.at(-1)!.name }) : 'Ctrl+Y'}
        >
          {t('part.redo')}
        </button>
        <span className="pm-sep" />
        <select
          value={sketchPlane}
          data-testid="sketch-plane"
          title={t('part.sketchPlane')}
          disabled={status !== 'ready' || busyPanel}
          onChange={(e) => setSketchPlane(e.target.value)}
        >
          <option value="XY">XY</option>
          <option value="XZ">XZ</option>
          <option value="YZ">YZ</option>
          <option value="face">{t('part.selectedFace')}</option>
        </select>
        <button type="button" data-testid="sketch-draw" disabled={status !== 'ready' || busyPanel} onClick={() => void newSketch()}>
          {t('part.drawSketch')}
        </button>
        <span className="pm-sep" />
        {toolbarTypes(catalog.map((c) => c.type)).map((type) => (
          <button
            type="button"
            key={type}
            data-testid={`add-${type}`}
            disabled={status !== 'ready' || busyPanel}
            onClick={() => openAdd(type)}
          >
            {caption(t, 'part.feature.', type)}
          </button>
        ))}
        <BusyIndicator client={kernel} onCancel={() => void cancelOperation()} />
        <span className="kv-status" data-testid="modeling-status">
          {status === 'loading' && t('part.loading')}
          {status === 'ready' && t('part.ready')}
          {status === 'failed' && t('part.failed')}
        </span>
      </header>
      {error && (
        <div className="kv-error" role="alert" data-testid="modeling-error">
          {error.code}: {error.message}
        </div>
      )}
      {notice && (
        <div className="app-notice" role="status" data-testid="modeling-notice">
          {notice}
          <button type="button" onClick={() => setNotice(null)} aria-label={t('notify.dismiss')}>
            ×
          </button>
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
              void run(async (d) => {
                await d.setRollback(null);
                await d.rebuild();
              })
            }
          />
          <section className="pm-variables">
            <h2>{t('part.variables')}</h2>
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
                            void run(async (d) => {
                              await d.setVariable(v.name, expr);
                              await d.rebuild();
                            });
                          }
                        }}
                      />
                    </td>
                    <td title={v.error ?? ''}>{v.error ? '!' : `= ${fmt(v.value, 4)}`}</td>
                    <td>
                      <button
                        type="button"
                        aria-label={t('part.removeVariable', { name: v.name })}
                        disabled={busyPanel}
                        onClick={() =>
                          void run(async (d) => {
                            await d.removeVariable(v.name);
                            await d.rebuild();
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
                      placeholder={t('part.variableName')}
                      value={newVar.name}
                      data-testid="var-new-name"
                      disabled={busyPanel}
                      onChange={(e) => setNewVar({ ...newVar, name: e.target.value })}
                    />
                  </td>
                  <td>
                    <input
                      placeholder={t('part.variableExpression')}
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
                      onClick={async () => {
                        const ok = await run(async (d) => {
                          await d.setVariable(newVar.name.trim(), newVar.expression.trim());
                          await d.rebuild();
                        });
                        if (ok) setNewVar({ name: '', expression: '' });
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
                onPreview={() => void preview()}
                onOk={() => void confirm()}
                onCancel={cancel}
              />
            ) : (
              <p className="pm-hint">{t('part.hint')}</p>
            )}
            <section>
              <h2>{t('part.properties')}</h2>
              <dl data-testid="properties">
                <dt>{t('part.volume')}</dt>
                <dd data-testid="prop-volume" data-value={props?.volume ?? ''}>
                  {props?.volume === null || props?.volume === undefined ? t('part.noBody') : fmt(props.volume)}
                </dd>
                <dt>{t('part.density')}</dt>
                <dd>
                  <input
                    value={density}
                    data-testid="density"
                    onChange={(e) => setDensity(e.target.value)}
                    onBlur={() => void refresh()}
                  />
                </dd>
                <dt>{t('part.mass')}</dt>
                <dd data-testid="prop-mass" data-value={props?.mass ?? ''}>
                  {fmt(props?.mass, 4)}
                </dd>
                <dt>{t('part.area')}</dt>
                <dd>{fmt(props?.area)}</dd>
                <dt>{t('part.centerOfMass')}</dt>
                <dd>{props?.centerOfMass ? fmtVec(props.centerOfMass) : '—'}</dd>
                <dt>{t('part.box')}</dt>
                <dd>{props?.boundingBox ? `${fmtVec(props.boundingBox.min)} – ${fmtVec(props.boundingBox.max)}` : '—'}</dd>
                <dt>{t('part.facesEdges')}</dt>
                <dd>{props ? `${props.counts.faces} / ${props.counts.edges}` : '—'}</dd>
                {selection && (
                  <>
                    <dt>{selection.kind === 'face' ? t('part.face') : t('part.edge')}</dt>
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
