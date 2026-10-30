import React, { useState } from 'react';

export interface SketchTransformationProps {
  isOpen: boolean;
  onClose: () => void;
  onTransform?: (type: string, params: any) => void;
  isDarkMode?: boolean;
}

const SketchTransformation: React.FC<SketchTransformationProps> = ({
  isOpen,
  onClose,
  onTransform,
  isDarkMode = false
}) => {
  const [transformType, setTransformType] = useState('');
  const [params, setParams] = useState({ x: 0, y: 0, angle: 0, scale: 1 });

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  const transformTypes = [
    { id: 'move', name: 'Move' },
    { id: 'rotate', name: 'Rotate' },
    { id: 'scale', name: 'Scale' },
    { id: 'mirror', name: 'Mirror' }
  ];

  const handleApplyTransform = () => {
    if (onTransform && transformType) {
      onTransform(transformType, params);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="transform-overlay">
      <div className="transform-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="trans-header">
          <h2>Sketch Transformation</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="trans-body">
          <div className="transform-types">
            {transformTypes.map(t => (
              <button
                key={t.id}
                className={`trans-btn ${transformType === t.id ? 'active' : ''}`}
                onClick={() => setTransformType(t.id)}
              >
                {t.name}
              </button>
            ))}
          </div>
          {transformType && (
            <div className="transform-params">
              <input
                type="number"
                placeholder="X"
                value={params.x}
                onChange={(e) => setParams({ ...params, x: parseFloat(e.target.value) })}
              />
              <input
                type="number"
                placeholder="Y"
                value={params.y}
                onChange={(e) => setParams({ ...params, y: parseFloat(e.target.value) })}
              />
              <button className="btn btn-apply" onClick={handleApplyTransform}>
                Apply
              </button>
            </div>
          )}
        </div>
        <div className="trans-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default SketchTransformation;
