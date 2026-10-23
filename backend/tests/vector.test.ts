/**
 * BendCore Vector Utility Tests
 * Unit tests for 3D vector operations
 */

import { Vector } from '../src/utils/vector';

describe('Vector', () => {
  describe('create', () => {
    it('should create vector with values', () => {
      const v = Vector.create(1, 2, 3);
      expect(v.x).toBe(1);
      expect(v.y).toBe(2);
      expect(v.z).toBe(3);
    });

    it('should create zero vector by default', () => {
      const v = Vector.create();
      expect(v.x).toBe(0);
      expect(v.y).toBe(0);
      expect(v.z).toBe(0);
    });
  });

  describe('add', () => {
    it('should add two vectors', () => {
      const v1 = Vector.create(1, 2, 3);
      const v2 = Vector.create(4, 5, 6);
      const result = Vector.add(v1, v2);
      expect(result.x).toBe(5);
      expect(result.y).toBe(7);
      expect(result.z).toBe(9);
    });
  });

  describe('subtract', () => {
    it('should subtract two vectors', () => {
      const v1 = Vector.create(5, 7, 9);
      const v2 = Vector.create(1, 2, 3);
      const result = Vector.subtract(v1, v2);
      expect(result.x).toBe(4);
      expect(result.y).toBe(5);
      expect(result.z).toBe(6);
    });
  });

  describe('scale', () => {
    it('should scale vector by scalar', () => {
      const v = Vector.create(1, 2, 3);
      const result = Vector.scale(v, 2);
      expect(result.x).toBe(2);
      expect(result.y).toBe(4);
      expect(result.z).toBe(6);
    });

    it('should handle negative scalar', () => {
      const v = Vector.create(1, 2, 3);
      const result = Vector.scale(v, -1);
      expect(result.x).toBe(-1);
      expect(result.y).toBe(-2);
      expect(result.z).toBe(-3);
    });
  });

  describe('dot', () => {
    it('should calculate dot product', () => {
      const v1 = Vector.create(1, 2, 3);
      const v2 = Vector.create(4, 5, 6);
      const result = Vector.dot(v1, v2);
      expect(result).toBe(32); // 1*4 + 2*5 + 3*6
    });

    it('should return 0 for perpendicular vectors', () => {
      const v1 = Vector.create(1, 0, 0);
      const v2 = Vector.create(0, 1, 0);
      const result = Vector.dot(v1, v2);
      expect(result).toBe(0);
    });
  });

  describe('cross', () => {
    it('should calculate cross product', () => {
      const v1 = Vector.create(1, 0, 0);
      const v2 = Vector.create(0, 1, 0);
      const result = Vector.cross(v1, v2);
      expect(result.x).toBe(0);
      expect(result.y).toBe(0);
      expect(result.z).toBe(1);
    });
  });

  describe('length', () => {
    it('should calculate vector length', () => {
      const v = Vector.create(3, 4, 0);
      const result = Vector.length(v);
      expect(result).toBe(5);
    });

    it('should handle zero vector', () => {
      const v = Vector.create(0, 0, 0);
      const result = Vector.length(v);
      expect(result).toBe(0);
    });
  });

  describe('normalize', () => {
    it('should normalize vector to unit length', () => {
      const v = Vector.create(3, 4, 0);
      const result = Vector.normalize(v);
      const length = Vector.length(result);
      expect(length).toBeCloseTo(1, 5);
    });

    it('should handle zero vector', () => {
      const v = Vector.create(0, 0, 0);
      const result = Vector.normalize(v);
      expect(result.x).toBe(0);
      expect(result.y).toBe(0);
      expect(result.z).toBe(0);
    });
  });

  describe('distance', () => {
    it('should calculate distance between points', () => {
      const a = Vector.create(0, 0, 0);
      const b = Vector.create(3, 4, 0);
      const result = Vector.distance(a, b);
      expect(result).toBe(5);
    });
  });

  describe('angle', () => {
    it('should calculate angle between vectors', () => {
      const v1 = Vector.create(1, 0, 0);
      const v2 = Vector.create(0, 1, 0);
      const result = Vector.angle(v1, v2);
      expect(result).toBeCloseTo(Math.PI / 2, 5);
    });

    it('should return 0 for parallel vectors', () => {
      const v1 = Vector.create(1, 0, 0);
      const v2 = Vector.create(2, 0, 0);
      const result = Vector.angle(v1, v2);
      expect(result).toBeCloseTo(0, 5);
    });
  });

  describe('toRadians and toDegrees', () => {
    it('should convert degrees to radians', () => {
      const radians = Vector.toRadians(180);
      expect(radians).toBeCloseTo(Math.PI, 5);
    });

    it('should convert radians to degrees', () => {
      const degrees = Vector.toDegrees(Math.PI);
      expect(degrees).toBeCloseTo(180, 5);
    });
  });

  describe('clamp', () => {
    it('should clamp value within range', () => {
      expect(Vector.clamp(5, 0, 10)).toBe(5);
      expect(Vector.clamp(-5, 0, 10)).toBe(0);
      expect(Vector.clamp(15, 0, 10)).toBe(10);
    });
  });

  describe('lerp', () => {
    it('should interpolate between vectors', () => {
      const v1 = Vector.create(0, 0, 0);
      const v2 = Vector.create(10, 10, 10);
      const result = Vector.lerp(v1, v2, 0.5);
      expect(result.x).toBeCloseTo(5, 5);
      expect(result.y).toBeCloseTo(5, 5);
      expect(result.z).toBeCloseTo(5, 5);
    });
  });

  describe('isParallel', () => {
    it('should detect parallel vectors', () => {
      const v1 = Vector.create(1, 0, 0);
      const v2 = Vector.create(2, 0, 0);
      expect(Vector.isParallel(v1, v2)).toBe(true);
    });

    it('should reject non-parallel vectors', () => {
      const v1 = Vector.create(1, 0, 0);
      const v2 = Vector.create(0, 1, 0);
      expect(Vector.isParallel(v1, v2)).toBe(false);
    });
  });

  describe('isPerpendicular', () => {
    it('should detect perpendicular vectors', () => {
      const v1 = Vector.create(1, 0, 0);
      const v2 = Vector.create(0, 1, 0);
      expect(Vector.isPerpendicular(v1, v2)).toBe(true);
    });

    it('should reject non-perpendicular vectors', () => {
      const v1 = Vector.create(1, 0, 0);
      const v2 = Vector.create(1, 1, 0);
      expect(Vector.isPerpendicular(v1, v2)).toBe(false);
    });
  });
});
