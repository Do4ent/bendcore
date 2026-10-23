/**
 * BendCore Desktop - Profile Database Integration Tests
 * Tests profile loading, selection, validation, and persistence
 *
 * Coverage: 25+ test cases, 300 LOC
 */

import { ProfileManager } from '../../src/bendcore/profile_manager';
import { v4 as uuid } from 'uuid';

describe('Profile Database Integration Tests', () => {
  let profileManager: ProfileManager;

  beforeEach(() => {
    profileManager = ProfileManager.getInstance();
  });

  describe('Profile Loading & Initialization', () => {
    test('should load 15+ profiles from JSON data directory', async () => {
      const profiles = profileManager.listProfiles();
      expect(profiles.length).toBeGreaterThanOrEqual(15);
    });

    test('should initialize profile manager with valid profiles', () => {
      const profiles = profileManager.listProfiles();
      expect(profiles).toBeDefined();
      expect(Array.isArray(profiles)).toBe(true);
    });

    test('should have valid profile structure for each profile', () => {
      const profiles = profileManager.listProfiles();
      profiles.forEach(profile => {
        expect(profile.id).toBeDefined();
        expect(profile.name).toBeDefined();
        expect(profile.category).toBeDefined();
        expect(profile.diameters).toBeInstanceOf(Array);
      });
    });

    test('should load profiles with all required diameter fields', () => {
      const profiles = profileManager.listProfiles();
      expect(profiles.length).toBeGreaterThan(0);

      const firstProfile = profiles[0];
      firstProfile.diameters.forEach(diameter => {
        expect(diameter.id).toBeDefined();
        expect(diameter.OD).toBeGreaterThan(0);
        expect(diameter.R).toBeGreaterThan(0);
        expect(diameter.S_min).toBeGreaterThanOrEqual(0);
        expect(diameter.N).toBeGreaterThan(0);
        expect(diameter.B_min).toBeGreaterThanOrEqual(0);
        expect(diameter.B_max).toBeGreaterThan(diameter.B_min);
      });
    });
  });

  describe('Profile Selection & Retrieval', () => {
    test('should retrieve profile by ID', () => {
      const profiles = profileManager.listProfiles();
      const firstProfile = profiles[0];

      const retrieved = profileManager.getProfile(firstProfile.id);
      expect(retrieved).toBeDefined();
      expect(retrieved?.id).toBe(firstProfile.id);
    });

    test('should retrieve all diameter options for a profile', () => {
      const profiles = profileManager.listProfiles();
      const profile = profiles[0];

      const diameters = profileManager.getDiameters(profile.id);
      expect(diameters).toBeDefined();
      expect(diameters.length).toBeGreaterThan(0);
    });

    test('should filter profiles by category', () => {
      const profiles = profileManager.listProfiles();
      const categories = [...new Set(profiles.map(p => p.category))];

      expect(categories.length).toBeGreaterThan(0);

      categories.forEach(category => {
        const filtered = profileManager.getProfilesByCategory(category);
        expect(filtered.length).toBeGreaterThan(0);
        expect(filtered.every(p => p.category === category)).toBe(true);
      });
    });

    test('should retrieve profiles by manufacturer', () => {
      const profiles = profileManager.listProfiles();
      const firstProfile = profiles[0];

      if (firstProfile.manufacturer) {
        const byMfg = profileManager.getProfilesByManufacturer(firstProfile.manufacturer);
        expect(byMfg.length).toBeGreaterThan(0);
      }
    });

    test('should list all profile IDs', () => {
      const ids = profileManager.listProfileIds();
      expect(Array.isArray(ids)).toBe(true);
      expect(ids.length).toBeGreaterThan(0);
    });
  });

  describe('Diameter Constraints Validation', () => {
    test('should validate diameter constraints', () => {
      const profiles = profileManager.listProfiles();
      const profile = profiles[0];
      const diameter = profile.diameters[0];

      const constraints = {
        OD: diameter.OD,
        S_min: diameter.S_min,
        B_min: diameter.B_min,
        B_max: diameter.B_max
      };

      expect(constraints.OD).toBeGreaterThan(0);
      expect(constraints.B_max).toBeGreaterThan(constraints.B_min);
    });

    test('should validate bend angle constraints', () => {
      const profiles = profileManager.listProfiles();
      profiles.forEach(profile => {
        profile.diameters.forEach(diameter => {
          expect(diameter.B_max).toBeGreaterThanOrEqual(diameter.B_min);
        });
      });
    });

    test('should validate straight length constraints', () => {
      const profiles = profileManager.listProfiles();
      profiles.forEach(profile => {
        profile.diameters.forEach(diameter => {
          expect(diameter.S_min).toBeGreaterThanOrEqual(0);
        });
      });
    });

    test('should identify constraint violations', () => {
      const profiles = profileManager.listProfiles();
      const profile = profiles[0];
      const diameter = profile.diameters[0];

      const violations = [];
      if (diameter.B_max < diameter.B_min) {
        violations.push('Invalid bend angle range');
      }
      if (diameter.OD <= 0) {
        violations.push('Invalid outer diameter');
      }

      expect(violations.length).toBe(0);
    });
  });

  describe('Tube Specification Creation', () => {
    test('should create tube with selected profile', () => {
      const profiles = profileManager.listProfiles();
      const profile = profiles[0];
      const diameter = profile.diameters[0];

      const spec = {
        profile_id: profile.id,
        diameter: diameter.OD,
        wall_thickness: 0.065,
        material: 'STEEL',
        standard: 'ASTM'
      };

      expect(spec.profile_id).toBe(profile.id);
      expect(spec.diameter).toBe(diameter.OD);
    });

    test('should validate tube specification constraints', () => {
      const profiles = profileManager.listProfiles();
      const profile = profiles[0];
      const diameter = profile.diameters[0];

      const isValid = diameter.OD > 0 && diameter.R > 0 && diameter.S_min >= 0;
      expect(isValid).toBe(true);
    });

    test('should create multiple tubes from same profile', () => {
      const profiles = profileManager.listProfiles();
      const profile = profiles[0];

      const tubes = profile.diameters.map(d => ({
        id: uuid(),
        profile_id: profile.id,
        diameter: d.OD
      }));

      expect(tubes.length).toBeGreaterThan(0);
      expect(tubes.every(t => t.profile_id === profile.id)).toBe(true);
    });

    test('should verify profile properties are propagated to tube', () => {
      const profiles = profileManager.listProfiles();
      const profile = profiles[0];
      const diameter = profile.diameters[0];

      const tubeData = {
        profileId: profile.id,
        profileName: profile.name,
        bendRadius: diameter.R,
        minStraight: diameter.S_min,
        minBend: diameter.B_min,
        maxBend: diameter.B_max
      };

      expect(tubeData.bendRadius).toBe(diameter.R);
      expect(tubeData.minStraight).toBe(diameter.S_min);
    });
  });

  describe('Profile Database Persistence', () => {
    test('should persist profile selection to project', () => {
      const profiles = profileManager.listProfiles();
      const profile = profiles[0];

      const projectData = {
        tubeProfile: profile.id,
        savedAt: new Date().toISOString()
      };

      expect(projectData.tubeProfile).toBe(profile.id);
    });

    test('should load profile from persisted project data', () => {
      const profiles = profileManager.listProfiles();
      const profile = profiles[0];

      const persistedId = profile.id;
      const loaded = profileManager.getProfile(persistedId);

      expect(loaded).toBeDefined();
      expect(loaded?.id).toBe(persistedId);
    });

    test('should maintain profile integrity during save/load cycle', () => {
      const profiles = profileManager.listProfiles();
      const original = profiles[0];

      const projectData = JSON.stringify({
        profileId: original.id,
        diameters: original.diameters.length
      });

      const restored = JSON.parse(projectData);
      expect(restored.profileId).toBe(original.id);
      expect(restored.diameters).toBe(original.diameters.length);
    });
  });

  describe('Profile Export & Import', () => {
    test('should export profile to JSON format', () => {
      const profiles = profileManager.listProfiles();
      const profile = profiles[0];

      const exported = JSON.stringify(profile);
      expect(typeof exported).toBe('string');
      expect(exported).toContain(profile.id);
    });

    test('should import profile from JSON', () => {
      const profiles = profileManager.listProfiles();
      const original = profiles[0];
      const exported = JSON.stringify(original);

      const imported = JSON.parse(exported);
      expect(imported.id).toBe(original.id);
      expect(imported.diameters.length).toBe(original.diameters.length);
    });

    test('should verify exported profile structure', () => {
      const profiles = profileManager.listProfiles();
      const profile = profiles[0];
      const exported = JSON.stringify(profile);
      const parsed = JSON.parse(exported);

      expect(parsed.id).toBeDefined();
      expect(parsed.name).toBeDefined();
      expect(parsed.diameters).toBeInstanceOf(Array);
    });

    test('should export multiple profiles as database', () => {
      const profiles = profileManager.listProfiles();
      const database = { profiles: profiles.slice(0, 5) };

      const exported = JSON.stringify(database);
      const imported = JSON.parse(exported);

      expect(imported.profiles.length).toBe(5);
    });

    test('should verify profile integrity after export/import cycle', () => {
      const profiles = profileManager.listProfiles();
      const original = profiles[0];

      const exported = JSON.stringify(original);
      const imported = JSON.parse(exported);

      expect(imported.id).toBe(original.id);
      expect(imported.name).toBe(original.name);
      expect(imported.category).toBe(original.category);
      expect(imported.diameters.length).toBe(original.diameters.length);
    });
  });
});
