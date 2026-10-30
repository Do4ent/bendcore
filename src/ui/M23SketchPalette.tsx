import React, { useState } from 'react';

export interface SketchPaletteProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const SketchPalette: React.FC<SketchPaletteProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [selectedColor, setSelectedColor] = useState('#000000');
  const [lineWidth, setLineWidth] = useState(2);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  const colors = ['#000000', '#ff0000', '#00ff00', '#0000ff', '#ffff00', '#ff00ff'];

  if (!isOpen) return null;

  return (
    <div className="palette-overlay">
      <div className="palette-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="palette-header">
          <h2>Sketch Palette</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="palette-body">
          <div className="color-section">
            <label>Color:</label>
            <div className="color-grid">
              {colors.map(color => (
                <button
                  key={color}
                  className={`color-btn ${selectedColor === color ? 'active' : ''}`}
                  style={{ backgroundColor: color }}
                  onClick={() => setSelectedColor(color)}
                  title={color}
                />
              ))}
            </div>
            <input
              type="color"
              value={selectedColor}
              onChange={(e) => setSelectedColor(e.target.value)}
            />
          </div>
          <div className="line-width-section">
            <label>Line Width:</label>
            <input
              type="range"
              min="1"
              max="10"
              value={lineWidth}
              onChange={(e) => setLineWidth(parseInt(e.target.value))}
            />
            <span>{lineWidth}px</span>
          </div>
        </div>
        <div className="palette-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default SketchPalette;
