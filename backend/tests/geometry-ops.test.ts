/**
 * BendCore Geometry Operations Tests
 * Unit tests for geometric calculations
 */

import { GeometryOps } from '../src/core/geometry-ops';
import { Cylinder, Torus, Vector3 } from '../src/models/types';

describe('GeometryOps', () => {
  describe('calculateBoundingBox', () => {
    it('should calculate bounding box for points', () => {
      const points: Vector3[] = [
        { x: 0, y: 0, z: 0 },
        { x: 10, y: 10, z: 10 }
      ];

      const bbox = GeometryOps.calculateBoundingBox(points);

      expect(bbox.min.x).toBe(0);
      expect(bbox.min.y).toBe(0);
      expect(bbox.max.x).toBe(10);
      expect(bbox.max.y).toBe(10);
    });

    it('should handle empty array', () => {
      const bbox = GeometryOps.calculateBoundingBox([]);

      expect(bbox.min.x).toBe(0);
      expect(bbox.max.x).toBe(0);
    });

    it('should calculate center correctly', () => {
      const points: Vector3[] = [
        { x: 0, y: 0, z: 0 },
        { x: 10, y: 10, z: 10 }
      ];

      const bbox = GeometryOps.calculateBoundingBox(points);

      expect(bbox.center.x).toBe(5);
      expect(bbox.center.y).toBe(5);
      expect(bbox.center.z).toBe(5);
    });
  });

  describe('aabbCollision', () => {
    it('should detect collision between overlapping boxes', () => {
      const box1 = {
        min: { x: 0, y: 0, z: 0 },
        max: { x: 10, y: 10, z: 10 },
        center: { x: 5, y: 5, z: 5 },
        size: { x: 10, y: 10, z: 10 }
      };

      const box2 = {
        min: { x: 5, y: 5, z: 5 },
        max: { x: 15, y: 15, z: 15 },
        center: { x: 10, y: 10, z: 10 },
        size: { x: 10, y: 10, z: 10 }
      };

      expect(GeometryOps.aabbCollision(box1, box2)).toBe(true);
    });

    it('should reject non-overlapping boxes', () => {
      const box1 = {
        min: { x: 0, y: 0, z: 0 },
        max: { x: 5, y: 5, z: 5 },
        center: { x: 2.5, y: 2.5, z: 2.5 },
        size: { x: 5, y: 5, z: 5 }
      };

      const box2 = {
        min: { x: 10, y: 10, z: 10 },
        max: { x: 15, y: 15, z: 15 },
        center: { x: 12.5, y: 12.5, z: 12.5 },
        size: { x: 5, y: 5, z: 5 }
      };

      expect(GeometryOps.aabbCollision(box1, box2)).toBe(false);
    });
  });

  describe('cylinderBoundingBox', () => {
    it('should calculate bounding box for cylinder', () => {
      const cylinder: Cylinder = {
        center: { x: 0, y: 0, z: 0 },
        axis: { x: 0, y: 0, z: 1 },
        length: 10,
        outer_radius: 1,
        inner_radius: 0.5,
        segment_id: 'test'
      };

      const bbox = GeometryOps.cylinderBoundingBox(cylinder);

      expect(bbox.min.x).toBeLessThanOrEqual(0);
      expect(bbox.max.x).toBeGreaterThanOrEqual(0);
      expect(bbox.min.z).toBeLessThanOrEqual(0);
      expect(bbox.max.z).toBeGreaterThanOrEqual(10);
    });
  });

  describe('pointInCylinder', () => {
    it('should detect point inside cylinder', () => {
      const cylinder: Cylinder = {
        center: { x: 0, y: 0, z: 0 },
        axis: { x: 0, y: 0, z: 1 },
        length: 10,
        outer_radius: 1,
        inner_radius: 0.5,
        segment_id: 'test'
      };

      const point: Vector3 = { x: 0.7, y: 0, z: 5 };

      expect(GeometryOps.pointInCylinder(point, cylinder)).toBe(true);
    });

    it('should reject point outside cylinder', () => {
      const cylinder: Cylinder = {
        center: { x: 0, y: 0, z: 0 },
        axis: { x: 0, y: 0, z: 1 },
        length: 10,
        outer_radius: 1,
        inner_radius: 0.5,
        segment_id: 'test'
      };

      const point: Vector3 = { x: 2, y: 0, z: 5 };

      expect(GeometryOps.pointInCylinder(point, cylinder)).toBe(false);
    });
  });

  describe('sphereAABBCollision', () => {
    it('should detect sphere inside box', () => {
      const sphere = { x: 5, y: 5, z: 5 };
      const box = {
        min: { x: 0, y: 0, z: 0 },
        max: { x: 10, y: 10, z: 10 },
        center: { x: 5, y: 5, z: 5 },
        size: { x: 10, y: 10, z: 10 }
      };

      expect(GeometryOps.sphereAABBCollision(sphere, 2, box)).toBe(true);
    });

    it('should reject sphere outside box', () => {
      const sphere = { x: 15, y: 15, z: 15 };
      const box = {
        min: { x: 0, y: 0, z: 0 },
        max: { x: 10, y: 10, z: 10 },
        center: { x: 5, y: 5, z: 5 },
        size: { x: 10, y: 10, z: 10 }
      };

      expect(GeometryOps.sphereAABBCollision(sphere, 1, box)).toBe(false);
    });
  });

  describe('calculateTotalProperties', () => {
    it('should calculate volume for cylinders', () => {
      const cylinders: Cylinder[] = [
        {
          center: { x: 0, y: 0, z: 0 },
          axis: { x: 0, y: 0, z: 1 },
          length: 10,
          outer_radius: 1,
          inner_radius: 0.5,
          segment_id: 'cyl1'
        }
      ];

      const { volume, surfaceArea } = GeometryOps.calculateTotalProperties(
        cylinders,
        []
      );

      expect(volume).toBeGreaterThan(0);
      expect(surfaceArea).toBeGreaterThan(0);
    });
  });
});
