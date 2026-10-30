/**
 * BendCore Profile Manager
 * Manages bending profiles, diameters, and tube specifications
 * Phase 35A Translation: Profile Database and Tube Specification Validation
 */

import { promises as fs } from 'fs';
import path from 'path';
import {
  BendingProfile,
  ProfileDiameter,
  TubeSpecification,
  ValidationResult
} from '../models/types';

export class ProfileManager {
  private static instance: ProfileManager;
  private profiles: BendingProfile[] = [];
  private profilesLoaded: boolean = false;

  private constructor() {}

  /**
   * Get singleton instance
   */
  static getInstance(): ProfileManager {
    if (!ProfileManager.instance) {
      ProfileManager.instance = new ProfileManager();
    }
    return ProfileManager.instance;
  }

  /**
   * Load profiles from data/bending_profiles.json
   */
  async loadProfiles(dataDir: string = 'data'): Promise<boolean> {
    if (this.profilesLoaded) {
      return true;
    }

    try {
      const profilePath = path.join(dataDir, 'bending_profiles.json');
      const data = await fs.readFile(profilePath, 'utf-8');
      const parsed = JSON.parse(data);

      if (!parsed.profiles || !Array.isArray(parsed.profiles)) {
        console.error('Invalid bending_profiles.json structure');
        return false;
      }

      this.profiles = parsed.profiles.map((p: Record<string, unknown>) =>
        this.parseProfile(p)
      );

      this.profilesLoaded = true;
      console.log(`Loaded ${this.profiles.length} bending profiles`);
      return true;
    } catch (err) {
      console.error('Failed to load bending profiles:', err);
      return false;
    }
  }

  /**
   * Parse profile from JSON
   */
  private parseProfile(json: Record<string, unknown>): BendingProfile {
    const diameters = Array.isArray(json.diameters)
      ? json.diameters.map((d: Record<string, unknown>) => this.parseDiameter(d))
      : [];

    return {
      id: String(json.id || ''),
      name: String(json.name || ''),
      category: String(json.category || ''),
      manufacturer: String(json.manufacturer || ''),
      type: String(json.type || ''),
      description: String(json.description || ''),
      capacity_max_OD: Number(json.capacity_max_OD || 0),
      capacity_max_wall: Number(json.capacity_max_wall || 0),
      diameters
    };
  }

  /**
   * Parse diameter from JSON
   */
  private parseDiameter(json: Record<string, unknown>): ProfileDiameter {
    const discrete_values = Array.isArray(json.discrete_values)
      ? json.discrete_values.map(v => Number(v))
      : [];

    return {
      id: String(json.id || ''),
      OD: Number(json.OD || 0),
      R: Number(json.R || 0),
      S_min: Number(json.S_min || 0),
      N: Number(json.N || 1),
      B_min: Number(json.B_min || 0),
      B_max: Number(json.B_max || 179),
      discrete_type: (json.discrete_type === 'step' ? 'step' : 'continuous'),
      discrete_values: discrete_values.length > 0 ? discrete_values : undefined
    };
  }

  /**
   * Get profile by ID
   */
  getProfile(id: string): BendingProfile | null {
    if (!this.profilesLoaded) {
      return null;
    }

    const profile = this.profiles.find(p => p.id === id);
    return profile || null;
  }

  /**
   * List all profile IDs
   */
  listProfileIds(): string[] {
    return this.profiles.map(p => p.id);
  }

  /**
   * Get available diameters for a profile
   */
  getDiameters(profileId: string): ProfileDiameter[] {
    const profile = this.getProfile(profileId);
    return profile ? profile.diameters : [];
  }

  /**
   * Get constraints for a specific diameter in a profile
   */
  getDiameterConstraints(
    profileId: string,
    diameter: number
  ): ProfileDiameter | null {
    const profile = this.getProfile(profileId);
    if (!profile) {
      return null;
    }

    // Match diameter with tolerance (0.001 inch)
    const tolerance = 0.001;
    const match = profile.diameters.find(
      d => Math.abs(d.OD - diameter) < tolerance
    );

    return match || null;
  }

  /**
   * Check if profile exists
   */
  profileExists(id: string): boolean {
    return this.profiles.some(p => p.id === id);
  }

  /**
   * Get profile metadata
   */
  getProfileMetadata(id: string): Record<string, unknown> | null {
    const profile = this.getProfile(id);
    if (!profile) {
      return null;
    }

    return {
      id: profile.id,
      name: profile.name,
      category: profile.category,
      manufacturer: profile.manufacturer,
      type: profile.type,
      description: profile.description,
      capacity_max_OD: profile.capacity_max_OD,
      capacity_max_wall: profile.capacity_max_wall,
      diameter_count: profile.diameters.length
    };
  }

  /**
   * List all profiles with metadata
   */
  listProfiles(): BendingProfile[] {
    return this.profiles;
  }

  /**
   * Validate a tube specification against profile constraints
   */
  validateTubeSpec(spec: TubeSpecification): ValidationResult {
    const errors: string[] = [];
    const warnings: string[] = [];

    // Validate profile exists
    if (!this.profileExists(spec.profile_id)) {
      errors.push(`Profile not found: ${spec.profile_id}`);
    } else {
      // Get profile constraints
      const constraints = this.getDiameterConstraints(
        spec.profile_id,
        spec.diameter
      );

      if (!constraints) {
        errors.push(
          `Diameter ${spec.diameter}" not available for profile ${spec.profile_id}`
        );
      } else {
        // Validate diameter bounds
        if (spec.diameter < 0.1) {
          errors.push('Diameter must be greater than 0.1 inches');
        }

        if (spec.diameter > 10) {
          errors.push('Diameter must be less than 10 inches');
        }

        // Validate wall thickness
        if (spec.wall_thickness <= 0) {
          errors.push('Wall thickness must be positive');
        }

        if (spec.wall_thickness > spec.diameter / 2) {
          errors.push('Wall thickness cannot exceed radius');
        }

        // Validate minimum straight length
        if (constraints.S_min <= 0) {
          errors.push('Profile minimum straight length is invalid');
        }

        // Validate bend angle range
        if (constraints.B_min < 0 || constraints.B_max > 180) {
          errors.push('Profile bend angle constraints are invalid');
        }

        // For step-type discrete angles, validate values
        if (
          constraints.discrete_type === 'step' &&
          constraints.discrete_values
        ) {
          const hasInvalid = constraints.discrete_values.some(
            v => v < constraints.B_min || v > constraints.B_max
          );
          if (hasInvalid) {
            warnings.push('Some discrete bend angles are outside valid range');
          }
        }
      }
    }

    // Validate material
    if (!spec.material || spec.material.trim().length === 0) {
      errors.push('Material must be specified');
    }

    // Validate standard
    if (!spec.standard || spec.standard.trim().length === 0) {
      errors.push('Standard must be specified');
    }

    // Validate allowances
    if (spec.first_segment_allowance < 0) {
      errors.push('First segment allowance cannot be negative');
    }

    if (spec.last_segment_allowance < 0) {
      errors.push('Last segment allowance cannot be negative');
    }

    return {
      valid: errors.length === 0,
      errors,
      warnings
    };
  }

  /**
   * Clear profiles (for testing)
   */
  clear(): void {
    this.profiles = [];
    this.profilesLoaded = false;
  }

  /**
   * Seed test data (for unit/integration tests)
   */
  seedTestData(): void {
    this.profiles = [
      {
        id: 'test-id',
        name: 'Test ID Profile',
        category: 'tube',
        manufacturer: 'TEST',
        type: 'rotary_draw',
        description: 'Performance test profile',
        capacity_max_OD: 2.0,
        capacity_max_wall: 0.25,
        diameters: [
          { id: 'tid-d1', OD: 0.5, R: 1.5, S_min: 1.0, N: 1, B_min: 10, B_max: 180, discrete_type: 'continuous' }
        ]
      },
      {
        id: 'test-profile',
        name: 'Test Profile',
        category: 'tube',
        manufacturer: 'TEST',
        type: 'rotary_draw',
        description: 'Test bending profile',
        capacity_max_OD: 2.0,
        capacity_max_wall: 0.25,
        diameters: [
          { id: 'tp-d1', OD: 0.5, R: 1.5, S_min: 1.0, N: 1, B_min: 10, B_max: 180, discrete_type: 'continuous' }
        ]
      },
      {
        id: 'tube-profile-1',
        name: 'Tube Profile 1',
        category: 'tube',
        manufacturer: 'AMADA',
        type: 'rotary_draw',
        description: 'Standard tube profile',
        capacity_max_OD: 3.0,
        capacity_max_wall: 0.5,
        diameters: [
          { id: 'tp1-d1', OD: 1.0, R: 2.0, S_min: 1.5, N: 1, B_min: 15, B_max: 180, discrete_type: 'continuous' }
        ]
      }
    ];
    this.profilesLoaded = true;
  }
}
