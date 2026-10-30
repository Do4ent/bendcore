import React, { useState } from 'react';

export interface SketchExportProps {
  isOpen: boolean;
  onClose: () => void;
  onExport?: (format: string, options: any) => void;
  isDarkMode?: boolean;
}

const SketchExport: React.FC<SketchExportProps> = ({
  isOpen,
  onClose,
  onExport,
  isDarkMode = false
}) => {
  const [format, setFormat] = useState('svg');
  const [quality, setQuality] = useState(100);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  const formats = [
    { id: 'svg', name: 'SVG' },
    { id: 'png', name: 'PNG' },
    { id: 'pdf', name: 'PDF' },
    { id: 'dxf', name: 'DXF' }
  ];

  const handleExport = () => {
    if (onExport) {
      onExport(format, { quality });
    }
  };

  if (!isOpen) return null;

  return (
    <div className="export-overlay">
      <div className="export-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="export-header">
          <h2>Export Sketch</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="export-body">
          <div className="format-section">
            <label>Format:</label>
            <div className="format-options">
              {formats.map(fmt => (
                <button
                  key={fmt.id}
                  className={`format-btn ${format === fmt.id ? 'active' : ''}`}
                  onClick={() => setFormat(fmt.id)}
                >
                  {fmt.name}
                </button>
              ))}
            </div>
          </div>
          <div className="quality-section">
            <label>Quality:</label>
            <input
              type="range"
              min="10"
              max="100"
              value={quality}
              onChange={(e) => setQuality(parseInt(e.target.value))}
            />
            <span>{quality}%</span>
          </div>
        </div>
        <div className="export-footer">
          <button className="btn btn-cancel" onClick={onClose}>Cancel</button>
          <button className="btn btn-primary" onClick={handleExport}>Export</button>
        </div>
      </div>
    </div>
  );
};

export default SketchExport;
