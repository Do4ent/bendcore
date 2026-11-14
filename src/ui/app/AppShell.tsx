/**
 * Application shell (docs/ui/APP_SHELL.md): start page (new part or assembly, open a file,
 * recent files, drafts to restore), tabs of open documents with the editors of the part
 * modeler, assembly and drawing pages, "part → create drawing" and "assembly → open component
 * part", autosave of drafts to IndexedDB, a common status bar with notifications, RU/EN; import
 * of STEP/IGES as a part without history or as an assembly, "Export…" of the active document and
 * the mass properties of part tabs (docs/ui/DATA_EXCHANGE.md).
 *
 * Every editor runs its kernel module in its own Web Worker. window.bendcoreApp exposes the shell
 * state (read-only) and idle() for end-to-end tests.
 */

import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import PartModelerApp from '../modeling/PartModelerApp';
import AssemblyApp from '../assembly/AssemblyApp';
import DrawingEditorApp from '../drawing/DrawingEditorApp';
import KernelViewerApp from '../kernel/KernelViewerApp';
import type { ImportedFile } from '../kernel/KernelViewerApp';
import { defaultModelingBaseUrl, loadModelingFrom } from '../modeling/modelingLoader';
import ExportDialog from '../exchange/ExportDialog';
import MassSection from '../exchange/MassSection';
import type { MassResult } from '../exchange/MassSection';
import { PartExchange } from '../exchange/partExchange';
import { IMPORT_ACCEPT, importFormatOf, parseDensity } from '../exchange/exchange';
import type { ImportFormat } from '../exchange/exchange';
import { WasmClient } from '../worker/wasmClient';
import { waitIdle } from '../worker/idle';
import type { DocKind, DocumentFile, PartSource } from './editorHost';
import { I18nContext, LANGS, storeLang, storedLang, translate } from './i18n';
import type { I18n, Lang, MessageKey } from './i18n';
import { useHotkeys } from './hotkeys';
import { AppStore, DraftEntry, RecentEntry, detectKind, openAppStore, recentId } from './storage';
import { migrateDrawingDocument, packDrawingFile, readDrawingFiles } from '../drawing/drawingFile';
import type { ModuleFaultInfo } from './faultReport';
import type { ModuleKind } from '../worker/protocol';
import './AppShell.css';

/** Interval of draft writes (ms); drafts are also written when the page is hidden or closed. */
export const AUTOSAVE_MS = 2000;

type TabKind = DocKind | 'viewer';

interface Tab {
  id: string;
  kind: TabKind;
  title: string;
  initial: DocumentFile | null;
  initialDirty: boolean;
  /** Latest document reported by the editor. */
  snapshot: DocumentFile | null;
  dirty: boolean;
  /** The editor's module failed; the shell offers a restart (docs/ui/APP_SHELL.md, "Сбой модуля"). */
  fault: TabFault | null;
  /** Mount number of the editor: a restart after a fault mounts it again with the draft. */
  generation: number;
  /** "Import STEP/IGES": the file of a viewer tab, or the assembly text of an assembly tab. */
  importFile?: ImportedFile | null;
  importAssembly?: { name: string; format: ImportFormat; text: string } | null;
  /** Incremented by "Export…" of the shell (EditorHostProps.exportRequest). */
  exportRequest: number;
}

interface TabFault extends ModuleFaultInfo {
  /** When the draft of the last completed document was written; null — not written. */
  draftAt: number | null;
  /** Why the draft was not written (storage closed, nothing to save, write failed). */
  draftError: string | null;
  restarting: boolean;
}

interface Notification {
  id: number;
  level: 'error' | 'info';
  text: string;
}

export interface AppShellState {
  tabs: { id: string; kind: TabKind; title: string; dirty: boolean; fault: { module: string; code: string; draftAt: number | null } | null; generation: number }[];
  active: string;
  draftSavedAt: number | null;
  storage: 'loading' | 'ready' | 'unavailable';
  offeredDrafts: { id: string; kind: DocKind; name: string }[];
  recent: { id: string; kind: DocKind; name: string }[];
  notifications: Notification[];
  lang: Lang;
}

declare global {
  interface Window {
    bendcoreApp?: { getState(): AppShellState; idle(): Promise<void> };
  }
}

let tabCounter = 0;
const newTabId = () => `t${Date.now().toString(36)}-${(tabCounter++).toString(36)}`;

const KIND_KEY: Record<TabKind, MessageKey> = {
  part: 'kind.part',
  assembly: 'kind.assembly',
  drawing: 'kind.drawing',
  viewer: 'kind.imported',
};

const MODULE_KEY: Record<ModuleKind, MessageKey> = {
  kernel: 'module.kernel',
  modeling: 'module.modeling',
  assembly: 'module.assembly',
  drawing: 'module.drawing',
};

const timeText = (ms: number) => new Date(ms).toLocaleTimeString();

const AppShell: React.FC = () => {
  const [lang, setLangState] = useState<Lang>(storedLang);
  const i18n = useMemo<I18n>(
    () => ({
      lang,
      t: (key, params) => translate(lang, key, params),
      setLang: (l: Lang) => {
        storeLang(l);
        setLangState(l);
      },
    }),
    [lang],
  );
  const { t } = i18n;

  const [tabs, setTabs] = useState<Tab[]>([]);
  const tabsRef = useRef(tabs);
  tabsRef.current = tabs;
  const [active, setActive] = useState('start');
  const [store, setStore] = useState<AppStore | null>(null);
  const storeRef = useRef(store);
  storeRef.current = store;
  const [storage, setStorage] = useState<AppShellState['storage']>('loading');
  const [storageReason, setStorageReason] = useState<string | null>(null);
  const [recent, setRecent] = useState<RecentEntry[]>([]);
  const [offered, setOffered] = useState<DraftEntry[]>([]);
  const [draftSavedAt, setDraftSavedAt] = useState<number | null>(null);
  const [notifications, setNotifications] = useState<Notification[]>([]);
  const [menu, setMenu] = useState<{ tab: string; x: number; y: number } | null>(null);
  const openInput = useRef<HTMLInputElement | null>(null);
  /** Export and mass properties of part tabs: one modeling worker, started on first use. */
  const partExchange = useMemo(() => new PartExchange(() => loadModelingFrom(defaultModelingBaseUrl())), []);
  useEffect(() => () => partExchange.dispose(), [partExchange]);
  const [partExport, setPartExport] = useState<{ name: string; text: string } | null>(null);
  const [massOpen, setMassOpen] = useState(false);
  const [partDensity, setPartDensity] = useState('7850');
  /** Tabs whose draft changed since the last write (or must be deleted). */
  const pendingDrafts = useRef(new Set<string>());
  /** Tabs that have a draft record in the store. */
  const storedDrafts = useRef(new Set<string>());

  const notify = useCallback((level: Notification['level'], text: string) => {
    setNotifications((list) => [...list.slice(-4), { id: Date.now() + Math.random(), level, text }]);
  }, []);

  // ------------------------------------------------------------ storage

  useEffect(() => {
    let live = true;
    void openAppStore().then(async ({ store: s, reason }) => {
      if (!live) return;
      if (!s) {
        setStorage('unavailable');
        setStorageReason(reason);
        return;
      }
      try {
        const [r, d] = await Promise.all([s.recent(), s.drafts()]);
        if (!live) return;
        setRecent(r);
        setOffered(d);
        for (const x of d) storedDrafts.current.add(x.id);
        setStore(s);
        setStorage('ready');
      } catch (e) {
        setStorage('unavailable');
        setStorageReason(e instanceof Error ? e.message : String(e));
      }
    });
    return () => {
      live = false;
    };
  }, []);

  /** Writes changed drafts and deletes drafts of saved or closed documents. */
  const flushDrafts = useCallback((): Promise<void> => {
    const s = storeRef.current;
    if (!s || pendingDrafts.current.size === 0) return Promise.resolve();
    const ids = [...pendingDrafts.current];
    pendingDrafts.current.clear();
    const now = Date.now();
    const write: DraftEntry[] = [];
    const remove: string[] = [];
    for (const id of ids) {
      const tab = tabsRef.current.find((x) => x.id === id);
      if (tab && tab.dirty && tab.snapshot) write.push({ id, file: tab.snapshot, savedAt: now });
      else if (storedDrafts.current.has(id)) remove.push(id);
    }
    // Both requests start now (pagehide): IndexedDB finishes them after the page is gone.
    const writing = write.length ? s.putDrafts(write) : Promise.resolve();
    const removing = remove.map((id) => s.removeDraft(id));
    for (const w of write) storedDrafts.current.add(w.id);
    for (const id of remove) storedDrafts.current.delete(id);
    return Promise.all([writing, ...removing]).then(
      () => {
        if (write.length) setDraftSavedAt(now);
      },
      (e: unknown) => notify('error', t('status.draftUnavailable', { reason: e instanceof Error ? e.message : String(e) })),
    );
  }, [notify, t]);

  useEffect(() => {
    const timer = window.setInterval(() => void flushDrafts(), AUTOSAVE_MS);
    const onHide = () => void flushDrafts();
    const onVisibility = () => document.visibilityState === 'hidden' && void flushDrafts();
    window.addEventListener('pagehide', onHide);
    window.addEventListener('beforeunload', onHide);
    document.addEventListener('visibilitychange', onVisibility);
    return () => {
      window.clearInterval(timer);
      window.removeEventListener('pagehide', onHide);
      window.removeEventListener('beforeunload', onHide);
      document.removeEventListener('visibilitychange', onVisibility);
    };
  }, [flushDrafts]);

  useEffect(() => {
    const onRejection = (e: PromiseRejectionEvent) => {
      const r = e.reason as { code?: string; detail?: string; message?: string } | undefined;
      notify('error', r?.code ? `${r.code}: ${r.detail ?? r.message}` : String(r?.message ?? e.reason));
    };
    window.addEventListener('unhandledrejection', onRejection);
    return () => window.removeEventListener('unhandledrejection', onRejection);
  }, [notify]);

  const recordFile = useCallback(
    (file: DocumentFile) => {
      const s = storeRef.current;
      if (!s) return;
      s.useFile(file).then(setRecent, (e: unknown) =>
        notify('error', t('start.recentUnavailable', { reason: e instanceof Error ? e.message : String(e) })),
      );
    },
    [notify, t],
  );

  // ------------------------------------------------------------ tabs

  const openTab = useCallback((kind: TabKind, initial: DocumentFile | null, opts: { id?: string; dirty?: boolean; title?: string; importFile?: ImportedFile } = {}) => {
    const id = opts.id ?? newTabId();
    const title = opts.title ?? initial?.name ?? '';
    setTabs((list) => [
      ...list.filter((x) => x.id !== id),
      {
        id,
        kind,
        title,
        initial,
        initialDirty: opts.dirty ?? false,
        snapshot: initial,
        dirty: opts.dirty ?? false,
        fault: null,
        generation: 0,
        importFile: opts.importFile ?? null,
        exportRequest: 0,
      },
    ]);
    setActive(id);
    return id;
  }, []);

  const closeTab = (id: string) => {
    const tab = tabsRef.current.find((x) => x.id === id);
    if (!tab) return;
    if (tab.dirty && !window.confirm(t('tabs.closeUnsaved', { name: tab.title || t(KIND_KEY[tab.kind]) }))) return;
    setTabs((list) => list.filter((x) => x.id !== id));
    pendingDrafts.current.add(id);
    // The tab is gone from tabsRef at the next render; delete its draft now.
    const s = storeRef.current;
    if (s && storedDrafts.current.has(id)) {
      storedDrafts.current.delete(id);
      pendingDrafts.current.delete(id);
      s.removeDraft(id).catch(() => undefined);
    }
    if (active === id) {
      const rest = tabsRef.current.filter((x) => x.id !== id);
      setActive(rest.length ? rest[rest.length - 1].id : 'start');
    }
  };

  const onEditorChange = useCallback((id: string, file: DocumentFile, dirty: boolean) => {
    setTabs((list) => list.map((x) => (x.id === id ? { ...x, snapshot: file, dirty, title: file.name || x.title } : x)));
    pendingDrafts.current.add(id);
  }, []);

  /**
   * The module of a tab failed: the last completed document of the tab (its latest onChange
   * snapshot) is written as a draft at once, the tab shows the failure and a restart button.
   */
  const onModuleFault = useCallback(
    (id: string, info: ModuleFaultInfo) => {
      const tab = tabsRef.current.find((x) => x.id === id);
      if (!tab || tab.fault) return;
      const setFault = (fault: TabFault) => setTabs((list) => list.map((x) => (x.id === id ? { ...x, fault } : x)));
      const base: TabFault = { ...info, draftAt: null, draftError: null, restarting: false };
      notify('error', `${t('fault.title')}: ${info.code}: ${info.detail}`);
      const s = storeRef.current;
      if (!tab.snapshot) {
        setFault({ ...base, draftError: t('fault.nothingToSave') });
        return;
      }
      if (!s) {
        setFault({ ...base, draftError: storageReason ?? 'IndexedDB is not available' });
        return;
      }
      const now = Date.now();
      // Not left to the autosave timer: the draft must exist before the user restarts.
      pendingDrafts.current.delete(id);
      s.putDrafts([{ id, file: tab.snapshot, savedAt: now }]).then(
        () => {
          storedDrafts.current.add(id);
          setDraftSavedAt(now);
          setFault({ ...base, draftAt: now });
        },
        (e: unknown) => setFault({ ...base, draftError: e instanceof Error ? e.message : String(e) }),
      );
    },
    [notify, t, storageReason],
  );

  /** Mounts the editor of a failed tab again (a new worker) with the document of its draft. */
  const restartTab = async (id: string) => {
    const tab = tabsRef.current.find((x) => x.id === id);
    if (!tab || !tab.fault) return;
    setTabs((list) => list.map((x) => (x.id === id && x.fault ? { ...x, fault: { ...x.fault, restarting: true } } : x)));
    let file = tab.snapshot;
    const s = storeRef.current;
    if (s && tab.fault.draftAt !== null) {
      try {
        const draft = (await s.drafts()).find((d) => d.id === id);
        if (draft) file = migrateDrawingDocument(draft.file);
      } catch (e) {
        notify('error', t('fault.restoreFailed', { reason: e instanceof Error ? e.message : String(e) }));
      }
    }
    setTabs((list) =>
      list.map((x) =>
        x.id === id ? { ...x, initial: file, initialDirty: x.dirty, snapshot: file, fault: null, generation: x.generation + 1 } : x,
      ),
    );
    notify('info', file ? t('fault.restored') : t('fault.restartedEmpty'));
  };

  const openFiles = async (list: FileList | null) => {
    const files = list ? await Promise.all([...list].map(async (f) => ({ name: f.name, text: await f.text() }))) : [];
    if (files.length === 0) return;
    const typed = files.map((f) => ({ ...f, detected: detectKind(f.text) }));
    // Model files used by drawings of the earlier two-file format are not tabs of their own.
    const consumed = new Set<string>();
    for (const f of typed) {
      if (!('kind' in f.detected) || f.detected.kind !== 'drawing') continue;
      const models = typed.filter((m) => m !== f && 'kind' in m.detected && m.detected.kind !== 'drawing' && !consumed.has(m.name));
      const r = readDrawingFiles([f, ...models.slice(0, 1)]);
      if (!r.ok) {
        notify('error', r.error.code === 'needsModel' ? t('notify.drawingNeedsModel', { name: f.name }) : t('notify.unknownFile', { name: f.name, reason: r.error.detail }));
        continue;
      }
      if (r.migrated) consumed.add(models[0].name);
      openTab('drawing', { kind: 'drawing', name: r.name, text: r.migrated ? packDrawingFile(r.content) : f.text });
      notify('info', r.migrated ? t('notify.drawingMigrated', { name: f.name, file: r.name }) : t('notify.opened', { name: f.name }));
    }
    for (const f of typed) {
      if ('error' in f.detected) {
        notify('error', t('notify.unknownFile', { name: f.name, reason: f.detected.error }));
        continue;
      }
      const kind = f.detected.kind;
      if (kind === 'drawing' || consumed.has(f.name)) continue;
      openTab(kind, { kind, name: f.name, text: f.text });
      notify('info', t('notify.opened', { name: f.name }));
    }
  };

  /** "Import STEP/IGES": the viewer tab reads the file; an assembly moves on to the assembly editor. */
  const importModels = async (list: FileList | null) => {
    for (const f of list ? [...list] : []) {
      if (!importFormatOf(f.name)) {
        notify('error', t('exchange.unknownFormat', { name: f.name }));
        continue;
      }
      openTab('viewer', null, { title: f.name, importFile: { name: f.name, bytes: new Uint8Array(await f.arrayBuffer()) } });
    }
  };

  const viewerIsAssembly = (id: string, file: { name: string; format: ImportFormat; text: string; parts: number }) => {
    setTabs((list) =>
      list.map((x) =>
        x.id === id
          ? { ...x, kind: 'assembly', title: file.name, importFile: null, importAssembly: file, generation: x.generation + 1 }
          : x,
      ),
    );
    notify('info', t('exchange.importedAsAssembly', { name: file.name, parts: file.parts }));
  };

  /** "Export…" of the shell: part tabs export here, the other editors open their own dialog. */
  const exportActive = () => {
    const tab = tabsRef.current.find((x) => x.id === active);
    if (!tab) return;
    if (tab.kind === 'part') {
      if (tab.snapshot) setPartExport({ name: tab.snapshot.name, text: tab.snapshot.text });
      return;
    }
    setTabs((list) => list.map((x) => (x.id === tab.id ? { ...x, exportRequest: x.exportRequest + 1 } : x)));
  };

  const restoreDrafts = () => {
    for (const d of offered) openTab(d.file.kind, migrateDrawingDocument(d.file), { id: d.id, dirty: true });
    setOffered([]);
  };

  /** Deletes the offered drafts; the offer disappears once IndexedDB has committed it. */
  const discardDrafts = () => {
    const s = storeRef.current;
    const list = offered;
    for (const d of list) storedDrafts.current.delete(d.id);
    Promise.all(list.map((d) => s?.removeDraft(d.id))).then(
      () => setOffered([]),
      (e: unknown) => notify('error', t('status.draftUnavailable', { reason: e instanceof Error ? e.message : String(e) })),
    );
  };

  useHotkeys(active === 'start', { open: () => openInput.current?.click() });

  useEffect(() => {
    if (!menu) return undefined;
    const close = () => setMenu(null);
    window.addEventListener('click', close);
    return () => window.removeEventListener('click', close);
  }, [menu]);

  // Parts the assembly editor can insert: open part tabs, then recent part files.
  const partLibrary = useMemo<PartSource[]>(() => {
    const out: PartSource[] = [];
    const seen = new Set<string>();
    // Every open part tab is offered, also when two tabs have the same file name (new parts).
    let n = 0;
    for (const tab of tabs) {
      if (tab.kind !== 'part' || !tab.snapshot) continue;
      n += 1;
      const name = seen.has(tab.snapshot.name) ? tab.snapshot.name.replace(/(\.bcpart)?\.json$/i, ` (${n})$&`) : tab.snapshot.name;
      seen.add(tab.snapshot.name);
      seen.add(name);
      out.push({ definition: -1, name, text: tab.snapshot.text });
    }
    for (const r of recent) {
      if (r.file.kind === 'part' && !seen.has(r.file.name)) {
        seen.add(r.file.name);
        out.push({ definition: -1, name: r.file.name, text: r.file.text });
      }
    }
    return out;
  }, [tabs, recent]);

  // ------------------------------------------------------------ test hook

  const stateRef = useRef<AppShellState | null>(null);
  stateRef.current = {
    tabs: tabs.map((x) => ({
      id: x.id,
      kind: x.kind,
      title: x.title,
      dirty: x.dirty,
      fault: x.fault && { module: x.fault.module, code: x.fault.code, draftAt: x.fault.draftAt },
      generation: x.generation,
    })),
    active,
    draftSavedAt,
    storage,
    offeredDrafts: offered.map((d) => ({ id: d.id, kind: d.file.kind, name: d.file.name })),
    recent: recent.map((r) => ({ id: r.id, kind: r.file.kind, name: r.file.name })),
    notifications,
    lang,
  };
  useEffect(() => {
    window.bendcoreApp = {
      getState: () => stateRef.current!,
      idle: () => waitIdle(() => WasmClient.live()),
    };
    return () => {
      delete window.bendcoreApp;
    };
  }, []);

  // ------------------------------------------------------------ render

  const activeTab = tabs.find((x) => x.id === active) ?? null;

  const editorOf = (tab: Tab) => {
    const isActive = tab.id === active;
    const common = {
      active: isActive,
      initial: tab.initial,
      initialDirty: tab.initialDirty,
      onChange: (file: DocumentFile, dirty: boolean) => onEditorChange(tab.id, file, dirty),
      onFile: recordFile,
      onFault: (info: ModuleFaultInfo) => onModuleFault(tab.id, info),
    };
    switch (tab.kind) {
      case 'part':
        return (
          <PartModelerApp
            {...common}
            onCreateDrawing={(part) => openTab('drawing', part, { title: part.name.replace(/(\.bcpart)?\.json$/i, '') + '.bcdrw.json' })}
          />
        );
      case 'assembly':
        return (
          <AssemblyApp
            {...common}
            exportRequest={tab.exportRequest}
            importFile={tab.importAssembly ?? null}
            partLibrary={partLibrary}
            onOpenPart={(part) => openTab('part', part)}
          />
        );
      case 'drawing':
        return <DrawingEditorApp {...common} />;
      case 'viewer':
        return (
          <KernelViewerApp
            onFault={common.onFault}
            exportRequest={tab.exportRequest}
            importFile={tab.importFile ?? null}
            onAssembly={(file) => viewerIsAssembly(tab.id, file)}
          />
        );
    }
  };

  return (
    <I18nContext.Provider value={i18n}>
      <div className="app-root" data-testid="app-shell">
        <nav className="app-tabs" role="tablist">
          <button
            type="button"
            role="tab"
            aria-selected={active === 'start'}
            className={active === 'start' ? 'app-tab app-tab-active' : 'app-tab'}
            onClick={() => setActive('start')}
            data-testid="tab-start"
          >
            {t('tabs.start')}
          </button>
          {tabs.map((tab) => (
            <span
              key={tab.id}
              role="tab"
              aria-selected={active === tab.id}
              className={active === tab.id ? 'app-tab app-tab-active' : 'app-tab'}
              data-testid={`doc-tab-${tab.kind}`}
              data-tab={tab.id}
              onClick={() => setActive(tab.id)}
              onContextMenu={(e) => {
                e.preventDefault();
                setMenu({ tab: tab.id, x: e.clientX, y: e.clientY });
              }}
            >
              <span className="app-tab-kind">{t(KIND_KEY[tab.kind])}</span>
              <span className="app-tab-title">{tab.title}</span>
              {tab.dirty && (
                <span className="app-tab-dirty" title={t('status.unsaved')} data-testid="tab-dirty">
                  ●
                </span>
              )}
              <button
                type="button"
                className="app-tab-close"
                aria-label={t('tabs.close')}
                onClick={(e) => {
                  e.stopPropagation();
                  closeTab(tab.id);
                }}
              >
                ×
              </button>
            </span>
          ))}
          <span className="app-tabs-fill" />
          {activeTab && activeTab.kind !== 'drawing' && (
            <button type="button" className="app-tab-action" onClick={exportActive} disabled={activeTab.kind === 'part' && !activeTab.snapshot} data-testid="shell-export">
              {t('exchange.export')}
            </button>
          )}
          {activeTab?.kind === 'part' && (
            <button type="button" className="app-tab-action" aria-pressed={massOpen} onClick={() => setMassOpen((o) => !o)} data-testid="shell-mass">
              {t('exchange.mass')}
            </button>
          )}
          <label className="app-lang">
            {t('app.language')}
            <select value={lang} onChange={(e) => i18n.setLang(e.target.value as Lang)} data-testid="lang">
              {LANGS.map((l) => (
                <option key={l} value={l}>
                  {l.toUpperCase()}
                </option>
              ))}
            </select>
          </label>
        </nav>
        {menu && (
          <ul className="app-menu" role="menu" style={{ left: menu.x, top: menu.y }} onClick={(e) => e.stopPropagation()}>
            <li>
              <button type="button" role="menuitem" onClick={() => (setMenu(null), closeTab(menu.tab))}>
                {t('tabs.close')}
              </button>
            </li>
            <li>
              <button
                type="button"
                role="menuitem"
                onClick={() => {
                  setMenu(null);
                  for (const x of tabsRef.current) if (x.id !== menu.tab) closeTab(x.id);
                }}
              >
                {t('tabs.closeOthers')}
              </button>
            </li>
          </ul>
        )}
        <main className="app-main">
          <section className="app-start" style={{ display: active === 'start' ? undefined : 'none' }} data-testid="start-page">
            <h1>
              {t('app.title')} · {t('start.title')}
            </h1>
            {offered.length > 0 && (
              <div className="app-card app-drafts" data-testid="drafts-offer">
                <h2>{t('start.drafts')}</h2>
                <p>{t('start.draftsHint')}</p>
                <ul>
                  {offered.map((d) => (
                    <li key={d.id}>
                      {t(KIND_KEY[d.file.kind])}: <strong>{d.file.name}</strong> — {t('start.savedAt', { time: timeText(d.savedAt) })}
                    </li>
                  ))}
                </ul>
                <button type="button" className="app-primary" onClick={restoreDrafts} data-testid="drafts-restore">
                  {t('start.restore')}
                </button>
                <button type="button" onClick={discardDrafts} data-testid="drafts-discard">
                  {t('start.discard')}
                </button>
              </div>
            )}
            <div className="app-start-grid">
              <div className="app-card">
                <h2>{t('start.new')}</h2>
                <button type="button" className="app-primary" onClick={() => openTab('part', null, { title: t('part.newFile') })} data-testid="new-part">
                  {t('start.newPart')}
                </button>
                <button type="button" className="app-primary" onClick={() => openTab('assembly', null, { title: 'assembly.json' })} data-testid="new-assembly">
                  {t('start.newAssembly')}
                </button>
                <button type="button" onClick={() => openTab('viewer', null, { title: 'STEP' })} data-testid="new-viewer">
                  {t('start.viewStep')}
                </button>
                <label className="app-file">
                  {t('exchange.import')}
                  <input
                    type="file"
                    multiple
                    accept={IMPORT_ACCEPT}
                    data-testid="import-model"
                    onChange={(e) => {
                      void importModels(e.target.files);
                      e.target.value = '';
                    }}
                  />
                </label>
                <p className="app-hint">{t('exchange.importHint')}</p>
              </div>
              <div className="app-card">
                <h2>{t('start.open')}</h2>
                <label className="app-file">
                  {t('start.openFile')}
                  <input
                    ref={openInput}
                    type="file"
                    multiple
                    accept=".json,application/json"
                    data-testid="open-files"
                    onChange={(e) => {
                      void openFiles(e.target.files);
                      e.target.value = '';
                    }}
                  />
                </label>
                <p className="app-hint">{t('start.openHint')}</p>
              </div>
              <div className="app-card app-recent">
                <h2>{t('start.recent')}</h2>
                {storage === 'unavailable' ? (
                  <p className="app-hint">{t('start.recentUnavailable', { reason: storageReason ?? '' })}</p>
                ) : recent.length === 0 ? (
                  <p className="app-hint">{t('start.recentEmpty')}</p>
                ) : (
                  <ul data-testid="recent-list">
                    {recent.map((r) => (
                      <li key={r.id}>
                        <button type="button" className="app-link" onClick={() => openTab(r.file.kind, migrateDrawingDocument(r.file))} data-testid={`recent-${r.id}`}>
                          {t(KIND_KEY[r.file.kind])}: {r.file.name}
                        </button>
                        <span className="app-hint"> {timeText(r.usedAt)}</span>
                        <button
                          type="button"
                          className="app-tab-close"
                          aria-label={t('start.recentRemove')}
                          onClick={() =>
                            store
                              ?.removeRecent(r.id)
                              .then(() => setRecent((l) => l.filter((x) => x.id !== r.id)))
                              .catch((e: unknown) => notify('error', String(e)))
                          }
                        >
                          ×
                        </button>
                      </li>
                    ))}
                  </ul>
                )}
              </div>
            </div>
            <p className="app-hint">
              {t('keys.title')}: {t('keys.list')}
            </p>
          </section>
          {tabs.map((tab) => (
            <section
              key={tab.id}
              className="app-panel"
              data-tab-panel={tab.id}
              data-active={tab.id === active ? 'true' : 'false'}
              style={{ display: tab.id === active ? undefined : 'none' }}
            >
              {tab.fault && (
                <div className="app-fault" role="alert" data-testid="fault-card" data-module={tab.fault.module} data-code={tab.fault.code}>
                  <h2>{t('fault.title')}</h2>
                  <p>
                    {t('fault.text', {
                      module: t(MODULE_KEY[tab.fault.module]),
                      code: tab.fault.code,
                      detail: tab.fault.detail,
                      name: tab.title || t(KIND_KEY[tab.kind]),
                    })}
                  </p>
                  <p data-testid="fault-draft">
                    {tab.fault.draftAt !== null
                      ? t('fault.draftSaved', { time: timeText(tab.fault.draftAt) })
                      : tab.fault.draftError !== null
                        ? t('fault.draftFailed', { reason: tab.fault.draftError })
                        : t('fault.draftSaving')}
                  </p>
                  <button
                    type="button"
                    className="app-primary"
                    disabled={tab.fault.restarting || (tab.fault.draftAt === null && tab.fault.draftError === null)}
                    onClick={() => void restartTab(tab.id)}
                    data-testid="fault-restart"
                  >
                    {tab.fault.restarting ? t('fault.restarting') : t('fault.restart')}
                  </button>
                </div>
              )}
              <React.Fragment key={tab.generation}>{editorOf(tab)}</React.Fragment>
            </section>
          ))}
          {massOpen && activeTab?.kind === 'part' && activeTab.snapshot && (
            <aside className="app-mass kv-panel" data-testid="shell-mass-panel">
              <MassSection
                version={activeTab.snapshot.text}
                density={{ value: partDensity, onChange: setPartDensity }}
                load={async (): Promise<MassResult> => {
                  const rho = parseDensity(partDensity);
                  if (rho === null) return { kind: 'empty', reason: 'noDensity' };
                  const props = await partExchange.massProperties(activeTab.snapshot!.text, rho);
                  return props ? { kind: 'ok', props } : { kind: 'empty', reason: 'noBody' };
                }}
              />
            </aside>
          )}
          {partExport && (
            <ExportDialog
              name={partExport.name}
              onExport={(format, options) => partExchange.exportFile(partExport.text, format, options)}
              onSaved={(name, size) => notify('info', t('exchange.saved', { name, size }))}
              onClose={() => setPartExport(null)}
            />
          )}
        </main>
        <footer className="app-status" data-testid="app-status">
          <span data-testid="app-status-doc">
            {activeTab
              ? `${t(KIND_KEY[activeTab.kind])}: ${activeTab.title} · ${activeTab.dirty ? t('status.unsaved') : t('status.saved')}`
              : t('status.noDocument')}
          </span>
          <span className="app-status-fill" />
          {storage === 'unavailable' ? (
            <span className="app-hint">{t('status.draftUnavailable', { reason: storageReason ?? '' })}</span>
          ) : (
            draftSavedAt !== null && (
              <span data-testid="draft-saved">{t('status.draftSaved', { time: timeText(draftSavedAt) })}</span>
            )
          )}
          <ul className="app-notifications" aria-live="polite" data-testid="notifications">
            {notifications.map((n) => (
              <li key={n.id} className={`app-note app-note-${n.level}`} role={n.level === 'error' ? 'alert' : 'status'}>
                {n.level === 'error' ? `${t('notify.error')}: ` : ''}
                {n.text}
                <button type="button" aria-label={t('notify.dismiss')} onClick={() => setNotifications((l) => l.filter((x) => x.id !== n.id))}>
                  ×
                </button>
              </li>
            ))}
          </ul>
        </footer>
      </div>
    </I18nContext.Provider>
  );
};

export default AppShell;
