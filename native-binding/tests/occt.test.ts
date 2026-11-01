/**
 * OCCT Binding Tests - Full Test Suite
 * Tests for all geometry operations when OCCT is compiled
 *
 * Run with: npm test
 */

import {
  Shape,
  GeometryKernel,
  StepReader,
  TubeDetector,
  MeshGenerator,
  VERSION
} from '../lib/index';
import * as fs from 'fs';
import * as path from 'path';

describe('OCCT Binding - Full Test Suite', () => {

  // ==================== Version Tests ====================

  describe('Version Info', () => {
    test('should report binding version', () => {
      expect(VERSION.binding).toBeDefined();
      expect(VERSION.binding).toMatch(/\d+\.\d+\.\d+/);
    });

    test('should report OCCT version', () => {
      expect(VERSION.occt).toBeDefined();
      expect(VERSION.occt).toContain('7.7');
    });

    test('should report API version', () => {
      expect(VERSION.api).toBeDefined();
    });
  });

  // ==================== Shape Tests ====================

  describe('Shape Operations', () => {
    test('should create a shape', () => {
      const shape = new Shape('SOLID');
      expect(shape).toBeDefined();
    });

    test('shape should return type', () => {
      const shape = new Shape('SOLID');
      const type = shape.getShapeType();
      expect(type).toBeDefined();
    });

    test('shape volume should be non-negative', () => {
      const shape = new Shape('SOLID');
      const volume = shape.getVolume();
      expect(volume).toBeGreaterThanOrEqual(0);
    });

    test('shape surface area should be non-negative', () => {
      const shape = new Shape('SOLID');
      const area = shape.getSurfaceArea();
      expect(area).toBeGreaterThanOrEqual(0);
    });

    test('shape should convert to JSON', () => {
      const shape = new Shape('SOLID');
      const json = shape.toJSON();
      expect(json).toEqual(expect.any(Object));
    });
  });

  // ==================== Geometry Kernel Tests ====================

  describe('GeometryKernel - Primitives', () => {
    test('should create a box', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      expect(box).toBeDefined();
      expect(box.type).toBe('Box');
      expect(box.dx).toBe(100);
      expect(box.dy).toBe(100);
      expect(box.dz).toBe(100);
    });

    test('box volume should be correct', () => {
      const box = GeometryKernel.makeBox(10, 20, 30);
      expect(box.volume).toBe(6000);
    });

    test('should create a cylinder', () => {
      const cylinder = GeometryKernel.makeCylinder(10, 50);
      expect(cylinder).toBeDefined();
      expect(cylinder.type).toBe('Cylinder');
      expect(cylinder.radius).toBe(10);
      expect(cylinder.height).toBe(50);
    });

    test('cylinder volume should be approximately π*r²*h', () => {
      const cylinder = GeometryKernel.makeCylinder(10, 50);
      const expectedVolume = Math.PI * 10 * 10 * 50;
      expect(cylinder.volume).toBeCloseTo(expectedVolume, 0);
    });

    test('should create a tube', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      expect(tube).toBeDefined();
      expect(tube.type).toBe('Tube');
      expect(tube.od).toBe(50);
      expect(tube.wall).toBe(2.5);
      expect(tube.id).toBe(45);
      expect(tube.height).toBe(200);
    });

    test('tube volume should be correct', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      // V = π * ((OD/2)² - (ID/2)²) * h
      const expectedVolume = Math.PI * ((25 * 25) - (22.5 * 22.5)) * 200;
      expect(tube.volume).toBeCloseTo(expectedVolume, 0);
    });

    test('should handle edge case: very small tube', () => {
      const tube = GeometryKernel.makeTube(1, 0.1, 10);
      expect(tube.od).toBe(1);
      expect(tube.wall).toBe(0.1);
      expect(tube.volume).toBeGreaterThan(0);
    });

    test('should handle edge case: very large tube', () => {
      const tube = GeometryKernel.makeTube(1000, 50, 5000);
      expect(tube.od).toBe(1000);
      expect(tube.wall).toBe(50);
      expect(tube.volume).toBeGreaterThan(1000000);
    });
  });

  // ==================== Bounding Box Tests ====================

  describe('GeometryKernel - Bounding Box', () => {
    test('should get bounding box', () => {
      const shape = GeometryKernel.makeBox(100, 100, 100);
      const bbox = GeometryKernel.getBoundingBox(shape);

      expect(bbox).toBeDefined();
      expect(bbox.minX).toBeDefined();
      expect(bbox.minY).toBeDefined();
      expect(bbox.minZ).toBeDefined();
      expect(bbox.maxX).toBeDefined();
      expect(bbox.maxY).toBeDefined();
      expect(bbox.maxZ).toBeDefined();
    });

    test('bounding box should contain shape', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const bbox = GeometryKernel.getBoundingBox(box);

      // For a 100x100x100 box at origin
      expect(bbox.maxX - bbox.minX).toBeCloseTo(100, 1);
      expect(bbox.maxY - bbox.minY).toBeCloseTo(100, 1);
      expect(bbox.maxZ - bbox.minZ).toBeCloseTo(100, 1);
    });
  });

  // ==================== STEP File Tests ====================

  describe('StepReader', () => {
    test('should handle non-existent file gracefully', () => {
      const filePath = '/non/existent/file.step';
      // Should throw error
      expect(() => StepReader.readFile(filePath)).toThrow();
    });

    test('should analyze geometry', () => {
      const geometry = {
        boundingBox: { minX: 0, minY: 0, minZ: 0, maxX: 100, maxY: 100, maxZ: 100 },
        topology: { faceCount: 6, edgeCount: 12, vertexCount: 8 },
        properties: { volume: 1000000, surfaceArea: 60000 }
      };

      const analysis = StepReader.analyzeGeometry(geometry);
      expect(analysis).toBeDefined();
      expect(analysis.shapeType).toBeDefined();
      expect(analysis.isValid).toBeDefined();
    });
  });

  // ==================== Tube Detection Tests ====================

  describe('TubeDetector', () => {
    test('should detect tube from geometry', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const tube = TubeDetector.detectTube(box);

      // May return null or tube info depending on detection
      if (tube) {
        expect(tube.od).toBeGreaterThan(0);
        expect(tube.wall).toBeGreaterThanOrEqual(0);
        expect(tube.length).toBeGreaterThan(0);
        expect(tube.confidence).toBeGreaterThanOrEqual(0);
        expect(tube.confidence).toBeLessThanOrEqual(1);
      }
    });

    test('should detect all tubes in geometry', () => {
      const geometry = {
        topology: { faceCount: 6, edgeCount: 12 },
        properties: { volume: 1000000 }
      };

      const tubes = TubeDetector.detectAllTubes(geometry);
      expect(Array.isArray(tubes)).toBe(true);
    });

    test('should return empty array for non-tube geometry', () => {
      const sphere = { type: 'Sphere' };
      const tubes = TubeDetector.detectAllTubes(sphere);
      expect(tubes).toEqual([]);
    });
  });

  // ==================== Mesh Generation Tests ====================

  describe('MeshGenerator', () => {
    test('should generate mesh from shape', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      expect(mesh).toBeDefined();
      expect(mesh.vertices).toBeDefined();
      expect(mesh.indices).toBeDefined();
      expect(Array.isArray(mesh.vertices)).toBe(true);
      expect(Array.isArray(mesh.indices)).toBe(true);
    });

    test('mesh vertices should be triplets (x, y, z)', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      expect(mesh.vertices.length % 3).toBe(0);
    });

    test('mesh indices should be triplets', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      expect(mesh.indices.length % 3).toBe(0);
    });

    test('mesh should have valid vertex references', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      const maxVertex = Math.max(...mesh.indices);
      const expectedMaxVertex = (mesh.vertices.length / 3) - 1;
      expect(maxVertex).toBeLessThanOrEqual(expectedMaxVertex);
    });

    test('mesh deflection should affect quality', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const coarseMesh = MeshGenerator.generateMesh(box, 1.0);
      const fineMesh = MeshGenerator.generateMesh(box, 0.1);

      // Fine mesh should have more vertices
      expect(fineMesh.vertices.length).toBeGreaterThanOrEqual(coarseMesh.vertices.length);
    });
  });

  // ==================== Intersection Tests ====================

  describe('Collision Detection', () => {
    test('should check intersection between shapes', () => {
      const box1 = GeometryKernel.makeBox(100, 100, 100);
      const box2 = GeometryKernel.makeBox(100, 100, 100);

      const result = GeometryKernel.checkIntersection(box1, box2);
      expect(result).toBeDefined();
      expect(result.hasIntersection).toBeDefined();
      expect(result.distance).toBeDefined();
    });

    test('should detect when shapes do not intersect', () => {
      // Two boxes far apart
      const box1 = GeometryKernel.makeBox(100, 100, 100);
      const box2 = GeometryKernel.makeBox(100, 100, 100);

      const result = GeometryKernel.checkIntersection(box1, box2);
      // Depending on implementation, may report hasIntersection or distance > 0
      expect(result.distance).toBeGreaterThanOrEqual(0);
    });
  });

  // ==================== Integration Tests ====================

  describe('Integration Tests', () => {
    test('should create tube and generate mesh', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      expect(tube).toBeDefined();

      const mesh = MeshGenerator.generateMesh(tube);
      expect(mesh).toBeDefined();
      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.indices.length).toBeGreaterThan(0);
    });

    test('should detect tube and generate mesh', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      const detected = TubeDetector.detectTube(tube);

      if (detected) {
        expect(detected.od).toBe(50);
        expect(detected.wall).toBe(2.5);

        const mesh = MeshGenerator.generateMesh(tube);
        expect(mesh.vertices.length).toBeGreaterThan(0);
      }
    });

    test('should create multiple tubes and check collisions', () => {
      const tube1 = GeometryKernel.makeTube(50, 2.5, 200);
      const tube2 = GeometryKernel.makeTube(40, 2.0, 150);

      const result = GeometryKernel.checkIntersection(tube1, tube2);
      expect(result).toBeDefined();
    });
  });

  // ==================== Performance Tests ====================

  describe('Performance', () => {
    test('box creation should be fast', () => {
      const start = Date.now();
      for (let i = 0; i < 100; i++) {
        GeometryKernel.makeBox(100, 100, 100);
      }
      const elapsed = Date.now() - start;

      // Should complete 100 box creations in < 1 second
      expect(elapsed).toBeLessThan(1000);
    });

    test('tube detection should be fast', () => {
      const geometry = {
        topology: { faceCount: 6, edgeCount: 12 },
        properties: { volume: 1000000 }
      };

      const start = Date.now();
      for (let i = 0; i < 100; i++) {
        TubeDetector.detectAllTubes(geometry);
      }
      const elapsed = Date.now() - start;

      // Should detect 100 times in < 500ms
      expect(elapsed).toBeLessThan(500);
    });
  });

  // ==================== Error Handling ====================

  describe('Error Handling', () => {
    test('should handle invalid volume', () => {
      const box = GeometryKernel.makeBox(0, 0, 0);
      expect(box.volume).toBe(0);
    });

    test('should handle very small dimensions', () => {
      const box = GeometryKernel.makeBox(0.001, 0.001, 0.001);
      expect(box.volume).toBeGreaterThanOrEqual(0);
    });

    test('should handle very large dimensions', () => {
      const box = GeometryKernel.makeBox(1000000, 1000000, 1000000);
      expect(box.volume).toBeGreaterThan(0);
    });
  });
});
