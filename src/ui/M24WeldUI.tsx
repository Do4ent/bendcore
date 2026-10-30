import React, { useState } from 'react';

export interface WeldUIProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const WeldUI: React.FC<WeldUIProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [weldType, setWeldType] = useState('');
  const [weldPath, setWeldPath] = useState('');
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  const weldTypes = [
    { id: 'fillet', name: 'Fillet Weld' },
    { id: 'butt', name: 'Butt Weld' },
    { id: 'groove', name: 'Groove Weld' },
    { id: 'spot', name: 'Spot Weld' }
  ];

  if (!isOpen) return null;

  return (
    <div className="weld-ui-overlay">
      <div className="weld-ui-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="weld-header">
          <h2>Weld Configuration</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="weld-body">
          <div className="weld-type-selector">
            {weldTypes.map(wt => (
              <button
                key={wt.id}
                className={`weld-btn ${weldType === wt.id ? 'active' : ''}`}
                onClick={() => setWeldType(wt.id)}
              >
                {wt.name}
              </button>
            ))}
          </div>
        </div>
        <div className="weld-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default WeldUI;
