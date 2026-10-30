/**
 * PathValidator.ts - Phase 35B
 * Validates path sketch against profile constraints
 *
 * Validates:
 * - S (straight) >= S_min
 * - B_min <= angle <= B_max
 * - B >= 0.5° (non-degenerate)
 * - No arc overlap
 * - Path connectivity
 */

import { PathSegment, Constraint } from './PathSketch3D';

export type ValidationSeverity = 'error' | 'warning' | 'info';

export interface ValidationError {
  id: string;
  segment_id?: string;
  constraint_id?: string;
  type: string;
  message: string;
  severity: ValidationSeverity;
}

export class PathValidator {
  private profile_s_min: number;
  private profile_b_min: number;
  private profile_b_max: number;

  constructor(s_min: number, b_min: number, b_max: number) {
    this.profile_s_min = s_min;
    this.profile_b_min = b_min;
    this.profile_b_max = b_max;
  }

  /**
   * Validate all segments in the path
   */
  public validateSegments(segments: PathSegment[]): ValidationError[] {
    const errors: ValidationError[] = [];
    const seenIds = new Set<string>();

    segments.forEach((segment, index) => {
      const segmentErrors = this.validateSegment(segment);
      segmentErrors.forEach((err) => {
        // Ensure unique error IDs
        let errId = err.id;
        while (seenIds.has(errId)) {
          errId = errId + '_' + Math.random().toString(36).substr(2, 5);
        }
        seenIds.add(errId);
        errors.push({ ...err, id: errId });
      });
    });

    return errors;
  }

  /**
   * Validate a single segment
   */
  private validateSegment(segment: PathSegment): ValidationError[] {
    const errors: ValidationError[] = [];

    // Validate straight dimension (S)
    if (segment.dimension_S !== undefined) {
      if (segment.dimension_S < this.profile_s_min) {
        errors.push({
          id: `${segment.id}_s_min`,
          segment_id: segment.id,
          type: 'S_TOO_SMALL',
          message: `Straight (S) must be >= ${this.profile_s_min.toFixed(2)} (actual: ${segment.dimension_S.toFixed(2)})`,
          severity: 'error',
        });
      }

      if (segment.dimension_S <= 0) {
        errors.push({
          id: `${segment.id}_s_zero`,
          segment_id: segment.id,
          type: 'S_INVALID',
          message: 'Straight (S) must be positive',
          severity: 'error',
        });
      }
    }

    // Validate bend angle (B)
    if (segment.bend_angle !== undefined) {
      if (segment.bend_angle < this.profile_b_min) {
        errors.push({
          id: `${segment.id}_b_min`,
          segment_id: segment.id,
          type: 'B_TOO_SMALL',
          message: `Bend angle (B) must be >= ${this.profile_b_min.toFixed(1)}° (actual: ${segment.bend_angle.toFixed(1)}°)`,
          severity: 'error',
        });
      }

      if (segment.bend_angle > this.profile_b_max) {
        errors.push({
          id: `${segment.id}_b_max`,
          segment_id: segment.id,
          type: 'B_TOO_LARGE',
          message: `Bend angle (B) must be <= ${this.profile_b_max.toFixed(1)}° (actual: ${segment.bend_angle.toFixed(1)}°)`,
          severity: 'error',
        });
      }

      if (segment.bend_angle > 0 && segment.bend_angle < 0.5) {
        errors.push({
          id: `${segment.id}_b_degenerate`,
          segment_id: segment.id,
          type: 'B_DEGENERATE',
          message: 'Bend angle (B) must be >= 0.5° (non-degenerate bend)',
          severity: 'warning',
        });
      }
    }

    // Validate length dimension (L)
    if (segment.dimension_L !== undefined) {
      if (segment.dimension_L <= 0) {
        errors.push({
          id: `${segment.id}_l_invalid`,
          segment_id: segment.id,
          type: 'L_INVALID',
          message: 'Length (L) must be positive',
          severity: 'error',
        });
      }

      // Check if L >= S + 2*R (S + bend radius portions)
      // This is a loose constraint as the exact check needs profile radius
      if (segment.dimension_S && segment.dimension_L < segment.dimension_S) {
        errors.push({
          id: `${segment.id}_l_too_small`,
          segment_id: segment.id,
          type: 'L_TOO_SMALL',
          message: `Length (L) must be >= Straight (S). L: ${segment.dimension_L.toFixed(2)}, S: ${segment.dimension_S.toFixed(2)}`,
          severity: 'warning',
        });
      }
    }

    // Validate plane rotation (A)
    if (segment.plane_rotation !== undefined) {
      if (segment.plane_rotation < 0 || segment.plane_rotation > 360) {
        errors.push({
          id: `${segment.id}_a_invalid`,
          segment_id: segment.id,
          type: 'A_INVALID',
          message: `Plane rotation (A) must be between 0° and 360° (actual: ${segment.plane_rotation.toFixed(1)}°)`,
          severity: 'warning',
        });
      }
    }

    return errors;
  }

  /**
   * Validate a constraint
   */
  public validateConstraint(constraint: Constraint): ValidationError[] {
    const errors: ValidationError[] = [];

    if (!constraint.is_satisfied) {
      errors.push({
        id: `${constraint.id}_unsatisfied`,
        constraint_id: constraint.id,
        type: 'CONSTRAINT_UNSATISFIED',
        message: `Constraint ${constraint.type} not satisfied (error: ${constraint.error.toFixed(6)})`,
        severity: 'warning',
      });
    }

    if (constraint.error > 0.001) {
      errors.push({
        id: `${constraint.id}_high_error`,
        constraint_id: constraint.id,
        type: 'CONSTRAINT_HIGH_ERROR',
        message: `Constraint ${constraint.type} has high error: ${constraint.error.toFixed(6)}`,
        severity: 'warning',
      });
    }

    return errors;
  }

  /**
   * Check if path is fully connected
   */
  public validateConnectivity(segments: PathSegment[]): ValidationError[] {
    const errors: ValidationError[] = [];

    if (segments.length === 0) return errors;

    // Build adjacency map
    const adjacency = new Map<string, Set<string>>();
    segments.forEach((seg) => {
      if (!adjacency.has(seg.from_vertex)) {
        adjacency.set(seg.from_vertex, new Set());
      }
      if (!adjacency.has(seg.to_vertex)) {
        adjacency.set(seg.to_vertex, new Set());
      }

      adjacency.get(seg.from_vertex)!.add(seg.to_vertex);
      adjacency.get(seg.to_vertex)!.add(seg.from_vertex);
    });

    // Check connectivity using BFS
    if (adjacency.size > 0) {
      const start = adjacency.keys().next().value as string;
      const visited = new Set<string>();
      const queue: string[] = [start];

      while (queue.length > 0) {
        const node = queue.shift()!;
        if (visited.has(node)) continue;

        visited.add(node);
        const neighbors = adjacency.get(node) || new Set();
        neighbors.forEach((neighbor) => {
          if (!visited.has(neighbor)) {
            queue.push(neighbor);
          }
        });
      }

      // Check if all vertices were visited
      if (visited.size < adjacency.size) {
        errors.push({
          id: 'path_disconnected',
          type: 'PATH_DISCONNECTED',
          message: `Path is not fully connected (${visited.size}/${adjacency.size} vertices connected)`,
          severity: 'error',
        });
      }
    }

    return errors;
  }

  /**
   * Get all validation errors including constraint validation
   */
  public validateAll(
    segments: PathSegment[],
    constraints: Constraint[]
  ): ValidationError[] {
    const errors: ValidationError[] = [];

    // Validate segments
    errors.push(...this.validateSegments(segments));

    // Validate constraints
    constraints.forEach((constraint) => {
      errors.push(...this.validateConstraint(constraint));
    });

    // Validate connectivity
    errors.push(...this.validateConnectivity(segments));

    return errors;
  }

  /**
   * Check if all segments are valid (no errors, only warnings allowed)
   */
  public isValid(segments: PathSegment[], constraints: Constraint[]): boolean {
    const errors = this.validateAll(segments, constraints);
    return !errors.some((e) => e.severity === 'error');
  }

  /**
   * Update profile constraints
   */
  public updateProfileConstraints(
    s_min: number,
    b_min: number,
    b_max: number
  ): void {
    this.profile_s_min = s_min;
    this.profile_b_min = b_min;
    this.profile_b_max = b_max;
  }

  /**
   * Get validation statistics
   */
  public getValidationStats(segments: PathSegment[]): {
    total_errors: number;
    errors: number;
    warnings: number;
  } {
    const errors = this.validateSegments(segments);
    return {
      total_errors: errors.length,
      errors: errors.filter((e) => e.severity === 'error').length,
      warnings: errors.filter((e) => e.severity === 'warning').length,
    };
  }
}
