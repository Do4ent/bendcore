import React, { useState } from 'react';

export interface PathDefinitionProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const PathDefinition: React.FC<PathDefinitionProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [pathPoints, setPathPoints] = useState<any[]>([]);
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="path-definition-overlay">
      <div className="path-definition-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="path-header">
          <h2>Path Definition</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="path-body">
          <div className="path-points">
            <div className="section-title">Path Points ({pathPoints.length})</div>
            {pathPoints.length === 0 && <div className="empty-state">No points defined</div>}
          </div>
        </div>
        <div className="path-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default PathDefinition;
