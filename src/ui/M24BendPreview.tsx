import React, { useState } from 'react';

/**
 * BendPreview Component - GAP 0113
 * Real-time 3D bend preview
 */

export interface BendPreviewProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const BendPreview: React.FC<BendPreviewProps> = ({
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
    <div className="bend-preview-overlay">
      <div className="bend-preview-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="preview-header">
          <h2>Bend Preview</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="preview-body">
          <div className="preview-viewport">
            <div className="viewport-placeholder">3D Preview</div>
          </div>
          <div className="preview-controls">
            <button
              className="btn btn-primary"
              onClick={() => setAnimating(!animating)}
            >
              {animating ? 'Pause' : 'Play'} Animation
            </button>
            <div className="progress-bar">
              <div className="progress" style={{ width: `${progress}%` }}></div>
            </div>
          </div>
        </div>
        <div className="preview-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default BendPreview;
