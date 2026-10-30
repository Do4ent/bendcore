import React, { useState } from 'react';
export interface DrawingGeneratorProps { isOpen: boolean; onClose: () => void; isDarkMode?: boolean; }
const DrawingGenerator: React.FC<DrawingGeneratorProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [views, setViews] = useState<any[]>([]);
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  if (!isOpen) return null;
  return (
    <div className="drawing-gen-overlay">
      <div className="drawing-gen-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="dg-header"><h2>Drawing Generator</h2><button className="close-btn" onClick={onClose}>×</button></div>
        <div className="dg-body"><div className="view-selector"><button>Front</button><button>Top</button><button>Side</button><button>Isometric</button></div></div>
        <div className="dg-footer"><button className="btn btn-cancel" onClick={onClose}>Close</button></div>
      </div>
    </div>
  );
};
export default DrawingGenerator;
