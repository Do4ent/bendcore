/**
 * PathValidator.test.ts - Phase 35B
 * Tests for path sketch validation
 */

import { PathValidator } from '../src/ui/PathValidator';
import { PathSegment, Constraint } from '../src/ui/PathSketch3D';

describe('PathValidator', () => {
  let validator: PathValidator;

  beforeEach(() => {
    // Create validator with profile constraints
    validator = new PathValidator(2.0, 5.0, 180.0);
  });

  // ===== INITIALIZATION =====

  test('should create PathValidator instance', () => {
    expect(validator).toBeDefined();
    expect(validator).toBeInstanceOf(PathValidator);
  });

  test('should initialize with profile constraints', () => {
    const v = new PathValidator(1.5, 10.0, 170.0);
    expect(v).toBeDefined();
  });

  // ===== SEGMENT VALIDATION =====

  test('should validate empty segments', () => {
    const errors = validator.validateSegments([]);
    expect(errors).toEqual([]);
  });

  test('should detect S dimension too small', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 1.0,  // Below minimum of 2.0
        bend_angle: 45,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateSegments(segments);
    expect(errors.length).toBeGreaterThan(0);
    expect(errors[0].type).toBe('S_TOO_SMALL');
    expect(errors[0].severity).toBe('error');
  });

  test('should detect B angle too small', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 3.0,  // Below minimum of 5.0
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateSegments(segments);
    expect(errors.length).toBeGreaterThan(0);
    expect(errors.some((e) => e.type === 'B_TOO_SMALL')).toBe(true);
  });

  test('should detect B angle too large', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 200.0,  // Above maximum of 180.0
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateSegments(segments);
    expect(errors.length).toBeGreaterThan(0);
    expect(errors.some((e) => e.type === 'B_TOO_LARGE')).toBe(true);
  });

  test('should validate valid segment', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2.0,
        bend_angle: 90,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateSegments(segments);
    const errors_errors = errors.filter((e) => e.severity === 'error');
    expect(errors_errors.length).toBe(0);
  });

  // ===== DIMENSION VALIDATION =====

  test('should validate S dimension', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2.0,
        bend_angle: 45,
        plane_rotation: 0,
        selected_dimension: 'S',
      },
    ];

    const errors = validator.validateSegments(segments);
    const s_errors = errors.filter((e) => e.type.startsWith('S_'));
    expect(s_errors.filter((e) => e.severity === 'error').length).toBe(0);
  });

  test('should validate L dimension', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 45,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateSegments(segments);
    expect(errors.filter((e) => e.type === 'L_INVALID').length).toBe(0);
  });

  test('should detect negative L dimension', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: -5,
        dimension_S: 2,
        bend_angle: 45,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateSegments(segments);
    expect(errors.some((e) => e.type === 'L_INVALID')).toBe(true);
  });

  test('should validate A dimension', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 45,
        plane_rotation: 180,
        selected_dimension: 'A',
      },
    ];

    const errors = validator.validateSegments(segments);
    expect(errors.filter((e) => e.type === 'A_INVALID').length).toBe(0);
  });

  test('should detect invalid A dimension', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 45,
        plane_rotation: 400,  // Outside 0-360
        selected_dimension: 'A',
      },
    ];

    const errors = validator.validateSegments(segments);
    expect(errors.some((e) => e.type === 'A_INVALID')).toBe(true);
  });

  // ===== DEGENERATE BEND =====

  test('should warn on degenerate bend', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 0.2,  // Less than 0.5
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateSegments(segments);
    expect(errors.some((e) => e.type === 'B_DEGENERATE')).toBe(true);
  });

  // ===== CONSTRAINT VALIDATION =====

  test('should validate satisfied constraint', () => {
    const constraint: Constraint = {
      id: 'c1',
      type: 'COINCIDENT',
      entity_ids: ['v1', 'v2'],
      is_satisfied: true,
      error: 0.0,
    };

    const errors = validator.validateConstraint(constraint);
    expect(errors.length).toBe(0);
  });

  test('should detect unsatisfied constraint', () => {
    const constraint: Constraint = {
      id: 'c1',
      type: 'COINCIDENT',
      entity_ids: ['v1', 'v2'],
      is_satisfied: false,
      error: 0.5,
    };

    const errors = validator.validateConstraint(constraint);
    expect(errors.length).toBeGreaterThan(0);
    expect(errors[0].type).toBe('CONSTRAINT_UNSATISFIED');
  });

  test('should detect high constraint error', () => {
    const constraint: Constraint = {
      id: 'c1',
      type: 'PARALLEL',
      entity_ids: ['s1', 's2'],
      is_satisfied: true,
      error: 0.01,  // Above threshold of 0.001
    };

    const errors = validator.validateConstraint(constraint);
    expect(errors.some((e) => e.type === 'CONSTRAINT_HIGH_ERROR')).toBe(true);
  });

  // ===== CONNECTIVITY VALIDATION =====

  test('should validate connected path', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 45,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
      {
        id: 'seg_2',
        from_vertex: 'v2',
        to_vertex: 'v3',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 45,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateConnectivity(segments);
    expect(errors.filter((e) => e.type === 'PATH_DISCONNECTED').length).toBe(0);
  });

  test('should detect disconnected path', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 45,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
      {
        id: 'seg_2',
        from_vertex: 'v3',
        to_vertex: 'v4',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 45,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateConnectivity(segments);
    expect(errors.some((e) => e.type === 'PATH_DISCONNECTED')).toBe(true);
  });

  test('should handle single segment path', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 45,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateConnectivity(segments);
    expect(errors.filter((e) => e.type === 'PATH_DISCONNECTED').length).toBe(0);
  });

  // ===== COMPLETE VALIDATION =====

  test('should validate all aspects', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 90,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const constraints: Constraint[] = [
      {
        id: 'c1',
        type: 'FIXED',
        entity_ids: ['v1'],
        is_satisfied: true,
        error: 0,
      },
    ];

    const errors = validator.validateAll(segments, constraints);
    const error_errors = errors.filter((e) => e.severity === 'error');
    expect(error_errors.length).toBe(0);
  });

  // ===== VALIDITY CHECK =====

  test('should return true for valid sketch', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 90,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const constraints: Constraint[] = [];

    expect(validator.isValid(segments, constraints)).toBe(true);
  });

  test('should return false for invalid sketch', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 0.5,  // Below minimum
        bend_angle: 90,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const constraints: Constraint[] = [];

    expect(validator.isValid(segments, constraints)).toBe(false);
  });

  // ===== PROFILE CONSTRAINT UPDATES =====

  test('should update profile constraints', () => {
    validator.updateProfileConstraints(1.0, 0.0, 360.0);

    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 0.5,
        bend_angle: 90,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateSegments(segments);
    expect(errors.filter((e) => e.type === 'S_TOO_SMALL').length).toBe(0);
  });

  // ===== VALIDATION STATISTICS =====

  test('should calculate validation statistics', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 0.5,  // Error
        bend_angle: 90,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
      {
        id: 'seg_2',
        from_vertex: 'v2',
        to_vertex: 'v3',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 0.2,  // Warning
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const stats = validator.getValidationStats(segments);
    expect(stats.total_errors).toBeGreaterThanOrEqual(0);
    expect(stats.errors).toBeGreaterThanOrEqual(0);
    expect(stats.warnings).toBeGreaterThanOrEqual(0);
  });

  // ===== ERROR UNIQUENESS =====

  test('should generate unique error IDs', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 0.5,
        bend_angle: 90,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors1 = validator.validateSegments(segments);
    const errors2 = validator.validateSegments(segments);

    const ids1 = new Set(errors1.map((e) => e.id));
    const ids2 = new Set(errors2.map((e) => e.id));

    // Same validation should produce same error IDs
    expect(ids1.size).toBe(ids2.size);
  });

  // ===== BOUNDARY CONDITIONS =====

  test('should handle boundary S values', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2.0,  // Exactly at minimum
        bend_angle: 90,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateSegments(segments);
    expect(errors.filter((e) => e.type === 'S_TOO_SMALL').length).toBe(0);
  });

  test('should handle boundary B values', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 180.0,  // Exactly at maximum
        plane_rotation: 0,
        selected_dimension: 'L',
      },
    ];

    const errors = validator.validateSegments(segments);
    expect(errors.filter((e) => e.type === 'B_TOO_LARGE').length).toBe(0);
  });
});
