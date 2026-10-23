/**
 * BendCore Geometry Rounding Tests
 * Unit tests for standard size rounding
 */

import { GeometryRounder } from '../src/core/geometry-rounding';

describe('GeometryRounder', () => {
  describe('roundDiameter', () => {
    it('should round to nearest standard diameter', () => {
      const result = GeometryRounder.roundDiameter(0.5);
      expect(result).toBe(0.5);
    });

    it('should round between standards', () => {
      const result = GeometryRounder.roundDiameter(0.49);
      expect(GeometryRounder.getStandardDiameters()).toContain(result);
    });

    it('should handle small diameters', () => {
      const result = GeometryRounder.roundDiameter(0.15);
      expect(result).toBeGreaterThan(0);
    });

    it('should handle large diameters', () => {
      const result = GeometryRounder.roundDiameter(3.2);
      expect(GeometryRounder.getStandardDiameters()).toContain(result);
    });
  });

  describe('roundRadius', () => {
    it('should round to standard radius', () => {
      const result = GeometryRounder.roundRadius(1.5);
      expect(result).toBe(1.5);
    });

    it('should round to nearest standard', () => {
      const result = GeometryRounder.roundRadius(1.4);
      expect([1, 1.5, 2]).toContain(result);
    });
  });

  describe('roundAngle', () => {
    it('should round to 0.1 degrees', () => {
      const result = GeometryRounder.roundAngle(45.67);
      expect(result).toBe(45.7);
    });

    it('should round down correctly', () => {
      const result = GeometryRounder.roundAngle(45.64);
      expect(result).toBe(45.6);
    });
  });

  describe('roundLength', () => {
    it('should round to nearest mm', () => {
      const inchValue = 1.0; // approximately 25.4mm
      const result = GeometryRounder.roundLength(inchValue);
      expect(result).toBeCloseTo(inchValue, 3);
    });
  });

  describe('roundWallThickness', () => {
    it('should round to standard thickness', () => {
      const result = GeometryRounder.roundWallThickness(0.065);
      expect(result).toBe(0.065);
    });

    it('should round to nearest standard', () => {
      const result = GeometryRounder.roundWallThickness(0.066);
      expect([0.049, 0.065, 0.083]).toContain(result);
    });
  });

  describe('isValidAngle', () => {
    it('should accept valid angles', () => {
      expect(GeometryRounder.isValidAngle(45)).toBe(true);
      expect(GeometryRounder.isValidAngle(90)).toBe(true);
      expect(GeometryRounder.isValidAngle(179)).toBe(true);
    });

    it('should reject invalid angles', () => {
      expect(GeometryRounder.isValidAngle(0)).toBe(false);
      expect(GeometryRounder.isValidAngle(180)).toBe(false);
    });
  });

  describe('isStandardDiameter', () => {
    it('should recognize standard diameters', () => {
      expect(GeometryRounder.isStandardDiameter(0.5)).toBe(true);
      expect(GeometryRounder.isStandardDiameter(1.0)).toBe(true);
    });

    it('should reject non-standard diameters', () => {
      expect(GeometryRounder.isStandardDiameter(0.55)).toBe(false);
      expect(GeometryRounder.isStandardDiameter(1.05)).toBe(false);
    });
  });

  describe('getStandardDiameters', () => {
    it('should return array of standard diameters', () => {
      const diameters = GeometryRounder.getStandardDiameters();
      expect(Array.isArray(diameters)).toBe(true);
      expect(diameters.length).toBeGreaterThan(0);
      expect(diameters[0]).toBeGreaterThan(0);
    });

    it('should return sorted array', () => {
      const diameters = GeometryRounder.getStandardDiameters();
      for (let i = 1; i < diameters.length; i++) {
        expect(diameters[i]).toBeGreaterThanOrEqual(diameters[i - 1]);
      }
    });
  });
});
