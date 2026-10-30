import React, { useState } from 'react';

export interface BendAnalysisProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const BendAnalysis: React.FC<BendAnalysisProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [results, setResults] = useState<any>(null);
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="bend-analysis-overlay">
      <div className="bend-analysis-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="analysis-header">
          <h2>Bend Analysis</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="analysis-body">
          <div className="analysis-results">
            <div className="section-title">Analysis Results</div>
            {results ? <div>Results available</div> : <div className="empty-state">No analysis data</div>}
          </div>
        </div>
        <div className="analysis-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default BendAnalysis;
