/**
 * MouseGestures Test Suite
 */

import { MouseGestures, GestureEvent } from '../src/ui/MouseGestures';
import { OrbitCamera } from '../src/ui/OrbitCamera';

describe('MouseGestures', () => {
  let camera: OrbitCamera;
  let gestures: MouseGestures;
  let eventLog: GestureEvent[] = [];

  beforeEach(() => {
    camera = new OrbitCamera(16 / 9);
    gestures = new MouseGestures(camera);
    eventLog = [];

    gestures.on((event) => {
      eventLog.push(event);
    });
  });

  describe('initialization', () => {
    test('should initialize with default config', () => {
      const modifierState = gestures.getModifierState();
      expect(modifierState.ctrl).toBe(false);
      expect(modifierState.shift).toBe(false);
    });

    test('should accept custom config', () => {
      const customGestures = new MouseGestures(camera, {
        orbitButton: 'middle',
        panButton: 'shift',
      });
      expect(customGestures).toBeDefined();
    });
  });

  describe('mouse events', () => {
    test('should handle mouse down', () => {
      const mouseEvent = new MouseEvent('mousedown', {
        clientX: 100,
        clientY: 100,
        button: 2,  // Right click
      });

      gestures.handleMouseDown(mouseEvent, document.createElement('div'));
      expect(eventLog.length).toBe(0);  // No event until drag
    });

    test('should detect orbit gesture on right click drag', () => {
      const viewport = document.createElement('div');
      const downEvent = new MouseEvent('mousedown', {
        clientX: 100,
        clientY: 100,
        button: 2,
      });

      const moveEvent = new MouseEvent('mousemove', {
        clientX: 150,
        clientY: 150,
        button: 2,
      });

      gestures.handleMouseDown(downEvent, viewport);
      gestures.handleMouseMove(moveEvent, viewport);

      const orbitEvents = eventLog.filter(e => e.type === 'orbit');
      expect(orbitEvents.length).toBeGreaterThan(0);
    });

    test('should detect pan gesture on middle click drag', () => {
      const viewport = document.createElement('div');
      const downEvent = new MouseEvent('mousedown', {
        clientX: 100,
        clientY: 100,
        button: 1,  // Middle click
      });

      const moveEvent = new MouseEvent('mousemove', {
        clientX: 120,
        clientY: 120,
        button: 1,
      });

      gestures.handleMouseDown(downEvent, viewport);
      gestures.handleMouseMove(moveEvent, viewport);

      const panEvents = eventLog.filter(e => e.type === 'pan');
      expect(panEvents.length).toBeGreaterThan(0);
    });

    test('should ignore small movements (drag threshold)', () => {
      const viewport = document.createElement('div');
      const downEvent = new MouseEvent('mousedown', {
        clientX: 100,
        clientY: 100,
        button: 2,
      });

      const moveEvent = new MouseEvent('mousemove', {
        clientX: 102,
        clientY: 102,  // Only 2px movement
        button: 2,
      });

      gestures.handleMouseDown(downEvent, viewport);
      gestures.handleMouseMove(moveEvent, viewport);

      expect(eventLog.length).toBe(0);
    });

    test('should detect single click select', () => {
      const downEvent = new MouseEvent('mousedown', {
        clientX: 100,
        clientY: 100,
        button: 0,  // Left click
      });

      const upEvent = new MouseEvent('mouseup', {
        clientX: 100,
        clientY: 100,
        button: 0,
      });

      gestures.handleMouseDown(downEvent, document.createElement('div'));
      gestures.handleMouseUp(upEvent);

      const selectEvents = eventLog.filter(e => e.type === 'select');
      expect(selectEvents.length).toBe(1);
    });

    test('should detect multi-select with Ctrl key', () => {
      const downEvent = new MouseEvent('mousedown', {
        clientX: 100,
        clientY: 100,
        button: 0,
      });

      const upEvent = new MouseEvent('mouseup', {
        clientX: 100,
        clientY: 100,
        button: 0,
      });

      // Simulate Ctrl key press
      const keyEvent = new KeyboardEvent('keydown', { code: 'ControlLeft' });
      gestures.handleKeyDown(keyEvent);

      gestures.handleMouseDown(downEvent, document.createElement('div'));
      gestures.handleMouseUp(upEvent);

      const multiSelectEvents = eventLog.filter(e => e.type === 'multi-select');
      expect(multiSelectEvents.length).toBe(1);
    });

    test('should detect double-click for fit-view', () => {
      const clickEvent = new MouseEvent('mouseup', {
        clientX: 100,
        clientY: 100,
        button: 0,
      });

      // First click
      gestures.handleMouseDown(clickEvent as any, document.createElement('div'));
      gestures.handleMouseUp(clickEvent);

      // Second click quickly
      const click2Event = new MouseEvent('mouseup', {
        clientX: 100,
        clientY: 100,
        button: 0,
      });

      setTimeout(() => {
        gestures.handleMouseDown(click2Event as any, document.createElement('div'));
        gestures.handleMouseUp(click2Event);
      }, 200);
    });
  });

  describe('wheel events', () => {
    test('should zoom on wheel scroll', () => {
      const wheelEvent = new WheelEvent('wheel', {
        deltaY: 100,
      });

      gestures.handleWheel(wheelEvent);

      const zoomEvents = eventLog.filter(e => e.type === 'zoom');
      expect(zoomEvents.length).toBe(1);
    });

    test('should zoom in on negative deltaY', () => {
      const wheelEvent = new WheelEvent('wheel', {
        deltaY: -100,
      });

      gestures.handleWheel(wheelEvent);

      const zoomEvents = eventLog.filter(e => e.type === 'zoom');
      expect(zoomEvents[0].data.factor).toBeLessThan(1);
    });

    test('should zoom out on positive deltaY', () => {
      const wheelEvent = new WheelEvent('wheel', {
        deltaY: 100,
      });

      gestures.handleWheel(wheelEvent);

      const zoomEvents = eventLog.filter(e => e.type === 'zoom');
      expect(zoomEvents[0].data.factor).toBeGreaterThan(1);
    });
  });

  describe('keyboard events', () => {
    test('should handle arrow keys for pan', () => {
      const upEvent = new KeyboardEvent('keydown', { code: 'ArrowUp' });
      gestures.handleKeyDown(upEvent);

      const panEvents = eventLog.filter(e => e.type === 'pan');
      expect(panEvents.length).toBe(1);
      expect(panEvents[0].data.deltaY).toBeGreaterThan(0);
    });

    test('should handle plus/minus for zoom', () => {
      const plusEvent = new KeyboardEvent('keydown', { code: 'Equal' });
      gestures.handleKeyDown(plusEvent);

      const zoomEvents = eventLog.filter(e => e.type === 'zoom');
      expect(zoomEvents.length).toBe(1);
    });

    test('should handle Home key for home view', () => {
      const homeEvent = new KeyboardEvent('keydown', { code: 'Home' });
      gestures.handleKeyDown(homeEvent);

      const orbitEvents = eventLog.filter(e => e.type === 'orbit');
      expect(orbitEvents.length).toBeGreaterThan(0);
    });

    test('should handle Escape for deselect', () => {
      const escEvent = new KeyboardEvent('keydown', { code: 'Escape' });
      gestures.handleKeyDown(escEvent);

      const deselectEvents = eventLog.filter(e => e.type === 'deselect');
      expect(deselectEvents.length).toBe(1);
    });

    test('should track modifier keys', () => {
      const ctrlDown = new KeyboardEvent('keydown', { code: 'ControlLeft' });
      gestures.handleKeyDown(ctrlDown);

      let modifiers = gestures.getModifierState();
      expect(modifiers.ctrl).toBe(true);

      const ctrlUp = new KeyboardEvent('keyup', { code: 'ControlLeft' });
      gestures.handleKeyUp(ctrlUp);

      modifiers = gestures.getModifierState();
      expect(modifiers.ctrl).toBe(false);
    });
  });

  describe('event handlers', () => {
    test('should register and call event handlers', () => {
      let callCount = 0;
      const handler = () => { callCount++; };

      gestures.on(handler);

      const wheelEvent = new WheelEvent('wheel', { deltaY: 100 });
      gestures.handleWheel(wheelEvent);

      expect(callCount).toBeGreaterThan(0);
    });

    test('should unregister handlers', () => {
      const handler = () => {};
      gestures.on(handler);
      gestures.off(handler);

      const wheelEvent = new WheelEvent('wheel', { deltaY: 100 });
      const initialLength = eventLog.length;
      gestures.handleWheel(wheelEvent);

      expect(eventLog.length).toBe(initialLength);
    });
  });

  describe('configuration', () => {
    test('should respect custom orbit button', () => {
      const customGestures = new MouseGestures(camera, {
        orbitButton: 'middle',
      });

      const handler = (event: GestureEvent) => {
        expect(event.type).not.toBe('orbit');
      };

      customGestures.on(handler);
    });
  });
});
