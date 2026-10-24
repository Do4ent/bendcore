import express, { Request, Response } from 'express';
import { v4 as uuidv4 } from 'uuid';

const router = express.Router();

// In-memory storage for demo (would use database in production)
let eventQueue: any[] = [];
let kpiCache: any = null;
let lastKPIUpdate = 0;

/**
 * POST /api/events
 * Track a single event
 */
router.post('/events', (req: Request, res: Response) => {
  try {
    const { event_name, category, properties } = req.body;

    if (!event_name) {
      return res.status(400).json({ error: 'event_name is required' });
    }

    const event = {
      id: uuidv4(),
      event_name,
      category: category || 'SYSTEM',
      properties: properties || {},
      user_id: req.headers['x-user-id'] || 'anonymous',
      session_id: req.headers['x-session-id'] || 'session_' + uuidv4(),
      timestamp: new Date().toISOString(),
    };

    eventQueue.push(event);

    // Keep only last 1000 events in memory
    if (eventQueue.length > 1000) {
      eventQueue = eventQueue.slice(-1000);
    }

    res.json({ success: true, event_id: event.id });
  } catch (error) {
    res.status(500).json({ error: 'Failed to track event' });
  }
});

/**
 * GET /api/experiments
 * Get active A/B experiments
 */
router.get('/experiments', (req: Request, res: Response) => {
  try {
    const experiments = [
      {
        id: 'template_match_v1',
        name: 'Template Match UI',
        status: 'running',
        startDate: '2026-10-15',
        controlConversion: 10.5,
        treatmentConversion: 12.3,
        significance: 0.92,
        sampleSize: 5234,
      },
      {
        id: 'search_filter_v1',
        name: 'Search Filter Layout',
        status: 'running',
        startDate: '2026-10-16',
        controlConversion: 15.2,
        treatmentConversion: 14.8,
        significance: 0.65,
        sampleSize: 4891,
      },
      {
        id: 'formula_editor_v1',
        name: 'Formula Editor UX',
        status: 'running',
        startDate: '2026-10-17',
        controlConversion: 8.9,
        treatmentConversion: 11.7,
        significance: 0.88,
        sampleSize: 5102,
      },
    ];

    res.json({ experiments, total: experiments.length });
  } catch (error) {
    res.status(500).json({ error: 'Failed to fetch experiments' });
  }
});

/**
 * GET /api/analytics/kpi
 * Get KPI metrics
 */
router.get('/analytics/kpi', (req: Request, res: Response) => {
  try {
    const { start_date, end_date } = req.query;

    // Calculate metrics from events
    const now = Date.now();
    const oneHourAgo = now - 60 * 60 * 1000;

    // Filter events by date range if provided
    let filteredEvents = eventQueue;
    if (start_date && end_date) {
      const startTime = new Date(start_date as string).getTime();
      const endTime = new Date(end_date as string).getTime();
      filteredEvents = eventQueue.filter((e) => {
        const eventTime = new Date(e.timestamp).getTime();
        return eventTime >= startTime && eventTime <= endTime;
      });
    }

    // Calculate KPIs
    const uniqueSessions = new Set(filteredEvents.map((e) => e.session_id)).size;
    const totalEvents = filteredEvents.length;

    // Conversion funnel: import success / import opened
    const importOpened = filteredEvents.filter((e) =>
      e.event_name.includes('import_step_opened')
    ).length;
    const importSuccess = filteredEvents.filter((e) =>
      e.event_name.includes('import_step_success')
    ).length;
    const tubeCreated = filteredEvents.filter((e) =>
      e.event_name.includes('tube_created')
    ).length;
    const exported = filteredEvents.filter((e) =>
      e.event_name.includes('export_')
    ).length;

    const conversionRate =
      importOpened > 0 ? (importSuccess / importOpened) * 100 : 0;

    // Average session duration (in seconds)
    const avgSessionDuration = 245; // Mock value

    const kpiData = {
      sessions: uniqueSessions,
      events: totalEvents,
      conversionRate: parseFloat(conversionRate.toFixed(2)),
      avgSessionDuration,
      funnelMetrics: {
        importOpened,
        importSuccess,
        tubeCreated,
        exported,
      },
    };

    res.json(kpiData);
  } catch (error) {
    res.status(500).json({ error: 'Failed to fetch KPI metrics' });
  }
});

/**
 * GET /api/analytics/events
 * Get event timeline
 */
router.get('/analytics/events', (req: Request, res: Response) => {
  try {
    const limit = parseInt(req.query.limit as string) || 20;

    // Return most recent events
    const recentEvents = eventQueue
      .slice(-limit)
      .reverse()
      .map((e) => ({
        id: e.id,
        name: e.event_name,
        userId: e.user_id,
        timestamp: e.timestamp,
        properties: e.properties,
        variant: e.properties.variant || undefined,
      }));

    res.json({ events: recentEvents, total_count: eventQueue.length });
  } catch (error) {
    res.status(500).json({ error: 'Failed to fetch events' });
  }
});

/**
 * POST /api/analytics/export
 * Export metrics to CSV
 */
router.post('/analytics/export', (req: Request, res: Response) => {
  try {
    const { start_date, end_date, metrics } = req.body;

    // Build CSV header
    const csvHeaders = ['timestamp', 'event_name', 'user_id', 'session_id', 'properties'];
    let csvContent = csvHeaders.join(',') + '\n';

    // Filter events by date range
    let filteredEvents = eventQueue;
    if (start_date && end_date) {
      const startTime = new Date(start_date).getTime();
      const endTime = new Date(end_date).getTime();
      filteredEvents = eventQueue.filter((e) => {
        const eventTime = new Date(e.timestamp).getTime();
        return eventTime >= startTime && eventTime <= endTime;
      });
    }

    // Add event rows to CSV
    filteredEvents.forEach((e) => {
      const row = [
        e.timestamp,
        e.event_name,
        e.user_id,
        e.session_id,
        JSON.stringify(e.properties),
      ]
        .map((v) => `"${String(v).replace(/"/g, '""')}"`)
        .join(',');
      csvContent += row + '\n';
    });

    // Send CSV file
    res.setHeader('Content-Type', 'text/csv; charset=utf-8');
    res.setHeader('Content-Disposition', 'attachment; filename="analytics-export.csv"');
    res.send(csvContent);
  } catch (error) {
    res.status(500).json({ error: 'Failed to export metrics' });
  }
});

/**
 * GET /api/analytics/summary
 * Get analytics summary
 */
router.get('/analytics/summary', (req: Request, res: Response) => {
  try {
    const summary = {
      totalEvents: eventQueue.length,
      uniqueSessions: new Set(eventQueue.map((e) => e.session_id)).size,
      eventsByCategory: {
        IMPORT: eventQueue.filter((e) => e.category === 'IMPORT').length,
        TEMPLATE: eventQueue.filter((e) => e.category === 'TEMPLATE').length,
        SEARCH: eventQueue.filter((e) => e.category === 'SEARCH').length,
        FORMULA: eventQueue.filter((e) => e.category === 'FORMULA').length,
        TUBE_DESIGN: eventQueue.filter((e) => e.category === 'TUBE_DESIGN').length,
        EXPORT: eventQueue.filter((e) => e.category === 'EXPORT').length,
        SYSTEM: eventQueue.filter((e) => e.category === 'SYSTEM').length,
      },
      lastEventTime: eventQueue.length > 0 ? eventQueue[eventQueue.length - 1].timestamp : null,
    };

    res.json(summary);
  } catch (error) {
    res.status(500).json({ error: 'Failed to fetch summary' });
  }
});

export default router;
