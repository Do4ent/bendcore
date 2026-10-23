/**
 * OrbitCamera Test Suite
 */

import { OrbitCamera, CameraState } from '../src/ui/OrbitCamera';

describe('OrbitCamera', () => {
  let camera: OrbitCamera;

  beforeEach(() => {
    camera = new OrbitCamera(16 / 9);
  });

  describe('initialization', () => {
    test('should initialize with default state', () => {
      const state = camera.getState();
      expect(state.position.z).toBe(100);
      expect(state.target).toEqual({ x: 0, y: 0, z: 0 });
      expect(state.distance).toBe(100);
    });

    test('should create Three.js camera', () => {
      const threeCamera = camera.getCamera();
      expect(threeCamera).toBeDefined();
      expect(threeCamera.fov).toBe(50);
    });
  });

  describe('orbit', () => {
    test('should update pitch and yaw on orbit', () => {
      const initialState = camera.getState();
      camera.orbit(10, 20);
      const newState = camera.getState();

      expect(newState.pitch).not.toBe(initialState.pitch);
      expect(newState.yaw).not.toBe(initialState.yaw);
    });

    test('should clamp pitch to prevent gimbal lock', () => {
      camera.orbit(100, 0);
      const state = camera.getState();
      expect(state.pitch).toBeLessThanOrEqual(89);
      expect(state.pitch).toBeGreaterThanOrEqual(-89);
    });

    test('should handle negative rotation', () => {
      const initialState = camera.getState();
      camera.orbit(-5, -10);
      const newState = camera.getState();

      expect(newState.pitch).toBeLessThan(initialState.pitch);
      expect(newState.yaw).toBeLessThan(initialState.yaw);
    });
  });

  describe('zoom', () => {
    test('should decrease distance on zoom in', () => {
      const initialDistance = camera.getState().distance;
      camera.zoom(1.1);  // 10% zoom in
      const newDistance = camera.getState().distance;

      expect(newDistance).toBeLessThan(initialDistance);
    });

    test('should increase distance on zoom out', () => {
      const initialDistance = camera.getState().distance;
      camera.zoom(0.909);  // 10% zoom out
      const newDistance = camera.getState().distance;

      expect(newDistance).toBeGreaterThan(initialDistance);
    });

    test('should clamp distance to bounds', () => {
      camera.zoom(0.001);  // Extreme zoom in
      const state = camera.getState();
      expect(state.distance).toBeGreaterThanOrEqual(1);
    });
  });

  describe('pan', () => {
    test('should update target position', () => {
      const initialTarget = camera.getState().target;
      camera.pan(5, 5);
      const newTarget = camera.getState().target;

      expect(newTarget.x).not.toBe(initialTarget.x);
      expect(newTarget.y).not.toBe(initialTarget.y);
    });

    test('should handle negative pan', () => {
      const initialTarget = camera.getState().target;
      camera.pan(-5, -5);
      const newTarget = camera.getState().target;

      expect(newTarget.x).toBeLessThan(initialTarget.x);
      expect(newTarget.y).toBeLessThan(initialTarget.y);
    });
  });

  describe('named views', () => {
    test('should have default views', () => {
      const views = camera.getNamedViews();
      expect(views.length).toBeGreaterThan(0);
    });

    test('should contain Front view', () => {
      const views = camera.getNamedViews();
      const frontView = views.find(v => v.name === 'Front');
      expect(frontView).toBeDefined();
    });

    test('should contain Isometric view', () => {
      const views = camera.getNamedViews();
      const isoView = views.find(v => v.name === 'Isometric');
      expect(isoView).toBeDefined();
    });

    test('should save custom views', () => {
      camera.saveView('CustomView', 'Test view');
      const views = camera.getNamedViews();
      const customView = views.find(v => v.name === 'CustomView');
      expect(customView).toBeDefined();
      expect(customView?.description).toBe('Test view');
    });

    test('should switch to named view', () => {
      const initialState = camera.getState();
      camera.switchToView('Isometric');
      const newState = camera.getState();

      expect(newState.pitch).not.toBe(initialState.pitch);
    });
  });

  describe('animation', () => {
    test('should animate to target state', (done) => {
      const targetState: CameraState = {
        position: { x: 50, y: 50, z: 50 },
        target: { x: 0, y: 0, z: 0 },
        distance: 50,
        pitch: 45,
        yaw: 45,
        fov: 50,
        near: 0.1,
        far: 20000,
      };

      const initialState = camera.getState();
      camera.animateTo(targetState, 100);

      setTimeout(() => {
        camera.updateAnimation();
        const currentState = camera.getState();

        expect(currentState.pitch).toBeGreaterThan(initialState.pitch);
        done();
      }, 50);
    });

    test('should complete animation at duration', (done) => {
      const targetState: CameraState = {
        position: { x: 100, y: 100, z: 100 },
        target: { x: 0, y: 0, z: 0 },
        distance: 100,
        pitch: 30,
        yaw: 45,
        fov: 50,
        near: 0.1,
        far: 20000,
      };

      camera.animateTo(targetState, 200);

      let animationCompleted = false;
      for (let i = 0; i < 5; i++) {
        setTimeout(() => camera.updateAnimation(), i * 50);
      }

      setTimeout(() => {
        camera.updateAnimation();
        const finalState = camera.getState();
        expect(Math.abs(finalState.pitch - targetState.pitch)).toBeLessThan(1);
        done();
      }, 250);
    });
  });

  describe('home view', () => {
    test('should return to home position', () => {
      camera.orbit(45, 90);
      camera.zoom(2);
      camera.pan(10, 10);

      camera.homeView();

      setTimeout(() => {
        camera.updateAnimation();
        const state = camera.getState();
        expect(state.pitch).toBeLessThan(5);
        expect(state.yaw).toBeLessThan(5);
      }, 550);
    });
  });

  describe('window resize', () => {
    test('should handle resize', () => {
      camera.handleWindowResize(1920, 1080);
      const threeCamera = camera.getCamera();
      expect(threeCamera.aspect).toBeCloseTo(1920 / 1080, 5);
    });
  });

  describe('set state', () => {
    test('should update state with partial values', () => {
      camera.setState({
        distance: 200,
        pitch: 30,
      });

      const state = camera.getState();
      expect(state.distance).toBe(200);
      expect(state.pitch).toBe(30);
      expect(state.yaw).toBe(0);  // Should remain unchanged
    });

    test('should clamp distance in setState', () => {
      camera.setState({
        distance: 50000,  // Beyond max
      });

      const state = camera.getState();
      expect(state.distance).toBeLessThanOrEqual(10000);
    });
  });
});
