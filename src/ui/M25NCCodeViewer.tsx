import React, { useState } from 'react';
export interface NCCodeViewerProps { isOpen: boolean; onClose: () => void; isDarkMode?: boolean; }
const NCCodeViewer: React.FC<NCCodeViewerProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [code, setCode] = useState<string>('');
  const [lineNumber, setLineNumber] = useState(1);
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  if (!isOpen) return null;
  return (
    <div className="nc-viewer-overlay">
      <div className="nc-viewer-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="nc-header"><h2>NC Code Viewer</h2><button className="close-btn" onClick={onClose}>×</button></div>
        <div className="nc-body"><div className="nc-code-view"><textarea value={code} onChange={(e) => setCode(e.target.value)} placeholder="G-code will appear here" /></div></div>
        <div className="nc-footer"><button className="btn btn-cancel" onClick={onClose}>Close</button></div>
      </div>
    </div>
  );
};
export default NCCodeViewer;
