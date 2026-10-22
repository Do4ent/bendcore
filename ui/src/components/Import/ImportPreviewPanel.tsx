import React, { useState } from 'react';

interface Segment {
  number: number;
  type: 'straight' | 'bend';
  parameter: string;
  value: number;
  unit: string;
}

interface ImportPreviewPanelProps {
  segments: Segment[];
  tubeId: string;
  outerDiameter: number;
  wallThickness: number;
  material: string;
  isLoading?: boolean;
}

/**
 * Import Preview Panel
 * Displays parsed tube geometry before confirming import
 *
 * Features:
 * - Segment table with type, parameters, values
 * - Tube summary (OD, wall, material)
 * - Path visualization (textual description)
 * - Statistics (total length, number of bends)
 * - Validation indicators
 */
export const ImportPreviewPanel: React.FC<ImportPreviewPanelProps> = ({
  segments,
  tubeId,
  outerDiameter,
  wallThickness,
  material,
  isLoading = false,
}) => {
  const [expandedSegment, setExpandedSegment] = useState<number | null>(null);

  // Calculate statistics
  const stats = {
    totalSegments: segments.length,
    straightCount: segments.filter((s) => s.type === 'straight').length,
    bendCount: segments.filter((s) => s.type === 'bend').length,
    totalLength: segments
      .filter((s) => s.type === 'straight' && s.parameter === 'length')
      .reduce((sum, s) => sum + s.value, 0),
  };

  // Validate segments
  const validateSegment = (segment: Segment): { valid: boolean; issues: string[] } => {
    const issues: string[] = [];

    if (segment.type === 'straight' && segment.value <= 0) {
      issues.push('Straight length must be > 0');
    }

    if (segment.type === 'bend') {
      if (segment.parameter === 'radius' && segment.value < outerDiameter) {
        issues.push(`Bend radius (${segment.value}) < outer diameter (${outerDiameter})`);
      }
      if (segment.parameter === 'angle' && (segment.value <= 0 || segment.value > 360)) {
        issues.push('Bend angle must be 0-360 degrees');
      }
    }

    return {
      valid: issues.length === 0,
      issues,
    };
  };

  // Get segment icon
  const getSegmentIcon = (type: 'straight' | 'bend'): string => {
    return type === 'straight' ? '—' : '⌢';
  };

  // Get segment status
  const getSegmentStatus = (segment: Segment): 'valid' | 'warning' | 'error' => {
    const { valid, issues } = validateSegment(segment);
    if (valid) return 'valid';
    return issues.some((i) => i.includes('must be')) ? 'error' : 'warning';
  };

  return (
    <div className="import-preview-panel">
      <div className="panel-header">
        <h3>Import Preview</h3>
        {isLoading && <span className="loading-indicator">Parsing...</span>}
      </div>

      {/* Tube Summary */}
      <div className="summary-section">
        <div className="summary-card">
          <div className="summary-title">Tube ID</div>
          <div className="summary-value">{tubeId}</div>
        </div>
        <div className="summary-card">
          <div className="summary-title">OD</div>
          <div className="summary-value">{outerDiameter} mm</div>
        </div>
        <div className="summary-card">
          <div className="summary-title">Wall</div>
          <div className="summary-value">{wallThickness} mm</div>
        </div>
        <div className="summary-card">
          <div className="summary-title">Material</div>
          <div className="summary-value">{material}</div>
        </div>
      </div>

      {/* Statistics */}
      <div className="stats-section">
        <div className="stats-row">
          <span className="stats-label">Total Segments:</span>
          <span className="stats-value">{stats.totalSegments}</span>
        </div>
        <div className="stats-row">
          <span className="stats-label">Straights:</span>
          <span className="stats-value">{stats.straightCount}</span>
        </div>
        <div className="stats-row">
          <span className="stats-label">Bends:</span>
          <span className="stats-value">{stats.bendCount}</span>
        </div>
        <div className="stats-row">
          <span className="stats-label">Total Length:</span>
          <span className="stats-value">{stats.totalLength.toFixed(1)} mm</span>
        </div>
      </div>

      {/* Segments Table */}
      <div className="segments-section">
        <div className="segments-header">
          <span className="col-number">#</span>
          <span className="col-type">Type</span>
          <span className="col-parameter">Parameter</span>
          <span className="col-value">Value</span>
          <span className="col-unit">Unit</span>
          <span className="col-status">Status</span>
        </div>

        <div className="segments-list">
          {segments.map((segment, index) => {
            const status = getSegmentStatus(segment);
            const { issues } = validateSegment(segment);

            return (
              <div key={index}>
                <div
                  className={`segment-row status-${status}`}
                  onClick={() =>
                    setExpandedSegment(expandedSegment === index ? null : index)
                  }
                >
                  <span className="col-number">{segment.number}</span>
                  <span className="col-type">
                    <span className="type-icon">{getSegmentIcon(segment.type)}</span>
                    {segment.type}
                  </span>
                  <span className="col-parameter">{segment.parameter}</span>
                  <span className="col-value">{segment.value.toFixed(2)}</span>
                  <span className="col-unit">{segment.unit}</span>
                  <span className="col-status">
                    {status === 'valid' && <span className="status-icon valid">✓</span>}
                    {status === 'warning' && <span className="status-icon warning">⚠</span>}
                    {status === 'error' && <span className="status-icon error">✕</span>}
                  </span>
                </div>

                {/* Expanded Issues */}
                {expandedSegment === index && issues.length > 0 && (
                  <div className="segment-issues">
                    {issues.map((issue, i) => (
                      <div key={i} className="issue-item">
                        <span className="issue-icon">•</span>
                        <span className="issue-text">{issue}</span>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            );
          })}
        </div>
      </div>

      {/* Path Description */}
      <div className="path-section">
        <div className="path-header">Path Description</div>
        <div className="path-description">
          {segments.length > 0 ? (
            <p>
              Path with {stats.straightCount} straight segments and {stats.bendCount} bends.
              Total axial length: ~{stats.totalLength.toFixed(0)} mm
            </p>
          ) : (
            <p>No segments to display</p>
          )}
        </div>
      </div>

      <style jsx>{`
        .import-preview-panel {
          display: flex;
          flex-direction: column;
          height: 100%;
          background: white;
          border: 1px solid #e0e0e0;
          border-radius: 6px;
          overflow: hidden;
        }

        .panel-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 12px 16px;
          border-bottom: 1px solid #e0e0e0;
          background: #f9f9f9;
        }

        .panel-header h3 {
          margin: 0;
          font-size: 14px;
          font-weight: 600;
          color: #333;
        }

        .loading-indicator {
          font-size: 11px;
          color: #2196F3;
          animation: pulse 1s infinite;
        }

        @keyframes pulse {
          0%, 100% {
            opacity: 1;
          }
          50% {
            opacity: 0.6;
          }
        }

        /* Summary Cards */
        .summary-section {
          display: grid;
          grid-template-columns: repeat(4, 1fr);
          gap: 8px;
          padding: 12px;
          border-bottom: 1px solid #e0e0e0;
          background: #fafafa;
        }

        .summary-card {
          padding: 8px;
          background: white;
          border: 1px solid #e0e0e0;
          border-radius: 4px;
          text-align: center;
        }

        .summary-title {
          font-size: 11px;
          color: #999;
          font-weight: 500;
          margin-bottom: 4px;
        }

        .summary-value {
          font-size: 13px;
          font-weight: 600;
          color: #333;
        }

        /* Statistics */
        .stats-section {
          padding: 12px;
          border-bottom: 1px solid #e0e0e0;
          display: flex;
          flex-wrap: wrap;
          gap: 16px;
        }

        .stats-row {
          display: flex;
          gap: 8px;
          font-size: 12px;
        }

        .stats-label {
          color: #666;
          font-weight: 500;
        }

        .stats-value {
          color: #333;
          font-weight: 600;
        }

        /* Segments */
        .segments-section {
          flex: 1;
          display: flex;
          flex-direction: column;
          overflow: hidden;
          border-bottom: 1px solid #e0e0e0;
        }

        .segments-header {
          display: grid;
          grid-template-columns: 40px 80px 100px 80px 50px 60px;
          gap: 0;
          padding: 8px 12px;
          background: #f5f5f5;
          border-bottom: 1px solid #ddd;
          font-size: 11px;
          font-weight: 600;
          color: #666;
          text-transform: uppercase;
          letter-spacing: 0.5px;
        }

        .segments-list {
          flex: 1;
          overflow-y: auto;
        }

        .segment-row {
          display: grid;
          grid-template-columns: 40px 80px 100px 80px 50px 60px;
          gap: 0;
          padding: 8px 12px;
          border-bottom: 1px solid #f0f0f0;
          font-size: 12px;
          color: #333;
          cursor: pointer;
          transition: background 0.15s;
        }

        .segment-row:hover {
          background: #f9f9f9;
        }

        .segment-row.status-valid {
          border-left: 3px solid #4caf50;
        }

        .segment-row.status-warning {
          border-left: 3px solid #ff9800;
        }

        .segment-row.status-error {
          border-left: 3px solid #f44336;
        }

        .col-number {
          text-align: center;
          font-weight: 600;
          color: #999;
        }

        .col-type {
          display: flex;
          align-items: center;
          gap: 4px;
        }

        .type-icon {
          font-weight: bold;
          color: #2196F3;
        }

        .col-parameter {
          font-size: 11px;
          color: #666;
        }

        .col-value {
          text-align: right;
          font-family: 'Courier New', monospace;
          font-weight: 500;
        }

        .col-unit {
          text-align: center;
          font-size: 11px;
          color: #999;
        }

        .col-status {
          display: flex;
          justify-content: center;
        }

        .status-icon {
          font-size: 14px;
        }

        .status-icon.valid {
          color: #4caf50;
        }

        .status-icon.warning {
          color: #ff9800;
        }

        .status-icon.error {
          color: #f44336;
        }

        /* Issues */
        .segment-issues {
          grid-column: 1 / -1;
          padding: 8px 12px;
          background: #fff8e1;
          border-bottom: 1px solid #ffe082;
          font-size: 12px;
        }

        .issue-item {
          display: flex;
          gap: 6px;
          margin-bottom: 4px;
        }

        .issue-item:last-child {
          margin-bottom: 0;
        }

        .issue-icon {
          color: #ff9800;
          font-weight: bold;
        }

        .issue-text {
          color: #e65100;
        }

        /* Path Description */
        .path-section {
          padding: 12px;
          border-top: 1px solid #e0e0e0;
        }

        .path-header {
          font-size: 12px;
          font-weight: 600;
          color: #666;
          margin-bottom: 6px;
        }

        .path-description {
          font-size: 12px;
          color: #333;
          line-height: 1.5;
        }

        .path-description p {
          margin: 0;
        }
      `}</style>
    </div>
  );
};
