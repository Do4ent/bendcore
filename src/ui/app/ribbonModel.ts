// Model of the ribbon of the editors (docs/ui/APP_SHELL.md, "Лента"): tabs, groups, commands,
// the layout of a group, tooltips, the display mode kept in localStorage, the active tab of the
// scrolled strip. No React here: the logic is tested by Jest (src/ui/app/__tests__/ribbon.test.tsx).

import type { ReactNode, Ref } from 'react';
import { FALLBACK_ICON, hasIcon } from '../icons';
import type { Translate } from './i18n';

/** 'labels': icons and captions; 'icons': icons only (the caption stays the accessible name). */
export type RibbonDisplay = 'labels' | 'icons';

export interface RibbonPrefs {
  display: RibbonDisplay;
  collapsed: boolean;
}

export const RIBBON_STORAGE_KEY = 'bendcore.ribbon';
export const DEFAULT_RIBBON_PREFS: RibbonPrefs = { display: 'labels', collapsed: false };

/** Command of the ribbon: one call of the editor (and through it of the kernel). */
export interface RibbonCommand {
  kind?: 'button' | 'file';
  /** Stable id within the ribbon; also the key of the tooltip text ribbon.tip.<id> when `tip` is not given. */
  id: string;
  /** data-testid of the button (file command: of its input). */
  testId?: string;
  /** Icon name (src/ui/icons); a name without an icon shows the general feature icon. */
  icon: string;
  label: string;
  /** One line of description in the tooltip. */
  tip?: string;
  /** Keyboard shortcut shown in the tooltip ("Ctrl+S"). */
  hotkey?: string;
  /** Large button (icon 32 and caption) for frequent commands; small (icon 16) otherwise. */
  size?: 'large' | 'small';
  disabled?: boolean;
  /** Why the command is unavailable, shown in the tooltip of a disabled command. */
  reason?: string;
  /** Pressed state of a tool (aria-pressed). */
  active?: boolean;
  /** Extra class of the button (the editors' active-tool classes). */
  className?: string;
  onClick?: () => void;
  /** File command: the input's attributes and handler. */
  accept?: string;
  multiple?: boolean;
  inputRef?: Ref<HTMLInputElement>;
  onFiles?: (input: HTMLInputElement) => void;
}

/** Control that is not a command (plane choice, fields, mode radios) placed in a group. */
export interface RibbonCustom {
  kind: 'custom';
  id: string;
  render: () => ReactNode;
}

export type RibbonItem = RibbonCommand | RibbonCustom;

export interface RibbonGroup {
  id: string;
  label: string;
  items: RibbonItem[];
}

export interface RibbonTab {
  id: string;
  label: string;
  groups: RibbonGroup[];
}

export function isCustom(item: RibbonItem): item is RibbonCustom {
  return item.kind === 'custom';
}

/** Small commands stacked in a column of a group. */
export const SMALL_PER_COLUMN = 3;

export type GroupCell = { kind: 'large'; item: RibbonCommand } | { kind: 'column'; items: RibbonCommand[] } | { kind: 'custom'; item: RibbonCustom };

/**
 * Layout of a group: large buttons each in their own cell, runs of small buttons in columns of
 * SMALL_PER_COLUMN, custom controls in their own cell; the order of the items is kept.
 */
export function groupLayout(items: readonly RibbonItem[]): GroupCell[] {
  const cells: GroupCell[] = [];
  let column: RibbonCommand[] | null = null;
  for (const item of items) {
    if (isCustom(item)) {
      cells.push({ kind: 'custom', item });
      column = null;
    } else if (item.size === 'large') {
      cells.push({ kind: 'large', item });
      column = null;
    } else {
      if (!column || column.length >= SMALL_PER_COLUMN) {
        column = [];
        cells.push({ kind: 'column', items: column });
      }
      column.push(item);
    }
  }
  return cells;
}

/** Icon size of a button in pixels. */
export function iconPixels(size: RibbonCommand['size']): 32 | 16 {
  return size === 'large' ? 32 : 16;
}

/** Whether the caption is drawn next to the icon (it is the accessible name in any mode). */
export function showsCaption(display: RibbonDisplay): boolean {
  return display === 'labels';
}

/** Icon of a command: its own, or the general feature icon for a command without one. */
export function commandIcon(name: string): string {
  return hasIcon(name) ? name : FALLBACK_ICON;
}

export interface TooltipText {
  title: string;
  hotkey: string | null;
  description: string | null;
  /** Why the command is unavailable (disabled commands only). */
  reason: string | null;
}

/** Text of the tooltip of a command: name, shortcut, one line of description, reason if disabled. */
export function tooltipText(cmd: RibbonCommand, t: Translate): TooltipText {
  return {
    title: cmd.label,
    hotkey: cmd.hotkey ? t('ribbon.hotkey', { keys: cmd.hotkey }) : null,
    description: cmd.tip ?? null,
    reason: cmd.disabled ? cmd.reason ?? t('ribbon.unavailable') : null,
  };
}

function storage(): Storage | null {
  try {
    return typeof window === 'undefined' ? null : window.localStorage;
  } catch {
    return null;
  }
}

/** Preferences stored earlier in this browser; defaults when storage is closed or holds anything else. */
export function loadRibbonPrefs(store: Storage | null = storage()): RibbonPrefs {
  try {
    const raw = store?.getItem(RIBBON_STORAGE_KEY);
    if (!raw) return { ...DEFAULT_RIBBON_PREFS };
    const v = JSON.parse(raw) as Partial<RibbonPrefs>;
    return {
      display: v.display === 'icons' ? 'icons' : 'labels',
      collapsed: v.collapsed === true,
    };
  } catch {
    return { ...DEFAULT_RIBBON_PREFS };
  }
}

/** Stores the preferences; closed storage (private mode, blocked site data) keeps them for this page only. */
export function saveRibbonPrefs(prefs: RibbonPrefs, store: Storage | null = storage()): boolean {
  try {
    if (!store) return false;
    store.setItem(RIBBON_STORAGE_KEY, JSON.stringify(prefs));
    return true;
  } catch {
    return false;
  }
}

export interface PanelBox {
  id: string;
  left: number;
  width: number;
}

/**
 * Tab shown by the strip scrolled to `scrollLeft`: the panel under the middle of the visible
 * width. All panels stay in the page side by side, each at least as wide as the strip; a tab
 * click scrolls to its panel, and a command of another tab that receives focus or is scrolled
 * into view (keyboard, automation) brings its tab forward.
 */
export function panelAt(panels: readonly PanelBox[], scrollLeft: number, viewport: number): string | null {
  if (panels.length === 0) return null;
  const middle = scrollLeft + viewport / 2;
  for (const p of panels) if (middle >= p.left && middle < p.left + p.width) return p.id;
  return middle < panels[0].left ? panels[0].id : panels[panels.length - 1].id;
}

/** Scroll position that keeps the visible width inside one panel (no half of a neighbour shown). */
export function clampToPanel(panel: PanelBox, scrollLeft: number, viewport: number): number {
  const max = panel.left + Math.max(0, panel.width - viewport);
  return Math.min(Math.max(scrollLeft, panel.left), max);
}

/** Problems of a ribbon definition: repeated tab, group or command ids and repeated test ids. */
export function ribbonProblems(tabs: readonly RibbonTab[], quick: readonly RibbonCommand[] = []): string[] {
  const problems: string[] = [];
  const seen = new Set<string>();
  const testIds = new Set<string>();
  const once = (key: string, what: string) => {
    if (seen.has(key)) problems.push(`${what} is repeated`);
    seen.add(key);
  };
  const command = (c: RibbonItem) => {
    once(`item:${c.id}`, `command ${c.id}`);
    if (!isCustom(c) && c.testId) {
      if (testIds.has(c.testId)) problems.push(`test id ${c.testId} is repeated`);
      testIds.add(c.testId);
    }
  };
  quick.forEach(command);
  for (const tab of tabs) {
    once(`tab:${tab.id}`, `tab ${tab.id}`);
    for (const g of tab.groups) {
      once(`group:${tab.id}/${g.id}`, `group ${tab.id}/${g.id}`);
      g.items.forEach(command);
    }
  }
  return problems;
}
