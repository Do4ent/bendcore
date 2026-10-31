import { useEffect, useState, useCallback } from 'react';

export interface KPIMetrics {
  sessions: number;
  events: number;
  conversionRate: number;
  avgSessionDuration: number;
}

export interface Experiment {
  id: string;
  name: string;
  status: 'running' | 'completed' | 'paused';
  startDate: string;
  endDate?: string;
  controlConversion: number;
  treatmentConversion: number;
  significance: number;
  sampleSize: number;
}

export interface AnalyticsEvent {
  id: string;
  name: string;
  userId: string;
  timestamp: string;
  properties: Record<string, string>;
  variant?: 'control' | 'treatment';
}

export interface UseAnalyticsOptions {
  refreshInterval?: number;  // seconds
  startDate?: Date;
  endDate?: Date;
}

const KPI_FIELDS: Array<keyof KPIMetrics> = ['sessions', 'events', 'conversionRate', 'avgSessionDuration'];

/**
 * Merge a KPI API payload into the current metrics, keeping the previous value
 * for any field that is missing or not a finite number, so a partial or
 * malformed response cannot leave non-numeric values in the KPI cards.
 */
export function mergeKPIMetrics(previous: KPIMetrics, payload: unknown): KPIMetrics {
  const source = (payload && typeof payload === 'object' ? payload : {}) as Record<string, unknown>;
  const next = { ...previous };
  KPI_FIELDS.forEach((field) => {
    const value = source[field];
    if (typeof value === 'number' && Number.isFinite(value)) {
      next[field] = value;
    }
  });
  return next;
}

export function useAnalytics(options: UseAnalyticsOptions = {}) {
  const { refreshInterval = 30, startDate, endDate } = options;

  const [kpiMetrics, setKpiMetrics] = useState<KPIMetrics>({
    sessions: 0,
    events: 0,
    conversionRate: 0,
    avgSessionDuration: 0,
  });

  const [experiments, setExperiments] = useState<Experiment[]>([]);
  const [events, setEvents] = useState<AnalyticsEvent[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const fetchKPIMetrics = useCallback(async () => {
    try {
      setLoading(true);
      const response = await fetch('/api/analytics/kpi', {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' },
      });

      if (!response.ok) throw new Error('Failed to fetch KPI metrics');

      const data: unknown = await response.json();
      setKpiMetrics((previous) => mergeKPIMetrics(previous, data));
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setLoading(false);
    }
  }, []);

  const fetchExperiments = useCallback(async () => {
    try {
      const response = await fetch('/api/experiments', {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' },
      });

      if (!response.ok) throw new Error('Failed to fetch experiments');

      const data = await response.json();
      setExperiments(data.experiments || []);
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    }
  }, []);

  const fetchEventTimeline = useCallback(async () => {
    try {
      const response = await fetch('/api/analytics/events', {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' },
      });

      if (!response.ok) throw new Error('Failed to fetch events');

      const data = await response.json();
      setEvents(data.events || []);
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    }
  }, []);

  const trackEvent = useCallback(async (eventName: string, properties?: Record<string, string>) => {
    try {
      const response = await fetch('/api/events', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ event_name: eventName, properties }),
      });

      if (!response.ok) throw new Error('Failed to track event');

      return await response.json();
    } catch (err) {
      console.error('Event tracking error:', err);
      return null;
    }
  }, []);

  const exportMetrics = useCallback(async (metrics: string[]) => {
    try {
      const response = await fetch('/api/analytics/export', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          start_date: startDate?.toISOString(),
          end_date: endDate?.toISOString(),
          metrics,
        }),
      });

      if (!response.ok) throw new Error('Failed to export metrics');

      // Handle CSV download
      const blob = await response.blob();
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'analytics-export.csv';
      a.click();
      window.URL.revokeObjectURL(url);
    } catch (err) {
      console.error('Export error:', err);
    }
  }, [startDate, endDate]);

  // Auto-refresh data
  useEffect(() => {
    fetchKPIMetrics();
    fetchExperiments();
    fetchEventTimeline();

    const interval = setInterval(() => {
      fetchKPIMetrics();
      fetchExperiments();
      fetchEventTimeline();
    }, refreshInterval * 1000);

    return () => clearInterval(interval);
  }, [refreshInterval, fetchKPIMetrics, fetchExperiments, fetchEventTimeline]);

  return {
    kpiMetrics,
    experiments,
    events,
    loading,
    error,
    trackEvent,
    exportMetrics,
    refresh: async () => {
      await fetchKPIMetrics();
      await fetchExperiments();
      await fetchEventTimeline();
    },
  };
}
