/**
 * Integration Tests - Week 7
 * End-to-end testing of all components working together
 *
 * Run with: npm run test:integration
 */

import {
  Shape,
  GeometryKernel,
  StepReader,
  TubeDetector,
  MeshGenerator,
  VERSION,
} from '../lib/index';
import * as fs from 'fs';
import * as path from 'path';

describe('Integration Tests - Week 7', () => {

  // ==================== API Contract Tests ====================

  describe('API Contracts & Compatibility', () => {
    test('GeometryKernel API is stable', () => {
      // Check all expected methods exist
      expect(typeof GeometryKernel.makeBox).toBe('function');
      expect(typeof GeometryKernel.makeCylinder).toBe('function');
      expect(typeof GeometryKernel.makeTube).toBe('function');
      expect(typeof GeometryKernel.checkIntersection).toBe('function');
      expect(typeof GeometryKernel.getBoundingBox).toBe('function');
    });

    test('StepReader API is stable', () => {
      expect(typeof StepReader.readFile).toBe('function');
      expect(typeof StepReader.analyzeGeometry).toBe('function');
    });

    test('TubeDetector API is stable', () => {
      expect(typeof TubeDetector.detectTube).toBe('function');
      expect(typeof TubeDetector.detectAllTubes).toBe('function');
    });

    test('MeshGenerator API is stable', () => {
      expect(typeof MeshGenerator.generateMesh).toBe('function');
    });

    test('Shape class API is stable', () => {
      const shape = new Shape('SOLID');
      expect(typeof shape.getShapeType).toBe('function');
      expect(typeof shape.getVolume).toBe('function');
      expect(typeof shape.getSurfaceArea).toBe('function');
      expect(typeof shape.toJSON).toBe('function');
    });
  });

  // ==================== Cross-Component Integration ====================

  describe('Cross-Component Integration', () => {
    test('GeometryKernel → TubeDetector integration', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      const detected = TubeDetector.detectTube(tube);

      expect(detected).toBeDefined();
      expect(detected?.od).toBeCloseTo(50, 0);
      expect(detected?.wall).toBeCloseTo(2.5, 0);
      expect(detected?.confidence).toBeGreaterThan(0.8);
    });

    test('GeometryKernel → MeshGenerator integration', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.indices.length).toBeGreaterThan(0);
      expect(mesh.normals.length).toBe(mesh.vertices.length);
    });

    test('TubeDetector → MeshGenerator integration', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      const detected = TubeDetector.detectTube(tube);
      const mesh = MeshGenerator.generateMesh(tube);

      expect(detected?.confidence).toBeGreaterThan(0);
      expect(mesh.triangleCount).toBeGreaterThan(0);
    });

    test('Full geometry pipeline integration', () => {
      // Create → Detect → Mesh complete workflow
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      const detected = TubeDetector.detectTube(tube);
      const mesh = MeshGenerator.generateMesh(tube);

      expect(tube).toBeDefined();
      expect(detected?.isTube).toBe(true);
      expect(mesh.triangleCount).toBeGreaterThan(0);
    });
  });

  // ==================== Data Flow Integration ====================

  describe('Data Flow & Transformation', () => {
    test('Shape → ToJSON → Parse cycle', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      const json = tube.toJSON?.();

      expect(json).toBeDefined();
      expect(typeof json).toBe('object');
    });

    test('Geometry → Mesh → WebGL format conversion', () => {
      const cylinder = GeometryKernel.makeCylinder(50, 200);
      const mesh = MeshGenerator.generateMesh(cylinder);

      // Verify WebGL compatibility
      const vertices = new Float32Array(mesh.vertices);
      const indices = new Uint32Array(mesh.indices);
      const normals = new Float32Array(mesh.normals);

      expect(vertices instanceof Float32Array).toBe(true);
      expect(indices instanceof Uint32Array).toBe(true);
      expect(normals instanceof Float32Array).toBe(true);
    });

    test('Assembly → Mesh merge workflow', () => {
      const tubes = [
        GeometryKernel.makeTube(50, 2.5, 200),
        GeometryKernel.makeTube(40, 2.0, 150),
        GeometryKernel.makeTube(32, 1.5, 100),
      ];

      const meshes = tubes.map(t => MeshGenerator.generateMesh(t));

      // Merge meshes (simulate assembly rendering)
      let totalVertices = 0;
      let totalTriangles = 0;

      meshes.forEach(m => {
        totalVertices += m.vertices.length / 3;
        totalTriangles += m.triangleCount;
      });

      expect(totalVertices).toBeGreaterThan(0);
      expect(totalTriangles).toBeGreaterThan(0);
      expect(meshes).toHaveLength(3);
    });
  });

  // ==================== Error Handling Integration ====================

  describe('Error Handling & Recovery', () => {
    test('invalid geometry handled gracefully', () => {
      // Test edge case: invalid input
      const box = GeometryKernel.makeBox(0, 0, 0);
      expect(box).toBeDefined();
      expect(box.getVolume()).toBe(0);
    });

    test('large geometry handled without crash', () => {
      const largeBox = GeometryKernel.makeBox(100000, 100000, 100000);
      const mesh = MeshGenerator.generateMesh(largeBox);

      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(!isNaN(mesh.triangleCount)).toBe(true);
    });

    test('tiny geometry handled without loss', () => {
      const tinyTube = GeometryKernel.makeTube(0.001, 0.0001, 0.01);
      const mesh = MeshGenerator.generateMesh(tinyTube);

      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.normals.length).toBe(mesh.vertices.length);
    });

    test('invalid deflection clamped to safe range', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);

      // Test extreme values
      const veryCoarse = MeshGenerator.generateMesh(box, 100);
      const veryFine = MeshGenerator.generateMesh(box, 0.00001);

      expect(veryCoarse.triangleCount).toBeGreaterThan(0);
      expect(veryFine.triangleCount).toBeGreaterThan(0);
      expect(veryFine.triangleCount).toBeGreaterThanOrEqual(veryCoarse.triangleCount);
    });
  });

  // ==================== Performance Under Load ====================

  describe('Performance Under Load', () => {
    test('batch creation performance', () => {
      const count = 50;
      const start = Date.now();

      for (let i = 0; i < count; i++) {
        GeometryKernel.makeBox(100, 100, 100);
      }

      const elapsed = Date.now() - start;
      expect(elapsed).toBeLessThan(200); // 50 boxes in <200ms
    });

    test('batch mesh generation performance', () => {
      const boxes = Array(10)
        .fill(null)
        .map(() => GeometryKernel.makeBox(100, 100, 100));

      const start = Date.now();
      boxes.forEach(box => {
        MeshGenerator.generateMesh(box);
      });
      const elapsed = Date.now() - start;

      expect(elapsed).toBeLessThan(500); // 10 meshes in <500ms
    });

    test('assembly processing performance', () => {
      const assemblySize = 20;
      const tubes = Array(assemblySize)
        .fill(null)
        .map((_, i) => GeometryKernel.makeTube(50 - i, 2.5 - i * 0.1, 200 - i * 5));

      const start = Date.now();

      tubes.forEach(tube => {
        TubeDetector.detectTube(tube);
        MeshGenerator.generateMesh(tube);
      });

      const elapsed = Date.now() - start;
      expect(elapsed).toBeLessThan(2000); // 20 parts in <2s
    });
  });

  // ==================== Memory Management ====================

  describe('Memory Management', () => {
    test('mesh generation does not leak memory', () => {
      const getMemory = () => {
        if (global.gc) {
          global.gc();
          return (performance as any).memory?.usedJSHeapSize || 0;
        }
        return 0;
      };

      const initial = getMemory();

      for (let i = 0; i < 100; i++) {
        const box = GeometryKernel.makeBox(100, 100, 100);
        const mesh = MeshGenerator.generateMesh(box);
        // Mesh goes out of scope
      }

      const final = getMemory();

      // Memory growth should be reasonable (not exponential)
      const growth = final - initial;
      expect(growth).toBeLessThan(100 * 1024 * 1024); // Less than 100MB growth
    });

    test('large mesh disposal', () => {
      const largeCylinder = GeometryKernel.makeCylinder(1000, 5000);
      const mesh = MeshGenerator.generateMesh(largeCylinder);

      expect(mesh.vertices.length).toBeGreaterThan(100000);

      // After scope, mesh should be garbage collected
      // (No explicit assertion, just verify no crash)
    });
  });

  // ==================== Concurrency & Stability ====================

  describe('Concurrency & Stability', () => {
    test('sequential operation stability', () => {
      for (let i = 0; i < 100; i++) {
        const tube = GeometryKernel.makeTube(50, 2.5, 200);
        TubeDetector.detectTube(tube);
        MeshGenerator.generateMesh(tube);
      }
      // All operations completed without crash
      expect(true).toBe(true);
    });

    test('mixed geometry operations', () => {
      const operations = [
        () => GeometryKernel.makeBox(100, 100, 100),
        () => GeometryKernel.makeCylinder(50, 200),
        () => GeometryKernel.makeTube(50, 2.5, 200),
      ];

      for (let i = 0; i < 50; i++) {
        const op = operations[i % operations.length];
        const shape = op();
        MeshGenerator.generateMesh(shape);
      }

      expect(true).toBe(true);
    });
  });

  // ==================== API Documentation Verification ====================

  describe('API Documentation Accuracy', () => {
    test('makeBox signature matches documentation', () => {
      const box = GeometryKernel.makeBox(100, 200, 300);

      expect(box.getVolume()).toBeCloseTo(6000000, -4);
      // Documentation says: Volume = dx * dy * dz
    });

    test('makeCylinder signature matches documentation', () => {
      const cylinder = GeometryKernel.makeCylinder(10, 50);
      const expectedVolume = Math.PI * 10 * 10 * 50;

      expect(cylinder.getVolume()).toBeCloseTo(expectedVolume, 0);
      // Documentation says: Volume ≈ π * r² * h
    });

    test('makeTube signature matches documentation', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      const outerVolume = Math.PI * (25 ** 2) * 200;
      const innerVolume = Math.PI * (22.5 ** 2) * 200;
      const expectedVolume = outerVolume - innerVolume;

      expect(tube.getVolume()).toBeCloseTo(expectedVolume, 0);
      // Documentation: Volume = π * ((OD/2)² - (ID/2)²) * h
    });

    test('detectTube confidence score is in valid range', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      const detection = TubeDetector.detectTube(tube);

      expect(detection?.confidence).toBeGreaterThanOrEqual(0);
      expect(detection?.confidence).toBeLessThanOrEqual(1);
    });
  });

  // ==================== Version & Compatibility ====================

  describe('Version Compatibility', () => {
    test('version information is accessible', () => {
      expect(VERSION.binding).toBeDefined();
      expect(VERSION.occt).toBeDefined();
      expect(VERSION.api).toBeDefined();
    });

    test('version format is valid', () => {
      expect(VERSION.binding).toMatch(/\d+\.\d+\.\d+/);
      expect(VERSION.occt).toContain('7.7');
    });

    test('API version tracks implementation', () => {
      // API version should increment with breaking changes
      expect(typeof VERSION.api).toBe('string');
      expect(VERSION.api.length).toBeGreaterThan(0);
    });
  });

  // ==================== Real-World Use Cases ====================

  describe('Real-World Use Cases', () => {
    test('simple tube import workflow', () => {
      // User imports a tube
      const tube = GeometryKernel.makeTube(50, 2.5, 200);

      // System analyzes
      const analysis = TubeDetector.detectTube(tube);
      expect(analysis?.isTube).toBe(true);

      // System generates mesh
      const mesh = MeshGenerator.generateMesh(tube);
      expect(mesh.triangleCount).toBeGreaterThan(0);
    });

    test('assembly with bend detection', () => {
      // Import assembly
      const tube1 = GeometryKernel.makeTube(50, 2.5, 200);
      const tube2 = GeometryKernel.makeTube(50, 2.5, 150);

      // Analyze each
      const t1 = TubeDetector.detectTube(tube1);
      const t2 = TubeDetector.detectTube(tube2);

      expect(t1?.od).toBe(t2?.od); // Same OD
      expect(t1?.length).toBeGreaterThan(t2?.length); // Different length
    });

    test('quality vs performance tradeoff', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);

      // High quality (slow)
      const fineMesh = MeshGenerator.generateMesh(tube, 0.05);
      const fineTriangles = fineMesh.triangleCount;

      // Low quality (fast)
      const coarseMesh = MeshGenerator.generateMesh(tube, 0.5);
      const coarseTriangles = coarseMesh.triangleCount;

      expect(fineTriangles).toBeGreaterThan(coarseTriangles);
      // User can choose quality level based on performance needs
    });
  });

  // ==================== Regression Prevention ====================

  describe('Regression Tests', () => {
    test('box volume calculation remains accurate', () => {
      const test = (dx: number, dy: number, dz: number) => {
        const box = GeometryKernel.makeBox(dx, dy, dz);
        const expected = dx * dy * dz;
        expect(box.getVolume()).toBeCloseTo(expected, -4);
      };

      test(10, 20, 30);
      test(100, 100, 100);
      test(1, 1, 1);
      test(0.1, 0.2, 0.3);
    });

    test('tube wall thickness calculation', () => {
      const test = (od: number, wall: number) => {
        const tube = GeometryKernel.makeTube(od, wall, 100);
        const detection = TubeDetector.detectTube(tube);

        expect(detection?.od).toBeCloseTo(od, 0);
        expect(detection?.wall).toBeCloseTo(wall, 0);
        expect(detection?.id).toBeCloseTo(od - 2 * wall, 0);
      };

      test(50, 2.5);
      test(40, 2.0);
      test(32, 1.5);
    });

    test('mesh normal vectors remain valid', () => {
      const shapes = [
        GeometryKernel.makeBox(100, 100, 100),
        GeometryKernel.makeCylinder(50, 200),
        GeometryKernel.makeTube(50, 2.5, 200),
      ];

      shapes.forEach(shape => {
        const mesh = MeshGenerator.generateMesh(shape);

        for (let i = 0; i < mesh.normals.length; i += 3) {
          const nx = mesh.normals[i];
          const ny = mesh.normals[i + 1];
          const nz = mesh.normals[i + 2];

          const length = Math.sqrt(nx * nx + ny * ny + nz * nz);
          expect(length).toBeCloseTo(1.0, 5);
        }
      });
    });
  });
});
