/**
 * Application shell (docs/ui/APP_SHELL.md): start page (new part or assembly, open a file,
 * recent files, drafts to restore), tabs of open documents with the editors of the part
 * modeler, assembly and drawing pages, "part → create drawing" and "assembly → open component
 * part", autosave of drafts to IndexedDB, a common status bar with notifications, RU/EN.
 *
 * Every editor runs its kernel module in its own Web Worker. window.bendcoreApp exposes the shell
 * state (read-only) and idle() for end-to-end tests.
 */

import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import PartModelerApp from '../modeling/PartModelerApp';
import AssemblyApp from '../assembly/AssemblyApp';
import DrawingEditorApp from '../drawing/DrawingEditorApp';
import KernelViewerApp from '../kernel/KernelViewerApp';
import { WasmClient } from '../worker/wasmClient';
import { waitIdle } from '../worker/idle';
import type { DocKind, DocumentFile, PartSource } from './editorHost';
import { I18nContext, LANGS, storeLang, storedLang, translate } from './i18n';
import type { I18n, Lang, MessageKey } from './i18n';
import { useHotkeys } from './hotkeys';
import { AppStore, DraftEntry, RecentEntry, detectKind, openAppStore, recentId } from './storage';
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
}

interface Notification {
  id: number;
  level: 'error' | 'info';
  text: string;
}

export interface AppShellState {
  tabs: { id: string; kind: TabKind; title: string; dirty: boolean }[];
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
  viewer: 'start.viewStep',
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

  const openTab = useCallback((kind: TabKind, initial: DocumentFile | null, opts: { id?: string; dirty?: boolean; title?: string } = {}) => {
    const id = opts.id ?? newTabId();
    const title = opts.title ?? initial?.name ?? '';
    setTabs((list) => [
      ...list.filter((x) => x.id !== id),
      { id, kind, title, initial, initialDirty: opts.dirty ?? false, snapshot: initial, dirty: opts.dirty ?? false },
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

  const openFiles = async (list: FileList | null) => {
    const files = list ? await Promise.all([...list].map(async (f) => ({ name: f.name, text: await f.text() }))) : [];
    if (files.length === 0) return;
    const typed = files.map((f) => ({ ...f, detected: detectKind(f.text) }));
    const drawing = typed.find((f) => 'kind' in f.detected && f.detected.kind === 'drawing');
    for (const f of typed) {
      if ('error' in f.detected) {
        notify('error', t('notify.unknownFile', { name: f.name, reason: f.detected.error }));
        continue;
      }
      const kind = f.detected.kind;
      if (kind === 'drawing') {
        const model = typed.find((m) => m !== f && 'kind' in m.detected && m.detected.kind !== 'drawing');
        if (!model) {
          notify('error', t('notify.drawingNeedsModel', { name: f.name }));
          continue;
        }
        openTab('drawing', { kind: 'drawing', name: f.name, text: f.text, model: model.text });
      } else if (!drawing) {
        // A model chosen together with its drawing is the drawing's model, not a tab of its own.
        openTab(kind, { kind, name: f.name, text: f.text });
      }
      notify('info', t('notify.opened', { name: f.name }));
    }
  };

  const restoreDrafts = () => {
    for (const d of offered) openTab(d.file.kind, d.file, { id: d.id, dirty: true });
    setOffered([]);
  };

  const discardDrafts = () => {
    const s = storeRef.current;
    for (const d of offered) {
      storedDrafts.current.delete(d.id);
      s?.removeDraft(d.id).catch(() => undefined);
    }
    setOffered([]);
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
    for (const tab of tabs) {
      if (tab.kind === 'part' && tab.snapshot && !seen.has(tab.snapshot.name)) {
        seen.add(tab.snapshot.name);
        out.push({ definition: -1, name: tab.snapshot.name, text: tab.snapshot.text });
      }
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
    tabs: tabs.map((x) => ({ id: x.id, kind: x.kind, title: x.title, dirty: x.dirty })),
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
        return <AssemblyApp {...common} partLibrary={partLibrary} onOpenPart={(part) => openTab('part', part)} />;
      case 'drawing':
        return <DrawingEditorApp {...common} />;
      case 'viewer':
        return <KernelViewerApp />;
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
                <button type="button" className="app-primary" onClick={() => openTab('part', null, { title: 'деталь.bcpart.json' })} data-testid="new-part">
                  {t('start.newPart')}
                </button>
                <button type="button" className="app-primary" onClick={() => openTab('assembly', null, { title: 'assembly.json' })} data-testid="new-assembly">
                  {t('start.newAssembly')}
                </button>
                <button type="button" onClick={() => openTab('viewer', null, { title: 'STEP' })} data-testid="new-viewer">
                  {t('start.viewStep')}
                </button>
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
                        <button type="button" className="app-link" onClick={() => openTab(r.file.kind, r.file)} data-testid={`recent-${r.id}`}>
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
              {editorOf(tab)}
            </section>
          ))}
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
