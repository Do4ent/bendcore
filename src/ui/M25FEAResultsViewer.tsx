import React, { useState } from 'react';
export interface FEAResultsViewerProps { isOpen: boolean; onClose: () => void; isDarkMode?: boolean; }
const FEAResultsViewer: React.FC<FEAResultsViewerProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [resultType, setResultType] = useState('stress');
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  if (!isOpen) return null;
  return (
    <div className="fea-overlay">
      <div className="fea-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="fea-header"><h2>FEA Results</h2><button className="close-btn" onClick={onClose}>×</button></div>
        <div className="fea-body"><div className="result-selector"><button className={resultType === 'stress' ? 'active' : ''} onClick={() => setResultType('stress')}>Stress</button><button className={resultType === 'strain' ? 'active' : ''} onClick={() => setResultType('strain')}>Strain</button></div><div className="fea-viewport"><div className="viewport-placeholder">FEA Heatmap</div></div></div>
        <div className="fea-footer"><button className="btn btn-cancel" onClick={onClose}>Close</button></div>
      </div>
    </div>
  );
};
export default FEAResultsViewer;
