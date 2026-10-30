import React, { useState } from 'react';

/**
 * BendParametersEditor Component - GAP 0112
 * Detailed bend parameters editor
 */

export interface BendParametersEditorProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const BendParametersEditor: React.FC<BendParametersEditorProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [params, setParams] = useState({
    radius: 10,
    angle: 90,
    springback: 2,
    clearance: 0.5
  });

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="bend-params-overlay">
      <div className="bend-params-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="params-header">
          <h2>Bend Parameters</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="params-body">
          <div className="param-group">
            <label>Bend Radius:</label>
            <input
              type="number"
              value={params.radius}
              onChange={(e) => setParams({ ...params, radius: parseFloat(e.target.value) })}
            />
          </div>
          <div className="param-group">
            <label>Bend Angle:</label>
            <input
              type="number"
              value={params.angle}
              onChange={(e) => setParams({ ...params, angle: parseFloat(e.target.value) })}
            />
          </div>
          <div className="param-group">
            <label>Springback:</label>
            <input
              type="number"
              value={params.springback}
              onChange={(e) => setParams({ ...params, springback: parseFloat(e.target.value) })}
            />
          </div>
          <div className="param-group">
            <label>Clearance:</label>
            <input
              type="number"
              value={params.clearance}
              onChange={(e) => setParams({ ...params, clearance: parseFloat(e.target.value) })}
            />
          </div>
        </div>
        <div className="params-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default BendParametersEditor;
