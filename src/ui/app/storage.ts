/**
 * Browser storage of the application shell (docs/ui/APP_SHELL.md): recent files and drafts of
 * open documents in IndexedDB.
 *
 * IndexedDB can be missing or closed (private mode, blocked site data, quota): openAppStore()
 * then resolves to { store: null, reason } and the shell works without recent files and
 * autosave, saying so in the status bar. Every operation of an open store can still fail
 * (quota, a deleted database); callers catch and report.
 */

import type { DocKind, DocumentFile } from './editorHost';

export const DB_NAME = 'bendcore-app';
export const DB_VERSION = 1;
export const RECENT_LIMIT = 12;

export interface RecentEntry {
  /** kind + name: one entry per file name and kind. */
  id: string;
  file: DocumentFile;
  /** ms since epoch */
  usedAt: number;
}

export interface DraftEntry {
  /** Tab id of the document. */
  id: string;
  file: DocumentFile;
  savedAt: number;
}

export function recentId(kind: DocKind, name: string): string {
  return `${kind}:${name}`;
}

/** Recent list after using `entry`: newest first, one per id, at most `limit`. */
export function mergeRecent(list: RecentEntry[], entry: RecentEntry, limit = RECENT_LIMIT): RecentEntry[] {
  return [entry, ...list.filter((e) => e.id !== entry.id)].sort((a, b) => b.usedAt - a.usedAt).slice(0, limit);
}

/** Entries beyond the newest `limit` (to delete after a put). */
export function overflowRecent(list: RecentEntry[], limit = RECENT_LIMIT): RecentEntry[] {
  return [...list].sort((a, b) => b.usedAt - a.usedAt).slice(limit);
}

function request<T>(req: IDBRequest<T>): Promise<T> {
  return new Promise((resolve, reject) => {
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error ?? new Error('IndexedDB request failed'));
  });
}

function done(tx: IDBTransaction): Promise<void> {
  return new Promise((resolve, reject) => {
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error ?? new Error('IndexedDB transaction failed'));
    tx.onabort = () => reject(tx.error ?? new Error('IndexedDB transaction aborted'));
  });
}

export class AppStore {
  constructor(private readonly db: IDBDatabase) {}

  private async all<T>(store: string): Promise<T[]> {
    const tx = this.db.transaction(store, 'readonly');
    return request(tx.objectStore(store).getAll() as IDBRequest<T[]>);
  }

  private async put(store: string, value: unknown): Promise<void> {
    const tx = this.db.transaction(store, 'readwrite');
    tx.objectStore(store).put(value);
    await done(tx);
  }

  private async delete(store: string, key: string): Promise<void> {
    const tx = this.db.transaction(store, 'readwrite');
    tx.objectStore(store).delete(key);
    await done(tx);
  }

  async recent(): Promise<RecentEntry[]> {
    return (await this.all<RecentEntry>('recent')).sort((a, b) => b.usedAt - a.usedAt);
  }

  /** Records a used file; keeps the newest RECENT_LIMIT entries. */
  async useFile(file: DocumentFile, usedAt = Date.now()): Promise<RecentEntry[]> {
    const entry: RecentEntry = { id: recentId(file.kind, file.name), file, usedAt };
    await this.put('recent', entry);
    const list = await this.recent();
    for (const old of overflowRecent(list)) await this.delete('recent', old.id);
    return mergeRecent(list, entry);
  }

  removeRecent(id: string): Promise<void> {
    return this.delete('recent', id);
  }

  drafts(): Promise<DraftEntry[]> {
    return this.all<DraftEntry>('drafts');
  }

  /**
   * Writes drafts in one transaction. The write starts synchronously, so a call from a
   * pagehide handler reaches IndexedDB before the page is gone.
   */
  putDrafts(entries: DraftEntry[]): Promise<void> {
    const tx = this.db.transaction('drafts', 'readwrite');
    const store = tx.objectStore('drafts');
    for (const e of entries) store.put(e);
    return done(tx);
  }

  removeDraft(id: string): Promise<void> {
    return this.delete('drafts', id);
  }

  async clearDrafts(): Promise<void> {
    const tx = this.db.transaction('drafts', 'readwrite');
    tx.objectStore('drafts').clear();
    await done(tx);
  }
}

export async function openAppStore(): Promise<{ store: AppStore | null; reason: string | null }> {
  try {
    if (typeof indexedDB === 'undefined') return { store: null, reason: 'IndexedDB is not available' };
    const req = indexedDB.open(DB_NAME, DB_VERSION);
    req.onupgradeneeded = () => {
      const db = req.result;
      if (!db.objectStoreNames.contains('recent')) db.createObjectStore('recent', { keyPath: 'id' });
      if (!db.objectStoreNames.contains('drafts')) db.createObjectStore('drafts', { keyPath: 'id' });
    };
    const db = await request(req);
    return { store: new AppStore(db), reason: null };
  } catch (e) {
    return { store: null, reason: e instanceof Error ? e.message : String(e) };
  }
}

/** Kind of a document file from its JSON `format`, or an error message. */
export function detectKind(text: string): { kind: DocKind } | { error: string } {
  let format: unknown;
  try {
    format = (JSON.parse(text) as { format?: unknown }).format;
  } catch (e) {
    return { error: `not JSON (${e instanceof Error ? e.message : String(e)})` };
  }
  if (format === 'bendcore-part') return { kind: 'part' };
  if (format === 'bendcore-assembly') return { kind: 'assembly' };
  // A drawing file (src/ui/drawing/drawingFile.ts), or the drawing document of the earlier
  // two-file format (opened with its model file).
  if (format === 'bendcore-drawing-file' || format === 'bendcore-drawing') return { kind: 'drawing' };
  return { error: `unknown format ${JSON.stringify(format ?? null)}` };
}
