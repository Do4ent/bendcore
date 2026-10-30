/**
 * BendCore Geometry Operations
 * Geometric calculations: AABB, volumes, distances, collisions
 */

import { Vector3, BoundingBox, Cylinder, Torus } from '../models/types';
import { Vector } from '../utils/vector';

export class GeometryOps {
  /**
   * Calculate bounding box for a set of points
   */
  static calculateBoundingBox(points: Vector3[]): BoundingBox {
    if (points.length === 0) {
      return {
        min: Vector.create(0, 0, 0),
        max: Vector.create(0, 0, 0),
        center: Vector.create(0, 0, 0),
        size: Vector.create(0, 0, 0)
      };
    }

    let minX = points[0].x;
    let minY = points[0].y;
    let minZ = points[0].z;
    let maxX = points[0].x;
    let maxY = points[0].y;
    let maxZ = points[0].z;

    for (const p of points) {
      minX = Math.min(minX, p.x);
      minY = Math.min(minY, p.y);
      minZ = Math.min(minZ, p.z);
      maxX = Math.max(maxX, p.x);
      maxY = Math.max(maxY, p.y);
      maxZ = Math.max(maxZ, p.z);
    }

    const min = Vector.create(minX, minY, minZ);
    const max = Vector.create(maxX, maxY, maxZ);
    const center = Vector.scale(Vector.add(min, max), 0.5);
    const size = Vector.subtract(max, min);

    return { min, max, center, size };
  }

  /**
   * Check AABB (Axis-Aligned Bounding Box) collision
   */
  static aabbCollision(box1: BoundingBox, box2: BoundingBox): boolean {
    return (
      box1.min.x <= box2.max.x &&
      box1.max.x >= box2.min.x &&
      box1.min.y <= box2.max.y &&
      box1.max.y >= box2.min.y &&
      box1.min.z <= box2.max.z &&
      box1.max.z >= box2.min.z
    );
  }

  /**
   * Calculate cylinder bounding box
   */
  static cylinderBoundingBox(cylinder: Cylinder): BoundingBox {
    const start = cylinder.center;
    const end = Vector.add(
      cylinder.center,
      Vector.scale(cylinder.axis, cylinder.length)
    );

    const points = [start, end];

    // Add points at extremes due to radius
    const perpendicular = this.getPerpendicular(cylinder.axis);
    const radiusOffset = Vector.scale(perpendicular, cylinder.outer_radius);

    points.push(Vector.add(start, radiusOffset));
    points.push(Vector.subtract(start, radiusOffset));
    points.push(Vector.add(end, radiusOffset));
    points.push(Vector.subtract(end, radiusOffset));

    return this.calculateBoundingBox(points);
  }

  /**
   * Calculate torus bounding box
   */
  static torusBoundingBox(torus: Torus): BoundingBox {
    const maxRadius = torus.major_radius + torus.minor_radius;
    const points = [
      Vector.add(torus.center, Vector.create(maxRadius, 0, 0)),
      Vector.add(torus.center, Vector.create(-maxRadius, 0, 0)),
      Vector.add(torus.center, Vector.create(0, maxRadius, 0)),
      Vector.add(torus.center, Vector.create(0, -maxRadius, 0)),
      Vector.add(torus.center, Vector.create(0, 0, maxRadius)),
      Vector.add(torus.center, Vector.create(0, 0, -maxRadius))
    ];

    return this.calculateBoundingBox(points);
  }

  /**
   * Calculate surface area of cylinder
   */
  static cylinderSurfaceArea(cylinder: Cylinder): number {
    const roRadius = cylinder.outer_radius;
    const riRadius = cylinder.inner_radius;
    const length = cylinder.length;

    // Lateral surface area (both inner and outer)
    const lateralOuter = 2 * Math.PI * roRadius * length;
    const lateralInner = 2 * Math.PI * riRadius * length;

    // End cap area (ring)
    const endCapArea = 2 * Math.PI * (roRadius * roRadius - riRadius * riRadius);

    return lateralOuter + lateralInner + endCapArea;
  }

  /**
   * Calculate center of mass for cylinder
   */
  static cylinderCenterOfMass(cylinder: Cylinder): Vector3 {
    // For a uniform hollow cylinder, COM is at the geometric center
    return Vector.add(
      cylinder.center,
      Vector.scale(cylinder.axis, cylinder.length / 2)
    );
  }

  /**
   * Calculate center of mass for torus
   */
  static torusCenterOfMass(torus: Torus): Vector3 {
    // For a uniform torus, COM is at the geometric center
    return torus.center;
  }

  /**
   * Calculate distance between two line segments
   */
  static segmentSegmentDistance(
    p1: Vector3,
    p2: Vector3,
    p3: Vector3,
    p4: Vector3
  ): number {
    const d1 = Vector.subtract(p2, p1);
    const d2 = Vector.subtract(p4, p3);
    const d3 = Vector.subtract(p3, p1);

    const a = Vector.dot(d1, d1);
    const b = Vector.dot(d1, d2);
    const c = Vector.dot(d2, d2);
    const d = Vector.dot(d1, d3);
    const e = Vector.dot(d2, d3);

    const denom = a * c - b * b;

    let s = 0;
    let t = 0;

    if (denom !== 0) {
      s = (b * e - c * d) / denom;
      t = (a * e - b * d) / denom;
    }

    s = Math.max(0, Math.min(1, s));
    t = Math.max(0, Math.min(1, t));

    const closest1 = Vector.add(p1, Vector.scale(d1, s));
    const closest2 = Vector.add(p3, Vector.scale(d2, t));

    return Vector.distance(closest1, closest2);
  }

  /**
   * Get perpendicular vector
   */
  private static getPerpendicular(direction: Vector3): Vector3 {
    if (Math.abs(direction.x) < 0.9) {
      const perp = Vector.cross(direction, Vector.create(1, 0, 0));
      return Vector.normalize(perp);
    } else {
      const perp = Vector.cross(direction, Vector.create(0, 1, 0));
      return Vector.normalize(perp);
    }
  }

  /**
   * Check if point is inside cylinder
   */
  static pointInCylinder(point: Vector3, cylinder: Cylinder): boolean {
    const relative = Vector.subtract(point, cylinder.center);
    const proj = Vector.dot(relative, cylinder.axis);

    // Check if within height range
    if (proj < 0 || proj > cylinder.length) {
      return false;
    }

    // Check radial distance
    const projected = Vector.scale(cylinder.axis, proj);
    const radial = Vector.subtract(relative, projected);
    const distance = Vector.magnitude(radial);

    return distance <= cylinder.outer_radius && distance >= cylinder.inner_radius;
  }

  /**
   * Sphere-AABB collision
   */
  static sphereAABBCollision(
    sphereCenter: Vector3,
    sphereRadius: number,
    box: BoundingBox
  ): boolean {
    const closest = Vector.create(
      Vector.clamp(sphereCenter.x, box.min.x, box.max.x),
      Vector.clamp(sphereCenter.y, box.min.y, box.max.y),
      Vector.clamp(sphereCenter.z, box.min.z, box.max.z)
    );

    return Vector.distance(sphereCenter, closest) <= sphereRadius;
  }

  /**
   * Calculate total surface area and volume for a list of cylinders and tori
   */
  static calculateTotalProperties(
    cylinders: Cylinder[],
    tori: Torus[]
  ): { volume: number; surfaceArea: number } {
    let volume = 0;
    let surfaceArea = 0;

    for (const cyl of cylinders) {
      const outerArea = Math.PI * cyl.outer_radius * cyl.outer_radius;
      const innerArea = Math.PI * cyl.inner_radius * cyl.inner_radius;
      volume += (outerArea - innerArea) * cyl.length;
      surfaceArea += this.cylinderSurfaceArea(cyl);
    }

    for (const tor of tori) {
      const sweepAngle = tor.end_angle - tor.start_angle;
      const minorR = tor.minor_radius;
      const majorR = tor.major_radius;

      // Torus volume formula
      volume +=
        2 *
        Math.PI *
        minorR *
        minorR *
        majorR *
        (sweepAngle / (2 * Math.PI));

      // Approximate torus surface area
      surfaceArea +=
        4 *
        Math.PI *
        Math.PI *
        majorR *
        minorR *
        (sweepAngle / (2 * Math.PI));
    }

    return { volume, surfaceArea };
  }
}
