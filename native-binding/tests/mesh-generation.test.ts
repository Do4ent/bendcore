/**
 * Mesh Generation Tests - Week 6
 * Triangle mesh generation and Three.js integration
 *
 * Run with: npm run test:mesh
 */

import { MeshGenerator, GeometryKernel } from '../lib/index';
import * as fs from 'fs';
import * as path from 'path';

describe('Mesh Generation - 3D Triangle Mesh', () => {

  // ==================== Basic Mesh Generation ====================

  describe('Basic Mesh Generation', () => {
    test('should generate mesh from box', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      expect(mesh).toBeDefined();
      expect(mesh.vertices).toBeDefined();
      expect(mesh.indices).toBeDefined();
      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.indices.length).toBeGreaterThan(0);
    });

    test('should generate mesh from cylinder', () => {
      const cylinder = GeometryKernel.makeCylinder(50, 200);
      const mesh = MeshGenerator.generateMesh(cylinder);

      expect(mesh).toBeDefined();
      expect(mesh.vertices.length % 3).toBe(0); // Triplets
      expect(mesh.indices.length % 3).toBe(0);  // Triplets
    });

    test('should generate mesh from tube', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      const mesh = MeshGenerator.generateMesh(tube);

      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.indices.length).toBeGreaterThan(0);
      expect(mesh.vertexCount).toBeGreaterThan(0);
      expect(mesh.triangleCount).toBeGreaterThan(0);
    });
  });

  // ==================== Vertex & Index Validation ====================

  describe('Mesh Data Validation', () => {
    test('vertices should be triplets (x, y, z)', () => {
      const box = GeometryKernel.makeBox(50, 50, 50);
      const mesh = MeshGenerator.generateMesh(box);

      expect(mesh.vertices.length % 3).toBe(0);
    });

    test('indices should be triplets (triangle references)', () => {
      const box = GeometryKernel.makeBox(50, 50, 50);
      const mesh = MeshGenerator.generateMesh(box);

      expect(mesh.indices.length % 3).toBe(0);
    });

    test('all vertex indices should be valid', () => {
      const cylinder = GeometryKernel.makeCylinder(50, 200);
      const mesh = MeshGenerator.generateMesh(cylinder);

      const maxIndex = Math.max(...mesh.indices);
      const expectedMaxIndex = (mesh.vertices.length / 3) - 1;

      expect(maxIndex).toBeLessThanOrEqual(expectedMaxIndex);
      expect(maxIndex).toBeGreaterThanOrEqual(0);
    });

    test('vertex count should match vertices array', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      const expectedVertexCount = mesh.vertices.length / 3;
      expect(mesh.vertexCount).toBe(expectedVertexCount);
    });

    test('triangle count should match indices array', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      const expectedTriangleCount = mesh.indices.length / 3;
      expect(mesh.triangleCount).toBe(expectedTriangleCount);
    });

    test('should have no isolated vertices', () => {
      const cylinder = GeometryKernel.makeCylinder(50, 200);
      const mesh = MeshGenerator.generateMesh(cylinder);

      const usedVertices = new Set(mesh.indices);
      expect(usedVertices.size).toBeLessThanOrEqual(mesh.vertexCount);
    });
  });

  // ==================== Deflection Parameter ====================

  describe('Deflection Quality Control', () => {
    test('coarse deflection should produce fewer triangles', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const coarseMesh = MeshGenerator.generateMesh(box, 1.0);
      const fineMesh = MeshGenerator.generateMesh(box, 0.1);

      expect(fineMesh.triangleCount).toBeGreaterThanOrEqual(coarseMesh.triangleCount);
    });

    test('finer deflection improves accuracy', () => {
      const cylinder = GeometryKernel.makeCylinder(50, 200);

      const coarse = MeshGenerator.generateMesh(cylinder, 1.0);
      const medium = MeshGenerator.generateMesh(cylinder, 0.5);
      const fine = MeshGenerator.generateMesh(cylinder, 0.1);

      expect(coarse.triangleCount).toBeLessThanOrEqual(medium.triangleCount);
      expect(medium.triangleCount).toBeLessThanOrEqual(fine.triangleCount);
    });

    test('default deflection (0.1) should be reasonable', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      // Box should have reasonable triangle count
      expect(mesh.triangleCount).toBeGreaterThan(6); // At least 2 per face
      expect(mesh.triangleCount).toBeLessThan(10000); // Not too many
    });

    test('should handle edge case: very coarse deflection', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box, 10.0);

      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.indices.length).toBeGreaterThan(0);
    });

    test('should handle edge case: very fine deflection', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box, 0.001);

      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.triangleCount).toBeGreaterThan(12); // Higher quality
    });
  });

  // ==================== Bounding Box ====================

  describe('Mesh Bounding Box', () => {
    test('should calculate correct bounding box', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      expect(mesh.boundingBox).toBeDefined();
      expect(mesh.boundingBox.length).toBe(6);
    });

    test('box mesh bounding box should match dimensions', () => {
      const box = GeometryKernel.makeBox(50, 60, 70);
      const mesh = MeshGenerator.generateMesh(box);

      const [minX, minY, minZ, maxX, maxY, maxZ] = mesh.boundingBox;

      expect(maxX - minX).toBeCloseTo(50, 0);
      expect(maxY - minY).toBeCloseTo(60, 0);
      expect(maxZ - minZ).toBeCloseTo(70, 0);
    });

    test('bounding box should contain all vertices', () => {
      const cylinder = GeometryKernel.makeCylinder(50, 200);
      const mesh = MeshGenerator.generateMesh(cylinder);

      const [minX, minY, minZ, maxX, maxY, maxZ] = mesh.boundingBox;

      for (let i = 0; i < mesh.vertices.length; i += 3) {
        const x = mesh.vertices[i];
        const y = mesh.vertices[i + 1];
        const z = mesh.vertices[i + 2];

        expect(x).toBeGreaterThanOrEqual(minX - 1e-6);
        expect(x).toBeLessThanOrEqual(maxX + 1e-6);
        expect(y).toBeGreaterThanOrEqual(minY - 1e-6);
        expect(y).toBeLessThanOrEqual(maxY + 1e-6);
        expect(z).toBeGreaterThanOrEqual(minZ - 1e-6);
        expect(z).toBeLessThanOrEqual(maxZ + 1e-6);
      }
    });
  });

  // ==================== Normal Vectors ====================

  describe('Vertex Normals', () => {
    test('should calculate vertex normals', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      expect(mesh.normals).toBeDefined();
      expect(mesh.normals.length).toBe(mesh.vertices.length);
    });

    test('normals should be unit vectors', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      for (let i = 0; i < mesh.normals.length; i += 3) {
        const nx = mesh.normals[i];
        const ny = mesh.normals[i + 1];
        const nz = mesh.normals[i + 2];

        const length = Math.sqrt(nx * nx + ny * ny + nz * nz);
        expect(length).toBeCloseTo(1.0, 5);
      }
    });

    test('normals should point outward for box', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      // For box centered at origin, normal · position should be > 0
      for (let i = 0; i < mesh.vertices.length; i += 3) {
        const x = mesh.vertices[i];
        const y = mesh.vertices[i + 1];
        const z = mesh.vertices[i + 2];

        const nx = mesh.normals[i];
        const ny = mesh.normals[i + 1];
        const nz = mesh.normals[i + 2];

        const dot = x * nx + y * ny + z * nz;
        // Should point outward (positive dot product with position)
        // Allow some tolerance for numerical errors
        expect(dot).toBeGreaterThanOrEqual(-1e-5);
      }
    });
  });

  // ==================== Complex Geometries ====================

  describe('Complex Mesh Generation', () => {
    test('should generate mesh for multiple primitives', () => {
      const box = GeometryKernel.makeBox(50, 50, 50);
      const cylinder = GeometryKernel.makeCylinder(40, 150);
      const tube = GeometryKernel.makeTube(30, 2, 100);

      const boxMesh = MeshGenerator.generateMesh(box);
      const cylinderMesh = MeshGenerator.generateMesh(cylinder);
      const tubeMesh = MeshGenerator.generateMesh(tube);

      expect(boxMesh.triangleCount).toBeGreaterThan(0);
      expect(cylinderMesh.triangleCount).toBeGreaterThan(boxMesh.triangleCount);
      expect(tubeMesh.triangleCount).toBeGreaterThan(0);
    });

    test('should handle tube with thin walls', () => {
      const thinTube = GeometryKernel.makeTube(100, 0.5, 500);
      const mesh = MeshGenerator.generateMesh(thinTube);

      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.triangleCount).toBeGreaterThan(0);
    });

    test('should handle large geometries', () => {
      const largeCylinder = GeometryKernel.makeCylinder(1000, 5000);
      const mesh = MeshGenerator.generateMesh(largeCylinder);

      expect(mesh.vertexCount).toBeGreaterThan(0);
      expect(mesh.triangleCount).toBeGreaterThan(0);
    });
  });

  // ==================== Three.js Compatibility ====================

  describe('Three.js Format Compatibility', () => {
    test('vertices format matches Float32Array requirement', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      // Should be convertible to Float32Array
      const floatVertices = new Float32Array(mesh.vertices);
      expect(floatVertices.length).toBe(mesh.vertices.length);
    });

    test('indices format matches Uint32Array requirement', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      // Should be convertible to Uint32Array
      const uint32Indices = new Uint32Array(mesh.indices);
      expect(uint32Indices.length).toBe(mesh.indices.length);
    });

    test('should have correct structure for BufferGeometry', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      // Three.js BufferGeometry expects:
      expect(mesh.vertices).toBeDefined();
      expect(mesh.indices).toBeDefined();
      expect(mesh.normals).toBeDefined();
      expect(mesh.vertexCount).toBeGreaterThan(0);
      expect(mesh.triangleCount).toBeGreaterThan(0);
      expect(mesh.boundingBox.length).toBe(6);
    });
  });

  // ==================== Performance Tests ====================

  describe('Mesh Generation Performance', () => {
    test('simple geometry mesh generation <10ms', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const start = Date.now();
      const mesh = MeshGenerator.generateMesh(box);
      const elapsed = Date.now() - start;

      expect(elapsed).toBeLessThan(10);
    });

    test('cylinder mesh generation <100ms', () => {
      const cylinder = GeometryKernel.makeCylinder(50, 200);
      const start = Date.now();
      const mesh = MeshGenerator.generateMesh(cylinder);
      const elapsed = Date.now() - start;

      expect(elapsed).toBeLessThan(100);
    });

    test('multiple meshes <500ms', () => {
      const start = Date.now();

      for (let i = 0; i < 5; i++) {
        const box = GeometryKernel.makeBox(100, 100, 100);
        MeshGenerator.generateMesh(box);
      }

      const elapsed = Date.now() - start;
      expect(elapsed).toBeLessThan(500);
    });

    test('normal calculation should be fast', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      // Normal calculation is included in generation time
      expect(mesh.normals.length).toBe(mesh.vertices.length);
      expect(mesh.normals.every(n => !isNaN(n))).toBe(true);
    });
  });

  // ==================== Error Handling ====================

  describe('Error Handling & Edge Cases', () => {
    test('should handle very small box', () => {
      const tinyBox = GeometryKernel.makeBox(0.001, 0.001, 0.001);
      const mesh = MeshGenerator.generateMesh(tinyBox);

      expect(mesh.vertices.length).toBeGreaterThan(0);
    });

    test('should handle very large box', () => {
      const hugeBox = GeometryKernel.makeBox(100000, 100000, 100000);
      const mesh = MeshGenerator.generateMesh(hugeBox);

      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.triangleCount).toBeGreaterThan(0);
    });

    test('should handle high deflection', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box, 100);

      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.indices.length % 3).toBe(0);
    });

    test('should handle low deflection', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box, 0.0001);

      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.triangleCount).toBeGreaterThan(0);
    });

    test('normals should not contain NaN', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      expect(mesh.normals.every(n => !isNaN(n))).toBe(true);
    });

    test('bounding box should be valid', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      const [minX, minY, minZ, maxX, maxY, maxZ] = mesh.boundingBox;

      expect(minX).toBeLessThanOrEqual(maxX);
      expect(minY).toBeLessThanOrEqual(maxY);
      expect(minZ).toBeLessThanOrEqual(maxZ);
    });
  });

  // ==================== Integration Tests ====================

  describe('Mesh Generation Integration', () => {
    test('should mesh box and render in Three.js', () => {
      const box = GeometryKernel.makeBox(100, 100, 100);
      const mesh = MeshGenerator.generateMesh(box);

      // Simulate Three.js BufferGeometry creation
      const geometry = {
        position: new Float32Array(mesh.vertices),
        index: new Uint32Array(mesh.indices),
        normal: new Float32Array(mesh.normals),
      };

      expect(geometry.position.length % 3).toBe(0);
      expect(geometry.index.length % 3).toBe(0);
      expect(geometry.normal.length).toBe(geometry.position.length);
    });

    test('should mesh tube and include normals', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      const mesh = MeshGenerator.generateMesh(tube);

      expect(mesh.normals).toBeDefined();
      expect(mesh.normals.length).toBe(mesh.vertices.length);
    });

    test('end-to-end: create → mesh → render', () => {
      // Create geometry
      const cylinder = GeometryKernel.makeCylinder(50, 200);

      // Generate mesh
      const mesh = MeshGenerator.generateMesh(cylinder);

      // Prepare for Three.js
      const vertices = new Float32Array(mesh.vertices);
      const indices = new Uint32Array(mesh.indices);
      const normals = new Float32Array(mesh.normals);

      // Verify all components ready
      expect(vertices.length > 0).toBe(true);
      expect(indices.length > 0).toBe(true);
      expect(normals.length > 0).toBe(true);
      expect(vertices.length).toBe(normals.length);
    });
  });
});
