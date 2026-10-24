// Тесты для API маршрутов чертежей (Q4 Task 1 Subtask 6)
import request from 'supertest';
import express, { Express } from 'express';
import drawingsRouter from '../drawings';

let app: Express;

beforeEach(() => {
  app = express();
  app.use(express.json());
  app.use('/api/drawings', drawingsRouter);
});

describe('Drawing API Endpoints', () => {
  const testDrawingId = 'test_drawing_123';

  // ===== GET /api/drawings/:id =====
  describe('GET /api/drawings/:id', () => {
    it('Должен загрузить чертёж перед получением', async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);

      const response = await request(app)
        .get(`/api/drawings/${testDrawingId}`)
        .expect(200);

      expect(response.body).toHaveProperty('drawing');
      expect(response.body.drawing.drawing_id).toBe(testDrawingId);
      expect(response.body.drawing.parameters.length).toBeGreaterThan(0);
    });

    it('Должен вернуть 404 для несуществующего чертежа', async () => {
      await request(app)
        .get('/api/drawings/nonexistent')
        .expect(404);
    });
  });

  // ===== POST /api/drawings/:id/load =====
  describe('POST /api/drawings/:id/load', () => {
    it('Должен загрузить новый чертёж с параметрами', async () => {
      const response = await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);

      expect(response.body.status).toBe('loaded');
      expect(response.body.drawing).toHaveProperty('parameters');
      expect(response.body.layers.length).toBe(3);
    });
  });

  // ===== PUT /api/drawings/:id/parameters/:param_id =====
  describe('PUT /api/drawings/:id/parameters/:param_id', () => {
    beforeEach(async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);
    });

    it('Должен обновить параметр в диапазоне', async () => {
      const response = await request(app)
        .put(`/api/drawings/${testDrawingId}/parameters/length_1`)
        .send({ current_value: 150 })
        .expect(200);

      expect(response.body.parameter.current_value).toBe(150);
    });

    it('Должен отклонить значение вне диапазона', async () => {
      await request(app)
        .put(`/api/drawings/${testDrawingId}/parameters/length_1`)
        .send({ current_value: 1000 })
        .expect(400);
    });

    it('Должен обновить формулу в параметре', async () => {
      const response = await request(app)
        .put(`/api/drawings/${testDrawingId}/parameters/length_1`)
        .send({
          formula_text: '$OD * 2',
          is_formula: true,
        })
        .expect(200);

      expect(response.body.parameter.is_formula).toBe(true);
      expect(response.body.parameter.formula_text).toBe('$OD * 2');
    });

    it('Должен вернуть 404 для несуществующего параметра', async () => {
      await request(app)
        .put(`/api/drawings/${testDrawingId}/parameters/nonexistent`)
        .send({ current_value: 100 })
        .expect(404);
    });
  });

  // ===== GET /api/drawings/:id/parameters =====
  describe('GET /api/drawings/:id/parameters', () => {
    beforeEach(async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);
    });

    it('Должен получить все параметры', async () => {
      const response = await request(app)
        .get(`/api/drawings/${testDrawingId}/parameters`)
        .expect(200);

      expect(response.body.parameters).toBeInstanceOf(Array);
      expect(response.body.parameters.length).toBeGreaterThan(0);
      expect(response.body.parameters[0]).toHaveProperty('id');
      expect(response.body.parameters[0]).toHaveProperty('current_value');
    });
  });

  // ===== POST /api/drawings/:id/layers =====
  describe('POST /api/drawings/:id/layers', () => {
    beforeEach(async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);
    });

    it('Должен создать новый слой', async () => {
      const response = await request(app)
        .post(`/api/drawings/${testDrawingId}/layers`)
        .send({ name: 'Новый слой' })
        .expect(200);

      expect(response.body.layer).toHaveProperty('id');
      expect(response.body.layer.name).toBe('Новый слой');
      expect(response.body.layer.visible).toBe(true);
      expect(response.body.layers.length).toBeGreaterThan(3);
    });

    it('Должен требовать имя слоя', async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/layers`)
        .send({})
        .expect(400);
    });
  });

  // ===== GET /api/drawings/:id/layers =====
  describe('GET /api/drawings/:id/layers', () => {
    beforeEach(async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);
    });

    it('Должен получить все слои', async () => {
      const response = await request(app)
        .get(`/api/drawings/${testDrawingId}/layers`)
        .expect(200);

      expect(response.body.layers).toBeInstanceOf(Array);
      expect(response.body.layers.length).toBe(3);
    });
  });

  // ===== PUT /api/drawings/:id/layers/:layer_id =====
  describe('PUT /api/drawings/:id/layers/:layer_id', () => {
    beforeEach(async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);
    });

    it('Должен обновить видимость слоя', async () => {
      const response = await request(app)
        .put(`/api/drawings/${testDrawingId}/layers/layer_contours`)
        .send({ visible: false })
        .expect(200);

      expect(response.body.layer.visible).toBe(false);
    });

    it('Должен обновить блокировку слоя', async () => {
      const response = await request(app)
        .put(`/api/drawings/${testDrawingId}/layers/layer_contours`)
        .send({ locked: true })
        .expect(200);

      expect(response.body.layer.locked).toBe(true);
    });

    it('Должен переименовать слой', async () => {
      const response = await request(app)
        .put(`/api/drawings/${testDrawingId}/layers/layer_contours`)
        .send({ name: 'Новое имя' })
        .expect(200);

      expect(response.body.layer.name).toBe('Новое имя');
    });
  });

  // ===== POST /api/drawings/:id/annotations =====
  describe('POST /api/drawings/:id/annotations', () => {
    beforeEach(async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);
    });

    it('Должен добавить новую аннотацию', async () => {
      const response = await request(app)
        .post(`/api/drawings/${testDrawingId}/annotations`)
        .send({
          type: 'note',
          text: 'Тестовая записка',
          x: 50,
          y: 75,
          layer_id: 'layer_annotations',
        })
        .expect(200);

      expect(response.body.annotation).toHaveProperty('id');
      expect(response.body.annotation.type).toBe('note');
      expect(response.body.annotation.text).toBe('Тестовая записка');
    });

    it('Должен требовать тип и текст', async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/annotations`)
        .send({})
        .expect(400);
    });
  });

  // ===== GET /api/drawings/:id/annotations =====
  describe('GET /api/drawings/:id/annotations', () => {
    beforeEach(async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);
    });

    it('Должен получить все аннотации', async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/annotations`)
        .send({
          type: 'dimension',
          text: '100 mm',
          layer_id: 'layer_dimensions',
        })
        .expect(200);

      const response = await request(app)
        .get(`/api/drawings/${testDrawingId}/annotations`)
        .expect(200);

      expect(response.body.annotations).toBeInstanceOf(Array);
      expect(response.body.annotations.length).toBeGreaterThan(0);
    });
  });

  // ===== POST /api/drawings/:id/undo =====
  describe('POST /api/drawings/:id/undo', () => {
    beforeEach(async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);

      // Выполнить несколько действий для отмены
      await request(app)
        .put(`/api/drawings/${testDrawingId}/parameters/length_1`)
        .send({ current_value: 150 })
        .expect(200);
    });

    it('Должен отменить последнее действие', async () => {
      const response = await request(app)
        .post(`/api/drawings/${testDrawingId}/undo`)
        .expect(200);

      expect(response.body.message).toBe('Action undone');
      expect(response.body).toHaveProperty('last_action');
    });

    it('Должен вернуть ошибку при попытке отмены когда нечего отменять', async () => {
      // Загрузить новый чертёж без действий
      const newDrawingId = 'empty_drawing';
      await request(app)
        .post(`/api/drawings/${newDrawingId}/load`)
        .expect(200);

      await request(app)
        .post(`/api/drawings/${newDrawingId}/undo`)
        .expect(400);
    });
  });

  // ===== GET /api/drawings/:id/history =====
  describe('GET /api/drawings/:id/history', () => {
    beforeEach(async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);

      await request(app)
        .put(`/api/drawings/${testDrawingId}/parameters/length_1`)
        .send({ current_value: 150 })
        .expect(200);

      await request(app)
        .post(`/api/drawings/${testDrawingId}/layers`)
        .send({ name: 'Новый слой' })
        .expect(200);
    });

    it('Должен получить всю историю действий', async () => {
      const response = await request(app)
        .get(`/api/drawings/${testDrawingId}/history`)
        .expect(200);

      expect(response.body.history).toBeInstanceOf(Array);
      expect(response.body.count).toBeGreaterThan(0);
      expect(response.body.history[0]).toHaveProperty('action');
      expect(response.body.history[0]).toHaveProperty('timestamp');
    });
  });

  // ===== DELETE /api/drawings/:id/history =====
  describe('DELETE /api/drawings/:id/history', () => {
    beforeEach(async () => {
      await request(app)
        .post(`/api/drawings/${testDrawingId}/load`)
        .expect(200);

      await request(app)
        .put(`/api/drawings/${testDrawingId}/parameters/length_1`)
        .send({ current_value: 150 })
        .expect(200);
    });

    it('Должен очистить историю', async () => {
      const response = await request(app)
        .delete(`/api/drawings/${testDrawingId}/history`)
        .expect(200);

      expect(response.body.message).toBe('History cleared');

      const historyResponse = await request(app)
        .get(`/api/drawings/${testDrawingId}/history`)
        .expect(200);

      expect(historyResponse.body.count).toBe(0);
    });
  });
});
