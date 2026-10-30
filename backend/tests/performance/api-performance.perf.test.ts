/**
 * BendCore Web - API Performance Tests
 * Benchmarks API response times and throughput
 *
 * Coverage: Performance benchmarks with acceptance criteria
 */

import request from 'supertest';
import express, { Express } from 'express';
import profileRoutes from '../../src/api/routes/profiles';
import tubeRoutes from '../../src/api/routes/tubes';
import sketchRoutes from '../../src/api/routes/sketches';

describe('BendCore API Performance Tests', () => {
  let app: Express;

  beforeAll(() => {
    app = express();
    app.use(express.json());
    app.use('/api/profiles', profileRoutes);
    app.use('/api/tubes', tubeRoutes);
    app.use('/api/sketches', sketchRoutes);
  });

  describe('Response Time Benchmarks (< 200ms acceptance)', () => {
    test('should return profiles list in < 100ms', async () => {
      const startTime = Date.now();

      const res = await request(app)
        .get('/api/profiles')
        .expect(200);

      const duration = Date.now() - startTime;
      console.log(`GET /api/profiles: ${duration}ms`);

      expect(res.status).toBe(200);
      expect(duration).toBeLessThan(100);
    });

    test('should retrieve single profile in < 50ms', async () => {
      const startTime = Date.now();

      const res = await request(app)
        .get('/api/profiles/test-id')
        .expect(200);

      const duration = Date.now() - startTime;
      console.log(`GET /api/profiles/:id: ${duration}ms`);

      expect(duration).toBeLessThan(50);
    });

    test('should solve sketch in < 200ms', async () => {
      const sketch = {
        vertices: Array.from({ length: 10 }, (_, i) => ({
          id: `v${i}`,
          position: { x: i * 10, y: 0, z: 0 },
          is_fixed: i === 0
        })),
        segments: Array.from({ length: 9 }, (_, i) => ({
          id: `s${i}`,
          from_vertex: `v${i}`,
          to_vertex: `v${i + 1}`,
          dimension_L: 10,
          dimension_S: 5,
          bend_angle: 0,
          plane_rotation: 0
        })),
        constraints: Array.from({ length: 9 }, (_, i) => ({
          id: `c${i}`,
          type: 'DISTANCE',
          entity_ids: [`v${i}`, `v${i + 1}`],
          value: 10
        }))
      };

      const startTime = Date.now();

      const res = await request(app)
        .post('/api/sketches/solve')
        .send(sketch)
        .expect(200);

      const duration = Date.now() - startTime;
      console.log(`POST /api/sketches/solve: ${duration}ms`);

      expect(duration).toBeLessThan(200);
    });

    test('should create tube in < 150ms', async () => {
      const tubeData = {
        name: 'Performance Test Tube',
        specification: {
          profile_id: 'test',
          diameter: 0.5,
          wall_thickness: 0.065,
          material: 'STEEL',
          standard: 'ASTM'
        }
      };

      const startTime = Date.now();

      const res = await request(app)
        .post('/api/tubes')
        .send(tubeData)
        .expect(201);

      const duration = Date.now() - startTime;
      console.log(`POST /api/tubes: ${duration}ms`);

      expect(duration).toBeLessThan(150);
    });
  });

  describe('Throughput & Concurrency', () => {
    test('should handle 100 concurrent profile requests', async () => {
      const startTime = Date.now();

      const promises = Array.from({ length: 100 }, () =>
        request(app).get('/api/profiles').expect(200)
      );

      const results = await Promise.all(promises);
      const duration = Date.now() - startTime;

      const avgTime = duration / 100;
      console.log(`100 concurrent GETs: ${duration}ms total, ${avgTime.toFixed(2)}ms avg`);

      expect(results.every(r => r.status === 200)).toBe(true);
      expect(avgTime).toBeLessThan(200);
    });

    test('should handle 50 concurrent tube creations', async () => {
      const startTime = Date.now();

      const promises = Array.from({ length: 50 }, (_, i) =>
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
          .expect(201)
      );

      const results = await Promise.all(promises);
      const duration = Date.now() - startTime;

      const avgTime = duration / 50;
      console.log(`50 concurrent POSTs: ${duration}ms total, ${avgTime.toFixed(2)}ms avg`);

      expect(results.every(r => r.status === 201)).toBe(true);
    });

    test('should maintain performance under sustained load', async () => {
      const iterations = 50;
      const times: number[] = [];

      for (let i = 0; i < iterations; i++) {
        const startTime = Date.now();

        await request(app)
          .get('/api/profiles')
          .expect(200);

        times.push(Date.now() - startTime);
      }

      const avgTime = times.reduce((a, b) => a + b) / times.length;
      const maxTime = Math.max(...times);
      const minTime = Math.min(...times);

      console.log(`Sustained load - Avg: ${avgTime.toFixed(2)}ms, Min: ${minTime}ms, Max: ${maxTime}ms`);

      expect(avgTime).toBeLessThan(100);
      expect(maxTime).toBeLessThan(200);
    });
  });

  describe('Payload Size Impact', () => {
    test('should efficiently handle large sketch (100 vertices)', async () => {
      const largeSketch = {
        vertices: Array.from({ length: 100 }, (_, i) => ({
          id: `v${i}`,
          position: { x: Math.random() * 1000, y: Math.random() * 1000, z: 0 },
          is_fixed: i === 0
        })),
        segments: Array.from({ length: 99 }, (_, i) => ({
          id: `s${i}`,
          from_vertex: `v${i}`,
          to_vertex: `v${i + 1}`,
          dimension_L: 10,
          dimension_S: 5,
          bend_angle: 0,
          plane_rotation: 0
        })),
        constraints: []
      };

      const startTime = Date.now();

      const res = await request(app)
        .post('/api/sketches/solve')
        .send(largeSketch);

      const duration = Date.now() - startTime;
      const payloadSize = JSON.stringify(largeSketch).length / 1024; // KB

      console.log(`Large sketch (${payloadSize.toFixed(1)}KB): ${duration}ms`);

      expect(duration).toBeLessThan(500); // More lenient for large payloads
    });

    test('should process large batch imports efficiently', async () => {
      const batchImport = {
        segments: Array.from({ length: 200 }, (_, i) => ({
          start: [i * 10, 0, 0],
          end: [(i + 1) * 10, 0, 0],
          type: 'line'
        })),
        tolerance: 0.001
      };

      const startTime = Date.now();

      const res = await request(app)
        .post('/api/import')
        .send(batchImport)
        .expect(200);

      const duration = Date.now() - startTime;

      console.log(`Batch import (200 segments): ${duration}ms`);

      expect(duration).toBeLessThan(500);
    });
  });

  describe('Database Query Performance', () => {
    test('should filter profiles by category efficiently', async () => {
      const startTime = Date.now();

      const res = await request(app)
        .get('/api/profiles?category=tube')
        .expect(200);

      const duration = Date.now() - startTime;

      console.log(`Profile filter query: ${duration}ms`);

      expect(duration).toBeLessThan(100);
    });

    test('should retrieve paginated results efficiently', async () => {
      const startTime = Date.now();

      const res = await request(app)
        .get('/api/profiles?limit=50&offset=0')
        .expect(200);

      const duration = Date.now() - startTime;

      console.log(`Paginated query: ${duration}ms`);

      expect(duration).toBeLessThan(100);
    });
  });

  describe('Cache Effectiveness', () => {
    test('should improve response time on repeated requests', async () => {
      // First request (cache miss)
      const start1 = Date.now();
      await request(app).get('/api/profiles').expect(200);
      const time1 = Date.now() - start1;

      // Second request (cache hit)
      const start2 = Date.now();
      await request(app).get('/api/profiles').expect(200);
      const time2 = Date.now() - start2;

      console.log(`First request: ${time1}ms, Cached request: ${time2}ms`);

      // Cached request should be faster
      expect(time2).toBeLessThanOrEqual(time1);
    });
  });

  describe('Error Response Performance', () => {
    test('should return error quickly for invalid requests', async () => {
      const startTime = Date.now();

      const res = await request(app)
        .post('/api/tubes')
        .send({})
        .expect(400);

      const duration = Date.now() - startTime;

      console.log(`Error response: ${duration}ms`);

      expect(duration).toBeLessThan(50); // Errors should be fast
    });
  });
});
