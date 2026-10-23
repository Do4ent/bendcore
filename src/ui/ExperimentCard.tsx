import React from 'react';
import { Experiment } from './hooks/useAnalytics';
import './styles/AnalyticsDashboard.css';

interface ExperimentCardProps {
  experiment: Experiment;
}

export const ExperimentCard: React.FC<ExperimentCardProps> = ({ experiment }) => {
  const getStatusColor = (status: string): string => {
    switch (status) {
      case 'running':
        return 'status-running';
      case 'completed':
        return 'status-completed';
      case 'paused':
        return 'status-paused';
      default:
        return '';
    }
  };

  const getSignificanceColor = (significance: number): string => {
    if (significance >= 0.95) return 'significant';
    if (significance >= 0.8) return 'moderate';
    return 'insignificant';
  };

  const formatDate = (dateStr: string): string => {
    return new Date(dateStr).toLocaleDateString();
  };

  const winner =
    experiment.treatmentConversion > experiment.controlConversion ? 'treatment' : 'control';

  return (
    <div
      className="experiment-card"
      role="article"
      aria-label={`Experiment: ${experiment.name}`}
    >
      <div className="experiment-header">
        <h3 className="experiment-name">{experiment.name}</h3>
        <span className={`experiment-status ${getStatusColor(experiment.status)}`}>
          {experiment.status.charAt(0).toUpperCase() + experiment.status.slice(1)}
        </span>
      </div>

      <div className="experiment-dates">
        <span>{formatDate(experiment.startDate)}</span>
        <span>→</span>
        <span>{experiment.endDate ? formatDate(experiment.endDate) : 'Ongoing'}</span>
      </div>

      <div className="experiment-variants">
        <div className="variant control">
          <h4>Control</h4>
          <div className="conversion-rate">
            {experiment.controlConversion.toFixed(1)}%
          </div>
          <div className="sample-size">n = {experiment.sampleSize.toLocaleString()}</div>
        </div>

        <div className="variant treatment">
          <h4>Treatment</h4>
          <div className="conversion-rate">
            {experiment.treatmentConversion.toFixed(1)}%
          </div>
          <div className="sample-size">n = {experiment.sampleSize.toLocaleString()}</div>
        </div>
      </div>

      <div className="experiment-significance">
        <div>Statistical Significance</div>
        <div className={`significance-badge ${getSignificanceColor(experiment.significance)}`}>
          {(experiment.significance * 100).toFixed(0)}%
        </div>
      </div>

      {experiment.treatmentConversion > experiment.controlConversion && (
        <div className="experiment-winner">
          🎉 Treatment is {(
            ((experiment.treatmentConversion - experiment.controlConversion) /
              experiment.controlConversion) *
            100
          ).toFixed(1)}% better
        </div>
      )}
    </div>
  );
};
