import React, { useState } from 'react';

export interface SnapGridProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const SnapGrid: React.FC<SnapGridProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [gridSize, setGridSize] = useState(10);
  const [snapTypes, setSnapTypes] = useState({
    point: true,
    grid: true,
    line: false,
    tangent: false
  });

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="snap-grid-overlay">
      <div className="snap-grid-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="snap-header">
          <h2>Snap & Grid Settings</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="snap-body">
          <div className="grid-size-section">
            <label>Grid Size:</label>
            <input
              type="number"
              value={gridSize}
              onChange={(e) => setGridSize(parseInt(e.target.value))}
              min="1"
              max="100"
            />
          </div>
          <div className="snap-types-section">
            <div className="snap-type">
              <input
                type="checkbox"
                checked={snapTypes.point}
                onChange={(e) => setSnapTypes({ ...snapTypes, point: e.target.checked })}
              />
              <label>Snap to Points</label>
            </div>
            <div className="snap-type">
              <input
                type="checkbox"
                checked={snapTypes.grid}
                onChange={(e) => setSnapTypes({ ...snapTypes, grid: e.target.checked })}
              />
              <label>Snap to Grid</label>
            </div>
          </div>
        </div>
        <div className="snap-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default SnapGrid;
