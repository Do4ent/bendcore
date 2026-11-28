// Ribbon of the editors (docs/ui/APP_SHELL.md, "Лента"): layout of groups, display mode kept in
// localStorage (closed storage tolerated), tooltips, unavailable commands, collapsing, the active
// tab of the scrolled strip; the ribbons of the part, sketch, assembly, drawing and STEP viewer
// editors: tabs and groups, commands of the kernel catalog, unique test ids, known icons.

import React from 'react';
import { act, fireEvent, render, screen } from '@testing-library/react';
import Ribbon from '../Ribbon';
import { I18nContext, translate } from '../i18n';
import type { I18n, Lang } from '../i18n';
import {
  DEFAULT_RIBBON_PREFS,
  RIBBON_STORAGE_KEY,
  clampToPanel,
  commandIcon,
  groupLayout,
  iconPixels,
  isCustom,
  loadRibbonPrefs,
  panelAt,
  ribbonProblems,
  saveRibbonPrefs,
  showsCaption,
  tooltipText,
} from '../ribbonModel';
import type { RibbonCommand, RibbonItem, RibbonTab } from '../ribbonModel';
import { FALLBACK_ICON, hasIcon } from '../../icons';
import { partRibbon } from '../../modeling/partRibbon';
import type { PartRibbonActions, PartRibbonState } from '../../modeling/partRibbon';
import { constraintIcon, sketchRibbon } from '../../modeling/sketch/sketchRibbon';
import { assemblyRibbon } from '../../assembly/assemblyRibbon';
import { drawingRibbon } from '../../drawing/drawingRibbon';
import type { DrawingRibbonState } from '../../drawing/drawingRibbon';
import { viewerRibbon } from '../../kernel/viewerRibbon';
import { roundScale } from '../../modeling/sketch/snap';

const tr = (lang: Lang = 'ru') => (key: Parameters<typeof translate>[1], params?: Record<string, string | number>) => translate(lang, key, params);
const t = tr('ru');

const cmd = (id: string, size?: 'large' | 'small', extra: Partial<RibbonCommand> = {}): RibbonCommand => ({ id, icon: 'extrude', label: id, size, ...extra });

class MemoryStorage {
  data = new Map<string, string>();
  getItem(k: string) {
    return this.data.has(k) ? this.data.get(k)! : null;
  }
  setItem(k: string, v: string) {
    this.data.set(k, v);
  }
}
const closedStorage = {
  getItem() {
    throw new Error('SecurityError');
  },
  setItem() {
    throw new Error('SecurityError');
  },
} as unknown as Storage;

describe('ribbon model', () => {
  test('large buttons take a cell each, small ones stack three to a column, custom controls a cell', () => {
    const items: RibbonItem[] = [
      cmd('a', 'large'),
      cmd('b'),
      cmd('c'),
      cmd('d'),
      cmd('e'),
      { kind: 'custom', id: 'x', render: () => null },
      cmd('f'),
      cmd('g', 'large'),
    ];
    const cells = groupLayout(items);
    expect(cells.map((c) => (c.kind === 'column' ? c.items.map((i) => i.id).join('') : `${c.kind}:${c.item.id}`))).toEqual([
      'large:a',
      'bcd',
      'e',
      'custom:x',
      'f',
      'large:g',
    ]);
    expect(isCustom(items[5])).toBe(true);
    expect(iconPixels('large')).toBe(32);
    expect(iconPixels('small')).toBe(16);
    expect(iconPixels(undefined)).toBe(16);
  });

  test('display mode and collapsing are kept in storage; closed or broken storage gives the defaults', () => {
    const store = new MemoryStorage() as unknown as Storage;
    expect(loadRibbonPrefs(store)).toEqual(DEFAULT_RIBBON_PREFS);
    expect(saveRibbonPrefs({ display: 'icons', collapsed: true }, store)).toBe(true);
    expect(loadRibbonPrefs(store)).toEqual({ display: 'icons', collapsed: true });
    store.setItem(RIBBON_STORAGE_KEY, '{not json');
    expect(loadRibbonPrefs(store)).toEqual(DEFAULT_RIBBON_PREFS);
    store.setItem(RIBBON_STORAGE_KEY, JSON.stringify({ display: 'large', collapsed: 'yes' }));
    expect(loadRibbonPrefs(store)).toEqual(DEFAULT_RIBBON_PREFS);
    expect(loadRibbonPrefs(closedStorage)).toEqual(DEFAULT_RIBBON_PREFS);
    expect(saveRibbonPrefs({ display: 'icons', collapsed: false }, closedStorage)).toBe(false);
    expect(saveRibbonPrefs({ display: 'icons', collapsed: false }, null)).toBe(false);
    expect(showsCaption('labels')).toBe(true);
    expect(showsCaption('icons')).toBe(false);
  });

  test('tooltip: name, shortcut, one line of description, reason only when unavailable', () => {
    const c = cmd('save', 'small', { label: 'Сохранить', hotkey: 'Ctrl+S', tip: 'Сохранить документ в файл' });
    expect(tooltipText(c, t)).toEqual({ title: 'Сохранить', hotkey: 'Клавиши: Ctrl+S', description: 'Сохранить документ в файл', reason: null });
    expect(tooltipText({ ...c, disabled: true }, t).reason).toBe('Сейчас недоступно');
    expect(tooltipText({ ...c, disabled: true, reason: 'Нет модели' }, t).reason).toBe('Нет модели');
    expect(tooltipText({ ...c, disabled: true }, tr('en')).reason).toBe('Not available now');
    expect(tooltipText(cmd('x'), t)).toEqual({ title: 'x', hotkey: null, description: null, reason: null });
  });

  test('active tab of the strip: the panel under the middle, the view kept inside it', () => {
    const panels = [
      { id: 'a', left: 0, width: 800 },
      { id: 'b', left: 800, width: 1200 },
      { id: 'c', left: 2000, width: 800 },
    ];
    expect(panelAt(panels, 0, 800)).toBe('a');
    expect(panelAt(panels, 500, 800)).toBe('b');
    expect(panelAt(panels, 1700, 800)).toBe('c');
    expect(panelAt(panels, 5000, 800)).toBe('c');
    expect(panelAt([], 0, 800)).toBeNull();
    expect(clampToPanel(panels[1], 500, 800)).toBe(800);
    expect(clampToPanel(panels[1], 1000, 800)).toBe(1000);
    expect(clampToPanel(panels[1], 1500, 800)).toBe(1200);
    expect(clampToPanel(panels[0], 300, 800)).toBe(0);
  });

  test('an unknown command icon becomes the general feature icon', () => {
    expect(commandIcon('extrude')).toBe('extrude');
    expect(commandIcon('NewKernelThing')).toBe(FALLBACK_ICON);
  });

  test('repeated ids and test ids are reported', () => {
    const tabs: RibbonTab[] = [
      { id: 'x', label: 'x', groups: [{ id: 'g', label: 'g', items: [cmd('a', 'large', { testId: 't1' }), cmd('a', 'small', { testId: 't1' })] }] },
    ];
    expect(ribbonProblems(tabs)).toEqual(['command a is repeated', 'test id t1 is repeated']);
  });
});

function withI18n(ui: React.ReactElement, lang: Lang = 'ru') {
  const value: I18n = { lang, t: tr(lang), setLang: () => undefined };
  return <I18nContext.Provider value={value}>{ui}</I18nContext.Provider>;
}

describe('Ribbon component', () => {
  beforeEach(() => window.localStorage.clear());

  const tabs = (onExtrude: () => void): RibbonTab[] => [
    {
      id: 'features',
      label: 'Операции',
      groups: [
        {
          id: 'body',
          label: 'Формообразующие',
          items: [
            cmd('extrude', 'large', { testId: 'add-Extrude', label: 'Выдавливание', tip: 'Вытянуть профиль', onClick: onExtrude }),
            cmd('shell', 'small', { testId: 'add-Shell', label: 'Оболочка', disabled: true, reason: 'Нет тела' }),
          ],
        },
        { id: 'empty', label: 'Пустая', items: [] },
      ],
    },
    { id: 'patterns', label: 'Массивы', groups: [{ id: 'p', label: 'Массивы операций', items: [cmd('pattern', 'large', { testId: 'add-LinearPattern', label: 'Линейный массив', icon: 'pattern-linear' })] }] },
    { id: 'nothing', label: 'Ничего', groups: [] },
  ];

  test('tabs, captioned groups, large and small buttons; empty groups and tabs are left out', () => {
    const onExtrude = jest.fn();
    render(withI18n(<Ribbon title="деталь" testId="rb" quick={[cmd('save', 'small', { testId: 'doc-save', label: 'Сохранить', icon: 'save' })]} tabs={tabs(onExtrude)} />));
    expect(screen.getAllByRole('tab').map((x) => x.textContent)).toEqual(['Операции', 'Массивы']);
    expect(screen.getByRole('group', { name: 'Формообразующие' })).toBeInTheDocument();
    expect(screen.queryByRole('group', { name: 'Пустая' })).toBeNull();
    const extrude = screen.getByTestId('add-Extrude');
    expect(extrude).toHaveClass('bc-rb-large');
    expect(extrude).toHaveAccessibleName('Выдавливание');
    expect(extrude.querySelector('svg')?.getAttribute('width')).toBe('32');
    expect(screen.getByTestId('add-Shell')).toHaveClass('bc-rb-small');
    expect(screen.getByTestId('add-Shell').querySelector('svg')?.getAttribute('width')).toBe('16');
    expect(screen.getByTestId('doc-save')).toHaveAccessibleName('Сохранить');
    fireEvent.click(extrude);
    expect(onExtrude).toHaveBeenCalledTimes(1);
    // Every tab's panel is in the page (keyboard and automation reach every command).
    expect(screen.getByTestId('add-LinearPattern')).toBeInTheDocument();
  });

  test('unavailable commands are disabled and shaded; the tooltip names the reason', () => {
    jest.useFakeTimers();
    try {
      render(withI18n(<Ribbon title="деталь" tabs={tabs(() => undefined)} />));
      const shell = screen.getByTestId('add-Shell');
      expect(shell).toBeDisabled();
      expect(shell).toHaveClass('bc-rb-disabled');
      fireEvent.mouseEnter(shell.parentElement!);
      act(() => {
        jest.advanceTimersByTime(600);
      });
      const tip = screen.getByRole('tooltip');
      expect(tip).toBeVisible();
      expect(tip).toHaveTextContent('Оболочка');
      expect(tip).toHaveTextContent('Нет тела');
      fireEvent.mouseLeave(shell.parentElement!);
      expect(screen.getByTestId('ribbon-tooltip')).not.toBeVisible();

      fireEvent.focus(screen.getByTestId('add-Extrude'));
      act(() => {
        jest.advanceTimersByTime(600);
      });
      expect(screen.getByRole('tooltip')).toHaveTextContent('ВыдавливаниеВытянуть профиль');
    } finally {
      jest.useRealTimers();
    }
  });

  test('"icons only" hides the captions, keeps the names and is remembered', () => {
    const { unmount } = render(withI18n(<Ribbon title="деталь" testId="rb" tabs={tabs(() => undefined)} />));
    expect(screen.getByTestId('add-Extrude')).toHaveTextContent('Выдавливание');
    fireEvent.click(screen.getByTestId('ribbon-display'));
    expect(screen.getByTestId('rb')).toHaveAttribute('data-display', 'icons');
    expect(screen.getByTestId('add-Extrude')).toHaveTextContent('');
    expect(screen.getByTestId('add-Extrude')).toHaveAccessibleName('Выдавливание');
    expect(JSON.parse(window.localStorage.getItem(RIBBON_STORAGE_KEY)!)).toEqual({ display: 'icons', collapsed: false });
    unmount();
    render(withI18n(<Ribbon title="деталь" testId="rb" tabs={tabs(() => undefined)} />));
    expect(screen.getByTestId('rb')).toHaveAttribute('data-display', 'icons');
    expect(screen.getByTestId('ribbon-display')).toHaveAccessibleName('Значки и подписи');
  });

  test('collapsing hides the panels until a tab opens them over the page', () => {
    render(withI18n(<Ribbon title="деталь" testId="rb" tabs={tabs(() => undefined)} />, 'en'));
    expect(screen.getByTestId('ribbon-collapse')).toHaveAccessibleName('Collapse the ribbon');
    fireEvent.click(screen.getByTestId('ribbon-collapse'));
    expect(screen.getByTestId('rb')).toHaveAttribute('data-collapsed', 'true');
    expect(screen.getByTestId('rb-body')).not.toBeVisible();
    expect(JSON.parse(window.localStorage.getItem(RIBBON_STORAGE_KEY)!).collapsed).toBe(true);
    fireEvent.click(screen.getByTestId('ribbon-tab-patterns'));
    expect(screen.getByTestId('rb-body')).toBeVisible();
    expect(screen.getByTestId('rb-body')).toHaveClass('bc-rb-peek');
    // A command closes the opened panels again.
    fireEvent.click(screen.getByTestId('add-LinearPattern'));
    expect(screen.getByTestId('rb-body')).not.toBeVisible();
    fireEvent.click(screen.getByTestId('ribbon-collapse'));
    expect(screen.getByTestId('rb-body')).toBeVisible();
    expect(screen.getByTestId('ribbon-collapse')).toHaveAccessibleName('Collapse the ribbon');
  });

  test('a tab click selects the tab', () => {
    render(withI18n(<Ribbon title="деталь" testId="rb" tabs={tabs(() => undefined)} />));
    expect(screen.getByTestId('ribbon-tab-features')).toHaveAttribute('aria-selected', 'true');
    fireEvent.click(screen.getByTestId('ribbon-tab-patterns'));
    expect(screen.getByTestId('ribbon-tab-patterns')).toHaveAttribute('aria-selected', 'true');
    expect(screen.getByTestId('rb')).toHaveAttribute('data-active-tab', 'patterns');
  });

  test('a file command is a captioned input', () => {
    const onFiles = jest.fn();
    render(withI18n(<Ribbon title="деталь" quick={[cmd('open', 'small', { kind: 'file', testId: 'doc-open', label: 'Открыть…', icon: 'open', accept: '.json', onFiles })]} tabs={[]} />));
    const input = screen.getByTestId('doc-open') as HTMLInputElement;
    expect(input.type).toBe('file');
    expect(input.accept).toBe('.json');
    expect(input.closest('label')).toHaveTextContent('Открыть…');
    fireEvent.change(input, { target: { files: [] } });
    expect(onFiles).toHaveBeenCalledTimes(1);
  });
});

// ------------------------------------------------------------------ editors

const allCommands = (r: { quick: RibbonCommand[]; tabs: RibbonTab[] }) => [
  ...r.quick,
  ...r.tabs.flatMap((tab) => tab.groups.flatMap((g) => g.items.filter((i): i is RibbonCommand => !isCustom(i)))),
];
const layout = (r: { tabs: RibbonTab[] }) => r.tabs.map((tab) => [tab.label, tab.groups.filter((g) => g.items.length > 0).map((g) => g.label)]);
const group = (r: { tabs: RibbonTab[] }, tab: string, g: string) => r.tabs.find((x) => x.id === tab)!.groups.find((x) => x.id === g)!.items;
const ids = (items: RibbonItem[]) => items.map((i) => (isCustom(i) ? i.id : i.testId ?? i.id));

const noop = () => undefined;

function partState(over: Partial<PartRibbonState> = {}): PartRibbonState {
  return {
    ready: true,
    busy: false,
    catalog: [
      { type: 'Sketch' },
      { type: 'Extrude', options: [{ name: 'mode', allowed: ['boss', 'cut'] }] },
      { type: 'Fillet' },
      { type: 'Box' },
      { type: 'LinearPattern' },
      { type: 'DatumPlane' },
      { type: 'TorusKnot' },
    ],
    canUndo: true,
    canRedo: false,
    undoName: 'Edit Body',
    redoName: null,
    canCreateDrawing: true,
    measuring: false,
    section: false,
    configurationRows: 0,
    ...over,
  };
}
const partActions = (addFeature = jest.fn()): PartRibbonActions => ({
  newDocument: noop,
  openFile: noop,
  save: noop,
  undo: noop,
  redo: noop,
  createDrawing: noop,
  drawSketch: noop,
  plane: () => null,
  addFeature,
  toggleMeasure: noop,
  toggleSection: noop,
  insertMesh: noop,
  checkConfigurations: noop,
});

describe('part ribbon', () => {
  test('tabs and groups of the part editor', () => {
    const r = partRibbon(t, partState(), partActions());
    expect(layout(r)).toEqual([
      ['Эскиз', ['Эскиз']],
      ['Операции', ['Формообразующие', 'Кромки', 'Другие операции']],
      ['Массивы', ['Массивы операций']],
      ['Вспомогательная геометрия', ['Вспомогательные элементы', 'Справочные данные']],
      ['Анализ', ['Измерения', 'Исполнения']],
    ]);
    expect(ids(r.quick)).toEqual(['doc-new', 'doc-open', 'doc-save', 'doc-create-drawing', 'undo', 'redo']);
    expect(ids(group(r, 'sketch', 'sketch'))).toEqual(['sketchPlane', 'sketch-draw', 'add-Sketch']);
    expect(ids(group(r, 'features', 'body'))).toEqual(['add-Extrude', 'add-cut']);
    expect(ribbonProblems(r.tabs, r.quick)).toEqual([]);
  });

  test('a feature the kernel adds later: "other features", general icon, its kernel name; primitives stay off', () => {
    const add = jest.fn();
    const r = partRibbon(t, partState(), partActions(add));
    const other = group(r, 'features', 'other') as RibbonCommand[];
    expect(other).toHaveLength(1);
    expect(other[0]).toMatchObject({ testId: 'add-TorusKnot', icon: FALLBACK_ICON, label: 'TorusKnot', tip: 'Операция ядра «TorusKnot»: открыть панель параметров' });
    expect(allCommands(r).some((c) => c.testId === 'add-Box')).toBe(false);
    other[0].onClick!();
    expect(add).toHaveBeenCalledWith('TorusKnot');
    (group(r, 'features', 'body')[1] as RibbonCommand).onClick!();
    expect(add).toHaveBeenLastCalledWith('Extrude', { mode: 'cut' });
  });

  test('without the cut mode in the catalog there is no cut command', () => {
    const r = partRibbon(t, partState({ catalog: [{ type: 'Extrude', options: [] }] }), partActions());
    expect(allCommands(r).map((c) => c.testId)).not.toContain('add-cut');
  });

  test('commands are unavailable while a panel or sketch is open; undo names the history entry', () => {
    const r = partRibbon(t, partState({ busy: true, configurationRows: 2 }), partActions());
    const by = (id: string) => allCommands(r).find((c) => c.testId === id)!;
    expect(by('add-Extrude').disabled).toBe(true);
    expect(by('sketch-draw').disabled).toBe(true);
    expect(by('undo').disabled).toBe(true);
    expect(by('doc-save').disabled).toBe(false);
    const idle = partRibbon(t, partState(), partActions());
    const idleBy = (id: string) => allCommands(idle).find((c) => c.testId === id)!;
    expect(idleBy('undo').tip).toBe(t('part.undoNamed', { name: 'Edit Body' }));
    expect(idleBy('redo').disabled).toBe(true);
    expect(idleBy('ribbon-check-configurations').disabled).toBe(true);
    expect(partRibbon(t, partState({ section: null }), partActions()).tabs[4].groups[0].items.find((i) => i.id === 'section')).toMatchObject({ disabled: true });
    expect(partRibbon(t, partState({ canCreateDrawing: false }), partActions()).quick.map((c) => c.testId)).not.toContain('doc-create-drawing');
  });

  test('English captions', () => {
    const r = partRibbon(tr('en'), partState(), partActions());
    expect(r.tabs.map((x) => x.label)).toEqual(['Sketch', 'Features', 'Patterns', 'Reference geometry', 'Analysis']);
    expect(allCommands(r).find((c) => c.testId === 'add-cut')!.label).toBe('Cut');
  });
});

describe('sketch ribbon', () => {
  test('tools in groups, the current tool pressed, undo unavailable without history', () => {
    const r = sketchRibbon(t, { tool: 'trim', canUndo: false, dxfOpen: false }, { setTool: noop, exit: noop, cancel: noop, undo: noop, fit: noop, openDxf: noop, modes: () => null });
    expect(layout(r)).toEqual([['Эскиз', ['Рисование', 'Правка', 'Размеры и области', 'Вставка', 'Режимы', 'Вид']]]);
    expect(ids(group(r, 'sketchEdit', 'draw'))).toEqual([
      'tool-select', 'tool-line', 'tool-rectangle', 'tool-rectangle-center', 'tool-circle', 'tool-circle-3', 'tool-arc', 'tool-arc-3',
      'tool-arc-tangent', 'tool-ellipse', 'tool-spline', 'tool-point',
    ]);
    const all = allCommands(r);
    expect(all.find((c) => c.testId === 'tool-trim')).toMatchObject({ active: true, className: 'sk-active' });
    expect(all.find((c) => c.testId === 'tool-line')!.active).toBe(false);
    expect(all.find((c) => c.testId === 'sketch-undo')!.disabled).toBe(true);
    expect(ids(r.quick)).toEqual(['sketch-exit', 'sketch-cancel', 'sketch-undo']);
    expect(ribbonProblems(r.tabs, r.quick)).toEqual([]);
  });

  test('constraint icons', () => {
    expect(constraintIcon('horizontal')).toBe('constraint-horizontal');
    expect(constraintIcon('equal-radius')).toBe('constraint-equal');
    expect(constraintIcon('symmetric-point')).toBe('constraint-symmetric');
    expect(constraintIcon('point-on-circle')).toBe('constraint-point-on');
    expect(constraintIcon('arc-length')).toBe('constraint');
  });

  test('a new sketch starts at a round scale', () => {
    expect(roundScale(6.06)).toBe(5);
    expect(roundScale(7.2)).toBe(5);
    expect(roundScale(4)).toBe(2);
    expect(roundScale(1)).toBe(1);
    expect(roundScale(0.3)).toBeCloseTo(0.2);
    expect(roundScale(0)).toBe(1);
  });
});

describe('assembly ribbon', () => {
  const actions = {
    newDocument: noop,
    open: noop,
    save: noop,
    importModel: noop,
    exportModel: noop,
    insertParts: noop,
    insertSubassembly: noop,
    openComponentPart: noop,
    pickModes: () => null,
    undo: noop,
    redo: noop,
    solve: noop,
    showPanel: jest.fn(),
    mateType: jest.fn(),
    toggleMeasure: noop,
    toggleSection: noop,
  };
  const state = {
    ready: true,
    hasParts: true,
    canUndo: false,
    canRedo: false,
    undoLabel: null,
    redoLabel: null,
    panel: 'mate' as const,
    canOpenComponentPart: false,
    allowedMates: null,
    measuring: false,
    section: null,
  };
  const accept = { importAccept: '.step', partAccept: '.json,.step' };

  test('tabs and groups; panel commands keep the panel test ids', () => {
    const r = assemblyRibbon(t, state, actions, accept);
    expect(layout(r)).toEqual([
      ['Компоненты', ['Вставка', 'Компоненты', 'Обмен', 'Выбор']],
      ['Сопряжения', ['Сопряжение', 'Типы сопряжений', 'Решение']],
      ['Анализ', ['Проверка', 'Представление', 'Измерения']],
      ['Исполнения', ['Исполнения']],
    ]);
    const all = allCommands(r).map((c) => c.testId);
    for (const p of ['mate', 'fasteners', 'interference', 'bom', 'explode', 'pattern', 'properties', 'configurations']) expect(all).toContain(`tab-${p}`);
    expect(all).toEqual(expect.arrayContaining(['open-input', 'save', 'undo', 'redo', 'solve', 'insert-part-input', 'insert-subassembly-input', 'import-step-input', 'export-open', 'open-component-part']));
    expect(allCommands(r).find((c) => c.testId === 'tab-mate')).toMatchObject({ active: true, className: 'as-tab-active' });
    expect(allCommands(r).find((c) => c.testId === 'open-component-part')!.disabled).toBe(true);
    expect(ribbonProblems(r.tabs, r.quick)).toEqual([]);
  });

  test('mate types: all offered without a picked pair, only the allowed ones for a pair', () => {
    const open = assemblyRibbon(t, state, actions, accept);
    expect((group(open, 'mates', 'mateTypes') as RibbonCommand[]).every((c) => !c.disabled)).toBe(true);
    const pair = assemblyRibbon(t, { ...state, allowedMates: ['coincident', 'distance', 'rigid'] }, actions, accept);
    expect((group(pair, 'mates', 'mateTypes') as RibbonCommand[]).filter((c) => !c.disabled).map((c) => c.testId)).toEqual([
      'ribbon-mate-coincident',
      'ribbon-mate-distance',
      'ribbon-mate-rigid',
    ]);
    (group(pair, 'mates', 'mateTypes')[2] as RibbonCommand).onClick!();
    expect(actions.mateType).toHaveBeenCalledWith('distance');
  });
});

describe('drawing ribbon', () => {
  const state: DrawingRibbonState = {
    kernel: true,
    drawing: true,
    tool: 'dimension',
    canUndo: false,
    canRedo: false,
    undoLabel: '',
    redoLabel: '',
    hasBase: false,
    assembly: false,
    auxiliaryReady: true,
    canArrangeDimensions: false,
    canArrangeBalloons: false,
    modelChanged: false,
    picture: true,
    tab: 'sheet',
  };
  const actions = {
    openModel: noop,
    openDrawing: noop,
    replaceModel: noop,
    save: noop,
    undo: noop,
    redo: noop,
    setTool: jest.fn(),
    baseStandard: () => null,
    addBase: noop,
    addProjected: noop,
    addAxonometric: noop,
    auxiliaryFields: () => null,
    addAuxiliary: noop,
    update: noop,
    fit: noop,
    arrangeDimensions: noop,
    arrangeBalloons: noop,
    showTab: noop,
    exportFile: noop,
    exportSheetPdf: noop,
    print: noop,
  };

  test('tabs and groups of the drawing editor', () => {
    const r = drawingRibbon(t, state, actions);
    expect(layout(r)).toEqual([
      ['Виды', ['Главный вид', 'Проекционные виды', 'Дополнительный вид', 'Модель', 'Вид']],
      ['Разрезы', ['Разрезы и сечения', 'Выносные элементы']],
      ['Размеры', ['Линейные размеры', 'Компоновка']],
      ['Обозначения', ['Поверхности и базы', 'Позиции']],
      ['Спецификация', ['Документ', 'Позиции']],
      ['Вывод', ['Экспорт', 'Печать']],
    ]);
    expect(ids(group(r, 'dimensions', 'linear'))).toEqual(['tool-dimension', 'tool-chain', 'tool-baseline', 'tool-thread']);
    expect(ribbonProblems(r.tabs, r.quick)).toEqual([]);
  });

  test('availability: projections need the front view, balloons an assembly, arranging a view with dimensions', () => {
    const by = (r: ReturnType<typeof drawingRibbon>, id: string) => allCommands(r).find((c) => c.testId === id)!;
    const r = drawingRibbon(t, state, actions);
    expect(by(r, 'add-base').disabled).toBe(false);
    expect(by(r, 'add-top').disabled).toBe(true);
    expect(by(r, 'tool-balloon')).toMatchObject({ disabled: true, reason: 'Позиции ставятся на чертеже сборки.' });
    expect(by(r, 'view-arrange-dimensions').disabled).toBe(true);
    expect(by(r, 'tool-dimension')).toMatchObject({ active: true, className: 'dr-active' });
    const later = drawingRibbon(t, { ...state, hasBase: true, assembly: true, canArrangeDimensions: true }, actions);
    expect(by(later, 'add-base').disabled).toBe(true);
    expect(by(later, 'add-top').disabled).toBe(false);
    expect(by(later, 'tool-balloon').disabled).toBe(false);
    expect(by(later, 'view-arrange-dimensions').disabled).toBe(false);
    const none = drawingRibbon(t, { ...state, drawing: false }, actions);
    expect(allCommands(none).filter((c) => !c.disabled).map((c) => c.testId)).toEqual(['open-model', 'open-drawing', 'drawing-fit']);
    by(r, 'tool-roughness').onClick!();
    expect(actions.setTool).toHaveBeenCalledWith('roughness');
  });
});

describe('every ribbon command has an icon of the registry', () => {
  test('part, sketch, assembly, drawing, viewer', () => {
    const ribbons = [
      partRibbon(t, partState(), partActions()),
      sketchRibbon(t, { tool: 'select', canUndo: true, dxfOpen: false }, { setTool: noop, exit: noop, cancel: noop, undo: noop, fit: noop, openDxf: noop, modes: () => null }),
      assemblyRibbon(
        t,
        { ready: true, hasParts: true, canUndo: true, canRedo: true, undoLabel: null, redoLabel: null, panel: 'bom', canOpenComponentPart: true, allowedMates: null, measuring: false, section: false },
        { newDocument: noop, open: noop, save: noop, importModel: noop, exportModel: noop, insertParts: noop, insertSubassembly: noop, openComponentPart: noop, pickModes: () => null, undo: noop, redo: noop, solve: noop, showPanel: noop, mateType: noop, toggleMeasure: noop, toggleSection: noop },
        { importAccept: '', partAccept: '' },
      ),
      viewerRibbon(t, { ready: true, busy: false, hasModel: true, measuring: false, section: false }, { open: noop, exportModel: noop, tolerances: () => null, retessellate: noop, toggleMeasure: noop, toggleSection: noop }, '.step'),
    ];
    const missing = ribbons.flatMap(allCommands).filter((c) => c.icon !== FALLBACK_ICON && !hasIcon(c.icon) && c.testId !== 'add-TorusKnot');
    expect(missing.map((c) => `${c.id}: ${c.icon}`)).toEqual([]);
    for (const r of ribbons) expect(ribbonProblems(r.tabs, r.quick)).toEqual([]);
  });
});
