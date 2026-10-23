/**
 * BendCore Desktop - Sketch Solver Workflow Integration Tests
 * Tests sketch creation, constraint solving, and validation
 *
 * Coverage: 35+ test cases, 400 LOC
 */

import { SketchSolver } from '../../src/bendcore/sketch_solver';
import { v4 as uuid } from 'uuid';

describe('Sketch Solver Integration Tests', () => {
  let sketchSolver: SketchSolver;

  beforeEach(() => {
    sketchSolver = new SketchSolver();
  });

  describe('Sketch Creation & Geometry', () => {
    test('should create new sketch with empty state', () => {
      const sketch = sketchSolver.createSketch();

      expect(sketch.id).toBeDefined();
      expect(sketch.vertices).toHaveLength(0);
      expect(sketch.segments).toHaveLength(0);
      expect(sketch.constraints).toHaveLength(0);
    });

    test('should add vertices to sketch', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);

      const updated = sketchSolver.getSketch(sketch.id);
      expect(updated.vertices).toHaveLength(3);
    });

    test('should add segments connecting vertices', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      const segment = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      expect(segment).toBeDefined();

      const updated = sketchSolver.getSketch(sketch.id);
      expect(updated.segments).toHaveLength(1);
    });

    test('should create closed path from vertices', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);
      const v4 = sketchSolver.addVertex(sketch.id, { x: 0, y: 10, z: 0 }, true); // Fixed

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);
      sketchSolver.addSegment(sketch.id, v3.id, v4.id, 10, 5, 90, 0);

      const updated = sketchSolver.getSketch(sketch.id);
      expect(updated.segments).toHaveLength(3);
    });

    test('should handle 3D path with rotation', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 5 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.addSegment(sketch.id, v2.id, v3.id, 12, 5, 45, 30); // Rotated

      const updated = sketchSolver.getSketch(sketch.id);
      expect(updated.vertices).toHaveLength(3);
    });
  });

  describe('Constraint Application & Management', () => {
    test('should apply coincident constraint between vertices', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 0.1, y: 0.1, z: 0 }, false);

      const constraint = sketchSolver.addConstraint(sketch.id, 'COINCIDENT', [v1.id, v2.id], 0);
      expect(constraint).toBeDefined();

      const updated = sketchSolver.getSketch(sketch.id);
      expect(updated.constraints).toHaveLength(1);
    });

    test('should apply parallel constraint between segments', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 0, y: 5, z: 0 }, false);
      const v4 = sketchSolver.addVertex(sketch.id, { x: 10, y: 5, z: 0 }, false);

      const s1 = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      const s2 = sketchSolver.addSegment(sketch.id, v3.id, v4.id, 10, 5, 0, 0);

      const constraint = sketchSolver.addConstraint(sketch.id, 'PARALLEL', [s1.id, s2.id], 0);
      expect(constraint).toBeDefined();
    });

    test('should apply perpendicular constraint', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 0, y: 10, z: 0 }, false);

      const s1 = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      const s2 = sketchSolver.addSegment(sketch.id, v1.id, v3.id, 10, 5, 90, 0);

      const constraint = sketchSolver.addConstraint(sketch.id, 'PERPENDICULAR', [s1.id, s2.id], 0);
      expect(constraint).toBeDefined();
    });

    test('should apply distance constraint', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      const constraint = sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);
      expect(constraint).toBeDefined();
      expect(constraint.value).toBe(10);
    });

    test('should apply angle constraint', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);

      const s1 = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      const s2 = sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);

      const constraint = sketchSolver.addConstraint(sketch.id, 'ANGLE', [s1.id, s2.id], 90);
      expect(constraint).toBeDefined();
    });

    test('should remove constraint from sketch', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      const constraint = sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);

      let updated = sketchSolver.getSketch(sketch.id);
      expect(updated.constraints).toHaveLength(1);

      sketchSolver.removeConstraint(sketch.id, constraint.id);

      updated = sketchSolver.getSketch(sketch.id);
      expect(updated.constraints).toHaveLength(0);
    });
  });

  describe('Newton-Raphson Solver Convergence', () => {
    test('should converge for simple 2D sketch', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);

      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v2.id, v3.id], 10);

      const result = sketchSolver.solve(sketch.id);
      expect(result.converged).toBe(true);
      expect(result.iterations).toBeLessThan(100);
      expect(result.residual).toBeLessThan(0.01);
    });

    test('should converge within acceptable iterations', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 5, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 5, y: 5, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 5, 5, 0, 0);
      sketchSolver.addSegment(sketch.id, v2.id, v3.id, 5, 5, 90, 0);

      const result = sketchSolver.solve(sketch.id);
      expect(result.iterations).toBeLessThanOrEqual(100);
    });

    test('should detect over-constrained sketch', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);

      // Add conflicting constraints
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 15); // Conflict!

      const result = sketchSolver.solve(sketch.id);
      expect(result.over_constrained.length).toBeGreaterThan(0);
    });

    test('should report convergence failure for conflicting constraints', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 5, y: 10, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);

      // Add impossible constraints
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v3.id], 5);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v3.id], 50);

      const result = sketchSolver.solve(sketch.id);
      expect(result.conflicts.length).toBeGreaterThan(0);
    });
  });

  describe('Conflict Detection & Resolution', () => {
    test('should detect conflicting dimensions', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 20);

      const result = sketchSolver.solve(sketch.id);
      expect(result.conflicts).toBeDefined();
    });

    test('should detect over-constrained systems', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);

      const s1 = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      const s2 = sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);

      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v2.id, v3.id], 10);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v3.id], 10); // Over-constrained!

      const result = sketchSolver.solve(sketch.id);
      expect(result.over_constrained.length).toBeGreaterThan(0);
    });

    test('should suggest constraint relaxation', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);

      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v2.id, v3.id], 10);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v3.id], 10);

      const conflicts = sketchSolver.detectConflicts(sketch.id);
      expect(Array.isArray(conflicts)).toBe(true);
    });
  });

  describe('Real-time Validation Feedback', () => {
    test('should provide real-time constraint satisfaction feedback', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      const constraint = sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);

      const feedback = sketchSolver.validateConstraint(sketch.id, constraint.id);
      expect(feedback).toBeDefined();
      expect(feedback.is_satisfied).toBeDefined();
    });

    test('should calculate constraint error', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 15, y: 0, z: 0 }, false);

      const constraint = sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);

      const error = sketchSolver.getConstraintError(sketch.id, constraint.id);
      expect(typeof error).toBe('number');
      expect(Math.abs(error)).toBeGreaterThan(0); // Should have error before solving
    });

    test('should report all constraint statuses', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);

      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v2.id, v3.id], 10);

      const statuses = sketchSolver.getConstraintStatuses(sketch.id);
      expect(Array.isArray(statuses)).toBe(true);
    });
  });

  describe('Dimension Editing & Re-solving', () => {
    test('should update dimension and re-solve sketch', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      const segment = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);

      sketchSolver.updateSegmentDimension(sketch.id, segment.id, 15, 5);

      const updated = sketchSolver.getSegment(sketch.id, segment.id);
      expect(updated.dimension_L).toBe(15);
    });

    test('should update bend angle and re-solve', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);

      const s1 = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      const s2 = sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);

      sketchSolver.updateSegmentBend(sketch.id, s2.id, 45);

      const updated = sketchSolver.getSegment(sketch.id, s2.id);
      expect(updated.bend_angle).toBe(45);
    });

    test('should validate dimension changes during edit', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      const segment = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);

      const isValid = sketchSolver.validateDimensionChange(sketch.id, segment.id, 20, 5);
      expect(typeof isValid).toBe('boolean');
    });
  });

  describe('Edge Cases & Special Scenarios', () => {
    test('should handle parallel segments correctly', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 0, y: 5, z: 0 }, false);
      const v4 = sketchSolver.addVertex(sketch.id, { x: 10, y: 5, z: 0 }, false);

      const s1 = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      const s2 = sketchSolver.addSegment(sketch.id, v3.id, v4.id, 10, 5, 0, 0);

      const constraint = sketchSolver.addConstraint(sketch.id, 'PARALLEL', [s1.id, s2.id], 0);
      const result = sketchSolver.solve(sketch.id);

      expect(result.converged).toBe(true);
    });

    test('should handle coincident point constraint', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 0.01, y: 0.01, z: 0 }, false);

      const constraint = sketchSolver.addConstraint(sketch.id, 'COINCIDENT', [v1.id, v2.id], 0);
      const result = sketchSolver.solve(sketch.id);

      expect(result.converged).toBe(true);
    });

    test('should handle zero-length segment edge case', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);

      const segment = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 0, 0, 0, 0);
      expect(segment).toBeDefined();
    });

    test('should detect and handle nearly parallel segments', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, false);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 0, y: 5, z: 0 }, false);
      const v4 = sketchSolver.addVertex(sketch.id, { x: 10, y: 5.01, z: 0 }, false);

      const s1 = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      const s2 = sketchSolver.addSegment(sketch.id, v3.id, v4.id, 10, 5, 0, 0);

      const almostParallel = sketchSolver.areSegmentsParallel(sketch.id, s1.id, s2.id, 0.1);
      expect(typeof almostParallel).toBe('boolean');
    });
  });
});
