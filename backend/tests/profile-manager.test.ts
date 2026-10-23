/**
 * BendCore Profile Manager Tests
 * Unit tests for profile loading and validation
 */

import { ProfileManager } from '../src/core/profile-manager';
import { TubeSpecification } from '../src/models/types';
import path from 'path';

describe('ProfileManager', () => {
  let manager: ProfileManager;

  beforeEach(() => {
    manager = ProfileManager.getInstance();
    manager.clear();
  });

  describe('loadProfiles', () => {
    it('should load profiles from bending_profiles.json', async () => {
      const dataPath = path.join(process.cwd(), '..', 'data');
      const result = await manager.loadProfiles(dataPath);

      expect(result).toBe(true);
      expect(manager.listProfileIds().length).toBeGreaterThan(0);
    });

    it('should handle missing file gracefully', async () => {
      const result = await manager.loadProfiles('/nonexistent/path');
      expect(result).toBe(false);
    });

    it('should cache profiles after loading', async () => {
      const dataPath = path.join(process.cwd(), '..', 'data');
      await manager.loadProfiles(dataPath);

      const firstLoad = manager.listProfileIds();
      const secondLoad = manager.listProfileIds();

      expect(firstLoad).toEqual(secondLoad);
    });
  });

  describe('getProfile', () => {
    beforeEach(async () => {
      const dataPath = path.join(process.cwd(), '..', 'data');
      await manager.loadProfiles(dataPath);
    });

    it('should return profile by ID', () => {
      const profile = manager.getProfile('manual-half-inch');
      expect(profile).toBeDefined();
      expect(profile?.name).toContain('½');
    });

    it('should return null for non-existent profile', () => {
      const profile = manager.getProfile('nonexistent');
      expect(profile).toBeNull();
    });

    it('should have valid profile structure', () => {
      const profile = manager.getProfile('manual-half-inch');
      expect(profile).toHaveProperty('id');
      expect(profile).toHaveProperty('name');
      expect(profile).toHaveProperty('category');
      expect(profile).toHaveProperty('diameters');
      expect(Array.isArray(profile?.diameters)).toBe(true);
    });
  });

  describe('getDiameters', () => {
    beforeEach(async () => {
      const dataPath = path.join(process.cwd(), '..', 'data');
      await manager.loadProfiles(dataPath);
    });

    it('should return diameters for valid profile', () => {
      const diameters = manager.getDiameters('manual-half-inch');
      expect(diameters.length).toBeGreaterThan(0);
      expect(diameters[0]).toHaveProperty('OD');
      expect(diameters[0]).toHaveProperty('R');
    });

    it('should return empty array for invalid profile', () => {
      const diameters = manager.getDiameters('nonexistent');
      expect(diameters).toEqual([]);
    });
  });

  describe('getDiameterConstraints', () => {
    beforeEach(async () => {
      const dataPath = path.join(process.cwd(), '..', 'data');
      await manager.loadProfiles(dataPath);
    });

    it('should return constraints for exact diameter match', () => {
      const constraints = manager.getDiameterConstraints('manual-half-inch', 0.5);
      expect(constraints).toBeDefined();
      expect(constraints?.OD).toBe(0.5);
    });

    it('should return constraints with tolerance', () => {
      const constraints = manager.getDiameterConstraints(
        'manual-half-inch',
        0.5001
      );
      expect(constraints).toBeDefined();
      expect(constraints?.OD).toBeCloseTo(0.5, 3);
    });

    it('should return null for non-matching diameter', () => {
      const constraints = manager.getDiameterConstraints(
        'manual-half-inch',
        99.9
      );
      expect(constraints).toBeNull();
    });
  });

  describe('validateTubeSpec', () => {
    beforeEach(async () => {
      const dataPath = path.join(process.cwd(), '..', 'data');
      await manager.loadProfiles(dataPath);
    });

    it('should validate correct specification', () => {
      const spec: TubeSpecification = {
        standard: 'ANSI/ASME',
        material: 'steel-1020',
        type: 'tube',
        profile_id: 'manual-half-inch',
        diameter: 0.5,
        wall_thickness: 0.049,
        first_segment_allowance: 0.5,
        last_segment_allowance: 0.5,
        calculate_allowance: true
      };

      const result = manager.validateTubeSpec(spec);
      expect(result.valid).toBe(true);
      expect(result.errors.length).toBe(0);
    });

    it('should reject invalid profile', () => {
      const spec: TubeSpecification = {
        standard: 'ANSI/ASME',
        material: 'steel-1020',
        type: 'tube',
        profile_id: 'nonexistent',
        diameter: 0.5,
        wall_thickness: 0.049,
        first_segment_allowance: 0.5,
        last_segment_allowance: 0.5,
        calculate_allowance: true
      };

      const result = manager.validateTubeSpec(spec);
      expect(result.valid).toBe(false);
      expect(result.errors.some(e => e.includes('Profile not found'))).toBe(
        true
      );
    });

    it('should reject invalid diameter', () => {
      const spec: TubeSpecification = {
        standard: 'ANSI/ASME',
        material: 'steel-1020',
        type: 'tube',
        profile_id: 'manual-half-inch',
        diameter: 99.9,
        wall_thickness: 0.049,
        first_segment_allowance: 0.5,
        last_segment_allowance: 0.5,
        calculate_allowance: true
      };

      const result = manager.validateTubeSpec(spec);
      expect(result.valid).toBe(false);
    });

    it('should reject invalid wall thickness', () => {
      const spec: TubeSpecification = {
        standard: 'ANSI/ASME',
        material: 'steel-1020',
        type: 'tube',
        profile_id: 'manual-half-inch',
        diameter: 0.5,
        wall_thickness: -0.049,
        first_segment_allowance: 0.5,
        last_segment_allowance: 0.5,
        calculate_allowance: true
      };

      const result = manager.validateTubeSpec(spec);
      expect(result.valid).toBe(false);
    });

    it('should reject wall thickness > radius', () => {
      const spec: TubeSpecification = {
        standard: 'ANSI/ASME',
        material: 'steel-1020',
        type: 'tube',
        profile_id: 'manual-half-inch',
        diameter: 0.5,
        wall_thickness: 0.3,
        first_segment_allowance: 0.5,
        last_segment_allowance: 0.5,
        calculate_allowance: true
      };

      const result = manager.validateTubeSpec(spec);
      expect(result.valid).toBe(false);
    });
  });

  describe('listProfileIds', () => {
    beforeEach(async () => {
      const dataPath = path.join(process.cwd(), '..', 'data');
      await manager.loadProfiles(dataPath);
    });

    it('should return array of profile IDs', () => {
      const ids = manager.listProfileIds();
      expect(Array.isArray(ids)).toBe(true);
      expect(ids.length).toBeGreaterThan(0);
      expect(ids[0]).toMatch(/^[a-z-]+$/);
    });
  });

  describe('profileExists', () => {
    beforeEach(async () => {
      const dataPath = path.join(process.cwd(), '..', 'data');
      await manager.loadProfiles(dataPath);
    });

    it('should return true for existing profile', () => {
      const exists = manager.profileExists('manual-half-inch');
      expect(exists).toBe(true);
    });

    it('should return false for non-existing profile', () => {
      const exists = manager.profileExists('nonexistent');
      expect(exists).toBe(false);
    });
  });

  describe('getProfileMetadata', () => {
    beforeEach(async () => {
      const dataPath = path.join(process.cwd(), '..', 'data');
      await manager.loadProfiles(dataPath);
    });

    it('should return metadata for valid profile', () => {
      const metadata = manager.getProfileMetadata('manual-half-inch');
      expect(metadata).toBeDefined();
      expect(metadata?.id).toBe('manual-half-inch');
      expect(metadata?.name).toBeDefined();
      expect(metadata?.diameter_count).toBeGreaterThan(0);
    });

    it('should return null for invalid profile', () => {
      const metadata = manager.getProfileMetadata('nonexistent');
      expect(metadata).toBeNull();
    });
  });
});
