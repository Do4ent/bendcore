/**
 * BendCore Tube Builder
 * Core geometry generation from path sketches and bending profiles
 * Phase 35C Translation: Tube body generation algorithm
 *
 * Algorithm (Р. Сафаров):
 * For each segment:
 *   T = R × tan(B/2)     [tangent offset]
 *   S = L - T_left - T_right  [straight segment]
 *   Create cylinder on S, torus on bend arc
 */

import { Vector3, Cylinder, Torus, TubeBody, TubeSpecification, ValidationCheck } from '../models/types';
import { Vector } from '../utils/vector';
import { v4 as uuidv4 } from 'uuid';

export interface PathSegment {
  start: Vector3;
  end: Vector3;
  bend_angle: number; // in degrees
  segment_id: string;
  is_straight: boolean;
}

export interface BendingProfile {
  profile_id: string;
  name: string;
  radius: number;
  min_bend_angle: number;
  max_bend_angle: number;
  min_straight: number;
  min_degenerate: number;
}

export class TubeBuilder {
  /**
   * Build complete tube from path, profile, and spec
   */
  buildTube(
    path: PathSegment[],
    profile: BendingProfile,
    spec: TubeSpecification
  ): TubeBody {
    const tubeBody: TubeBody = {
      tube_id: uuidv4(),
      straight_sections: [],
      bend_sections: [],
      total_length: 0,
      total_volume: 0,
      total_mass: 0,
      valid: true,
      validation_errors: [],
      validation_warnings: [],
      feature_ids: []
    };

    // Generate geometry sections
    for (let i = 0; i < path.length; i++) {
      const segment = path[i];

      if (segment.is_straight) {
        // Create cylinder for straight section
        const cylinder = this.createCylinder(segment, spec);
        tubeBody.straight_sections.push(cylinder);
        tubeBody.feature_ids.push(uuidv4());
      } else {
        // Create torus for bend section
        const torus = this.createTorus(segment, profile, spec);
        tubeBody.bend_sections.push(torus);
        tubeBody.feature_ids.push(uuidv4());
      }
    }

    // Calculate properties
    tubeBody.total_volume = this.calculateTotalVolume(
      tubeBody.straight_sections,
      tubeBody.bend_sections
    );

    tubeBody.total_mass = this.calculateMass(
      tubeBody.total_volume,
      2.7 // Default aluminum density g/cm³
    );

    // Validate tube
    this.validateTube(tubeBody, path, profile, spec);

    return tubeBody;
  }

  /**
   * Create hollow cylinder for straight segment
   */
  private createCylinder(segment: PathSegment, spec: TubeSpecification): Cylinder {
    const direction = Vector.normalize(Vector.subtract(segment.end, segment.start));
    const length = Vector.distance(segment.start, segment.end);
    const outerRadius = spec.diameter / 2;
    const innerRadius = outerRadius - spec.wall_thickness;

    return {
      center: segment.start,
      axis: direction,
      length,
      outer_radius: outerRadius,
      inner_radius: innerRadius,
      segment_id: segment.segment_id
    };
  }

  /**
   * Create hollow torus for bend segment
   */
  private createTorus(
    segment: PathSegment,
    profile: BendingProfile,
    spec: TubeSpecification
  ): Torus {
    const angleRad = Vector.toRadians(segment.bend_angle);
    const minorRadius = spec.wall_thickness;
    const majorRadius = profile.radius;

    // Calculate torus center and orientation
    const direction = Vector.normalize(Vector.subtract(segment.end, segment.start));
    const perpendicular = this.getPerpendicular(direction);

    return {
      center: segment.start,
      axis: perpendicular,
      major_radius: majorRadius,
      minor_radius: minorRadius,
      start_angle: 0,
      end_angle: angleRad,
      segment_id: segment.segment_id
    };
  }

  /**
   * Calculate tangent offset: T = R × tan(B/2)
   */
  private calculateTangent(radius: number, angleDegreesD: number): number {
    const angleRad = Vector.toRadians(angleDegreesD);
    return radius * Math.tan(angleRad / 2);
  }

  /**
   * Calculate straight segment: S = L - T_left - T_right
   */
  private calculateStraightSegment(
    totalLength: number,
    tangentLeft: number,
    tangentRight: number
  ): number {
    return Math.max(0, totalLength - tangentLeft - tangentRight);
  }

  /**
   * Calculate arc length: arc = R × B (in radians)
   */
  private calculateArcLength(radius: number, angleRadians: number): number {
    return radius * angleRadians;
  }

  /**
   * Get perpendicular vector to segment
   */
  private getPerpendicular(direction: Vector3): Vector3 {
    // Find a vector not parallel to direction
    let perpendicular: Vector3;

    if (Math.abs(direction.x) < 0.9) {
      perpendicular = Vector.create(1, 0, 0);
    } else {
      perpendicular = Vector.create(0, 1, 0);
    }

    const result = Vector.cross(direction, perpendicular);
    return Vector.normalize(result);
  }

  /**
   * Validate entire tube geometry
   */
  private validateTube(
    tube: TubeBody,
    path: PathSegment[],
    profile: BendingProfile,
    spec: TubeSpecification
  ): void {
    const checks: ValidationCheck[] = [];

    // Check 1: Straight segments
    if (!this.checkStraightSegments(path, profile, spec)) {
      tube.validation_errors.push('Some straight segments below minimum length');
      checks.push({
        name: 'Straight Segments',
        passed: false,
        message: `Minimum: ${profile.min_straight} inches`,
        severity: 'error'
      });
    }

    // Check 2: Bend angles
    if (!this.checkBendAngles(path, profile)) {
      tube.validation_errors.push('Some bend angles outside valid range');
      checks.push({
        name: 'Bend Angles',
        passed: false,
        message: `Range: ${profile.min_bend_angle}° - ${profile.max_bend_angle}°`,
        severity: 'error'
      });
    }

    // Check 3: Arc overlap
    if (!this.checkArcOverlap(path, spec, profile)) {
      tube.validation_warnings.push('Potential arc overlap detected');
      checks.push({
        name: 'Arc Overlap',
        passed: false,
        message: 'Adjacent bends may overlap',
        severity: 'warning'
      });
    }

    // Check 4: Degenerate bends
    if (!this.checkDegenerateBends(path, profile)) {
      tube.validation_warnings.push('Some bends may be degenerate');
      checks.push({
        name: 'Degenerate Bends',
        passed: false,
        message: `Minimum angle: ${profile.min_degenerate}°`,
        severity: 'warning'
      });
    }

    // Check 5: Diameter/wall validation
    if (spec.wall_thickness > spec.diameter / 2) {
      tube.validation_errors.push('Wall thickness exceeds radius');
    }

    if (spec.wall_thickness <= 0) {
      tube.validation_errors.push('Wall thickness must be positive');
    }

    tube.valid = tube.validation_errors.length === 0;
  }

  /**
   * Check straight segments meet minimum requirement
   */
  private checkStraightSegments(
    path: PathSegment[],
    profile: BendingProfile,
    spec: TubeSpecification
  ): boolean {
    for (const segment of path) {
      if (segment.is_straight) {
        const length = Vector.distance(segment.start, segment.end);
        if (length < profile.min_straight) {
          return false;
        }
      }
    }
    return true;
  }

  /**
   * Check bend angles within limits
   */
  private checkBendAngles(path: PathSegment[], profile: BendingProfile): boolean {
    for (const segment of path) {
      if (!segment.is_straight) {
        const angle = segment.bend_angle;
        if (angle < profile.min_bend_angle || angle > profile.max_bend_angle) {
          return false;
        }
      }
    }
    return true;
  }

  /**
   * Check for arc overlap at tangent points
   */
  private checkArcOverlap(
    path: PathSegment[],
    spec: TubeSpecification,
    profile: BendingProfile
  ): boolean {
    for (let i = 0; i < path.length - 1; i++) {
      if (!path[i].is_straight && !path[i + 1].is_straight) {
        const arc1 = this.calculateArcLength(
          profile.radius,
          Vector.toRadians(path[i].bend_angle)
        );
        const arc2 = this.calculateArcLength(
          profile.radius,
          Vector.toRadians(path[i + 1].bend_angle)
        );

        if (arc1 + arc2 > 0) {
          // They overlap
          return false;
        }
      }
    }
    return true;
  }

  /**
   * Check for degenerate bends
   */
  private checkDegenerateBends(path: PathSegment[], profile: BendingProfile): boolean {
    for (const segment of path) {
      if (!segment.is_straight) {
        if (segment.bend_angle < profile.min_degenerate) {
          return false;
        }
      }
    }
    return true;
  }

  /**
   * Calculate total volume in cm³
   */
  private calculateTotalVolume(cylinders: Cylinder[], tori: Torus[]): number {
    let volume = 0;

    // Cylinder volumes
    for (const cyl of cylinders) {
      const innerArea = Math.PI * cyl.inner_radius * cyl.inner_radius;
      const outerArea = Math.PI * cyl.outer_radius * cyl.outer_radius;
      volume += (outerArea - innerArea) * cyl.length;
    }

    // Torus volumes
    for (const tor of tori) {
      const sweepAngle = tor.end_angle - tor.start_angle;
      const minorRadius = tor.minor_radius;
      const majorRadius = tor.major_radius;

      // Volume of torus segment
      volume +=
        2 *
        Math.PI *
        minorRadius *
        minorRadius *
        majorRadius *
        (sweepAngle / (2 * Math.PI));
    }

    return volume;
  }

  /**
   * Calculate total mass
   */
  private calculateMass(volumeCm3: number, density: number): number {
    // volume in cm³, density in g/cm³ -> mass in grams
    return volumeCm3 * density;
  }
}
