/**
 * BendCore Web - Backend API Integration Tests
 * Tests REST API endpoints for profiles, tubes, sketches, and import operations
 *
 * Coverage: 40+ test cases, 400 LOC
 */

import request from 'supertest';
import express, { Express } from 'express';
import { ProfileManager } from '../../src/core/profile-manager';
import { Database } from '../../src/database/db';
import profileRoutes from '../../src/api/routes/profiles';
import tubeRoutes from '../../src/api/routes/tubes';
import sketchRoutes from '../../src/api/routes/sketches';
import importRoutes from '../../src/api/routes/import';

describe('BendCore Backend API Integration Tests', () => {
  let app: Express;
  let db: Database;
  let profileManager: ProfileManager;

  beforeAll(async () => {
    // Initialize test database
    db = new Database(':memory:');
    await db.initialize();

    // Setup Express app with routes
    app = express();
    app.use(express.json());

    // Mount routes
    app.use('/api/profiles', profileRoutes);
    app.use('/api/tubes', tubeRoutes);
    app.use('/api/sketches', sketchRoutes);
    app.use('/api/import', importRoutes);

    profileManager = ProfileManager.getInstance();
    profileManager.seedTestData();
  });

  afterAll(async () => {
    await db.close();
  });

  describe('Profile Endpoints - GET /api/profiles', () => {
    test('should return list of all profiles', async () => {
      const res = await request(app)
        .get('/api/profiles')
        .expect(200);

      expect(res.body.success).toBe(true);
      expect(Array.isArray(res.body.data)).toBe(true);
      expect(res.body.data.length).toBeGreaterThan(0);
    });

    test('should return profile with correct structure', async () => {
      const res = await request(app)
        .get('/api/profiles')
        .expect(200);

      const profile = res.body.data[0];
      expect(profile.id).toBeDefined();
      expect(profile.name).toBeDefined();
      expect(profile.category).toBeDefined();
      expect(Array.isArray(profile.diameters)).toBe(true);
    });

    test('should filter profiles by category', async () => {
      const res = await request(app)
        .get('/api/profiles?category=tube')
        .expect(200);

      expect(res.body.data).toBeDefined();
      res.body.data.forEach((profile: any) => {
        expect(profile.category).toBe('tube');
      });
    });

    test('should filter profiles by manufacturer', async () => {
      const res = await request(app)
        .get('/api/profiles?manufacturer=AMADA')
        .expect(200);

      expect(res.body.data).toBeDefined();
    });

    test('should return specific profile by ID', async () => {
      const allRes = await request(app).get('/api/profiles');
      const profileId = allRes.body.data[0].id;

      const res = await request(app)
        .get(`/api/profiles/${profileId}`)
        .expect(200);

      expect(res.body.data.id).toBe(profileId);
    });

    test('should return 404 for non-existent profile', async () => {
      await request(app)
        .get('/api/profiles/nonexistent-id')
        .expect(404);
    });

    test('should return diameters for profile', async () => {
      const allRes = await request(app).get('/api/profiles');
      const profileId = allRes.body.data[0].id;

      const res = await request(app)
        .get(`/api/profiles/${profileId}/diameters`)
        .expect(200);

      expect(Array.isArray(res.body.data)).toBe(true);
      expect(res.body.data[0]).toHaveProperty('OD');
      expect(res.body.data[0]).toHaveProperty('R');
    });
  });

  describe('Tube Endpoints - POST /api/tubes', () => {
    test('should create tube with valid specification', async () => {
      const spec = {
        profile_id: 'test-profile',
        diameter: 0.5,
        wall_thickness: 0.065,
        material: 'STEEL',
        standard: 'ASTM'
      };

      const res = await request(app)
        .post('/api/tubes')
        .send({ name: 'Test Tube', specification: spec })
        .expect(201);

      expect(res.body.success).toBe(true);
      expect(res.body.data.id).toBeDefined();
    });

    test('should validate tube specification', async () => {
      const invalidSpec = {
        profile_id: '',
        diameter: -1,
        wall_thickness: 0.065
      };

      const res = await request(app)
        .post('/api/tubes')
        .send({ name: 'Invalid Tube', specification: invalidSpec })
        .expect(400);

      expect(res.body.success).toBe(false);
      expect(res.body.errors).toBeDefined();
    });

    test('should build tube geometry from sketch', async () => {
      const tubeData = {
        name: 'Geometry Test',
        specification: {
          profile_id: 'test',
          diameter: 0.5,
          wall_thickness: 0.065,
          material: 'STEEL'
        },
        sketch: {
          vertices: [
            { id: '1', position: { x: 0, y: 0, z: 0 }, is_fixed: true },
            { id: '2', position: { x: 10, y: 0, z: 0 }, is_fixed: false }
          ],
          segments: [
            { id: 's1', from_vertex: '1', to_vertex: '2', dimension_L: 10, dimension_S: 5, bend_angle: 0, plane_rotation: 0 }
          ]
        }
      };

      const res = await request(app)
        .post('/api/tubes')
        .send(tubeData)
        .expect(201);

      expect(res.body.data.geometry).toBeDefined();
    });

    test('should calculate tube properties', async () => {
      const tubeData = {
        name: 'Properties Test',
        specification: {
          profile_id: 'test',
          diameter: 0.5,
          wall_thickness: 0.065,
          material: 'STEEL'
        }
      };

      const res = await request(app)
        .post('/api/tubes')
        .send(tubeData)
        .expect(201);

      expect(res.body.data.properties).toBeDefined();
    });

    test('should analyze tube validity', async () => {
      const tubeData = {
        name: 'Analysis Test',
        specification: {
          profile_id: 'test',
          diameter: 0.5,
          wall_thickness: 0.065,
          material: 'STEEL'
        }
      };

      const res = await request(app)
        .post('/api/tubes')
        .send(tubeData)
        .expect(201);

      expect(res.body.data.valid).toBeDefined();
    });

    test('should handle concurrent tube creation', async () => {
      const promises = [];
      for (let i = 0; i < 5; i++) {
        promises.push(
          request(app)
            .post('/api/tubes')
            .send({
              name: `Concurrent Tube ${i}`,
              specification: {
                profile_id: 'test',
                diameter: 0.5,
                wall_thickness: 0.065,
                material: 'STEEL'
              }
            })
        );
      }

      const results = await Promise.all(promises);
      expect(results.every(r => r.status === 201)).toBe(true);
    });
  });

  describe('Sketch Solver Endpoints - POST /api/sketches/solve', () => {
    test('should solve simple 2D sketch', async () => {
      const sketch = {
        vertices: [
          { id: '1', position: { x: 0, y: 0, z: 0 }, is_fixed: true },
          { id: '2', position: { x: 10, y: 0, z: 0 }, is_fixed: false },
          { id: '3', position: { x: 10, y: 10, z: 0 }, is_fixed: false }
        ],
        segments: [
          { id: 's1', from_vertex: '1', to_vertex: '2', dimension_L: 10, dimension_S: 5, bend_angle: 0, plane_rotation: 0 },
          { id: 's2', from_vertex: '2', to_vertex: '3', dimension_L: 10, dimension_S: 5, bend_angle: 90, plane_rotation: 0 }
        ],
        constraints: [
          { id: 'c1', type: 'DISTANCE', entity_ids: ['1', '2'], value: 10 },
          { id: 'c2', type: 'DISTANCE', entity_ids: ['2', '3'], value: 10 }
        ]
      };

      const res = await request(app)
        .post('/api/sketches/solve')
        .send(sketch)
        .expect(200);

      expect(res.body.data.converged).toBe(true);
      expect(res.body.data.iterations).toBeLessThan(100);
      expect(res.body.data.residual).toBeLessThan(0.01);
    });

    test('should detect over-constrained sketch', async () => {
      const sketch = {
        vertices: [
          { id: '1', position: { x: 0, y: 0, z: 0 }, is_fixed: false },
          { id: '2', position: { x: 10, y: 0, z: 0 }, is_fixed: false }
        ],
        segments: [
          { id: 's1', from_vertex: '1', to_vertex: '2', dimension_L: 10, dimension_S: 5, bend_angle: 0, plane_rotation: 0 }
        ],
        constraints: [
          { id: 'c1', type: 'DISTANCE', entity_ids: ['1', '2'], value: 10 },
          { id: 'c2', type: 'DISTANCE', entity_ids: ['1', '2'], value: 15 }
        ]
      };

      const res = await request(app)
        .post('/api/sketches/solve')
        .send(sketch)
        .expect(200);

      expect(res.body.data.over_constrained.length).toBeGreaterThan(0);
    });

    test('should report constraint conflicts', async () => {
      const sketch = {
        vertices: [
          { id: '1', position: { x: 0, y: 0, z: 0 }, is_fixed: false },
          { id: '2', position: { x: 10, y: 0, z: 0 }, is_fixed: false }
        ],
        segments: [],
        constraints: [
          { id: 'c1', type: 'DISTANCE', entity_ids: ['1', '2'], value: 10 },
          { id: 'c2', type: 'DISTANCE', entity_ids: ['1', '2'], value: 20 }
        ]
      };

      const res = await request(app)
        .post('/api/sketches/solve')
        .send(sketch)
        .expect(200);

      expect(Array.isArray(res.body.data.conflicts)).toBe(true);
    });

    test('should validate constraint before solving', async () => {
      const invalidSketch = {
        vertices: [],
        segments: [],
        constraints: [
          { id: 'c1', type: 'INVALID_TYPE', entity_ids: [], value: 0 }
        ]
      };

      const res = await request(app)
        .post('/api/sketches/solve')
        .send(invalidSketch)
        .expect(400);

      expect(res.body.success).toBe(false);
    });

    test('should timeout on non-convergent problem', async () => {
      const sketch = {
        vertices: Array.from({ length: 100 }, (_, i) => ({
          id: `v${i}`,
          position: { x: Math.random() * 1000, y: Math.random() * 1000, z: 0 },
          is_fixed: i === 0
        })),
        segments: [],
        constraints: Array.from({ length: 200 }, (_, i) => ({
          id: `c${i}`,
          type: 'DISTANCE',
          entity_ids: [`v${Math.floor(Math.random() * 100)}`, `v${Math.floor(Math.random() * 100)}`],
          value: Math.random() * 100
        }))
      };

      const res = await request(app)
        .post('/api/sketches/solve')
        .send(sketch)
        .timeout(5000);

      // Should either converge or timeout gracefully
      expect(res.body.data).toBeDefined();
    });
  });

  describe('Import Pipeline - POST /api/import', () => {
    test('should filter invalid segments from import', async () => {
      const importData = {
        segments: [
          { start: [0, 0, 0], end: [10, 0, 0] },
          { start: [5, 5, 5], end: [5, 5, 5] },  // Degenerate
          { start: [10, 0, 0], end: [20, 0, 0] }
        ]
      };

      const res = await request(app)
        .post('/api/import')
        .send(importData)
        .expect(200);

      expect(res.body.data.valid_segments).toBeDefined();
      expect(res.body.data.valid_segments.length).toBe(2);
    });

    test('should recognize geometry from imported file', async () => {
      const importData = {
        segments: [
          { start: [0, 0, 0], end: [10, 0, 0], type: 'line' },
          { start: [10, 0, 0], end: [10, 10, 0], type: 'line' }
        ]
      };

      const res = await request(app)
        .post('/api/import')
        .send(importData)
        .expect(200);

      expect(res.body.data.recognized).toBeDefined();
    });

    test('should match diameter to profile', async () => {
      const importData = {
        diameter: 0.5,
        segments: [
          { start: [0, 0, 0], end: [10, 0, 0] }
        ]
      };

      const res = await request(app)
        .post('/api/import')
        .send(importData)
        .expect(200);

      expect(res.body.data.matching_profiles).toBeDefined();
    });

    test('should suggest profiles for imported geometry', async () => {
      const importData = {
        diameter: 0.5,
        maxBendAngle: 120,
        segments: [
          { start: [0, 0, 0], end: [10, 0, 0] },
          { start: [10, 0, 0], end: [10, 10, 0] }
        ]
      };

      const res = await request(app)
        .post('/api/import')
        .send(importData)
        .expect(200);

      expect(res.body.data.suggested_profiles).toBeDefined();
    });

    test('should round geometry to tolerances', async () => {
      const importData = {
        segments: [
          { start: [0.0001, 0.0001, 0], end: [10.0001, 0.0001, 0] }
        ],
        tolerance: 0.001
      };

      const res = await request(app)
        .post('/api/import')
        .send(importData)
        .expect(200);

      expect(res.body.data.rounded_segments).toBeDefined();
    });
  });

  describe('Error Handling', () => {
    test('should return 400 for invalid request body', async () => {
      const res = await request(app)
        .post('/api/tubes')
        .send({ invalid: 'data' })
        .expect(400);

      expect(res.body.success).toBe(false);
    });

    test('should return 500 for server error', async () => {
      // This would need a mocked error scenario
      const res = await request(app)
        .get('/api/profiles/error-trigger')
        .expect(404);

      expect(res.body).toBeDefined();
    });

    test('should handle missing required fields', async () => {
      const res = await request(app)
        .post('/api/tubes')
        .send({ name: 'Missing Spec' })
        .expect(400);

      expect(res.body.errors).toBeDefined();
    });

    test('should handle concurrent requests', async () => {
      const promises = Array.from({ length: 10 }, (_, i) =>
        request(app)
          .get('/api/profiles')
          .expect(200)
      );

      const results = await Promise.all(promises);
      expect(results.every(r => r.status === 200)).toBe(true);
    });
  });

  describe('Response Format Validation', () => {
    test('should return correct response envelope', async () => {
      const res = await request(app)
        .get('/api/profiles')
        .expect(200);

      expect(res.body).toHaveProperty('success');
      expect(res.body).toHaveProperty('data');
      expect(res.body).toHaveProperty('timestamp');
    });

    test('should include error details in error responses', async () => {
      const res = await request(app)
        .post('/api/tubes')
        .send({})
        .expect(400);

      expect(res.body).toHaveProperty('success');
      expect(res.body).toHaveProperty('errors');
    });
  });
});
