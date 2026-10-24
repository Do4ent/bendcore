import request from 'supertest';
import express from 'express';
import analyticsRouter from '../analytics';

describe('Analytics API Routes', () => {
  let app: express.Application;

  beforeEach(() => {
    app = express();
    app.use(express.json());
    app.use('/api', analyticsRouter);
  });

  describe('POST /api/events', () => {
    test('should track event successfully', async () => {
      const response = await request(app).post('/api/events').send({
        event_name: 'test_event',
        category: 'IMPORT',
        properties: { file_size: '1024' },
      });

      expect(response.status).toBe(200);
      expect(response.body.success).toBe(true);
      expect(response.body.event_id).toBeDefined();
    });

    test('should require event_name', async () => {
      const response = await request(app).post('/api/events').send({
        category: 'IMPORT',
      });

      expect(response.status).toBe(400);
      expect(response.body.error).toBeDefined();
    });

    test('should handle missing properties', async () => {
      const response = await request(app).post('/api/events').send({
        event_name: 'test_event',
      });

      expect(response.status).toBe(200);
      expect(response.body.success).toBe(true);
    });
  });

  describe('GET /api/experiments', () => {
    test('should return active experiments', async () => {
      const response = await request(app).get('/api/experiments');

      expect(response.status).toBe(200);
      expect(response.body.experiments).toBeDefined();
      expect(Array.isArray(response.body.experiments)).toBe(true);
      expect(response.body.experiments.length).toBeGreaterThan(0);
    });

    test('should return experiment details', async () => {
      const response = await request(app).get('/api/experiments');

      const exp = response.body.experiments[0];
      expect(exp.id).toBeDefined();
      expect(exp.name).toBeDefined();
      expect(exp.status).toBeDefined();
      expect(exp.controlConversion).toBeDefined();
      expect(exp.treatmentConversion).toBeDefined();
      expect(exp.significance).toBeDefined();
      expect(exp.sampleSize).toBeDefined();
    });
  });

  describe('GET /api/analytics/kpi', () => {
    test('should return KPI metrics', async () => {
      const response = await request(app).get('/api/analytics/kpi');

      expect(response.status).toBe(200);
      expect(response.body.sessions).toBeDefined();
      expect(response.body.events).toBeDefined();
      expect(response.body.conversionRate).toBeDefined();
      expect(response.body.avgSessionDuration).toBeDefined();
    });

    test('should accept date range parameters', async () => {
      const response = await request(app)
        .get('/api/analytics/kpi')
        .query({
          start_date: '2026-10-01',
          end_date: '2026-10-31',
        });

      expect(response.status).toBe(200);
      expect(response.body.sessions).toBeDefined();
    });

    test('should return funnel metrics', async () => {
      const response = await request(app).get('/api/analytics/kpi');

      expect(response.body.funnelMetrics).toBeDefined();
      expect(response.body.funnelMetrics.importOpened).toBeDefined();
      expect(response.body.funnelMetrics.importSuccess).toBeDefined();
      expect(response.body.funnelMetrics.tubeCreated).toBeDefined();
      expect(response.body.funnelMetrics.exported).toBeDefined();
    });
  });

  describe('GET /api/analytics/events', () => {
    test('should return event timeline', async () => {
      const response = await request(app).get('/api/analytics/events');

      expect(response.status).toBe(200);
      expect(response.body.events).toBeDefined();
      expect(Array.isArray(response.body.events)).toBe(true);
    });

    test('should respect limit parameter', async () => {
      const response = await request(app)
        .get('/api/analytics/events')
        .query({ limit: 5 });

      expect(response.status).toBe(200);
      expect(response.body.events.length).toBeLessThanOrEqual(5);
    });

    test('should return event properties', async () => {
      // First, track an event
      await request(app).post('/api/events').send({
        event_name: 'test_event',
        properties: { key: 'value' },
      });

      // Then get events
      const response = await request(app).get('/api/analytics/events');

      expect(response.status).toBe(200);
      if (response.body.events.length > 0) {
        const event = response.body.events[0];
        expect(event.id).toBeDefined();
        expect(event.name).toBeDefined();
        expect(event.userId).toBeDefined();
        expect(event.timestamp).toBeDefined();
      }
    });
  });

  describe('POST /api/analytics/export', () => {
    test('should export metrics to CSV', async () => {
      const response = await request(app).post('/api/analytics/export').send({
        start_date: '2026-10-01',
        end_date: '2026-10-31',
        metrics: ['sessions', 'conversions'],
      });

      expect(response.status).toBe(200);
      expect(response.headers['content-type']).toContain('text/csv');
      expect(response.headers['content-disposition']).toContain('attachment');
    });

    test('should include CSV headers', async () => {
      const response = await request(app).post('/api/analytics/export').send({
        start_date: '2026-10-01',
        end_date: '2026-10-31',
        metrics: ['sessions'],
      });

      expect(response.text).toContain('timestamp');
      expect(response.text).toContain('event_name');
      expect(response.text).toContain('user_id');
    });
  });

  describe('GET /api/analytics/summary', () => {
    test('should return analytics summary', async () => {
      const response = await request(app).get('/api/analytics/summary');

      expect(response.status).toBe(200);
      expect(response.body.totalEvents).toBeDefined();
      expect(response.body.uniqueSessions).toBeDefined();
      expect(response.body.eventsByCategory).toBeDefined();
    });

    test('should include all event categories', async () => {
      const response = await request(app).get('/api/analytics/summary');

      const categories = response.body.eventsByCategory;
      expect(categories.IMPORT).toBeDefined();
      expect(categories.TEMPLATE).toBeDefined();
      expect(categories.SEARCH).toBeDefined();
      expect(categories.FORMULA).toBeDefined();
      expect(categories.TUBE_DESIGN).toBeDefined();
      expect(categories.EXPORT).toBeDefined();
      expect(categories.SYSTEM).toBeDefined();
    });
  });
});
