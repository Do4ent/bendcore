import React, { useState, useEffect } from 'react';
import { useAnalytics } from './hooks/useAnalytics';
import { KPICards } from './KPICards';
import { ExperimentCard } from './ExperimentCard';
import { EventTimeline } from './EventTimeline';
import { FunnelChart, FunnelStep } from './FunnelChart';
import './styles/AnalyticsDashboard.css';

export interface AnalyticsDashboardProps {
  startDate?: Date;
  endDate?: Date;
  refreshInterval?: number;  // seconds
  darkMode?: boolean;
}

export const AnalyticsDashboard: React.FC<AnalyticsDashboardProps> = ({
  startDate,
  endDate,
  refreshInterval = 30,
  darkMode = false,
}) => {
  const { kpiMetrics, experiments, events, loading, error, trackEvent, exportMetrics } =
    useAnalytics({
      refreshInterval,
      startDate,
      endDate,
    });

  const [showDatePicker, setShowDatePicker] = useState(false);
  const [selectedMetrics, setSelectedMetrics] = useState<string[]>([
    'sessions',
    'conversions',
  ]);

  // Calculate funnel steps from events
  const funnelSteps: FunnelStep[] = [
    {
      name: 'Import Started',
      count: events.filter((e) => e.name.includes('import_step_opened')).length,
    },
    {
      name: 'Import Completed',
      count: events.filter((e) => e.name.includes('import_step_success')).length,
    },
    {
      name: 'Design Created',
      count: events.filter((e) => e.name.includes('tube_created')).length,
    },
    {
      name: 'Design Exported',
      count: events.filter((e) => e.name.includes('export_')).length,
    },
  ];

  const handleExport = async () => {
    await exportMetrics(selectedMetrics);
  };

  return (
    <div
      className={`analytics-dashboard ${darkMode ? 'dark-mode' : 'light-mode'}`}
      role="main"
      aria-label="Analytics Dashboard"
    >
      {/* Header */}
      <header className="dashboard-header">
        <h1>Analytics Dashboard</h1>
        <div className="header-controls">
          <button
            className="btn btn-secondary"
            onClick={() => setShowDatePicker(!showDatePicker)}
            aria-label="Toggle date picker"
          >
            📅 Date Range
          </button>
          <button className="btn btn-secondary" aria-label="Refresh data">
            🔄 Refresh (Auto: {refreshInterval}s)
          </button>
          <button
            className="btn btn-primary"
            onClick={handleExport}
            aria-label="Export metrics to CSV"
          >
            ⬇️ Export CSV
          </button>
        </div>
      </header>

      {/* Error State */}
      {error && (
        <div className="error-banner" role="alert">
          ⚠️ {error}
        </div>
      )}

      {/* Date Picker */}
      {showDatePicker && (
        <div className="date-picker-panel" role="region" aria-label="Date range picker">
          <div className="date-inputs">
            <input
              type="date"
              defaultValue={startDate?.toISOString().split('T')[0]}
              aria-label="Start date"
            />
            <span>to</span>
            <input
              type="date"
              defaultValue={endDate?.toISOString().split('T')[0]}
              aria-label="End date"
            />
          </div>
          <button className="btn btn-primary" onClick={() => setShowDatePicker(false)}>
            Apply
          </button>
        </div>
      )}

      {/* KPI Section */}
      <section className="dashboard-section" aria-label="KPI metrics">
        <h2>Key Performance Indicators</h2>
        <KPICards metrics={kpiMetrics} isLoading={loading} />
      </section>

      {/* Funnel Section */}
      <section className="dashboard-section" aria-label="Conversion funnel">
        <h2>Conversion Funnel</h2>
        <FunnelChart
          steps={funnelSteps}
          title="Import → Design → Export Conversion Funnel"
        />
      </section>

      {/* Experiments Section */}
      <section className="dashboard-section" aria-label="Active A/B experiments">
        <h2>Active Experiments ({experiments.length})</h2>
        {experiments.length === 0 ? (
          <div className="section-empty">No active experiments</div>
        ) : (
          <div className="experiments-grid">
            {experiments.map((exp) => (
              <ExperimentCard key={exp.id} experiment={exp} />
            ))}
          </div>
        )}
      </section>

      {/* Feature Flags Section */}
      <section className="dashboard-section" aria-label="Feature flags">
        <h2>Feature Flags</h2>
        <div className="feature-flags-grid">
          <div className="feature-flag-item">
            <span className="flag-name">Enhanced Template Matcher</span>
            <span className="flag-status enabled">Enabled</span>
          </div>
          <div className="feature-flag-item">
            <span className="flag-name">Horizontal Search Filters</span>
            <span className="flag-status disabled">Disabled</span>
          </div>
          <div className="feature-flag-item">
            <span className="flag-name">Formula Autocomplete</span>
            <span className="flag-status enabled">Enabled</span>
          </div>
        </div>
      </section>

      {/* Event Timeline Section */}
      <section className="dashboard-section" aria-label="Event timeline">
        <h2>Recent Events</h2>
        <EventTimeline events={events} maxEvents={20} />
      </section>

      {/* Export Metrics Section */}
      <section className="dashboard-section" aria-label="Export options">
        <h2>Export Metrics</h2>
        <div className="export-options">
          <label className="checkbox-label">
            <input
              type="checkbox"
              checked={selectedMetrics.includes('sessions')}
              onChange={(e) =>
                setSelectedMetrics((prev) =>
                  e.target.checked
                    ? [...prev, 'sessions']
                    : prev.filter((m) => m !== 'sessions')
                )
              }
            />
            Sessions
          </label>
          <label className="checkbox-label">
            <input
              type="checkbox"
              checked={selectedMetrics.includes('events')}
              onChange={(e) =>
                setSelectedMetrics((prev) =>
                  e.target.checked
                    ? [...prev, 'events']
                    : prev.filter((m) => m !== 'events')
                )
              }
            />
            Events
          </label>
          <label className="checkbox-label">
            <input
              type="checkbox"
              checked={selectedMetrics.includes('conversions')}
              onChange={(e) =>
                setSelectedMetrics((prev) =>
                  e.target.checked
                    ? [...prev, 'conversions']
                    : prev.filter((m) => m !== 'conversions')
                )
              }
            />
            Conversions
          </label>
          <button className="btn btn-primary" onClick={handleExport}>
            Download CSV
          </button>
        </div>
      </section>

      {/* Loading State */}
      {loading && (
        <div className="loading-overlay" role="status" aria-label="Loading">
          <div className="spinner"></div>
          <p>Updating analytics...</p>
        </div>
      )}
    </div>
  );
};

export default AnalyticsDashboard;
