import React, { useState } from 'react';

export interface ExplodedViewProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const ExplodedView: React.FC<ExplodedViewProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [animating, setAnimating] = useState(false);
  const [progress, setProgress] = useState(0);
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="exploded-view-overlay">
      <div className="exploded-view-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="ev-header">
          <h2>Exploded View</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="ev-body">
          <div className="exploded-viewport">
            <div className="viewport-placeholder">Exploded View 3D</div>
          </div>
          <div className="exploded-controls">
            <button className="btn btn-primary" onClick={() => setAnimating(!animating)}>
              {animating ? 'Pause' : 'Play'}
            </button>
          </div>
        </div>
        <div className="ev-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default ExplodedView;
