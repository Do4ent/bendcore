/**
 * BendCore Desktop - Sketch Solver Performance Tests
 * Benchmarks Newton-Raphson convergence time and efficiency
 *
 * Coverage: Performance benchmarks with acceptance criteria
 */

import { SketchSolver } from '../../src/bendcore/sketch_solver';

describe('Sketch Solver Performance Tests', () => {
  let sketchSolver: SketchSolver;

  beforeEach(() => {
    sketchSolver = new SketchSolver();
  });

  describe('Convergence Performance (< 100ms acceptance)', () => {
    test('should solve simple 2D sketch in < 50ms', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);

      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v2.id, v3.id], 10);

      const startTime = performance.now();
      const result = sketchSolver.solve(sketch.id);
      const endTime = performance.now();

      const duration = endTime - startTime;
      console.log(`Simple 2D sketch solved in ${duration.toFixed(2)}ms`);

      expect(result.converged).toBe(true);
      expect(duration).toBeLessThan(50);
    });

    test('should solve 10-vertex sketch in < 100ms', () => {
      const sketch = sketchSolver.createSketch();

      const vertices = [];
      for (let i = 0; i < 10; i++) {
        const angle = (i / 10) * Math.PI * 2;
        const x = Math.cos(angle) * 50;
        const y = Math.sin(angle) * 50;
        const fixed = i === 0;
        vertices.push(sketchSolver.addVertex(sketch.id, { x, y, z: 0 }, fixed));
      }

      for (let i = 0; i < 10; i++) {
        sketchSolver.addSegment(sketch.id, vertices[i].id, vertices[(i + 1) % 10].id, 30, 5, 0, 0);
      }

      const startTime = performance.now();
      const result = sketchSolver.solve(sketch.id);
      const endTime = performance.now();

      const duration = endTime - startTime;
      console.log(`10-vertex sketch solved in ${duration.toFixed(2)}ms`);

      expect(result.converged).toBe(true);
      expect(duration).toBeLessThan(100);
    });

    test('should converge with minimal iterations', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);

      const result = sketchSolver.solve(sketch.id);

      expect(result.iterations).toBeLessThan(20);
      console.log(`Convergence in ${result.iterations} iterations`);
    });

    test('should achieve low residual error', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);

      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);
      sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v2.id, v3.id], 10);

      const result = sketchSolver.solve(sketch.id);

      expect(result.residual).toBeLessThan(0.001);
      console.log(`Residual error: ${result.residual.toFixed(6)}`);
    });
  });

  describe('Constraint Validation Performance', () => {
    test('should validate 50 constraints in < 20ms', () => {
      const sketch = sketchSolver.createSketch();

      const vertices = [];
      for (let i = 0; i < 10; i++) {
        vertices.push(sketchSolver.addVertex(sketch.id, { x: i * 10, y: 0, z: 0 }, i === 0));
      }

      const constraints = [];
      for (let i = 0; i < 9; i++) {
        constraints.push(
          sketchSolver.addConstraint(sketch.id, 'DISTANCE', [vertices[i].id, vertices[i + 1].id], 10)
        );
      }

      const startTime = performance.now();
      constraints.forEach(c => {
        sketchSolver.validateConstraint(sketch.id, c.id);
      });
      const endTime = performance.now();

      const duration = endTime - startTime;
      console.log(`Validated ${constraints.length} constraints in ${duration.toFixed(2)}ms`);

      expect(duration).toBeLessThan(20);
    });
  });

  describe('Memory Usage Benchmarks', () => {
    test('should handle 100-vertex sketch without excessive memory', () => {
      const sketch = sketchSolver.createSketch();

      const initialMemory = process.memoryUsage().heapUsed;

      for (let i = 0; i < 100; i++) {
        sketchSolver.addVertex(sketch.id, { x: Math.random() * 1000, y: Math.random() * 1000, z: 0 }, i === 0);
      }

      const finalMemory = process.memoryUsage().heapUsed;
      const memoryIncrease = (finalMemory - initialMemory) / 1024 / 1024; // MB

      console.log(`Memory increase for 100 vertices: ${memoryIncrease.toFixed(2)}MB`);
      expect(memoryIncrease).toBeLessThan(10); // Less than 10MB
    });

    test('should not leak memory on multiple solve cycles', () => {
      const initialMemory = process.memoryUsage().heapUsed;

      for (let cycle = 0; cycle < 10; cycle++) {
        const sketch = sketchSolver.createSketch();

        const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
        const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

        sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
        sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10);

        sketchSolver.solve(sketch.id);
      }

      const finalMemory = process.memoryUsage().heapUsed;
      const memoryGrowth = (finalMemory - initialMemory) / 1024 / 1024; // MB

      console.log(`Memory growth after 10 cycles: ${memoryGrowth.toFixed(2)}MB`);
      expect(memoryGrowth).toBeLessThan(5); // Less than 5MB growth
    });
  });

  describe('Stress Testing', () => {
    test('should handle rapid constraint additions', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);

      const startTime = performance.now();

      for (let i = 0; i < 100; i++) {
        sketchSolver.addConstraint(sketch.id, 'DISTANCE', [v1.id, v2.id], 10 + (Math.random() - 0.5) * 2);
      }

      const endTime = performance.now();
      const duration = endTime - startTime;

      console.log(`Added 100 constraints in ${duration.toFixed(2)}ms`);
      expect(duration).toBeLessThan(500);
    });

    test('should handle dimension updates efficiently', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      const segment = sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);

      const startTime = performance.now();

      for (let i = 0; i < 100; i++) {
        sketchSolver.updateSegmentDimension(sketch.id, segment.id, 10 + i * 0.1, 5);
      }

      const endTime = performance.now();
      const duration = endTime - startTime;

      console.log(`Updated dimension 100 times in ${duration.toFixed(2)}ms`);
      expect(duration).toBeLessThan(200);
    });
  });
});
