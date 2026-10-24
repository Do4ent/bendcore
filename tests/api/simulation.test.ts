/**
 * BendCore FEA Simulation API Tests
 */

import axios from 'axios';

const API_URL = process.env.API_URL || 'http://localhost:3001/api';

describe('FEA Simulation API', () => {
  const client = axios.create({
    baseURL: API_URL,
    timeout: 10000,
  });

  describe('Health Check', () => {
    test('should respond to health check', async () => {
      const response = await client.get('/health');
      expect(response.status).toBe(200);
      expect(response.data).toHaveProperty('status', 'ok');
      expect(response.data).toHaveProperty('version');
    });

    test('health check should be fast', async () => {
      const start = Date.now();
      await client.get('/health');
      const duration = Date.now() - start;
      expect(duration).toBeLessThan(200);
    });
  });

  describe('Materials Library', () => {
    test('should list available materials', async () => {
      const response = await client.get('/simulation/materials');
      expect(response.status).toBe(200);
      expect(response.data.materials).toBeDefined();
      expect(Array.isArray(response.data.materials)).toBe(true);
      expect(response.data.materials.length).toBeGreaterThan(0);
    });

    test('should include steel material', async () => {
      const response = await client.get('/simulation/materials');
      const steel = response.data.materials.find(
        (m: any) => m.id === 'steel'
      );
      expect(steel).toBeDefined();
      expect(steel.elasticModulus).toBe(210e9);
      expect(steel.yieldStrength).toBe(250e6);
    });

    test('should include aluminum material', async () => {
      const response = await client.get('/simulation/materials');
      const aluminum = response.data.materials.find(
        (m: any) => m.id === 'aluminum'
      );
      expect(aluminum).toBeDefined();
      expect(aluminum.elasticModulus).toBe(69e9);
    });

    test('materials should have required properties', async () => {
      const response = await client.get('/simulation/materials');
      for (const material of response.data.materials) {
        expect(material).toHaveProperty('id');
        expect(material).toHaveProperty('name');
        expect(material).toHaveProperty('elasticModulus');
        expect(material).toHaveProperty('poissonsRatio');
        expect(material).toHaveProperty('yieldStrength');
      }
    });
  });

  describe('Stress Analysis', () => {
    const geometry = {
      outerRadius: 25,
      innerRadius: 20,
      length: 300,
    };

    test('should run stress analysis', async () => {
      const response = await client.post('/simulation/stress', {
        geometry,
        material: 'steel',
        appliedLoad: 5000,
      });

      expect(response.status).toBe(200);
      expect(response.data.success).toBe(true);
      expect(response.data.result).toBeDefined();
    });

    test('stress analysis should return required fields', async () => {
      const response = await client.post('/simulation/stress', {
        geometry,
        material: 'steel',
      });

      const result = response.data.result;
      expect(result).toHaveProperty('maxStress');
      expect(result).toHaveProperty('minStress');
      expect(result).toHaveProperty('maxDisplacement');
      expect(result).toHaveProperty('safetyFactor');
      expect(result).toHaveProperty('convergence');
      expect(result).toHaveProperty('executionTime');
    });

    test('stress values should be physical', async () => {
      const response = await client.post('/simulation/stress', {
        geometry,
        material: 'steel',
        appliedLoad: 1000,
      });

      const result = response.data.result;
      expect(result.maxStress).toBeGreaterThan(0);
      expect(result.safetyFactor).toBeGreaterThan(0);
      expect(result.executionTime).toBeGreaterThan(0);
      expect(result.executionTime).toBeLessThan(5000);
    });

    test('should include interpretation', async () => {
      const response = await client.post('/simulation/stress', {
        geometry,
        material: 'steel',
      });

      const interpretation = response.data.interpretation;
      expect(interpretation).toHaveProperty('isSafe');
      expect(interpretation).toHaveProperty('safetyFactor');
      expect(interpretation).toHaveProperty('recommendation');
    });

    test('should handle different support types', async () => {
      const supportTypes = ['fixed', 'cantilever', 'supported'];

      for (const supportType of supportTypes) {
        const response = await client.post('/simulation/stress', {
          geometry,
          material: 'steel',
          supportType,
        });

        expect(response.status).toBe(200);
        expect(response.data.result).toBeDefined();
      }
    });

    test('should handle different materials', async () => {
      const materials = ['steel', 'aluminum', 'titanium', 'copper'];

      for (const material of materials) {
        const response = await client.post('/simulation/stress', {
          geometry,
          material,
        });

        expect(response.status).toBe(200);
        expect(response.data.result).toBeDefined();
      }
    });

    test('should reject invalid material', async () => {
      try {
        await client.post('/simulation/stress', {
          geometry,
          material: 'invalid_material',
        });
        fail('Should throw error');
      } catch (error: any) {
        expect(error.response.status).toBe(400);
        expect(error.response.data.error).toBe(true);
      }
    });

    test('should reject missing geometry', async () => {
      try {
        await client.post('/simulation/stress', {
          material: 'steel',
        });
        fail('Should throw error');
      } catch (error: any) {
        expect(error.response.status).toBe(400);
      }
    });
  });

  describe('Displacement Analysis', () => {
    const geometry = {
      outerRadius: 25,
      innerRadius: 20,
      length: 300,
    };

    test('should run displacement analysis', async () => {
      const response = await client.post('/simulation/displacement', {
        geometry,
        material: 'steel',
        appliedLoad: 1000,
      });

      expect(response.status).toBe(200);
      expect(response.data.result).toBeDefined();
    });

    test('should include buckling analysis', async () => {
      const response = await client.post('/simulation/displacement', {
        geometry,
        material: 'steel',
      });

      expect(response.data.buckling).toBeDefined();
      expect(response.data.buckling).toHaveProperty('criticalLoad');
      expect(response.data.buckling).toHaveProperty('bucklingMode');
      expect(response.data.buckling).toHaveProperty('riskLevel');
    });
  });

  describe('Buckling Analysis', () => {
    const geometry = {
      outerRadius: 15,
      innerRadius: 10,
      length: 500,
    };

    test('should run buckling analysis', async () => {
      const response = await client.post('/simulation/buckling', {
        geometry,
        material: 'steel',
      });

      expect(response.status).toBe(200);
      expect(response.data.buckling).toBeDefined();
    });

    test('should return critical load', async () => {
      const response = await client.post('/simulation/buckling', {
        geometry,
        material: 'steel',
      });

      const buckling = response.data.buckling;
      expect(buckling.criticalLoad).toBeGreaterThan(0);
      expect(buckling.slendernessRatio).toBeGreaterThan(0);
      expect(buckling.momentOfInertia).toBeGreaterThan(0);
    });

    test('should assess risk level', async () => {
      const response = await client.post('/simulation/buckling', {
        geometry,
        material: 'steel',
        supportCondition: 'both-fixed',
      });

      const buckling = response.data.buckling;
      expect(['low', 'medium', 'high']).toContain(buckling.riskLevel);
    });

    test('should handle different support conditions', async () => {
      const conditions = ['both-fixed', 'fixed-free', 'both-supported'];

      for (const condition of conditions) {
        const response = await client.post('/simulation/buckling', {
          geometry,
          material: 'steel',
          supportCondition: condition,
        });

        expect(response.status).toBe(200);
      }
    });
  });

  describe('Bending Analysis', () => {
    const geometry = {
      outerRadius: 25,
      innerRadius: 20,
      length: 300,
    };

    test('should run bending analysis', async () => {
      const response = await client.post('/simulation/bending', {
        geometry,
        material: 'steel',
        bendRadius: 100,
        bendAngle: 90,
      });

      expect(response.status).toBe(200);
      expect(response.data.bending).toBeDefined();
    });

    test('should analyze material thinning', async () => {
      const response = await client.post('/simulation/bending', {
        geometry,
        material: 'steel',
        bendRadius: 50, // Tight bend
        bendAngle: 90,
      });

      const bending = response.data.bending;
      expect(bending).toHaveProperty('wipeoutPercentage');
      expect(bending).toHaveProperty('materialThinning');
      expect(bending.wipeoutPercentage).toBeGreaterThan(0);
    });

    test('should calculate springback angle', async () => {
      const response = await client.post('/simulation/bending', {
        geometry,
        material: 'steel',
        bendRadius: 100,
        bendAngle: 90,
      });

      const bending = response.data.bending;
      expect(bending).toHaveProperty('springbackAngle');
      expect(parseFloat(bending.springbackAngle)).toBeGreaterThan(0);
    });
  });

  describe('Concurrent Requests', () => {
    test('should handle 5 concurrent requests', async () => {
      const requests = Array(5)
        .fill(null)
        .map(() =>
          client.post('/simulation/stress', {
            geometry: {
              outerRadius: 25,
              innerRadius: 20,
              length: 300,
            },
            material: 'steel',
          })
        );

      const results = await Promise.all(requests);
      expect(results.length).toBe(5);
      results.forEach((response) => {
        expect(response.status).toBe(200);
      });
    });

    test('should handle 10 concurrent requests', async () => {
      const requests = Array(10)
        .fill(null)
        .map(() =>
          client.get('/simulation/materials')
        );

      const results = await Promise.all(requests);
      expect(results.length).toBe(10);
    });
  });

  describe('Performance', () => {
    test('stress analysis should complete in < 2s', async () => {
      const start = Date.now();

      await client.post('/simulation/stress', {
        geometry: {
          outerRadius: 25,
          innerRadius: 20,
          length: 300,
        },
        material: 'steel',
      });

      const duration = Date.now() - start;
      expect(duration).toBeLessThan(2000);
    });

    test('materials endpoint should respond in < 500ms', async () => {
      const start = Date.now();
      await client.get('/simulation/materials');
      const duration = Date.now() - start;
      expect(duration).toBeLessThan(500);
    });
  });

  describe('Error Handling', () => {
    test('should return 400 for invalid geometry', async () => {
      try {
        await client.post('/simulation/stress', {
          geometry: {
            outerRadius: 0,
            innerRadius: 0,
            length: 0,
          },
          material: 'steel',
        });
        fail('Should throw error');
      } catch (error: any) {
        expect(error.response.status).toBeGreaterThanOrEqual(400);
      }
    });

    test('should return error for missing required fields', async () => {
      try {
        await client.post('/simulation/stress', {
          geometry: {
            outerRadius: 25,
            // missing innerRadius and length
          },
          material: 'steel',
        });
        fail('Should throw error');
      } catch (error: any) {
        expect(error.response.status).toBeGreaterThanOrEqual(400);
      }
    });

    test('should handle network timeout gracefully', async () => {
      try {
        const timeoutClient = axios.create({
          baseURL: API_URL,
          timeout: 1, // Very short timeout
        });

        await timeoutClient.post('/simulation/stress', {
          geometry: {
            outerRadius: 25,
            innerRadius: 20,
            length: 300,
          },
          material: 'steel',
        });
      } catch (error: any) {
        expect(error.code).toBe('ECONNABORTED');
      }
    });
  });
});
