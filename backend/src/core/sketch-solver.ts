/**
 * BendCore Sketch Solver
 * Newton-Raphson constraint solver for path sketches
 * Phase 35B Translation: Constraint solving for bending paths
 */

import {
  Vector3,
  Vertex,
  Segment,
  ConstraintDef,
  ConstraintType,
  SolveResult,
  ValidationResult
} from '../models/types';
import { Vector } from '../utils/vector';

export class SketchSolver {
  private sketchId: string = '';
  private vertices: Vertex[] = [];
  private initialVertices: Vertex[] = [];
  private segments: Segment[] = [];
  private constraints: ConstraintDef[] = [];
  private vertexIndex: Map<string, number> = new Map();

  // Profile constraints
  private profileR: number = 0;
  private profileSMin: number = 0;
  private profileBMin: number = 0;
  private profileBMax: number = 0;

  // Solver parameters
  private static readonly JACOBIAN_DELTA = 1e-8;
  private static readonly MIN_RESIDUAL = 1e-6;

  constructor() {}

  /**
   * Initialize solver with sketch data
   */
  initialize(
    sketchId: string,
    vertices: Vertex[],
    segments: Segment[],
    constraints: ConstraintDef[],
    profileR: number,
    profileSMin: number,
    profileBMin: number,
    profileBMax: number
  ): void {
    this.sketchId = sketchId;
    this.vertices = JSON.parse(JSON.stringify(vertices));
    this.initialVertices = JSON.parse(JSON.stringify(vertices));
    this.segments = segments;
    this.constraints = JSON.parse(JSON.stringify(constraints));

    this.profileR = profileR;
    this.profileSMin = profileSMin;
    this.profileBMin = profileBMin;
    this.profileBMax = profileBMax;

    // Build vertex index
    this.vertexIndex.clear();
    for (let i = 0; i < this.vertices.length; i++) {
      this.vertexIndex.set(this.vertices[i].id, i);
    }
  }

  /**
   * Solve constraints using Newton-Raphson
   */
  solve(maxIterations: number = 100, tolerance: number = 1e-6): SolveResult {
    const result: SolveResult = {
      converged: false,
      iterations: 0,
      residual: 0,
      conflicts: [],
      over_constrained: [],
      vertices: this.vertices,
      constraints: this.constraints
    };

    for (let iter = 0; iter < maxIterations; iter++) {
      result.iterations = iter + 1;

      // Compute residuals
      const residuals = this.computeResiduals();
      result.residual = this.vectorNorm(residuals);

      // Check convergence
      if (result.residual < tolerance) {
        result.converged = true;
        break;
      }

      // Compute gradient
      const gradient = this.computeGradient();

      // Check for singular matrix
      const gradientNorm = this.vectorNorm(gradient);
      if (gradientNorm < 1e-10) {
        result.conflicts = this.detectConflicts();
        result.over_constrained = this.detectOverConstrained();
        break;
      }

      // Update vertices using gradient descent
      this.updateVertices(gradient, 0.1);
    }

    // Final validation
    result.conflicts = this.detectConflicts();
    result.over_constrained = this.detectOverConstrained();

    return result;
  }

  /**
   * Validate path against profile constraints
   */
  validatePath(): ValidationResult {
    const errors: string[] = [];
    const warnings: string[] = [];

    // Check minimum straight length
    for (let i = 0; i < this.segments.length; i++) {
      if (this.segments[i].dimension_S < this.profileSMin) {
        errors.push(
          `Segment ${i}: straight length ${this.segments[i].dimension_S.toFixed(2)} < minimum ${this.profileSMin}`
        );
      }
    }

    // Check bend angles
    for (let i = 0; i < this.segments.length; i++) {
      const angle = this.segments[i].bend_angle;
      if (angle < this.profileBMin) {
        errors.push(
          `Segment ${i}: bend angle ${angle.toFixed(1)}° < minimum ${this.profileBMin}°`
        );
      }
      if (angle > this.profileBMax) {
        errors.push(
          `Segment ${i}: bend angle ${angle.toFixed(1)}° > maximum ${this.profileBMax}°`
        );
      }
    }

    // Check for arc overlap
    for (let i = 0; i < this.segments.length - 1; i++) {
      const arcLen1 = this.segments[i].bend_angle * (Math.PI / 180) * this.profileR;
      const arcLen2 = this.segments[i + 1].bend_angle * (Math.PI / 180) * this.profileR;
      const straightLen = this.segments[i].dimension_S;

      if (arcLen1 + arcLen2 > straightLen) {
        warnings.push(
          `Segment ${i}: arc overlap detected (${(arcLen1 + arcLen2).toFixed(2)} > ${straightLen.toFixed(2)})`
        );
      }
    }

    return {
      valid: errors.length === 0,
      errors,
      warnings
    };
  }

  /**
   * Get solved vertices
   */
  getVertices(): Vertex[] {
    return this.vertices;
  }

  /**
   * Get constraints
   */
  getConstraints(): ConstraintDef[] {
    return this.constraints;
  }

  /**
   * Reset to initial state
   */
  reset(): void {
    this.vertices = JSON.parse(JSON.stringify(this.initialVertices));
    this.vertexIndex.clear();
    for (let i = 0; i < this.vertices.length; i++) {
      this.vertexIndex.set(this.vertices[i].id, i);
    }
  }

  /**
   * Convert to JSON
   */
  toJson(): Record<string, unknown> {
    return {
      sketch_id: this.sketchId,
      vertices: this.vertices,
      segments: this.segments,
      constraints: this.constraints
    };
  }

  /**
   * Evaluate a single constraint
   */
  private evaluateConstraint(constraint: ConstraintDef): number {
    switch (constraint.type) {
      case 'COINCIDENT':
        return this.constraintCoincident(constraint.entity_ids[0], constraint.entity_ids[1]);
      case 'PARALLEL':
        return this.constraintParallel(constraint.entity_ids[0], constraint.entity_ids[1]);
      case 'PERPENDICULAR':
        return this.constraintPerpendicular(constraint.entity_ids[0], constraint.entity_ids[1]);
      case 'EQUALITY':
        return this.constraintEquality(constraint.entity_ids[0], constraint.entity_ids[1]);
      case 'FIXED':
        return this.constraintFixed(constraint.entity_ids[0]);
      case 'DISTANCE':
        return this.constraintDistance(
          constraint.entity_ids[0],
          constraint.entity_ids[1],
          constraint.value
        );
      case 'ANGLE':
        return this.constraintAngle(
          constraint.entity_ids[0],
          constraint.entity_ids[1],
          constraint.value
        );
      default:
        return 0;
    }
  }

  /**
   * Constraint: two vertices at same location
   */
  private constraintCoincident(v1Id: string, v2Id: string): number {
    const v1 = this.getVertexById(v1Id);
    const v2 = this.getVertexById(v2Id);
    if (!v1 || !v2) return 0;
    return Vector.distance(v1.position, v2.position);
  }

  /**
   * Constraint: two segments parallel
   */
  private constraintParallel(s1Id: string, s2Id: string): number {
    const d1 = this.getSegmentDirection(s1Id);
    const d2 = this.getSegmentDirection(s2Id);
    if (!d1 || !d2) return 0;
    return Math.abs(Vector.dot(d1, d2)) - 1; // Should be ±1 for parallel
  }

  /**
   * Constraint: two segments perpendicular
   */
  private constraintPerpendicular(s1Id: string, s2Id: string): number {
    const d1 = this.getSegmentDirection(s1Id);
    const d2 = this.getSegmentDirection(s2Id);
    if (!d1 || !d2) return 0;
    return Vector.dot(d1, d2); // Should be 0 for perpendicular
  }

  /**
   * Constraint: two dimensions equal
   */
  private constraintEquality(e1Id: string, e2Id: string): number {
    const s1 = this.getSegmentById(e1Id);
    const s2 = this.getSegmentById(e2Id);
    if (!s1 || !s2) return 0;
    return s1.dimension_L - s2.dimension_L;
  }

  /**
   * Constraint: vertex position fixed
   */
  private constraintFixed(vId: string): number {
    const v = this.getVertexById(vId);
    const initial = this.initialVertices.find(vi => vi.id === vId);
    if (!v || !initial) return 0;
    return Vector.distance(v.position, initial.position);
  }

  /**
   * Constraint: distance between vertices
   */
  private constraintDistance(v1Id: string, v2Id: string, target: number): number {
    const v1 = this.getVertexById(v1Id);
    const v2 = this.getVertexById(v2Id);
    if (!v1 || !v2) return 0;
    return Vector.distance(v1.position, v2.position) - target;
  }

  /**
   * Constraint: angle between segments
   */
  private constraintAngle(s1Id: string, s2Id: string, target: number): number {
    const d1 = this.getSegmentDirection(s1Id);
    const d2 = this.getSegmentDirection(s2Id);
    if (!d1 || !d2) return 0;
    const angle = Vector.angle(d1, d2);
    return Vector.toDegrees(angle) - target;
  }

  /**
   * Get segment direction vector
   */
  private getSegmentDirection(segmentId: string): Vector3 | null {
    const seg = this.getSegmentById(segmentId);
    if (!seg) return null;
    const from = this.getVertexById(seg.from_vertex);
    const to = this.getVertexById(seg.to_vertex);
    if (!from || !to) return null;
    const dir = Vector.subtract(to.position, from.position);
    return Vector.normalize(dir);
  }

  /**
   * Get segment by ID
   */
  private getSegmentById(id: string): Segment | null {
    return this.segments.find(s => s.id === id) || null;
  }

  /**
   * Get vertex by ID
   */
  private getVertexById(id: string): Vertex | null {
    const idx = this.vertexIndex.get(id);
    return idx !== undefined ? this.vertices[idx] : null;
  }

  /**
   * Compute residuals for all constraints
   */
  private computeResiduals(): number[] {
    return this.constraints.map((c, idx) => {
      const residual = this.evaluateConstraint(c);
      c.error = Math.abs(residual);
      c.is_satisfied = Math.abs(residual) < 1e-4;
      return residual;
    });
  }

  /**
   * Compute gradient (negative residuals as direction)
   */
  private computeGradient(): Vector3[] {
    const gradient: Vector3[] = [];

    for (const vertex of this.vertices) {
      let grad = Vector.create(0, 0, 0);

      for (const constraint of this.constraints) {
        const r0 = this.evaluateConstraint(constraint);

        // Perturb vertex in x direction
        const origX = vertex.position.x;
        vertex.position.x += SketchSolver.JACOBIAN_DELTA;
        const rx = this.evaluateConstraint(constraint);
        vertex.position.x = origX;

        // Perturb vertex in y direction
        const origY = vertex.position.y;
        vertex.position.y += SketchSolver.JACOBIAN_DELTA;
        const ry = this.evaluateConstraint(constraint);
        vertex.position.y = origY;

        // Perturb vertex in z direction
        const origZ = vertex.position.z;
        vertex.position.z += SketchSolver.JACOBIAN_DELTA;
        const rz = this.evaluateConstraint(constraint);
        vertex.position.z = origZ;

        const drdx = (rx - r0) / SketchSolver.JACOBIAN_DELTA;
        const drdy = (ry - r0) / SketchSolver.JACOBIAN_DELTA;
        const drdz = (rz - r0) / SketchSolver.JACOBIAN_DELTA;

        grad = Vector.add(grad, Vector.create(drdx, drdy, drdz));
      }

      gradient.push(grad);
    }

    return gradient;
  }

  /**
   * Update vertices based on gradient
   */
  private updateVertices(gradient: Vector3[], stepSize: number): void {
    for (let i = 0; i < this.vertices.length; i++) {
      if (!this.vertices[i].is_fixed) {
        const scaled = Vector.scale(gradient[i], -stepSize);
        this.vertices[i].position = Vector.add(this.vertices[i].position, scaled);
      }
    }
  }

  /**
   * Compute norm of vector array
   */
  private vectorNorm(vectors: Vector3[] | number[]): number {
    if (vectors.length === 0) return 0;
    if (typeof vectors[0] === 'number') {
      return Math.sqrt((vectors as number[]).reduce((sum, v) => sum + v * v, 0));
    }
    return Math.sqrt((vectors as Vector3[]).reduce((sum, v) => sum + v.x * v.x + v.y * v.y + v.z * v.z, 0));
  }

  /**
   * Detect conflicting constraints
   */
  private detectConflicts(): string[] {
    const conflicts: string[] = [];

    for (let i = 0; i < this.constraints.length; i++) {
      for (let j = i + 1; j < this.constraints.length; j++) {
        // Check for directly conflicting constraints
        const c1 = this.constraints[i];
        const c2 = this.constraints[j];

        // Same vertices with conflicting constraint types
        if (c1.entity_ids.some(id => c2.entity_ids.includes(id))) {
          if (
            (c1.type === 'FIXED' && c2.type === 'COINCIDENT') ||
            (c1.type === 'COINCIDENT' && c2.type === 'FIXED')
          ) {
            conflicts.push(
              `Constraint conflict between ${c1.id} and ${c2.id}`
            );
          }
        }
      }
    }

    return conflicts;
  }

  /**
   * Detect over-constrained sketch
   */
  private detectOverConstrained(): string[] {
    const overConstrained: string[] = [];

    // A sketch is over-constrained if:
    // 1. Number of constraints > 3*N - 3 (for 3D with N vertices)
    // 2. Or residuals cannot be driven to zero

    const expectedDOF = Math.max(0, 3 * this.vertices.length - 3);
    if (this.constraints.length > expectedDOF) {
      overConstrained.push(
        `Sketch is over-constrained: ${this.constraints.length} constraints for ${expectedDOF} degrees of freedom`
      );
    }

    return overConstrained;
  }
}
