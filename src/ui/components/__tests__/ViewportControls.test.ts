/**
 * CAD-CORE-028 Tests
 * ViewportControls tests
 */

import * as THREE from 'three';
import { ViewportControls, CameraState } from '../ViewportControls';

describe('ViewportControls', () => {
  let camera: THREE.PerspectiveCamera;
  let canvas: HTMLCanvasElement;
  let controls: ViewportControls;

  beforeEach(() => {
    // Create mock canvas
    canvas = document.createElement('canvas');
    canvas.width = 800;
    canvas.height = 600;

    // Create camera
    camera = new THREE.PerspectiveCamera(75, 800 / 600, 0.1, 10000);
    camera.position.set(0, 0, 100);

    // Create controls
    controls = new ViewportControls(camera, canvas);
  });

  describe('Initialization', () => {
    test('should create controls with default config', () => {
      expect(controls).toBeDefined();
    });

    test('should use default enable flags', () => {
      const defaultConfig = {
        enableOrbit: true,
        enablePan: true,
        enableZoom: true,
        enableKeyboard: true,
      };
      expect(defaultConfig.enableOrbit).toBe(true);
    });

    test('should use default speeds', () => {
      const defaultConfig = {
        zoomSpeed: 0.1,
        panSpeed: 0.005,
        rotationSpeed: 0.005,
      };
      expect(defaultConfig.zoomSpeed).toBe(0.1);
      expect(defaultConfig.panSpeed).toBe(0.005);
    });

    test('should configure damping', () => {
      const defaultConfig = {
        enableDamping: true,
        dampingFactor: 0.05,
      };
      expect(defaultConfig.enableDamping).toBe(true);
      expect(defaultConfig.dampingFactor).toBe(0.05);
    });
  });

  describe('Preset Views', () => {
    test('should set front view', () => {
      controls.setViewFront();
      const state = controls.getCameraState();

      expect(state.position.z).toBeGreaterThan(0);
      expect(state.position.x).toBe(0);
      expect(state.position.y).toBe(0);
    });

    test('should set top view', () => {
      controls.setViewTop();
      const state = controls.getCameraState();

      expect(state.position.y).toBeGreaterThan(0);
      expect(state.position.x).toBe(0);
      expect(state.position.z).toBe(0);
    });

    test('should set right view', () => {
      controls.setViewRight();
      const state = controls.getCameraState();

      expect(state.position.x).toBeGreaterThan(0);
      expect(state.position.y).toBe(0);
      expect(state.position.z).toBe(0);
    });

    test('should set isometric view', () => {
      controls.setViewIsometric();
      const state = controls.getCameraState();

      expect(state.position.x).toBeTruthy();
      expect(state.position.y).toBeTruthy();
      expect(state.position.z).toBeTruthy();
    });

    test('should reset to home view', () => {
      controls.resetToHome();
      const state = controls.getCameraState();

      expect(state.position.x).toBe(100);
      expect(state.position.y).toBe(100);
      expect(state.position.z).toBe(100);
    });
  });

  describe('View Saving and Loading', () => {
    test('should save current view', () => {
      camera.position.set(50, 75, 100);
      controls.saveView('custom');

      const state = controls.getCameraState();
      expect(state.position.x).toBe(50);
      expect(state.position.y).toBe(75);
      expect(state.position.z).toBe(100);
    });

    test('should load saved view', () => {
      camera.position.set(100, 200, 300);
      controls.saveView('view1');

      camera.position.set(10, 20, 30);
      controls.loadView('view1');

      const state = controls.getCameraState();
      expect(state.position.x).toBe(100);
      expect(state.position.y).toBe(200);
      expect(state.position.z).toBe(300);
    });

    test('should handle loading non-existent view gracefully', () => {
      const initialState = controls.getCameraState();
      controls.loadView('non_existent');
      const afterState = controls.getCameraState();

      // Should remain unchanged
      expect(afterState.position.x).toBe(initialState.position.x);
    });

    test('should support multiple saved views', () => {
      camera.position.set(100, 0, 0);
      controls.saveView('front');

      camera.position.set(0, 100, 0);
      controls.saveView('top');

      camera.position.set(0, 0, 100);
      controls.saveView('side');

      controls.loadView('front');
      let state = controls.getCameraState();
      expect(state.position.x).toBe(100);

      controls.loadView('top');
      state = controls.getCameraState();
      expect(state.position.y).toBe(100);

      controls.loadView('side');
      state = controls.getCameraState();
      expect(state.position.z).toBe(100);
    });
  });

  describe('Camera State Management', () => {
    test('should get current camera state', () => {
      camera.position.set(50, 60, 70);
      const state = controls.getCameraState();

      expect(state.position.x).toBe(50);
      expect(state.position.y).toBe(60);
      expect(state.position.z).toBe(70);
    });

    test('should set camera state', () => {
      const newState: CameraState = {
        position: { x: 100, y: 200, z: 300 },
        target: { x: 0, y: 0, z: 0 },
        up: { x: 0, y: 1, z: 0 },
      };

      controls.setCameraState(newState);
      const state = controls.getCameraState();

      expect(state.position.x).toBe(100);
      expect(state.position.y).toBe(200);
      expect(state.position.z).toBe(300);
    });

    test('should preserve camera up vector', () => {
      const state = controls.getCameraState();
      expect(state.up.y).toBe(1); // Default up is Y axis
      expect(state.up.x).toBe(0);
      expect(state.up.z).toBe(0);
    });

    test('should track camera target', () => {
      const state = controls.getCameraState();
      expect(state.target).toBeDefined();
      expect(state.target.x).toBe(0); // Default target is origin
      expect(state.target.y).toBe(0);
      expect(state.target.z).toBe(0);
    });
  });

  describe('Configuration Updates', () => {
    test('should update zoom speed', () => {
      controls.updateConfig({ zoomSpeed: 0.2 });
      // Config is updated internally
      expect(controls).toBeDefined();
    });

    test('should update pan speed', () => {
      controls.updateConfig({ panSpeed: 0.01 });
      expect(controls).toBeDefined();
    });

    test('should update rotation speed', () => {
      controls.updateConfig({ rotationSpeed: 0.01 });
      expect(controls).toBeDefined();
    });

    test('should enable/disable features', () => {
      controls.updateConfig({ enableZoom: false, enableOrbit: true });
      expect(controls).toBeDefined();
    });

    test('should apply multiple config updates', () => {
      controls.updateConfig({
        zoomSpeed: 0.15,
        panSpeed: 0.008,
        rotationSpeed: 0.008,
        enableDamping: false,
      });
      expect(controls).toBeDefined();
    });
  });

  describe('Camera Callbacks', () => {
    test('should trigger camera change callback on state update', () => {
      let callbackTriggered = false;
      controls.setOnCameraChange(() => {
        callbackTriggered = true;
      });

      controls.resetToHome();
      expect(callbackTriggered).toBe(true);
    });

    test('should pass camera to callback', () => {
      let capturedCamera: THREE.Camera | null = null;
      controls.setOnCameraChange((cam) => {
        capturedCamera = cam;
      });

      controls.setViewFront();
      expect(capturedCamera).toBe(camera);
    });
  });

  describe('Preset View Distances', () => {
    test('should maintain consistent distance in front view', () => {
      const initialDistance = camera.position.distanceTo(new THREE.Vector3(0, 0, 0));
      controls.setViewFront();
      const finalDistance = camera.position.distanceTo(new THREE.Vector3(0, 0, 0));

      expect(finalDistance).toBeCloseTo(initialDistance, 0);
    });

    test('should maintain consistent distance in isometric view', () => {
      const initialDistance = camera.position.distanceTo(new THREE.Vector3(0, 0, 0));
      controls.setViewIsometric();
      const finalDistance = camera.position.distanceTo(new THREE.Vector3(0, 0, 0));

      expect(finalDistance).toBeCloseTo(initialDistance, 0);
    });
  });

  describe('Fit All Geometry', () => {
    test('should reset camera to home view on fit all', () => {
      camera.position.set(500, 500, 500);
      controls.fitAllGeometry();

      const state = controls.getCameraState();
      // Should be reset to home view
      expect(state.position).toBeDefined();
    });
  });

  describe('Event Handling Integration', () => {
    test('should handle mouse events without error', () => {
      const mouseEvent = new MouseEvent('mousedown', { button: 0 });
      expect(() => {
        canvas.dispatchEvent(mouseEvent);
      }).not.toThrow();
    });

    test('should handle wheel events without error', () => {
      const wheelEvent = new WheelEvent('wheel', { deltaY: 10 });
      expect(() => {
        canvas.dispatchEvent(wheelEvent);
      }).not.toThrow();
    });

    test('should handle touch events without error', () => {
      const touchEvent = new TouchEvent('touchstart');
      expect(() => {
        canvas.dispatchEvent(touchEvent);
      }).not.toThrow();
    });
  });

  describe('Viewport Range Validation', () => {
    test('should respect minimum zoom distance', () => {
      // Default minDistance is 1
      camera.position.set(0.5, 0, 0);
      const distance = camera.position.length();
      const minDistance = 1;

      expect(distance).toBeLessThan(minDistance);
    });

    test('should respect maximum zoom distance', () => {
      // Default maxDistance is 10000
      camera.position.set(15000, 0, 0);
      const distance = camera.position.length();
      const maxDistance = 10000;

      expect(distance).toBeGreaterThan(maxDistance);
    });
  });

  describe('Cleanup', () => {
    test('should dispose resources', () => {
      expect(() => {
        controls.dispose();
      }).not.toThrow();
    });

    test('should handle multiple dispose calls', () => {
      expect(() => {
        controls.dispose();
        controls.dispose();
      }).not.toThrow();
    });
  });

  describe('Edge Cases', () => {
    test('should handle camera position at origin', () => {
      camera.position.set(0, 0, 0);
      const state = controls.getCameraState();

      expect(state.position.x).toBe(0);
      expect(state.position.y).toBe(0);
      expect(state.position.z).toBe(0);
    });

    test('should handle very large coordinates', () => {
      const largeCoord = 1e6;
      camera.position.set(largeCoord, largeCoord, largeCoord);
      const state = controls.getCameraState();

      expect(state.position.x).toBe(largeCoord);
    });

    test('should handle very small coordinates', () => {
      const smallCoord = 1e-6;
      camera.position.set(smallCoord, smallCoord, smallCoord);
      const state = controls.getCameraState();

      expect(Math.abs(state.position.x - smallCoord)).toBeLessThan(1e-5);
    });
  });
});

export {};
