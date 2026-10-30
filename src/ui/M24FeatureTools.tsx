import React, { useState } from 'react';

export interface FeatureToolsProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const FeatureTools: React.FC<FeatureToolsProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [activeFeature, setActiveFeature] = useState('');
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  const features = [
    { id: 'pad', name: 'Pad' },
    { id: 'pocket', name: 'Pocket' },
    { id: 'shell', name: 'Shell' },
    { id: 'fillet', name: 'Fillet' }
  ];

  if (!isOpen) return null;

  return (
    <div className="feature-tools-overlay">
      <div className="feature-tools-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="features-header">
          <h2>Feature Tools</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="features-body">
          <div className="features-grid">
            {features.map(f => (
              <button
                key={f.id}
                className={`feature-btn ${activeFeature === f.id ? 'active' : ''}`}
                onClick={() => setActiveFeature(f.id)}
              >
                {f.name}
              </button>
            ))}
          </div>
        </div>
        <div className="features-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default FeatureTools;
