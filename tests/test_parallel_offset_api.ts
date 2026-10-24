import request from 'supertest';
import express, { Express } from 'express';
import parallelOffsetRouter from '../src/api/routes/parallel-offset';

describe('Parallel Offset API Routes', () => {
  let app: Express;

  beforeAll(() => {
    app = express();
    app.use(express.json());
    app.use('/api/bendcore/parallel-offset', parallelOffsetRouter);
  });

  // ======================== ТЕСТЫ POST /calculate ========================

  describe('POST /calculate', () => {
    test('должен вернуть результат для валидных параметров', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/calculate').send({
        outer_diameter: 20.0,
        wall_thickness: 2.0,
        bend_angle: 90.0,
        offset_distance: 30.0,
        bend_radius: 50.0,
        material: 'Steel',
      });

      expect(response.status).toBe(200);
      expect(response.body.success).toBe(true);
      expect(response.body.data).toBeDefined();
      expect(response.body.data.total_length).toBeGreaterThan(0);
      expect(response.body.data.is_valid).toBe(true);
    });

    test('должен вернуть ошибку для диаметра меньше минимума', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/calculate').send({
        outer_diameter: 2.0, // Ошибка: < 3
        wall_thickness: 0.5,
        bend_angle: 90.0,
        offset_distance: 10.0,
        bend_radius: 10.0,
        material: 'Steel',
      });

      expect(response.status).toBe(400);
      expect(response.body.success).toBe(false);
      expect(response.body.errors).toBeDefined();
      expect(response.body.errors.length).toBeGreaterThan(0);
    });

    test('должен вернуть ошибку для недопустимого угла гибки', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/calculate').send({
        outer_diameter: 20.0,
        wall_thickness: 2.0,
        bend_angle: 2.0, // Ошибка: < 5
        offset_distance: 30.0,
        bend_radius: 50.0,
        material: 'Steel',
      });

      expect(response.status).toBe(400);
      expect(response.body.errors[0].code).toBe('INVALID_ANGLE');
    });

    test('должен вернуть ошибку для отрицательного смещения', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/calculate').send({
        outer_diameter: 20.0,
        wall_thickness: 2.0,
        bend_angle: 90.0,
        offset_distance: -10.0, // Ошибка: отрицательное
        bend_radius: 50.0,
        material: 'Steel',
      });

      expect(response.status).toBe(400);
      expect(response.body.errors[0].code).toBe('INVALID_OFFSET');
    });

    test('должен вернуть ошибку для радиуса слишком малого', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/calculate').send({
        outer_diameter: 20.0,
        wall_thickness: 2.0,
        bend_angle: 90.0,
        offset_distance: 30.0,
        bend_radius: 10.0, // Слишком малый (нужен минимум 50 для стали)
        material: 'Steel',
      });

      expect(response.status).toBe(400);
      expect(response.body.errors[0].code).toBe('RADIUS_TOO_SMALL');
    });

    test('должен работать с разными материалами', async () => {
      const materials = ['Steel', 'Aluminum', 'Copper', 'Stainless Steel'];

      for (const material of materials) {
        const response = await request(app)
          .post('/api/bendcore/parallel-offset/calculate')
          .send({
            outer_diameter: 20.0,
            wall_thickness: 2.0,
            bend_angle: 90.0,
            offset_distance: 30.0,
            bend_radius: material === 'Aluminum' ? 40.0 : 50.0, // Алюминий требует меньший радиус
            material,
          });

        if (material === 'Aluminum') {
          // Алюминий имеет более низкие требования к радиусу
          expect([200, 400]).toContain(response.status);
        } else {
          expect(response.status).toBe(200);
          expect(response.body.success).toBe(true);
        }
      }
    });

    test('должен вернуть 90 градусов в результате для 90 градусов на входе', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/calculate').send({
        outer_diameter: 20.0,
        wall_thickness: 2.0,
        bend_angle: 90.0,
        offset_distance: 30.0,
        bend_radius: 50.0,
        material: 'Steel',
      });

      expect(response.status).toBe(200);
      expect(response.body.data.bend_angles[0]).toBeCloseTo(90.0, 1);
    });

    test('должен иметь положительную total_length', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/calculate').send({
        outer_diameter: 20.0,
        wall_thickness: 2.0,
        bend_angle: 90.0,
        offset_distance: 30.0,
        bend_radius: 50.0,
        material: 'Steel',
      });

      expect(response.status).toBe(200);
      expect(response.body.data.total_length).toBeGreaterThan(0);
    });

    test('должен иметь confidence между 50 и 100', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/calculate').send({
        outer_diameter: 20.0,
        wall_thickness: 2.0,
        bend_angle: 90.0,
        offset_distance: 30.0,
        bend_radius: 50.0,
        material: 'Steel',
      });

      expect(response.status).toBe(200);
      expect(response.body.data.confidence).toBeGreaterThanOrEqual(50);
      expect(response.body.data.confidence).toBeLessThanOrEqual(100);
    });
  });

  // ======================== ТЕСТЫ POST /validate ========================

  describe('POST /validate', () => {
    test('должен вернуть is_valid true для валидных параметров', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/validate').send({
        outer_diameter: 20.0,
        wall_thickness: 2.0,
        bend_angle: 90.0,
        offset_distance: 30.0,
        bend_radius: 50.0,
        material: 'Steel',
      });

      expect(response.status).toBe(200);
      expect(response.body.success).toBe(true);
      expect(response.body.errors.length).toBe(0);
    });

    test('должен вернуть ошибки для невалидных параметров', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/validate').send({
        outer_diameter: 2.0,
        wall_thickness: 0.1,
        bend_angle: 2.0,
        offset_distance: -5.0,
        bend_radius: 5.0,
        material: 'Steel',
      });

      expect(response.status).toBe(200);
      expect(response.body.success).toBe(false);
      expect(response.body.errors.length).toBeGreaterThan(0);
    });

    test('должен возвращать предупреждения для экстремальных углов', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/validate').send({
        outer_diameter: 20.0,
        wall_thickness: 2.0,
        bend_angle: 5.0, // Минимальный угол
        offset_distance: 30.0,
        bend_radius: 50.0,
        material: 'Steel',
      });

      expect(response.status).toBe(200);
      expect(response.body.warnings).toBeDefined();
      expect(Array.isArray(response.body.warnings)).toBe(true);
    });

    test('должен возвращать предупреждения для маленького смещения', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/validate').send({
        outer_diameter: 20.0,
        wall_thickness: 2.0,
        bend_angle: 90.0,
        offset_distance: 0.1, // Очень малое смещение (0.5% OD)
        bend_radius: 50.0,
        material: 'Steel',
      });

      expect(response.status).toBe(200);
      expect(response.body.warnings.length).toBeGreaterThan(0);
    });
  });

  // ======================== ТЕСТЫ GET /materials ========================

  describe('GET /materials', () => {
    test('должен вернуть список материалов', async () => {
      const response = await request(app).get('/api/bendcore/parallel-offset/materials');

      expect(response.status).toBe(200);
      expect(response.body.success).toBe(true);
      expect(response.body.materials).toBeDefined();
      expect(Array.isArray(response.body.materials)).toBe(true);
    });

    test('должен содержать Steel в списке материалов', async () => {
      const response = await request(app).get('/api/bendcore/parallel-offset/materials');

      expect(response.status).toBe(200);
      const materials = response.body.materials.map((m: any) => m.name);
      expect(materials).toContain('Steel');
    });

    test('материалы должны иметь factor и min_bend_radius_factor', async () => {
      const response = await request(app).get('/api/bendcore/parallel-offset/materials');

      expect(response.status).toBe(200);
      for (const material of response.body.materials) {
        expect(material.name).toBeDefined();
        expect(material.factor).toBeDefined();
        expect(material.min_bend_radius_factor).toBeDefined();
        expect(typeof material.factor).toBe('number');
        expect(typeof material.min_bend_radius_factor).toBe('number');
      }
    });

    test('Steel должен иметь factor 1.0', async () => {
      const response = await request(app).get('/api/bendcore/parallel-offset/materials');

      const steel = response.body.materials.find((m: any) => m.name === 'Steel');
      expect(steel).toBeDefined();
      expect(steel.factor).toBe(1.0);
    });

    test('Aluminum должен иметь меньший factor чем Steel', async () => {
      const response = await request(app).get('/api/bendcore/parallel-offset/materials');

      const steel = response.body.materials.find((m: any) => m.name === 'Steel');
      const aluminum = response.body.materials.find((m: any) => m.name === 'Aluminum');

      expect(aluminum.factor).toBeLessThan(steel.factor);
    });
  });

  // ======================== ТЕСТЫ POST /export ========================

  describe('POST /export', () => {
    test('должен экспортировать в CSV', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/export').send({
        calculation_result: {
          total_length: 100,
          bend_1_length: 20,
          straight_section: 60,
          bend_2_length: 20,
          bend_angles: [90, 90],
          bend_1_pos: [0, 0, 0],
          bend_2_pos: [80, 30, 5],
          confidence: 95,
          warnings: 'Test',
          is_valid: true,
        },
        parameters: {
          outer_diameter: 20,
          wall_thickness: 2,
          bend_angle: 90,
          offset_distance: 30,
          bend_radius: 50,
          material: 'Steel',
        },
        format: 'csv',
      });

      expect(response.status).toBe(200);
      expect(response.headers['content-type']).toContain('text/csv');
      expect(response.text).toContain('Результаты расчёта');
    });

    test('должен экспортировать в DXF', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/export').send({
        calculation_result: {
          total_length: 100,
          bend_1_length: 20,
          straight_section: 60,
          bend_2_length: 20,
          bend_angles: [90, 90],
          bend_1_pos: [0, 0, 0],
          bend_2_pos: [80, 30, 5],
          confidence: 95,
          warnings: '',
          is_valid: true,
        },
        parameters: {
          outer_diameter: 20,
          wall_thickness: 2,
          bend_angle: 90,
          offset_distance: 30,
          bend_radius: 50,
          material: 'Steel',
        },
        format: 'dxf',
      });

      expect(response.status).toBe(200);
      expect(response.headers['content-type']).toContain('application/dxf');
      expect(response.text).toContain('SECTION');
    });

    test('должен экспортировать в PDF', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/export').send({
        calculation_result: {
          total_length: 100,
          bend_1_length: 20,
          straight_section: 60,
          bend_2_length: 20,
          bend_angles: [90, 90],
          bend_1_pos: [0, 0, 0],
          bend_2_pos: [80, 30, 5],
          confidence: 95,
          warnings: '',
          is_valid: true,
        },
        parameters: {
          outer_diameter: 20,
          wall_thickness: 2,
          bend_angle: 90,
          offset_distance: 30,
          bend_radius: 50,
          material: 'Steel',
        },
        format: 'pdf',
      });

      expect(response.status).toBe(200);
      expect(response.headers['content-type']).toContain('application/pdf');
      expect(response.text).toContain('PDF');
    });

    test('должен вернуть ошибку для неподдерживаемого формата', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/export').send({
        calculation_result: {},
        parameters: {},
        format: 'xlsx', // Неподдерживаемый формат
      });

      expect(response.status).toBe(400);
      expect(response.body.success).toBe(false);
      expect(response.body.error).toContain('Неподдерживаемый');
    });

    test('должен установить Content-Disposition header', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/export').send({
        calculation_result: {
          total_length: 100,
          bend_1_length: 20,
          straight_section: 60,
          bend_2_length: 20,
          bend_angles: [90, 90],
          bend_1_pos: [0, 0, 0],
          bend_2_pos: [80, 30, 5],
          confidence: 95,
          warnings: '',
          is_valid: true,
        },
        parameters: {
          outer_diameter: 20,
          wall_thickness: 2,
          bend_angle: 90,
          offset_distance: 30,
          bend_radius: 50,
          material: 'Steel',
        },
        format: 'csv',
      });

      expect(response.status).toBe(200);
      expect(response.headers['content-disposition']).toContain('attachment');
      expect(response.headers['content-disposition']).toContain('parallel-offset');
    });
  });

  // ======================== ТЕСТЫ ОБРАБОТКИ ОШИБОК ========================

  describe('Error Handling', () => {
    test('должен обработать missing параметры', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/calculate').send({
        outer_diameter: 20.0,
        // Остальные параметры отсутствуют
      });

      expect(response.status).toBe(400);
      expect(response.body.success).toBe(false);
    });

    test('должен обработать некорректный JSON', async () => {
      const response = await request(app)
        .post('/api/bendcore/parallel-offset/calculate')
        .set('Content-Type', 'application/json')
        .send('invalid json {');

      expect([400, 500]).toContain(response.status);
    });

    test('должен возвращать консистентную структуру ошибок', async () => {
      const response = await request(app).post('/api/bendcore/parallel-offset/calculate').send({
        outer_diameter: 2.0,
        wall_thickness: 2.0,
        bend_angle: 90.0,
        offset_distance: 30.0,
        bend_radius: 50.0,
        material: 'Steel',
      });

      expect(response.body.errors[0]).toHaveProperty('code');
      expect(response.body.errors[0]).toHaveProperty('message');
      expect(response.body.errors[0]).toHaveProperty('parameter');
    });
  });

  // ======================== ТЕСТЫ ПРОИЗВОДИТЕЛЬНОСТИ ========================

  describe('Performance', () => {
    test('должен ответить за < 1000ms', async () => {
      const start = Date.now();
      const response = await request(app).post('/api/bendcore/parallel-offset/calculate').send({
        outer_diameter: 20.0,
        wall_thickness: 2.0,
        bend_angle: 90.0,
        offset_distance: 30.0,
        bend_radius: 50.0,
        material: 'Steel',
      });
      const duration = Date.now() - start;

      expect(response.status).toBe(200);
      expect(duration).toBeLessThan(1000);
    });

    test('должен обработать несколько запросов параллельно', async () => {
      const requests = Array(5)
        .fill(null)
        .map(() =>
          request(app).post('/api/bendcore/parallel-offset/calculate').send({
            outer_diameter: 20.0,
            wall_thickness: 2.0,
            bend_angle: 90.0,
            offset_distance: 30.0,
            bend_radius: 50.0,
            material: 'Steel',
          })
        );

      const responses = await Promise.all(requests);

      for (const response of responses) {
        expect(response.status).toBe(200);
        expect(response.body.success).toBe(true);
      }
    });
  });
});
