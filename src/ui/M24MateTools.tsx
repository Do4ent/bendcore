import React, { useState } from 'react';

/**
 * MateTools Component - GAP 0107
 * Assembly mate tools (coincident, parallel, perpendicular, distance)
 */

export interface MateToolsProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const MateTools: React.FC<MateToolsProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [mateType, setMateType] = useState('');
  const [mates, setMates] = useState<any[]>([]);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  const mateTypes = [
    { id: 'coincident', name: 'Coincident Mate' },
    { id: 'parallel', name: 'Parallel Mate' },
    { id: 'perpendicular', name: 'Perpendicular Mate' },
    { id: 'distance', name: 'Distance Mate' },
    { id: 'angle', name: 'Angle Mate' },
    { id: 'tangent', name: 'Tangent Mate' },
    { id: 'symmetric', name: 'Symmetric Mate' },
    { id: 'profile', name: 'Profile Mate' },
    { id: 'path', name: 'Path Mate' }
  ];

  const handleCreateMate = (typeId: string) => {
    const newMate = { id: `mate_${Date.now()}`, type: typeId };
    setMates([...mates, newMate]);
    setMateType('');
  };

  if (!isOpen) return null;

  return (
    <div className="mate-tools-overlay">
      <div className="mate-tools-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="mate-header">
          <h2>Mate Tools</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="mate-body">
          <div className="mate-types">
            {mateTypes.map(mate => (
              <button
                key={mate.id}
                className={`mate-btn ${mateType === mate.id ? 'active' : ''}`}
                onClick={() => setMateType(mateType === mate.id ? '' : mate.id)}
              >
                {mate.name}
              </button>
            ))}
          </div>
          <div className="mates-list">
            <div className="section-title">Applied Mates ({mates.length})</div>
            {mates.length === 0 && <div className="empty-state">No mates applied</div>}
          </div>
        </div>
        <div className="mate-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default MateTools;
