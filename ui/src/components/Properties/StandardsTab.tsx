import React, { useState, useEffect } from 'react';
import { useStandards } from '../../hooks/useStandards';

interface StandardsTabProps {
  tubeId?: string;
  od?: number;
  wall?: number;
}

interface DisplayedStandard {
  name: string;
  status: 'compliant' | 'non-compliant' | 'unknown';
  score: number;
}

/**
 * Standards Tab Component
 * Part of Properties panel
 *
 * Features:
 * - Display applicable standards for current tube
 * - Show compliance status with scores
 * - Switch between available standards
 * - Detailed requirements view
 */
export const StandardsTab: React.FC<StandardsTabProps> = ({ tubeId, od, wall }) => {
  const standards = useStandards();
  const [selectedStandard, setSelectedStandard] = useState<string>('DIN 2391');
  const [displayedStandards, setDisplayedStandards] = useState<DisplayedStandard[]>([]);
  const [showDetails, setShowDetails] = useState(false);
  const [detailsData, setDetailsData] = useState<any>(null);

  // Load standards for current tube
  useEffect(() => {
    if (tubeId && od && wall) {
      standards.getStandardsForTube(tubeId, od, wall).then((stds) => {
        const displayed: DisplayedStandard[] = stds.map((std) => ({
          name: std.standardName,
          status: std.complianceStatus === 1 ? 'compliant' : 'non-compliant',
          score: std.complianceScore,
        }));
        setDisplayedStandards(displayed);
      });
    }
  }, [tubeId, od, wall, standards]);

  // Load standard details when selected changes
  useEffect(() => {
    standards.getStandardDetails(selectedStandard).then((details) => {
      setDetailsData(details);
    });
  }, [selectedStandard, standards]);

  const handleStandardChange = async (stdName: string) => {
    const success = await standards.setCurrentStandard(stdName);
    if (success) {
      setSelectedStandard(stdName);
    }
  };

  const getStatusColor = (status: string): string => {
    switch (status) {
      case 'compliant':
        return '#4CAF50'; // Green
      case 'non-compliant':
        return '#F44336'; // Red
      case 'unknown':
      default:
        return '#999'; // Gray
    }
  };

  const getStatusIcon = (status: string): string => {
    switch (status) {
      case 'compliant':
        return '✓';
      case 'non-compliant':
        return '✗';
      case 'unknown':
      default:
        return '?';
    }
  };

  return (
    <div className="standards-tab">
      <div className="standards-header">
        <h3>Standards Compliance</h3>
        {standards.ipcConnected && <span className="status-online">Online</span>}
      </div>

      {/* Current Standard Selector */}
      <div className="standard-selector">
        <label>Current Standard:</label>
        <select
          value={selectedStandard}
          onChange={(e) => handleStandardChange(e.target.value)}
          className="standard-dropdown"
        >
          {standards.availableStandards().map((std) => (
            <option key={std} value={std}>
              {std}
            </option>
          ))}
        </select>
      </div>

      {/* Standards List */}
      {displayedStandards.length > 0 ? (
        <div className="standards-list">
          <div className="standards-heading">Applicable Standards</div>
          {displayedStandards.map((std) => (
            <div
              key={std.name}
              className={`standard-item standard-${std.status}`}
              onClick={() => setSelectedStandard(std.name)}
            >
              <div className="standard-item-header">
                <span
                  className="status-icon"
                  style={{ color: getStatusColor(std.status) }}
                >
                  {getStatusIcon(std.status)}
                </span>
                <span className="standard-name">{std.name}</span>
              </div>
              <div className="standard-item-body">
                <div className="compliance-bar-container">
                  <div
                    className="compliance-bar"
                    style={{
                      width: `${std.score}%`,
                      backgroundColor: getStatusColor(std.status),
                    }}
                  />
                </div>
                <span className="compliance-score">{Math.round(std.score)}%</span>
              </div>
            </div>
          ))}
        </div>
      ) : (
        <div className="no-standards">No standards data available</div>
      )}

      {/* Details Section */}
      <div className="details-section">
        <button
          className="details-toggle"
          onClick={() => setShowDetails(!showDetails)}
        >
          {showDetails ? '▼' : '▶'} {selectedStandard} Details
        </button>

        {showDetails && detailsData && (
          <div className="details-content">
            {detailsData.description && (
              <div className="detail-item">
                <label>Description:</label>
                <p>{detailsData.description}</p>
              </div>
            )}

            {detailsData.minWall !== undefined && (
              <div className="detail-item">
                <label>Minimum Wall:</label>
                <span className="detail-value">{detailsData.minWall} mm</span>
              </div>
            )}

            {detailsData.maxOd !== undefined && (
              <div className="detail-item">
                <label>Maximum OD:</label>
                <span className="detail-value">{detailsData.maxOd} mm</span>
              </div>
            )}

            {/* Additional details */}
            {Object.entries(detailsData).map(([key, value]) => {
              if (['name', 'description', 'minWall', 'maxOd'].includes(key)) {
                return null;
              }
              return (
                <div key={key} className="detail-item">
                  <label>{key}:</label>
                  <span className="detail-value">{String(value)}</span>
                </div>
              );
            })}
          </div>
        )}
      </div>

      <style jsx>{`
        .standards-tab {
          display: flex;
          flex-direction: column;
          height: 100%;
          background: white;
          overflow-y: auto;
        }

        .standards-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 12px 16px;
          border-bottom: 1px solid #e0e0e0;
        }

        .standards-header h3 {
          margin: 0;
          font-size: 16px;
          font-weight: 600;
        }

        .status-online {
          font-size: 11px;
          padding: 2px 6px;
          background: #4CAF50;
          color: white;
          border-radius: 3px;
          font-weight: 500;
        }

        .standard-selector {
          padding: 12px 16px;
          border-bottom: 1px solid #f0f0f0;
        }

        .standard-selector label {
          display: block;
          font-size: 12px;
          font-weight: 500;
          margin-bottom: 6px;
          color: #666;
        }

        .standard-dropdown {
          width: 100%;
          padding: 8px 10px;
          border: 1px solid #ddd;
          border-radius: 4px;
          font-size: 14px;
        }

        .standard-dropdown:focus {
          outline: none;
          border-color: #2196F3;
          box-shadow: 0 0 0 2px rgba(33, 150, 243, 0.1);
        }

        .standards-list {
          flex: 1;
          padding: 8px 12px;
          overflow-y: auto;
        }

        .standards-heading {
          font-size: 12px;
          font-weight: 600;
          color: #666;
          padding: 8px 4px;
          text-transform: uppercase;
        }

        .standard-item {
          padding: 10px;
          margin: 6px 0;
          border: 1px solid #e0e0e0;
          border-radius: 4px;
          cursor: pointer;
          transition: all 0.2s;
        }

        .standard-item:hover {
          background: #f9f9f9;
        }

        .standard-item-compliant {
          border-left: 4px solid #4CAF50;
        }

        .standard-item-non-compliant {
          border-left: 4px solid #F44336;
        }

        .standard-item-header {
          display: flex;
          align-items: center;
          gap: 8px;
          margin-bottom: 8px;
        }

        .status-icon {
          font-weight: bold;
          font-size: 14px;
          width: 20px;
          text-align: center;
        }

        .standard-name {
          font-weight: 500;
          font-size: 14px;
          color: #333;
        }

        .standard-item-body {
          display: flex;
          align-items: center;
          gap: 8px;
        }

        .compliance-bar-container {
          flex: 1;
          height: 6px;
          background: #f0f0f0;
          border-radius: 3px;
          overflow: hidden;
        }

        .compliance-bar {
          height: 100%;
          border-radius: 3px;
          transition: width 0.3s;
        }

        .compliance-score {
          font-size: 12px;
          color: #666;
          font-weight: 500;
          min-width: 35px;
          text-align: right;
        }

        .no-standards {
          padding: 20px;
          text-align: center;
          color: #999;
          font-size: 14px;
        }

        .details-section {
          border-top: 1px solid #e0e0e0;
          padding: 8px 12px;
        }

        .details-toggle {
          width: 100%;
          padding: 8px 10px;
          background: #f9f9f9;
          border: 1px solid #ddd;
          border-radius: 4px;
          font-size: 13px;
          font-weight: 500;
          cursor: pointer;
          text-align: left;
          transition: all 0.2s;
        }

        .details-toggle:hover {
          background: #f0f0f0;
        }

        .details-content {
          padding: 8px 0;
          margin-top: 8px;
          border-top: 1px solid #f0f0f0;
        }

        .detail-item {
          padding: 6px 0;
          font-size: 13px;
        }

        .detail-item label {
          font-weight: 500;
          color: #666;
          display: inline-block;
          width: 100px;
        }

        .detail-value {
          color: #333;
          font-family: monospace;
          font-size: 12px;
        }

        .detail-item p {
          margin: 4px 0 0 0;
          font-size: 12px;
          color: #666;
          line-height: 1.4;
        }
      `}</style>
    </div>
  );
};
