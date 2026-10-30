import React, { useState } from 'react';
export interface ToolpathPreviewProps { isOpen: boolean; onClose: () => void; isDarkMode?: boolean; }
const ToolpathPreview: React.FC<ToolpathPreviewProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [animating, setAnimating] = useState(false);
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  if (!isOpen) return null;
  return (
    <div className="toolpath-overlay">
      <div className="toolpath-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="tp-header"><h2>Toolpath Preview</h2><button className="close-btn" onClick={onClose}>×</button></div>
        <div className="tp-body"><div className="tp-viewport"><div className="viewport-placeholder">3D Toolpath</div></div></div>
        <div className="tp-footer"><button className="btn btn-cancel" onClick={onClose}>Close</button></div>
      </div>
    </div>
  );
};
export default ToolpathPreview;
