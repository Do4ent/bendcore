/**
 * BendCore Geometry Recognition
 * Extract and recognize tube features from 3D geometry
 * Phase 35E: Geometry recognition and feature extraction
 */

import { Vector3 } from '../models/types';
import { Vector } from '../utils/vector';
import { GeometryRounder } from './geometry-rounding';

export interface RecognitionResult {
  diameter: number;
  diameter_confidence: number;
  bend_angles: number[];
  angle_confidence: number;
  straight_segments: number[];
  segment_confidence: number;
  total_confidence: number;
  profile_suggestion?: string;
  warnings: string[];
}

export class GeometryRecognizer {
  /**
   * Recognize tube from centerline geometry
   */
  static recognizeTube(centerline: Vector3[]): RecognitionResult {
    const result: RecognitionResult = {
      diameter: 0.5,
      diameter_confidence: 0,
      bend_angles: [],
      angle_confidence: 0,
      straight_segments: [],
      segment_confidence: 0,
      total_confidence: 0,
      warnings: []
    };

    if (centerline.length < 2) {
      result.warnings.push('Insufficient centerline points');
      return result;
    }

    // Extract bend angles from direction changes
    const bendAngles = this.extractBendAngles(centerline);
    result.bend_angles = bendAngles.angles;
    result.angle_confidence = bendAngles.confidence;

    // Extract straight segments
    const segments = this.extractStraightSegments(centerline);
    result.straight_segments = segments.lengths;
    result.segment_confidence = segments.confidence;

    // Estimate diameter (would need 3D geometry for accurate measurement)
    const diameterEstimate = this.estimateDiameter(centerline);
    result.diameter = diameterEstimate.value;
    result.diameter_confidence = diameterEstimate.confidence;

    // Round to standard
    const rounded = GeometryRounder.roundDiameter(result.diameter);
    if (Math.abs(rounded - result.diameter) > 0.01) {
      result.warnings.push(
        `Diameter rounded from ${result.diameter.toFixed(3)}" to ${rounded.toFixed(3)}"`
      );
    }
    result.diameter = rounded;

    // Calculate total confidence
    result.total_confidence =
      (result.diameter_confidence +
        result.angle_confidence +
        result.segment_confidence) /
      3;

    // Suggest profile based on diameter
    result.profile_suggestion = this.suggestProfile(result.diameter);

    return result;
  }

  /**
   * Extract bend angles from centerline
   */
  private static extractBendAngles(
    centerline: Vector3[]
  ): { angles: number[]; confidence: number } {
    const angles: number[] = [];
    let confidence = 0;

    if (centerline.length < 3) {
      return { angles, confidence: 0 };
    }

    // Calculate angle at each vertex
    for (let i = 1; i < centerline.length - 1; i++) {
      const p1 = centerline[i - 1];
      const p2 = centerline[i];
      const p3 = centerline[i + 1];

      const v1 = Vector.normalize(Vector.subtract(p2, p1));
      const v2 = Vector.normalize(Vector.subtract(p3, p2));

      const angle = Vector.angle(v1, v2);
      const angleDeg = Vector.toDegrees(angle);

      // Only include significant angles (> 1 degree)
      if (angleDeg > 1) {
        angles.push(Math.round(angleDeg * 10) / 10);
      }
    }

    // Confidence based on number of distinct bends
    confidence = Math.min(1.0, angles.length / 5);

    return { angles, confidence };
  }

  /**
   * Extract straight segments
   */
  private static extractStraightSegments(
    centerline: Vector3[]
  ): { lengths: number[]; confidence: number } {
    const lengths: number[] = [];
    let confidence = 0;

    if (centerline.length < 2) {
      return { lengths, confidence: 0 };
    }

    // Calculate segment lengths
    for (let i = 0; i < centerline.length - 1; i++) {
      const length = Vector.distance(centerline[i], centerline[i + 1]);
      if (length > 0.01) {
        // Filter out very small segments (noise)
        lengths.push(length);
      }
    }

    // Confidence based on uniformity of segments
    if (lengths.length > 1) {
      const average = lengths.reduce((a, b) => a + b, 0) / lengths.length;
      const variance = lengths.reduce((sum, v) => sum + Math.pow(v - average, 2), 0) /
        lengths.length;
      const stdDev = Math.sqrt(variance);

      confidence = Math.max(0, 1 - stdDev / average);
    } else {
      confidence = 0.5;
    }

    return { lengths, confidence };
  }

  /**
   * Estimate diameter from centerline
   * (In practice, this would use 3D geometry cross-sections)
   */
  private static estimateDiameter(centerline: Vector3[]): { value: number; confidence: number } {
    // Default estimate - in real implementation, measure from 3D geometry
    return {
      value: 0.5,
      confidence: 0.3 // Low confidence without actual cross-section data
    };
  }

  /**
   * Suggest profile based on diameter
   */
  private static suggestProfile(diameter: number): string {
    if (diameter < 0.3) {
      return 'hydraulic-hb8';
    } else if (diameter < 0.6) {
      return 'manual-half-inch';
    } else if (diameter < 1.0) {
      return 'manual-three-quarter-inch';
    } else {
      return 'manual-one-inch';
    }
  }

  /**
   * Smooth centerline using Catmull-Rom
   */
  static smoothCenterline(centerline: Vector3[], subdivisions: number = 2): Vector3[] {
    if (centerline.length < 4) {
      return centerline;
    }

    const smoothed: Vector3[] = [];

    for (let i = 0; i < centerline.length - 1; i++) {
      const p0 = i > 0 ? centerline[i - 1] : centerline[i];
      const p1 = centerline[i];
      const p2 = centerline[i + 1];
      const p3 = i < centerline.length - 2 ? centerline[i + 2] : centerline[i + 1];

      smoothed.push(p1);

      for (let t = 1 / subdivisions; t < 1; t += 1 / subdivisions) {
        const point = this.catmullRom(p0, p1, p2, p3, t);
        smoothed.push(point);
      }
    }

    smoothed.push(centerline[centerline.length - 1]);
    return smoothed;
  }

  /**
   * Catmull-Rom interpolation
   */
  private static catmullRom(p0: Vector3, p1: Vector3, p2: Vector3, p3: Vector3, t: number): Vector3 {
    const t2 = t * t;
    const t3 = t2 * t;

    const v0 = (p2.x - p0.x) * 0.5;
    const v1 = (p3.x - p1.x) * 0.5;
    const x =
      p1.x + v0 * t + (3 * (p2.x - p1.x) - 2 * v0 - v1) * t2 +
      (2 * (p1.x - p2.x) + v0 + v1) * t3;

    const v0y = (p2.y - p0.y) * 0.5;
    const v1y = (p3.y - p1.y) * 0.5;
    const y =
      p1.y + v0y * t + (3 * (p2.y - p1.y) - 2 * v0y - v1y) * t2 +
      (2 * (p1.y - p2.y) + v0y + v1y) * t3;

    const v0z = (p2.z - p0.z) * 0.5;
    const v1z = (p3.z - p1.z) * 0.5;
    const z =
      p1.z + v0z * t + (3 * (p2.z - p1.z) - 2 * v0z - v1z) * t2 +
      (2 * (p1.z - p2.z) + v0z + v1z) * t3;

    return { x, y, z };
  }

  /**
   * Validate recognized parameters
   */
  static validateRecognition(result: RecognitionResult): { valid: boolean; errors: string[] } {
    const errors: string[] = [];

    if (result.total_confidence < 0.5) {
      errors.push(
        `Low recognition confidence: ${(result.total_confidence * 100).toFixed(1)}%`
      );
    }

    if (result.diameter < 0.1 || result.diameter > 10) {
      errors.push(
        `Diameter out of range: ${result.diameter.toFixed(3)}" (valid: 0.1-10")`
      );
    }

    for (const angle of result.bend_angles) {
      if (angle < 0.5 || angle > 179) {
        errors.push(`Bend angle out of range: ${angle.toFixed(1)}°`);
      }
    }

    return {
      valid: errors.length === 0,
      errors
    };
  }
}
