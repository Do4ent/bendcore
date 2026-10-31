/**
 * Unit tests for the workspace state behind KeyboardShortcuts
 */

import {
  KeyboardShortcuts,
  initializeShortcutListeners,
  normalizeShortcut,
} from '../src/ui/KeyboardShortcuts';
import { ActionHistory, ProjectSession, ThemeRegistry, ToolbarState } from '../src/ui/WorkspaceState';

describe('normalizeShortcut', () => {
  test('orders modifiers and upper-cases single-character keys', () => {
    expect(normalizeShortcut(['shift', 'x', 'Ctrl'])).toBe('Ctrl+Shift+X');
    expect(normalizeShortcut(['Ctrl+Alt+c'])).toBe('Ctrl+Alt+C');
    expect(normalizeShortcut(['Cmd', 'S'])).toBe('Meta+S');
    expect(normalizeShortcut(['F2'])).toBe('F2');
  });

  test('rejects combinations without exactly one key', () => {
    expect(() => normalizeShortcut(['Ctrl'])).toThrow();
    expect(() => normalizeShortcut(['Ctrl', 'A', 'B'])).toThrow();
  });
});

describe('KeyboardShortcuts', () => {
  test('maps the built-in shortcut table regardless of modifier order', () => {
    const keyboard = new KeyboardShortcuts();
    expect(keyboard.handleKeyboardShortcut('Ctrl', 'Shift', 'S')).toBe('save-as');
    expect(keyboard.handleKeyboardShortcut('Shift', 'Ctrl', 'S')).toBe('save-as');
    expect(keyboard.handleKeyboardShortcut('Ctrl', 'Q')).toBeUndefined();
  });

  test('rebinding replaces the previous action for that combination', () => {
    const keyboard = new KeyboardShortcuts();
    keyboard.setShortcut('quick-save', 'Ctrl', 'S');
    expect(keyboard.handleKeyboardShortcut('Ctrl', 'S')).toBe('quick-save');
    expect(keyboard.getAllShortcuts().filter((s) => s.keys.join('+') === 'Ctrl+S')).toEqual([
      { action: 'quick-save', keys: ['Ctrl', 'S'] },
    ]);
  });

  test('getPreferences is null until preferences are saved', () => {
    const keyboard = new KeyboardShortcuts();
    expect(keyboard.getPreferences()).toBeNull();
    keyboard.setToolbarVisibility('solve', false);
    const prefs = keyboard.savePreferences();
    expect(prefs.hiddenToolbarButtons).toEqual(['solve']);
    expect(prefs.theme).toBe('light');
  });
});

describe('initializeShortcutListeners', () => {
  test('dispatches registered shortcuts and can be removed', () => {
    const onCommand = jest.fn();
    const dispose = initializeShortcutListeners(onCommand);

    window.dispatchEvent(new KeyboardEvent('keydown', { key: 's', ctrlKey: true }));
    expect(onCommand).toHaveBeenCalledWith('save');

    dispose();
    window.dispatchEvent(new KeyboardEvent('keydown', { key: 's', ctrlKey: true }));
    expect(onCommand).toHaveBeenCalledTimes(1);
  });
});

describe('ActionHistory', () => {
  test('undo/redo move actions between stacks and new actions clear redo', () => {
    const history = new ActionHistory();
    history.record({ type: 'a' });
    history.record({ type: 'b' });
    expect(history.undo()).toEqual({ type: 'b' });
    expect(history.canRedo()).toBe(true);
    expect(history.redo()).toEqual({ type: 'b' });
    history.undo();
    history.record({ type: 'c' });
    expect(history.canRedo()).toBe(false);
    expect(history.getHistory().map((a) => a.type)).toEqual(['a', 'c']);
    expect(history.undo()).toEqual({ type: 'c' });
  });

  test('respects the history limit and rejects untyped actions', () => {
    const history = new ActionHistory(2);
    ['a', 'b', 'c'].forEach((type) => history.record({ type }));
    expect(history.getHistory().map((a) => a.type)).toEqual(['b', 'c']);
    expect(() => history.record({ type: '' })).toThrow();
  });
});

describe('ProjectSession', () => {
  afterEach(() => {
    jest.useRealTimers();
  });

  test('loading an unknown project id fails', () => {
    expect(() => new ProjectSession().load('missing')).toThrow('Project not found: missing');
  });

  test('saving clears unsaved changes and stores an independent copy', () => {
    const session = new ProjectSession();
    const project = { id: 'p1', name: 'Frame', tubes: [] as string[] };
    expect(session.save(project)).toBe(true);
    project.tubes.push('mutated after save');

    session.modify({ name: 'Frame v2' });
    expect(session.hasUnsavedChanges()).toBe(true);
    expect(session.load('p1')).toEqual({ id: 'p1', name: 'Frame', tubes: [] });
    expect(session.hasUnsavedChanges()).toBe(false);
  });

  test('modify keeps the project id and requires an open project', () => {
    const session = new ProjectSession();
    expect(() => session.modify({ name: 'x' })).toThrow('No project is open');
    session.load({ id: 'p1', name: 'A' });
    expect(session.modify({ id: 'other', name: 'B' })).toEqual({ id: 'p1', name: 'B' });
  });

  test('auto-save snapshots the open project periodically', () => {
    jest.useFakeTimers();
    const session = new ProjectSession();
    session.load({ id: 'p1', name: 'A' });
    session.enableAutoSave(1000);
    session.modify({ name: 'B' });

    expect(session.getAutoSave('p1')).toBeUndefined();
    jest.advanceTimersByTime(1000);
    expect(session.getAutoSave('p1')?.project).toEqual({ id: 'p1', name: 'B' });

    session.disableAutoSave();
    session.modify({ name: 'C' });
    jest.advanceTimersByTime(5000);
    expect(session.getAutoSave('p1')?.project.name).toBe('B');
  });
});

describe('ToolbarState and ThemeRegistry', () => {
  test('toolbar order is de-duplicated and layouts update the order', () => {
    const toolbar = new ToolbarState();
    toolbar.setOrder(['solve', 'sketch', 'solve']);
    expect(toolbar.getOrder()).toEqual(['solve', 'sketch']);
    toolbar.saveLayout({ buttons: ['dimensions'], size: 'large' });
    expect(toolbar.getOrder()).toEqual(['dimensions']);
  });

  test('unknown or incomplete themes are rejected', () => {
    const themes = new ThemeRegistry();
    expect(() => themes.set('neon')).toThrow('Unknown theme: neon');
    expect(() =>
      themes.register({ name: 'broken', background: '#000' } as unknown as Parameters<ThemeRegistry['register']>[0])
    ).toThrow();
    expect(themes.get()).toBe('light');
  });
});
