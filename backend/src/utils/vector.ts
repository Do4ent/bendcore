/**
 * Vector 3D Operations Utility
 * Common vector math operations for geometry calculations
 */

import { Vector3 } from '../models/types';

export class Vector {
  /**
   * Create a new vector
   */
  static create(x: number = 0, y: number = 0, z: number = 0): Vector3 {
    return { x, y, z };
  }

  /**
   * Add two vectors
   */
  static add(a: Vector3, b: Vector3): Vector3 {
    return { x: a.x + b.x, y: a.y + b.y, z: a.z + b.z };
  }

  /**
   * Subtract two vectors
   */
  static subtract(a: Vector3, b: Vector3): Vector3 {
    return { x: a.x - b.x, y: a.y - b.y, z: a.z - b.z };
  }

  /**
   * Scale vector by scalar
   */
  static scale(v: Vector3, scalar: number): Vector3 {
    return { x: v.x * scalar, y: v.y * scalar, z: v.z * scalar };
  }

  /**
   * Dot product
   */
  static dot(a: Vector3, b: Vector3): number {
    return a.x * b.x + a.y * b.y + a.z * b.z;
  }

  /**
   * Cross product
   */
  static cross(a: Vector3, b: Vector3): Vector3 {
    return {
      x: a.y * b.z - a.z * b.y,
      y: a.z * b.x - a.x * b.z,
      z: a.x * b.y - a.y * b.x
    };
  }

  /**
   * Length/magnitude
   */
  static magnitude(v: Vector3): number {
    return Math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  }

  /** @deprecated use Vector.magnitude */
  static len(v: Vector3): number { return Vector.magnitude(v); }

  /**
   * Normalize vector (unit vector in same direction)
   */
  static normalize(v: Vector3): Vector3 {
    const len = this.magnitude(v);
    if (len < 1e-10) {
      return { x: 0, y: 0, z: 0 };
    }
    return { x: v.x / len, y: v.y / len, z: v.z / len };
  }

  /**
   * Distance between two points
   */
  static distance(a: Vector3, b: Vector3): number {
    return this.magnitude(this.subtract(a, b));
  }

  /**
   * Angle between two vectors (in radians)
   */
  static angle(a: Vector3, b: Vector3): number {
    const aDot = this.dot(a, b);
    const lenA = this.magnitude(a);
    const lenB = this.magnitude(b);

    if (lenA < 1e-10 || lenB < 1e-10) {
      return 0;
    }

    const cosAngle = aDot / (lenA * lenB);
    // Clamp to [-1, 1] to handle floating point errors
    const clamped = Math.max(-1, Math.min(1, cosAngle));
    return Math.acos(clamped);
  }

  /**
   * Linear interpolation between two vectors
   */
  static lerp(a: Vector3, b: Vector3, t: number): Vector3 {
    return {
      x: a.x + (b.x - a.x) * t,
      y: a.y + (b.y - a.y) * t,
      z: a.z + (b.z - a.z) * t
    };
  }

  /**
   * Project vector a onto vector b
   */
  static project(a: Vector3, b: Vector3): Vector3 {
    const bLen = this.magnitude(b);
    if (bLen < 1e-10) {
      return { x: 0, y: 0, z: 0 };
    }
    const scalar = this.dot(a, b) / (bLen * bLen);
    return this.scale(b, scalar);
  }

  /**
   * Check if two vectors are parallel (within tolerance)
   */
  static isParallel(a: Vector3, b: Vector3, tolerance: number = 1e-6): boolean {
    const cross = this.cross(a, b);
    return this.magnitude(cross) < tolerance;
  }

  /**
   * Check if two vectors are perpendicular (within tolerance)
   */
  static isPerpendicular(a: Vector3, b: Vector3, tolerance: number = 1e-6): boolean {
    return Math.abs(this.dot(a, b)) < tolerance;
  }

  /**
   * Convert degrees to radians
   */
  static toRadians(degrees: number): number {
    return (degrees * Math.PI) / 180;
  }

  /**
   * Convert radians to degrees
   */
  static toDegrees(radians: number): number {
    return (radians * 180) / Math.PI;
  }

  /**
   * Clamp value between min and max
   */
  static clamp(value: number, min: number, max: number): number {
    return Math.max(min, Math.min(max, value));
  }
}
