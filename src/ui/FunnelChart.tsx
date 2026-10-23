import React from 'react';
import './styles/AnalyticsDashboard.css';

export interface FunnelStep {
  name: string;
  count: number;
  rate?: number;
}

interface FunnelChartProps {
  steps: FunnelStep[];
  title?: string;
}

export const FunnelChart: React.FC<FunnelChartProps> = ({
  steps,
  title = 'Conversion Funnel',
}) => {
  if (steps.length === 0) {
    return (
      <div className="funnel-chart" role="region" aria-label={title}>
        <h3>{title}</h3>
        <div className="funnel-empty">No funnel data available</div>
      </div>
    );
  }

  // Calculate conversion rates between steps
  const stepsWithRates = steps.map((step, index) => {
    const rate =
      index === 0 ? 100 : (step.count / steps[0].count) * 100;
    const dropoff = index === 0 ? 0 : steps[index - 1].count - step.count;
    return {
      ...step,
      rate,
      dropoff,
    };
  });

  const maxWidth = 100; // percentage

  return (
    <div className="funnel-chart" role="region" aria-label={title}>
      <h3>{title}</h3>
      <div className="funnel-container">
        {stepsWithRates.map((step, index) => (
          <div
            key={index}
            className="funnel-step"
            style={{
              width: `${step.rate}%`,
              marginLeft: `${(100 - step.rate) / 2}%`,
              marginRight: `${(100 - step.rate) / 2}%`,
            }}
          >
            <div className="step-label">
              <span className="step-name">{step.name}</span>
              <span className="step-metrics">
                {step.count.toLocaleString()} ({step.rate.toFixed(1)}%)
              </span>
            </div>
            {step.dropoff > 0 && (
              <div className="step-dropoff" aria-label={`Dropoff: ${step.dropoff}`}>
                ↓ {step.dropoff.toLocaleString()} ({(
                  (step.dropoff / steps[index - 1].count) * 100
                ).toFixed(1)}%)
              </div>
            )}
          </div>
        ))}
      </div>
    </div>
  );
};
