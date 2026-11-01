/**
 * End-to-End Pipeline Tests - Week 5-6 Integration
 * Complete workflow from STEP file to 3D visualization
 *
 * Run with: npm run test:e2e
 */

import {
  StepReader,
  GeometryKernel,
  TubeDetector,
  MeshGenerator,
  VERSION
} from '../lib/index';

describe('E2E: STEP → Tube Detection → Mesh → Visualization', () => {

  // ==================== Scenario 1: Simple Tube ====================

  describe('Scenario 1: Import and visualize simple tube', () => {
    test('complete workflow for tube', () => {
      // Step 1: Create tube geometry (simulating STEP import)
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      expect(tube).toBeDefined();

      // Step 2: Detect tube properties
      const tubeDetection = TubeDetector.detectTube(tube);
      expect(tubeDetection).toBeDefined();
      expect(tubeDetection?.od).toBeCloseTo(50, 0);
      expect(tubeDetection?.wall).toBeCloseTo(2.5, 0);
      expect(tubeDetection?.confidence).toBeGreaterThan(0.8);

      // Step 3: Generate mesh for visualization
      const mesh = MeshGenerator.generateMesh(tube);
      expect(mesh).toBeDefined();
      expect(mesh.vertices.length).toBeGreaterThan(0);
      expect(mesh.indices.length).toBeGreaterThan(0);
      expect(mesh.normals.length).toBe(mesh.vertices.length);

      // Step 4: Prepare for Three.js rendering
      const vertices = new Float32Array(mesh.vertices);
      const indices = new Uint32Array(mesh.indices);
      const normals = new Float32Array(mesh.normals);

      expect(vertices.length).toBe(mesh.vertices.length);
      expect(indices.length).toBe(mesh.indices.length);
      expect(normals.length).toBe(mesh.vertices.length);

      // Pipeline complete!
      console.log(`✓ Tube pipeline: OD=${tubeDetection?.od}, Triangles=${mesh.triangleCount}`);
    });
  });

  // ==================== Scenario 2: Assembly with Multiple Tubes ====================

  describe('Scenario 2: Multi-part assembly', () => {
    test('process assembly with 3 tubes', () => {
      // Step 1: Create assembly
      const tubes = [
        GeometryKernel.makeTube(50, 2.5, 200),
        GeometryKernel.makeTube(40, 2.0, 150),
        GeometryKernel.makeTube(32, 1.5, 100),
      ];

      expect(tubes).toHaveLength(3);

      // Step 2: Detect each tube
      const detectedTubes = tubes.map((t, i) => {
        const detected = TubeDetector.detectTube(t);
        return {
          name: `Tube_${i + 1}`,
          detection: detected,
          geometry: t,
        };
      });

      detectedTubes.forEach((tube, i) => {
        expect(tube.detection).toBeDefined();
        expect(tube.detection?.isTube).toBe(true);
      });

      // Step 3: Generate meshes for all parts
      const meshes = detectedTubes.map((t, i) => ({
        name: `Tube_${i + 1}`,
        mesh: MeshGenerator.generateMesh(t.geometry),
      }));

      // Step 4: Prepare assembly for rendering
      const totalVertices = meshes.reduce((sum, m) => sum + m.mesh.vertices.length, 0);
      const totalTriangles = meshes.reduce((sum, m) => sum + m.mesh.triangleCount, 0);

      expect(totalVertices).toBeGreaterThan(0);
      expect(totalTriangles).toBeGreaterThan(0);

      console.log(`✓ Assembly: ${detectedTubes.length} tubes, ${totalTriangles} triangles total`);
    });
  });

  // ==================== Scenario 3: Quality Control & Validation ====================

  describe('Scenario 3: Mesh quality validation', () => {
    test('validate mesh for rendering', () => {
      const cylinder = GeometryKernel.makeCylinder(50, 200);
      const mesh = MeshGenerator.generateMesh(cylinder);

      // Validation 1: No invalid indices
      const maxIndex = Math.max(...mesh.indices);
      const vertexCount = mesh.vertices.length / 3;
      expect(maxIndex).toBeLessThan(vertexCount);

      // Validation 2: All normals unit length
      for (let i = 0; i < mesh.normals.length; i += 3) {
        const nx = mesh.normals[i];
        const ny = mesh.normals[i + 1];
        const nz = mesh.normals[i + 2];
        const length = Math.sqrt(nx * nx + ny * ny + nz * nz);
        expect(length).toBeCloseTo(1.0, 5);
      }

      // Validation 3: Bounding box is valid
      const [minX, minY, minZ, maxX, maxY, maxZ] = mesh.boundingBox;
      expect(minX).toBeLessThanOrEqual(maxX);
      expect(minY).toBeLessThanOrEqual(maxY);
      expect(minZ).toBeLessThanOrEqual(maxZ);

      // Validation 4: Stats match arrays
      expect(mesh.vertexCount).toBe(mesh.vertices.length / 3);
      expect(mesh.triangleCount).toBe(mesh.indices.length / 3);

      console.log(`✓ Mesh validation passed: ${mesh.triangleCount} triangles`);
    });
  });

  // ==================== Scenario 4: Performance Benchmarks ====================

  describe('Scenario 4: Performance validation', () => {
    test('meet performance targets', () => {
      const iterations = 10;

      // Box creation benchmark
      const boxStart = Date.now();
      for (let i = 0; i < iterations; i++) {
        GeometryKernel.makeBox(100, 100, 100);
      }
      const boxTime = (Date.now() - boxStart) / iterations;
      expect(boxTime).toBeLessThan(2);
      console.log(`  - Box creation: ${boxTime.toFixed(2)}ms avg`);

      // Tube detection benchmark
      const tube = GeometryKernel.makeTube(50, 2.5, 200);
      const detectionStart = Date.now();
      for (let i = 0; i < iterations; i++) {
        TubeDetector.detectTube(tube);
      }
      const detectionTime = (Date.now() - detectionStart) / iterations;
      expect(detectionTime).toBeLessThan(10);
      console.log(`  - Tube detection: ${detectionTime.toFixed(2)}ms avg`);

      // Mesh generation benchmark
      const meshStart = Date.now();
      for (let i = 0; i < iterations; i++) {
        MeshGenerator.generateMesh(tube);
      }
      const meshTime = (Date.now() - meshStart) / iterations;
      expect(meshTime).toBeLessThan(200);
      console.log(`  - Mesh generation: ${meshTime.toFixed(2)}ms avg`);

      // Total pipeline
      const totalStart = Date.now();
      for (let i = 0; i < iterations; i++) {
        const t = GeometryKernel.makeTube(50, 2.5, 200);
        TubeDetector.detectTube(t);
        MeshGenerator.generateMesh(t);
      }
      const totalTime = (Date.now() - totalStart) / iterations;
      expect(totalTime).toBeLessThan(300);
      console.log(`  - Complete pipeline: ${totalTime.toFixed(2)}ms avg`);
    });
  });

  // ==================== Scenario 5: Error Recovery ====================

  describe('Scenario 5: Error handling and recovery', () => {
    test('handle edge cases gracefully', () => {
      // Test 1: Tiny geometry
      const tinyTube = GeometryKernel.makeTube(0.01, 0.001, 0.1);
      const tinyMesh = MeshGenerator.generateMesh(tinyTube);
      expect(tinyMesh.vertices.length).toBeGreaterThan(0);

      // Test 2: Huge geometry
      const hugeTube = GeometryKernel.makeTube(10000, 500, 50000);
      const hugeMesh = MeshGenerator.generateMesh(hugeTube);
      expect(hugeMesh.vertices.length).toBeGreaterThan(0);

      // Test 3: Very coarse mesh
      const coarseMesh = MeshGenerator.generateMesh(tinyTube, 10);
      expect(coarseMesh.triangleCount).toBeGreaterThan(0);

      // Test 4: Very fine mesh
      const fineMesh = MeshGenerator.generateMesh(tinyTube, 0.0001);
      expect(fineMesh.triangleCount).toBeGreaterThan(coarseMesh.triangleCount);

      console.log(`✓ Edge cases handled: tiny=${tinyMesh.triangleCount}, coarse=${coarseMesh.triangleCount}, fine=${fineMesh.triangleCount}`);
    });
  });

  // ==================== Scenario 6: REST API Simulation ====================

  describe('Scenario 6: Simulate REST API workflow', () => {
    test('complete REST API call cycle', async () => {
      // Simulate POST /api/mesh/generate request
      const request = {
        shapeType: 'tube',
        params: {
          od: 50,
          wall: 2.5,
          height: 200,
        },
        deflection: 0.1,
      };

      // Backend processing
      const tube = GeometryKernel.makeTube(
        request.params.od,
        request.params.wall,
        request.params.height
      );

      const mesh = MeshGenerator.generateMesh(tube, request.deflection);

      // Prepare response
      const response = {
        ok: true,
        mesh: {
          vertices: mesh.vertices,
          indices: mesh.indices,
          normals: mesh.normals,
          vertexCount: mesh.vertexCount,
          triangleCount: mesh.triangleCount,
          boundingBox: mesh.boundingBox,
        },
        performance: {
          generationTime: 15, // mock
          totalTime: 45, // mock
          deflection: request.deflection,
        },
      };

      expect(response.ok).toBe(true);
      expect(response.mesh.vertexCount).toBeGreaterThan(0);
      expect(response.mesh.triangleCount).toBeGreaterThan(0);

      // Frontend receives and processes
      const { vertices, indices, normals } = response.mesh;
      const float32Vertices = new Float32Array(vertices);
      const uint32Indices = new Uint32Array(indices);
      const float32Normals = new Float32Array(normals);

      // Three.js BufferGeometry creation
      const geometry = {
        position: float32Vertices,
        index: uint32Indices,
        normal: float32Normals,
        vertexCount: response.mesh.vertexCount,
        triangleCount: response.mesh.triangleCount,
      };

      expect(geometry.position.length % 3).toBe(0);
      expect(geometry.index.length % 3).toBe(0);
      expect(geometry.normal.length).toBe(geometry.position.length);

      console.log(`✓ REST API cycle: ${response.mesh.triangleCount} triangles in ${response.performance.totalTime}ms`);
    });
  });

  // ==================== Scenario 7: Deflection Quality Comparison ====================

  describe('Scenario 7: Quality vs Performance tradeoff', () => {
    test('compare deflection settings', () => {
      const tube = GeometryKernel.makeTube(50, 2.5, 200);

      const deflections = [0.01, 0.05, 0.1, 0.5, 1.0];
      const results: any[] = [];

      deflections.forEach((deflection) => {
        const start = Date.now();
        const mesh = MeshGenerator.generateMesh(tube, deflection);
        const time = Date.now() - start;

        results.push({
          deflection,
          triangles: mesh.triangleCount,
          time,
          efficiency: mesh.triangleCount / time, // triangles per ms
        });
      });

      // Verify quality improves with finer deflection
      for (let i = 1; i < results.length; i++) {
        expect(results[i].triangles).toBeLessThanOrEqual(results[i - 1].triangles);
      }

      console.log('Deflection Quality Comparison:');
      results.forEach((r) => {
        console.log(
          `  deflection=${r.deflection}: ${r.triangles} triangles in ${r.time}ms (${r.efficiency.toFixed(0)} tri/ms)`
        );
      });
    });
  });

  // ==================== Scenario 8: Version & Compatibility ====================

  describe('Scenario 8: Version compatibility check', () => {
    test('verify binding versions', () => {
      expect(VERSION.binding).toBeDefined();
      expect(VERSION.occt).toBeDefined();
      expect(VERSION.api).toBeDefined();

      // Version format check
      expect(VERSION.binding).toMatch(/\d+\.\d+\.\d+/);
      expect(VERSION.occt).toContain('7.7');

      console.log(`✓ Binding: ${VERSION.binding}, OCCT: ${VERSION.occt}, API: ${VERSION.api}`);
    });
  });

  // ==================== Full Workflow Test ====================

  describe('Full Workflow: STEP Import → Detection → Mesh → Render', () => {
    test('complete workflow from start to finish', () => {
      console.log('\n=== FULL E2E WORKFLOW ===\n');

      // Phase 1: Import geometry
      console.log('1. Importing geometry...');
      const tubes = [
        { name: 'Main Tube', ...GeometryKernel.makeTube(50, 2.5, 300) },
        { name: 'Branch 1', ...GeometryKernel.makeTube(32, 1.5, 150) },
        { name: 'Branch 2', ...GeometryKernel.makeTube(25, 1.2, 100) },
      ];
      console.log(`   ✓ Imported ${tubes.length} components`);

      // Phase 2: Analyze geometry
      console.log('2. Analyzing geometry...');
      const analysis = tubes.map((t) => ({
        name: t.name,
        detected: TubeDetector.detectTube(t),
      }));
      console.log(`   ✓ Analyzed ${analysis.length} parts`);

      // Phase 3: Generate meshes
      console.log('3. Generating meshes...');
      const meshes = tubes.map((t, i) => ({
        name: analysis[i].name,
        mesh: MeshGenerator.generateMesh(t),
      }));
      const totalTriangles = meshes.reduce((sum, m) => sum + m.mesh.triangleCount, 0);
      console.log(`   ✓ Generated ${totalTriangles} total triangles`);

      // Phase 4: Prepare for rendering
      console.log('4. Preparing for rendering...');
      const renderData = meshes.map((m) => ({
        name: m.name,
        geometry: {
          vertices: new Float32Array(m.mesh.vertices),
          indices: new Uint32Array(m.mesh.indices),
          normals: new Float32Array(m.mesh.normals),
        },
      }));
      console.log(`   ✓ Prepared ${renderData.length} geometries for Three.js`);

      // Phase 5: Render (simulated)
      console.log('5. Rendering to Three.js scene...');
      const scene = {
        meshes: renderData.length,
        totalTriangles,
        totalVertices: renderData.reduce((sum, r) => sum + r.geometry.vertices.length / 3, 0),
      };
      console.log(`   ✓ Scene contains ${scene.meshes} meshes, ${scene.totalTriangles} triangles`);

      console.log('\n=== WORKFLOW COMPLETE ===\n');

      expect(scene.meshes).toBe(3);
      expect(scene.totalTriangles).toBeGreaterThan(0);
    });
  });
});
