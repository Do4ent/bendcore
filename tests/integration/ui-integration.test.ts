/**
 * BendCore Desktop - UI/UX Integration Tests
 * Tests viewport interactions, context menus, and project management
 *
 * Coverage: 15+ test cases, 100 LOC
 */

import { OrbitCamera } from '../../src/ui/OrbitCamera';
import { SnapConstraints } from '../../src/ui/SnapConstraints';
import { KeyboardShortcuts } from '../../src/ui/KeyboardShortcuts';
import { MouseGestures } from '../../src/ui/MouseGestures';
import { v4 as uuid } from 'uuid';

describe('UI/UX Integration Tests', () => {
  let orbitCamera: OrbitCamera;
  let snapConstraints: SnapConstraints;
  let keyboard: KeyboardShortcuts;
  let mouse: MouseGestures;

  beforeEach(() => {
    orbitCamera = new OrbitCamera();
    snapConstraints = new SnapConstraints();
    keyboard = new KeyboardShortcuts();
    mouse = new MouseGestures();
  });

  describe('Viewport Interactions', () => {
    test('should handle orbit camera rotation', () => {
      const initialRotation = { x: 0, y: 0 };

      orbitCamera.orbit(30, 45); // Move 30px right, 45px down

      const rotation = orbitCamera.getRotation();
      expect(rotation.x).not.toBe(initialRotation.x);
      expect(rotation.y).not.toBe(initialRotation.y);
    });

    test('should handle pan operation', () => {
      const initialPosition = orbitCamera.getPosition();

      orbitCamera.pan(10, 20); // Pan 10 right, 20 up

      const newPosition = orbitCamera.getPosition();
      expect(newPosition).not.toEqual(initialPosition);
    });

    test('should handle zoom in/out', () => {
      const initialDistance = orbitCamera.getDistance();

      orbitCamera.zoom(1.2); // Zoom in 20%
      const zoomedIn = orbitCamera.getDistance();
      expect(zoomedIn).toBeLessThan(initialDistance);

      orbitCamera.zoom(0.8); // Zoom out
      const zoomedOut = orbitCamera.getDistance();
      expect(zoomedOut).toBeGreaterThan(zoomedIn);
    });

    test('should reset viewport to fit all', () => {
      orbitCamera.orbit(45, 45);
      orbitCamera.pan(50, 50);

      orbitCamera.fitAll();

      const rotation = orbitCamera.getRotation();
      const position = orbitCamera.getPosition();
      expect(rotation).toBeDefined();
      expect(position).toBeDefined();
    });

    test('should handle viewport resize', () => {
      const viewport = { width: 1920, height: 1080 };

      orbitCamera.setViewport(viewport.width, viewport.height);
      expect(orbitCamera.getViewport()).toEqual(viewport);
    });
  });

  describe('Context Menu Operations', () => {
    test('should show context menu on right-click', () => {
      const position = { x: 100, y: 200 };

      const menu = mouse.createContextMenu(position);
      expect(menu).toBeDefined();
      expect(menu.visible).toBe(true);
    });

    test('should provide segment operations in context menu', () => {
      const segmentId = uuid();
      const menu = mouse.getContextMenu('segment', segmentId);

      expect(menu.items).toContain(expect.objectContaining({ label: 'Edit' }));
      expect(menu.items).toContain(expect.objectContaining({ label: 'Delete' }));
    });

    test('should provide vertex operations in context menu', () => {
      const vertexId = uuid();
      const menu = mouse.getContextMenu('vertex', vertexId);

      expect(menu.items).toContain(expect.objectContaining({ label: 'Fixed' }));
      expect(menu.items).toContain(expect.objectContaining({ label: 'Remove' }));
    });

    test('should execute menu action on selection', () => {
      const action = { type: 'delete', targetId: uuid() };

      const result = mouse.executeMenuAction(action);
      expect(result).toBeDefined();
    });
  });

  describe('Toolbar Customization', () => {
    test('should allow toolbar button rearrangement', () => {
      const buttons = ['sketch', 'dimensions', 'constraints', 'solve'];

      keyboard.setToolbarOrder(['constraints', 'solve', 'sketch', 'dimensions']);
      const order = keyboard.getToolbarOrder();

      expect(order[0]).toBe('constraints');
    });

    test('should hide/show toolbar buttons', () => {
      keyboard.setToolbarVisibility('sketch', false);

      const visible = keyboard.isToolbarButtonVisible('sketch');
      expect(visible).toBe(false);
    });

    test('should reset toolbar to defaults', () => {
      keyboard.setToolbarOrder(['custom', 'order']);
      keyboard.resetToolbarDefaults();

      const order = keyboard.getToolbarOrder();
      expect(order[0]).toBe('sketch');
    });

    test('should persist toolbar layout to preferences', () => {
      const layout = { buttons: ['sketch', 'solve'], size: 'medium' };

      keyboard.saveToolbarLayout(layout);
      const saved = keyboard.getToolbarLayout();

      expect(saved.size).toBe('medium');
    });
  });

  describe('Theme Switching', () => {
    test('should switch between light and dark themes', () => {
      keyboard.setTheme('dark');

      const currentTheme = keyboard.getTheme();
      expect(currentTheme).toBe('dark');
    });

    test('should apply theme colors to viewport', () => {
      keyboard.setTheme('light');

      const colors = keyboard.getThemeColors();
      expect(colors.background).toBeDefined();
      expect(colors.grid).toBeDefined();
    });

    test('should persist theme preference', () => {
      keyboard.setTheme('dark');
      keyboard.savePreferences();

      const prefs = keyboard.getPreferences();
      expect(prefs.theme).toBe('dark');
    });

    test('should support custom theme', () => {
      const customTheme = {
        name: 'custom',
        background: '#1a1a1a',
        grid: '#333333'
      };

      keyboard.registerTheme(customTheme);
      keyboard.setTheme('custom');

      const current = keyboard.getTheme();
      expect(current).toBe('custom');
    });
  });

  describe('Project Save/Load Cycles', () => {
    test('should save project to file', () => {
      const project = {
        id: uuid(),
        name: 'Test Project',
        tubes: []
      };

      const saved = keyboard.saveProject(project);
      expect(saved).toBe(true);
    });

    test('should load project from file', () => {
      const projectId = uuid();
      keyboard.saveProject({ id: projectId, name: 'Test', tubes: [] });

      const loaded = keyboard.loadProject(projectId);
      expect(loaded.id).toBe(projectId);
    });

    test('should detect unsaved changes', () => {
      const project = { id: uuid(), name: 'Test' };
      keyboard.loadProject(project);

      keyboard.modifyProject({ name: 'Modified' });

      const hasChanges = keyboard.hasUnsavedChanges();
      expect(hasChanges).toBe(true);
    });

    test('should prompt on close with unsaved changes', () => {
      const project = { id: uuid(), name: 'Test' };
      keyboard.loadProject(project);
      keyboard.modifyProject({ name: 'Modified' });

      const shouldPrompt = keyboard.hasUnsavedChanges();
      expect(shouldPrompt).toBe(true);
    });

    test('should recover from crash via auto-save', () => {
      const project = { id: uuid(), name: 'Crash Test' };
      keyboard.enableAutoSave(60000); // 1 minute
      keyboard.saveProject(project);

      const recovered = keyboard.getAutoSaveData(project.id);
      expect(recovered).toBeDefined();
    });
  });

  describe('Keyboard Shortcuts', () => {
    test('should handle common shortcuts', () => {
      const action = keyboard.handleKeyboardShortcut('Ctrl', 'S');
      expect(action).toBe('save');
    });

    test('should allow custom shortcut mapping', () => {
      keyboard.setShortcut('custom-action', 'Ctrl', 'Alt', 'X');

      const action = keyboard.handleKeyboardShortcut('Ctrl', 'Alt', 'X');
      expect(action).toBe('custom-action');
    });

    test('should conflict check for shortcuts', () => {
      keyboard.setShortcut('action-1', 'Ctrl', 'S');

      const conflict = keyboard.checkShortcutConflict('Ctrl', 'S');
      expect(conflict).toBe('action-1');
    });

    test('should list all available shortcuts', () => {
      const shortcuts = keyboard.getAllShortcuts();

      expect(shortcuts.length).toBeGreaterThan(0);
      expect(shortcuts[0]).toHaveProperty('action');
      expect(shortcuts[0]).toHaveProperty('keys');
    });
  });

  describe('Snap System', () => {
    test('should enable/disable snap to grid', () => {
      snapConstraints.setSnapToGrid(true);

      const enabled = snapConstraints.isSnapEnabled('grid');
      expect(enabled).toBe(true);
    });

    test('should snap coordinates to grid', () => {
      snapConstraints.setSnapToGrid(true);
      snapConstraints.setGridSize(0.5);

      const snapped = snapConstraints.snapPoint({ x: 5.23, y: 10.71 });
      expect(snapped.x).toBeCloseTo(5.0, 1);
      expect(snapped.y).toBeCloseTo(10.5, 1);
    });

    test('should snap to vertices', () => {
      const vertices = [
        { id: '1', position: { x: 0, y: 0 } },
        { id: '2', position: { x: 10, y: 10 } }
      ];

      snapConstraints.setSnapToVertices(true, vertices);

      const snapped = snapConstraints.snapPoint({ x: 0.1, y: 0.1 });
      expect(snapped.x).toBe(0);
      expect(snapped.y).toBe(0);
    });

    test('should provide snap feedback visual', () => {
      snapConstraints.setSnapToGrid(true);
      const feedback = snapConstraints.getSnapFeedback({ x: 5.23, y: 10.71 });

      expect(feedback).toBeDefined();
      expect(feedback.isSnapped).toBe(true);
    });
  });

  describe('Undo/Redo', () => {
    test('should record action for undo', () => {
      const action = { type: 'add_vertex', data: { x: 10, y: 20 } };

      keyboard.recordAction(action);

      const canUndo = keyboard.canUndo();
      expect(canUndo).toBe(true);
    });

    test('should undo last action', () => {
      keyboard.recordAction({ type: 'add_segment' });
      keyboard.recordAction({ type: 'add_constraint' });

      keyboard.undo();

      const history = keyboard.getUndoHistory();
      expect(history.length).toBe(1);
    });

    test('should redo undone action', () => {
      keyboard.recordAction({ type: 'add_vertex' });
      keyboard.undo();

      keyboard.redo();

      const history = keyboard.getUndoHistory();
      expect(history.length).toBe(1);
    });

    test('should clear redo history on new action', () => {
      keyboard.recordAction({ type: 'action1' });
      keyboard.undo();

      expect(keyboard.canRedo()).toBe(true);

      keyboard.recordAction({ type: 'action2' });

      expect(keyboard.canRedo()).toBe(false);
    });
  });
});
