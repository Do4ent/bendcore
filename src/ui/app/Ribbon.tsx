/**
 * Ribbon of the editors (docs/ui/APP_SHELL.md, "Лента"): a title, the quick-access commands of
 * the document (new, open, save, undo, redo), tabs of the editor's sections with captioned
 * groups of large and small icon buttons, a tooltip (name, shortcut, one line of description,
 * reason when unavailable), the "icons and captions" / "icons only" mode and collapsing, both kept
 * in localStorage. Every command calls the editor's handler; the ribbon holds no actions of its
 * own. The model (layout, tooltips, preferences, active tab) is ribbonModel.ts.
 *
 * The panels of all tabs stay in the page side by side in a strip scrolled by the tabs: a command
 * of another tab that receives focus or is scrolled into view brings its tab forward, so the
 * keyboard and the automation reach every command by its data-testid.
 */

import React, { useCallback, useEffect, useId, useLayoutEffect, useMemo, useRef, useState } from 'react';
import { Icon } from '../icons';
import { useI18n } from './i18n';
import {
  commandIcon,
  groupLayout,
  iconPixels,
  isCustom,
  loadRibbonPrefs,
  panelAt,
  saveRibbonPrefs,
  showsCaption,
  tooltipText,
} from './ribbonModel';
import type { PanelBox, RibbonCommand, RibbonDisplay, RibbonPrefs, RibbonTab } from './ribbonModel';
import './Ribbon.css';

export interface RibbonProps {
  /** Name of the editor, left of the quick-access commands. */
  title: React.ReactNode;
  quick?: RibbonCommand[];
  tabs: RibbonTab[];
  /** Tab to bring forward when it changes (the sketch editor opens its tab). */
  activeTab?: string | null;
  /** Indicator and status of the editor, right of the tabs. */
  status?: React.ReactNode;
  testId?: string;
  className?: string;
}

interface TipState {
  id: string;
  cmd: RibbonCommand;
  x: number;
  y: number;
}

const TIP_DELAY_MS = 450;

const RibbonButton: React.FC<{
  cmd: RibbonCommand;
  display: RibbonDisplay;
  size: 'large' | 'small';
  tipId: string;
  onTip: (cmd: RibbonCommand | null, el?: HTMLElement) => void;
}> = ({ cmd, display, size, tipId, onTip }) => {
  const caption = showsCaption(display);
  const cls = [
    'bc-rb-btn',
    size === 'large' ? 'bc-rb-large' : 'bc-rb-small',
    cmd.active ? 'bc-rb-active' : '',
    cmd.disabled ? 'bc-rb-disabled' : '',
    cmd.className ?? '',
  ]
    .filter(Boolean)
    .join(' ');
  const content = (
    <>
      <Icon name={commandIcon(cmd.icon)} size={iconPixels(size)} />
      {caption && <span className="bc-rb-caption">{cmd.label}</span>}
    </>
  );
  const show = (e: React.SyntheticEvent<HTMLElement>) => onTip(cmd, e.currentTarget);
  const hide = () => onTip(null);
  return (
    <span className="bc-rb-cmd" onMouseEnter={show} onMouseLeave={hide} data-command={cmd.id}>
      {cmd.kind === 'file' ? (
        <label className={cls} aria-label={cmd.label} aria-describedby={tipId} aria-disabled={cmd.disabled || undefined}>
          {content}
          <input
            ref={cmd.inputRef}
            type="file"
            className="bc-rb-file"
            accept={cmd.accept}
            multiple={cmd.multiple}
            disabled={cmd.disabled}
            data-testid={cmd.testId}
            aria-label={cmd.label}
            onFocus={show}
            onBlur={hide}
            onChange={(e) => cmd.onFiles?.(e)}
          />
        </label>
      ) : (
        <button
          type="button"
          className={cls}
          disabled={cmd.disabled}
          aria-label={cmd.label}
          aria-pressed={cmd.active === undefined ? undefined : cmd.active}
          aria-describedby={tipId}
          data-testid={cmd.testId}
          onFocus={show}
          onBlur={hide}
          onClick={() => {
            hide();
            cmd.onClick?.();
          }}
        >
          {content}
        </button>
      )}
    </span>
  );
};

const Ribbon: React.FC<RibbonProps> = ({ title, quick = [], tabs, activeTab, status, testId, className }) => {
  const { t } = useI18n();
  const [prefs, setPrefs] = useState<RibbonPrefs>(() => loadRibbonPrefs());
  const [current, setCurrent] = useState<string | null>(tabs[0]?.id ?? null);
  /** A tab opened over the collapsed ribbon (closed again by a command or a click elsewhere). */
  const [peek, setPeek] = useState(false);
  const [tip, setTip] = useState<TipState | null>(null);
  const tipTimer = useRef<number | null>(null);
  const stripRef = useRef<HTMLDivElement | null>(null);
  const tipId = useId();
  const shownTabs = useMemo(
    () => tabs.map((tab) => ({ ...tab, groups: tab.groups.filter((g) => g.items.length > 0) })).filter((tab) => tab.groups.length > 0),
    [tabs],
  );
  const tabKey = shownTabs.map((x) => x.id).join('|');

  const update = useCallback((next: Partial<RibbonPrefs>) => {
    setPrefs((p) => {
      const merged = { ...p, ...next };
      saveRibbonPrefs(merged);
      return merged;
    });
  }, []);

  const boxes = useCallback((): PanelBox[] => {
    const strip = stripRef.current;
    if (!strip) return [];
    return [...strip.querySelectorAll<HTMLElement>(':scope > .bc-rb-panel')].map((el) => ({
      id: el.dataset.tab ?? '',
      left: el.offsetLeft,
      width: el.offsetWidth,
    }));
  }, []);

  const scrollTo = useCallback(
    (id: string) => {
      const strip = stripRef.current;
      const box = boxes().find((b) => b.id === id);
      if (strip && box) strip.scrollLeft = box.left;
      setCurrent(id);
    },
    [boxes],
  );

  // A strip scrolled by focus or by scrollIntoView shows the tab under its middle, kept inside
  // that tab's panel.
  const onScroll = useCallback(() => {
    const strip = stripRef.current;
    if (!strip) return;
    const all = boxes();
    let id = panelAt(all, strip.scrollLeft, strip.clientWidth);
    // A minimal scroll that brings a command of the neighbouring tab into view (keyboard focus,
    // automation) leaves the middle of the strip on the current tab; aligning back to it would
    // move the command out from under the pointer. A strip moved off the current tab's start is
    // aligned to the neighbour in the direction of the move instead.
    const cur = all.findIndex((b) => b.id === current);
    if (id === current && cur >= 0) {
      const delta = strip.scrollLeft - all[cur].left;
      const next = delta > 0.5 ? all[cur + 1] : delta < -0.5 ? all[cur - 1] : undefined;
      if (next && next.width > 0) id = next.id;
    }
    // The scroll handler only names the tab: it never moves the strip itself. Moving it here, while
    // a command that was just scrolled into view is being clicked, moved the command out from
    // under the pointer. The strip is aligned to a tab by a tab click and by a change of width.
    if (id && all.some((b) => b.id === id)) setCurrent(id);
  }, [boxes, current]);

  // The tab the strip shows right now, read from its scroll position. A command of another tab
  // scrolled into view (keyboard focus, automation) moves the strip before the scroll event
  // updates `current`; re-aligning to the stale state would scroll the command away again under
  // the pointer, so the alignments below keep the tab the strip actually shows.
  const shownNow = useCallback((): string | null => {
    const strip = stripRef.current;
    // Without layout (zero widths: hidden strip, test DOM) the scroll position says nothing.
    if (!strip || strip.clientWidth <= 0) return current;
    const all = boxes();
    if (!all.some((b) => b.width > 0)) return current;
    // The current tab while any of its panel is in view, else the tab under the middle.
    const box = all.find((b) => b.id === current);
    if (box && box.left < strip.scrollLeft + strip.clientWidth && box.left + box.width > strip.scrollLeft) return current;
    return panelAt(all, strip.scrollLeft, strip.clientWidth) ?? current;
  }, [boxes, current]);

  // Keep the current tab in view when the tabs or the width change.
  useLayoutEffect(() => {
    const now = shownNow();
    const id = now && shownTabs.some((x) => x.id === now) ? now : shownTabs[0]?.id;
    if (id) scrollTo(id);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tabKey, prefs.collapsed, peek]);
  useEffect(() => {
    if (activeTab && shownTabs.some((x) => x.id === activeTab)) scrollTo(activeTab);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [activeTab, tabKey]);
  useEffect(() => {
    const strip = stripRef.current;
    if (!strip || typeof ResizeObserver === 'undefined') return undefined;
    // Only a real change of width re-aligns (the observer also reports on observe and on height
    // changes, which would move the strip under a pointer for nothing).
    let width = strip.clientWidth;
    const ro = new ResizeObserver(() => {
      if (Math.abs(strip.clientWidth - width) < 0.5) return;
      width = strip.clientWidth;
      const id = shownNow();
      if (id) scrollTo(id);
    });
    ro.observe(strip);
    return () => ro.disconnect();
  }, [shownNow, scrollTo]);
  useEffect(
    () => () => {
      if (tipTimer.current !== null) window.clearTimeout(tipTimer.current);
    },
    [],
  );

  const onTip = useCallback((cmd: RibbonCommand | null, el?: HTMLElement) => {
    if (tipTimer.current !== null) window.clearTimeout(tipTimer.current);
    tipTimer.current = null;
    if (!cmd || !el) {
      setTip(null);
      return;
    }
    const r = el.getBoundingClientRect();
    tipTimer.current = window.setTimeout(() => setTip({ id: cmd.id, cmd, x: r.left, y: r.bottom + 4 }), TIP_DELAY_MS);
  }, []);

  const bodyShown = !prefs.collapsed || peek;
  const tipText = tip ? tooltipText(tip.cmd, t) : null;

  return (
    <div
      className={`bc-ribbon ${prefs.display === 'icons' ? 'bc-rb-icons-only' : ''} ${prefs.collapsed ? 'bc-rb-collapsed' : ''} ${className ?? ''}`}
      data-testid={testId}
      data-display={prefs.display}
      data-collapsed={prefs.collapsed ? 'true' : 'false'}
      data-active-tab={current ?? ''}
      onClickCapture={(e) => {
        if (peek && (e.target as HTMLElement).closest('.bc-rb-panel .bc-rb-btn')) setPeek(false);
      }}
    >
      <div className="bc-rb-top">
        <span className="bc-rb-title">{title}</span>
        {quick.length > 0 && (
          <span className="bc-rb-quick" role="toolbar" aria-label={t('ribbon.quickAccess')}>
            {quick.map((cmd) => (
              <RibbonButton key={cmd.id} cmd={cmd} display={prefs.display} size="small" tipId={tipId} onTip={onTip} />
            ))}
          </span>
        )}
        <span className="bc-rb-tabs" role="tablist" aria-label={t('ribbon.tabs')}>
          {shownTabs.map((tab) => (
            <button
              key={tab.id}
              type="button"
              role="tab"
              aria-selected={current === tab.id}
              className={current === tab.id ? 'bc-rb-tab bc-rb-tab-active' : 'bc-rb-tab'}
              data-testid={`ribbon-tab-${tab.id}`}
              onClick={() => {
                if (prefs.collapsed) setPeek((p) => (current === tab.id ? !p : true));
                scrollTo(tab.id);
              }}
              onDoubleClick={() => {
                setPeek(false);
                update({ collapsed: !prefs.collapsed });
              }}
            >
              {tab.label}
            </button>
          ))}
        </span>
        <span className="bc-rb-status">{status}</span>
        <span className="bc-rb-prefs">
          <button
            type="button"
            className="bc-rb-pref"
            data-testid="ribbon-display"
            aria-pressed={prefs.display === 'icons'}
            aria-label={prefs.display === 'icons' ? t('ribbon.showLabels') : t('ribbon.iconsOnly')}
            title={prefs.display === 'icons' ? t('ribbon.showLabels') : t('ribbon.iconsOnly')}
            onClick={() => update({ display: prefs.display === 'icons' ? 'labels' : 'icons' })}
          >
            <Icon name={prefs.display === 'icons' ? 'list' : 'show'} size={16} />
          </button>
          <button
            type="button"
            className="bc-rb-pref"
            data-testid="ribbon-collapse"
            aria-expanded={!prefs.collapsed}
            aria-label={prefs.collapsed ? t('ribbon.expand') : t('ribbon.collapse')}
            title={prefs.collapsed ? t('ribbon.expand') : t('ribbon.collapse')}
            onClick={() => {
              setPeek(false);
              update({ collapsed: !prefs.collapsed });
            }}
          >
            <Icon name={prefs.collapsed ? 'expand' : 'collapse'} size={16} />
          </button>
        </span>
      </div>
      <div
        className={`bc-rb-strip ${peek ? 'bc-rb-peek' : ''}`}
        ref={stripRef}
        onScroll={onScroll}
        hidden={!bodyShown}
        data-testid={testId ? `${testId}-body` : undefined}
      >
        {shownTabs.map((tab) => (
          <section key={tab.id} className="bc-rb-panel" data-tab={tab.id} role="tabpanel" aria-label={tab.label}>
            {tab.groups.map((g) => (
              <div key={g.id} className="bc-rb-group" role="group" aria-label={g.label} data-group={g.id}>
                <div className="bc-rb-cells">
                  {groupLayout(g.items).map((cell, i) =>
                    cell.kind === 'large' ? (
                      <RibbonButton key={cell.item.id} cmd={cell.item} display={prefs.display} size="large" tipId={tipId} onTip={onTip} />
                    ) : cell.kind === 'column' ? (
                      <span key={`c${i}`} className="bc-rb-column">
                        {cell.items.map((cmd) => (
                          <RibbonButton key={cmd.id} cmd={cmd} display={prefs.display} size="small" tipId={tipId} onTip={onTip} />
                        ))}
                      </span>
                    ) : (
                      <span key={cell.item.id} className="bc-rb-custom" data-command={cell.item.id}>
                        {cell.item.render()}
                      </span>
                    ),
                  )}
                </div>
                <div className="bc-rb-group-label">{g.label}</div>
              </div>
            ))}
          </section>
        ))}
      </div>
      <div
        id={tipId}
        role="tooltip"
        className="bc-rb-tip"
        data-testid="ribbon-tooltip"
        hidden={!tipText}
        style={tip ? { left: tip.x, top: tip.y } : undefined}
      >
        {tipText && (
          <>
            <strong className="bc-rb-tip-title">{tipText.title}</strong>
            {tipText.hotkey && <span className="bc-rb-tip-keys">{tipText.hotkey}</span>}
            {tipText.description && <span className="bc-rb-tip-text">{tipText.description}</span>}
            {tipText.reason && <span className="bc-rb-tip-reason">{tipText.reason}</span>}
          </>
        )}
      </div>
    </div>
  );
};

export default Ribbon;
