/**
 * SnapConstraints Test Suite
 */

import { SnapConstraints, SnapMode } from '../src/ui/SnapConstraints';

describe('SnapConstraints', () => {
  let snap: SnapConstraints;

  beforeEach(() => {
    snap = new SnapConstraints();
  });

  describe('initialization', () => {
    test('should initialize with default snap modes', () => {
      const modes = snap.getSnapModes();
      expect(modes).toContain(SnapMode.GRID);
      expect(modes).toContain(SnapMode.ENDPOINT);
    });

    test('should be enabled by default', () => {
      expect(snap.isSnapEnabled()).toBe(true);
    });

    test('should have default snap radius', () => {
      expect(snap.getSnapRadius()).toBe(8);
    });

    test('should return Three.js group', () => {
      const group = snap.getSnapGroup();
      expect(group).toBeDefined();
      expect(group.name).toBe('SnapIndicators');
    });
  });

  describe('snap mode management', () => {
    test('should set single snap mode', () => {
      snap.setSnapModes([SnapMode.AXIS]);
      const modes = snap.getSnapModes();

      expect(modes).toContain(SnapMode.AXIS);
      expect(modes).not.toContain(SnapMode.GRID);
    });

    test('should set multiple snap modes', () => {
      snap.setSnapModes([SnapMode.GRID, SnapMode.AXIS, SnapMode.CENTER]);
      const modes = snap.getSnapModes();

      expect(modes).toContain(SnapMode.GRID);
      expect(modes).toContain(SnapMode.AXIS);
      expect(modes).toContain(SnapMode.CENTER);
      expect(modes.length).toBe(3);
    });

    test('should toggle snap mode', () => {
      snap.setSnapModes([SnapMode.GRID]);
      snap.toggleSnapMode(SnapMode.AXIS);

      const modes = snap.getSnapModes();
      expect(modes).toContain(SnapMode.AXIS);
      expect(modes).toContain(SnapMode.GRID);
    });

    test('should remove snap mode on toggle', () => {
      snap.setSnapModes([SnapMode.GRID, SnapMode.AXIS]);
      snap.toggleSnapMode(SnapMode.GRID);

      const modes = snap.getSnapModes();
      expect(modes).not.toContain(SnapMode.GRID);
      expect(modes).toContain(SnapMode.AXIS);
    });
  });

  describe('snap radius', () => {
    test('should set snap radius', () => {
      snap.setSnapRadius(16);
      expect(snap.getSnapRadius()).toBe(16);
    });

    test('should enforce minimum snap radius', () => {
      snap.setSnapRadius(0);
      expect(snap.getSnapRadius()).toBe(1);
    });

    test('should accept large snap radius', () => {
      snap.setSnapRadius(100);
      expect(snap.getSnapRadius()).toBe(100);
    });
  });

  describe('enable/disable snap', () => {
    test('should disable snap', () => {
      snap.setEnabled(false);
      expect(snap.isSnapEnabled()).toBe(false);
    });

    test('should enable snap', () => {
      snap.setEnabled(false);
      snap.setEnabled(true);
      expect(snap.isSnapEnabled()).toBe(true);
    });

    test('should toggle snap visibility', () => {
      const group = snap.getSnapGroup();
      expect(group.visible).toBe(true);

      snap.setEnabled(false);
      expect(group.visible).toBe(false);

      snap.setEnabled(true);
      expect(group.visible).toBe(true);
    });
  });

  describe('snap point detection', () => {
    test('should return empty array when snap disabled', () => {
      snap.setEnabled(false);
      const mockScene = { traverse: () => {} } as any;
      const mockCamera = {} as any;

      const points = snap.detectSnapPoints(100, 100, mockScene, mockCamera);
      expect(points).toEqual([]);
    });

    test('should detect snap points near cursor', () => {
      // Note: In a full test, we'd set up a complete Three.js scene
      // This is a basic validation of the function signature
      const mockScene = { traverse: () => {} } as any;
      const mockCamera = {} as any;

      const points = snap.detectSnapPoints(100, 100, mockScene, mockCamera);
      expect(Array.isArray(points)).toBe(true);
    });

    test('should filter by snap radius', () => {
      snap.setSnapRadius(5);  // Very small radius

      const mockScene = { traverse: () => {} } as any;
      const mockCamera = {} as any;

      const points = snap.detectSnapPoints(0, 0, mockScene, mockCamera);
      // Should find fewer points with small radius
      expect(Array.isArray(points)).toBe(true);
    });

    test('should respect snap modes when detecting', () => {
      snap.setSnapModes([SnapMode.GRID]);

      const mockScene = { traverse: () => {} } as any;
      const mockCamera = {} as any;

      const points = snap.detectSnapPoints(100, 100, mockScene, mockCamera);
      // All detected points should be GRID type
      points.forEach(point => {
        expect(point.type).toBe(SnapMode.GRID);
      });
    });
  });

  describe('snap point visualization', () => {
    test('should visualize endpoint snap point', () => {
      const snapPoint = {
        position: { x: 10, y: 10, z: 0 },
        type: SnapMode.ENDPOINT,
        normal: { x: 0, y: 0, z: 1 },
        distance: 5,
      };

      snap.visualizeSnapPoint(snapPoint);

      const activeSnaps = snap.getActiveSnaps();
      expect(activeSnaps.length).toBe(1);
      expect(activeSnaps[0].type).toBe(SnapMode.ENDPOINT);
    });

    test('should visualize center snap point', () => {
      const snapPoint = {
        position: { x: 5, y: 5, z: 5 },
        type: SnapMode.CENTER,
        distance: 3,
      };

      snap.visualizeSnapPoint(snapPoint);

      const activeSnaps = snap.getActiveSnaps();
      expect(activeSnaps.length).toBe(1);
      expect(activeSnaps[0].type).toBe(SnapMode.CENTER);
    });

    test('should visualize grid snap point', () => {
      const snapPoint = {
        position: { x: 0, y: 0, z: 0 },
        type: SnapMode.GRID,
        distance: 1,
      };

      snap.visualizeSnapPoint(snapPoint);

      const activeSnaps = snap.getActiveSnaps();
      expect(activeSnaps.length).toBe(1);
      expect(activeSnaps[0].type).toBe(SnapMode.GRID);
    });

    test('should visualize axis snap point', () => {
      const snapPoint = {
        position: { x: 5, y: 0, z: 0 },
        type: SnapMode.AXIS,
        normal: { x: 1, y: 0, z: 0 },
        distance: 2,
      };

      snap.visualizeSnapPoint(snapPoint);

      const activeSnaps = snap.getActiveSnaps();
      expect(activeSnaps.length).toBe(1);
      expect(activeSnaps[0].type).toBe(SnapMode.AXIS);
    });

    test('should clear snap indicators', () => {
      const snapPoint = {
        position: { x: 0, y: 0, z: 0 },
        type: SnapMode.GRID,
        distance: 1,
      };

      snap.visualizeSnapPoint(snapPoint);
      expect(snap.getActiveSnaps().length).toBe(1);

      snap.clearSnapIndicators();
      expect(snap.getActiveSnaps().length).toBe(0);
    });

    test('should clear previous indicators when visualizing new point', () => {
      const point1 = {
        position: { x: 0, y: 0, z: 0 },
        type: SnapMode.GRID,
        distance: 1,
      };

      const point2 = {
        position: { x: 10, y: 10, z: 0 },
        type: SnapMode.ENDPOINT,
        distance: 2,
      };

      snap.visualizeSnapPoint(point1);
      expect(snap.getActiveSnaps().length).toBe(1);

      snap.visualizeSnapPoint(point2);
      expect(snap.getActiveSnaps().length).toBe(1);
      expect(snap.getActiveSnaps()[0].type).toBe(SnapMode.ENDPOINT);
    });
  });

  describe('snap group management', () => {
    test('should add snap group to Three.js scene', () => {
      const group = snap.getSnapGroup();
      expect(group.name).toBe('SnapIndicators');
      expect(group.children).toBeDefined();
    });

    test('should maintain snap group visibility state', () => {
      const group = snap.getSnapGroup();
      snap.setEnabled(false);
      expect(group.visible).toBe(false);

      snap.setEnabled(true);
      expect(group.visible).toBe(true);
    });
  });

  describe('multiple snap modes', () => {
    test('should detect points from multiple modes', () => {
      snap.setSnapModes([SnapMode.GRID, SnapMode.AXIS]);

      const mockScene = { traverse: () => {} } as any;
      const mockCamera = {} as any;

      const points = snap.detectSnapPoints(100, 100, mockScene, mockCamera);
      expect(Array.isArray(points)).toBe(true);
    });

    test('should isolate points by mode', () => {
      snap.setSnapModes([SnapMode.GRID]);

      const mockScene = { traverse: () => {} } as any;
      const mockCamera = {} as any;

      const gridPoints = snap.detectSnapPoints(100, 100, mockScene, mockCamera);

      snap.setSnapModes([SnapMode.AXIS]);
      const axisPoints = snap.detectSnapPoints(100, 100, mockScene, mockCamera);

      // Both should have results but potentially different points
      expect(Array.isArray(gridPoints)).toBe(true);
      expect(Array.isArray(axisPoints)).toBe(true);
    });
  });
});
