import React from 'react';
import { KPIMetrics } from './hooks/useAnalytics';
import './styles/AnalyticsDashboard.css';

interface KPICardsProps {
  metrics: KPIMetrics;
  isLoading?: boolean;
}

export const KPICards: React.FC<KPICardsProps> = ({ metrics, isLoading }) => {
  const formatNumber = (num: number): string => {
    if (num >= 1000000) return (num / 1000000).toFixed(1) + 'M';
    if (num >= 1000) return (num / 1000).toFixed(1) + 'K';
    return Math.round(num).toString();
  };

  const Card: React.FC<{
    title: string;
    value: string | number;
    unit?: string;
    trend?: number;
  }> = ({ title, value, unit, trend }) => (
    <div className="kpi-card" role="article" aria-label={`${title}: ${value}${unit || ''}`}>
      <h3 className="kpi-title">{title}</h3>
      <div className="kpi-value">
        <span>{value}</span>
        {unit && <span className="kpi-unit">{unit}</span>}
      </div>
      {trend !== undefined && (
        <div className={`kpi-trend ${trend >= 0 ? 'positive' : 'negative'}`}>
          {trend >= 0 ? '↑' : '↓'} {Math.abs(trend)}%
        </div>
      )}
    </div>
  );

  if (isLoading) {
    return (
      <div className="kpi-cards-container" role="region" aria-label="KPI metrics loading">
        <Card title="Sessions" value="..." />
        <Card title="Events" value="..." />
        <Card title="Conversion Rate" value="..." unit="%" />
        <Card title="Avg Duration" value="..." unit="s" />
      </div>
    );
  }

  return (
    <div className="kpi-cards-container" role="region" aria-label="KPI metrics">
      <Card
        title="Sessions"
        value={formatNumber(metrics.sessions)}
      />
      <Card
        title="Events"
        value={formatNumber(metrics.events)}
      />
      <Card
        title="Conversion Rate"
        value={metrics.conversionRate.toFixed(1)}
        unit="%"
      />
      <Card
        title="Avg Duration"
        value={Math.round(metrics.avgSessionDuration)}
        unit="s"
      />
    </div>
  );
};
