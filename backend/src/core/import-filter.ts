/**
 * BendCore Import Filter
 * Groups and filters imported geometry
 * Phase 35E: Geometry import and recognition
 */

import { Vector3, BoundingBox } from '../models/types';
import { Vector } from '../utils/vector';
import { GeometryOps } from './geometry-ops';

export interface ImportedTube {
  id: string;
  centerline: Vector3[];
  diameter: number;
  material?: string;
  profile?: string;
  confidence: number;
}

export interface TubeGroup {
  id: string;
  tubes: ImportedTube[];
  grouping_rule: string;
  contact_points: Vector3[];
}

export class ImportFilter {
  /**
   * Filter and group imported geometry
   */
  static filterGeometry(tubes: ImportedTube[]): TubeGroup[] {
    const groups: TubeGroup[] = [];

    // Try diameter-based grouping first
    const diameterGroups = this.groupByDiameter(tubes);
    groups.push(...diameterGroups);

    // Try material-based grouping
    const materialGroups = this.groupByMaterial(tubes);
    for (const group of materialGroups) {
      if (!groups.find(g => g.tubes.some(t => group.tubes.find(gt => gt.id === t.id)))) {
        groups.push(group);
      }
    }

    // Try contact-based grouping
    const contactGroups = this.groupByContact(tubes);
    for (const group of contactGroups) {
      if (!groups.find(g => g.tubes.some(t => group.tubes.find(gt => gt.id === t.id)))) {
        groups.push(group);
      }
    }

    // Mark isolated components
    const groupedIds = new Set(groups.flatMap(g => g.tubes.map(t => t.id)));
    for (const tube of tubes) {
      if (!groupedIds.has(tube.id)) {
        groups.push({
          id: `isolated_${tube.id}`,
          tubes: [tube],
          grouping_rule: 'isolated',
          contact_points: []
        });
      }
    }

    return groups;
  }

  /**
   * Group tubes by diameter
   */
  private static groupByDiameter(tubes: ImportedTube[]): TubeGroup[] {
    const diameterMap = new Map<string, ImportedTube[]>();
    const tolerance = 0.05; // ±0.05 inches

    for (const tube of tubes) {
      let found = false;

      for (const [key, group] of diameterMap.entries()) {
        if (group.length > 0 && Math.abs(tube.diameter - group[0].diameter) < tolerance) {
          group.push(tube);
          found = true;
          break;
        }
      }

      if (!found) {
        diameterMap.set(`d_${tube.diameter}`, [tube]);
      }
    }

    return Array.from(diameterMap.entries()).map(([key, tubelist]) => ({
      id: key,
      tubes: tubelist,
      grouping_rule: 'diameter',
      contact_points: this.findContactPoints(tubelist)
    }));
  }

  /**
   * Group tubes by material
   */
  private static groupByMaterial(tubes: ImportedTube[]): TubeGroup[] {
    const materialMap = new Map<string, ImportedTube[]>();

    for (const tube of tubes) {
      const material = tube.material || 'unknown';
      const group = materialMap.get(material) || [];
      group.push(tube);
      materialMap.set(material, group);
    }

    return Array.from(materialMap.entries()).map(([material, tubelist]) => ({
      id: `m_${material}`,
      tubes: tubelist,
      grouping_rule: 'material',
      contact_points: this.findContactPoints(tubelist)
    }));
  }

  /**
   * Group tubes by contact (AABB collision detection)
   */
  private static groupByContact(tubes: ImportedTube[]): TubeGroup[] {
    const groups: TubeGroup[] = [];
    const visited = new Set<string>();

    for (const tube of tubes) {
      if (visited.has(tube.id)) {
        continue;
      }

      const group = [tube];
      visited.add(tube.id);

      // Find all tubes that touch this one
      for (const otherTube of tubes) {
        if (otherTube.id === tube.id || visited.has(otherTube.id)) {
          continue;
        }

        const box1 = this.getCenterlineBoundingBox(tube.centerline, tube.diameter / 2);
        const box2 = this.getCenterlineBoundingBox(
          otherTube.centerline,
          otherTube.diameter / 2
        );

        if (GeometryOps.aabbCollision(box1, box2)) {
          group.push(otherTube);
          visited.add(otherTube.id);
        }
      }

      if (group.length > 1) {
        groups.push({
          id: `contact_${group[0].id}`,
          tubes: group,
          grouping_rule: 'contact',
          contact_points: this.findContactPoints(group)
        });
      }
    }

    return groups;
  }

  /**
   * Find contact points between tubes
   */
  private static findContactPoints(tubes: ImportedTube[]): Vector3[] {
    const contacts: Vector3[] = [];

    if (tubes.length < 2) {
      return contacts;
    }

    for (let i = 0; i < tubes.length; i++) {
      for (let j = i + 1; j < tubes.length; j++) {
        const tube1 = tubes[i];
        const tube2 = tubes[j];

        // Simple proximity check: closest points on centerlines
        let minDist = Infinity;
        let contactPoint: Vector3 | null = null;

        for (const p1 of tube1.centerline) {
          for (const p2 of tube2.centerline) {
            const dist = Vector.distance(p1, p2);
            const contactDist = tube1.diameter / 2 + tube2.diameter / 2;

            if (dist < contactDist && dist < minDist) {
              minDist = dist;
              contactPoint = Vector.lerp(p1, p2, 0.5);
            }
          }
        }

        if (contactPoint) {
          contacts.push(contactPoint);
        }
      }
    }

    return contacts;
  }

  /**
   * Get bounding box for a centerline with radius
   */
  private static getCenterlineBoundingBox(
    centerline: Vector3[],
    radius: number
  ): BoundingBox {
    const expanded = centerline.map(p => ({
      x: p.x + radius,
      y: p.y + radius,
      z: p.z + radius
    }));

    return GeometryOps.calculateBoundingBox(expanded);
  }

  /**
   * Detect isolated components
   */
  static detectIsolated(tubes: ImportedTube[]): ImportedTube[] {
    const isolated: ImportedTube[] = [];

    for (const tube of tubes) {
      let hasContact = false;

      for (const otherTube of tubes) {
        if (tube.id === otherTube.id) {
          continue;
        }

        const box1 = this.getCenterlineBoundingBox(tube.centerline, tube.diameter / 2);
        const box2 = this.getCenterlineBoundingBox(
          otherTube.centerline,
          otherTube.diameter / 2
        );

        if (GeometryOps.aabbCollision(box1, box2)) {
          hasContact = true;
          break;
        }
      }

      if (!hasContact) {
        isolated.push(tube);
      }
    }

    return isolated;
  }
}
