/**
 * Keyboard shortcuts of the editors (docs/ui/APP_SHELL.md):
 *
 *   Ctrl+Z undo · Ctrl+Y / Ctrl+Shift+Z redo · Ctrl+S save · Ctrl+O open
 *   Esc cancel the open panel or mode · Delete remove the selection · F fit the view
 *
 * Keys typed into inputs are left to the input, except Esc. Only the active editor (the shown
 * tab of the shell) listens.
 */

import { useEffect, useRef } from 'react';

export type HotkeyAction = 'undo' | 'redo' | 'save' | 'open' | 'escape' | 'delete' | 'fit';
export type HotkeyHandlers = Partial<Record<HotkeyAction, () => void>>;

export interface KeyLike {
  key: string;
  ctrlKey: boolean;
  metaKey: boolean;
  shiftKey: boolean;
  altKey: boolean;
  /** Tag name of the event target. */
  targetTag?: string;
}

const TEXT_TAGS = new Set(['INPUT', 'TEXTAREA', 'SELECT']);

/** Action bound to a key press, or null. */
export function hotkeyAction(e: KeyLike): HotkeyAction | null {
  const key = e.key.toLowerCase();
  const inText = e.targetTag !== undefined && TEXT_TAGS.has(e.targetTag.toUpperCase());
  if (key === 'escape') return 'escape';
  if (inText || e.altKey) return null;
  if (e.ctrlKey || e.metaKey) {
    if (key === 'z') return e.shiftKey ? 'redo' : 'undo';
    if (key === 'y') return 'redo';
    if (key === 's') return 'save';
    if (key === 'o') return 'open';
    return null;
  }
  if (e.shiftKey) return null;
  if (key === 'delete') return 'delete';
  if (key === 'f') return 'fit';
  return null;
}

export function useHotkeys(active: boolean, handlers: HotkeyHandlers): void {
  const ref = useRef(handlers);
  ref.current = handlers;
  useEffect(() => {
    if (!active) return undefined;
    const onKey = (e: KeyboardEvent) => {
      const target = e.target as HTMLElement | null;
      const action = hotkeyAction({
        key: e.key,
        ctrlKey: e.ctrlKey,
        metaKey: e.metaKey,
        shiftKey: e.shiftKey,
        altKey: e.altKey,
        targetTag: target?.tagName,
      });
      const handler = action ? ref.current[action] : undefined;
      if (!handler) return;
      e.preventDefault();
      handler();
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [active]);
}
