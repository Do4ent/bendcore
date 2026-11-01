/**
 * Feature Operation Panel Component
 */

import React, { useState } from 'react';
import './FeatureOperationPanel.css';

interface ExtrudeSettings {
  length: number;
  symmetric: boolean;
  taperAngle: number;
  createSolid: boolean;
}

interface RevolveSettings {
  angle: number;
  numSegments: number;
  createSolid: boolean;
}

interface FeatureOperationPanelProps {
  onExtrude?: (settings: ExtrudeSettings) => void;
  onRevolve?: (settings: RevolveSettings) => void;
  onCancel?: () => void;
}

export const FeatureOperationPanel: React.FC<FeatureOperationPanelProps> = ({
  onExtrude,
  onRevolve,
  onCancel
}) => {
  const [activeTab, setActiveTab] = useState<'extrude' | 'revolve'>('extrude');
  const [extrudeSettings, setExtrudeSettings] = useState<ExtrudeSettings>({
    length: 10,
    symmetric: false,
    taperAngle: 0,
    createSolid: true
  });
  const [revolveSettings, setRevolveSettings] = useState<RevolveSettings>({
    angle: 360,
    numSegments: 12,
    createSolid: true
  });

  return (
    <div className="feature-operation-panel">
      <div className="panel-header">
        <h2>Feature Operations</h2>
      </div>

      <div className="tabs">
        <button
          className={`tab ${activeTab === 'extrude' ? 'active' : ''}`}
          onClick={() => setActiveTab('extrude')}
        >
          Extrude
        </button>
        <button
          className={`tab ${activeTab === 'revolve' ? 'active' : ''}`}
          onClick={() => setActiveTab('revolve')}
        >
          Revolve
        </button>
      </div>

      {activeTab === 'extrude' && (
        <div className="tab-content">
          <div className="control-group">
            <label>Length (mm)</label>
            <input
              type="number"
              value={extrudeSettings.length}
              onChange={(e) => setExtrudeSettings({
                ...extrudeSettings,
                length: parseFloat(e.target.value) || 0
              })}
              min="0.1"
              step="0.5"
            />
          </div>

          <div className="control-group">
            <label>
              <input
                type="checkbox"
                checked={extrudeSettings.symmetric}
                onChange={(e) => setExtrudeSettings({
                  ...extrudeSettings,
                  symmetric: e.target.checked
                })}
              />
              Symmetric Extrusion
            </label>
          </div>

          <div className="control-group">
            <label>Taper Angle (°)</label>
            <input
              type="number"
              value={extrudeSettings.taperAngle}
              onChange={(e) => setExtrudeSettings({
                ...extrudeSettings,
                taperAngle: parseFloat(e.target.value) || 0
              })}
              min="-45"
              max="45"
              step="0.5"
            />
          </div>

          <div className="control-group">
            <label>
              <input
                type="checkbox"
                checked={extrudeSettings.createSolid}
                onChange={(e) => setExtrudeSettings({
                  ...extrudeSettings,
                  createSolid: e.target.checked
                })}
              />
              Create Solid
            </label>
          </div>

          <button
            className="apply-button"
            onClick={() => onExtrude?.(extrudeSettings)}
          >
            Apply Extrude
          </button>
        </div>
      )}

      {activeTab === 'revolve' && (
        <div className="tab-content">
          <div className="control-group">
            <label>Angle (°)</label>
            <input
              type="number"
              value={revolveSettings.angle}
              onChange={(e) => setRevolveSettings({
                ...revolveSettings,
                angle: parseFloat(e.target.value) || 0
              })}
              min="0"
              max="360"
              step="5"
            />
          </div>

          <div className="control-group">
            <label>Number of Segments</label>
            <input
              type="number"
              value={revolveSettings.numSegments}
              onChange={(e) => setRevolveSettings({
                ...revolveSettings,
                numSegments: parseInt(e.target.value) || 12
              })}
              min="3"
              max="128"
              step="1"
            />
          </div>

          <div className="control-group">
            <label>
              <input
                type="checkbox"
                checked={revolveSettings.createSolid}
                onChange={(e) => setRevolveSettings({
                  ...revolveSettings,
                  createSolid: e.target.checked
                })}
              />
              Create Solid
            </label>
          </div>

          <button
            className="apply-button"
            onClick={() => onRevolve?.(revolveSettings)}
          >
            Apply Revolve
          </button>
        </div>
      )}

      <button className="cancel-button" onClick={onCancel}>
        Cancel
      </button>
    </div>
  );
};

export default FeatureOperationPanel;
