import React, { useState } from 'react';

export interface DynamicInputProps {
  isOpen: boolean;
  onClose: () => void;
  onSubmit?: (x: number, y: number) => void;
  isDarkMode?: boolean;
}

const DynamicInput: React.FC<DynamicInputProps> = ({
  isOpen,
  onClose,
  onSubmit,
  isDarkMode = false
}) => {
  const [x, setX] = useState(0);
  const [y, setY] = useState(0);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  const handleSubmit = () => {
    if (onSubmit) {
      onSubmit(x, y);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="dynamic-input-overlay">
      <div className="dynamic-input-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="di-header">
          <h2>Dynamic Input</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="di-body">
          <div className="di-input-group">
            <label>X Coordinate:</label>
            <input
              type="number"
              value={x}
              onChange={(e) => setX(parseFloat(e.target.value))}
              placeholder="Enter X"
            />
          </div>
          <div className="di-input-group">
            <label>Y Coordinate:</label>
            <input
              type="number"
              value={y}
              onChange={(e) => setY(parseFloat(e.target.value))}
              placeholder="Enter Y"
            />
          </div>
        </div>
        <div className="di-footer">
          <button className="btn btn-cancel" onClick={onClose}>Cancel</button>
          <button className="btn btn-primary" onClick={handleSubmit}>OK</button>
        </div>
      </div>
    </div>
  );
};

export default DynamicInput;
